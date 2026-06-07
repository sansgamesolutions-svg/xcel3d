#pragma once
#include "Graphics/RenderGraph.h"
#include "Graphics/PassOptions.h"
#include "Graphics/Swapchain.h"
#include "Graphics/DescriptorManager.h"
#include "Platforms/IWindowWidget.h"
#include <string>

namespace xcel {

// Fluent builder that assembles a RenderGraph from a PassOptions configuration.
// No PIMPL: all members are borrowed pointers or POD with no resource ownership —
// there is nothing to hide or isolate.
class RenderGraphBuilder
{
public:
    RenderGraphBuilder() = default;

    RenderGraphBuilder& SetOptions(const PassOptions& opts);
    RenderGraphBuilder& SetSwapchain(Swapchain& swapchain);
    RenderGraphBuilder& SetSurface(VkSurfaceKHR surface);
    RenderGraphBuilder& SetWindow(IWindowWidget& window);
    RenderGraphBuilder& SetDescriptors(DescriptorManager& descriptors);
    RenderGraphBuilder& SetShaderDir(const std::string& dir);
    RenderGraphBuilder& SetMaxObjects(uint32_t maxObjects);

    RenderGraph Build(DeviceContext& dev);

private:
    PassOptions        m_options;
    Swapchain*         m_swapchain   = nullptr;
    VkSurfaceKHR       m_surface     = VK_NULL_HANDLE;
    IWindowWidget*     m_window      = nullptr;
    DescriptorManager* m_descriptors = nullptr;
    std::string        m_shaderDir   = "shaders/";
    uint32_t           m_maxObjects  = 65536;
};

} // namespace xcel
