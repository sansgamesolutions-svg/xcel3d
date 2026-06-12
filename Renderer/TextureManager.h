#pragma once
#include "Renderer/DeviceContext.h"
#include "Renderer/GpuImage.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace xcel {

class TextureManager
{
public:
    static constexpr uint32_t MAX_TEXTURES = 1024;
    static constexpr uint32_t NO_TEXTURE   = 0xFFFFFFFFu;

    TextureManager()  = default;
    ~TextureManager() = default;

    TextureManager(const TextureManager&)            = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    void Create(DeviceContext& dev);
    void Destroy(VkDevice device);

    // Upload RGBA8 pixels from CPU. Returns texture index; store it on the entity.
    uint32_t Upload(DeviceContext& dev, uint32_t width, uint32_t height, const void* pixels);
    void     Free(VkDevice device, uint32_t index);

    VkDescriptorSetLayout Layout()        const;
    VkDescriptorSet       DescriptorSet() const;

private:
    std::vector<GpuImage>    m_images;
    std::vector<bool>        m_occupied;
    VkSampler                m_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout    m_layout  = VK_NULL_HANDLE;
    VkDescriptorPool         m_pool    = VK_NULL_HANDLE;
    VkDescriptorSet          m_set     = VK_NULL_HANDLE;
};

} // namespace xcel
