#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/mat4x4.hpp>

namespace xcel::io {

struct BoneData
{
    std::string name;
    int32_t     parentIndex = -1;   // -1 = root
    glm::mat4   bindMatrix{1.f};    // bone-to-mesh-space inverse bind
};

struct SkeletonData
{
    std::string           name;
    std::vector<BoneData> bones;
};

} // namespace xcel::io
