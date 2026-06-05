#include "Common/Registry.h"
#include <cassert>
#include <stdexcept>
#include <unordered_map>

namespace xcel {

// â”€â”€ Impl â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct Registry::Impl {
    struct Slot {
        uint32_t generation : 12;
        uint32_t alive      :  1;
        uint32_t            : 19; // padding
        Slot() : generation(0u), alive(0u) {}
    };

    std::vector<Slot>                                          slots;
    std::vector<uint32_t>                                      freeList;
    std::unordered_map<size_t, std::unique_ptr<IComponentPool>> pools;
    size_t                                                     aliveCount = 0u;
};

// â”€â”€ Construction â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

Registry::Registry()  : m_impl(std::make_unique<Impl>()) {}
Registry::~Registry() = default;

// â”€â”€ Entity lifecycle â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

Entity Registry::Create()
{
    uint32_t index;
    uint32_t generation;

    if (!m_impl->freeList.empty()) {
        index = m_impl->freeList.back();
        m_impl->freeList.pop_back();
        // Increment generation so old handles issued for this slot are stale
        auto& slot = m_impl->slots[index];
        generation = ++slot.generation;
        // Generation 0 is reserved for the invalid sentinel; skip if wrapped
        if (generation == 0u) generation = ++slot.generation;
        slot.alive = 1u;
    } else {
        index = static_cast<uint32_t>(m_impl->slots.size());
        Impl::Slot slot{};
        slot.generation = 1u; // first use: generation 1 (0 = invalid)
        slot.alive      = 1u;
        generation = slot.generation;
        m_impl->slots.push_back(slot);
    }

    ++m_impl->aliveCount;
    return MakeEntity(index, generation);
}

void Registry::Destroy(Entity e)
{
    if (!IsAlive(e)) return;

    uint32_t idx = e.Index();

    // Remove this entity from every pool that contains it
    for (auto& [typeId, pool] : m_impl->pools)
        if (pool->Contains(idx)) pool->Remove(idx);

    m_impl->slots[idx].alive = 0u;
    m_impl->freeList.push_back(idx);
    --m_impl->aliveCount;
}

bool Registry::IsAlive(Entity e) const
{
    if (!e.IsValid()) return false;
    uint32_t idx = e.Index();
    if (idx >= m_impl->slots.size()) return false;
    const auto& slot = m_impl->slots[idx];
    return slot.alive && (slot.generation == e.Generation());
}

size_t Registry::AliveCount() const { return m_impl->aliveCount; }

Entity Registry::EntityFromIndex(uint32_t index) const
{
    if (index >= m_impl->slots.size()) return Entity{};
    const auto& slot = m_impl->slots[index];
    return slot.alive ? MakeEntity(index, slot.generation) : Entity{};
}

// â”€â”€ Non-template pool bridge â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

IComponentPool* Registry::GetOrCreatePool(
    size_t typeId,
    std::function<std::unique_ptr<IComponentPool>()> factory)
{
    auto it = m_impl->pools.find(typeId);
    if (it == m_impl->pools.end()) {
        auto [inserted, ok] = m_impl->pools.emplace(typeId, factory());
        return inserted->second.get();
    }
    return it->second.get();
}

IComponentPool* Registry::FindPool(size_t typeId) const
{
    auto it = m_impl->pools.find(typeId);
    return it != m_impl->pools.end() ? it->second.get() : nullptr;
}

} // namespace xcel
