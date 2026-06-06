#include "Renderer/WindowContext.h"
#include "Renderer/Swapchain.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/Camera.h"
#include "Renderer/Drawable.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/InstanceDrawable.h"
#include "Renderer/BatchingSystem.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include "Kernel/PrimitiveSet.h"
#include "Renderer/Component.h"
#include "Common/ISystem.h"
#include <flecs.h>
#include "Common/ThreadPool.h"
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace xcel {

static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VkResult CreateDebugMessenger(
    VkInstance                                instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pInfo,
    const VkAllocationCallbacks*              pAllocator,
    VkDebugUtilsMessengerEXT*                 pMessenger)
{
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    return fn ? fn(instance, pInfo, pAllocator, pMessenger)
              : VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugMessenger(
    VkInstance                   instance,
    VkDebugUtilsMessengerEXT     messenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn) fn(instance, messenger, pAllocator);
}


struct WindowContext::Impl
{
    // Windowing backend (platform-neutral)
    std::unique_ptr<IWindowWidget> widget;
    bool   framebufferResized = false;
    double lastMouseX         = 0.0;
    double lastMouseY         = 0.0;
    bool   mousePressed       = false;

    // Vulkan instance level
    VkInstance               instance          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger    = VK_NULL_HANDLE;
    VkSurfaceKHR             surface           = VK_NULL_HANDLE;
    bool                     validationEnabled = false;

    // All suitable devices, ranked best first
    std::vector<std::unique_ptr<DeviceContext>> devices;

    // Rendering objects
    Swapchain         swapchain;
    DescriptorManager descriptors;
    Camera            camera;
    ThreadPool        pool;

    // Multi-pass render graph (owns command buffers, sync objects, and all passes)
    RenderGraph  renderGraph;
    PassOptions  passOptions;
    bool         graphDirty = false;

    // ECS world
    flecs::world world;

    // Systems run once per frame before the draw call, in registration order
    std::vector<std::unique_ptr<ISystem>> systems;

    // Packs mesh entities registered via AddMesh(name, Mesh) into batched GPU pages.
    BatchingSystem batchingSystem;

    // Shared 1-element identity buffer bound at binding 1 for non-instanced draw calls.
    GpuBuffer defaultInstanceBuffer;
};


WindowContext::WindowContext(std::unique_ptr<IWindowWidget> widget)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->widget = std::move(widget);

    // Register input callbacks as lambdas â€” no platform types cross this boundary
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

Entity WindowContext::AddMesh(const std::string&                         name,
                               std::shared_ptr<CoordTable>                coords,
                               std::shared_ptr<ScalarTable>               scalars,
                               std::shared_ptr<ColorTable>                colorTable,
                               std::vector<std::shared_ptr<PrimitiveSet>> primSets)
{
    flecs::entity e = m_impl->world.entity()
        .set<NameComponent>({name})
        .set<CoordTableComponent>({std::move(coords)})
        .set<ScalarTableComponent>({std::move(scalars)})
        .set<ColorTableComponent>({std::move(colorTable)})
        .set<PrimitiveSetsComponent>({std::move(primSets)})
        .set<TransformComponent>({})
        .set<VisibilityComponent>({});

    m_impl->batchingSystem.Register(e);
    return e;
}

Entity WindowContext::AddInstanceMesh(
    const std::string&                         name,
    std::shared_ptr<CoordTable>                coords,
    std::shared_ptr<ScalarTable>               scalars,
    std::shared_ptr<ColorTable>                colorTable,
    std::vector<std::shared_ptr<PrimitiveSet>> primSets)
{
    flecs::entity e = m_impl->world.entity()
        .set<NameComponent>({name})
        .set<CoordTableComponent>({std::move(coords)})
        .set<ScalarTableComponent>({std::move(scalars)})
        .set<ColorTableComponent>({std::move(colorTable)})
        .set<PrimitiveSetsComponent>({std::move(primSets)})
        .set<TransformComponent>({})
        .set<VisibilityComponent>({});
    e.set<MeshComponent>({std::make_shared<InstanceDrawable>(e)});
    return e;
}

Entity WindowContext::AddInstance(Entity templateEntity, const glm::mat4& transform)
{
    return m_impl->world.entity()
        .set<TransformComponent>({transform})
        .set<VisibilityComponent>({true})
        .add<InstanceOf>(static_cast<flecs::entity>(templateEntity));
}

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

size_t         WindowContext::DeviceCount() const { return m_impl->devices.size(); }
DeviceContext& WindowContext::GetDevice(size_t i) const { return *m_impl->devices.at(i); }

DeviceContext* WindowContext::FindDevice(std::function<bool(const DeviceContext&)> pred) const
{
    for (auto& d : m_impl->devices)
        if (pred(*d)) return d.get();
    return nullptr;
}

VkInstance   WindowContext::Instance() const { return m_impl->instance; }
VkSurfaceKHR WindowContext::Surface()  const { return m_impl->surface;  }

// â”€â”€ Private â€” Vulkan instance setup â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

VKAPI_ATTR VkBool32 VKAPI_CALL WindowContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT    severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pData,
    void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "[Vulkan] " << pData->pMessage << "\n";
    return VK_FALSE;
}

