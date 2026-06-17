#pragma once
#include "Renderer/IPass.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Pipeline.h"
#include "Renderer/DrawCall.h"
#include "Renderer/RenderGraphConfig.h"
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

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

    RenderPass m_renderPass;

    // Parallel arrays: one entry per pipeline descriptor from the config.
    std::vector<Pipeline>           m_pipelines;
    std::vector<PipelineDescriptor> m_pipelineDescs;

    std::array<float, 4>  m_clearColor        = {0.15f, 0.15f, 0.15f, 1.0f};
    std::optional<size_t> m_opaquePipelineIdx; // index of the Opaque/Opaque pipeline

    VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bindlessLayout   = VK_NULL_HANDLE;
    std::string           m_shaderDir;
};

} // namespace xcel
