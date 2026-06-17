#include "Renderer/BatchingSystem.h"
#include "Renderer/BatchDrawable.h"
#include "Renderer/Component.h"
#include "Common/Logger.h"
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>

namespace xcel {

BatchingSystem::BatchingSystem(uint64_t pageCapacityBytes, BatchingStrategy strategy)
{
    m_pageCapacity = pageCapacityBytes;
    m_strategy     = strategy;
}

void BatchingSystem::SetStrategy(BatchingStrategy strategy)
{
    assert(m_pageCount == 0 && "BatchingSystem::SetStrategy must be called before BuildAll");
    m_strategy = strategy;
}

void BatchingSystem::Register(flecs::entity meshEntity)
{
    m_pending.push_back(meshEntity);
}

// ── Byte budget estimation ────────────────────────────────────────────────────
// Derived from MeshTessellator.h output table (sizeof(MeshVertex)=36, sizeof(uint32_t)=4):
//   HEX  : 24 verts×36 + 36 idx×4 = 1 008 bytes/element
//   TET  : 12×36  + 12×4           =   480 bytes/element
//   QUAD : 4×36   + 6×4            =   168 bytes/element
//   TRI  : 3×36   + 3×4            =   120 bytes/element
//   LINE : 4×36   + 6×4            =   168 bytes/element
//   POLYLINE: assume avg 10 nodes → 9 segments × 168
uint64_t BatchingSystem::EstimateBytes(const PrimitiveSet& ps)
{
    const uint64_t kVB = 36;
    const uint64_t kIB = 4;
    uint64_t n = ps.ElementCount();

    switch (ps.Type()) {
    case PrimitiveType::PT_HEXAHEDRON:  return n * (24 * kVB + 36 * kIB);
    case PrimitiveType::PT_TETRAHEDRON: return n * (12 * kVB + 12 * kIB);
    case PrimitiveType::PT_QUAD:        return n * ( 4 * kVB +  6 * kIB);
    case PrimitiveType::PT_TRIANGLE:    return n * ( 3 * kVB +  3 * kIB);
    case PrimitiveType::PT_LINE:        return n * ( 4 * kVB +  6 * kIB);
    case PrimitiveType::PT_POLYLINE:    return n * 9 * ( 4 * kVB +  6 * kIB);
    }
    return 0;
}

const char* BatchingSystem::PrimitiveTypeName(PrimitiveType type)
{
    switch (type) {
    case PrimitiveType::PT_HEXAHEDRON:  return "HEX";
    case PrimitiveType::PT_TETRAHEDRON: return "TET";
    case PrimitiveType::PT_QUAD:        return "QUAD";
    case PrimitiveType::PT_TRIANGLE:    return "TRI";
    case PrimitiveType::PT_LINE:        return "LINE";
    case PrimitiveType::PT_POLYLINE:    return "POLYLINE";
    }
    return "UNKNOWN";
}

// ── Page management ───────────────────────────────────────────────────────────

flecs::entity BatchingSystem::FindOrCreatePage(
    flecs::world& world,
    PrimitiveType type,
    BlendMode     blendMode,
    uint64_t      neededBytes)
{
    // When strategy is ByPrimitiveType, blendMode is not part of the page key.
    const bool matchBlend = (m_strategy == BatchingStrategy::ByPrimitiveTypeAndBlend);

    flecs::entity found;
    world.each([&](flecs::entity e, PageMetaComponent& meta) {
        if (found.is_valid()) return;
        if (meta.primitiveType != type) return;
        if (matchBlend && meta.blendMode != blendMode) return;
        if ((meta.usedBytes + neededBytes) <= meta.capacityBytes)
            found = e;
    });

    if (found.is_valid()) return found;

    std::string name = std::string("page_") + PrimitiveTypeName(type);
    if (matchBlend)
    {
        switch (blendMode)
        {
        case BlendMode::AlphaBlend:         name += "_alpha";    break;
        case BlendMode::Additive:           name += "_additive"; break;
        case BlendMode::Premultiplied:      name += "_premult";  break;
        case BlendMode::WeightedBlendedOIT: name += "_oit";      break;
        default:                                                 break;
        }
    }
    name += "_" + std::to_string(m_pageCount++);

    XCEL_LOG_DEBUG(Batching, "Creating page '{}' ({} byte budget)", name, m_pageCapacity);

    auto bd = std::make_shared<BatchDrawable>();

    flecs::entity page = world.entity()
        .set<NameComponent>({name})
        .set<MeshComponent>({std::move(bd)})
        .set<VisibilityComponent>({true})
        .set<PageMetaComponent>({type, blendMode, m_pageCapacity, 0});

    return page;
}

// ── BuildAll ──────────────────────────────────────────────────────────────────

void BatchingSystem::BuildAll(
    flecs::world& world,
    DeviceContext& dev,
    ThreadPool*    pool)
{
    m_world = &world;
    m_dev   = &dev;

    // Assign each registered mesh entity to page(s) by PrimitiveType (and BlendMode
    // when strategy == ByPrimitiveTypeAndBlend).
    for (flecs::entity e : m_pending) {
        const PrimitiveSetsComponent* psc = e.get<PrimitiveSetsComponent>();
        if (!psc) continue;

        const BlendMode blendMode = [&]() -> BlendMode {
            const auto* ro = e.get<MeshRenderOptions>();
            return ro ? ro->blendMode : BlendMode::Opaque;
        }();

        std::unordered_map<int, uint64_t> bytesPerType;
        for (const auto& ps : psc->sets)
            bytesPerType[static_cast<int>(ps->Type())] += EstimateBytes(*ps);

        for (auto& [typeInt, bytes] : bytesPerType) {
            auto type = static_cast<PrimitiveType>(typeInt);
            flecs::entity page = FindOrCreatePage(world, type, blendMode, bytes);
            e.add<BelongsToPage>(page);

            auto* meta = page.get_mut<PageMetaComponent>();
            if (meta) meta->usedBytes += bytes;
        }
    }

    XCEL_LOG_INFO(Batching, "BuildAll: {} mesh(es) -> {} page(s)",
                  m_pending.size(), m_pageCount);
    m_pending.clear();

    // Build GPU buffers for all pages.
    world.each([&](flecs::entity e, const PageMetaComponent&) {
        RebuildPage(e, pool);
    });

    // Observer: mark pages dirty when a mesh entity's visibility changes.
    world.observer<VisibilityComponent>()
        .event(flecs::OnSet)
        .each([this](flecs::entity e, const VisibilityComponent&) {
            if (!e.has<PrimitiveSetsComponent>()) return;
            e.each<BelongsToPage>([this](flecs::entity page) {
                m_dirtyPages.insert(page.id());
            });
        });
}

// ── Per-page rebuild ──────────────────────────────────────────────────────────

void BatchingSystem::RebuildPage(flecs::entity pageEntity, ThreadPool* pool)
{
    const MeshComponent*     mc   = pageEntity.get<MeshComponent>();
    const PageMetaComponent* meta = pageEntity.get<PageMetaComponent>();
    if (!mc || !meta) return;

    auto* bd = dynamic_cast<BatchDrawable*>(mc->mesh.get());
    if (!bd) return;

    PrimitiveType pageType = meta->primitiveType;

    std::vector<MeshTessellationInput> inputs;

    m_world->each([&](flecs::entity e,
                              const PrimitiveSetsComponent& psc,
                              const VisibilityComponent&    vc) {
        if (!vc.visible || !e.has<BelongsToPage>(pageEntity)) return;

        const CoordTableComponent*  cc = e.get<CoordTableComponent>();
        const ScalarTableComponent* sc = e.get<ScalarTableComponent>();
        const ColorTableComponent*  co = e.get<ColorTableComponent>();
        if (!cc || !sc || !co) return;

        const TessellationStrategyComponent* stc = e.get<TessellationStrategyComponent>();
        const ITessellationStrategy* strat = stc ? stc->strategy.get() : nullptr;

        size_t scalarOffset = 0;
        for (const auto& ps : psc.sets) {
            if (ps->Type() == pageType) {
                inputs.push_back({ps.get(),
                                  cc->coords.get(),
                                  sc->scalars.get(),
                                  co->colorTable.get(),
                                  strat,
                                  scalarOffset});
            }
            scalarOffset += ps->ElementCount();
        }
    });

    XCEL_LOG_DEBUG(Batching, "RebuildPage type={} inputs={}",
                   PrimitiveTypeName(pageType), inputs.size());

    bd->Rebuild(*m_dev, inputs, pool);

    XCEL_LOG_TRACE(Batching, "  -> {} indices", bd->IndexCount());
}

// ── FlushRebuild ──────────────────────────────────────────────────────────────

void BatchingSystem::FlushRebuild(ThreadPool* pool)
{
    if (m_dirtyPages.empty() || !m_world) return;

    for (uint64_t id : m_dirtyPages) {
        flecs::entity page(*m_world, id);
        if (page.is_valid())
            RebuildPage(page, pool);
    }
    m_dirtyPages.clear();
}

} // namespace xcel
