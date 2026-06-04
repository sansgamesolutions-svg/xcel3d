#pragma once
#include "Common/Entity.h"
#include "Common/ComponentPool.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <tuple>
#include <typeinfo>

namespace xcel {

// ── Forward declarations ──────────────────────────────────────────────────────

class Registry;

// ── ViewRange<Ts...> ──────────────────────────────────────────────────────────
// Lazy input-range returned by Registry::View<Ts...>().
// Iterates entities that have ALL of Ts, yielding std::tuple<Entity, Ts&...>.
// Uses the smallest component pool as the primary iteration cursor; all others
// are checked via Contains() on each step.

template<typename... Ts>
struct ViewRange {
    static constexpr size_t kCount = sizeof...(Ts);

    struct Iterator {
        using value_type        = std::tuple<Entity, Ts&...>;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        Registry*                                      registry;
        std::tuple<ComponentPool<Ts>*...>*             pools;
        IComponentPool*                                primary;
        const std::array<IComponentPool*, kCount>*     allPools;
        size_t                                         pos;
        size_t                                         end;

        bool IsCurrentValid() const noexcept {
            uint32_t idx = primary->EntityIndexAt(pos);
            for (auto* p : *allPools)
                if (!p->Contains(idx)) return false;
            return true;
        }

        void Advance() noexcept {
            while (pos < end && !IsCurrentValid()) ++pos;
        }

        template<size_t... Is>
        value_type DerefImpl(uint32_t idx, Entity e,
                             std::index_sequence<Is...>) const noexcept {
            return value_type{e, *std::get<Is>(*pools)->TryGet(idx)...};
        }

        value_type operator*() const noexcept {
            uint32_t idx = primary->EntityIndexAt(pos);
            Entity   e   = MakeEntity(idx, 0); // generation filled below
            // Reconstruct full entity from registry
            e = RegistryEntityAt(idx);
            return DerefImpl(idx, e, std::index_sequence_for<Ts...>{});
        }

        Entity RegistryEntityAt(uint32_t idx) const noexcept;

        Iterator& operator++() noexcept { ++pos; Advance(); return *this; }
        bool operator==(const Iterator& o) const noexcept { return pos == o.pos; }
        bool operator!=(const Iterator& o) const noexcept { return pos != o.pos; }
    };

    Registry*                                    m_registry;
    std::tuple<ComponentPool<Ts>*...>            m_pools;
    IComponentPool*                              m_primary   = nullptr;
    std::array<IComponentPool*, kCount>          m_allPools  = {};
    bool                                         m_empty     = true;

    Iterator begin() noexcept {
        if (m_empty || !m_primary || m_primary->Size() == 0) return end();
        Iterator it{m_registry, &m_pools, m_primary, &m_allPools, 0, m_primary->Size()};
        it.Advance();
        return it;
    }

    Iterator end() noexcept {
        size_t sz = (!m_empty && m_primary) ? m_primary->Size() : 0u;
        return Iterator{m_registry, &m_pools, m_primary, &m_allPools, sz, sz};
    }
};

// ── Registry ──────────────────────────────────────────────────────────────────
// Core ECS data store. Manages entity lifetimes and typed component pools.
//
// PIMPL + template hybrid: non-template state (entity slots, free list, pool map)
// lives in Impl (defined in Registry.cpp). Template methods use a non-template
// bridge (GetOrCreatePool / FindPool) to access Impl without exposing it.

class Registry {
public:
    Registry();
    ~Registry();

    Registry(const Registry&)            = delete;
    Registry& operator=(const Registry&) = delete;

    // ── Entity lifecycle ──────────────────────────────────────────────────────

    [[nodiscard]] Entity Create();
    void                 Destroy(Entity e);
    [[nodiscard]] bool   IsAlive(Entity e) const;
    [[nodiscard]] size_t AliveCount() const;

    // Reconstruct a full Entity (with correct generation) from a raw slot index.
    // Used internally by ViewRange iterators.
    [[nodiscard]] Entity EntityFromIndex(uint32_t index) const;

