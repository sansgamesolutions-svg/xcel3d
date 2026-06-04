#pragma once
#include "Graphics/DeviceContext.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>

namespace xcel {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

class Swapchain {
public:
    Swapchain();
    ~Swapchain();

    void Create(DeviceContext& dev, VkSurfaceKHR surface, GLFWwindow* window, VkRenderPass renderPass);
    void Destroy(VkDevice device);
    void Recreate(DeviceContext& dev, VkSurfaceKHR surface, GLFWwindow* window, VkRenderPass renderPass);

    VkSwapchainKHR GetHandle()          const;
    VkFormat       ImageFormat()        const;
    VkExtent2D     Extent()             const;
    uint32_t       ImageCount()         const;
    VkFramebuffer  Framebuffer(uint32_t i) const;

    static SwapchainSupportDetails QuerySupport(VkPhysicalDevice dev, VkSurfaceKHR surface);

private:
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available);
    VkPresentModeKHR   ChoosePresentMode(const std::vector<VkPresentModeKHR>& available);
    VkExtent2D         ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window);
    void               CreateImageViews(VkDevice device);
    void               CreateDepthResources(DeviceContext& dev);
    void               CreateFramebuffers(VkDevice device, VkRenderPass renderPass);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
