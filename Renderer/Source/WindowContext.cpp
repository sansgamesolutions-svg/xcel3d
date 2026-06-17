#include "Renderer/WindowContext.h"
#include "Renderer/World.h"
#include "Renderer/Swapchain.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/Camera.h"
#include "Renderer/Drawable.h"
#include "Renderer/InstanceDrawable.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/Component.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ColorTable.h"
#include "Renderer/Manipulator/PickingRay.h"
#include "Common/ISystem.h"
#include "Common/ThreadPool.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <limits>
#include <optional>
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
        int w = 0, h = 0;
        m_widget->GetFramebufferSize(w, h);

        // Build inverse view-proj for ray casting.
        const float aspect = w > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.f;
        const glm::mat4 vp       = m_camera.ProjMatrix(aspect) * m_camera.ViewMatrix();
        const glm::mat4 invVP    = glm::inverse(vp);

        // Detect drag (> 4 px total movement from press).
        if (m_leftPressed || m_rightPressed) {
            double dx = x - m_mouseDownX, dy = y - m_mouseDownY;
            if (!m_isDragging && (dx*dx + dy*dy) > 16.0)
                m_isDragging = true;
        }

        if (m_isDragging) {
            if (m_manipulators.OnCursorMove(x, y, w, h, invVP, m_camera, m_world.Ecs()))
            {
                m_lastMouseX = x; m_lastMouseY = y;
                return;
            }
            if (m_leftPressed) {
                m_camera.Orbit(
                     (float)(x - m_lastMouseX) * 0.005f,
                    -(float)(y - m_lastMouseY) * 0.005f);
            }
            if (m_rightPressed) {
                const float scale = m_camera.Radius() * 0.0015f;
                m_camera.Pan(-(float)(x - m_lastMouseX) * scale,
                              (float)(y - m_lastMouseY) * scale);
            }
        }
        m_lastMouseX = x;
        m_lastMouseY = y;
    });
    m_widget->SetMouseButtonCallback([this](MouseButton b, InputAction a, int) {
        int w = 0, h = 0;
        m_widget->GetFramebufferSize(w, h);
        const float aspect = w > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.f;
        const glm::mat4 vp    = m_camera.ProjMatrix(aspect) * m_camera.ViewMatrix();
        const glm::mat4 invVP = glm::inverse(vp);

        if (b == MouseButton::Left) {
            if (a == InputAction::Press) {
                m_leftPressed = true;
                m_mouseDownX  = m_lastMouseX;
                m_mouseDownY  = m_lastMouseY;
                m_isDragging  = false;
            } else {
                // Left release: if not a drag → pick/select.
                if (!m_isDragging) {
                    // Let manipulators handle the click first.
                    bool consumed = m_manipulators.OnMouseButton(
                        b, a, m_lastMouseX, m_lastMouseY, w, h, invVP, m_camera, m_world.Ecs());

                    if (!consumed) {
                        // Entity picking: cast ray, select closest AABB hit.
                        Ray ray = RayFromScreen(m_lastMouseX, m_lastMouseY, w, h,
                                               invVP, m_camera.Position());
                        float      bestT    = std::numeric_limits<float>::max();
                        flecs::entity closest;
                        m_world.Ecs().each([&](flecs::entity e, const BoundingBoxComponent& bb) {
                            auto hit = RayVsAABB(ray, bb.min, bb.max);
                            if (hit && *hit < bestT) { bestT = *hit; closest = e; }
                        });
                        // Clear all selections.
                        std::vector<flecs::entity> selectedEntities;
                        m_world.Ecs().each([&](flecs::entity e, const SelectedComponent&) {
                            selectedEntities.push_back(e);
                        });
                        for (flecs::entity selected : selectedEntities)
                            selected.remove<SelectedComponent>();
                        if (closest.is_alive())
                            closest.add<SelectedComponent>();
                    }
                } else {
                    // Drag release: notify manipulators.
                    m_manipulators.OnMouseButton(b, a, m_lastMouseX, m_lastMouseY,
                                                  w, h, invVP, m_camera, m_world.Ecs());
                }
                m_leftPressed = false;
                m_isDragging  = false;
            }
        }
        if (b == MouseButton::Right) {
            m_rightPressed = (a == InputAction::Press);
            if (a == InputAction::Press) {
                m_mouseDownX = m_lastMouseX;
                m_mouseDownY = m_lastMouseY;
                m_isDragging = false;
            }
        }
    });
}

WindowContext::~WindowContext()
{
    Cleanup();
}

World&                 WindowContext::GetWorld()        { return m_world;        }
Camera&                WindowContext::GetCamera()       { return m_camera;       }
ManipulatorController& WindowContext::GetManipulators() { return m_manipulators; }

