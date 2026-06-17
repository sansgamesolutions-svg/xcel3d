#pragma once
#include "Renderer/HardwareCaps.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <type_traits>

namespace xcel {

// ── Enumerations ──────────────────────────────────────────────────────────────

enum class BlendMode : uint8_t
{
    Opaque,        // no blending; depth write on
    AlphaBlend,    // SrcAlpha / OneMinusSrcAlpha; depth write off
    Additive,      // One / One; depth write off
    Premultiplied, // One / OneMinusSrcAlpha; depth write off
};

enum class RenderLayer : uint8_t
{
    Opaque      = 0, // drawn first, depth write on
    Transparent = 1, // drawn after opaque, sorted back-to-front, depth write off
    Overlay     = 2, // drawn last, reserved for future use
};

enum class BatchingStrategy : uint8_t
{
    ByPrimitiveType,         // one page family per PrimitiveType (current behaviour)
    ByPrimitiveTypeAndBlend, // separate pages per (PrimitiveType, BlendMode) pair
};

// ── Per-mesh ECS component ────────────────────────────────────────────────────

// Optional; absent entities render as Opaque/Opaque with depth test and write on.
struct MeshRenderOptions
{
    BlendMode   blendMode        = BlendMode::Opaque;
    RenderLayer renderLayer      = RenderLayer::Opaque;
    bool        depthTestEnable  = true;
    bool        depthWriteEnable = true;
};

static_assert(std::is_trivially_copyable_v<MeshRenderOptions>);

// ── Global application-level options ─────────────────────────────────────────

// Replaces PassOptions. Held in WindowContext.
struct GlobalRenderOptions
{
    // Culling (migrated from PassOptions)
    bool frustumCulling   = false;
    bool occlusionCulling = false;

    // Batching strategy injected into BatchingSystem before BuildAll().
    BatchingStrategy batchingStrategy = BatchingStrategy::ByPrimitiveType;

    // Hardware restriction overrides.
    // EffectiveCaps = HardwareCaps ∩ these limits.
    // maxMsaaSamples = VK_SAMPLE_COUNT_1_BIT disables MSAA even if the GPU supports it.
    // Full MSAA render-pass wiring is deferred; this field is reserved for future use.
    VkSampleCountFlagBits maxMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
    bool                  allowAnisotropy = true;
};

// ── Effective resolved caps ───────────────────────────────────────────────────

// The intersection of what the GPU supports and what the app permits.
struct EffectiveCaps
{
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    bool                  anisotropy  = false;
};

[[nodiscard]] EffectiveCaps ComputeEffectiveCaps(
    const HardwareCaps&        hw,
    const GlobalRenderOptions& opts) noexcept;

} // namespace xcel
