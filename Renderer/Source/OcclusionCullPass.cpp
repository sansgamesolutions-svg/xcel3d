#include "Renderer/OcclusionCullPass.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/DeviceContext.h"
#include <glm/glm.hpp>
#include <fstream>
#include <stdexcept>
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace xcel {

// ── Helpers ──────────────────────────────────────────────────────────────────
static std::vector<char> LoadSpirVOcc(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("OcclusionCullPass: cannot open SPIR-V: " + path);
    size_t sz = (size_t)file.tellg();
    std::vector<char> buf(sz);
    file.seekg(0);
    file.read(buf.data(), sz);
    return buf;
}

static uint32_t FindMemTypeOcc(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((filter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("OcclusionCullPass: no suitable memory type");
}

// ── Hi-Z image (R32_SFLOAT, mipmapped) ──────────────────────────────────────
// Mip 0 is written by the depth pre-pass as a COLOR attachment
// (fragment shader outputs gl_FragCoord.z to it).
// Mips 1..N are built by the Hi-Z compute pass.
struct HiZImage {
    VkImage        image      = VK_NULL_HANDLE;
    VkDeviceMemory memory     = VK_NULL_HANDLE;
    VkImageView    fullView   = VK_NULL_HANDLE;  // all mips, SHADER_READ_ONLY
    std::vector<VkImageView> mipViews;            // per-mip views for storage / attachment
    VkSampler      sampler    = VK_NULL_HANDLE;
    uint32_t       mipLevels  = 1;
    VkExtent2D     extent     = {};

    void Create(DeviceContext& dev, VkExtent2D ext);
    void Destroy(VkDevice device);
};

void HiZImage::Create(DeviceContext& dev, VkExtent2D ext)
{
    extent    = ext;
    mipLevels = (uint32_t)std::floor(std::log2((float)std::max(ext.width, ext.height))) + 1u;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.format        = VK_FORMAT_R32_SFLOAT;
    imgInfo.extent        = {ext.width, ext.height, 1};
    imgInfo.mipLevels     = mipLevels;
    imgInfo.arrayLayers   = 1;
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT  // mip 0: depth pre-pass output
                          | VK_IMAGE_USAGE_SAMPLED_BIT           // Hi-Z occlusion test
                          | VK_IMAGE_USAGE_STORAGE_BIT;          // Hi-Z build compute writes
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(dev.Device(), &imgInfo, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("HiZImage: vkCreateImage failed");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(dev.Device(), image, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = FindMemTypeOcc(dev.PhysicalDevice(), req.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(dev.Device(), &ai, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("HiZImage: vkAllocateMemory failed");
    vkBindImageMemory(dev.Device(), image, memory, 0);

    // Full view (all mips, for sampling)
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R32_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    if (vkCreateImageView(dev.Device(), &viewInfo, nullptr, &fullView) != VK_SUCCESS)
        throw std::runtime_error("HiZImage: vkCreateImageView (full) failed");

    // Per-mip views
    mipViews.resize(mipLevels);
    viewInfo.subresourceRange.levelCount = 1;
    for (uint32_t m = 0; m < mipLevels; ++m) {
        viewInfo.subresourceRange.baseMipLevel = m;
        if (vkCreateImageView(dev.Device(), &viewInfo, nullptr, &mipViews[m]) != VK_SUCCESS)
            throw std::runtime_error("HiZImage: vkCreateImageView (mip) failed");
    }

    // Sampler (nearest, clamp)
    VkSamplerCreateInfo si{};
    si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter    = VK_FILTER_NEAREST;
    si.minFilter    = VK_FILTER_NEAREST;
    si.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.minLod       = 0.f;
    si.maxLod       = (float)mipLevels;
    if (vkCreateSampler(dev.Device(), &si, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("HiZImage: vkCreateSampler failed");
}

void HiZImage::Destroy(VkDevice device)
{
    if (sampler  != VK_NULL_HANDLE) { vkDestroySampler(device, sampler, nullptr);    sampler  = VK_NULL_HANDLE; }
    for (auto& v : mipViews) if (v) vkDestroyImageView(device, v, nullptr);
    mipViews.clear();
    if (fullView != VK_NULL_HANDLE) { vkDestroyImageView(device, fullView, nullptr); fullView = VK_NULL_HANDLE; }
    if (image    != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr);        image    = VK_NULL_HANDLE; }
    if (memory   != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr);         memory   = VK_NULL_HANDLE; }
}

// ── Depth pre-pass resources ─────────────────────────────────────────────────
// Renders to:
//   Attachment 0 (color R32_SFLOAT): hiz.mipViews[0]  — writes gl_FragCoord.z
//   Attachment 1 (depth D32_SFLOAT): a private depth image — for correct occlusion ordering
struct DepthPrePassResources {
    // Private depth image (rasterization only; not sampled afterward)
    VkImage        depthImage  = VK_NULL_HANDLE;
    VkDeviceMemory depthMem    = VK_NULL_HANDLE;
    VkImageView    depthView   = VK_NULL_HANDLE;

    VkRenderPass   renderPass  = VK_NULL_HANDLE;
    VkFramebuffer  framebuffer = VK_NULL_HANDLE;
    VkExtent2D     extent      = {};

    VkPipelineLayout plLayout  = VK_NULL_HANDLE;
    VkPipeline       pipeline  = VK_NULL_HANDLE;

    void Create(DeviceContext& dev, VkExtent2D ext,
                VkDescriptorSetLayout frameLayout,
                VkImageView hizMip0View,
                const std::string& vertSpv,
                const std::string& fragSpv);
    void Destroy(VkDevice device);
};

void DepthPrePassResources::Create(DeviceContext& dev, VkExtent2D ext,
                                    VkDescriptorSetLayout frameLayout,
                                    VkImageView hizMip0View,
                                    const std::string& vertSpv,
                                    const std::string& fragSpv)
{
    extent       = ext;
    VkDevice dev_ = dev.Device();

    // Private D32_SFLOAT depth image
    {
        VkImageCreateInfo ii{};
        ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType     = VK_IMAGE_TYPE_2D;
        ii.format        = VK_FORMAT_D32_SFLOAT;
        ii.extent        = {ext.width, ext.height, 1};
        ii.mipLevels     = 1; ii.arrayLayers = 1;
        ii.samples       = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ii.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (vkCreateImage(dev_, &ii, nullptr, &depthImage) != VK_SUCCESS)
            throw std::runtime_error("DepthPrePass: vkCreateImage failed");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(dev_, depthImage, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = FindMemTypeOcc(dev.PhysicalDevice(), req.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(dev_, &ai, nullptr, &depthMem) != VK_SUCCESS)
            throw std::runtime_error("DepthPrePass: vkAllocateMemory failed");
        vkBindImageMemory(dev_, depthImage, depthMem, 0);

        VkImageViewCreateInfo vi{};
        vi.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image                           = depthImage;
        vi.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vi.format                          = VK_FORMAT_D32_SFLOAT;
        vi.subresourceRange                = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(dev_, &vi, nullptr, &depthView) != VK_SUCCESS)
            throw std::runtime_error("DepthPrePass: vkCreateImageView failed");
    }

    // Render pass: attachment 0 = R32_SFLOAT color, attachment 1 = D32_SFLOAT depth
    {
        std::array<VkAttachmentDescription, 2> atts{};
        atts[0].format         = VK_FORMAT_R32_SFLOAT;
        atts[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        atts[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        atts[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        atts[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        atts[0].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        atts[1].format         = VK_FORMAT_D32_SFLOAT;
        atts[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        atts[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        atts[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        atts[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

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
        dep.srcAccessMask = 0;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                          | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 2;
        rpInfo.pAttachments    = atts.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dep;
        if (vkCreateRenderPass(dev_, &rpInfo, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("DepthPrePass: vkCreateRenderPass failed");
    }

    // Framebuffer: hiz mip 0 (color) + private depth
    {
        std::array<VkImageView, 2> fbViews = {hizMip0View, depthView};
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments    = fbViews.data();
        fbInfo.width           = ext.width;
        fbInfo.height          = ext.height;
        fbInfo.layers          = 1;
        if (vkCreateFramebuffer(dev_, &fbInfo, nullptr, &framebuffer) != VK_SUCCESS)
            throw std::runtime_error("DepthPrePass: vkCreateFramebuffer failed");
    }

    // Pipeline layout (reuse the frame UBO descriptor set layout)
    {
        VkPipelineLayoutCreateInfo plInfo{};
        plInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plInfo.setLayoutCount = 1;
        plInfo.pSetLayouts    = &frameLayout;
        if (vkCreatePipelineLayout(dev_, &plInfo, nullptr, &plLayout) != VK_SUCCESS)
            throw std::runtime_error("DepthPrePass: vkCreatePipelineLayout failed");
    }

    // Pipeline (vert + frag)
    {
        auto vertCode = LoadSpirVOcc(vertSpv);
        auto fragCode = LoadSpirVOcc(fragSpv);

        auto makeSM = [&](const std::vector<char>& code) -> VkShaderModule {
            VkShaderModuleCreateInfo si{};
            si.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            si.codeSize = code.size();
            si.pCode    = reinterpret_cast<const uint32_t*>(code.data());
            VkShaderModule sm;
            if (vkCreateShaderModule(dev_, &si, nullptr, &sm) != VK_SUCCESS)
                throw std::runtime_error("DepthPrePass: vkCreateShaderModule failed");
            return sm;
        };

        VkShaderModule vertSM = makeSM(vertCode);
        VkShaderModule fragSM = makeSM(fragCode);

        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                     VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr};
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                     VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr};

        // Vertex input: position (binding 0), instModel mat4 (binding 1)
        // Stride = sizeof(MeshVertex) = 36 bytes (position + normal + color)
        VkVertexInputBindingDescription vbinds[2]{};
        vbinds[0] = {0, 9 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
        vbinds[1] = {1, 16 * sizeof(float), VK_VERTEX_INPUT_RATE_INSTANCE};

        std::array<VkVertexInputAttributeDescription, 5> attrs{};
        attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};          // position
        attrs[1] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0};       // instModel col 0
        attrs[2] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 16};      // instModel col 1
        attrs[3] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 32};      // instModel col 2
        attrs[4] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 48};      // instModel col 3

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount   = 2;
        vi.pVertexBindingDescriptions      = vbinds;
        vi.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
        vi.pVertexAttributeDescriptions    = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport vp{0.f, 0.f, (float)ext.width, (float)ext.height, 0.f, 1.f};
        VkRect2D   sc{{0, 0}, ext};
        VkPipelineViewportStateCreateInfo vs{};
        vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1; vs.pViewports = &vp;
        vs.scissorCount  = 1; vs.pScissors  = &sc;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode    = VK_CULL_MODE_BACK_BIT;
        rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth   = 1.f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable  = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp   = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState ba{};
        ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments    = &ba;

        VkGraphicsPipelineCreateInfo gpInfo{};
        gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpInfo.stageCount          = 2;
        gpInfo.pStages             = stages.data();
        gpInfo.pVertexInputState   = &vi;
        gpInfo.pInputAssemblyState = &ia;
        gpInfo.pViewportState      = &vs;
        gpInfo.pRasterizationState = &rs;
        gpInfo.pMultisampleState   = &ms;
        gpInfo.pDepthStencilState  = &ds;
        gpInfo.pColorBlendState    = &cb;
        gpInfo.layout              = plLayout;
        gpInfo.renderPass          = renderPass;

        VkResult r = vkCreateGraphicsPipelines(dev_, VK_NULL_HANDLE, 1, &gpInfo, nullptr, &pipeline);
        vkDestroyShaderModule(dev_, vertSM, nullptr);
        vkDestroyShaderModule(dev_, fragSM, nullptr);
        if (r != VK_SUCCESS)
            throw std::runtime_error("DepthPrePass: vkCreateGraphicsPipelines failed");
    }
}

void DepthPrePassResources::Destroy(VkDevice device)
{
    if (pipeline   != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr);         pipeline   = VK_NULL_HANDLE; }
    if (plLayout   != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, plLayout, nullptr);   plLayout   = VK_NULL_HANDLE; }
    if (framebuffer!= VK_NULL_HANDLE) { vkDestroyFramebuffer(device, framebuffer, nullptr);   framebuffer= VK_NULL_HANDLE; }
    if (renderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(device, renderPass, nullptr);     renderPass = VK_NULL_HANDLE; }
    if (depthView  != VK_NULL_HANDLE) { vkDestroyImageView(device, depthView, nullptr);       depthView  = VK_NULL_HANDLE; }
    if (depthImage != VK_NULL_HANDLE) { vkDestroyImage(device, depthImage, nullptr);          depthImage = VK_NULL_HANDLE; }
    if (depthMem   != VK_NULL_HANDLE) { vkFreeMemory(device, depthMem, nullptr);              depthMem   = VK_NULL_HANDLE; }
}

// ── Hi-Z build pass resources ────────────────────────────────────────────────
struct HiZBuildResources {
    VkDescriptorSetLayout dsLayout = VK_NULL_HANDLE;
    VkDescriptorPool      dsPool   = VK_NULL_HANDLE;
    VkPipelineLayout      plLayout = VK_NULL_HANDLE;
    VkPipeline            pipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> mipSets;

    void Create(DeviceContext& dev, const HiZImage& hiz, const std::string& compSpv);
    void Destroy(VkDevice device);
};

void HiZBuildResources::Create(DeviceContext& dev, const HiZImage& hiz,
                                 const std::string& compSpv)
{
    VkDevice dev_ = dev.Device();
    uint32_t mips = hiz.mipLevels;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo dsInfo{};
    dsInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsInfo.bindingCount = 2;
    dsInfo.pBindings    = bindings.data();
    if (vkCreateDescriptorSetLayout(dev_, &dsInfo, nullptr, &dsLayout) != VK_SUCCESS)
        throw std::runtime_error("HiZBuild: vkCreateDescriptorSetLayout failed");

    uint32_t sets = std::max(1u, mips - 1u);
    std::array<VkDescriptorPoolSize, 2> pSizes{};
    pSizes[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets};
    pSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          sets};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = sets;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = pSizes.data();
    if (vkCreateDescriptorPool(dev_, &poolInfo, nullptr, &dsPool) != VK_SUCCESS)
        throw std::runtime_error("HiZBuild: vkCreateDescriptorPool failed");

    std::vector<VkDescriptorSetLayout> layouts(sets, dsLayout);
    mipSets.resize(sets);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = dsPool;
    ai.descriptorSetCount = sets;
    ai.pSetLayouts        = layouts.data();
    if (vkAllocateDescriptorSets(dev_, &ai, mipSets.data()) != VK_SUCCESS)
        throw std::runtime_error("HiZBuild: vkAllocateDescriptorSets failed");

    for (uint32_t m = 0; m < sets; ++m) {
        VkDescriptorImageInfo srcInfo{hiz.sampler, hiz.mipViews[m],     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo dstInfo{VK_NULL_HANDLE, hiz.mipViews[m+1], VK_IMAGE_LAYOUT_GENERAL};

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mipSets[m], 0, 0, 1,
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcInfo, nullptr, nullptr};
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, mipSets[m], 1, 0, 1,
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstInfo, nullptr, nullptr};
        vkUpdateDescriptorSets(dev_, 2, writes.data(), 0, nullptr);
    }

    VkPushConstantRange pcRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::uvec2)};
    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount         = 1;
    plInfo.pSetLayouts            = &dsLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges    = &pcRange;
    if (vkCreatePipelineLayout(dev_, &plInfo, nullptr, &plLayout) != VK_SUCCESS)
        throw std::runtime_error("HiZBuild: vkCreatePipelineLayout failed");

    auto code = LoadSpirVOcc(compSpv);
    VkShaderModuleCreateInfo smInfo{};
    smInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smInfo.codeSize = code.size();
    smInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule sm;
    if (vkCreateShaderModule(dev_, &smInfo, nullptr, &sm) != VK_SUCCESS)
        throw std::runtime_error("HiZBuild: vkCreateShaderModule failed");

    VkComputePipelineCreateInfo cpInfo{};
    cpInfo.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpInfo.stage        = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                           nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, sm, "main", nullptr};
    cpInfo.layout       = plLayout;
    VkResult r = vkCreateComputePipelines(dev_, VK_NULL_HANDLE, 1, &cpInfo, nullptr, &pipeline);
    vkDestroyShaderModule(dev_, sm, nullptr);
    if (r != VK_SUCCESS)
        throw std::runtime_error("HiZBuild: vkCreateComputePipelines failed");
}

void HiZBuildResources::Destroy(VkDevice device)
{
    if (pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr);       pipeline = VK_NULL_HANDLE; }
    if (plLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, plLayout, nullptr); plLayout = VK_NULL_HANDLE; }
    if (dsPool   != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, dsPool, nullptr);   dsPool   = VK_NULL_HANDLE; }
    if (dsLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, dsLayout, nullptr); dsLayout = VK_NULL_HANDLE; }
    mipSets.clear();
}

