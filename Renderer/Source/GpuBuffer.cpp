#include "Renderer/GpuBuffer.h"
#include <stdexcept>
#include <cstring>

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
    m_size = size;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_buffer) != VK_SUCCESS)
        throw std::runtime_error("GpuBuffer: vkCreateBuffer failed");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memReq.memoryTypeBits, memProps);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS)
        throw std::runtime_error("GpuBuffer: vkAllocateMemory failed");

    vkBindBufferMemory(device, m_buffer, m_memory, 0);

    if (memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        vkMapMemory(device, m_memory, 0, size, 0, &m_mapped);
}

void GpuBuffer::UploadViaStaging(
    DeviceContext& dev,
    const void*    data,
    VkDeviceSize   size)
{
    GpuBuffer staging;
    staging.Create(
        dev.Device(), dev.PhysicalDevice(), size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    staging.WriteHostVisible(data, size);

    VkCommandBuffer cmd = dev.BeginSingleTimeCommands();
    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(cmd, staging.Buffer(), m_buffer, 1, &copy);
    dev.EndSingleTimeCommands(cmd);

    staging.Destroy(dev.Device());
}

void GpuBuffer::WriteHostVisible(const void* data, VkDeviceSize size)
{
    if (m_mapped)
        std::memcpy(m_mapped, data, static_cast<size_t>(size));
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
