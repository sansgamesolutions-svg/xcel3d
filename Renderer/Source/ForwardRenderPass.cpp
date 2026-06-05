#include "Renderer/ForwardRenderPass.h"
#include "Renderer/Pipeline.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/DeviceContext.h"
#include <stdexcept>
#include <array>
#include <vector>

namespace xcel {

struct ForwardRenderPass::Impl {
    std::string  vertSpv;
    std::string  fragSpv;
    Pipeline     pipeline;

    VkRenderPass          storedRenderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout storedLayout     = VK_NULL_HANDLE;

    // CPU-side draw calls, used both for direct drawing and for VBO/IBO binding in indirect mode.
    std::vector<DrawCall> drawCalls;
};

ForwardRenderPass::ForwardRenderPass(std::string vertSpv, std::string fragSpv)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->vertSpv = std::move(vertSpv);
    m_impl->fragSpv = std::move(fragSpv);
}

ForwardRenderPass::~ForwardRenderPass() = default;

void ForwardRenderPass::Build(DeviceContext& dev, const BuildPassInfo& info)
{
    m_impl->storedRenderPass = info.swapchainRenderPass;
    m_impl->storedLayout     = info.frameLayout;

    m_impl->pipeline.Create(
        dev.Device(),
        info.swapchainRenderPass,
        info.frameLayout,
        info.extent,
        m_impl->vertSpv,
        m_impl->fragSpv);
}

void ForwardRenderPass::Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass newRP)
{
    m_impl->storedRenderPass = newRP;
    m_impl->pipeline.Destroy(dev.Device());
    m_impl->pipeline.Create(
        dev.Device(),
        newRP,
        m_impl->storedLayout,
        newExtent,
        m_impl->vertSpv,
        m_impl->fragSpv);
}

void ForwardRenderPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    if (m_impl->drawCalls.empty()) return;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.15f, 0.15f, 0.15f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_impl->storedRenderPass;
    rpInfo.framebuffer       = ctx.framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = ctx.extent;
    rpInfo.clearValueCount   = (uint32_t)clearValues.size();
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_impl->pipeline.GetHandle());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_impl->pipeline.PipelineLayout(), 0, 1,
                            &ctx.frameDescSet, 0, nullptr);

    // When a culling pass has run, indirectDrawBuffer is populated.
    // Slot i in the indirect buffer corresponds to drawCalls[i].
    // Culled objects get instanceCount=0; GPU skips them automatically.
    const bool indirect = (ctx.indirectDrawBuffer != VK_NULL_HANDLE);

    for (size_t i = 0; i < m_impl->drawCalls.size(); ++i) {
        const auto& dc = m_impl->drawCalls[i];
        VkBuffer     bufs[2] = {dc.vertexBuffer->Buffer(), dc.instanceBuffer->Buffer()};
        VkDeviceSize offs[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
        vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);

        if (indirect) {
            VkDeviceSize offset = i * sizeof(VkDrawIndexedIndirectCommand);
            vkCmdDrawIndexedIndirect(cmd, ctx.indirectDrawBuffer, offset, 1, 0);
        } else {
            vkCmdDrawIndexed(cmd, dc.indexCount, dc.instanceCount, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
}

void ForwardRenderPass::Destroy(VkDevice device)
{
    m_impl->pipeline.Destroy(device);
}

void ForwardRenderPass::SetDrawCalls(std::span<const DrawCall> drawCalls)
{
    m_impl->drawCalls.assign(drawCalls.begin(), drawCalls.end());
}

void ForwardRenderPass::SetIndirectVertexIndexBuffers(std::span<const DrawCall> drawCalls)
{
    m_impl->drawCalls.assign(drawCalls.begin(), drawCalls.end());
}

} // namespace xcel
