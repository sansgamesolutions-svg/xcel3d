#include "Graphics/WindowContext.h"
#include "Graphics/Swapchain.h"
#include "Graphics/RenderPass.h"
#include "Graphics/Pipeline.h"
#include "Graphics/DescriptorManager.h"
#include "Graphics/CommandRecorder.h"
#include "Graphics/Camera.h"
#include "Graphics/StaticMesh.h"
#include "Graphics/ColorTable.h"
#include "Graphics/Components.h"
#include "Common/Registry.h"
#include "Common/ISystem.h"
#include "Common/ThreadPool.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <iostream>
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

// ── Debug messenger helpers ───────────────────────────────────────────────────

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

// ── Impl ──────────────────────────────────────────────────────────────────────

struct WindowContext::Impl {
    // GLFW
    GLFWwindow* window             = nullptr;
    bool        framebufferResized = false;
    double      lastMouseX         = 0.0;
    double      lastMouseY         = 0.0;
    bool        mousePressed       = false;

    // Vulkan instance level
    VkInstance               instance          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger    = VK_NULL_HANDLE;
    VkSurfaceKHR             surface           = VK_NULL_HANDLE;
    bool                     validationEnabled = false;

    // All suitable devices, ranked best first
    std::vector<std::unique_ptr<DeviceContext>> devices;

    // Rendering objects
    RenderPass        renderPass;
    Swapchain         swapchain;
    DescriptorManager descriptors;
    Pipeline          pipeline;
    CommandRecorder   recorder;
    Camera            camera;
    ThreadPool        pool;

    // ECS registry — replaces MeshManager
    Registry registry;

    // Systems run once per frame before the draw call, in registration order
    std::vector<std::unique_ptr<ISystem>> systems;

    std::string vertSpvPath = "shaders/mesh.vert.spv";
    std::string fragSpvPath = "shaders/mesh.frag.spv";

    static constexpr int MAX_FRAMES = DescriptorManager::MAX_FRAMES;
    VkSemaphore imageAvailableSem[MAX_FRAMES] = {};
    VkSemaphore renderFinishedSem[MAX_FRAMES] = {};
    VkFence     inFlightFence[MAX_FRAMES]     = {};
    uint32_t    currentFrame = 0;
};

// ── Construction / destruction ────────────────────────────────────────────────

WindowContext::WindowContext(int width, int height, const std::string& title)
    : m_impl(std::make_unique<Impl>())
{
    if (!glfwInit())
        throw std::runtime_error("WindowContext: glfwInit failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

    m_impl->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_impl->window)
        throw std::runtime_error("WindowContext: glfwCreateWindow failed");

    glfwSetWindowUserPointer(m_impl->window, this);
    glfwSetFramebufferSizeCallback(m_impl->window, FramebufferResizeCallback);
    glfwSetScrollCallback(m_impl->window,          ScrollCallback);
    glfwSetCursorPosCallback(m_impl->window,       CursorPosCallback);
    glfwSetMouseButtonCallback(m_impl->window,     MouseButtonCallback);
}

WindowContext::~WindowContext() {
    Cleanup();
}

// ── Public API ────────────────────────────────────────────────────────────────

Entity WindowContext::AddMesh(const std::string& name, std::shared_ptr<StaticMesh> mesh) {
    Entity e = m_impl->registry.Create();
    m_impl->registry.Add<NameComponent>(e, name);
    m_impl->registry.Add<MeshComponent>(e, std::move(mesh));
    m_impl->registry.Add<TransformComponent>(e);
    m_impl->registry.Add<VisibilityComponent>(e);
    return e;
}

Camera& WindowContext::GetCamera() { return m_impl->camera; }

void WindowContext::Run() {
    InitVulkan();
    MainLoop();
}

size_t         WindowContext::DeviceCount() const { return m_impl->devices.size(); }
DeviceContext& WindowContext::GetDevice(size_t i) const { return *m_impl->devices.at(i); }

DeviceContext* WindowContext::FindDevice(std::function<bool(const DeviceContext&)> pred) const {
    for (auto& d : m_impl->devices)
        if (pred(*d)) return d.get();
    return nullptr;
}

VkInstance   WindowContext::Instance() const { return m_impl->instance; }
VkSurfaceKHR WindowContext::Surface()  const { return m_impl->surface;  }

// ── Private — Vulkan instance setup ──────────────────────────────────────────

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

