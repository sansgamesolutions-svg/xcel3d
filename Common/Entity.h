#pragma once
#include <flecs.h>

namespace xcel {

// Entity handle backed by flecs. Holds a (world*, id) pair; valid as long as
// the owning flecs::world is alive and the entity has not been destroyed.
using Entity = flecs::entity;

} // namespace xcel
