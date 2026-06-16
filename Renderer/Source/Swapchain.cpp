#include "Renderer/Swapchain.h"
#include <stdexcept>
#include <algorithm>
#include <array>
#include <limits>

namespace xcel {

SwapchainSupportDetails Swapchain::QuerySupport(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, nullptr);
    if (count) { details.formats.resize(count); vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, details.formats.data()); }

    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, nullptr);
    if (count) { details.presentModes.resize(count); vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, details.presentModes.data()); }

    return details;
}

VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available)
{
    for (const auto& fmt : available)
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmt;
    return available[0];
}

VkPresentModeKHR Swapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& available)
{
    for (auto mode : available)
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, IWindowWidget& window)
{
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return caps.currentExtent;

    int w, h;
    window.GetFramebufferSize(w, h);
    VkExtent2D ext = { (uint32_t)w, (uint32_t)h };
    ext.width  = std::clamp(ext.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return ext;
}

void Swapchain::CreateImageViews(VkDevice device)
{
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image                           = m_images[i];
        info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        info.format                          = m_imageFormat;
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(device, &info, nullptr, &m_imageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("Swapchain: vkCreateImageView failed");
    }
}

static uint32_t FindDepthMemoryType(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((filter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("Swapchain: no suitable depth memory type");
}

void Swapchain::CreateDepthResources(DeviceContext& dev)
{
    VkImageCreateInfo imgInfo{};
    imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.format        = m_depthFormat;
    imgInfo.extent        = {m_extent.width, m_extent.height, 1};
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(dev.Device(), &imgInfo, nullptr, &m_depthImage) != VK_SUCCESS)
        throw std::runtime_error("Swapchain: vkCreateImage (depth) failed");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(dev.Device(), m_depthImage, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindDepthMemoryType(
        dev.PhysicalDevice(), memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(dev.Device(), &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS)
        throw std::runtime_error("Swapchain: vkAllocateMemory (depth) failed");

    vkBindImageMemory(dev.Device(), m_depthImage, m_depthMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_depthImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = m_depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(dev.Device(), &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
        throw std::runtime_error("Swapchain: vkCreateImageView (depth) failed");
}

void Swapchain::CreateFramebuffersInto(VkDevice device, VkRenderPass renderPass,
                                        std::vector<VkFramebuffer>& out)
{
    out.resize(m_imageViews.size());
    for (size_t i = 0; i < m_imageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {m_imageViews[i], m_depthImageView};
        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = renderPass;
        info.attachmentCount = (uint32_t)attachments.size();
        info.pAttachments    = attachments.data();
        info.width           = m_extent.width;
        info.height          = m_extent.height;
        info.layers          = 1;
        if (vkCreateFramebuffer(device, &info, nullptr, &out[i]) != VK_SUCCESS)
            throw std::runtime_error("Swapchain: vkCreateFramebuffer failed");
    }
}

void Swapchain::CreateFramebuffers(VkDevice device, VkRenderPass renderPass)
{
    CreateFramebuffersInto(device, renderPass, m_framebuffers);
}

void Swapchain::Create(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window,
                       VkRenderPass renderPass, VkRenderPass overlayRenderPass)
{
    auto support         = QuerySupport(dev.PhysicalDevice(), surface);
    auto fmt             = ChooseSurfaceFormat(support.formats);
    auto mode            = ChoosePresentMode(support.presentModes);
    m_extent       = ChooseExtent(support.capabilities, window);
    m_imageFormat  = fmt.format;

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = surface;
    info.minImageCount    = imageCount;
    info.imageFormat      = fmt.format;
    info.imageColorSpace  = fmt.colorSpace;
    info.imageExtent      = m_extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilies[] = { dev.GraphicsFamily(), dev.PresentFamily() };
    if (dev.GraphicsFamily() != dev.PresentFamily()) {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = queueFamilies;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform   = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = mode;
    info.clipped        = VK_TRUE;
    info.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(dev.Device(), &info, nullptr, &m_swapchain) != VK_SUCCESS)
        throw std::runtime_error("Swapchain: vkCreateSwapchainKHR failed");

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(dev.Device(), m_swapchain, &count, nullptr);
    m_images.resize(count);
    vkGetSwapchainImagesKHR(dev.Device(), m_swapchain, &count, m_images.data());

    CreateImageViews(dev.Device());
    CreateDepthResources(dev);
    CreateFramebuffers(dev.Device(), renderPass);
    if (overlayRenderPass != VK_NULL_HANDLE) {
        m_overlayRenderPass = overlayRenderPass;
        CreateFramebuffersInto(dev.Device(), overlayRenderPass, m_overlayFramebuffers);
    }
}

void Swapchain::Destroy(VkDevice device)
{
    for (auto fb : m_overlayFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    m_overlayFramebuffers.clear();
    m_overlayRenderPass = VK_NULL_HANDLE;
    for (auto fb : m_framebuffers)   vkDestroyFramebuffer(device, fb, nullptr);
    if (m_depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, m_depthImageView, nullptr);
    if (m_depthImage     != VK_NULL_HANDLE) vkDestroyImage(device, m_depthImage, nullptr);
    if (m_depthMemory    != VK_NULL_HANDLE) vkFreeMemory(device, m_depthMemory, nullptr);
    for (auto iv : m_imageViews)     vkDestroyImageView(device, iv, nullptr);
    if (m_swapchain      != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, m_swapchain, nullptr);

    m_framebuffers.clear();
    m_imageViews.clear();
    m_images.clear();
    m_depthImageView = VK_NULL_HANDLE;
    m_depthImage     = VK_NULL_HANDLE;
    m_depthMemory    = VK_NULL_HANDLE;
    m_swapchain      = VK_NULL_HANDLE;
}

void Swapchain::Recreate(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window,
                          VkRenderPass renderPass, VkRenderPass overlayRenderPass)
{
    int w = 0, h = 0;
    while (w == 0 || h == 0) {
        window.GetFramebufferSize(w, h);
        window.WaitEvents();
    }
    vkDeviceWaitIdle(dev.Device());
    Destroy(dev.Device());
    Create(dev, surface, window, renderPass, overlayRenderPass);
}

VkSwapchainKHR Swapchain::GetHandle()                    const { return m_swapchain; }
VkFormat       Swapchain::ImageFormat()                  const { return m_imageFormat; }
VkExtent2D     Swapchain::Extent()                       const { return m_extent; }
uint32_t       Swapchain::ImageCount()                   const { return (uint32_t)m_imageViews.size(); }
VkFramebuffer  Swapchain::Framebuffer(uint32_t i)         const { return m_framebuffers[i]; }
VkFramebuffer  Swapchain::OverlayFramebuffer(uint32_t i)  const { return m_overlayFramebuffers[i]; }

} // namespace xcel
