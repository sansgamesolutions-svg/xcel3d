#pragma once
#include "Renderer/IPass.h"
#include "Renderer/FrustumCullPass.h"
#include "Renderer/CommandRecorder.h"
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <vector>

namespace xcel {

// GPU occlusion-culling pass group (depth pre-pass + Hi-Z build + occlusion test).
//
// Internally contains three sub-passes that must run in order:
//   1. DepthPrePass   — renders all visible objects depth-only to an offscreen FB.
//   2. HiZBuildPass   — compute: progressively halves depth image into a mip chain.
//   3. OccCullPass    — compute: tests object AABBs against Hi-Z pyramid; writes
//                       VkDrawIndexedIndirectCommand list into PassContext.
//
// OcclusionCullPass::Record() chains all three sub-passes with the required barriers.
class OcclusionCullPass : public IPass {
public:
    explicit OcclusionCullPass(std::string shaderDir);
    ~OcclusionCullPass() override;

    void Build(DeviceContext& dev, const BuildPassInfo& info) override;
    void Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass newRP) override;
    void Record(VkCommandBuffer cmd, PassContext& ctx) override;
    void Destroy(VkDevice device) override;

    // Upload per-object data before Record() is called.
    void SetObjects(DeviceContext& dev,
                    const std::vector<CullableObject>& objects,
                    const glm::mat4& viewProj,
                    const glm::mat4& proj);

    // Supply the draw calls needed by the depth pre-pass (same list as ForwardRenderPass).
    void SetDrawCalls(std::span<const DrawCall> drawCalls);

    // The forward pass still needs vertex/index buffers; returns the indirect draw buffer
    // and draw count buffer handles after Build().
    VkBuffer IndirectDrawBuffer() const;
    VkBuffer DrawCountBuffer()    const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
