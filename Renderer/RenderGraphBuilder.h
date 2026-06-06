#pragma once
#include "Renderer/RenderGraph.h"
#include "Renderer/PassOptions.h"
#include <string>
#include <memory>

namespace xcel {

class Swapchain;
class DescriptorManager;
class IWindowWidget;

// Fluent builder that assembles a RenderGraph from a PassOptions configuration.
// All Set*() calls are optional (defaults are provided). Build() performs the
// one-time setup: creates the ForwardRenderPass VkRenderPass, creates/recreates
// the swapchain with that render pass, then builds all selected passes.
class RenderGraphBuilder
{
public:
    RenderGraphBuilder();
    ~RenderGraphBuilder();

    RenderGraphBuilder& SetOptions(const PassOptions& opts);
    RenderGraphBuilder& SetSwapchain(Swapchain& swapchain);
    RenderGraphBuilder& SetSurface(VkSurfaceKHR surface);
    RenderGraphBuilder& SetWindow(IWindowWidget& window);
    RenderGraphBuilder& SetDescriptors(DescriptorManager& descriptors);
    RenderGraphBuilder& SetShaderDir(const std::string& dir);
    RenderGraphBuilder& SetMaxObjects(uint32_t maxObjects);

    RenderGraph Build(DeviceContext& dev);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
