#pragma once
#include "Graphics/IPass.h"
#include "Graphics/GpuBuffer.h"
#include <string>
#include <vector>

namespace xcel {

class FrustumCullPass final : public IPass
{
public:
    FrustumCullPass()  = default;
    ~FrustumCullPass() = default;

    FrustumCullPass(const FrustumCullPass&)            = delete;
    FrustumCullPass& operator=(const FrustumCullPass&) = delete;

    void Build(const BuildPassInfo&)           override;
    void Rebuild(DeviceContext&, VkExtent2D)   override;
    void Record(VkCommandBuffer, PassContext&) override;
    void Destroy(VkDevice)                     override;

private:
    struct FrameResources
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        GpuBuffer       inputSSBO;
        GpuBuffer       indirectBuffer;
        GpuBuffer       countBuffer;
    };

    VkDevice              m_device         = VK_NULL_HANDLE;
    VkPhysicalDevice      m_physDevice     = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_dsLayout       = VK_NULL_HANDLE;
    VkDescriptorPool      m_dsPool         = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline            m_pipeline       = VK_NULL_HANDLE;

    std::vector<FrameResources> m_frames;

    uint32_t    m_maxDrawCalls = 0;
    std::string m_shaderDir;
};

} // namespace xcel
