#pragma once
#include "Renderer/IPass.h"
#include "Renderer/CommandRecorder.h"
#include <memory>
#include <span>
#include <string>

namespace xcel {

class Pipeline;

// Graphics pass: Blinn-Phong forward shading.
//
// When PassContext::indirectDrawBuffer is non-null (a culling pass ran before this one),
// issues vkCmdDrawIndexedIndirectCount for each object buffer slice.
// Otherwise falls back to direct vkCmdDrawIndexed over the CPU-side draw-call list.
class ForwardRenderPass : public IPass {
public:
    explicit ForwardRenderPass(std::string vertSpv, std::string fragSpv);
    ~ForwardRenderPass() override;

    void Build(DeviceContext& dev, const BuildPassInfo& info) override;
    void Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass newRP) override;
    void Record(VkCommandBuffer cmd, PassContext& ctx) override;
    void Destroy(VkDevice device) override;

    // Called each frame by WindowContext before Execute() to supply the draw-call list.
    void SetDrawCalls(std::span<const DrawCall> drawCalls);

    // Expose the GpuBuffer registry for indirect drawing (vertex / index buffers per draw call).
    // These are still needed so the ForwardPass can bind VBOs/IBOs even in indirect mode.
    void SetIndirectVertexIndexBuffers(std::span<const DrawCall> drawCalls);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
