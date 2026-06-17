#pragma once
#include "Common/Entity.h"
#include "Renderer/BatchingSystem.h"
#include "Renderer/RenderOptions.h"
#include <glm/glm.hpp>
#include <flecs.h>
#include <memory>
#include <string>
#include <vector>

namespace xcel {

class CoordTable;
class ScalarTable;
class ColorTable;
class PrimitiveSet;
class DeviceContext;
class ThreadPool;

class World
{
public:
    World();
    ~World();

    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    static World& Instance();

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

    Entity AddLight(const std::string& name,
                    const glm::vec3&   position,
                    const glm::vec3&   color     = glm::vec3{1.f},
                    float              intensity = 1.0f);

    flecs::world& Ecs();

    // Must be called before BuildAll(); no-op if no pages have been created yet.
    void SetBatchingStrategy(BatchingStrategy strategy);

    void BuildAll(DeviceContext& dev, ThreadPool* pool);
    void FlushRebuild(ThreadPool* pool);

private:
    flecs::world   m_ecs;
    BatchingSystem m_batchingSystem;

    static World* s_instance;
};

} // namespace xcel
