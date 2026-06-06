#pragma once
#include "Graphics/PrimitiveSet.h"
#include <flecs.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace xcel {

class DeviceContext;
class ThreadPool;

// Packs mesh entities into shared GPU buffer pages to reduce draw calls.
//
// Usage:
//   1. Call Register(e) once per mesh entity (before Run()).
//   2. Call BuildAll() during initialization (inside WindowContext::BuildMeshes).
//      This assigns entities to page entities by PrimitiveType + byte budget,
//      then tessellates and uploads all pages.
//   3. Call FlushRebuild() each frame (after vkWaitForFences) to re-upload any
//      pages dirtied by visibility changes.
//
// Page grouping:
//   - Only meshes contributing the same PrimitiveType share a page.
//   - Pages have a configurable byte budget (default 256 MB).
//   - A mesh with N distinct PrimitiveTypes belongs to N pages.
//
// Each mesh entity must carry CoordTableComponent, ScalarTableComponent,
// ColorTableComponent, and PrimitiveSetsComponent directly as components.
class BatchingSystem {
public:
    explicit BatchingSystem(uint64_t pageCapacityBytes = 256ULL * 1024 * 1024);
    ~BatchingSystem();

    // Record a mesh entity for deferred page assignment. Safe to call before BuildAll().
    void Register(flecs::entity meshEntity);

    // Assign registered mesh entities to page entities, tessellate, and upload GPU buffers.
    // Wires the VisibilityComponent observer that marks pages dirty on hide/show.
    void BuildAll(flecs::world& world, DeviceContext& dev, ThreadPool* pool);

    // Re-upload pages dirtied since the last call. Call once per frame after vkWaitForFences.
    void FlushRebuild(ThreadPool* pool);

private:
    flecs::entity FindOrCreatePage(flecs::world& world, PrimitiveType type, uint64_t neededBytes);
    void          RebuildPage(flecs::entity page, ThreadPool* pool);

    static uint64_t    EstimateBytes(const PrimitiveSet& ps);
    static const char* PrimitiveTypeName(PrimitiveType type);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
