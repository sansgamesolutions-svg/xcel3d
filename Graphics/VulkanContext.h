#pragma once
#include "Graphics/DeviceContext.h"
#include <memory>
#include <functional>
#include <vector>

namespace xcel {

class IWindowWidget;

// Owns the Vulkan instance tier: VkInstance, VkDebugUtilsMessengerEXT,
// VkSurfaceKHR, and the ranked vector of DeviceContexts.
// RAII: Destroy() is called by the destructor, so explicit Destroy() calls
// in Cleanup() are safe (all handles are guarded against double-free).
class VulkanContext
{
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void Init(IWindowWidget& widget, bool enableValidation);
    void Destroy();

    VkInstance   Instance() const;
    VkSurfaceKHR Surface()  const;

    size_t         DeviceCount()  const;
    DeviceContext& Device(size_t i) const;
    DeviceContext& PrimaryDevice() const;  // Device(0)
    DeviceContext* FindDevice(std::function<bool(const DeviceContext&)> pred) const;

private:
    void CreateInstance(IWindowWidget& widget, bool enableValidation);
    void SetupDebugMessenger();
    void CreateSurface(IWindowWidget& widget);
    void EnumerateDevices(bool enableValidation);
    bool IsDeviceSuitable(VkPhysicalDevice phys) const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT    severity,
        VkDebugUtilsMessageTypeFlagsEXT           type,
        const VkDebugUtilsMessengerCallbackDataEXT* pData,
        void*                                     pUserData);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
