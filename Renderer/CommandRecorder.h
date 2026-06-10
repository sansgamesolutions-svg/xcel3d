#pragma once
#include "Renderer/DeviceContext.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/DrawCall.h"
#include <vector>
#include <span>

namespace xcel {

class Pipeline;

class CommandRecorder
{
public:
    CommandRecorder()  = default;
    ~CommandRecorder() = default;

    void Create(DeviceContext& dev);
    void Destroy(VkDevice device);

    void Record(
        uint32_t                  frameIndex,
        VkFramebuffer             framebuffer,
        VkExtent2D                extent,
        VkRenderPass              renderPass,
        Pipeline&                 pipeline,
        DescriptorManager&        descriptors,
        std::span<const DrawCall> drawCalls);

    const VkCommandBuffer& CommandBuffer(uint32_t i) const;

private:
    std::vector<VkCommandBuffer> m_cmdBuffers;
};

} // namespace xcel