void WindowContext::CreateInstance(bool enableValidation)
{
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Xcel3D";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "XcelGraphics";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    auto extensions = m_impl->widget->RequiredVulkanExtensions();
    if (enableValidation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    if (enableValidation) {
        createInfo.enabledLayerCount   = (uint32_t)kValidationLayers.size();
        createInfo.ppEnabledLayerNames = kValidationLayers.data();

        debugInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = DebugCallback;
        createInfo.pNext          = &debugInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_impl->instance) != VK_SUCCESS)
        throw std::runtime_error("WindowContext: vkCreateInstance failed");
}

void WindowContext::SetupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;

    if (CreateDebugMessenger(m_impl->instance, &info, nullptr, &m_impl->debugMessenger) != VK_SUCCESS)
        throw std::runtime_error("WindowContext: failed to create debug messenger");
}

void WindowContext::CreateSurface()
{
    m_impl->surface = m_impl->widget->CreateVulkanSurface(m_impl->instance);
}

bool WindowContext::IsDeviceSuitable(VkPhysicalDevice dev) const
{
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, available.data());

    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& ext : available) required.erase(ext.extensionName);
    if (!required.empty()) return false;

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qFamilies(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qFamilies.data());

    bool hasGraphics = false, hasPresent = false;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) hasGraphics = true;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_impl->surface, &presentSupport);
        if (presentSupport) hasPresent = true;
    }
    return hasGraphics && hasPresent;
}

void WindowContext::EnumerateDevices(bool enableValidation)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_impl->instance, &count, nullptr);
    if (count == 0)
        throw std::runtime_error("WindowContext: no Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> all(count);
    vkEnumeratePhysicalDevices(m_impl->instance, &count, all.data());

    std::vector<VkPhysicalDevice> suitable;
    suitable.reserve(count);
    for (auto dev : all)
        if (IsDeviceSuitable(dev)) suitable.push_back(dev);

    if (suitable.empty())
        throw std::runtime_error("WindowContext: no suitable GPU found");

    std::sort(suitable.begin(), suitable.end(), [](VkPhysicalDevice a, VkPhysicalDevice b) {
        auto score = [](VkPhysicalDevice d) {
            VkPhysicalDeviceProperties p;
            vkGetPhysicalDeviceProperties(d, &p);
            return p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   ? 1000
                 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ?  100
                 : 1;
        };
        return score(a) > score(b);
    });

    m_impl->devices.reserve(suitable.size());
    for (auto dev : suitable) {
        auto ctx = std::make_unique<DeviceContext>();
        ctx->Create(dev, m_impl->surface, enableValidation);
        m_impl->devices.push_back(std::move(ctx));
    }
}

