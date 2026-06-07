#include "Graphics/VulkanContext.h"
#include "Platforms/IWindowWidget.h"
#include <stdexcept>
#include <algorithm>
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

struct VulkanContext::Impl
{
    VkInstance               instance          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger    = VK_NULL_HANDLE;
    VkSurfaceKHR             surface           = VK_NULL_HANDLE;
    bool                     validationEnabled = false;
    std::vector<std::unique_ptr<DeviceContext>> devices;
};

VulkanContext::VulkanContext()
    : m_impl(std::make_unique<Impl>()) {}

VulkanContext::~VulkanContext()
{
    Destroy();
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT    severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pData,
    void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "[Vulkan] " << pData->pMessage << "\n";
    return VK_FALSE;
}

void VulkanContext::CreateInstance(IWindowWidget& widget, bool enableValidation)
{
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Xcel3D";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "XcelGraphics";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    auto extensions = widget.RequiredVulkanExtensions();
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
        throw std::runtime_error("VulkanContext: vkCreateInstance failed");
}

void VulkanContext::SetupDebugMessenger()
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
        throw std::runtime_error("VulkanContext: failed to create debug messenger");
}

void VulkanContext::CreateSurface(IWindowWidget& widget)
{
    m_impl->surface = widget.CreateVulkanSurface(m_impl->instance);
}

bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice dev) const
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

void VulkanContext::EnumerateDevices(bool enableValidation)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_impl->instance, &count, nullptr);
    if (count == 0)
        throw std::runtime_error("VulkanContext: no Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> all(count);
    vkEnumeratePhysicalDevices(m_impl->instance, &count, all.data());

    std::vector<VkPhysicalDevice> suitable;
    suitable.reserve(count);
    for (auto dev : all)
        if (IsDeviceSuitable(dev)) suitable.push_back(dev);

    if (suitable.empty())
        throw std::runtime_error("VulkanContext: no suitable GPU found");

    std::sort(suitable.begin(), suitable.end(), [](VkPhysicalDevice a, VkPhysicalDevice b) {
        auto score = [](VkPhysicalDevice d)
        {
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

void VulkanContext::Init(IWindowWidget& widget, bool enableValidation)
{
    m_impl->validationEnabled = enableValidation;
    CreateInstance(widget, enableValidation);
    if (enableValidation) SetupDebugMessenger();
    CreateSurface(widget);
    EnumerateDevices(enableValidation);
}

void VulkanContext::Destroy()
{
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
}

VkInstance   VulkanContext::Instance() const { return m_impl->instance; }
VkSurfaceKHR VulkanContext::Surface()  const { return m_impl->surface;  }

size_t VulkanContext::DeviceCount() const { return m_impl->devices.size(); }

DeviceContext& VulkanContext::Device(size_t i) const
{
    return *m_impl->devices.at(i);
}

DeviceContext& VulkanContext::PrimaryDevice() const
{
    return Device(0);
}

DeviceContext* VulkanContext::FindDevice(std::function<bool(const DeviceContext&)> pred) const
{
    for (auto& d : m_impl->devices)
        if (pred(*d)) return d.get();
    return nullptr;
}

} // namespace xcel
