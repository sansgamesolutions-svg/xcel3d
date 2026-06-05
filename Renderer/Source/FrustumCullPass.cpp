#include "Renderer/FrustumCullPass.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/DeviceContext.h"
#include <glm/gtc/matrix_access.hpp>
#include <fstream>
#include <stdexcept>
#include <array>
#include <vector>
#include <cstring>

namespace xcel {

// Extracted from the view-projection matrix (Gribb-Hartmann method, GL convention).
static std::array<glm::vec4, 6> ExtractFrustumPlanes(const glm::mat4& vp)
{
    std::array<glm::vec4, 6> planes{};
    // left
    planes[0] = glm::row(vp, 3) + glm::row(vp, 0);
    // right
    planes[1] = glm::row(vp, 3) - glm::row(vp, 0);
    // bottom
    planes[2] = glm::row(vp, 3) + glm::row(vp, 1);
    // top
    planes[3] = glm::row(vp, 3) - glm::row(vp, 1);
    // near
    planes[4] = glm::row(vp, 3) + glm::row(vp, 2);
    // far
    planes[5] = glm::row(vp, 3) - glm::row(vp, 2);

    for (auto& p : planes) {
        float len = glm::length(glm::vec3(p));
        if (len > 1e-6f) p /= len;
    }
    return planes;
}

static std::vector<char> LoadSpirV(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("FrustumCullPass: cannot open SPIR-V: " + path);
    size_t size = (size_t)file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
}

struct FrustumCullPush {
    glm::vec4 frustumPlanes[6];
    uint32_t  objectCount;
    uint32_t  _pad[3];
};
static_assert(sizeof(FrustumCullPush) <= 128, "push constant too large");

struct FrustumCullPass::Impl {
    std::string shaderDir;
    uint32_t    maxObjects = 0;

    // Compute pipeline resources
    VkDescriptorSetLayout dsLayout   = VK_NULL_HANDLE;
    VkDescriptorPool      dsPool     = VK_NULL_HANDLE;
    VkDescriptorSet       dsSet      = VK_NULL_HANDLE;
    VkPipelineLayout      plLayout   = VK_NULL_HANDLE;
    VkPipeline            pipeline   = VK_NULL_HANDLE;

    // SSBOs
    GpuBuffer objectBuffer;
    GpuBuffer drawCmdBuffer;

    // Push constant data
    FrustumCullPush push{};
    uint32_t        objectCount = 0;
};

FrustumCullPass::FrustumCullPass(std::string shaderDir)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->shaderDir = std::move(shaderDir);
}

FrustumCullPass::~FrustumCullPass() = default;

