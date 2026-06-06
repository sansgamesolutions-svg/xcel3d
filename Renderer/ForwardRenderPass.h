#pragma once
#include "Renderer/IPass.h"
#include <memory>

namespace xcel {

// Owns the forward VkRenderPass and graphics Pipeline.
// Must call CreateRenderPass() before the Swapchain is created so its
// VkRenderPass handle can be used for framebuffer creation.
class ForwardRenderPass final : public IPass
{
public:
    ForwardRenderPass();
    ~ForwardRenderPass() override;

    ForwardRenderPass(const ForwardRenderPass&)            = delete;
    ForwardRenderPass& operator=(const ForwardRenderPass&) = delete;

    // Phase 1 of construction: creates the VkRenderPass before Swapchain framebuffers are bound.
    void         CreateRenderPass(DeviceContext& dev, VkFormat colorFormat, VkFormat depthFormat);
    VkRenderPass GetRenderPass() const;

    void Build(const BuildPassInfo&)           override;
    void Rebuild(DeviceContext&, VkExtent2D)   override;
    void Record(VkCommandBuffer, PassContext&) override;
    void Destroy(VkDevice)                     override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
