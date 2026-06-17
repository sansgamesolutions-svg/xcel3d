#pragma once
#include <vulkan/vulkan.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xcel {

struct PipelineConfig
{
    bool            depthTestEnable  = true;
    bool            depthWriteEnable = true;
    bool            alphaBlend       = false;
    bool            includeTexCoord  = true;   // set false when shader has no location 7
    VkCullModeFlags cullMode         = VK_CULL_MODE_BACK_BIT;
    // Push-constant size in bytes (fragment stage). 0 = no push constant.
    uint32_t        pushConstantSize = 0;

    // Blend factors used when alphaBlend == true.
    // Defaults reproduce the classic SrcAlpha / OneMinusSrcAlpha behaviour.
    VkBlendFactor   srcColorFactor   = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor   dstColorFactor   = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendFactor   srcAlphaFactor   = VK_BLEND_FACTOR_ONE;
    VkBlendFactor   dstAlphaFactor   = VK_BLEND_FACTOR_ZERO;

    // Multi-render-target blend states, one per color attachment, used verbatim.
    // Empty (default) keeps the single-attachment path driven by the fields above.
    // Requires VkPhysicalDeviceFeatures::independentBlend when entries differ.
    std::vector<VkPipelineColorBlendAttachmentState> mrtBlendAttachments;

    // True for pipelines that take no vertex buffers (e.g. a fullscreen-triangle
    // composite pass driven entirely by gl_VertexIndex).
    bool            noVertexInput    = false;

    // Subpass index within renderPass this pipeline is bound to (e.g. OitPass's
    // composite pipeline runs in subpass 1, after the accumulate subpass).
    uint32_t        subpass          = 0;
};

class Pipeline
{
public:
    Pipeline()  = default;
    ~Pipeline() = default;

    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline(Pipeline&& other) noexcept
        : m_pipelineLayout(std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE))
        , m_pipeline(std::exchange(other.m_pipeline, VK_NULL_HANDLE))
    {}
    Pipeline& operator=(Pipeline&&) = delete;

    // Primary overload: accepts any number of descriptor set layouts (set=0, set=1, …).
    void Create(
        VkDevice                               device,
        VkRenderPass                           renderPass,
        std::span<const VkDescriptorSetLayout> descriptorLayouts,
        VkExtent2D                             viewportExtent,
        const std::string&                     vertSpvPath,
        const std::string&                     fragSpvPath,
        const PipelineConfig&                  config = {});

    // Backward-compatible single-layout overload. ManipulatorPass uses this.
    void Create(
        VkDevice              device,
        VkRenderPass          renderPass,
        VkDescriptorSetLayout descriptorLayout,
        VkExtent2D            viewportExtent,
        const std::string&    vertSpvPath,
        const std::string&    fragSpvPath,
        const PipelineConfig& config = {});

    void Destroy(VkDevice device);

    VkPipeline       GetHandle()      const;
    VkPipelineLayout PipelineLayout() const;

private:
    std::vector<char> LoadSpirV(const std::string& path);
    VkShaderModule    CreateShaderModule(VkDevice device, const std::vector<char>& code);

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
};

} // namespace xcel
