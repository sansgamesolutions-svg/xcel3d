#pragma once
#include "Renderer/IPass.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace xcel {

// GPU frustum-culling compute pass.
//
// Build(): Creates compute pipeline (frustum_cull.comp.spv), three SSBOs:
//   - ObjectBuffer:      CullableObject[maxObjects]
//   - DrawCommandBuffer: VkDrawIndexedIndirectCommand[maxObjects]
//   - DrawCountBuffer:   uint32_t (atomic counter)
//
// SetObjects(): Upload CullableObject data from CPU each time the draw list changes.
// Record(): Resets DrawCountBuffer via vkCmdFillBuffer, dispatches compute,
//           emits buffer barrier (SHADER_WRITE → INDIRECT_COMMAND_READ),
//           and writes the output buffer handles into PassContext.
struct CullableObject {
    glm::vec4 aabbMin{0.f};    // xyz = min, w = unused
    glm::vec4 aabbMax{0.f};    // xyz = max, w = unused
    uint32_t  indexCount    = 0;
    uint32_t  instanceCount = 1;
    uint32_t  firstIndex    = 0;
    int32_t   vertexOffset  = 0;
    uint32_t  firstInstance = 0;
    uint32_t  _pad[3]       = {};
};

class FrustumCullPass : public IPass {
public:
    explicit FrustumCullPass(std::string shaderDir);
    ~FrustumCullPass() override;

    void Build(DeviceContext& dev, const BuildPassInfo& info) override;
    void Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass newRP) override;
    void Record(VkCommandBuffer cmd, PassContext& ctx) override;
    void Destroy(VkDevice device) override;

    // Upload per-object data before Record() is called.
    // viewProj is used to extract frustum planes on the CPU and pass as push constant.
    void SetObjects(DeviceContext& dev,
                    const std::vector<CullableObject>& objects,
                    const glm::mat4& viewProj);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
