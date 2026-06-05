#include "Renderer/RenderGraph.h"
#include "Renderer/DeviceContext.h"
#include "Renderer/DescriptorManager.h"
#include <stdexcept>
#include <vector>

namespace xcel {

struct RenderGraph::Impl {
    std::vector<std::unique_ptr<IPass>> passes;
    std::vector<VkCommandBuffer>        cmdBuffers;
    VkCommandPool                       pool = VK_NULL_HANDLE;
};

RenderGraph::RenderGraph()
    : m_impl(std::make_unique<Impl>()) {}

RenderGraph::~RenderGraph() = default;

RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
RenderGraph& RenderGraph::operator=(RenderGraph&&) noexcept = default;

void RenderGraph::AddPass(std::unique_ptr<IPass> pass)
{
    m_impl->passes.push_back(std::move(pass));
}

void RenderGraph::Build(DeviceContext& dev, const BuildPassInfo& info)
{
    m_impl->pool = dev.GraphicsCommandPool();

    m_impl->cmdBuffers.resize(DescriptorManager::MAX_FRAMES);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_impl->pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_impl->cmdBuffers.size();

    if (vkAllocateCommandBuffers(dev.Device(), &allocInfo, m_impl->cmdBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkAllocateCommandBuffers failed");

    for (auto& pass : m_impl->passes)
        pass->Build(dev, info);
}

void RenderGraph::Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass newRP)
{
    for (auto& pass : m_impl->passes)
        pass->Rebuild(dev, newExtent, newRP);
}

VkCommandBuffer RenderGraph::Execute(
    uint32_t        frameIndex,
    VkFramebuffer   framebuffer,
    VkExtent2D      extent,
    VkDescriptorSet frameDescSet)
{
    VkCommandBuffer cmd = m_impl->cmdBuffers[frameIndex];

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkBeginCommandBuffer failed");

    PassContext ctx{};
    ctx.frameIndex   = frameIndex;
    ctx.framebuffer  = framebuffer;
    ctx.extent       = extent;
    ctx.frameDescSet = frameDescSet;

    for (auto& pass : m_impl->passes)
        pass->Record(cmd, ctx);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("RenderGraph: vkEndCommandBuffer failed");

    return cmd;
}

void RenderGraph::Destroy(VkDevice device)
{
    for (auto it = m_impl->passes.rbegin(); it != m_impl->passes.rend(); ++it)
        (*it)->Destroy(device);
    m_impl->passes.clear();
    // Command buffers are freed implicitly when the command pool is destroyed.
    m_impl->cmdBuffers.clear();
}

VkCommandBuffer RenderGraph::CommandBuffer(uint32_t frameIndex) const
{
    return m_impl->cmdBuffers[frameIndex];
}

const std::vector<std::unique_ptr<IPass>>& RenderGraph::Passes() const
{
    return m_impl->passes;
}

} // namespace xcel
