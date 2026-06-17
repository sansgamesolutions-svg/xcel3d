#pragma once
#include "Renderer/RenderOptions.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace xcel {

class GpuBuffer;

// Per-draw-call material delivered as a push constant (fragment stage).
// textureIndex = 0xFFFFFFFF means no texture; shader falls back to per-vertex color.
struct MaterialData {
    float    ambientFactor  = 0.15f;
    float    diffuseFactor  = 1.0f;
    float    specularFactor = 0.4f;
    float    shininess      = 32.0f;
    float    alpha          = 1.0f;
    uint32_t textureIndex   = 0xFFFFFFFFu;
};

struct DrawCall
{
    const GpuBuffer* vertexBuffer;
    const GpuBuffer* indexBuffer;
    uint32_t         indexCount;
    const GpuBuffer* instanceBuffer = nullptr;
    uint32_t         instanceCount  = 1;
    // World-space AABB used for GPU frustum/occlusion culling.
    // Zero (default) means "always draw" when culling is active.
    glm::vec3        aabbMin        = glm::vec3(0.f);
    glm::vec3        aabbMax        = glm::vec3(0.f);
    MaterialData     material{};
    BlendMode        blendMode   = BlendMode::Opaque;
    RenderLayer      renderLayer = RenderLayer::Opaque;
};

} // namespace xcel
