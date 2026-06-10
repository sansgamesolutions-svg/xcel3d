#include "Renderer/Manipulator/ManipulatorPass.h"
#include "Renderer/DeviceContext.h"
#include "Renderer/GpuBuffer.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <array>

namespace xcel {

// Push constant layout for manipulator.frag.
struct ManipulatorPC
{
    float colorOverride[4]; // rgba; w>0 = override, w==0 = use vertex color
};

void ManipulatorPass::CreateOverlayRenderPass(VkDevice device, VkFormat color, VkFormat depth)
{
    std::array<VkAttachmentDescription, 2> attachments{};

    // Color: LOAD preserves ForwardRenderPass output; STORE so presenter sees it.
    attachments[0].format         = color;
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth: pipelines are depth-test-off so values are irrelevant; DONT_CARE avoids
    // a load but still satisfies the renderpass attachment requirement.
    attachments[1].format         = depth;
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                      | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dep;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("ManipulatorPass: vkCreateRenderPass failed");
}

void ManipulatorPass::CreatePipelines(VkDevice              device,
                                       VkDescriptorSetLayout uboLayout,
                                       VkExtent2D            extent,
                                       const std::string&    shaderDir)
{
    PipelineConfig solidCfg{};
    solidCfg.depthTestEnable  = false;
    solidCfg.depthWriteEnable = false;
    solidCfg.alphaBlend       = false;
    solidCfg.cullMode         = VK_CULL_MODE_NONE;
    solidCfg.pushConstantSize = sizeof(ManipulatorPC);

    m_solidPipeline.Create(device, m_renderPass, uboLayout, extent,
                            shaderDir + "manipulator.vert.spv",
                            shaderDir + "manipulator.frag.spv",
                            solidCfg);

    PipelineConfig alphaCfg   = solidCfg;
    alphaCfg.alphaBlend       = true;

    m_alphaPipeline.Create(device, m_renderPass, uboLayout, extent,
                            shaderDir + "manipulator.vert.spv",
                            shaderDir + "manipulator.frag.spv",
                            alphaCfg);
}

void ManipulatorPass::Build(const BuildPassInfo& info)
{
    m_device    = info.dev->Device();
    m_uboLayout = info.uboLayout;
    m_shaderDir = info.shaderDir;
    m_colorFmt  = info.colorFormat;
    m_depthFmt  = info.depthFormat;

    CreateOverlayRenderPass(m_device, m_colorFmt, m_depthFmt);
    CreatePipelines(m_device, m_uboLayout, info.extent, m_shaderDir);

    // Identity matrix instance buffer used for draws that supply no instance data.
    glm::mat4 identity{1.f};
    m_defaultInstanceBuf.Create(m_device, info.dev->PhysicalDevice(), sizeof(glm::mat4),
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_defaultInstanceBuf.UploadViaStaging(*info.dev, &identity, sizeof(glm::mat4));
}

void ManipulatorPass::Rebuild(DeviceContext& dev, VkExtent2D ext)
{
    m_solidPipeline.Destroy(dev.Device());
    m_alphaPipeline.Destroy(dev.Device());
    CreatePipelines(dev.Device(), m_uboLayout, ext, m_shaderDir);
}

void ManipulatorPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    // Skip entirely if there's nothing to draw.
    if (ctx.manipulatorSolidDrawCalls.empty() && ctx.manipulatorAlphaDrawCalls.empty())
        return;

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_renderPass;
    rpInfo.framebuffer       = ctx.swapchainFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = ctx.extent;
    rpInfo.clearValueCount   = 0; // no clear — we LOAD

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Full-viewport for scene manipulators.
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

    // Helper to draw one batch.
    auto drawBatch = [&](VkPipeline pipe, VkPipelineLayout layout,
                          std::span<const DrawCall> calls)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                layout, 0, 1, &ctx.uboDescriptorSet, 0, nullptr);

        for (const auto& dc : calls) {
            // Encode color override as push constant.
            // Convention: material.ambientFactor/diffuseFactor/specularFactor = rgb,
            //             material.shininess = alpha/flag.
            ManipulatorPC pc{};
            pc.colorOverride[0] = dc.material.ambientFactor;
            pc.colorOverride[1] = dc.material.diffuseFactor;
            pc.colorOverride[2] = dc.material.specularFactor;
            pc.colorOverride[3] = dc.material.shininess;
            vkCmdPushConstants(cmd, layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(ManipulatorPC), &pc);

            const GpuBuffer* instBuf = dc.instanceBuffer ? dc.instanceBuffer : &m_defaultInstanceBuf;
            VkBuffer     bufs[2] = {dc.vertexBuffer->Buffer(), instBuf->Buffer()};
            VkDeviceSize offs[2] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
            vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, dc.indexCount, dc.instanceCount, 0, 0, 0);
        }
    };

    // --- Scene manipulators (solid then alpha) ---
    // Separate view cube draws (material.shininess == 0, ambientFactor == 1) from axis arrows.
    std::vector<DrawCall> viewCubeDraws;
    std::vector<DrawCall> sceneDraws;
    for (const auto& dc : ctx.manipulatorSolidDrawCalls) {
        // View cube marker: ambient=1, diffuse=0, specular=0, shininess=0.
        if (dc.material.ambientFactor == 1.f && dc.material.diffuseFactor == 0.f
                && dc.material.specularFactor == 0.f && dc.material.shininess == 0.f)
            viewCubeDraws.push_back(dc);
        else
            sceneDraws.push_back(dc);
    }

    if (!sceneDraws.empty())
        drawBatch(m_solidPipeline.GetHandle(), m_solidPipeline.PipelineLayout(), sceneDraws);

    if (!ctx.manipulatorAlphaDrawCalls.empty())
        drawBatch(m_alphaPipeline.GetHandle(), m_alphaPipeline.PipelineLayout(),
                  ctx.manipulatorAlphaDrawCalls);

    // --- View cube in top-right corner viewport ---
    if (!viewCubeDraws.empty()) {
        int cubeSize = 80;
        VkViewport cubeVp{};
        cubeVp.x        = static_cast<float>(ctx.extent.width  - cubeSize);
        cubeVp.y        = 0.f;
        cubeVp.width    = static_cast<float>(cubeSize);
        cubeVp.height   = static_cast<float>(cubeSize);
        cubeVp.minDepth = 0.f;
        cubeVp.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &cubeVp);

        VkRect2D cubeScissor{};
        cubeScissor.offset = {static_cast<int32_t>(ctx.extent.width - cubeSize), 0};
        cubeScissor.extent = {static_cast<uint32_t>(cubeSize), static_cast<uint32_t>(cubeSize)};
        vkCmdSetScissor(cmd, 0, 1, &cubeScissor);

        drawBatch(m_solidPipeline.GetHandle(), m_solidPipeline.PipelineLayout(), viewCubeDraws);
    }

    vkCmdEndRenderPass(cmd);
}

void ManipulatorPass::Destroy(VkDevice device)
{
    m_defaultInstanceBuf.Destroy(device);
    m_alphaPipeline.Destroy(device);
    m_solidPipeline.Destroy(device);
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

} // namespace xcel