    // ── Component operations ──────────────────────────────────────────────────

    template<typename T, typename... Args>
        requires std::constructible_from<T, Args...>
    T& Add(Entity e, Args&&... args) {
        assert(IsAlive(e));
        return PoolAs<T>().Emplace(e.Index(), T{std::forward<Args>(args)...});
    }

    template<typename T>
    void Remove(Entity e) {
        if (auto* pool = FindPoolAs<T>()) pool->Remove(e.Index());
    }

    template<typename T>
    [[nodiscard]] T& Get(Entity e) {
        return *PoolAs<T>().TryGet(e.Index());
    }

    template<typename T>
    [[nodiscard]] const T& Get(Entity e) const {
        return *FindPoolAs<T>()->TryGet(e.Index());
    }

    template<typename T>
    [[nodiscard]] T* TryGet(Entity e) noexcept {
        auto* pool = FindPoolAs<T>();
        return pool ? pool->TryGet(e.Index()) : nullptr;
    }

    template<typename T>
    [[nodiscard]] bool Has(Entity e) const noexcept {
        const auto* pool = FindPoolAs<T>();
        return pool && pool->Contains(e.Index());
    }

    // ── View ──────────────────────────────────────────────────────────────────
    // Returns a lazy range over all entities that have every component in Ts.
    // Iterates the smallest pool as the primary cursor for efficiency.
    // Usage: for (auto [entity, a, b] : registry.View<A, B>()) { ... }

    template<typename... Ts>
        requires (sizeof...(Ts) >= 1)
    [[nodiscard]] ViewRange<Ts...> View() {
        // Get typed pool pointers (nullptr if pool doesn't exist yet)
        auto pools = std::make_tuple(FindPoolAs<Ts>()...);

        // If any pool is absent, the view is empty
        bool anyNull = false;
        std::apply([&](auto*... p) { anyNull = (... || (p == nullptr)); }, pools);
        if (anyNull) return ViewRange<Ts...>{this, pools, nullptr, {}, true};

        // Collect raw pointers for Contains checks and to find the smallest pool
        std::array<IComponentPool*, sizeof...(Ts)> allPools;
        {
            size_t i = 0;
            std::apply([&](auto*... p) { ((allPools[i++] = p), ...); }, pools);
        }

        IComponentPool* primary = *std::min_element(
            allPools.begin(), allPools.end(),
            [](IComponentPool* a, IComponentPool* b) { return a->Size() < b->Size(); });

        return ViewRange<Ts...>{this, pools, primary, allPools, false};
    }

private:
    // Non-template bridge — defined in Registry.cpp, accesses Impl internals
    IComponentPool* GetOrCreatePool(size_t typeId,
                        std::function<std::unique_ptr<IComponentPool>()> factory);
    IComponentPool* FindPool(size_t typeId) const;

    template<typename T>
    ComponentPool<T>& PoolAs() {
        auto* raw = GetOrCreatePool(typeid(T).hash_code(), []() {
            return std::make_unique<ComponentPool<T>>();
        });
        return static_cast<ComponentPool<T>&>(*raw);
    }

    template<typename T>
    ComponentPool<T>* FindPoolAs() {
        auto* raw = FindPool(typeid(T).hash_code());
        return raw ? static_cast<ComponentPool<T>*>(raw) : nullptr;
    }

    template<typename T>
    const ComponentPool<T>* FindPoolAs() const {
        auto* raw = FindPool(typeid(T).hash_code());
        return raw ? static_cast<const ComponentPool<T>*>(raw) : nullptr;
    }

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ── ViewRange::Iterator::RegistryEntityAt (needs full Registry def) ───────────

template<typename... Ts>
Entity ViewRange<Ts...>::Iterator::RegistryEntityAt(uint32_t idx) const noexcept {
    return registry->EntityFromIndex(idx);
}

} // namespace xcel
