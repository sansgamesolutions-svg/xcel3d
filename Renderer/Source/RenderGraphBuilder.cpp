#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/ForwardRenderPass.h"
#include "Renderer/FrustumCullPass.h"
#include "Renderer/OcclusionCullPass.h"
#include "Renderer/Swapchain.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceContext.h"
#include <stdexcept>

namespace xcel {

RenderGraphBuilder& RenderGraphBuilder::SetOptions(const PassOptions& opts)
{
    m_options = opts;
    return *this;
}

RenderGraphBuilder& RenderGraphBuilder::SetSwapchain(Swapchain& swapchain)
{
    m_swapchain = &swapchain;
    return *this;
}

RenderGraphBuilder& RenderGraphBuilder::SetDescriptors(DescriptorManager& descriptors)
{
    m_descriptors = &descriptors;
    return *this;
}

RenderGraphBuilder& RenderGraphBuilder::SetShaderDir(const std::string& dir)
{
    m_shaderDir = dir;
    return *this;
}

RenderGraphBuilder& RenderGraphBuilder::SetMaxObjectCount(uint32_t count)
{
    m_maxObjects = count;
    return *this;
}

RenderGraph RenderGraphBuilder::Build(DeviceContext& dev)
{
    if (!m_swapchain)   throw std::runtime_error("RenderGraphBuilder: swapchain not set");
    if (!m_descriptors) throw std::runtime_error("RenderGraphBuilder: descriptors not set");

    RenderGraph graph;

    // Occlusion culling first (depth pre-pass + Hi-Z + cull)
    if (m_options.occlusionCulling) {
        graph.AddPass(std::make_unique<OcclusionCullPass>(m_shaderDir));
    }

    // Frustum culling (compute AABB test, writes indirect draw commands)
    if (m_options.frustumCulling) {
        graph.AddPass(std::make_unique<FrustumCullPass>(m_shaderDir));
    }

    // Forward shading pass (always present)
    graph.AddPass(std::make_unique<ForwardRenderPass>(
        m_shaderDir + "mesh.vert.spv",
        m_shaderDir + "mesh.frag.spv"));

    BuildPassInfo info{};
    info.swapchainRenderPass = VK_NULL_HANDLE; // placeholder; filled below
    info.extent              = m_swapchain->Extent();
    info.frameLayout         = m_descriptors->Layout();
    info.maxObjectCount      = m_maxObjects;

    // The ForwardRenderPass needs the actual VkRenderPass.
    // We create a temporary RenderPass here to get the handle and store it in BuildPassInfo.
    // The Swapchain was created with a VkRenderPass — expose it via accessor.
    // Since Swapchain doesn't store the render pass, we pass it through BuildPassInfo.
    // WindowContext creates the RenderPass before the builder; it passes the handle via info.
    // IMPORTANT: WindowContext must call Build() via a different path that supplies renderPassHandle.
    // The builder stores it; callers set it via SetRenderPassHandle().
    info.swapchainRenderPass = m_renderPass;

    graph.Build(dev, info);
    return graph;
}

RenderGraphBuilder& RenderGraphBuilder::SetRenderPassHandle(VkRenderPass rp)
{
    m_renderPass = rp;
    return *this;
}

} // namespace xcel
