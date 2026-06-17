#include "Renderer/ForwardRenderPass.h"
#include "Renderer/RenderGraphConfig.h"
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

static PipelineConfig MakePipelineConfig(const PipelineDescriptor& desc)
{
    PipelineConfig cfg{};
    cfg.depthTestEnable  = desc.depthTest;
    cfg.depthWriteEnable = desc.depthWrite;
    cfg.cullMode         = desc.cullMode;
    cfg.pushConstantSize = static_cast<uint32_t>(sizeof(MaterialData));
    if (desc.blendMode != BlendMode::Opaque)
    {
        cfg.alphaBlend     = true;
        cfg.srcColorFactor = desc.srcColor;
        cfg.dstColorFactor = desc.dstColor;
        cfg.srcAlphaFactor = desc.srcAlpha;
        cfg.dstAlphaFactor = desc.dstAlpha;
    }
    return cfg;
}

void ForwardRenderPass::Build(const BuildPassInfo& info)
{
    m_descriptorLayout = info.uboLayout;
    m_bindlessLayout   = info.bindlessLayout;
    m_shaderDir        = info.shaderDir;

    const ForwardPassConfig& fwdCfg = info.forwardConfig
        ? *info.forwardConfig
        : RenderGraphConfig::Default().passes[1].forwardConfig.value();

    m_clearColor    = fwdCfg.clearColor;
    m_pipelineDescs = fwdCfg.pipelines;
    m_pipelines.resize(m_pipelineDescs.size());
    m_opaquePipelineIdx.reset();

    auto   layouts = MakeLayouts(info.uboLayout, info.bindlessLayout);
    const VkDevice dev = info.dev->Device();

    for (size_t i = 0; i < m_pipelineDescs.size(); ++i)
    {
        const auto& desc = m_pipelineDescs[i];
        const std::string vert = info.shaderDir + desc.vertShader;
        const std::string frag = info.shaderDir + desc.fragShader;
        m_pipelines[i].Create(dev, info.forwardRenderPass,
                              std::span{layouts}, info.extent,
                              vert, frag, MakePipelineConfig(desc));
        if (!m_opaquePipelineIdx.has_value() &&
            desc.blendMode   == BlendMode::Opaque &&
            desc.renderLayer == RenderLayer::Opaque)
        {
            m_opaquePipelineIdx = i;
        }
    }
}

void ForwardRenderPass::Rebuild(DeviceContext& dev, VkExtent2D newExtent)
{
    for (auto& p : m_pipelines)
        p.Destroy(dev.Device());

    auto   layouts = MakeLayouts(m_descriptorLayout, m_bindlessLayout);
    const VkDevice d = dev.Device();

    for (size_t i = 0; i < m_pipelineDescs.size(); ++i)
    {
        const auto& desc = m_pipelineDescs[i];
        const std::string vert = m_shaderDir + desc.vertShader;
        const std::string frag = m_shaderDir + desc.fragShader;
        m_pipelines[i].Create(d, m_renderPass.GetHandle(),
                              std::span{layouts}, newExtent,
                              vert, frag, MakePipelineConfig(desc));
    }
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
    clearValues[0].color        = {{m_clearColor[0], m_clearColor[1],
                                    m_clearColor[2], m_clearColor[3]}};
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

    if (m_pipelines.empty())
    {
        vkCmdEndRenderPass(cmd);
        return;
    }

    // Bind descriptor sets once; all pipelines share the same layout.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelines[0].PipelineLayout(),
                            0, 1, &ctx.uboDescriptorSet, 0, nullptr);
    if (ctx.bindlessDescriptorSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelines[0].PipelineLayout(),
                                1, 1, &ctx.bindlessDescriptorSet, 0, nullptr);

    // ── Opaque layer ─────────────────────────────────────────────────────────
    // GPU-indirect path (FrustumCullPass) only applies to opaque draws because
    // the compute cull pass has no per-draw blend-mode concept.
    if (m_opaquePipelineIdx.has_value())
    {
        Pipeline& opaquePipeline = m_pipelines[*m_opaquePipelineIdx];
        if (ctx.indirectDrawCount > 0 && ctx.drawCountBuffer != VK_NULL_HANDLE)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              opaquePipeline.GetHandle());
            for (uint32_t i = 0; i < ctx.indirectDrawCount; ++i)
            {
                const auto& dc = ctx.directDrawCalls[i];
                if (dc.renderLayer != RenderLayer::Opaque) continue;
                vkCmdPushConstants(cmd, opaquePipeline.PipelineLayout(),
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
                              opaquePipeline.GetHandle());
            for (const auto& dc : ctx.directDrawCalls)
            {
                if (dc.renderLayer != RenderLayer::Opaque) continue;
                EmitDraw(cmd, dc, opaquePipeline.PipelineLayout());
            }
        }
    }

    // ── Non-opaque layers (direct path only, sorted back-to-front by caller) ─
    for (size_t i = 0; i < m_pipelineDescs.size(); ++i)
    {
        const auto& desc = m_pipelineDescs[i];
        if (desc.renderLayer == RenderLayer::Opaque) continue;
        RecordLayer(cmd, ctx, desc.renderLayer, desc.blendMode, m_pipelines[i]);
    }

    vkCmdEndRenderPass(cmd);
}

void ForwardRenderPass::Destroy(VkDevice device)
{
    for (auto& p : m_pipelines)
        p.Destroy(device);
    m_pipelines.clear();
    m_pipelineDescs.clear();
    m_opaquePipelineIdx.reset();
    m_renderPass.Destroy(device);
}

} // namespace xcel