void WindowContext::SetPassOptions(const PassOptions& opts)
{
    m_globalOptions.frustumCulling   = opts.frustumCulling;
    m_globalOptions.occlusionCulling = opts.occlusionCulling;
    m_graphDirty = true;
}

void WindowContext::SetGlobalRenderOptions(const GlobalRenderOptions& opts)
{
    m_globalOptions = opts;
    m_effectiveCaps = ComputeEffectiveCaps(m_hwCaps, m_globalOptions);
    m_world.SetBatchingStrategy(opts.batchingStrategy);
    m_graphDirty = true;
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

uint32_t WindowContext::UploadTexture(uint32_t width, uint32_t height, const void* pixels)
{
    return m_textures.Upload(m_vulkan.PrimaryDevice(), width, height, pixels);
}

void WindowContext::FreeTexture(uint32_t index)
{
    m_textures.Free(m_vulkan.PrimaryDevice().Device(), index);
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
    m_hwCaps        = QueryHardwareCaps(dev.PhysicalDevice());
    m_effectiveCaps = ComputeEffectiveCaps(m_hwCaps, m_globalOptions);

    m_descriptors.Create(dev);
    m_textures.Create(dev);

    m_renderGraph = RenderGraphBuilder{}
        .SetOptions(m_globalOptions)
        .SetEffectiveCaps(m_effectiveCaps)
        .SetSwapchain(m_swapchain)
        .SetSurface(m_vulkan.Surface())
        .SetWindow(*m_widget)
        .SetDescriptors(m_descriptors)
        .SetTextures(m_textures)
        .SetShaderDir(m_shaderDir.string())
        .Build(dev);

    m_manipulators.Build(dev);

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

    // Defer set<BoundingBoxComponent> outside the each() — adding a new component during
    // iteration mutates the entity's archetype and invalidates the iterator in flecs.
    struct BbPatch { flecs::entity e; BoundingBoxComponent bb; };
    std::vector<BbPatch> bbPatches;
    m_world.Ecs().each([&](flecs::entity e, CoordTableComponent& ctc) {
        if (!ctc.coords || ctc.coords->Size() == 0) return;
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        for (const auto& pos : ctc.coords->Data()) {
            bbMin = glm::min(bbMin, pos);
            bbMax = glm::max(bbMax, pos);
        }
        bbPatches.push_back({e, {bbMin, bbMax}});
    });
    for (auto& p : bbPatches)
        p.e.set<BoundingBoxComponent>(p.bb);

    struct PageBbPatch { flecs::entity e; glm::vec3 bbMin, bbMax; };
    std::vector<PageBbPatch> pagePatches;
    m_world.Ecs().each([&](flecs::entity pageEnt, PageMetaComponent&) {
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        m_world.Ecs().each([&](flecs::entity meshEnt, BoundingBoxComponent& bb) {
            if (meshEnt.has<BelongsToPage>(pageEnt)) {
                bbMin = glm::min(bbMin, bb.min);
                bbMax = glm::max(bbMax, bb.max);
            }
        });
        if (bbMin.x <= bbMax.x)
            pagePatches.push_back({pageEnt, bbMin, bbMax});
    });
    for (auto& p : pagePatches) {
        auto* pm = p.e.get_mut<PageMetaComponent>();
        if (pm) { pm->aabbMin = p.bbMin; pm->aabbMax = p.bbMax; }
    }
}

void WindowContext::UpdateUBO(uint32_t frameIndex)
{
    const float aspect = static_cast<float>(m_swapchain.Extent().width)
                       / static_cast<float>(m_swapchain.Extent().height);

    const glm::mat4 model   = glm::mat4(1.f);
    const glm::mat4 view    = m_camera.ViewMatrix();
    const glm::mat4 proj    = m_camera.ProjMatrix(aspect);
    const glm::vec3 viewPos = m_camera.Position();

    FrameUBO ubo{};
    std::memcpy(ubo.model,   glm::value_ptr(model),   sizeof(ubo.model));
    std::memcpy(ubo.view,    glm::value_ptr(view),     sizeof(ubo.view));
    std::memcpy(ubo.proj,    glm::value_ptr(proj),     sizeof(ubo.proj));
    std::memcpy(ubo.viewPos, glm::value_ptr(viewPos),  sizeof(ubo.viewPos));

    uint32_t lightCount = 0;
    m_world.Ecs().each([&](const LightComponent& lc) {
        if (lightCount >= MAX_LIGHTS) return;
        LightGpu& g = ubo.lights[lightCount++];
        g.positionAndIntensity[0] = lc.position.x;
        g.positionAndIntensity[1] = lc.position.y;
        g.positionAndIntensity[2] = lc.position.z;
        g.positionAndIntensity[3] = lc.intensity;
        g.colorAndPad[0] = lc.color.r;
        g.colorAndPad[1] = lc.color.g;
        g.colorAndPad[2] = lc.color.b;
        g.colorAndPad[3] = 0.f;
    });
    ubo.lightCount = lightCount;

    const glm::vec4 sp = m_manipulators.SectionPlane();
    ubo.sectionPlane[0] = sp.x;
    ubo.sectionPlane[1] = sp.y;
    ubo.sectionPlane[2] = sp.z;
    ubo.sectionPlane[3] = sp.w;

    m_descriptors.UpdateUBO(frameIndex, ubo);
}

void WindowContext::DrawFrame()
{
    DeviceContext& dev = m_vulkan.PrimaryDevice();

    if (m_graphDirty) {
        vkDeviceWaitIdle(dev.Device());
        m_renderGraph.Destroy(dev.Device());
        m_renderGraph = RenderGraphBuilder{}
            .SetOptions(m_globalOptions)
            .SetEffectiveCaps(m_effectiveCaps)
            .SetSwapchain(m_swapchain)
            .SetSurface(m_vulkan.Surface())
            .SetWindow(*m_widget)
            .SetDescriptors(m_descriptors)
            .SetTextures(m_textures)
            .SetShaderDir(m_shaderDir.string())
            .Build(dev);
        m_graphDirty = false;
    }

    m_renderGraph.WaitForCurrentFrame(dev.Device());
    m_world.FlushRebuild(&m_pool);

    m_drawCalls.clear();
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
        if (const auto* mat = e.get<MaterialComponent>())
            dc.material = {mat->ambientFactor, mat->diffuseFactor,
                           mat->specularFactor, mat->shininess, mat->alpha, mat->textureIndex};
        if (const auto* ro = e.get<MeshRenderOptions>())
        {
            dc.blendMode   = ro->blendMode;
            dc.renderLayer = ro->renderLayer;
        }
        m_drawCalls.push_back(dc);
    });

    // Sort: opaque first, transparent second (back-to-front), overlay last.
    const glm::vec3 camPos = m_camera.Position();
    std::stable_sort(m_drawCalls.begin(), m_drawCalls.end(),
        [&camPos](const DrawCall& a, const DrawCall& b)
        {
            if (a.renderLayer != b.renderLayer)
                return static_cast<uint8_t>(a.renderLayer) <
                       static_cast<uint8_t>(b.renderLayer);
            if (a.renderLayer == RenderLayer::Transparent)
            {
                const glm::vec3 ca = (a.aabbMin + a.aabbMax) * 0.5f;
                const glm::vec3 cb = (b.aabbMin + b.aabbMax) * 0.5f;
                return glm::dot(ca - camPos, ca - camPos) >
                       glm::dot(cb - camPos, cb - camPos);
            }
            return false;
        });

    const uint32_t  currentFrame = m_renderGraph.CurrentFrame();

    // Update manipulators (gizmo positions, view cube orientation).
    const float     aspect   = static_cast<float>(m_swapchain.Extent().width)
                             / static_cast<float>(m_swapchain.Extent().height);
    const glm::mat4 view     = m_camera.ViewMatrix();
    const glm::mat4 viewProj = m_camera.ProjMatrix(aspect) * view;
    m_manipulators.Update(m_camera, m_world.Ecs(), view, m_swapchain.Extent());

    m_manipulatorSolidDraws.clear();
    m_manipulatorAlphaDraws.clear();
    m_manipulators.GatherDrawCalls(m_manipulatorSolidDraws, m_manipulatorAlphaDraws);

    if (m_drawCalls.empty() && m_manipulatorSolidDraws.empty() && m_manipulatorAlphaDraws.empty())
        return;

    UpdateUBO(currentFrame);

    PassContext ctx{};
    ctx.uboDescriptorSet           = m_descriptors.DescriptorSet(currentFrame);
    ctx.bindlessDescriptorSet      = m_textures.DescriptorSet();
    ctx.directDrawCalls            = m_drawCalls;
    ctx.viewProj                   = viewProj;
    ctx.manipulatorSolidDrawCalls  = m_manipulatorSolidDraws;
    ctx.manipulatorAlphaDrawCalls  = m_manipulatorAlphaDraws;

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

        m_manipulators.Destroy(device);
        m_renderGraph.Destroy(device);
        m_defaultInstanceBuffer.Destroy(device);
        m_textures.Destroy(device);
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
