#pragma once
#include "Renderer/PassOptions.h"
#include "Renderer/RenderGraph.h"
#include <vulkan/vulkan.h>
#include <string>

namespace xcel {

class DeviceContext;
class Swapchain;
class DescriptorManager;

// Constructs a RenderGraph whose pass list is determined by PassOptions.
//
// Pass ordering:
//   occlusionCulling=true  →  DepthPrePass + HiZBuildPass + OcclusionCullPass
//   frustumCulling=true    →  FrustumCullPass
//   always                 →  ForwardRenderPass
class RenderGraphBuilder {
public:
    RenderGraphBuilder& SetOptions(const PassOptions& opts);
    RenderGraphBuilder& SetSwapchain(Swapchain& swapchain);
    RenderGraphBuilder& SetDescriptors(DescriptorManager& descriptors);
    RenderGraphBuilder& SetShaderDir(const std::string& dir);
    RenderGraphBuilder& SetMaxObjectCount(uint32_t count);

    // Supply the VkRenderPass handle created by WindowContext's RenderPass object.
    // Must be called before Build().
    RenderGraphBuilder& SetRenderPassHandle(VkRenderPass rp);

    // Build and return the initialised RenderGraph.
    RenderGraph Build(DeviceContext& dev);

private:
    PassOptions       m_options{};
    Swapchain*        m_swapchain    = nullptr;
    DescriptorManager* m_descriptors = nullptr;
    std::string       m_shaderDir   = "shaders/";
    uint32_t          m_maxObjects  = 4096;
    VkRenderPass      m_renderPass  = VK_NULL_HANDLE;
};

} // namespace xcel
