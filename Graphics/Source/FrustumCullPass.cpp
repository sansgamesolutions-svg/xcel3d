#include "Graphics/FrustumCullPass.h"
#include "Graphics/GpuBuffer.h"
#include <glm/glm.hpp>
#include <stdexcept>
#include <array>
#include <fstream>
#include <vector>
#include <cstring>
#include <limits>

namespace xcel {

namespace {

struct alignas(16) CullObject
{
    glm::vec4 aabbMin;
    glm::vec4 aabbMax;
};

struct FrustumPushConstants
{
    glm::vec4 planes[6];
    uint32_t  objectCount;
};

static std::array<glm::vec4, 6> ExtractFrustumPlanes(const glm::mat4& vp)
{
    // Gribb-Hartmann extraction from a column-major GLM matrix.
    // vp[col][row]: rows are accessed as vp[0][r], vp[1][r], vp[2][r], vp[3][r].
    auto row = [&](int r) {
        return glm::vec4(vp[0][r], vp[1][r], vp[2][r], vp[3][r]);
    };
    const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);

    std::array<glm::vec4, 6> planes = {
        r3 + r0,   // Left
        r3 - r0,   // Right
        r3 + r1,   // Bottom
        r3 - r1,   // Top
        r2,        // Near  (GLM_FORCE_DEPTH_ZERO_TO_ONE: z in [0,1])
        r3 - r2,   // Far
    };
    for (auto& p : planes) {
        float len = glm::length(glm::vec3(p));
        if (len > 1e-6f) p /= len;
    }
    return planes;
}

static std::vector<char> LoadSpirV(const std::string& path)
{
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("FrustumCullPass: cannot open " + path);
    const auto size = static_cast<size_t>(f.tellg());
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), static_cast<std::streamsize>(size));
    return buf;
}

} // anonymous namespace

