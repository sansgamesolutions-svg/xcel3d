#pragma once
#include "Graphics/IPass.h"
#include "Graphics/Pipeline.h"
#include "Graphics/GpuBuffer.h"
#include <vulkan/vulkan.h>
#include <string>

namespace xcel {

// Overlay pass that renders manipulator geometry on top of the scene.
// Uses a separate VkRenderPass with LOAD ops (preserves color and depth from
// ForwardRenderPass) and pipelines with depthTestEnable=FALSE so handles are
// always visible.
//
// The view cube is rendered last with a reduced viewport scissored to the
// top-right corner.
class ManipulatorPass final : public IPass
{
public:
    ManipulatorPass()  = default;
    ~ManipulatorPass() = default;

    ManipulatorPass(const ManipulatorPass&)            = delete;
    ManipulatorPass& operator=(const ManipulatorPass&) = delete;

    void Build(const BuildPassInfo& info)           override;
    void Rebuild(DeviceContext& dev, VkExtent2D ext) override;
    void Record(VkCommandBuffer cmd, PassContext& ctx) override;
    void Destroy(VkDevice device)                    override;

private:
    void CreateOverlayRenderPass(VkDevice device, VkFormat color, VkFormat depth);
    void CreatePipelines(VkDevice device,
                         VkDescriptorSetLayout uboLayout,
                         VkExtent2D extent,
                         const std::string& shaderDir);

    VkDevice     m_device      = VK_NULL_HANDLE;
    VkRenderPass m_renderPass  = VK_NULL_HANDLE;
    Pipeline     m_solidPipeline;
    Pipeline     m_alphaPipeline;
    GpuBuffer    m_defaultInstanceBuf; // identity mat4 for view cube

    VkDescriptorSetLayout m_uboLayout  = VK_NULL_HANDLE;
    std::string           m_shaderDir;
    VkFormat              m_colorFmt   = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat              m_depthFmt   = VK_FORMAT_D32_SFLOAT;
};

} // namespace xcel
