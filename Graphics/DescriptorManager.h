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
// In std140, vec3 occupies 12 bytes (not 16); the next 4-byte-aligned field follows immediately.
// Offsets: model=0, view=64, proj=128, viewPos=192, lightCount=204, lights[8]=208,
//          sectionPlane=464 (16-byte aligned). Total=480.
struct alignas(16) FrameUBO {
    float    model[16];
    float    view[16];
    float    proj[16];
    float    viewPos[3];           // offset 192, 12 bytes
    uint32_t lightCount;           // offset 204
    LightGpu lights[MAX_LIGHTS];   // offset 208, 8 × 32 = 256 bytes
    float    sectionPlane[4];      // offset 464: xyz=world-normal, w=d; (0,0,0,0) = disabled
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
