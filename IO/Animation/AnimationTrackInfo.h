#pragma once
#include <cstdint>
#include <string>

namespace xcel::io {

struct AnimationTrackInfo
{
    uint32_t    id              = 0;
    std::string name;
    uint32_t    meshId          = 0;   // which mesh this track drives
    uint32_t    frameCount      = 0;
    float       timeStepSeconds = 0.f;

    float Duration() const { return static_cast<float>(frameCount) * timeStepSeconds; }
};

} // namespace xcel::io
