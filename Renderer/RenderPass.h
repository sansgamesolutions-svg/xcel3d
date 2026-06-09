#pragma once
#include <vulkan/vulkan.h>

namespace xcel {

class RenderPass
{
public:
    RenderPass()  = default;
    ~RenderPass() = default;

    RenderPass(const RenderPass&)            = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    void Create(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void Destroy(VkDevice device);

    VkRenderPass GetHandle() const;

private:
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};

} // namespace xcel