void FrustumCullPass::Build(DeviceContext& dev, const BuildPassInfo& info)
{
    m_impl->maxObjects = info.maxObjectCount;
    VkDevice device    = dev.Device();

    // ── Descriptor set layout ────────────────────────────────────────────────
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dsInfo{};
    dsInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsInfo.bindingCount = (uint32_t)bindings.size();
    dsInfo.pBindings    = bindings.data();
    if (vkCreateDescriptorSetLayout(device, &dsInfo, nullptr, &m_impl->dsLayout) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreateDescriptorSetLayout failed");

    // ── Descriptor pool ──────────────────────────────────────────────────────
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_impl->dsPool) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreateDescriptorPool failed");

    // ── Allocate descriptor set ──────────────────────────────────────────────
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_impl->dsPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_impl->dsLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_impl->dsSet) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkAllocateDescriptorSets failed");

    // ── SSBOs ────────────────────────────────────────────────────────────────
    VkDeviceSize objBytes = sizeof(CullableObject) * m_impl->maxObjects;
    VkDeviceSize cmdBytes = sizeof(VkDrawIndexedIndirectCommand) * m_impl->maxObjects;

    m_impl->objectBuffer.Create(device, dev.PhysicalDevice(), objBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_impl->drawCmdBuffer.Create(device, dev.PhysicalDevice(), cmdBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // ── Update descriptor set ────────────────────────────────────────────────
    VkDescriptorBufferInfo objBufInfo{};
    objBufInfo.buffer = m_impl->objectBuffer.Buffer();
    objBufInfo.offset = 0;
    objBufInfo.range  = objBytes;

    VkDescriptorBufferInfo cmdBufInfo{};
    cmdBufInfo.buffer = m_impl->drawCmdBuffer.Buffer();
    cmdBufInfo.offset = 0;
    cmdBufInfo.range  = cmdBytes;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = m_impl->dsSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo     = &objBufInfo;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = m_impl->dsSet;
    writes[1].dstBinding      = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo     = &cmdBufInfo;

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);

    // ── Pipeline layout (push constants) ────────────────────────────────────
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(FrustumCullPush);

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount         = 1;
    plInfo.pSetLayouts            = &m_impl->dsLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges    = &pcRange;
    if (vkCreatePipelineLayout(device, &plInfo, nullptr, &m_impl->plLayout) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreatePipelineLayout failed");

    // ── Compute pipeline ─────────────────────────────────────────────────────
    auto code = LoadSpirV(m_impl->shaderDir + "frustum_cull.comp.spv");

    VkShaderModuleCreateInfo smInfo{};
    smInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smInfo.codeSize = code.size();
    smInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule sm;
    if (vkCreateShaderModule(device, &smInfo, nullptr, &sm) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreateShaderModule failed");

    VkComputePipelineCreateInfo cpInfo{};
    cpInfo.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    cpInfo.stage.module = sm;
    cpInfo.stage.pName  = "main";
    cpInfo.layout       = m_impl->plLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpInfo, nullptr, &m_impl->pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, sm, nullptr);
        throw std::runtime_error("FrustumCullPass: vkCreateComputePipelines failed");
    }
    vkDestroyShaderModule(device, sm, nullptr);
}

void FrustumCullPass::Rebuild(DeviceContext&, VkExtent2D, VkRenderPass)
{
    // No size-dependent resources.
}

void FrustumCullPass::SetObjects(DeviceContext& dev,
                                  const std::vector<CullableObject>& objects,
                                  const glm::mat4& viewProj)
{
    if (objects.empty()) return;

    auto planes = ExtractFrustumPlanes(viewProj);
    for (int i = 0; i < 6; ++i)
        m_impl->push.frustumPlanes[i] = planes[static_cast<size_t>(i)];

    m_impl->objectCount          = (uint32_t)objects.size();
    m_impl->push.objectCount     = m_impl->objectCount;

    m_impl->objectBuffer.UploadViaStaging(dev, objects.data(),
                                           sizeof(CullableObject) * objects.size());
}

void FrustumCullPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    if (m_impl->objectCount == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_impl->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_impl->plLayout, 0, 1, &m_impl->dsSet, 0, nullptr);
    vkCmdPushConstants(cmd, m_impl->plLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(FrustumCullPush), &m_impl->push);

    uint32_t groups = (m_impl->objectCount + 63u) / 64u;
    vkCmdDispatch(cmd, groups, 1, 1);

    // Barrier: compute write → indirect command read
    VkBufferMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask       = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer              = m_impl->drawCmdBuffer.Buffer();
    barrier.offset              = 0;
    barrier.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr);

    // Publish output buffer to the pass context for ForwardRenderPass to consume.
    ctx.indirectDrawBuffer = m_impl->drawCmdBuffer.Buffer();
    ctx.maxDrawCount       = m_impl->objectCount;
}

void FrustumCullPass::Destroy(VkDevice device)
{
    if (m_impl->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_impl->pipeline, nullptr);
        m_impl->pipeline = VK_NULL_HANDLE;
    }
    if (m_impl->plLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_impl->plLayout, nullptr);
        m_impl->plLayout = VK_NULL_HANDLE;
    }
    if (m_impl->dsPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_impl->dsPool, nullptr);
        m_impl->dsPool = VK_NULL_HANDLE;
    }
    if (m_impl->dsLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_impl->dsLayout, nullptr);
        m_impl->dsLayout = VK_NULL_HANDLE;
    }
    m_impl->objectBuffer.Destroy(device);
    m_impl->drawCmdBuffer.Destroy(device);
}

} // namespace xcel
