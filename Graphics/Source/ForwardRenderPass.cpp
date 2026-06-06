#include "Graphics/ForwardRenderPass.h"
#include "Graphics/RenderPass.h"
#include "Graphics/Pipeline.h"
#include "Graphics/GpuBuffer.h"
#include <stdexcept>
#include <array>

namespace xcel {

struct ForwardRenderPass::Impl
{
    RenderPass            renderPass;
    Pipeline              pipeline;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    std::string           shaderDir;
};

ForwardRenderPass::ForwardRenderPass()
    : m_impl(std::make_unique<Impl>()) {}

ForwardRenderPass::~ForwardRenderPass() = default;

void ForwardRenderPass::CreateRenderPass(DeviceContext& dev,
                                         VkFormat colorFormat,
                                         VkFormat depthFormat)
{
    m_impl->renderPass.Create(dev.Device(), colorFormat, depthFormat);
}

VkRenderPass ForwardRenderPass::GetRenderPass() const
{
    return m_impl->renderPass.GetHandle();
}

void ForwardRenderPass::Build(const BuildPassInfo& info)
{
    m_impl->descriptorLayout = info.uboLayout;
    m_impl->shaderDir        = info.shaderDir;

    m_impl->pipeline.Create(
        info.dev->Device(),
        info.forwardRenderPass,
        info.uboLayout,
        info.extent,
        info.shaderDir + "mesh.vert.spv",
        info.shaderDir + "mesh.frag.spv");
}

void ForwardRenderPass::Rebuild(DeviceContext& dev, VkExtent2D newExtent)
{
    m_impl->pipeline.Destroy(dev.Device());
    m_impl->pipeline.Create(
        dev.Device(),
        m_impl->renderPass.GetHandle(),
        m_impl->descriptorLayout,
        newExtent,
        m_impl->shaderDir + "mesh.vert.spv",
        m_impl->shaderDir + "mesh.frag.spv");
}

void ForwardRenderPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.15f, 0.15f, 0.15f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_impl->renderPass.GetHandle();
    rpInfo.framebuffer       = ctx.swapchainFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = ctx.extent;
    rpInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_impl->pipeline.GetHandle());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_impl->pipeline.PipelineLayout(),
                            0, 1, &ctx.uboDescriptorSet, 0, nullptr);

    if (ctx.indirectDrawCount > 0 && ctx.drawCountBuffer != VK_NULL_HANDLE) {
        // Indirect path: FrustumCullPass has populated per-slot indirect cmd + count buffers.
        // Each slot corresponds to one direct draw call; GPU skips slots with count == 0.
        for (uint32_t i = 0; i < ctx.indirectDrawCount; ++i) {
            const auto& dc = ctx.directDrawCalls[i];
            VkBuffer     bufs[2] = {dc.vertexBuffer->Buffer(), dc.instanceBuffer->Buffer()};
            VkDeviceSize offs[2] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
            vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexedIndirectCount(
                cmd,
                ctx.indirectDrawBuffer,
                static_cast<VkDeviceSize>(i) * sizeof(VkDrawIndexedIndirectCommand),
                ctx.drawCountBuffer,
                static_cast<VkDeviceSize>(i) * sizeof(uint32_t),
                1,
                sizeof(VkDrawIndexedIndirectCommand));
        }
    } else {
        // Direct path: no culling active.
        for (const auto& dc : ctx.directDrawCalls) {
            VkBuffer     bufs[2] = {dc.vertexBuffer->Buffer(), dc.instanceBuffer->Buffer()};
            VkDeviceSize offs[2] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
            vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, dc.indexCount, dc.instanceCount, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
}

void ForwardRenderPass::Destroy(VkDevice device)
{
    m_impl->pipeline.Destroy(device);
    m_impl->renderPass.Destroy(device);
}

} // namespace xcel