// ── Occlusion cull compute resources ────────────────────────────────────────
struct OccCullResources {
    VkDescriptorSetLayout dsLayout = VK_NULL_HANDLE;
    VkDescriptorPool      dsPool   = VK_NULL_HANDLE;
    VkDescriptorSet       dsSet    = VK_NULL_HANDLE;
    VkPipelineLayout      plLayout = VK_NULL_HANDLE;
    VkPipeline            pipeline = VK_NULL_HANDLE;
    GpuBuffer             objectBuffer;
    GpuBuffer             drawCmdBuffer;
    uint32_t              maxObjects = 0;

    void Create(DeviceContext& dev, uint32_t maxObj,
                const HiZImage& hiz, const std::string& compSpv);
    void Destroy(VkDevice device);
};

struct OccPush {
    glm::mat4 viewProj;
    glm::mat4 proj;
    uint32_t  objectCount;
    uint32_t  hizMipCount;
    uint32_t  hizWidth;
    uint32_t  hizHeight;
};
static_assert(sizeof(OccPush) <= 256, "OccPush too large for push constants");

void OccCullResources::Create(DeviceContext& dev, uint32_t maxObj,
                                const HiZImage& hiz,
                                const std::string& compSpv)
{
    maxObjects   = maxObj;
    VkDevice dev_ = dev.Device();

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo dsInfo{};
    dsInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsInfo.bindingCount = 3;
    dsInfo.pBindings    = bindings.data();
    if (vkCreateDescriptorSetLayout(dev_, &dsInfo, nullptr, &dsLayout) != VK_SUCCESS)
        throw std::runtime_error("OccCull: vkCreateDescriptorSetLayout failed");

    std::array<VkDescriptorPoolSize, 2> pSizes{};
    pSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         2};
    pSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = pSizes.data();
    if (vkCreateDescriptorPool(dev_, &poolInfo, nullptr, &dsPool) != VK_SUCCESS)
        throw std::runtime_error("OccCull: vkCreateDescriptorPool failed");

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = dsPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &dsLayout;
    if (vkAllocateDescriptorSets(dev_, &ai, &dsSet) != VK_SUCCESS)
        throw std::runtime_error("OccCull: vkAllocateDescriptorSets failed");

    VkDeviceSize objBytes = sizeof(CullableObject) * maxObj;
    VkDeviceSize cmdBytes = sizeof(VkDrawIndexedIndirectCommand) * maxObj;
    objectBuffer.Create(dev_, dev.PhysicalDevice(), objBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    drawCmdBuffer.Create(dev_, dev.PhysicalDevice(), cmdBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDescriptorBufferInfo objBuf{objectBuffer.Buffer(),  0, objBytes};
    VkDescriptorBufferInfo cmdBuf{drawCmdBuffer.Buffer(), 0, cmdBytes};
    VkDescriptorImageInfo  hizImg{hiz.sampler, hiz.fullView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    std::array<VkWriteDescriptorSet, 3> writes{};
    writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsSet, 0, 0, 1,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &objBuf, nullptr};
    writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsSet, 1, 0, 1,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &cmdBuf, nullptr};
    writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsSet, 2, 0, 1,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &hizImg, nullptr, nullptr};
    vkUpdateDescriptorSets(dev_, 3, writes.data(), 0, nullptr);

    VkPushConstantRange pcRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(OccPush)};
    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount         = 1;
    plInfo.pSetLayouts            = &dsLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges    = &pcRange;
    if (vkCreatePipelineLayout(dev_, &plInfo, nullptr, &plLayout) != VK_SUCCESS)
        throw std::runtime_error("OccCull: vkCreatePipelineLayout failed");

    auto code = LoadSpirVOcc(compSpv);
    VkShaderModuleCreateInfo smInfo{};
    smInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smInfo.codeSize = code.size();
    smInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule sm;
    if (vkCreateShaderModule(dev_, &smInfo, nullptr, &sm) != VK_SUCCESS)
        throw std::runtime_error("OccCull: vkCreateShaderModule failed");

    VkComputePipelineCreateInfo cpInfo{};
    cpInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpInfo.stage  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, sm, "main", nullptr};
    cpInfo.layout = plLayout;
    VkResult r = vkCreateComputePipelines(dev_, VK_NULL_HANDLE, 1, &cpInfo, nullptr, &pipeline);
    vkDestroyShaderModule(dev_, sm, nullptr);
    if (r != VK_SUCCESS)
        throw std::runtime_error("OccCull: vkCreateComputePipelines failed");
}

