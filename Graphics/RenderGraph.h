#pragma once
#include "Graphics/IPass.h"
#include "Graphics/Swapchain.h"
#include "Platforms/IWindowWidget.h"
#include <memory>
#include <vector>

namespace xcel {

class RenderGraph
{
public:
    static constexpr uint32_t MAX_FRAMES = 2;

    RenderGraph();
    ~RenderGraph();

    RenderGraph(const RenderGraph&)            = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) noexcept;
    RenderGraph& operator=(RenderGraph&&) noexcept;

    // Allocates command buffers, creates sync objects, calls Build() on every pass.
    // swapchain must outlive this RenderGraph.
    void Build(DeviceContext& dev,
               Swapchain&    swapchain,
               const BuildPassInfo& info,
               std::vector<std::unique_ptr<IPass>> passes);

    // Waits for the in-flight fence for the current frame slot.
    // Call before CPU work that depends on the previous frame's GPU submission
    // (e.g. rebuilding dirty GPU pages) to avoid write-after-read hazards.
    void WaitForCurrentFrame(VkDevice device);

    // Acquires the next swapchain image, records all passes into the frame's
    // command buffer, submits, and presents. Fills ctx.frameIndex,
    // ctx.swapchainFramebuffer, and ctx.extent from the graph.
    // Sets outNeedsResize to true if the swapchain is out-of-date or suboptimal.
    void Execute(DeviceContext& dev, PassContext ctx, bool& outNeedsResize);

    // Recreates the swapchain for the new surface dimensions, then calls
    // Rebuild() on every pass with the updated extent.
    void Resize(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window);

    void Destroy(VkDevice device);

    uint32_t       CurrentFrame()              const;
    VkCommandBuffer CommandBuffer(uint32_t i)  const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
