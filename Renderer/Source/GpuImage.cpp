#include "Renderer/GpuImage.h"
#include "Renderer/GpuBuffer.h"
#include <stdexcept>
#include <utility>

namespace xcel {

GpuImage::GpuImage(GpuImage&& other) noexcept
    : m_image(std::exchange(other.m_image, VK_NULL_HANDLE))
    , m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE))
    , m_imageView(std::exchange(other.m_imageView, VK_NULL_HANDLE))
{}

GpuImage& GpuImage::operator=(GpuImage&& other) noexcept
{
    if (this != &other) {
        m_image     = std::exchange(other.m_image, VK_NULL_HANDLE);
        m_memory    = std::exchange(other.m_memory, VK_NULL_HANDLE);
        m_imageView = std::exchange(other.m_imageView, VK_NULL_HANDLE);
    }
    return *this;
}

void GpuImage::Create(
    DeviceContext&     dev,
    uint32_t           width,
    uint32_t           height,
    VkFormat           format,
    VkImageUsageFlags  usage,
    VkImageAspectFlags aspect)
{
    if (m_image != VK_NULL_HANDLE)
        throw std::logic_error("GpuImage: Create called on an already-created image");

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = format;
    imageInfo.extent        = {width, height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = usage;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(dev.Device(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
        throw std::runtime_error("GpuImage: vkCreateImage failed");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(dev.Device(), m_image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = GpuBuffer::FindMemoryType(
        dev.PhysicalDevice(), memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(dev.Device(), &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        vkDestroyImage(dev.Device(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        throw std::runtime_error("GpuImage: vkAllocateMemory failed");
    }

    if (vkBindImageMemory(dev.Device(), m_image, m_memory, 0) != VK_SUCCESS) {
        Destroy(dev.Device());
        throw std::runtime_error("GpuImage: vkBindImageMemory failed");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspect;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(dev.Device(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        Destroy(dev.Device());
        throw std::runtime_error("GpuImage: vkCreateImageView failed");
    }
}

void GpuImage::Destroy(VkDevice device)
{
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

VkImage     GpuImage::Image()     const { return m_image;     }
VkImageView GpuImage::ImageView() const { return m_imageView; }

} // namespace xcel
