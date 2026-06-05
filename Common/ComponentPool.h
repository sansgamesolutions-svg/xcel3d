#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
#include <limits>

namespace xcel {

// Ã¢â€â‚¬Ã¢â€â‚¬ IComponentPool Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Type-erased base so the Registry can store heterogeneous pools in one map.

class IComponentPool {
public:
    virtual ~IComponentPool() = default;

    virtual void     Remove(uint32_t entityIndex)            = 0;
    virtual bool     Contains(uint32_t entityIndex) const    = 0;
    virtual size_t   Size() const                            = 0;
    // Returns the entity slot index at position denseSlot in the dense array.
    // Used by ViewRange to iterate entity indices without knowing T.
    virtual uint32_t EntityIndexAt(size_t denseSlot) const   = 0;
};

// Ã¢â€â‚¬Ã¢â€â‚¬ SparseSetStorage<T> Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Default storage policy.
//
// Three parallel arrays maintain a dense packing:
//   m_dense    Ã¢â‚¬â€ tightly packed component values (cache-friendly iteration)
//   m_entities Ã¢â‚¬â€ dense[i] Ã¢â€ â€™ entity slot index (for EntityIndexAt)
//   m_sparse   Ã¢â‚¬â€ entity slot index Ã¢â€ â€™ dense slot (UINT32_MAX = absent)
//
// Add / Remove are O(1). Contains is O(1). Iteration is cache-friendly.
// To swap in a different storage strategy: provide a class with the same
// interface (Emplace, Erase, TryGet, Contains, Size, EntityAt) and pass it
// as the Storage template argument to ComponentPool.

template<typename T>
class SparseSetStorage {
public:
    static constexpr uint32_t kAbsent = std::numeric_limits<uint32_t>::max();

    T& Emplace(uint32_t entityIdx, T value)
    {
        if (entityIdx >= m_sparse.size())
            m_sparse.resize(static_cast<size_t>(entityIdx) + 1u, kAbsent);

        m_sparse[entityIdx] = static_cast<uint32_t>(m_dense.size());
        m_dense.push_back(std::move(value));
        m_entities.push_back(entityIdx);
        return m_dense.back();
    }

    void Erase(uint32_t entityIdx)
    {
        if (entityIdx >= m_sparse.size() || m_sparse[entityIdx] == kAbsent)
            return;

        uint32_t slot     = m_sparse[entityIdx];
        uint32_t lastIdx  = m_entities.back();

        // Swap-with-last to keep dense array packed
        m_dense[slot]    = std::move(m_dense.back());
        m_entities[slot] = lastIdx;
        m_sparse[lastIdx] = slot;

        m_dense.pop_back();
        m_entities.pop_back();
        m_sparse[entityIdx] = kAbsent;
    }

    [[nodiscard]] T* TryGet(uint32_t entityIdx) noexcept
    {
        if (entityIdx >= m_sparse.size() || m_sparse[entityIdx] == kAbsent)
            return nullptr;
        return &m_dense[m_sparse[entityIdx]];
    }

    [[nodiscard]] const T* TryGet(uint32_t entityIdx) const noexcept
    {
        if (entityIdx >= m_sparse.size() || m_sparse[entityIdx] == kAbsent)
            return nullptr;
        return &m_dense[m_sparse[entityIdx]];
    }

    [[nodiscard]] bool     Contains(uint32_t entityIdx) const noexcept
    {
        return entityIdx < m_sparse.size() && m_sparse[entityIdx] != kAbsent;
    }

    [[nodiscard]] size_t   Size()              const noexcept { return m_dense.size(); }
    [[nodiscard]] uint32_t EntityAt(size_t i)  const noexcept { return m_entities[i]; }

private:
    std::vector<T>        m_dense;
    std::vector<uint32_t> m_entities;   // dense Ã¢â€ â€™ entity index
    std::vector<uint32_t> m_sparse;     // entity index Ã¢â€ â€™ dense slot
};

// Ã¢â€â‚¬Ã¢â€â‚¬ ComponentPool<T, Storage> Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Concrete typed pool that wraps a storage policy and implements IComponentPool.
// Default storage is SparseSetStorage<T>; swap in another policy at the template
// argument without changing any other code.

template<typename T, template<typename> typename Storage = SparseSetStorage>
class ComponentPool : public IComponentPool {
public:
    T& Emplace(uint32_t entityIdx, T value)
    {
        return m_storage.Emplace(entityIdx, std::move(value));
    }

    void Remove(uint32_t entityIdx) override
    {
        m_storage.Erase(entityIdx);
    }

    [[nodiscard]] bool Contains(uint32_t entityIdx) const override
    {
        return m_storage.Contains(entityIdx);
    }

    [[nodiscard]] size_t Size() const override
    {
        return m_storage.Size();
    }

    [[nodiscard]] uint32_t EntityIndexAt(size_t denseSlot) const override
    {
        return m_storage.EntityAt(denseSlot);
    }

    [[nodiscard]] T* TryGet(uint32_t entityIdx) noexcept
    {
        return m_storage.TryGet(entityIdx);
    }

    [[nodiscard]] const T* TryGet(uint32_t entityIdx) const noexcept
    {
        return m_storage.TryGet(entityIdx);
    }

    [[nodiscard]] Storage<T>& GetStorage() noexcept { return m_storage; }

private:
    Storage<T> m_storage;
};

} // namespace xcel
