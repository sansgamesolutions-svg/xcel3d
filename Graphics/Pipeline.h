#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

namespace xcel {

class Pipeline {
public:
    Pipeline();
    ~Pipeline();

    void Create(
        VkDevice              device,
        VkRenderPass          renderPass,
        VkDescriptorSetLayout descriptorLayout,
        VkExtent2D            viewportExtent,
        const std::string&    vertSpvPath,
        const std::string&    fragSpvPath
    );

    void Destroy(VkDevice device);

    VkPipeline       GetHandle()       const;
    VkPipelineLayout PipelineLayout()  const;

private:
    std::vector<char> LoadSpirV(const std::string& path);
    VkShaderModule    CreateShaderModule(VkDevice device, const std::vector<char>& code);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
