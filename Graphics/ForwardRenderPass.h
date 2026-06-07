#pragma once
#include "Graphics/IPass.h"
#include "Graphics/RenderPass.h"
#include "Graphics/Pipeline.h"
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
    RenderPass            m_renderPass;
    Pipeline              m_pipeline;
    VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
    std::string           m_shaderDir;
};

} // namespace xcel
