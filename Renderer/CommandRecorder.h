#pragma once
#include "Renderer/DeviceContext.h"
#include "Renderer/DescriptorManager.h"
#include <memory>
#include <span>

namespace xcel {

class Pipeline;
class GpuBuffer;

struct DrawCall {
    const GpuBuffer* vertexBuffer;
    const GpuBuffer* indexBuffer;
    uint32_t         indexCount;
    // Instance data — always non-null at submission time.
    // Non-instanced draws use WindowContext's shared identity-matrix buffer.
    const GpuBuffer* instanceBuffer  = nullptr;
    uint32_t         instanceCount   = 1;
};

class CommandRecorder {
public:
    CommandRecorder();
    ~CommandRecorder();

    void Create(DeviceContext& dev);
    void Destroy(VkDevice device);

    void Record(
        uint32_t                frameIndex,
        VkFramebuffer           framebuffer,
        VkExtent2D              extent,
        VkRenderPass            renderPass,
        Pipeline&               pipeline,
        DescriptorManager&      descriptors,
        std::span<const DrawCall> drawCalls
    );

    const VkCommandBuffer& CommandBuffer(uint32_t i) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
