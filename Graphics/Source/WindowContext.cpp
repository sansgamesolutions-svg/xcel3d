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

struct WindowContext::Impl
{
    // ── Windowing + input ─────────────────────────────────────────────────────
    std::unique_ptr<IWindowWidget> widget;
    double lastMouseX = 0.0, lastMouseY = 0.0;
    bool   mousePressed = false;

    // ── Vulkan bootstrap (instance / surface / devices) ───────────────────────
    VulkanContext vulkan;

    // ── Rendering ─────────────────────────────────────────────────────────────
    Swapchain         swapchain;
    DescriptorManager descriptors;
    Camera            camera;
    RenderGraph       renderGraph;
    GpuBuffer         defaultInstanceBuffer;
    PassOptions       passOptions;
    bool              graphDirty         = false;
    bool              framebufferResized = false;

    // ── Scene + systems ───────────────────────────────────────────────────────
    World                                 world;
    std::vector<std::unique_ptr<ISystem>> systems;
    ThreadPool                            pool;
};

WindowContext::WindowContext(std::unique_ptr<IWindowWidget> widget)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->widget = std::move(widget);

    m_impl->widget->SetFramebufferResizeCallback([this](int, int) {
        m_impl->framebufferResized = true;
    });
    m_impl->widget->SetScrollCallback([this](double, double yOffset) {
        m_impl->camera.Zoom((float)-yOffset * 0.5f);
    });
    m_impl->widget->SetCursorPosCallback([this](double x, double y) {
        if (m_impl->mousePressed) {
            m_impl->camera.Orbit(
                 (float)(x - m_impl->lastMouseX) * 0.005f,
                -(float)(y - m_impl->lastMouseY) * 0.005f);
        }
        m_impl->lastMouseX = x;
        m_impl->lastMouseY = y;
    });
    m_impl->widget->SetMouseButtonCallback([this](MouseButton b, InputAction a, int) {
        if (b == MouseButton::Left)
            m_impl->mousePressed = (a == InputAction::Press);
    });
}

WindowContext::~WindowContext()
{
    Cleanup();
}

World&  WindowContext::GetWorld()  { return m_impl->world; }
Camera& WindowContext::GetCamera() { return m_impl->camera; }

void WindowContext::SetPassOptions(const PassOptions& opts)
{
    m_impl->passOptions = opts;
    m_impl->graphDirty  = true;
}

void WindowContext::Run()
{
    InitVulkan();
    MainLoop();
}

void WindowContext::AddSystemImpl(std::unique_ptr<ISystem> system)
{
    m_impl->systems.push_back(std::move(system));
}

size_t         WindowContext::DeviceCount() const { return m_impl->vulkan.DeviceCount(); }
DeviceContext& WindowContext::GetDevice(size_t i) const { return m_impl->vulkan.Device(i); }

DeviceContext* WindowContext::FindDevice(std::function<bool(const DeviceContext&)> pred) const
{
    return m_impl->vulkan.FindDevice(pred);
}

VkInstance   WindowContext::Instance() const { return m_impl->vulkan.Instance(); }
VkSurfaceKHR WindowContext::Surface()  const { return m_impl->vulkan.Surface();  }

void WindowContext::InitVulkan()
{
    m_impl->vulkan.Init(*m_impl->widget, /*enableValidation=*/true);

    DeviceContext& dev = m_impl->vulkan.PrimaryDevice();
    m_impl->descriptors.Create(dev);

    m_impl->renderGraph = RenderGraphBuilder{}
        .SetOptions(m_impl->passOptions)
        .SetSwapchain(m_impl->swapchain)
        .SetSurface(m_impl->vulkan.Surface())
        .SetWindow(*m_impl->widget)
        .SetDescriptors(m_impl->descriptors)
        .SetShaderDir("shaders/")
        .Build(dev);

    BuildMeshes();
}

