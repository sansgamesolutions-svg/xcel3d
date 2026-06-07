#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace xcel {

class Pipeline
{
public:
    Pipeline()  = default;
    ~Pipeline() = default;

    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    void Create(
        VkDevice              device,
        VkRenderPass          renderPass,
        VkDescriptorSetLayout descriptorLayout,
        VkExtent2D            viewportExtent,
        const std::string&    vertSpvPath,
        const std::string&    fragSpvPath);

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
