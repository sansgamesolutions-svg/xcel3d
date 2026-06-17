#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/ForwardRenderPass.h"
#include "Renderer/FrustumCullPass.h"
#include "Renderer/Manipulator/ManipulatorPass.h"
#include <stdexcept>

namespace xcel {

RenderGraphBuilder& RenderGraphBuilder::SetOptions(const PassOptions& opts)
{
    m_options.frustumCulling   = opts.frustumCulling;
    m_options.occlusionCulling = opts.occlusionCulling;
    return *this;
}

RenderGraphBuilder& RenderGraphBuilder::SetOptions(const GlobalRenderOptions& opts) { m_options      = opts;        return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetEffectiveCaps(const EffectiveCaps& caps) { m_effectiveCaps = caps;       return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetSwapchain(Swapchain& sc)             { m_swapchain   = &sc;         return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetSurface(VkSurfaceKHR s)              { m_surface     = s;           return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetWindow(IWindowWidget& w)             { m_window      = &w;          return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetDescriptors(DescriptorManager& d)    { m_descriptors = &d;          return *this; }
RenderGraphBuilder& RenderGraphBuilder::SetTextures(TextureManager& t)          { m_textures    = &t;          return *this; }
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

    // Create forward and overlay render passes before the swapchain so both sets
    // of framebuffers are created with the correct compatible render pass.
    auto forwardPass = std::make_unique<ForwardRenderPass>();
    forwardPass->CreateRenderPass(dev, colorFmt, VK_FORMAT_D32_SFLOAT);

    auto manipPass = std::make_unique<ManipulatorPass>();
    manipPass->CreateOverlayRenderPass(dev.Device(), colorFmt, VK_FORMAT_D32_SFLOAT);

    m_swapchain->Recreate(dev, m_surface, *m_window,
                          forwardPass->GetRenderPass(),
                          manipPass->GetRenderPass());

    BuildPassInfo info{};
    info.dev               = &dev;
    info.forwardRenderPass = forwardPass->GetRenderPass();
    info.overlayRenderPass = manipPass->GetRenderPass();
    info.uboLayout         = m_descriptors->Layout();
    info.bindlessLayout    = m_textures ? m_textures->Layout() : VK_NULL_HANDLE;
    info.extent            = m_swapchain->Extent();
    info.framesInFlight    = RenderGraph::MAX_FRAMES;
    info.maxObjects        = m_maxObjects;
    info.shaderDir         = m_shaderDir;
    info.colorFormat       = colorFmt;
    info.depthFormat       = VK_FORMAT_D32_SFLOAT;
    info.effectiveCaps     = m_effectiveCaps;

    std::vector<std::unique_ptr<IPass>> passes;

    if (m_options.frustumCulling)
        passes.push_back(std::make_unique<FrustumCullPass>());

    (void)m_options.occlusionCulling;

    passes.push_back(std::move(forwardPass));
    passes.push_back(std::move(manipPass));

    RenderGraph graph;
    graph.Build(dev, *m_swapchain, info, std::move(passes));
    return graph;
}

} // namespace xcel
