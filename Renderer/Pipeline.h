#pragma once
#include <vulkan/vulkan.h>
#include <span>
#include <string>
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
};

class Pipeline
{
public:
    Pipeline()  = default;
    ~Pipeline() = default;

    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;

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
