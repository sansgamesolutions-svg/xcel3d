#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/mat4x4.hpp>

namespace xcel::io {

struct SceneNode
{
    std::string      name;
    glm::mat4        localTransform{1.f};
    uint32_t         meshId    = UINT32_MAX;    // UINT32_MAX = no mesh
    uint32_t         skelId    = UINT32_MAX;    // UINT32_MAX = no skeleton
    std::vector<SceneNode> children;
};

} // namespace xcel::io
