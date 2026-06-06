#include "Graphics/RenderGraph.h"
#include <stdexcept>
#include <algorithm>

namespace xcel {

struct RenderGraph::Impl
{
    Swapchain*   swapchain         = nullptr;
    VkRenderPass forwardRenderPass = VK_NULL_HANDLE;
    VkCommandPool cmdPool          = VK_NULL_HANDLE;

    std::vector<std::unique_ptr<IPass>> passes;

    VkCommandBuffer cmdBuffers[MAX_FRAMES]       = {};
    VkSemaphore     imageAvailableSem[MAX_FRAMES] = {};
    VkSemaphore     renderFinishedSem[MAX_FRAMES] = {};
    VkFence         inFlightFence[MAX_FRAMES]     = {};
    uint32_t        currentFrame = 0;
};

RenderGraph::RenderGraph()
    : m_impl(std::make_unique<Impl>()) {}

RenderGraph::~RenderGraph() = default;

RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
RenderGraph& RenderGraph::operator=(RenderGraph&&) noexcept = default;

void RenderGraph::Build(DeviceContext& dev,
                        Swapchain&    swapchain,
                        const BuildPassInfo& info,
                        std::vector<std::unique_ptr<IPass>> passes)
{
    m_impl->swapchain         = &swapchain;
    m_impl->forwardRenderPass = info.forwardRenderPass;
    m_impl->cmdPool           = dev.GraphicsCommandPool();
    m_impl->passes            = std::move(passes);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_impl->cmdPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES;

    if (vkAllocateCommandBuffers(dev.Device(), &allocInfo, m_impl->cmdBuffers) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkAllocateCommandBuffers failed");

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES; ++i) {
        if (vkCreateSemaphore(dev.Device(), &semInfo,   nullptr, &m_impl->imageAvailableSem[i]) != VK_SUCCESS ||
            vkCreateSemaphore(dev.Device(), &semInfo,   nullptr, &m_impl->renderFinishedSem[i]) != VK_SUCCESS ||
            vkCreateFence    (dev.Device(), &fenceInfo, nullptr, &m_impl->inFlightFence[i])     != VK_SUCCESS)
            throw std::runtime_error("RenderGraph: failed to create sync objects");
    }

    for (auto& pass : m_impl->passes)
        pass->Build(info);
}

void RenderGraph::WaitForCurrentFrame(VkDevice device)
{
    vkWaitForFences(device, 1, &m_impl->inFlightFence[m_impl->currentFrame], VK_TRUE, UINT64_MAX);
}

void RenderGraph::Execute(DeviceContext& dev, PassContext ctx, bool& outNeedsResize)
{
    const uint32_t frame  = m_impl->currentFrame;
    VkDevice       device = dev.Device();

    vkWaitForFences(device, 1, &m_impl->inFlightFence[frame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        device, m_impl->swapchain->GetHandle(), UINT64_MAX,
        m_impl->imageAvailableSem[frame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        outNeedsResize = true;
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("RenderGraph: vkAcquireNextImageKHR failed");

    vkResetFences(device, 1, &m_impl->inFlightFence[frame]);

    ctx.frameIndex           = frame;
    ctx.swapchainFramebuffer = m_impl->swapchain->Framebuffer(imageIndex);
    ctx.extent               = m_impl->swapchain->Extent();

    VkCommandBuffer cmd = m_impl->cmdBuffers[frame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkBeginCommandBuffer failed");

    for (auto& pass : m_impl->passes)
        pass->Record(cmd, ctx);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkEndCommandBuffer failed");

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_impl->imageAvailableSem[frame];
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_impl->renderFinishedSem[frame];

    if (vkQueueSubmit(dev.GraphicsQueue(), 1, &submitInfo, m_impl->inFlightFence[frame]) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkQueueSubmit failed");

    VkSwapchainKHR sc = m_impl->swapchain->GetHandle();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_impl->renderFinishedSem[frame];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &sc;
    presentInfo.pImageIndices      = &imageIndex;

    result = vkQueuePresentKHR(dev.PresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        outNeedsResize = true;
    else if (result != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkQueuePresentKHR failed");

    m_impl->currentFrame = (m_impl->currentFrame + 1) % MAX_FRAMES;
}

void RenderGraph::Resize(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window)
{
    m_impl->swapchain->Recreate(dev, surface, window, m_impl->forwardRenderPass);
    const VkExtent2D newExtent = m_impl->swapchain->Extent();
    for (auto& pass : m_impl->passes)
        pass->Rebuild(dev, newExtent);
}

void RenderGraph::Destroy(VkDevice device)
{
    for (int i = static_cast<int>(MAX_FRAMES) - 1; i >= 0; --i) {
        if (m_impl->renderFinishedSem[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, m_impl->renderFinishedSem[i], nullptr);
        if (m_impl->imageAvailableSem[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, m_impl->imageAvailableSem[i], nullptr);
        if (m_impl->inFlightFence[i] != VK_NULL_HANDLE)
            vkDestroyFence(device, m_impl->inFlightFence[i], nullptr);
    }

    if (m_impl->cmdPool != VK_NULL_HANDLE)
        vkFreeCommandBuffers(device, m_impl->cmdPool, MAX_FRAMES, m_impl->cmdBuffers);

    for (auto it = m_impl->passes.rbegin(); it != m_impl->passes.rend(); ++it)
        (*it)->Destroy(device);
    m_impl->passes.clear();

    // Reset handles to guard against double-destroy
    for (uint32_t i = 0; i < MAX_FRAMES; ++i) {
        m_impl->renderFinishedSem[i] = VK_NULL_HANDLE;
        m_impl->imageAvailableSem[i] = VK_NULL_HANDLE;
        m_impl->inFlightFence[i]     = VK_NULL_HANDLE;
        m_impl->cmdBuffers[i]        = VK_NULL_HANDLE;
    }
    m_impl->cmdPool           = VK_NULL_HANDLE;
    m_impl->forwardRenderPass = VK_NULL_HANDLE;
}

uint32_t        RenderGraph::CurrentFrame()             const { return m_impl->currentFrame; }
VkCommandBuffer RenderGraph::CommandBuffer(uint32_t i)  const { return m_impl->cmdBuffers[i]; }

} // namespace xcel
