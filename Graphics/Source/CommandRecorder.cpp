#include "Graphics/CommandRecorder.h"
#include "Graphics/Pipeline.h"
#include "Graphics/GpuBuffer.h"
#include <stdexcept>
#include <array>
#include <vector>

namespace xcel {

struct CommandRecorder::Impl {
    std::vector<VkCommandBuffer> cmdBuffers;
};

CommandRecorder::CommandRecorder()
    : m_impl(std::make_unique<Impl>()) {}

CommandRecorder::~CommandRecorder() = default;

void CommandRecorder::Create(DeviceContext& dev) {
    m_impl->cmdBuffers.resize(DescriptorManager::MAX_FRAMES);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = dev.GraphicsCommandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_impl->cmdBuffers.size();

    if (vkAllocateCommandBuffers(dev.Device(), &allocInfo, m_impl->cmdBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("CommandRecorder: vkAllocateCommandBuffers failed");
}

void CommandRecorder::Destroy(VkDevice device) {
    (void)device;
    m_impl->cmdBuffers.clear();
}

void CommandRecorder::Record(
    uint32_t                  frameIndex,
    VkFramebuffer             framebuffer,
    VkExtent2D                extent,
    VkRenderPass              renderPass,
    Pipeline&                 pipeline,
    DescriptorManager&        descriptors,
    std::span<const DrawCall> drawCalls)
{
    VkCommandBuffer cmd = m_impl->cmdBuffers[frameIndex];

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("CommandRecorder: vkBeginCommandBuffer failed");

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.15f, 0.15f, 0.15f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = renderPass;
    rpInfo.framebuffer       = framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = extent;
    rpInfo.clearValueCount   = (uint32_t)clearValues.size();
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetHandle());

    VkDescriptorSet ds = descriptors.DescriptorSet(frameIndex);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.PipelineLayout(), 0, 1, &ds, 0, nullptr);

    for (const auto& dc : drawCalls) {
        VkBuffer     vb     = dc.vertexBuffer->Buffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
        vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, dc.indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("CommandRecorder: vkEndCommandBuffer failed");
}

const VkCommandBuffer& CommandRecorder::CommandBuffer(uint32_t i) const {
    return m_impl->cmdBuffers[i];
}

} // namespace xcel