void OccCullResources::Destroy(VkDevice device)
{
    if (pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr);       pipeline = VK_NULL_HANDLE; }
    if (plLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, plLayout, nullptr); plLayout = VK_NULL_HANDLE; }
    if (dsPool   != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, dsPool, nullptr);   dsPool   = VK_NULL_HANDLE; }
    if (dsLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, dsLayout, nullptr); dsLayout = VK_NULL_HANDLE; }
    objectBuffer.Destroy(device);
    drawCmdBuffer.Destroy(device);
}

// ── OcclusionCullPass ────────────────────────────────────────────────────────
struct OcclusionCullPass::Impl {
    std::string shaderDir;
    uint32_t    maxObjects = 0;

    HiZImage             hiz;
    DepthPrePassResources depthPre;
    HiZBuildResources     hizBuild;
    OccCullResources      occCull;

    OccPush   push{};
    uint32_t  objectCount = 0;

    std::vector<DrawCall> drawCalls;
    VkDescriptorSetLayout frameLayout = VK_NULL_HANDLE;
};

OcclusionCullPass::OcclusionCullPass(std::string shaderDir)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->shaderDir = std::move(shaderDir);
}

OcclusionCullPass::~OcclusionCullPass() = default;

void OcclusionCullPass::Build(DeviceContext& dev, const BuildPassInfo& info)
{
    m_impl->maxObjects  = info.maxObjectCount;
    m_impl->frameLayout = info.frameLayout;

    m_impl->hiz.Create(dev, info.extent);
    m_impl->depthPre.Create(dev, info.extent, info.frameLayout,
                             m_impl->hiz.mipViews[0],
                             m_impl->shaderDir + "depth_prepass.vert.spv",
                             m_impl->shaderDir + "depth_prepass.frag.spv");
    m_impl->hizBuild.Create(dev, m_impl->hiz, m_impl->shaderDir + "hiz_build.comp.spv");
    m_impl->occCull.Create(dev, info.maxObjectCount, m_impl->hiz,
                            m_impl->shaderDir + "occlusion_cull.comp.spv");
}

