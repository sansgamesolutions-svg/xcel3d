#pragma once
#include "Renderer/DeviceContext.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/DrawCall.h"
#include <memory>
#include <span>

namespace xcel {

class Pipeline;

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
