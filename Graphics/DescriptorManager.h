#pragma once
#include "Graphics/DeviceContext.h"
#include "Graphics/GpuBuffer.h"
#include <vector>

namespace xcel {

static constexpr uint32_t MAX_LIGHTS = 8;

// GPU light record — two vec4s, matching GLSL std140 exactly (32 bytes each).
struct alignas(16) LightGpu {
    float positionAndIntensity[4]; // xyz = world position, w = intensity
    float colorAndPad[4];          // xyz = linear RGB color, w = 0
};

// C++ layout matches GLSL std140 exactly.  See Shaders/mesh.vert for the GLSL mirror.
// Offsets: model=0, view=64, proj=128, viewPos=192, lightCount=208, lights[8]=224. Total=480.
struct alignas(16) FrameUBO {
    float    model[16];
    float    view[16];
    float    proj[16];
    float    viewPos[3];           // offset 192
    float    _viewPad;             // offset 204 — pads vec3 to 16 bytes (GLSL std140)
    uint32_t lightCount;           // offset 208
    float    _pad[3];              // offset 212 — 12-byte pad to reach 16-byte boundary at 224
    LightGpu lights[MAX_LIGHTS];   // offset 224, 8 × 32 = 256 bytes
    // total: 480 bytes
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