void OcclusionCullPass::Rebuild(DeviceContext& dev, VkExtent2D newExtent, VkRenderPass)
{
    VkDevice device = dev.Device();
    m_impl->occCull.Destroy(device);
    m_impl->hizBuild.Destroy(device);
    m_impl->depthPre.Destroy(device);
    m_impl->hiz.Destroy(device);

    BuildPassInfo info{};
    info.extent         = newExtent;
    info.frameLayout    = m_impl->frameLayout;
    info.maxObjectCount = m_impl->maxObjects;
    Build(dev, info);
}

void OcclusionCullPass::SetObjects(DeviceContext& dev,
                                    const std::vector<CullableObject>& objects,
                                    const glm::mat4& viewProj,
                                    const glm::mat4& proj)
{
    if (objects.empty()) return;
    m_impl->objectCount = (uint32_t)objects.size();
    m_impl->occCull.objectBuffer.UploadViaStaging(
        dev, objects.data(), sizeof(CullableObject) * objects.size());

    m_impl->push.viewProj    = viewProj;
    m_impl->push.proj        = proj;
    m_impl->push.objectCount = m_impl->objectCount;
    m_impl->push.hizMipCount = m_impl->hiz.mipLevels;
    m_impl->push.hizWidth    = m_impl->hiz.extent.width;
    m_impl->push.hizHeight   = m_impl->hiz.extent.height;
}

