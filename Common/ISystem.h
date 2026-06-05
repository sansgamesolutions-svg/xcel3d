#pragma once
#include <flecs.h>

namespace xcel {

// Base class for all ECS systems. Systems hold references to the resources they
// need (rendering device, audio context, etc.) injected at construction time.
// Update() is called once per frame by WindowContext before the draw call.
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(flecs::world& world) = 0;
};

} // namespace xcel
