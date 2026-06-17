#pragma once
#include "Renderer/RenderGraph.h"
#include "Renderer/PassOptions.h"
#include "Renderer/RenderOptions.h"
#include "Renderer/RenderGraphConfig.h"
#include "Renderer/HardwareCaps.h"
#include "Renderer/Swapchain.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/TextureManager.h"
#include "Platforms/IWindowWidget.h"
#include <filesystem>
#include <string>

namespace xcel {

// Fluent builder that assembles a RenderGraph from a PassOptions configuration.
// No PIMPL: all members are borrowed pointers or POD with no resource ownership —
// there is nothing to hide or isolate.
class RenderGraphBuilder
{
public:
    RenderGraphBuilder() = default;

    RenderGraphBuilder& SetOptions(const PassOptions& opts);      // backward-compat bridge
    RenderGraphBuilder& SetOptions(const GlobalRenderOptions& opts);
    RenderGraphBuilder& SetEffectiveCaps(const EffectiveCaps& caps);
    RenderGraphBuilder& SetSwapchain(Swapchain& swapchain);
    RenderGraphBuilder& SetSurface(VkSurfaceKHR surface);
    RenderGraphBuilder& SetWindow(IWindowWidget& window);
    RenderGraphBuilder& SetDescriptors(DescriptorManager& descriptors);
    RenderGraphBuilder& SetTextures(TextureManager& textures);
    RenderGraphBuilder& SetShaderDir(const std::string& dir);
    RenderGraphBuilder& SetMaxObjects(uint32_t maxObjects);
    RenderGraphBuilder& LoadFromJson(const std::filesystem::path& path);
    RenderGraphBuilder& SetRenderGraphConfig(RenderGraphConfig config);

    RenderGraph Build(DeviceContext& dev);

private:
    GlobalRenderOptions m_options;
    EffectiveCaps       m_effectiveCaps;
    RenderGraphConfig   m_graphConfig;
    bool                m_configLoaded = false;
    Swapchain*          m_swapchain    = nullptr;
    VkSurfaceKHR        m_surface      = VK_NULL_HANDLE;
    IWindowWidget*      m_window       = nullptr;
    DescriptorManager*  m_descriptors  = nullptr;
    TextureManager*     m_textures     = nullptr;
    std::string         m_shaderDir    = "shaders/";
    uint32_t            m_maxObjects   = 65536;
};

} // namespace xcel