void OcclusionCullPass::SetDrawCalls(std::span<const DrawCall> drawCalls)
{
    m_impl->drawCalls.assign(drawCalls.begin(), drawCalls.end());
}

void OcclusionCullPass::Record(VkCommandBuffer cmd, PassContext& ctx)
{
    if (m_impl->drawCalls.empty() || m_impl->objectCount == 0) return;

    // ── 1. Depth pre-pass: render to Hi-Z mip 0 (R32_SFLOAT color) ─────────
    {
        // Transition Hi-Z mip 0 from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
        VkImageMemoryBarrier b{};
        b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image               = m_impl->hiz.image;
        b.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.srcAccessMask       = 0;
        b.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &b);
    }

    std::array<VkClearValue, 2> clears{};
    clears[0].color.float32[0] = 1.0f; // clear color depth to "far"
    clears[1].depthStencil     = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_impl->depthPre.renderPass;
    rpInfo.framebuffer       = m_impl->depthPre.framebuffer;
    rpInfo.renderArea.extent = m_impl->depthPre.extent;
    rpInfo.clearValueCount   = 2;
    rpInfo.pClearValues      = clears.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_impl->depthPre.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_impl->depthPre.plLayout, 0, 1,
                            &ctx.frameDescSet, 0, nullptr);

    for (const auto& dc : m_impl->drawCalls) {
        VkBuffer     bufs[2] = {dc.vertexBuffer->Buffer(), dc.instanceBuffer->Buffer()};
        VkDeviceSize offs[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
        vkCmdBindIndexBuffer(cmd, dc.indexBuffer->Buffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, dc.indexCount, dc.instanceCount, 0, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
    // Hi-Z mip 0 is now SHADER_READ_ONLY_OPTIMAL (render pass finalLayout)

    // ── 2. Hi-Z build: build mip chain from mip 0 ───────────────────────────
    // Transition all other mips to GENERAL for storage writes
    {
        VkImageMemoryBarrier b{};
        b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image               = m_impl->hiz.image;
        b.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 1, m_impl->hiz.mipLevels - 1, 0, 1};
        b.srcAccessMask       = 0;
        b.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        if (m_impl->hiz.mipLevels > 1)
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &b);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_impl->hizBuild.pipeline);
    uint32_t mipW = m_impl->hiz.extent.width;
    uint32_t mipH = m_impl->hiz.extent.height;

    for (uint32_t m = 0; m + 1 < m_impl->hiz.mipLevels; ++m) {
        // Barrier: mip m (SHADER_READ_ONLY) → readable by compute sampler
        // Mip 0 is already SHADER_READ_ONLY from the render pass finalLayout.
        // Mips 1..N-1 transition from GENERAL to SHADER_READ_ONLY after being written.
        if (m > 0) {
            VkImageMemoryBarrier rb{};
            rb.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            rb.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
            rb.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rb.image               = m_impl->hiz.image;
            rb.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, m, 1, 0, 1};
            rb.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
            rb.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &rb);
        }

        glm::uvec2 srcSize{mipW, mipH};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_impl->hizBuild.plLayout, 0, 1,
                                &m_impl->hizBuild.mipSets[m], 0, nullptr);
        vkCmdPushConstants(cmd, m_impl->hizBuild.plLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(glm::uvec2), &srcSize);

        uint32_t dstW = std::max(1u, mipW / 2u);
        uint32_t dstH = std::max(1u, mipH / 2u);
        vkCmdDispatch(cmd, (dstW + 7) / 8, (dstH + 7) / 8, 1);
        mipW = dstW;
        mipH = dstH;
    }

    // Transition all mips to SHADER_READ_ONLY for occlusion test sampling
    {
        VkImageMemoryBarrier fb{};
        fb.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        fb.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
        fb.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        fb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        fb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        fb.image               = m_impl->hiz.image;
        fb.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT,
                                  m_impl->hiz.mipLevels > 1 ? m_impl->hiz.mipLevels - 1 : 0,
                                  1, 0, 1};
        fb.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        fb.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        if (m_impl->hiz.mipLevels > 1)
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &fb);
    }

    // ── 3. Occlusion cull dispatch ──────────────────────────────────────────
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_impl->occCull.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_impl->occCull.plLayout, 0, 1,
                            &m_impl->occCull.dsSet, 0, nullptr);
    vkCmdPushConstants(cmd, m_impl->occCull.plLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(OccPush), &m_impl->push);

    uint32_t groups = (m_impl->objectCount + 63u) / 64u;
    vkCmdDispatch(cmd, groups, 1, 1);

    VkBufferMemoryBarrier indB{};
    indB.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    indB.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    indB.dstAccessMask       = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    indB.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    indB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    indB.buffer              = m_impl->occCull.drawCmdBuffer.Buffer();
    indB.offset              = 0;
    indB.size                = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0, 0, nullptr, 1, &indB, 0, nullptr);

    ctx.indirectDrawBuffer = m_impl->occCull.drawCmdBuffer.Buffer();
    ctx.maxDrawCount       = m_impl->objectCount;
}

void OcclusionCullPass::Destroy(VkDevice device)
{
    m_impl->occCull.Destroy(device);
    m_impl->hizBuild.Destroy(device);
    m_impl->depthPre.Destroy(device);
    m_impl->hiz.Destroy(device);
}

VkBuffer OcclusionCullPass::IndirectDrawBuffer() const
{
    return m_impl->occCull.drawCmdBuffer.Buffer();
}

VkBuffer OcclusionCullPass::DrawCountBuffer() const
{
    return VK_NULL_HANDLE;
}

} // namespace xcel
