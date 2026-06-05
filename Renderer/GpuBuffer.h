#pragma once
#include "Renderer/DeviceContext.h"
#include <cstddef>
#include <memory>

namespace xcel {

class GpuBuffer {
public:
    GpuBuffer();
    ~GpuBuffer();

    GpuBuffer(const GpuBuffer&)            = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;
    GpuBuffer(GpuBuffer&&) noexcept;
    GpuBuffer& operator=(GpuBuffer&&) noexcept;

    void Create(
        VkDevice              device,
        VkPhysicalDevice      physicalDevice,
        VkDeviceSize          size,
        VkBufferUsageFlags    usage,
        VkMemoryPropertyFlags memProps
    );

    // Upload data through a temporary host-visible staging buffer.
    void UploadViaStaging(
        DeviceContext& dev,
        const void*    data,
        VkDeviceSize   size
    );

    // Direct write for persistently mapped host-visible buffers (UBOs).
    void WriteHostVisible(const void* data, VkDeviceSize size);

    void Destroy(VkDevice device);

    VkBuffer     Buffer()     const;
    VkDeviceSize BufferSize() const;

private:
    uint32_t FindMemoryType(
        VkPhysicalDevice      physicalDevice,
        uint32_t              typeFilter,
        VkMemoryPropertyFlags properties
    );

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
