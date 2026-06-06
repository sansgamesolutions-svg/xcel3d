#pragma once
#include "Common/Entity.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

#include <flecs.h>

namespace xcel {

class CoordTable;
class ScalarTable;
class ColorTable;
class PrimitiveSet;
class DeviceContext;
class ThreadPool;

// Scene-graph owner. Wraps flecs::world and BatchingSystem, and exposes the
// mesh/entity creation API previously on WindowContext.
//
// Singleton contract: exactly one World may be alive at a time. The constructor
// registers this as s_instance; the destructor clears it. WindowContext::Impl
// controls the lifetime. Call World::Instance() from anywhere to reach it.
class World
{
public:
    World();
    ~World();

    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    // Returns the one live World. Asserts if none has been constructed yet.
    static World& Instance();

    // ── Scene object creation ─────────────────────────────────────────────────
    Entity AddMesh(const std::string&                         name,
                   std::shared_ptr<CoordTable>                coords,
                   std::shared_ptr<ScalarTable>               scalars,
                   std::shared_ptr<ColorTable>                colorTable,
                   std::vector<std::shared_ptr<PrimitiveSet>> primSets);

    Entity AddInstanceMesh(const std::string&                         name,
                           std::shared_ptr<CoordTable>                coords,
                           std::shared_ptr<ScalarTable>               scalars,
                           std::shared_ptr<ColorTable>                colorTable,
                           std::vector<std::shared_ptr<PrimitiveSet>> primSets);

    Entity AddInstance(Entity templateEntity,
                       const glm::mat4& transform = glm::mat4{1.f});

    // ── ECS access ────────────────────────────────────────────────────────────
    // Returns the underlying flecs::world for query/observer use inside Graphics/.
    flecs::world& Ecs();

    // ── BatchingSystem lifecycle (called by WindowContext) ────────────────────
    void BuildAll(DeviceContext& dev, ThreadPool* pool);
    void FlushRebuild(ThreadPool* pool);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    static World* s_instance;
};

} // namespace xcel