void FrustumCullPass::Build(const BuildPassInfo& info)
{
    m_device     = info.dev->Device();
    m_physDevice = info.dev->PhysicalDevice();
    m_shaderDir  = info.shaderDir;
    m_maxDrawCalls = std::max(info.maxObjects, 1u);

    VkDevice dev = m_device;

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

    VkDescriptorSetLayoutCreateInfo dsLayoutCI{};
    dsLayoutCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    dsLayoutCI.pBindings    = bindings.data();
    if (vkCreateDescriptorSetLayout(dev, &dsLayoutCI, nullptr, &m_dsLayout) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreateDescriptorSetLayout failed");

    // ── Descriptor pool ──────────────────────────────────────────────────────
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.maxSets       = 1;
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes    = &poolSize;
    if (vkCreateDescriptorPool(dev, &poolCI, nullptr, &m_dsPool) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreateDescriptorPool failed");

    VkDescriptorSetAllocateInfo dsAllocInfo{};
    dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAllocInfo.descriptorPool     = m_dsPool;
    dsAllocInfo.descriptorSetCount = 1;
    dsAllocInfo.pSetLayouts        = &m_dsLayout;
    if (vkAllocateDescriptorSets(dev, &dsAllocInfo, &m_descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkAllocateDescriptorSets failed");

    // ── Pipeline layout (push constants: 6 * vec4 + uint = 100 bytes) ───────
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(FrustumPushConstants);

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount         = 1;
    plCI.pSetLayouts            = &m_dsLayout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges    = &pcRange;
    if (vkCreatePipelineLayout(dev, &plCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreatePipelineLayout failed");

    // ── Compute pipeline ─────────────────────────────────────────────────────
    auto spv = LoadSpirV(m_shaderDir + "frustum_cull.comp.spv");

    VkShaderModuleCreateInfo smCI{};
    smCI.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smCI.codeSize = spv.size();
    smCI.pCode    = reinterpret_cast<const uint32_t*>(spv.data());
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev, &smCI, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreateShaderModule failed");

    VkComputePipelineCreateInfo cpCI{};
    cpCI.sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpCI.stage.sType        = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpCI.stage.stage        = VK_SHADER_STAGE_COMPUTE_BIT;
    cpCI.stage.module       = shaderModule;
    cpCI.stage.pName        = "main";
    cpCI.layout             = m_pipelineLayout;

    VkResult r = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpCI, nullptr, &m_pipeline);
    vkDestroyShaderModule(dev, shaderModule, nullptr);
    if (r != VK_SUCCESS)
        throw std::runtime_error("FrustumCullPass: vkCreateComputePipelines failed");

    // ── GPU buffers ──────────────────────────────────────────────────────────
    const VkDeviceSize inputSize    = sizeof(CullObject) * m_maxDrawCalls;
    const VkDeviceSize indirectSize = sizeof(VkDrawIndexedIndirectCommand) * m_maxDrawCalls;
    const VkDeviceSize countSize    = sizeof(uint32_t) * m_maxDrawCalls;

    m_inputSSBO.Create(dev, m_physDevice, inputSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    m_indirectBuffer.Create(dev, m_physDevice, indirectSize,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    m_countBuffer.Create(dev, m_physDevice, countSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // ── Update descriptor set ────────────────────────────────────────────────
    std::array<VkDescriptorBufferInfo, 2> bufInfos{};
    bufInfos[0].buffer = m_inputSSBO.Buffer();
    bufInfos[0].offset = 0;
    bufInfos[0].range  = inputSize;

    bufInfos[1].buffer = m_countBuffer.Buffer();
    bufInfos[1].offset = 0;
    bufInfos[1].range  = countSize;

    std::array<VkWriteDescriptorSet, 2> writes{};
    for (int i = 0; i < 2; ++i) {
        writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet          = m_descriptorSet;
        writes[i].dstBinding      = static_cast<uint32_t>(i);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo     = &bufInfos[i];
    }
    vkUpdateDescriptorSets(dev, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void FrustumCullPass::Rebuild(DeviceContext&, VkExtent2D)
{
    // Buffers are screen-size-independent; nothing to rebuild on resize.
}

void FrustumCullPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    const uint32_t n = static_cast<uint32_t>(ctx.directDrawCalls.size());
    if (n == 0 || n > m_maxDrawCalls) return;

    // ── Upload input SSBO (world-space AABBs) ────────────────────────────────
    {
        std::vector<CullObject> objects(n);
        for (uint32_t i = 0; i < n; ++i) {
            objects[i].aabbMin = glm::vec4(ctx.directDrawCalls[i].aabbMin, 0.f);
            objects[i].aabbMax = glm::vec4(ctx.directDrawCalls[i].aabbMax, 0.f);
        }
        m_inputSSBO.WriteHostVisible(objects.data(),
                                           static_cast<VkDeviceSize>(sizeof(CullObject) * n));
    }

    // ── Pre-fill indirect buffer with draw commands ──────────────────────────
    {
        std::vector<VkDrawIndexedIndirectCommand> cmds(n);
        for (uint32_t i = 0; i < n; ++i) {
            cmds[i].indexCount    = ctx.directDrawCalls[i].indexCount;
            cmds[i].instanceCount = ctx.directDrawCalls[i].instanceCount;
            cmds[i].firstIndex    = 0;
            cmds[i].vertexOffset  = 0;
            cmds[i].firstInstance = 0;
        }
        m_indirectBuffer.WriteHostVisible(cmds.data(),
            static_cast<VkDeviceSize>(sizeof(VkDrawIndexedIndirectCommand) * n));
    }

    // ── Reset count buffer to zero via transfer ──────────────────────────────
    vkCmdFillBuffer(cmd, m_countBuffer.Buffer(), 0,
                    static_cast<VkDeviceSize>(sizeof(uint32_t) * n), 0u);

    // ── Barrier: transfer write → compute shader read/write ─────────────────
    {
        VkMemoryBarrier mb{};
        mb.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &mb, 0, nullptr, 0, nullptr);
    }

    // ── Dispatch compute ─────────────────────────────────────────────────────
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_pipelineLayout, 0, 1, &m_descriptorSet,
                            0, nullptr);

    const auto planes = ExtractFrustumPlanes(ctx.viewProj);
    FrustumPushConstants pc{};
    for (int i = 0; i < 6; ++i) pc.planes[i] = planes[i];
    pc.objectCount = n;
    vkCmdPushConstants(cmd, m_pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    const uint32_t groups = (n + 63u) / 64u;
    vkCmdDispatch(cmd, groups, 1, 1);

    // ── Barrier: compute write → indirect read ───────────────────────────────
    {
        VkMemoryBarrier mb{};
        mb.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0, 1, &mb, 0, nullptr, 0, nullptr);
    }

    // ── Tell ForwardRenderPass to use indirect path ──────────────────────────
    ctx.indirectDrawBuffer = m_indirectBuffer.Buffer();
    ctx.drawCountBuffer    = m_countBuffer.Buffer();
    ctx.indirectDrawCount  = n;
}

void FrustumCullPass::Destroy(VkDevice device)
{
    m_countBuffer.Destroy(device);
    m_indirectBuffer.Destroy(device);
    m_inputSSBO.Destroy(device);

    if (m_pipeline      != VK_NULL_HANDLE) vkDestroyPipeline      (device, m_pipeline,      nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    if (m_dsPool         != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_dsPool,         nullptr);
    if (m_dsLayout       != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_dsLayout,  nullptr);

    m_pipeline       = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_dsPool         = VK_NULL_HANDLE;
    m_dsLayout       = VK_NULL_HANDLE;
    m_descriptorSet  = VK_NULL_HANDLE;
}

} // namespace xcel
