#pragma once
#include "Renderer/DeviceContext.h"
#include <vulkan/vulkan.h>
#include <cstdint>

namespace xcel {

class GpuImage
{
public:
    GpuImage()  = default;
    ~GpuImage() = default;

    GpuImage(const GpuImage&)            = delete;
    GpuImage& operator=(const GpuImage&) = delete;
    GpuImage(GpuImage&&) noexcept;
    GpuImage& operator=(GpuImage&&) noexcept;

    void Create(
        DeviceContext&    dev,
        uint32_t          width,
        uint32_t          height,
        VkFormat          format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspect);

    void Destroy(VkDevice device);

    VkImage     Image()     const;
    VkImageView ImageView() const;

private:
    VkImage        m_image     = VK_NULL_HANDLE;
    VkDeviceMemory m_memory    = VK_NULL_HANDLE;
    VkImageView    m_imageView = VK_NULL_HANDLE;
};

} // namespace xcel
