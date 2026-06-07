#include "IO/Animation/FrameCache.h"

namespace xcel::io {

void FrameCache::Put(uint32_t trackId, uint32_t frameIndex, AnimationFrame frame)
{
    std::scoped_lock lock(m_mutex);
    Key key{trackId, frameIndex};

    auto it = m_map.find(key);
    if (it != m_map.end())
    {
        it->second->second = std::move(frame);
        m_list.splice(m_list.begin(), m_list, it->second);
        return;
    }

    if (m_list.size() == m_capacity)
    {
        m_map.erase(m_list.back().first);
        m_list.pop_back();
    }

    m_list.emplace_front(key, std::move(frame));
    m_map[key] = m_list.begin();
}

const AnimationFrame* FrameCache::Get(uint32_t trackId, uint32_t frameIndex)
{
    std::scoped_lock lock(m_mutex);
    auto it = m_map.find(Key{trackId, frameIndex});
    if (it == m_map.end())
        return nullptr;

    m_list.splice(m_list.begin(), m_list, it->second);
    return &it->second->second;
}

void FrameCache::Clear()
{
    std::scoped_lock lock(m_mutex);
    m_list.clear();
    m_map.clear();
}

} // namespace xcel::io
