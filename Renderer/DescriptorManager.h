#pragma once
#include "Renderer/DeviceContext.h"
#include <memory>

namespace xcel {

// Must match the uniform block declared in both shaders (std140 layout).
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

class DescriptorManager {
public:
    static constexpr int MAX_FRAMES = 2;

    DescriptorManager();
    ~DescriptorManager();

    void Create(DeviceContext& dev);
    void Destroy(VkDevice device);

    void UpdateUBO(uint32_t frameIndex, const FrameUBO& data);

    VkDescriptorSetLayout Layout()            const;
    VkDescriptorSet       DescriptorSet(uint32_t i) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
