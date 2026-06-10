#pragma once
#include "Renderer/DeviceContext.h"
#include <functional>
#include <memory>
#include <vector>

namespace xcel {

class IWindowWidget;

class VulkanContext
{
public:
    VulkanContext()  = default;
    ~VulkanContext() = default;

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void Init(IWindowWidget& widget, bool enableValidation);
    void Destroy();

    VkInstance   Instance() const;
    VkSurfaceKHR Surface()  const;

    size_t         DeviceCount()  const;
    DeviceContext& Device(size_t i) const;
    DeviceContext& PrimaryDevice() const;
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

    VkInstance               m_instance          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger    = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface           = VK_NULL_HANDLE;
    bool                     m_validationEnabled = false;
    std::vector<std::unique_ptr<DeviceContext>> m_devices;
};

} // namespace xcel
