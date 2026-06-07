#include "Graphics/WindowContext.h"
#include "Graphics/World.h"
#include "Graphics/Swapchain.h"
#include "Graphics/DescriptorManager.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RenderGraphBuilder.h"
#include "Graphics/Camera.h"
#include "Graphics/Drawable.h"
#include "Graphics/InstanceDrawable.h"
#include "Graphics/GpuBuffer.h"
#include "Graphics/Component.h"
#include "Graphics/CoordTable.h"
#include "Graphics/ColorTable.h"
#include "Common/ISystem.h"
#include "Common/ThreadPool.h"
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <cstring>
#include <limits>
#include <vector>

namespace xcel {

WindowContext::WindowContext(std::unique_ptr<IWindowWidget> widget)
    {
    m_widget = std::move(widget);

    m_widget->SetFramebufferResizeCallback([this](int, int) {
        m_framebufferResized = true;
    });
    m_widget->SetScrollCallback([this](double, double yOffset) {
        m_camera.Zoom((float)-yOffset * 0.5f);
    });
    m_widget->SetCursorPosCallback([this](double x, double y) {
        if (m_mousePressed) {
            m_camera.Orbit(
                 (float)(x - m_lastMouseX) * 0.005f,
                -(float)(y - m_lastMouseY) * 0.005f);
        }
        m_lastMouseX = x;
        m_lastMouseY = y;
    });
    m_widget->SetMouseButtonCallback([this](MouseButton b, InputAction a, int) {
        if (b == MouseButton::Left)
            m_mousePressed = (a == InputAction::Press);
    });
}

WindowContext::~WindowContext()
{
    Cleanup();
}

World&  WindowContext::GetWorld()  { return m_world; }
Camera& WindowContext::GetCamera() { return m_camera; }

void WindowContext::SetPassOptions(const PassOptions& opts)
{
    m_passOptions = opts;
    m_graphDirty  = true;
}

void WindowContext::SetShaderDir(std::filesystem::path dir)
{
    m_shaderDir = std::move(dir);
}

void WindowContext::Init()
{
    if (m_initialized) return;
    InitVulkan();
    m_initialized = true;
}

bool WindowContext::Tick()
{
    if (m_widget->ShouldClose()) return false;
    m_widget->PollEvents();
    for (auto& sys : m_systems)
        sys->Update(m_world.Ecs());
    DrawFrame();
    return true;
}

void WindowContext::Run()
{
    Init();
    while (Tick()) {}
    vkDeviceWaitIdle(m_vulkan.PrimaryDevice().Device());
}

void WindowContext::AddSystemImpl(std::unique_ptr<ISystem> system)
{
    m_systems.push_back(std::move(system));
}

size_t         WindowContext::DeviceCount() const { return m_vulkan.DeviceCount(); }
DeviceContext& WindowContext::GetDevice(size_t i) const { return m_vulkan.Device(i); }

DeviceContext* WindowContext::FindDevice(std::function<bool(const DeviceContext&)> pred) const
{
    return m_vulkan.FindDevice(pred);
}

VkInstance   WindowContext::Instance() const { return m_vulkan.Instance(); }
VkSurfaceKHR WindowContext::Surface()  const { return m_vulkan.Surface();  }

void WindowContext::InitVulkan()
{
    m_vulkan.Init(*m_widget, /*enableValidation=*/true);

    DeviceContext& dev = m_vulkan.PrimaryDevice();
    m_descriptors.Create(dev);

    m_renderGraph = RenderGraphBuilder{}
        .SetOptions(m_passOptions)
        .SetSwapchain(m_swapchain)
        .SetSurface(m_vulkan.Surface())
        .SetWindow(*m_widget)
        .SetDescriptors(m_descriptors)
        .SetShaderDir(m_shaderDir.string())
        .Build(dev);

    BuildMeshes();
}

void WindowContext::BuildMeshes()
{
    PaletteColor   colormap;
    DeviceContext& dev = m_vulkan.PrimaryDevice();

    glm::mat4 identity{1.f};
    m_defaultInstanceBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), sizeof(glm::mat4),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_defaultInstanceBuffer.UploadViaStaging(dev, &identity, sizeof(glm::mat4));

    m_world.Ecs().each([&](MeshComponent& mc) {
        mc.mesh->Build(dev, colormap, &m_pool);
    });

    m_world.BuildAll(dev, &m_pool);

    m_world.Ecs().each([&](flecs::entity templateEnt, MeshComponent& mc) {
        auto* id = dynamic_cast<InstanceDrawable*>(mc.mesh.get());
        if (!id || id->IndexCount() == 0) return;

        std::vector<glm::mat4> transforms;
        m_world.Ecs().each([&](flecs::entity e,
                                     const TransformComponent& tc,
                                     const VisibilityComponent& vc) {
            if (vc.visible && e.has<InstanceOf>(templateEnt))
                transforms.push_back(tc.matrix);
        });
        id->UpdateInstances(dev, transforms);
    });

    m_world.Ecs().each([&](flecs::entity e, CoordTableComponent& ctc) {
        if (!ctc.coords || ctc.coords->Size() == 0) return;
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        for (const auto& pos : ctc.coords->Data()) {
            bbMin = glm::min(bbMin, pos);
            bbMax = glm::max(bbMax, pos);
        }
        e.set<BoundingBoxComponent>({bbMin, bbMax});
    });

