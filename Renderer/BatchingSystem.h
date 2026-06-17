#pragma once
#include "Kernel/PrimitiveSet.h"
#include "Renderer/RenderOptions.h"
#include <flecs.h>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace xcel {

class DeviceContext;
class ThreadPool;

class BatchingSystem
{
public:
    explicit BatchingSystem(
        uint64_t         pageCapacityBytes = 256ULL * 1024 * 1024,
        BatchingStrategy strategy          = BatchingStrategy::ByPrimitiveType);

    ~BatchingSystem() = default;

    BatchingSystem(const BatchingSystem&)            = delete;
    BatchingSystem& operator=(const BatchingSystem&) = delete;

    // Must be called before BuildAll(); asserts if pages already exist.
    void SetStrategy(BatchingStrategy strategy);

    void Register(flecs::entity meshEntity);
    void BuildAll(flecs::world& world, DeviceContext& dev, ThreadPool* pool);
    void FlushRebuild(ThreadPool* pool);

private:
    flecs::entity FindOrCreatePage(
        flecs::world& world,
        PrimitiveType type,
        BlendMode     blendMode,
        uint64_t      neededBytes);

    void RebuildPage(flecs::entity page, ThreadPool* pool);

    static uint64_t    EstimateBytes(const PrimitiveSet& ps);
    static const char* PrimitiveTypeName(PrimitiveType type);

    flecs::world*    m_world        = nullptr;
    DeviceContext*   m_dev          = nullptr;
    uint64_t         m_pageCapacity = 0;
    uint32_t         m_pageCount    = 0;
    BatchingStrategy m_strategy     = BatchingStrategy::ByPrimitiveType;

    std::vector<flecs::entity>   m_pending;
    std::unordered_set<uint64_t> m_dirtyPages;
};

} // namespace xcel
