#pragma once
#include "Graphics/IPass.h"
#include <memory>

namespace xcel {

// GPU frustum culling compute pass.
//
// Per frame:
//  1. Uploads per-draw-call AABBs and pre-filled VkDrawIndexedIndirectCommand entries
//     to host-visible buffers.
//  2. Resets the per-slot draw-count buffer via vkCmdFillBuffer.
//  3. Dispatches a compute shader that tests each AABB against the 6 frustum planes
//     extracted from ctx.viewProj and writes 1 or 0 to counts[slot].
//  4. Emits a pipeline barrier so ForwardRenderPass can safely use the count buffer
//     via vkCmdDrawIndexedIndirectCount.
//
// ctx.indirectDrawBuffer and ctx.drawCountBuffer are set for ForwardRenderPass.
class FrustumCullPass final : public IPass
{
public:
    FrustumCullPass();
    ~FrustumCullPass() override;

    FrustumCullPass(const FrustumCullPass&)            = delete;
    FrustumCullPass& operator=(const FrustumCullPass&) = delete;

    void Build(const BuildPassInfo&)           override;
    void Rebuild(DeviceContext&, VkExtent2D)   override;
    void Record(VkCommandBuffer, PassContext&) override;
    void Destroy(VkDevice)                     override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
