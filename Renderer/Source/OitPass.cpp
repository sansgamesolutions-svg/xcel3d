#include "Renderer/OitPass.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/Swapchain.h"
#include "Renderer/DeviceContext.h"
#include <stdexcept>
#include <array>
#include <span>
#include <vector>

namespace xcel {

void OitPass::CreateRenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat)
{
    std::array<VkAttachmentDescription, 4> attachments{};

    // 0: swapchain color. Untouched by subpass 0 (preserved), written by composite.
    attachments[0].format         = colorFormat;
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 1: depth from ForwardRenderPass. Read-only in subpass 0 (occludes OIT geometry
    // against opaque draws); never written here.
    attachments[1].format         = depthFormat;
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 2: weighted color-sum accumulation target (cleared, never needs to survive
    // past this render pass once the composite subpass has consumed it).
    attachments[2].format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[2].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 3: revealage product target. Cleared to 1.0 (fully revealed) by Record().
    attachments[3]         = attachments[2];
    attachments[3].format  = VK_FORMAT_R16_SFLOAT;

    VkAttachmentReference colorRef  {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef  {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    VkAttachmentReference accumRef  {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference revealRef {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    std::array<VkAttachmentReference, 2> accumulateColorRefs = {accumRef, revealRef};
    const uint32_t preservedColor = 0;

    VkSubpassDescription accumulateSubpass{};
    accumulateSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    accumulateSubpass.colorAttachmentCount    = static_cast<uint32_t>(accumulateColorRefs.size());
    accumulateSubpass.pColorAttachments       = accumulateColorRefs.data();
    accumulateSubpass.pDepthStencilAttachment = &depthRef;
    accumulateSubpass.preserveAttachmentCount = 1;
    accumulateSubpass.pPreserveAttachments    = &preservedColor;

    std::array<VkAttachmentReference, 2> compositeInputRefs = {
        VkAttachmentReference{2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        VkAttachmentReference{3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    };

    VkSubpassDescription compositeSubpass{};
    compositeSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    compositeSubpass.colorAttachmentCount = 1;
    compositeSubpass.pColorAttachments    = &colorRef;
    compositeSubpass.inputAttachmentCount = static_cast<uint32_t>(compositeInputRefs.size());
    compositeSubpass.pInputAttachments    = compositeInputRefs.data();

    std::array<VkSubpassDescription, 2> subpasses = {accumulateSubpass, compositeSubpass};

    std::array<VkSubpassDependency, 2> deps{};

    // ForwardRenderPass's color/depth writes must complete before this pass reads/writes them.
    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                          | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                          | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    // Subpass 0's accum/reveal writes must complete before subpass 1 reads them as input attachments.
    deps[1].srcSubpass      = 0;
    deps[1].dstSubpass      = 1;
    deps[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = static_cast<uint32_t>(subpasses.size());
    rpInfo.pSubpasses      = subpasses.data();
    rpInfo.dependencyCount = static_cast<uint32_t>(deps.size());
    rpInfo.pDependencies   = deps.data();

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("OitPass: vkCreateRenderPass failed");
}

void OitPass::CreateInputAttachmentLayout(VkDevice device)
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1]                 = bindings[0];
    bindings[1].binding         = 1;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_inputAttachmentLayout) != VK_SUCCESS)
        throw std::runtime_error("OitPass: vkCreateDescriptorSetLayout failed");
}

void OitPass::CreatePipelines(VkDevice device, VkExtent2D extent)
{
    std::vector<VkDescriptorSetLayout> sceneLayouts;
    sceneLayouts.push_back(m_uboLayout);
    if (m_bindlessLayout != VK_NULL_HANDLE)
        sceneLayouts.push_back(m_bindlessLayout);

    // Accum: weighted premultiplied color + weight, additively summed (ONE/ONE).
    VkPipelineColorBlendAttachmentState accumBlend{};
    accumBlend.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                   | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    accumBlend.blendEnable         = VK_TRUE;
    accumBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    accumBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    accumBlend.colorBlendOp        = VK_BLEND_OP_ADD;
    accumBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    accumBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    accumBlend.alphaBlendOp        = VK_BLEND_OP_ADD;

    // Reveal: multiplicative (1-alpha) product (ZERO/ONE_MINUS_SRC_COLOR).
    VkPipelineColorBlendAttachmentState revealBlend{};
    revealBlend.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT;
    revealBlend.blendEnable         = VK_TRUE;
    revealBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    revealBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    revealBlend.colorBlendOp        = VK_BLEND_OP_ADD;
    revealBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    revealBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    revealBlend.alphaBlendOp        = VK_BLEND_OP_ADD;

    PipelineConfig accumulateCfg{};
    accumulateCfg.depthTestEnable     = true;
    accumulateCfg.depthWriteEnable    = false;
    accumulateCfg.cullMode            = VK_CULL_MODE_NONE;
    accumulateCfg.mrtBlendAttachments = {accumBlend, revealBlend};
    accumulateCfg.subpass             = 0;

    m_accumulatePipeline.Create(device, m_renderPass, std::span<const VkDescriptorSetLayout>{sceneLayouts}, extent,
                                m_shaderDir + "mesh.vert.spv",
                                m_shaderDir + "oit_accumulate.frag.spv",
                                accumulateCfg);

    PipelineConfig compositeCfg{};
    compositeCfg.depthTestEnable  = false;
    compositeCfg.depthWriteEnable = false;
    compositeCfg.alphaBlend       = true;
    compositeCfg.srcColorFactor   = VK_BLEND_FACTOR_SRC_ALPHA;
    compositeCfg.dstColorFactor   = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    compositeCfg.noVertexInput    = true;
    compositeCfg.subpass          = 1;

    const std::array<VkDescriptorSetLayout, 1> compositeLayouts = {m_inputAttachmentLayout};
    m_compositePipeline.Create(device, m_renderPass,
                               std::span<const VkDescriptorSetLayout>{compositeLayouts}, extent,
                               m_shaderDir + "oit_composite.vert.spv",
                               m_shaderDir + "oit_composite.frag.spv",
                               compositeCfg);
}

void OitPass::CreateCompositeDescriptors(VkDevice device)
{
    const uint32_t imageCount = static_cast<uint32_t>(m_perImage.size());

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    poolSize.descriptorCount = imageCount * 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = imageCount;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_inputAttachmentPool) != VK_SUCCESS)
        throw std::runtime_error("OitPass: vkCreateDescriptorPool failed");

    std::vector<VkDescriptorSetLayout> layouts(imageCount, m_inputAttachmentLayout);
    std::vector<VkDescriptorSet>       sets(imageCount);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_inputAttachmentPool;
    allocInfo.descriptorSetCount = imageCount;
    allocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, sets.data()) != VK_SUCCESS)
        throw std::runtime_error("OitPass: vkAllocateDescriptorSets failed");

    for (uint32_t i = 0; i < imageCount; ++i) {
        m_perImage[i].compositeSet = sets[i];

        VkDescriptorImageInfo accumInfo{};
        accumInfo.imageView   = m_perImage[i].accum.ImageView();
        accumInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo revealInfo{};
        revealInfo.imageView   = m_perImage[i].reveal.ImageView();
        revealInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = sets[i];
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[0].pImageInfo      = &accumInfo;

        writes[1]            = writes[0];
        writes[1].dstBinding = 1;
        writes[1].pImageInfo = &revealInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void OitPass::CreatePerImageResources(DeviceContext& dev, VkExtent2D extent)
{
    const uint32_t imageCount = m_swapchain->ImageCount();
    m_perImage.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; ++i) {
        auto& img = m_perImage[i];
        img.accum.Create(dev, extent.width, extent.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                         VK_IMAGE_ASPECT_COLOR_BIT);
        img.reveal.Create(dev, extent.width, extent.height, VK_FORMAT_R16_SFLOAT,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT);

        std::array<VkImageView, 4> attachments = {
            m_swapchain->ImageView(i), m_swapchain->DepthImageView(),
            img.accum.ImageView(), img.reveal.ImageView(),
        };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments    = attachments.data();
        fbInfo.width           = extent.width;
        fbInfo.height          = extent.height;
        fbInfo.layers          = 1;
        if (vkCreateFramebuffer(dev.Device(), &fbInfo, nullptr, &img.framebuffer) != VK_SUCCESS)
            throw std::runtime_error("OitPass: vkCreateFramebuffer failed");
    }

    CreateCompositeDescriptors(dev.Device());
}

void OitPass::DestroyPerImageResources(VkDevice device)
{
    if (m_inputAttachmentPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_inputAttachmentPool, nullptr);
        m_inputAttachmentPool = VK_NULL_HANDLE;
    }
    for (auto& img : m_perImage) {
        if (img.framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(device, img.framebuffer, nullptr);
        img.accum.Destroy(device);
        img.reveal.Destroy(device);
    }
    m_perImage.clear();
}

void OitPass::Build(const BuildPassInfo& info)
{
    m_device         = info.dev->Device();
    m_colorFmt        = info.colorFormat;
    m_depthFmt        = info.depthFormat;
    m_uboLayout       = info.uboLayout;
    m_bindlessLayout  = info.bindlessLayout;
    m_shaderDir       = info.shaderDir;
    m_swapchain       = info.swapchain;
    m_extent          = info.extent;

    CreateRenderPass(m_device, m_colorFmt, m_depthFmt);
    CreateInputAttachmentLayout(m_device);
    CreatePipelines(m_device, m_extent);
    CreatePerImageResources(*info.dev, m_extent);
}

void OitPass::Rebuild(DeviceContext& dev, VkExtent2D ext)
{
    m_extent = ext;
    DestroyPerImageResources(dev.Device());

    m_accumulatePipeline.Destroy(dev.Device());
    m_compositePipeline.Destroy(dev.Device());
    CreatePipelines(dev.Device(), ext);

    CreatePerImageResources(dev, ext);
}

void OitPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    bool any = false;
    for (const auto& dc : ctx.directDrawCalls)
        if (dc.blendMode == BlendMode::WeightedBlendedOIT) { any = true; break; }
    if (!any) return;

    const auto& img = m_perImage[ctx.imageIndex];

    std::array<VkClearValue, 4> clearValues{};
    clearValues[2].color = {{0.f, 0.f, 0.f, 0.f}}; // accum
    clearValues[3].color = {{1.f, 0.f, 0.f, 0.f}}; // reveal starts fully revealed

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_renderPass;
    rpInfo.framebuffer       = img.framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = ctx.extent;
    rpInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x = 0.f; vp.y = 0.f;
    vp.width    = static_cast<float>(ctx.extent.width);
    vp.height   = static_cast<float>(ctx.extent.height);
    vp.minDepth = 0.f; vp.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = ctx.extent;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    // --- Subpass 0: accumulate ---
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_accumulatePipeline.GetHandle());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_accumulatePipeline.PipelineLayout(),
                            0, 1, &ctx.uboDescriptorSet, 0, nullptr);
    if (ctx.bindlessDescriptorSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_accumulatePipeline.PipelineLayout(),
                                1, 1, &ctx.bindlessDescriptorSet, 0, nullptr);

    for (const auto& dc : ctx.directDrawCalls) {
        if (dc.blendMode != BlendMode::WeightedBlendedOIT) continue;
        vkCmdPushConstants(cmd, m_accumulatePipeline.PipelineLayout(),
                           VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MaterialData), &dc.material);
        VkBuffer     bufs[2] = {dc.vertexBuffer->Buffer(), dc.instanceBuffer->Buffer()};
        VkDeviceSize offs[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
        vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, dc.indexCount, dc.instanceCount, 0, 0, 0);
    }

    // --- Subpass 1: composite ---
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipeline.GetHandle());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_compositePipeline.PipelineLayout(),
                            0, 1, &img.compositeSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
}

void OitPass::Destroy(VkDevice device)
{
    DestroyPerImageResources(device);
    if (m_inputAttachmentLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_inputAttachmentLayout, nullptr);
        m_inputAttachmentLayout = VK_NULL_HANDLE;
    }
    m_compositePipeline.Destroy(device);
    m_accumulatePipeline.Destroy(device);
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

} // namespace xcel
