#pragma once
#include "IO/Animation/AnimationTrackInfo.h"
#include <string_view>
#include <vector>

namespace xcel::io {

class AnimationCatalogue
{
public:
    void                     AddTrack(AnimationTrackInfo info);

    size_t                   TrackCount()                          const;
    const AnimationTrackInfo& Track(size_t index)                 const;
    const AnimationTrackInfo* FindByName(std::string_view name)   const;
    const AnimationTrackInfo* FindByMeshId(uint32_t meshId)       const;

private:
    std::vector<AnimationTrackInfo> m_tracks;
};

} // namespace xcel::io
