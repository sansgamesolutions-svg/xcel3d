#pragma warning(push, 0)
#include <boost/json.hpp>
#pragma warning(pop)
#include "Renderer/RenderGraphConfig.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace xcel {

namespace {

[[nodiscard]] BlendMode BlendModeFromString(std::string_view s)
{
    if (s == "opaque")               return BlendMode::Opaque;
    if (s == "alpha_blend")          return BlendMode::AlphaBlend;
    if (s == "additive")             return BlendMode::Additive;
    if (s == "premultiplied")        return BlendMode::Premultiplied;
    if (s == "weighted_blended_oit") return BlendMode::WeightedBlendedOIT;
    throw std::runtime_error("Unknown blend_mode: " + std::string(s));
}

[[nodiscard]] RenderLayer RenderLayerFromString(std::string_view s)
{
    if (s == "opaque")      return RenderLayer::Opaque;
    if (s == "transparent") return RenderLayer::Transparent;
    if (s == "overlay")     return RenderLayer::Overlay;
    throw std::runtime_error("Unknown render_layer: " + std::string(s));
}

[[nodiscard]] VkBlendFactor VkBlendFactorFromString(std::string_view s)
{
    if (s == "zero")                return VK_BLEND_FACTOR_ZERO;
    if (s == "one")                 return VK_BLEND_FACTOR_ONE;
    if (s == "src_color")           return VK_BLEND_FACTOR_SRC_COLOR;
    if (s == "one_minus_src_color") return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    if (s == "dst_color")           return VK_BLEND_FACTOR_DST_COLOR;
    if (s == "one_minus_dst_color") return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    if (s == "src_alpha")           return VK_BLEND_FACTOR_SRC_ALPHA;
    if (s == "one_minus_src_alpha") return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    if (s == "dst_alpha")           return VK_BLEND_FACTOR_DST_ALPHA;
    if (s == "one_minus_dst_alpha") return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    throw std::runtime_error("Unknown blend factor: " + std::string(s));
}

[[nodiscard]] VkCullModeFlags VkCullModeFromString(std::string_view s)
{
    if (s == "none")           return VK_CULL_MODE_NONE;
    if (s == "front")          return VK_CULL_MODE_FRONT_BIT;
    if (s == "back")           return VK_CULL_MODE_BACK_BIT;
    if (s == "front_and_back") return VK_CULL_MODE_FRONT_AND_BACK;
    throw std::runtime_error("Unknown cull_mode: " + std::string(s));
}

[[nodiscard]] PassType PassTypeFromString(std::string_view s)
{
    if (s == "frustum_cull") return PassType::FrustumCull;
    if (s == "forward_lit")  return PassType::ForwardLit;
    if (s == "oit")          return PassType::OIT;
    if (s == "manipulator")  return PassType::Manipulator;
    throw std::runtime_error("Unknown pass type: " + std::string(s));
}

[[nodiscard]] PipelineDescriptor ParsePipelineDescriptor(const boost::json::object& obj)
{
    PipelineDescriptor desc;
    if (const auto* v = obj.if_contains("name"))         desc.name       = v->as_string().c_str();
    if (const auto* v = obj.if_contains("vert"))         desc.vertShader = v->as_string().c_str();
    if (const auto* v = obj.if_contains("frag"))         desc.fragShader = v->as_string().c_str();
    if (const auto* v = obj.if_contains("blend_mode"))   desc.blendMode  = BlendModeFromString(v->as_string().c_str());
    if (const auto* v = obj.if_contains("render_layer")) desc.renderLayer = RenderLayerFromString(v->as_string().c_str());
    if (const auto* v = obj.if_contains("depth_test"))   desc.depthTest  = v->as_bool();
    if (const auto* v = obj.if_contains("depth_write"))  desc.depthWrite = v->as_bool();
    if (const auto* v = obj.if_contains("src_color"))    desc.srcColor   = VkBlendFactorFromString(v->as_string().c_str());
    if (const auto* v = obj.if_contains("dst_color"))    desc.dstColor   = VkBlendFactorFromString(v->as_string().c_str());
    if (const auto* v = obj.if_contains("src_alpha"))    desc.srcAlpha   = VkBlendFactorFromString(v->as_string().c_str());
    if (const auto* v = obj.if_contains("dst_alpha"))    desc.dstAlpha   = VkBlendFactorFromString(v->as_string().c_str());
    if (const auto* v = obj.if_contains("cull_mode"))    desc.cullMode   = VkCullModeFromString(v->as_string().c_str());
    return desc;
}

[[nodiscard]] ForwardPassConfig ParseForwardPassConfig(const boost::json::object& obj)
{
    ForwardPassConfig cfg;
    if (const auto* cc = obj.if_contains("clear_color"))
    {
        const auto& arr = cc->as_array();
        for (size_t i = 0; i < 4 && i < arr.size(); ++i)
            cfg.clearColor[i] = static_cast<float>(arr[i].as_double());
    }
    if (const auto* pipes = obj.if_contains("pipelines"))
    {
        for (const auto& pv : pipes->as_array())
            cfg.pipelines.push_back(ParsePipelineDescriptor(pv.as_object()));
    }
    return cfg;
}

} // anonymous namespace

