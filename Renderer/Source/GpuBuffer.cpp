#include "Renderer/GpuBuffer.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <vector>

namespace xcel {

GpuBuffer::GpuBuffer(GpuBuffer&& other) noexcept
    : m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE))
    , m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE))
    , m_size(std::exchange(other.m_size, 0))
    , m_mapped(std::exchange(other.m_mapped, nullptr))
{}

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other) noexcept
{
    if (this != &other) {
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
        m_size   = std::exchange(other.m_size, 0);
        m_mapped = std::exchange(other.m_mapped, nullptr);
    }
    return *this;
}

uint32_t GpuBuffer::FindMemoryType(
    VkPhysicalDevice      physicalDevice,
    uint32_t              typeFilter,
    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("GpuBuffer: failed to find suitable memory type");
}

void GpuBuffer::Create(
    VkDevice              device,
    VkPhysicalDevice      physicalDevice,
    VkDeviceSize          size,
    VkBufferUsageFlags    usage,
    VkMemoryPropertyFlags memProps)
{
    if (m_buffer != VK_NULL_HANDLE || m_memory != VK_NULL_HANDLE)
        throw std::logic_error("GpuBuffer: Create called on an existing buffer");
    if (size == 0)
        throw std::invalid_argument("GpuBuffer: size must be greater than zero");

    m_size = size;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        m_size = 0;
        throw std::runtime_error("GpuBuffer: vkCreateBuffer failed");
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memReq.memoryTypeBits, memProps);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        m_size   = 0;
        throw std::runtime_error("GpuBuffer: vkAllocateMemory failed");
    }

    if (vkBindBufferMemory(device, m_buffer, m_memory, 0) != VK_SUCCESS) {
        Destroy(device);
        throw std::runtime_error("GpuBuffer: vkBindBufferMemory failed");
    }

    if ((memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        vkMapMemory(device, m_memory, 0, size, 0, &m_mapped) != VK_SUCCESS)
    {
        Destroy(device);
        throw std::runtime_error("GpuBuffer: vkMapMemory failed");
    }
}

void GpuBuffer::UploadViaStaging(
    DeviceContext& dev,
    const void*    data,
    VkDeviceSize   size)
{
    const GpuBufferUpload upload{this, data, size};
    UploadGpuBuffersViaStaging(dev, std::span{&upload, size_t{1}});
}

void GpuBuffer::WriteHostVisible(
    const void* data,
    VkDeviceSize size,
    VkDeviceSize offset)
{
    if (!m_mapped)
        throw std::runtime_error("GpuBuffer: buffer is not host visible");
    if (size == 0) return;
    if (!data && size != 0)
        throw std::invalid_argument("GpuBuffer: upload data is null");
    if (offset > m_size || size > m_size - offset)
        throw std::out_of_range("GpuBuffer: host write exceeds buffer size");

    auto* destination = static_cast<std::byte*>(m_mapped) + offset;
    std::memcpy(destination, data, static_cast<size_t>(size));
}

void UploadGpuBuffersViaStaging(
    DeviceContext& dev,
    std::span<const GpuBufferUpload> uploads)
{
    struct PendingCopy
    {
        const GpuBufferUpload* upload = nullptr;
        VkDeviceSize           offset = 0;
    };

    std::vector<PendingCopy> copies;
    copies.reserve(uploads.size());

    VkDeviceSize stagingSize = 0;
    for (const auto& upload : uploads) {
        if (!upload.destination || !upload.data || upload.size == 0)
            throw std::invalid_argument("GpuBuffer: invalid staging upload");
        if (upload.size > upload.destination->BufferSize())
            throw std::out_of_range("GpuBuffer: staging upload exceeds buffer size");
        if ((upload.size & 3u) != 0)
            throw std::invalid_argument(
                "GpuBuffer: Vulkan buffer copy size must be 4-byte aligned");

        stagingSize = (stagingSize + 3u) & ~VkDeviceSize{3u};
        copies.push_back({&upload, stagingSize});
        stagingSize += upload.size;
    }

    if (copies.empty()) return;

    GpuBuffer staging;
    staging.Create(
        dev.Device(), dev.PhysicalDevice(), stagingSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    for (const auto& copy : copies)
        staging.WriteHostVisible(copy.upload->data, copy.upload->size, copy.offset);

    VkCommandBuffer cmd = dev.BeginSingleTimeCommands();
    for (const auto& copy : copies) {
        VkBufferCopy region{};
        region.srcOffset = copy.offset;
        region.size      = copy.upload->size;
        vkCmdCopyBuffer(
            cmd, staging.Buffer(), copy.upload->destination->Buffer(), 1, &region);
    }
    dev.EndSingleTimeCommands(cmd);

    staging.Destroy(dev.Device());
}

void GpuBuffer::Destroy(VkDevice device)
{
    if (m_mapped) {
        vkUnmapMemory(device, m_memory);
        m_mapped = nullptr;
    }
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_size = 0;
}

VkBuffer     GpuBuffer::Buffer()     const { return m_buffer; }
VkDeviceSize GpuBuffer::BufferSize() const { return m_size; }

} // namespace xcel
