#pragma once
#include <glm/glm.hpp>
#include <cstdint>

namespace xcel {

class GpuBuffer;

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
};

} // namespace xcel