void WindowContext::BuildMeshes()
{
    PaletteColor   colormap;
    DeviceContext& dev = m_impl->vulkan.PrimaryDevice();

    glm::mat4 identity{1.f};
    m_impl->defaultInstanceBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), sizeof(glm::mat4),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_impl->defaultInstanceBuffer.UploadViaStaging(dev, &identity, sizeof(glm::mat4));

    m_impl->world.Ecs().each([&](MeshComponent& mc) {
        mc.mesh->Build(dev, colormap, &m_impl->pool);
    });

    m_impl->world.BuildAll(dev, &m_impl->pool);

    m_impl->world.Ecs().each([&](flecs::entity templateEnt, MeshComponent& mc) {
        auto* id = dynamic_cast<InstanceDrawable*>(mc.mesh.get());
        if (!id || id->IndexCount() == 0) return;

        std::vector<glm::mat4> transforms;
        m_impl->world.Ecs().each([&](flecs::entity e,
                                     const TransformComponent& tc,
                                     const VisibilityComponent& vc) {
            if (vc.visible && e.has<InstanceOf>(templateEnt))
                transforms.push_back(tc.matrix);
        });
        id->UpdateInstances(dev, transforms);
    });

    m_impl->world.Ecs().each([&](flecs::entity e, CoordTableComponent& ctc) {
        if (!ctc.coords || ctc.coords->Size() == 0) return;
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        for (const auto& pos : ctc.coords->Data()) {
            bbMin = glm::min(bbMin, pos);
            bbMax = glm::max(bbMax, pos);
        }
        e.set<BoundingBoxComponent>({bbMin, bbMax});
    });

    m_impl->world.Ecs().each([&](flecs::entity pageEnt, PageMetaComponent& pm) {
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        m_impl->world.Ecs().each([&](flecs::entity meshEnt, BoundingBoxComponent& bb) {
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

void WindowContext::MainLoop()
{
    while (!m_impl->widget->ShouldClose()) {
        m_impl->widget->PollEvents();
        for (auto& sys : m_impl->systems)
            sys->Update(m_impl->world.Ecs());
        DrawFrame();
    }
    vkDeviceWaitIdle(m_impl->vulkan.PrimaryDevice().Device());
}

void WindowContext::UpdateUBO(uint32_t frameIndex)
{
    const float aspect = static_cast<float>(m_impl->swapchain.Extent().width)
                       / static_cast<float>(m_impl->swapchain.Extent().height);

    const glm::mat4 model = glm::mat4(1.f);
    const glm::mat4 view  = m_impl->camera.ViewMatrix();
    const glm::mat4 proj  = m_impl->camera.ProjMatrix(aspect);

    const glm::vec3 lightPos   = m_impl->camera.Position() + glm::vec3(2.f, 4.f, 2.f);
    const glm::vec3 lightColor = glm::vec3(1.f, 1.f, 1.f);
    const glm::vec3 viewPos    = m_impl->camera.Position();

    FrameUBO ubo{};
    std::memcpy(ubo.model,      glm::value_ptr(model),      sizeof(ubo.model));
    std::memcpy(ubo.view,       glm::value_ptr(view),       sizeof(ubo.view));
    std::memcpy(ubo.proj,       glm::value_ptr(proj),       sizeof(ubo.proj));
    std::memcpy(ubo.lightPos,   glm::value_ptr(lightPos),   sizeof(ubo.lightPos));
    std::memcpy(ubo.lightColor, glm::value_ptr(lightColor), sizeof(ubo.lightColor));
    std::memcpy(ubo.viewPos,    glm::value_ptr(viewPos),    sizeof(ubo.viewPos));

    m_impl->descriptors.UpdateUBO(frameIndex, ubo);
}

void WindowContext::DrawFrame()
{
    DeviceContext& dev = m_impl->vulkan.PrimaryDevice();

    if (m_impl->graphDirty) {
        vkDeviceWaitIdle(dev.Device());
        m_impl->renderGraph.Destroy(dev.Device());
        m_impl->renderGraph = RenderGraphBuilder{}
            .SetOptions(m_impl->passOptions)
            .SetSwapchain(m_impl->swapchain)
            .SetSurface(m_impl->vulkan.Surface())
            .SetWindow(*m_impl->widget)
            .SetDescriptors(m_impl->descriptors)
            .SetShaderDir("shaders/")
            .Build(dev);
        m_impl->graphDirty = false;
    }

    m_impl->renderGraph.WaitForCurrentFrame(dev.Device());
    m_impl->world.FlushRebuild(&m_impl->pool);

    std::vector<DrawCall> drawCalls;
    m_impl->world.Ecs().each([&](flecs::entity e, MeshComponent& mc, VisibilityComponent& vc) {
        if (!vc.visible || mc.mesh->IndexCount() == 0) return;
        const GpuBuffer* instBuf   = mc.mesh->InstanceBuffer();
        uint32_t         instCount = mc.mesh->InstanceCount();
        if (!instBuf) {
            instBuf   = &m_impl->defaultInstanceBuffer;
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

    const uint32_t  currentFrame = m_impl->renderGraph.CurrentFrame();
    UpdateUBO(currentFrame);

    const float     aspect   = static_cast<float>(m_impl->swapchain.Extent().width)
                             / static_cast<float>(m_impl->swapchain.Extent().height);
    const glm::mat4 viewProj = m_impl->camera.ProjMatrix(aspect) * m_impl->camera.ViewMatrix();

    PassContext ctx{};
    ctx.uboDescriptorSet = m_impl->descriptors.DescriptorSet(currentFrame);
    ctx.directDrawCalls  = drawCalls;
    ctx.viewProj         = viewProj;

    bool needsResize = m_impl->framebufferResized;
    m_impl->renderGraph.Execute(dev, ctx, needsResize);

    if (needsResize) {
        m_impl->framebufferResized = false;
        m_impl->renderGraph.Resize(dev, m_impl->vulkan.Surface(), *m_impl->widget);
    }
}

void WindowContext::Cleanup()
{
    if (!m_impl->widget) return;

    if (m_impl->vulkan.DeviceCount() > 0) {
        VkDevice device = m_impl->vulkan.PrimaryDevice().Device();
        vkDeviceWaitIdle(device);

        m_impl->renderGraph.Destroy(device);
        m_impl->defaultInstanceBuffer.Destroy(device);
        m_impl->descriptors.Destroy(device);
        m_impl->swapchain.Destroy(device);

        m_impl->world.Ecs().each([&](MeshComponent& mc) {
            mc.mesh->Destroy(device);
        });
    }

    m_impl->vulkan.Destroy();
    m_impl->widget.reset();
}

} // namespace xcel
