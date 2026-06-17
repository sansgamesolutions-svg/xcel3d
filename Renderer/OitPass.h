#pragma once
#include "Renderer/IPass.h"
#include "Renderer/Pipeline.h"
#include "Renderer/GpuImage.h"
#include <vector>
#include <string>

namespace xcel {

// Weighted Blended Order-Independent Transparency pass (McGuire & Bavoil, JCGT 2013).
// Inserted between ForwardRenderPass and ManipulatorPass. Subpass 0 ("accumulate")
// draws BlendMode::WeightedBlendedOIT geometry into two transient targets (depth-
// tested read-only against ForwardRenderPass's depth buffer); subpass 1
// ("composite") resolves them into the swapchain color attachment via a
// buffer-less fullscreen triangle.
class OitPass final : public IPass
{
public:
    OitPass()  = default;
    ~OitPass() = default;

    OitPass(const OitPass&)            = delete;
    OitPass& operator=(const OitPass&) = delete;

    void Build(const BuildPassInfo& info)             override;
    void Rebuild(DeviceContext& dev, VkExtent2D ext)   override;
    void Record(VkCommandBuffer cmd, PassContext& ctx) override;
    void Destroy(VkDevice device)                      override;

private:
    void CreateRenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void CreateInputAttachmentLayout(VkDevice device);
    void CreatePipelines(VkDevice device, VkExtent2D extent);
    void CreatePerImageResources(DeviceContext& dev, VkExtent2D extent);
    void CreateCompositeDescriptors(VkDevice device);
    void DestroyPerImageResources(VkDevice device);

    VkDevice     m_device     = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFormat     m_colorFmt   = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat     m_depthFmt   = VK_FORMAT_D32_SFLOAT;
    VkExtent2D   m_extent     = {};
    Swapchain*   m_swapchain  = nullptr; // non-owning; cached from BuildPassInfo for Rebuild

    VkDescriptorSetLayout m_uboLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bindlessLayout = VK_NULL_HANDLE;
    std::string           m_shaderDir;

    Pipeline              m_accumulatePipeline;
    Pipeline              m_compositePipeline;
    VkDescriptorSetLayout m_inputAttachmentLayout = VK_NULL_HANDLE; // set=0 for composite: accum + reveal
    VkDescriptorPool      m_inputAttachmentPool   = VK_NULL_HANDLE;

    struct PerImage
    {
        GpuImage        accum;
        GpuImage        reveal;
        VkFramebuffer   framebuffer  = VK_NULL_HANDLE;
        VkDescriptorSet compositeSet = VK_NULL_HANDLE;
    };
    std::vector<PerImage> m_perImage;
};

} // namespace xcel