RenderGraphConfig RenderGraphConfig::FromJson(const std::filesystem::path& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("render_graph.json: cannot open " + path.string());
    std::ostringstream ss;
    ss << f.rdbuf();

    const boost::json::value root = boost::json::parse(ss.str());
    const auto& obj        = root.as_object();
    const auto& passesArr  = obj.at("passes").as_array();

    RenderGraphConfig config;
    for (const auto& pv : passesArr)
    {
        const auto& po = pv.as_object();
        PassEntry entry;
        entry.type = PassTypeFromString(po.at("type").as_string().c_str());
        if (const auto* v = po.if_contains("enabled"))
            entry.enabled = v->as_bool();
        if (entry.type == PassType::ForwardLit)
            entry.forwardConfig = ParseForwardPassConfig(po);
        config.passes.push_back(std::move(entry));
    }
    return config;
}

RenderGraphConfig RenderGraphConfig::Default()
{
    PipelineDescriptor opaque;
    opaque.name        = "opaque";
    opaque.blendMode   = BlendMode::Opaque;
    opaque.renderLayer = RenderLayer::Opaque;
    opaque.depthTest   = true;
    opaque.depthWrite  = true;

    PipelineDescriptor alphaBlend;
    alphaBlend.name        = "alpha_blend";
    alphaBlend.blendMode   = BlendMode::AlphaBlend;
    alphaBlend.renderLayer = RenderLayer::Transparent;
    alphaBlend.depthTest   = true;
    alphaBlend.depthWrite  = false;
    alphaBlend.srcColor    = VK_BLEND_FACTOR_SRC_ALPHA;
    alphaBlend.dstColor    = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    alphaBlend.srcAlpha    = VK_BLEND_FACTOR_ONE;
    alphaBlend.dstAlpha    = VK_BLEND_FACTOR_ZERO;

    PipelineDescriptor additive;
    additive.name        = "additive";
    additive.blendMode   = BlendMode::Additive;
    additive.renderLayer = RenderLayer::Transparent;
    additive.depthTest   = true;
    additive.depthWrite  = false;
    additive.srcColor    = VK_BLEND_FACTOR_ONE;
    additive.dstColor    = VK_BLEND_FACTOR_ONE;
    additive.srcAlpha    = VK_BLEND_FACTOR_ONE;
    additive.dstAlpha    = VK_BLEND_FACTOR_ZERO;

    PipelineDescriptor premult;
    premult.name        = "premult";
    premult.blendMode   = BlendMode::Premultiplied;
    premult.renderLayer = RenderLayer::Transparent;
    premult.depthTest   = true;
    premult.depthWrite  = false;
    premult.srcColor    = VK_BLEND_FACTOR_ONE;
    premult.dstColor    = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    premult.srcAlpha    = VK_BLEND_FACTOR_ONE;
    premult.dstAlpha    = VK_BLEND_FACTOR_ZERO;

    ForwardPassConfig fwd;
    fwd.clearColor = {0.15f, 0.15f, 0.15f, 1.0f};
    fwd.pipelines  = {opaque, alphaBlend, additive, premult};

    PassEntry frustumEntry;
    frustumEntry.type    = PassType::FrustumCull;
    frustumEntry.enabled = false;

    PassEntry forwardEntry;
    forwardEntry.type          = PassType::ForwardLit;
    forwardEntry.enabled       = true;
    forwardEntry.forwardConfig = std::move(fwd);

    PassEntry oitEntry;
    oitEntry.type    = PassType::OIT;
    oitEntry.enabled = true;

    PassEntry manipEntry;
    manipEntry.type    = PassType::Manipulator;
    manipEntry.enabled = true;

    RenderGraphConfig cfg;
    cfg.passes = {frustumEntry, forwardEntry, oitEntry, manipEntry};
    return cfg;
}

} // namespace xcel
