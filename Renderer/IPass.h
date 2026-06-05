#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <span>
#include <cstdint>

namespace xcel {

class DeviceContext;

// Per-frame data threaded through all passes during Execute().
struct PassContext {
    uint32_t      frameIndex      = 0;
    VkFramebuffer framebuffer     = VK_NULL_HANDLE; // swapchain FB for the forward pass
    VkExtent2D    extent          = {};
    VkDescriptorSet frameDescSet  = VK_NULL_HANDLE;

    // Written by culling passes, consumed by ForwardRenderPass.
    // Null when no culling pass precedes the forward pass.
    VkBuffer      indirectDrawBuffer = VK_NULL_HANDLE;
    VkBuffer      drawCountBuffer    = VK_NULL_HANDLE;
    uint32_t      maxDrawCount       = 0;
};

// Context supplied to IPass::Build() and IPass::Rebuild().
struct BuildPassInfo {
    VkRenderPass  swapchainRenderPass = VK_NULL_HANDLE; // for ForwardRenderPass
    VkExtent2D    extent              = {};
    VkDescriptorSetLayout frameLayout = VK_NULL_HANDLE;
    uint32_t      maxObjectCount      = 0; // upper bound on draw calls
};

class IPass {
public:
    virtual ~IPass() = default;

    // Called once during RenderGraph::Build(); allocate pipelines, descriptor sets, buffers.
    virtual void Build(DeviceContext& dev, const BuildPassInfo& info) = 0;

    // Called on window resize; recreate size-dependent resources.
    virtual void Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass newRP) = 0;

    // Record this pass into cmd.  May read and write ctx fields.
    virtual void Record(VkCommandBuffer cmd, PassContext& ctx) = 0;

    virtual void Destroy(VkDevice device) = 0;

protected:
    IPass() = default;
    IPass(const IPass&)            = delete;
    IPass& operator=(const IPass&) = delete;
};

} // namespace xcel
