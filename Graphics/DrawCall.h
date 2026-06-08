#pragma once
#include <glm/glm.hpp>
#include <cstdint>

namespace xcel {

class GpuBuffer;

// Per-draw-call material delivered as a push constant (16 bytes, fragment stage).
struct MaterialData {
    float ambientFactor  = 0.15f;
    float diffuseFactor  = 1.0f;
    float specularFactor = 0.4f;
    float shininess      = 32.0f;
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
};

} // namespace xcel
