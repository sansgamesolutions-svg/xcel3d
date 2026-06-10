#pragma once
#include "Renderer/IPass.h"
#include "Renderer/GpuBuffer.h"
#include <string>

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
    VkDevice              m_device         = VK_NULL_HANDLE;
    VkPhysicalDevice      m_physDevice     = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_dsLayout       = VK_NULL_HANDLE;
    VkDescriptorPool      m_dsPool         = VK_NULL_HANDLE;
    VkDescriptorSet       m_descriptorSet  = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline            m_pipeline       = VK_NULL_HANDLE;

    GpuBuffer m_inputSSBO;
    GpuBuffer m_indirectBuffer;
    GpuBuffer m_countBuffer;

    uint32_t    m_maxDrawCalls = 0;
    std::string m_shaderDir;
};

} // namespace xcel