    m_world.Ecs().each([&](flecs::entity pageEnt, PageMetaComponent& pm) {
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        m_world.Ecs().each([&](flecs::entity meshEnt, BoundingBoxComponent& bb) {
            if (meshEnt.has<BelongsToPage>(pageEnt)) {
                bbMin = glm::min(bbMin, bb.min);
                bbMax = glm::max(bbMax, bb.max);
            }
        });
        if (bbMin.x <= bbMax.x) {
            pm.aabbMin = bbMin;
            pm.aabbMax = bbMax;
        }
    });
}

void WindowContext::UpdateUBO(uint32_t frameIndex)
{
    const float aspect = static_cast<float>(m_swapchain.Extent().width)
                       / static_cast<float>(m_swapchain.Extent().height);

    const glm::mat4 model = glm::mat4(1.f);
    const glm::mat4 view  = m_camera.ViewMatrix();
    const glm::mat4 proj  = m_camera.ProjMatrix(aspect);

    const glm::vec3 lightPos   = m_camera.Position() + glm::vec3(2.f, 4.f, 2.f);
    const glm::vec3 lightColor = glm::vec3(1.f, 1.f, 1.f);
    const glm::vec3 viewPos    = m_camera.Position();

    FrameUBO ubo{};
    std::memcpy(ubo.model,      glm::value_ptr(model),      sizeof(ubo.model));
    std::memcpy(ubo.view,       glm::value_ptr(view),       sizeof(ubo.view));
    std::memcpy(ubo.proj,       glm::value_ptr(proj),       sizeof(ubo.proj));
    std::memcpy(ubo.lightPos,   glm::value_ptr(lightPos),   sizeof(ubo.lightPos));
    std::memcpy(ubo.lightColor, glm::value_ptr(lightColor), sizeof(ubo.lightColor));
    std::memcpy(ubo.viewPos,    glm::value_ptr(viewPos),    sizeof(ubo.viewPos));

    m_descriptors.UpdateUBO(frameIndex, ubo);
}

void WindowContext::DrawFrame()
{
    DeviceContext& dev = m_vulkan.PrimaryDevice();

    if (m_graphDirty) {
        vkDeviceWaitIdle(dev.Device());
        m_renderGraph.Destroy(dev.Device());
        m_renderGraph = RenderGraphBuilder{}
            .SetOptions(m_passOptions)
            .SetSwapchain(m_swapchain)
            .SetSurface(m_vulkan.Surface())
            .SetWindow(*m_widget)
            .SetDescriptors(m_descriptors)
            .SetShaderDir(m_shaderDir.string())
            .Build(dev);
        m_graphDirty = false;
    }

    m_renderGraph.WaitForCurrentFrame(dev.Device());
    m_world.FlushRebuild(&m_pool);

    std::vector<DrawCall> drawCalls;
    m_world.Ecs().each([&](flecs::entity e, MeshComponent& mc, VisibilityComponent& vc) {
        if (!vc.visible || mc.mesh->IndexCount() == 0) return;
        const GpuBuffer* instBuf   = mc.mesh->InstanceBuffer();
        uint32_t         instCount = mc.mesh->InstanceCount();
        if (!instBuf) {
            instBuf   = &m_defaultInstanceBuffer;
            instCount = 1;
        }
        DrawCall dc{&mc.mesh->VertexBuffer(), &mc.mesh->IndexBuffer(),
                    mc.mesh->IndexCount(), instBuf, instCount};
        const auto* pm = e.get<PageMetaComponent>();
        const auto* bb = e.get<BoundingBoxComponent>();
        if (pm)      { dc.aabbMin = pm->aabbMin; dc.aabbMax = pm->aabbMax; }
        else if (bb) { dc.aabbMin = bb->min;     dc.aabbMax = bb->max;     }
        drawCalls.push_back(dc);
    });
    if (drawCalls.empty()) return;

    const uint32_t  currentFrame = m_renderGraph.CurrentFrame();
    UpdateUBO(currentFrame);

    const float     aspect   = static_cast<float>(m_swapchain.Extent().width)
                             / static_cast<float>(m_swapchain.Extent().height);
    const glm::mat4 viewProj = m_camera.ProjMatrix(aspect) * m_camera.ViewMatrix();

    PassContext ctx{};
    ctx.uboDescriptorSet = m_descriptors.DescriptorSet(currentFrame);
    ctx.directDrawCalls  = drawCalls;
    ctx.viewProj         = viewProj;

    bool needsResize = m_framebufferResized;
    m_renderGraph.Execute(dev, ctx, needsResize);

    if (needsResize) {
        m_framebufferResized = false;
        m_renderGraph.Resize(dev, m_vulkan.Surface(), *m_widget);
    }
}

void WindowContext::Cleanup()
{
    if (!m_widget) return;

    if (m_vulkan.DeviceCount() > 0) {
        VkDevice device = m_vulkan.PrimaryDevice().Device();
        vkDeviceWaitIdle(device);

        m_renderGraph.Destroy(device);
        m_defaultInstanceBuffer.Destroy(device);
        m_descriptors.Destroy(device);
        m_swapchain.Destroy(device);

        m_world.Ecs().each([&](MeshComponent& mc) {
            mc.mesh->Destroy(device);
        });
    }

    m_vulkan.Destroy();
    m_widget.reset();
}

} // namespace xcel
