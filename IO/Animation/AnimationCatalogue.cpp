#include "IO/Animation/AnimationCatalogue.h"
#include <algorithm>
#include <stdexcept>

namespace xcel::io {

void AnimationCatalogue::AddTrack(AnimationTrackInfo info)
{
    m_tracks.push_back(std::move(info));
}

size_t AnimationCatalogue::TrackCount() const { return m_tracks.size(); }

const AnimationTrackInfo& AnimationCatalogue::Track(size_t index) const
{
    return m_tracks.at(index);
}

const AnimationTrackInfo* AnimationCatalogue::FindByName(std::string_view name) const
{
    auto it = std::ranges::find_if(m_tracks, [name](const AnimationTrackInfo& t) {
        return t.name == name;
    });
    return (it != m_tracks.end()) ? &*it : nullptr;
}

const AnimationTrackInfo* AnimationCatalogue::FindByMeshId(uint32_t meshId) const
{
    auto it = std::ranges::find_if(m_tracks, [meshId](const AnimationTrackInfo& t) {
        return t.meshId == meshId;
    });
    return (it != m_tracks.end()) ? &*it : nullptr;
}

} // namespace xcel::io
