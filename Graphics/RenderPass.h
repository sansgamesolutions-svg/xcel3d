#pragma once
#include <vulkan/vulkan.h>
#include <memory>

namespace xcel {

class RenderPass {
public:
    RenderPass();
    ~RenderPass();

    void Create(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void Destroy(VkDevice device);

    VkRenderPass GetHandle() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
