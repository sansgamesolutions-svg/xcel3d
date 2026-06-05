#pragma once
#include <cstdint>

namespace xcel {

// Opaque entity handle. Upper 12 bits = generation counter (catches use-after-destroy).
// Lower 20 bits = slot index (supports ~1M simultaneous live entities).
// id == 0 is the invalid sentinel â€” generation 0 at slot 0 is never issued.
struct Entity {
    static constexpr uint32_t kIndexBits = 20u;
    static constexpr uint32_t kIndexMask = (1u << kIndexBits) - 1u;
    static constexpr uint32_t kGenShift  = kIndexBits;

    uint32_t id = 0u;

    [[nodiscard]] uint32_t Index()      const noexcept { return id & kIndexMask; }
    [[nodiscard]] uint32_t Generation() const noexcept { return id >> kGenShift; }
    [[nodiscard]] bool     IsValid()    const noexcept { return id != 0u; }

    bool operator==(const Entity&) const noexcept = default;
    bool operator!=(const Entity&) const noexcept = default;
};

inline Entity MakeEntity(uint32_t index, uint32_t generation) noexcept
{
    return Entity{(generation << Entity::kGenShift) | (index & Entity::kIndexMask)};
}

} // namespace xcel
