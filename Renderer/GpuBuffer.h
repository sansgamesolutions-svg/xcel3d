#pragma once
#include "Renderer/DeviceContext.h"

namespace xcel {

class GpuBuffer
{
public:
    GpuBuffer()  = default;
    ~GpuBuffer() = default;

    GpuBuffer(const GpuBuffer&)            = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;
    GpuBuffer(GpuBuffer&&) noexcept;
    GpuBuffer& operator=(GpuBuffer&&) noexcept;

    void Create(
        VkDevice              device,
        VkPhysicalDevice      physicalDevice,
        VkDeviceSize          size,
        VkBufferUsageFlags    usage,
        VkMemoryPropertyFlags memProps);

    void UploadViaStaging(DeviceContext& dev, const void* data, VkDeviceSize size);
    void WriteHostVisible(const void* data, VkDeviceSize size);
    void Destroy(VkDevice device);

    VkBuffer     Buffer()     const;
    VkDeviceSize BufferSize() const;

private:
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                            uint32_t         typeFilter,
                            VkMemoryPropertyFlags properties);

    VkBuffer       m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize   m_size   = 0;
    void*          m_mapped = nullptr;
};

} // namespace xcel
