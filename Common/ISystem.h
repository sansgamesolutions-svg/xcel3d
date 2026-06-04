#pragma once

namespace xcel {

class Registry;

// Base class for all ECS systems. Systems hold references to the resources they
// need (rendering device, audio context, etc.) injected at construction time.
// Registry::Update(registry) is called once per frame by the host context.
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(Registry& registry) = 0;
};

} // namespace xcel
