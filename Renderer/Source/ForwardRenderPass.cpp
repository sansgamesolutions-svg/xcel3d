#include "Renderer/ForwardRenderPass.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Pipeline.h"
#include "Renderer/GpuBuffer.h"
#include <stdexcept>
#include <array>
#include <span>
#include <vector>

namespace xcel {

void ForwardRenderPass::CreateRenderPass(DeviceContext& dev,
                                         VkFormat colorFormat,
                                         VkFormat depthFormat)
{
    m_renderPass.Create(dev.Device(), colorFormat, depthFormat);
}

VkRenderPass ForwardRenderPass::GetRenderPass() const
{
    return m_renderPass.GetHandle();
}

static std::vector<VkDescriptorSetLayout> MakeLayouts(
    VkDescriptorSetLayout ubo,
    VkDescriptorSetLayout bindless)
{
    std::vector<VkDescriptorSetLayout> layouts;
    layouts.push_back(ubo);
    if (bindless != VK_NULL_HANDLE)
        layouts.push_back(bindless);
    return layouts;
}

static PipelineConfig MakeBlendConfig(
    VkBlendFactor srcColor,
    VkBlendFactor dstColor,
    VkBlendFactor srcAlpha = VK_BLEND_FACTOR_ONE,
    VkBlendFactor dstAlpha = VK_BLEND_FACTOR_ZERO)
{
    PipelineConfig cfg{};
    cfg.depthWriteEnable = false;
    cfg.alphaBlend       = true;
    cfg.srcColorFactor   = srcColor;
    cfg.dstColorFactor   = dstColor;
    cfg.srcAlphaFactor   = srcAlpha;
    cfg.dstAlphaFactor   = dstAlpha;
    return cfg;
}

void ForwardRenderPass::Build(const BuildPassInfo& info)
{
    m_descriptorLayout = info.uboLayout;
    m_bindlessLayout   = info.bindlessLayout;
    m_shaderDir        = info.shaderDir;

    auto layouts = MakeLayouts(info.uboLayout, info.bindlessLayout);

    const std::string vert = info.shaderDir + "mesh.vert.spv";
    const std::string frag = info.shaderDir + "mesh.frag.spv";
    const VkDevice    dev  = info.dev->Device();

    // Opaque: defaults (depth write on, no blend)
    m_opaquePipeline.Create(dev, info.forwardRenderPass,
                            std::span{layouts}, info.extent, vert, frag);

    // AlphaBlend: SrcAlpha / OneMinusSrcAlpha
    m_alphaBlendPipeline.Create(dev, info.forwardRenderPass,
                                std::span{layouts}, info.extent, vert, frag,
                                MakeBlendConfig(VK_BLEND_FACTOR_SRC_ALPHA,
                                                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA));

    // Additive: One / One
    m_additivePipeline.Create(dev, info.forwardRenderPass,
                              std::span{layouts}, info.extent, vert, frag,
                              MakeBlendConfig(VK_BLEND_FACTOR_ONE,
                                             VK_BLEND_FACTOR_ONE));

    // Premultiplied: One / OneMinusSrcAlpha
    m_premultPipeline.Create(dev, info.forwardRenderPass,
                             std::span{layouts}, info.extent, vert, frag,
                             MakeBlendConfig(VK_BLEND_FACTOR_ONE,
                                            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA));
}

void ForwardRenderPass::Rebuild(DeviceContext& dev, VkExtent2D newExtent)
{
    m_opaquePipeline.Destroy(dev.Device());
    m_alphaBlendPipeline.Destroy(dev.Device());
    m_additivePipeline.Destroy(dev.Device());
    m_premultPipeline.Destroy(dev.Device());

    auto layouts = MakeLayouts(m_descriptorLayout, m_bindlessLayout);

    const std::string vert = m_shaderDir + "mesh.vert.spv";
    const std::string frag = m_shaderDir + "mesh.frag.spv";
    const VkDevice    d    = dev.Device();

    m_opaquePipeline.Create(d, m_renderPass.GetHandle(),
                            std::span{layouts}, newExtent, vert, frag);

    m_alphaBlendPipeline.Create(d, m_renderPass.GetHandle(),
                                std::span{layouts}, newExtent, vert, frag,
                                MakeBlendConfig(VK_BLEND_FACTOR_SRC_ALPHA,
                                                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA));

    m_additivePipeline.Create(d, m_renderPass.GetHandle(),
                              std::span{layouts}, newExtent, vert, frag,
                              MakeBlendConfig(VK_BLEND_FACTOR_ONE,
                                             VK_BLEND_FACTOR_ONE));

    m_premultPipeline.Create(d, m_renderPass.GetHandle(),
                             std::span{layouts}, newExtent, vert, frag,
                             MakeBlendConfig(VK_BLEND_FACTOR_ONE,
                                            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA));
}

void ForwardRenderPass::EmitDraw(VkCommandBuffer cmd,
                                 const DrawCall& dc,
                                 VkPipelineLayout layout) const
{
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MaterialData), &dc.material);
    VkBuffer     bufs[2] = {dc.vertexBuffer->Buffer(), dc.instanceBuffer->Buffer()};
    VkDeviceSize offs[2] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
    vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, dc.indexCount, dc.instanceCount, 0, 0, 0);
}

