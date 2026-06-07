#include "Graphics/RenderGraphBuilder.h"
#include "Graphics/ForwardRenderPass.h"
#include "Graphics/FrustumCullPass.h"
#include <stdexcept>

namespace xcel {

RenderGraphBuilder& RenderGraphBuilder::SetOptions(const PassOptions& opts)     { m_options     = opts;        return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetSwapchain(Swapchain& sc)             { m_swapchain   = &sc;         return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetSurface(VkSurfaceKHR s)              { m_surface     = s;           return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetWindow(IWindowWidget& w)             { m_window      = &w;          return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetDescriptors(DescriptorManager& d)    { m_descriptors = &d;          return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetShaderDir(const std::string& dir)    { m_shaderDir   = dir;         return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetMaxObjects(uint32_t n)               { m_maxObjects  = n;           return *this; }

RenderGraph RenderGraphBuilder::Build(DeviceContext& dev)
{
    if (!m_swapchain)              throw std::runtime_error("RenderGraphBuilder: swapchain not set");
    if (!m_descriptors)            throw std::runtime_error("RenderGraphBuilder: descriptors not set");
    if (!m_window)                 throw std::runtime_error("RenderGraphBuilder: window not set");
    if (m_surface == VK_NULL_HANDLE)
        throw std::runtime_error("RenderGraphBuilder: surface not set");

    // Determine the swapchain color format from the surface.
    auto support = Swapchain::QuerySupport(dev.PhysicalDevice(), m_surface);
    VkFormat colorFmt = support.formats.empty()
        ? VK_FORMAT_B8G8R8A8_SRGB
        : support.formats[0].format;
    for (const auto& f : support.formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB) { colorFmt = f.format; break; }

    // Create the forward render pass first so its VkRenderPass handle is available
    // before the swapchain framebuffers are created.
    auto forwardPass = std::make_unique<ForwardRenderPass>();
    forwardPass->CreateRenderPass(dev, colorFmt, VK_FORMAT_D32_SFLOAT);

    m_swapchain->Recreate(dev, m_surface, *m_window, forwardPass->GetRenderPass());

    BuildPassInfo info{};
    info.dev              = &dev;
    info.forwardRenderPass = forwardPass->GetRenderPass();
    info.uboLayout        = m_descriptors->Layout();
    info.extent           = m_swapchain->Extent();
    info.framesInFlight   = RenderGraph::MAX_FRAMES;
    info.maxObjects       = m_maxObjects;
    info.shaderDir        = m_shaderDir;

    std::vector<std::unique_ptr<IPass>> passes;

    if (m_options.frustumCulling)
        passes.push_back(std::make_unique<FrustumCullPass>());

    (void)m_options.occlusionCulling;

    passes.push_back(std::move(forwardPass));

    RenderGraph graph;
    graph.Build(dev, *m_swapchain, info, std::move(passes));
    return graph;
}

} // namespace xcel
