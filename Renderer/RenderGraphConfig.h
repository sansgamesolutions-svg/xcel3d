#pragma once
#include "Renderer/RenderOptions.h"
#include <vulkan/vulkan.h>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace xcel {

struct PipelineDescriptor
{
    std::string     name;
    std::string     vertShader  = "mesh.vert.spv";
    std::string     fragShader  = "mesh.frag.spv";
    BlendMode       blendMode   = BlendMode::Opaque;
    RenderLayer     renderLayer = RenderLayer::Opaque;
    bool            depthTest   = true;
    bool            depthWrite  = true;
    VkBlendFactor   srcColor    = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor   dstColor    = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendFactor   srcAlpha    = VK_BLEND_FACTOR_ONE;
    VkBlendFactor   dstAlpha    = VK_BLEND_FACTOR_ZERO;
    VkCullModeFlags cullMode    = VK_CULL_MODE_BACK_BIT;
};

struct ForwardPassConfig
{
    std::array<float, 4>            clearColor = {0.15f, 0.15f, 0.15f, 1.0f};
    std::vector<PipelineDescriptor> pipelines;
};

enum class PassType { FrustumCull, ForwardLit, OIT, Manipulator };

struct PassEntry
{
    PassType                         type;
    bool                             enabled       = true;
    std::optional<ForwardPassConfig> forwardConfig; // set when type == ForwardLit
};

struct RenderGraphConfig
{
    std::vector<PassEntry> passes;

    // Load from a JSON file; throws std::runtime_error on parse failure.
    static RenderGraphConfig FromJson(const std::filesystem::path& path);

    // Returns config matching the current hardcoded behavior (safe fallback).
    static RenderGraphConfig Default();
};

} // namespace xcel
