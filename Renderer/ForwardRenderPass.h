#pragma once
#include "Renderer/IPass.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Pipeline.h"
#include "Renderer/DrawCall.h"
#include "Renderer/RenderOptions.h"
#include <string>

namespace xcel {

class ForwardRenderPass final : public IPass
{
public:
    ForwardRenderPass()  = default;
    ~ForwardRenderPass() = default;

    ForwardRenderPass(const ForwardRenderPass&)            = delete;
    ForwardRenderPass& operator=(const ForwardRenderPass&) = delete;

    void         CreateRenderPass(DeviceContext& dev, VkFormat colorFormat, VkFormat depthFormat);
    VkRenderPass GetRenderPass() const;

    void Build(const BuildPassInfo&)           override;
    void Rebuild(DeviceContext&, VkExtent2D)   override;
    void Record(VkCommandBuffer, PassContext&) override;
    void Destroy(VkDevice)                     override;

private:
    void EmitDraw(VkCommandBuffer cmd, const DrawCall& dc, VkPipelineLayout layout) const;
    void RecordLayer(VkCommandBuffer cmd, const PassContext& ctx,
                     RenderLayer layer, BlendMode mode, Pipeline& pipeline);

    RenderPass            m_renderPass;
    Pipeline              m_opaquePipeline;
    Pipeline              m_alphaBlendPipeline;
    Pipeline              m_additivePipeline;
    Pipeline              m_premultPipeline;
    VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bindlessLayout   = VK_NULL_HANDLE;
    std::string           m_shaderDir;
};

} // namespace xcel