void ForwardRenderPass::RecordLayer(VkCommandBuffer   cmd,
                                    const PassContext& ctx,
                                    RenderLayer        layer,
                                    BlendMode          mode,
                                    Pipeline&          pipeline)
{
    bool any = false;
    for (const auto& dc : ctx.directDrawCalls)
        if (dc.renderLayer == layer && dc.blendMode == mode) { any = true; break; }
    if (!any) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetHandle());
    for (const auto& dc : ctx.directDrawCalls)
    {
        if (dc.renderLayer != layer || dc.blendMode != mode) continue;
        EmitDraw(cmd, dc, pipeline.PipelineLayout());
    }
}

void ForwardRenderPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.15f, 0.15f, 0.15f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_renderPass.GetHandle();
    rpInfo.framebuffer       = ctx.swapchainFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = ctx.extent;
    rpInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x        = 0.f;
    vp.y        = 0.f;
    vp.width    = static_cast<float>(ctx.extent.width);
    vp.height   = static_cast<float>(ctx.extent.height);
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = ctx.extent;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    // Bind descriptor sets once; layout is shared by all four pipelines.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_opaquePipeline.PipelineLayout(),
                            0, 1, &ctx.uboDescriptorSet, 0, nullptr);
    if (ctx.bindlessDescriptorSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_opaquePipeline.PipelineLayout(),
                                1, 1, &ctx.bindlessDescriptorSet, 0, nullptr);

    // ── Opaque layer ─────────────────────────────────────────────────────────
    // GPU-indirect path (FrustumCullPass) only applies to opaque draws because
    // the compute cull pass has no per-draw blend-mode concept.
    if (ctx.indirectDrawCount > 0 && ctx.drawCountBuffer != VK_NULL_HANDLE)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_opaquePipeline.GetHandle());
        for (uint32_t i = 0; i < ctx.indirectDrawCount; ++i)
        {
            const auto& dc = ctx.directDrawCalls[i];
            if (dc.renderLayer != RenderLayer::Opaque) continue;
            vkCmdPushConstants(cmd, m_opaquePipeline.PipelineLayout(),
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(MaterialData), &dc.material);
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
    }
    else
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_opaquePipeline.GetHandle());
        for (const auto& dc : ctx.directDrawCalls)
        {
            if (dc.renderLayer != RenderLayer::Opaque) continue;
            EmitDraw(cmd, dc, m_opaquePipeline.PipelineLayout());
        }
    }

    // ── Transparent layer (direct path only, sorted back-to-front by caller) ─
    RecordLayer(cmd, ctx, RenderLayer::Transparent, BlendMode::AlphaBlend,    m_alphaBlendPipeline);
    RecordLayer(cmd, ctx, RenderLayer::Transparent, BlendMode::Additive,      m_additivePipeline);
    RecordLayer(cmd, ctx, RenderLayer::Transparent, BlendMode::Premultiplied, m_premultPipeline);

    // ── Overlay layer (reserved) ──────────────────────────────────────────────

    vkCmdEndRenderPass(cmd);
}

void ForwardRenderPass::Destroy(VkDevice device)
{
    m_opaquePipeline.Destroy(device);
    m_alphaBlendPipeline.Destroy(device);
    m_additivePipeline.Destroy(device);
    m_premultPipeline.Destroy(device);
    m_renderPass.Destroy(device);
}

} // namespace xcel
