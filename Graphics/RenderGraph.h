#pragma once
#include "Graphics/IPass.h"
#include "Graphics/Swapchain.h"
#include "Platforms/IWindowWidget.h"
#include <array>
#include <memory>
#include <vector>

namespace xcel {

class RenderGraph
{
public:
    static constexpr uint32_t MAX_FRAMES = 2;

    RenderGraph()  = default;
    ~RenderGraph() = default;

    RenderGraph(const RenderGraph&)            = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) noexcept;
    RenderGraph& operator=(RenderGraph&&) noexcept;

    void Build(DeviceContext& dev,
               Swapchain&    swapchain,
               const BuildPassInfo& info,
               std::vector<std::unique_ptr<IPass>> passes);

    void WaitForCurrentFrame(VkDevice device);
    void Execute(DeviceContext& dev, PassContext ctx, bool& outNeedsResize);
    void Resize(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window);
    void Destroy(VkDevice device);

    uint32_t        CurrentFrame()             const;
    VkCommandBuffer CommandBuffer(uint32_t i)  const;

private:
    Swapchain*   m_swapchain         = nullptr;
    VkRenderPass m_forwardRenderPass = VK_NULL_HANDLE;
    VkCommandPool m_cmdPool          = VK_NULL_HANDLE;

    std::vector<std::unique_ptr<IPass>> m_passes;

    std::array<VkCommandBuffer, MAX_FRAMES> m_cmdBuffers        = {};
    std::array<VkSemaphore, MAX_FRAMES>     m_imageAvailableSem = {};
    std::array<VkSemaphore, MAX_FRAMES>     m_renderFinishedSem = {};
    std::array<VkFence, MAX_FRAMES>         m_inFlightFence     = {};
    uint32_t                                m_currentFrame      = 0;
};

} // namespace xcel
