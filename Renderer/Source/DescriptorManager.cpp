#include "Renderer/DescriptorManager.h"
#include "Renderer/GpuBuffer.h"
#include <stdexcept>
#include <array>
#include <vector>

namespace xcel {

struct DescriptorManager::Impl {
    VkDescriptorSetLayout        setLayout = VK_NULL_HANDLE;
    VkDescriptorPool             pool      = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> sets;
    std::vector<GpuBuffer>       uboBuffers;
};

DescriptorManager::DescriptorManager()
    : m_impl(std::make_unique<Impl>()) {}

DescriptorManager::~DescriptorManager() = default;

void DescriptorManager::Create(DeviceContext& dev)
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(dev.Device(), &layoutInfo, nullptr, &m_impl->setLayout) != VK_SUCCESS)
        throw std::runtime_error("DescriptorManager: vkCreateDescriptorSetLayout failed");

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = (uint32_t)MAX_FRAMES;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = (uint32_t)MAX_FRAMES;

    if (vkCreateDescriptorPool(dev.Device(), &poolInfo, nullptr, &m_impl->pool) != VK_SUCCESS)
        throw std::runtime_error("DescriptorManager: vkCreateDescriptorPool failed");

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES, m_impl->setLayout);
    m_impl->sets.resize(MAX_FRAMES);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_impl->pool;
    allocInfo.descriptorSetCount = (uint32_t)MAX_FRAMES;
    allocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(dev.Device(), &allocInfo, m_impl->sets.data()) != VK_SUCCESS)
        throw std::runtime_error("DescriptorManager: vkAllocateDescriptorSets failed");

    m_impl->uboBuffers.resize(MAX_FRAMES);
    VkDeviceSize uboSize = sizeof(FrameUBO);

    for (int i = 0; i < MAX_FRAMES; ++i) {
        m_impl->uboBuffers[i].Create(
            dev.Device(), dev.PhysicalDevice(), uboSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_impl->uboBuffers[i].Buffer();
        bufInfo.offset = 0;
        bufInfo.range  = uboSize;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_impl->sets[i];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufInfo;

        vkUpdateDescriptorSets(dev.Device(), 1, &write, 0, nullptr);
    }
}

void DescriptorManager::UpdateUBO(uint32_t frameIndex, const FrameUBO& data)
{
    m_impl->uboBuffers[frameIndex].WriteHostVisible(&data, sizeof(FrameUBO));
}

void DescriptorManager::Destroy(VkDevice device)
{
    for (auto& buf : m_impl->uboBuffers)
        buf.Destroy(device);
    m_impl->uboBuffers.clear();

    if (m_impl->pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_impl->pool, nullptr);
        m_impl->pool = VK_NULL_HANDLE;
    }
    if (m_impl->setLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_impl->setLayout, nullptr);
        m_impl->setLayout = VK_NULL_HANDLE;
    }
    m_impl->sets.clear();
}

VkDescriptorSetLayout DescriptorManager::Layout()            const { return m_impl->setLayout; }
VkDescriptorSet       DescriptorManager::DescriptorSet(uint32_t i) const { return m_impl->sets[i]; }

} // namespace xcel
