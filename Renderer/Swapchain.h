#pragma once
#include "Renderer/DeviceContext.h"
#include "Platforms/IWindowWidget.h"
#include <vector>

namespace xcel {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

class Swapchain
{
public:
    Swapchain()  = default;
    ~Swapchain() = default;

    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void Create(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window,
                VkRenderPass renderPass, VkRenderPass overlayRenderPass = VK_NULL_HANDLE);
    void Destroy(VkDevice device);
    void Recreate(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window,
                  VkRenderPass renderPass, VkRenderPass overlayRenderPass = VK_NULL_HANDLE);

    VkSwapchainKHR GetHandle()                   const;
    VkFormat       ImageFormat()                 const;
    VkExtent2D     Extent()                      const;
    uint32_t       ImageCount()                  const;
    VkFramebuffer  Framebuffer(uint32_t i)        const;
    VkFramebuffer  OverlayFramebuffer(uint32_t i) const;

    static SwapchainSupportDetails QuerySupport(VkPhysicalDevice dev, VkSurfaceKHR surface);

private:
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available);
    VkPresentModeKHR   ChoosePresentMode(const std::vector<VkPresentModeKHR>& available);
    VkExtent2D         ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, IWindowWidget& window);
    void               CreateImageViews(VkDevice device);
    void               CreateDepthResources(DeviceContext& dev);
    void               CreateFramebuffers(VkDevice device, VkRenderPass renderPass);
    void               CreateFramebuffersInto(VkDevice device, VkRenderPass renderPass,
                                               std::vector<VkFramebuffer>& out);

    VkSwapchainKHR             m_swapchain             = VK_NULL_HANDLE;
    VkFormat                   m_imageFormat           = VK_FORMAT_UNDEFINED;
    VkExtent2D                 m_extent                = {0, 0};
    std::vector<VkImage>       m_images;
    std::vector<VkImageView>   m_imageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkFramebuffer> m_overlayFramebuffers;
    VkRenderPass               m_overlayRenderPass     = VK_NULL_HANDLE;
    VkImage                    m_depthImage            = VK_NULL_HANDLE;
    VkDeviceMemory             m_depthMemory           = VK_NULL_HANDLE;
    VkImageView                m_depthImageView        = VK_NULL_HANDLE;
    VkFormat                   m_depthFormat           = VK_FORMAT_D32_SFLOAT;
};

} // namespace xcel