void WindowContext::InitVulkan()
{
    m_impl->validationEnabled = true;
    CreateInstance(true);
    SetupDebugMessenger();
    CreateSurface();
    EnumerateDevices(true);

    DeviceContext& dev = GetDevice(0);
    m_impl->descriptors.Create(dev);

    m_impl->renderGraph = RenderGraphBuilder{}
        .SetOptions(m_impl->passOptions)
        .SetSwapchain(m_impl->swapchain)
        .SetSurface(m_impl->surface)
        .SetWindow(*m_impl->widget)
        .SetDescriptors(m_impl->descriptors)
        .SetShaderDir("shaders/")
        .Build(dev);

    BuildMeshes();
}

void WindowContext::BuildMeshes()
{
    PaletteColor   colormap;
    DeviceContext& dev = GetDevice(0);

    // Create the shared identity instance buffer for non-instanced draw calls.
    glm::mat4 identity{1.f};
    m_impl->defaultInstanceBuffer.Create(
        dev.Device(), dev.PhysicalDevice(), sizeof(glm::mat4),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_impl->defaultInstanceBuffer.UploadViaStaging(dev, &identity, sizeof(glm::mat4));

    // Legacy path: pre-built Drawables registered via AddMesh(name, Drawable).
    m_impl->world.each([&](MeshComponent& mc) {
        mc.mesh->Build(dev, colormap, &m_impl->pool);
    });

    // Batched path: assign mesh entities to pages and upload.
    m_impl->batchingSystem.BuildAll(m_impl->world, dev, &m_impl->pool);

    // Instanced path: collect visible instance transforms per template and upload.
    m_impl->world.each([&](flecs::entity templateEnt, MeshComponent& mc) {
        auto* id = dynamic_cast<InstanceDrawable*>(mc.mesh.get());
        if (!id || id->IndexCount() == 0) return;

        std::vector<glm::mat4> transforms;
        m_impl->world.each([&](flecs::entity e,
                                const TransformComponent& tc,
                                const VisibilityComponent& vc) {
            if (vc.visible && e.has<InstanceOf>(templateEnt))
                transforms.push_back(tc.matrix);
        });
        id->UpdateInstances(dev, transforms);
    });

    // Compute per-entity bounding boxes from CoordTableComponent positions.
    m_impl->world.each([&](flecs::entity e, CoordTableComponent& ctc) {
        if (!ctc.coords || ctc.coords->Size() == 0) return;
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        for (const auto& pos : ctc.coords->Data()) {
            bbMin = glm::min(bbMin, pos);
            bbMax = glm::max(bbMax, pos);
        }
        e.set<BoundingBoxComponent>({bbMin, bbMax});
    });

    // Compute per-page AABBs as the union of member entity AABBs.
    m_impl->world.each([&](flecs::entity pageEnt, PageMetaComponent& pm) {
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        m_impl->world.each([&](flecs::entity meshEnt, BoundingBoxComponent& bb) {
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
            sys->Update(m_impl->world);
        DrawFrame();
    }
    vkDeviceWaitIdle(GetDevice(0).Device());
}

void WindowContext::UpdateUBO(uint32_t frameIndex)
{
    float aspect = (float)m_impl->swapchain.Extent().width / (float)m_impl->swapchain.Extent().height;

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view  = m_impl->camera.ViewMatrix();
    glm::mat4 proj  = m_impl->camera.ProjMatrix(aspect);

    glm::vec3 lightPos   = m_impl->camera.Position() + glm::vec3(2.f, 4.f, 2.f);
    glm::vec3 lightColor = glm::vec3(1.f, 1.f, 1.f);
    glm::vec3 viewPos    = m_impl->camera.Position();

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
    DeviceContext& dev = GetDevice(0);

    if (m_impl->graphDirty) {
        vkDeviceWaitIdle(dev.Device());
        m_impl->renderGraph.Destroy(dev.Device());
        m_impl->renderGraph = RenderGraphBuilder{}
            .SetOptions(m_impl->passOptions)
            .SetSwapchain(m_impl->swapchain)
            .SetSurface(m_impl->surface)
            .SetWindow(*m_impl->widget)
            .SetDescriptors(m_impl->descriptors)
            .SetShaderDir("shaders/")
            .Build(dev);
        m_impl->graphDirty = false;
    }

    // Wait for the previous frame's GPU work before rebuilding dirty GPU pages.
    m_impl->renderGraph.WaitForCurrentFrame(dev.Device());

    // Rebuild any pages dirtied by visibility changes.
    m_impl->batchingSystem.FlushRebuild(&m_impl->pool);

    std::vector<DrawCall> drawCalls;
    m_impl->world.each([&](flecs::entity e, MeshComponent& mc, VisibilityComponent& vc) {
        if (!vc.visible || mc.mesh->IndexCount() == 0) return;
        const GpuBuffer* instBuf   = mc.mesh->InstanceBuffer();
        uint32_t         instCount = mc.mesh->InstanceCount();
        if (!instBuf) {
            instBuf   = &m_impl->defaultInstanceBuffer;
            instCount = 1;
        }
        DrawCall dc{&mc.mesh->VertexBuffer(), &mc.mesh->IndexBuffer(),
                    mc.mesh->IndexCount(), instBuf, instCount};

        // Populate world-space AABB for culling passes.
        const auto* pm = e.get<PageMetaComponent>();
        const auto* bb = e.get<BoundingBoxComponent>();
        if (pm)      { dc.aabbMin = pm->aabbMin; dc.aabbMax = pm->aabbMax; }
        else if (bb) { dc.aabbMin = bb->min;     dc.aabbMax = bb->max;     }

        drawCalls.push_back(dc);
    });
    if (drawCalls.empty()) return;

    const uint32_t currentFrame = m_impl->renderGraph.CurrentFrame();
    UpdateUBO(currentFrame);

    // Build the clip-from-world matrix for GPU culling passes.
    const float    aspect   = static_cast<float>(m_impl->swapchain.Extent().width)
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
        m_impl->renderGraph.Resize(dev, m_impl->surface, *m_impl->widget);
    }
}

void WindowContext::HandleResize()
{
    // Resize is now handled inside DrawFrame() via RenderGraph::Resize().
    // This method is kept for the WindowContext.h declaration but is unreachable.
}

void WindowContext::Cleanup()
{
    if (!m_impl->widget) return;

    if (m_impl->instance == VK_NULL_HANDLE) {
        m_impl->widget.reset();
        return;
    }

    if (!m_impl->devices.empty()) {
        VkDevice device = GetDevice(0).Device();
        vkDeviceWaitIdle(device);

        m_impl->renderGraph.Destroy(device);
        m_impl->defaultInstanceBuffer.Destroy(device);
        m_impl->descriptors.Destroy(device);
        m_impl->swapchain.Destroy(device);

        m_impl->world.each([&](MeshComponent& mc) {
            mc.mesh->Destroy(device);
        });
    }

    m_impl->devices.clear();

    if (m_impl->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_impl->instance, m_impl->surface, nullptr);
        m_impl->surface = VK_NULL_HANDLE;
    }
    if (m_impl->debugMessenger != VK_NULL_HANDLE) {
        DestroyDebugMessenger(m_impl->instance, m_impl->debugMessenger, nullptr);
        m_impl->debugMessenger = VK_NULL_HANDLE;
    }
    if (m_impl->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_impl->instance, nullptr);
        m_impl->instance = VK_NULL_HANDLE;
    }

    // Widget destructor calls glfwDestroyWindow + glfwTerminate
    m_impl->widget.reset();
}

} // namespace xcel
