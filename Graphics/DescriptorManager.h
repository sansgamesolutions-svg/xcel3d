#pragma once
#include "Graphics/DeviceContext.h"
#include "Graphics/GpuBuffer.h"
#include <vector>

namespace xcel {

struct alignas(16) FrameUBO {
    float model[16];        // offset   0
    float view[16];         // offset  64
    float proj[16];         // offset 128
    float lightPos[3];      // offset 192
    float _pad0;
    float lightColor[3];    // offset 208
    float _pad1;
    float viewPos[3];       // offset 224
    float _pad2;
    // total: 240 bytes
};

class DescriptorManager
{
public:
    static constexpr int MAX_FRAMES = 2;

    DescriptorManager()  = default;
    ~DescriptorManager() = default;

    DescriptorManager(const DescriptorManager&)            = delete;
    DescriptorManager& operator=(const DescriptorManager&) = delete;

    void Create(DeviceContext& dev);
    void Destroy(VkDevice device);

    void UpdateUBO(uint32_t frameIndex, const FrameUBO& data);

    VkDescriptorSetLayout Layout()                  const;
    VkDescriptorSet       DescriptorSet(uint32_t i) const;

private:
    VkDescriptorSetLayout        m_setLayout  = VK_NULL_HANDLE;
    VkDescriptorPool             m_pool       = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_sets;
    std::vector<GpuBuffer>       m_uboBuffers;
};

} // namespace xcel
