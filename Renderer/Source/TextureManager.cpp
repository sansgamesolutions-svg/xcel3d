#include "Renderer/TextureManager.h"
#include "Renderer/GpuBuffer.h"
#include <array>
#include <stdexcept>

namespace xcel {

void TextureManager::Create(DeviceContext& dev)
{
    m_slots.resize(MAX_TEXTURES);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.maxAnisotropy           = 1.0f;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = 0.0f;

    if (vkCreateSampler(dev.Device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        throw std::runtime_error("TextureManager: vkCreateSampler failed");

    std::array<VkDescriptorBindingFlags, 2> bindingFlags = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        0u
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
    flagsInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsInfo.bindingCount  = static_cast<uint32_t>(bindingFlags.size());
    flagsInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutBinding imageBinding{};
    imageBinding.binding            = 0;
    imageBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageBinding.descriptorCount    = MAX_TEXTURES;
    imageBinding.stageFlags         = VK_SHADER_STAGE_ALL;
    imageBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding            = 1;
    samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerBinding.descriptorCount    = 1;
    samplerBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerBinding.pImmutableSamplers = &m_sampler;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {imageBinding, samplerBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext        = &flagsInfo;
    layoutInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(dev.Device(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
        throw std::runtime_error("TextureManager: vkCreateDescriptorSetLayout failed");

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURES};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_SAMPLER, 1};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    if (vkCreateDescriptorPool(dev.Device(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
        throw std::runtime_error("TextureManager: vkCreateDescriptorPool failed");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_layout;

    if (vkAllocateDescriptorSets(dev.Device(), &allocInfo, &m_set) != VK_SUCCESS)
        throw std::runtime_error("TextureManager: vkAllocateDescriptorSets failed");
}

void TextureManager::Destroy(VkDevice device)
{
    for (auto& slot : m_slots)
        if (slot) slot->Destroy(device);
    m_slots.clear();

    m_set = VK_NULL_HANDLE; // freed with the pool

    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
}

uint32_t TextureManager::Upload(
    DeviceContext& dev,
    uint32_t       width,
    uint32_t       height,
    const void*    pixels)
{
    uint32_t slot = NO_TEXTURE;
    for (uint32_t i = 0; i < MAX_TEXTURES; ++i) {
        if (!m_slots[i]) { slot = i; break; }
    }
    if (slot == NO_TEXTURE)
        throw std::runtime_error("TextureManager: MAX_TEXTURES slots exhausted");

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

    GpuBuffer staging;
    staging.Create(
        dev.Device(), dev.PhysicalDevice(), imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.WriteHostVisible(pixels, imageSize);

    m_slots[slot].emplace().Create(
        dev, width, height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    VkCommandBuffer cmd = dev.BeginSingleTimeCommands(DeviceContext::QueueType::Transfer);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image                           = m_slots[slot]->Image();
    toTransfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel   = 0;
    toTransfer.subresourceRange.levelCount     = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount     = 1;
    toTransfer.srcAccessMask                   = 0;
    toTransfer.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, staging.Buffer(), m_slots[slot]->Image(),
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier toShader      = toTransfer;
    toShader.oldLayout                 = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout                 = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcAccessMask             = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask             = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toShader);

    dev.EndSingleTimeCommands(cmd, DeviceContext::QueueType::Transfer);
    staging.Destroy(dev.Device());

    VkDescriptorImageInfo imgInfo{};
    imgInfo.sampler     = VK_NULL_HANDLE;
    imgInfo.imageView   = m_slots[slot]->ImageView();
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = m_set;
    write.dstBinding      = 0;
    write.dstArrayElement = slot;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo      = &imgInfo;

    vkUpdateDescriptorSets(dev.Device(), 1, &write, 0, nullptr);

    return slot;
}

void TextureManager::Free(VkDevice device, uint32_t index)
{
    if (index >= MAX_TEXTURES || !m_slots[index]) return;
    m_slots[index]->Destroy(device);
    m_slots[index].reset();
}

VkDescriptorSetLayout TextureManager::Layout()        const { return m_layout; }
VkDescriptorSet       TextureManager::DescriptorSet() const { return m_set;    }

} // namespace xcel
