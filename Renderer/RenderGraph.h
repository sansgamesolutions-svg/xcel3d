#pragma once
#include "Renderer/IPass.h"
#include "Renderer/DescriptorManager.h"
#include <vulkan/vulkan.h>
#include <concepts>
#include <memory>
#include <vector>

namespace xcel {

class DeviceContext;

// Owns an ordered list of IPass objects and a per-frame command buffer.
// Executes all passes in order each frame; passes communicate via PassContext.
class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph();

    RenderGraph(RenderGraph&&) noexcept;
    RenderGraph& operator=(RenderGraph&&) noexcept;

    RenderGraph(const RenderGraph&)            = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    // Called once after all passes are added; allocates command buffers and builds each pass.
    void Build(DeviceContext& dev, const BuildPassInfo& info);

    // Resize all passes and rebuild size-dependent resources.
    void Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass newRP);

    // Record all passes into the per-frame command buffer and return it (ready to submit).
    // DrawCount / indirect buffer fields in ctx are zeroed before the first pass.
    VkCommandBuffer Execute(
        uint32_t        frameIndex,
        VkFramebuffer   framebuffer,
        VkExtent2D      extent,
        VkDescriptorSet frameDescSet);

    void Destroy(VkDevice device);

    // Add a pass.  Must be called before Build().
    void AddPass(std::unique_ptr<IPass> pass);

    // Return the first pass of type T in the list, or nullptr if none.
    template<typename T>
        requires std::derived_from<T, IPass>
    T* FindPass() const
    {
        for (auto& p : Passes())
            if (auto* t = dynamic_cast<T*>(p.get())) return t;
        return nullptr;
    }

    VkCommandBuffer CommandBuffer(uint32_t frameIndex) const;

private:
    const std::vector<std::unique_ptr<IPass>>& Passes() const;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
