#include "Graphics/GpuBuffer.h"
#include <stdexcept>
#include <cstring>

namespace xcel {

struct GpuBuffer::Impl {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize   size   = 0;
    void*          mapped = nullptr;
};

GpuBuffer::GpuBuffer()
    : m_impl(std::make_unique<Impl>()) {}

GpuBuffer::~GpuBuffer() = default;

GpuBuffer::GpuBuffer(GpuBuffer&& other) noexcept
    : m_impl(std::move(other.m_impl))
{
    other.m_impl = std::make_unique<Impl>();
}

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
        other.m_impl = std::make_unique<Impl>();
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
    m_impl->size = size;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_impl->buffer) != VK_SUCCESS)
        throw std::runtime_error("GpuBuffer: vkCreateBuffer failed");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_impl->buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memReq.memoryTypeBits, memProps);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_impl->memory) != VK_SUCCESS)
        throw std::runtime_error("GpuBuffer: vkAllocateMemory failed");

    vkBindBufferMemory(device, m_impl->buffer, m_impl->memory, 0);

    if (memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        vkMapMemory(device, m_impl->memory, 0, size, 0, &m_impl->mapped);
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
    vkCmdCopyBuffer(cmd, staging.Buffer(), m_impl->buffer, 1, &copy);
    dev.EndSingleTimeCommands(cmd);

    staging.Destroy(dev.Device());
}

void GpuBuffer::WriteHostVisible(const void* data, VkDeviceSize size)
{
    if (m_impl->mapped)
        std::memcpy(m_impl->mapped, data, static_cast<size_t>(size));
}

void GpuBuffer::Destroy(VkDevice device)
{
    if (m_impl->mapped) {
        vkUnmapMemory(device, m_impl->memory);
        m_impl->mapped = nullptr;
    }
    if (m_impl->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_impl->buffer, nullptr);
        m_impl->buffer = VK_NULL_HANDLE;
    }
    if (m_impl->memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_impl->memory, nullptr);
        m_impl->memory = VK_NULL_HANDLE;
    }
    m_impl->size = 0;
}

VkBuffer     GpuBuffer::Buffer()     const { return m_impl->buffer; }
VkDeviceSize GpuBuffer::BufferSize() const { return m_impl->size; }

} // namespace xcel
