#pragma once
#include "IO/Animation/AnimationFrame.h"
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

namespace xcel::io {

// LRU cache for decoded animation frames, keyed by (trackId, frameIndex).
class FrameCache
{
public:
    explicit FrameCache(size_t capacity = 32) : m_capacity(capacity) {}

    void Put(uint32_t trackId, uint32_t frameIndex, AnimationFrame frame);

    // Returns nullptr on cache miss.
    const AnimationFrame* Get(uint32_t trackId, uint32_t frameIndex);

    void Clear();

private:
    struct Key
    {
        uint32_t trackId;
        uint32_t frameIndex;
        bool operator==(const Key& o) const noexcept
        {
            return trackId == o.trackId && frameIndex == o.frameIndex;
        }
    };
    struct KeyHash
    {
        size_t operator()(const Key& k) const noexcept
        {
            return std::hash<uint64_t>{}((uint64_t)k.trackId << 32 | k.frameIndex);
        }
    };

    using List    = std::list<std::pair<Key, AnimationFrame>>;
    using ListIt  = List::iterator;

    size_t                                         m_capacity;
    List                                           m_list;   // MRU front, LRU back
    std::unordered_map<Key, ListIt, KeyHash>       m_map;
    std::mutex                                     m_mutex;
};

} // namespace xcel::io
