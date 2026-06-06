#pragma once
#include "Renderer/DeviceContext.h"
#include "Renderer/DrawCall.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <cstdint>

namespace xcel {

struct PassContext
{
    uint32_t               frameIndex           = 0;
    VkFramebuffer          swapchainFramebuffer = VK_NULL_HANDLE;
    VkExtent2D             extent               = {};
    VkDescriptorSet        uboDescriptorSet     = VK_NULL_HANDLE;
    std::span<const DrawCall> directDrawCalls;

    // Clip-from-world matrix (proj * view). Used by FrustumCullPass to extract planes.
    glm::mat4              viewProj             = glm::mat4(1.f);

    // Written by FrustumCullPass, read by ForwardRenderPass.
    // When non-null: ForwardRenderPass uses vkCmdDrawIndexedIndirectCount per draw slot.
    // When null:     ForwardRenderPass uses the direct vkCmdDrawIndexed path.
    VkBuffer  indirectDrawBuffer = VK_NULL_HANDLE;
    VkBuffer  drawCountBuffer    = VK_NULL_HANDLE;
    uint32_t  indirectDrawCount  = 0;  // number of occupied slots in the indirect buffers
};

struct BuildPassInfo
{
    DeviceContext*        dev               = nullptr;
    VkRenderPass          forwardRenderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout uboLayout         = VK_NULL_HANDLE;
    VkExtent2D            extent            = {};
    uint32_t              framesInFlight    = 2;
    uint32_t              maxObjects        = 0;
    std::string           shaderDir;
};

class IPass
{
public:
    virtual ~IPass() = default;
    virtual void Build(const BuildPassInfo&)           = 0;
    virtual void Rebuild(DeviceContext&, VkExtent2D)   = 0;
    virtual void Record(VkCommandBuffer, PassContext&) = 0;
    virtual void Destroy(VkDevice)                     = 0;
};

} // namespace xcel
