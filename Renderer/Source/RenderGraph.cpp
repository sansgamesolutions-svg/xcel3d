#include "Renderer/RenderGraph.h"
#include <stdexcept>
#include <algorithm>

namespace xcel {

RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
RenderGraph& RenderGraph::operator=(RenderGraph&&) noexcept = default;

void RenderGraph::Build(DeviceContext& dev,
                        Swapchain&    swapchain,
                        const BuildPassInfo& info,
                        std::vector<std::unique_ptr<IPass>> passes)
{
    m_swapchain         = &swapchain;
    m_forwardRenderPass = info.forwardRenderPass;
    m_overlayRenderPass = info.overlayRenderPass;
    m_cmdPool           = dev.GraphicsCommandPool();
    m_passes            = std::move(passes);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_cmdPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES;

    if (vkAllocateCommandBuffers(dev.Device(), &allocInfo, m_cmdBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkAllocateCommandBuffers failed");

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES; ++i) {
        if (vkCreateSemaphore(dev.Device(), &semInfo,   nullptr, &m_imageAvailableSem[i]) != VK_SUCCESS ||
            vkCreateFence    (dev.Device(), &fenceInfo, nullptr, &m_inFlightFence[i])     != VK_SUCCESS)
            throw std::runtime_error("RenderGraph: failed to create sync objects");
    }

    // One render-finished semaphore per swapchain image: the semaphore is passed to
    // vkQueuePresentKHR and may be held by the presentation engine until that image is
    // re-acquired.  Indexing by imageIndex (not frame) ensures we never signal a
    // semaphore that is still waited on by an in-flight presentation.
    m_renderFinishedSem.resize(swapchain.ImageCount());
    for (auto& sem : m_renderFinishedSem) {
        if (vkCreateSemaphore(dev.Device(), &semInfo, nullptr, &sem) != VK_SUCCESS)
            throw std::runtime_error("RenderGraph: failed to create render-finished semaphore");
    }

    for (auto& pass : m_passes)
        pass->Build(info);
}

void RenderGraph::WaitForCurrentFrame(VkDevice device)
{
    vkWaitForFences(device, 1, &m_inFlightFence[m_currentFrame], VK_TRUE, UINT64_MAX);
}

void RenderGraph::Execute(DeviceContext& dev, PassContext ctx, bool& outNeedsResize)
{
    const uint32_t frame  = m_currentFrame;
    VkDevice       device = dev.Device();

    vkWaitForFences(device, 1, &m_inFlightFence[frame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        device, m_swapchain->GetHandle(), UINT64_MAX,
        m_imageAvailableSem[frame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        outNeedsResize = true;
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("RenderGraph: vkAcquireNextImageKHR failed");

    vkResetFences(device, 1, &m_inFlightFence[frame]);

    ctx.frameIndex           = frame;
    ctx.swapchainFramebuffer = m_swapchain->Framebuffer(imageIndex);
    ctx.overlayFramebuffer   = m_overlayRenderPass != VK_NULL_HANDLE
                                   ? m_swapchain->OverlayFramebuffer(imageIndex)
                                   : VK_NULL_HANDLE;
    ctx.extent               = m_swapchain->Extent();

    VkCommandBuffer cmd = m_cmdBuffers[frame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkBeginCommandBuffer failed");

    for (auto& pass : m_passes)
        pass->Record(cmd, ctx);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkEndCommandBuffer failed");

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &m_imageAvailableSem[frame];
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_renderFinishedSem[imageIndex];

    if (vkQueueSubmit(dev.GraphicsQueue(), 1, &submitInfo, m_inFlightFence[frame]) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkQueueSubmit failed");

    VkSwapchainKHR sc = m_swapchain->GetHandle();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_renderFinishedSem[imageIndex];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &sc;
    presentInfo.pImageIndices      = &imageIndex;

    result = vkQueuePresentKHR(dev.PresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        outNeedsResize = true;
    else if (result != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkQueuePresentKHR failed");

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES;
}

void RenderGraph::Resize(DeviceContext& dev, VkSurfaceKHR surface, IWindowWidget& window)
{
    m_swapchain->Recreate(dev, surface, window, m_forwardRenderPass, m_overlayRenderPass);
    const VkExtent2D newExtent = m_swapchain->Extent();
    for (auto& pass : m_passes)
        pass->Rebuild(dev, newExtent);
}

void RenderGraph::Destroy(VkDevice device)
{
    for (int i = static_cast<int>(MAX_FRAMES) - 1; i >= 0; --i) {
        if (m_imageAvailableSem[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device, m_imageAvailableSem[i], nullptr);
        if (m_inFlightFence[i] != VK_NULL_HANDLE)
            vkDestroyFence(device, m_inFlightFence[i], nullptr);
    }
    for (auto& sem : m_renderFinishedSem) {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(device, sem, nullptr);
    }
    m_renderFinishedSem.clear();

    if (m_cmdPool != VK_NULL_HANDLE)
        vkFreeCommandBuffers(device, m_cmdPool, MAX_FRAMES, m_cmdBuffers.data());

    for (auto it = m_passes.rbegin(); it != m_passes.rend(); ++it)
        (*it)->Destroy(device);
    m_passes.clear();

    // Reset handles to guard against double-destroy
    for (uint32_t i = 0; i < MAX_FRAMES; ++i) {
        m_imageAvailableSem[i] = VK_NULL_HANDLE;
        m_inFlightFence[i]     = VK_NULL_HANDLE;
        m_cmdBuffers[i]        = VK_NULL_HANDLE;
    }
    m_cmdPool           = VK_NULL_HANDLE;
    m_forwardRenderPass = VK_NULL_HANDLE;
}

uint32_t        RenderGraph::CurrentFrame()             const { return m_currentFrame; }
VkCommandBuffer RenderGraph::CommandBuffer(uint32_t i)  const { return m_cmdBuffers[i]; }

} // namespace xcel
