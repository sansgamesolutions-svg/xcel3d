#include "Graphics/World.h"
#include "Graphics/BatchingSystem.h"
#include "Graphics/Component.h"
#include "Graphics/InstanceDrawable.h"
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include "Graphics/PrimitiveSet.h"
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