void WindowContext::CreateInstance(bool enableValidation) {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Xcel3D";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "XcelGraphics";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);
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

void WindowContext::SetupDebugMessenger() {
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

void WindowContext::CreateSurface() {
    if (glfwCreateWindowSurface(m_impl->instance, m_impl->window, nullptr, &m_impl->surface) != VK_SUCCESS)
        throw std::runtime_error("WindowContext: glfwCreateWindowSurface failed");
}

bool WindowContext::IsDeviceSuitable(VkPhysicalDevice dev) const {
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

void WindowContext::EnumerateDevices(bool enableValidation) {
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

// ── Private — Vulkan init ─────────────────────────────────────────────────────

void WindowContext::InitVulkan() {
    m_impl->validationEnabled = true;
    CreateInstance(true);
    SetupDebugMessenger();
    CreateSurface();
    EnumerateDevices(true);

    DeviceContext& dev = GetDevice(0);

    auto support = Swapchain::QuerySupport(dev.PhysicalDevice(), m_impl->surface);
    VkFormat colorFmt = support.formats.empty()
        ? VK_FORMAT_B8G8R8A8_SRGB
        : support.formats[0].format;
    for (const auto& f : support.formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB) { colorFmt = f.format; break; }

    m_impl->renderPass.Create(dev.Device(), colorFmt, VK_FORMAT_D32_SFLOAT);
    m_impl->swapchain.Create(dev, m_impl->surface, m_impl->window, m_impl->renderPass.GetHandle());
    m_impl->descriptors.Create(dev);
    m_impl->pipeline.Create(dev.Device(),
                            m_impl->renderPass.GetHandle(),
                            m_impl->descriptors.Layout(),
                            m_impl->swapchain.Extent(),
                            m_impl->vertSpvPath, m_impl->fragSpvPath);
    m_impl->recorder.Create(dev);
    BuildMeshes();
    CreateSyncObjects();
}

void WindowContext::BuildMeshes() {
    ColorTable     colormap;
    DeviceContext& dev = GetDevice(0);
    for (auto [e, mc] : m_impl->registry.View<MeshComponent>())
        mc.mesh->Build(dev, colormap, &m_impl->pool);
}

void WindowContext::CreateSyncObjects() {
    VkDevice device = GetDevice(0).Device();

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < Impl::MAX_FRAMES; ++i) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &m_impl->imageAvailableSem[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &m_impl->renderFinishedSem[i]) != VK_SUCCESS ||
            vkCreateFence    (device, &fenceInfo, nullptr, &m_impl->inFlightFence[i])   != VK_SUCCESS)
            throw std::runtime_error("WindowContext: failed to create sync objects");
    }
}

// ── Private — frame loop ──────────────────────────────────────────────────────

void WindowContext::AddSystemImpl(std::unique_ptr<ISystem> system) {
    m_impl->systems.push_back(std::move(system));
}

void WindowContext::MainLoop() {
    while (!glfwWindowShouldClose(m_impl->window)) {
        glfwPollEvents();
        for (auto& sys : m_impl->systems)
            sys->Update(m_impl->registry);
        DrawFrame();
    }
    vkDeviceWaitIdle(GetDevice(0).Device());
}

void WindowContext::UpdateUBO(uint32_t frameIndex) {
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

void WindowContext::DrawFrame() {
    VkDevice device = GetDevice(0).Device();

    vkWaitForFences(device, 1, &m_impl->inFlightFence[m_impl->currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device, m_impl->swapchain.GetHandle(), UINT64_MAX,
        m_impl->imageAvailableSem[m_impl->currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        HandleResize();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("WindowContext: vkAcquireNextImageKHR failed");
    }

    vkResetFences(device, 1, &m_impl->inFlightFence[m_impl->currentFrame]);

    UpdateUBO(m_impl->currentFrame);

    // Collect draw calls from visible mesh entities
    std::vector<DrawCall> drawCalls;
    for (auto [e, mc, vc] : m_impl->registry.View<MeshComponent, VisibilityComponent>()) {
        if (vc.visible && mc.mesh->IndexCount() > 0)
            drawCalls.push_back({&mc.mesh->VertexBuffer(), &mc.mesh->IndexBuffer(), mc.mesh->IndexCount()});
    }
    if (drawCalls.empty()) return;

    m_impl->recorder.Record(
        m_impl->currentFrame,
        m_impl->swapchain.Framebuffer(imageIndex),
        m_impl->swapchain.Extent(),
        m_impl->renderPass.GetHandle(),
        m_impl->pipeline,
        m_impl->descriptors,
        drawCalls);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_impl->imageAvailableSem[m_impl->currentFrame];
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &m_impl->recorder.CommandBuffer(m_impl->currentFrame);
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_impl->renderFinishedSem[m_impl->currentFrame];

    if (vkQueueSubmit(GetDevice(0).GraphicsQueue(), 1, &submitInfo,
                      m_impl->inFlightFence[m_impl->currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("WindowContext: vkQueueSubmit failed");

    VkSwapchainKHR sc = m_impl->swapchain.GetHandle();
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_impl->renderFinishedSem[m_impl->currentFrame];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &sc;
    presentInfo.pImageIndices      = &imageIndex;

    result = vkQueuePresentKHR(GetDevice(0).PresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_impl->framebufferResized) {
        m_impl->framebufferResized = false;
        HandleResize();
    }

    m_impl->currentFrame = (m_impl->currentFrame + 1) % Impl::MAX_FRAMES;
}

void WindowContext::HandleResize() {
    DeviceContext& dev = GetDevice(0);
    m_impl->swapchain.Recreate(dev, m_impl->surface, m_impl->window, m_impl->renderPass.GetHandle());
    m_impl->pipeline.Destroy(dev.Device());
    m_impl->pipeline.Create(dev.Device(),
                            m_impl->renderPass.GetHandle(),
                            m_impl->descriptors.Layout(),
                            m_impl->swapchain.Extent(),
                            m_impl->vertSpvPath, m_impl->fragSpvPath);
}

void WindowContext::Cleanup() {
    if (!m_impl->window) return;

    if (m_impl->instance == VK_NULL_HANDLE) {
        glfwDestroyWindow(m_impl->window);
        glfwTerminate();
        m_impl->window = nullptr;
        return;
    }

    if (!m_impl->devices.empty()) {
        VkDevice device = GetDevice(0).Device();
        vkDeviceWaitIdle(device);

        for (int i = 0; i < Impl::MAX_FRAMES; ++i) {
            vkDestroySemaphore(device, m_impl->renderFinishedSem[i], nullptr);
            vkDestroySemaphore(device, m_impl->imageAvailableSem[i], nullptr);
            vkDestroyFence    (device, m_impl->inFlightFence[i],     nullptr);
        }

        m_impl->recorder.Destroy(device);
        m_impl->pipeline.Destroy(device);
        m_impl->descriptors.Destroy(device);
        m_impl->swapchain.Destroy(device);
        m_impl->renderPass.Destroy(device);

        // Destroy GPU buffers for all mesh entities
        for (auto [e, mc] : m_impl->registry.View<MeshComponent>())
            mc.mesh->Destroy(device);
    }

    // Registry destructor frees all entities and component data
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

    glfwDestroyWindow(m_impl->window);
    glfwTerminate();
    m_impl->window = nullptr;
}

// ── GLFW static callbacks ─────────────────────────────────────────────────────

void WindowContext::FramebufferResizeCallback(GLFWwindow* w, int, int) {
    auto* ctx = static_cast<WindowContext*>(glfwGetWindowUserPointer(w));
    ctx->m_impl->framebufferResized = true;
}

void WindowContext::ScrollCallback(GLFWwindow* w, double, double yOffset) {
    auto* ctx = static_cast<WindowContext*>(glfwGetWindowUserPointer(w));
    ctx->m_impl->camera.Zoom((float)-yOffset * 0.5f);
}

void WindowContext::CursorPosCallback(GLFWwindow* w, double x, double y) {
    auto* ctx = static_cast<WindowContext*>(glfwGetWindowUserPointer(w));
    if (ctx->m_impl->mousePressed) {
        float dx = (float)(x - ctx->m_impl->lastMouseX);
        float dy = (float)(y - ctx->m_impl->lastMouseY);
        ctx->m_impl->camera.Orbit(dx * 0.005f, -dy * 0.005f);
    }
    ctx->m_impl->lastMouseX = x;
    ctx->m_impl->lastMouseY = y;
}

void WindowContext::MouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    auto* ctx = static_cast<WindowContext*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        ctx->m_impl->mousePressed = (action == GLFW_PRESS);
}

} // namespace xcel
