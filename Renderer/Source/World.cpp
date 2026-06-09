#include "Renderer/World.h"
#include "Renderer/BatchingSystem.h"
#include "Renderer/Component.h"
#include "Renderer/InstanceDrawable.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include "Kernel/PrimitiveSet.h"
#include <cassert>

namespace xcel {

World* World::s_instance = nullptr;

World::World()
    {
    assert(s_instance == nullptr && "Only one World may be alive at a time");
    s_instance = this;
}

World::~World()
{
    s_instance = nullptr;
}

World& World::Instance()
{
    assert(s_instance != nullptr && "World::Instance() called before a World was constructed");
    return *s_instance;
}

Entity World::AddMesh(const std::string&                         name,
                      std::shared_ptr<CoordTable>                coords,
                      std::shared_ptr<ScalarTable>               scalars,
                      std::shared_ptr<ColorTable>                colorTable,
                      std::vector<std::shared_ptr<PrimitiveSet>> primSets)
{
    flecs::entity e = m_ecs.entity()
        .set<NameComponent>({name})
        .set<CoordTableComponent>({std::move(coords)})
        .set<ScalarTableComponent>({std::move(scalars)})
        .set<ColorTableComponent>({std::move(colorTable)})
        .set<PrimitiveSetsComponent>({std::move(primSets)})
        .set<TransformComponent>({})
        .set<VisibilityComponent>({});

    m_batchingSystem.Register(e);
    return e;
}

Entity World::AddInstanceMesh(const std::string&                         name,
                               std::shared_ptr<CoordTable>                coords,
                               std::shared_ptr<ScalarTable>               scalars,
                               std::shared_ptr<ColorTable>                colorTable,
                               std::vector<std::shared_ptr<PrimitiveSet>> primSets)
{
    flecs::entity e = m_ecs.entity()
        .set<NameComponent>({name})
        .set<CoordTableComponent>({std::move(coords)})
        .set<ScalarTableComponent>({std::move(scalars)})
        .set<ColorTableComponent>({std::move(colorTable)})
        .set<PrimitiveSetsComponent>({std::move(primSets)})
        .set<TransformComponent>({})
        .set<VisibilityComponent>({});
    e.set<MeshComponent>({std::make_shared<InstanceDrawable>(e)});
    return e;
}

Entity World::AddInstance(Entity templateEntity, const glm::mat4& transform)
{
    return m_ecs.entity()
        .set<TransformComponent>({transform})
        .set<VisibilityComponent>({true})
        .add<InstanceOf>(static_cast<flecs::entity>(templateEntity));
}

Entity World::AddLight(const std::string& name,
                       const glm::vec3&   position,
                       const glm::vec3&   color,
                       float              intensity)
{
    return m_ecs.entity()
        .set<NameComponent>({name})
        .set<LightComponent>({position, color, intensity});
}

flecs::world& World::Ecs()
{
    return m_ecs;
}

void World::BuildAll(DeviceContext& dev, ThreadPool* pool)
{
    m_batchingSystem.BuildAll(m_ecs, dev, pool);
}

void World::FlushRebuild(ThreadPool* pool)
{
    m_batchingSystem.FlushRebuild(pool);
}

} // namespace xcel
