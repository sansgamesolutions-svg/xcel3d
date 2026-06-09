#pragma once
#include "Graphics/MeshTessellator.h"
#include "Graphics/PrimitiveSet.h"
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include <cstddef>
#include <memory>

namespace xcel {

// Forward declaration — full definition below.
class ITessellationStrategy;

// Bundles one PrimitiveSet with its three data tables and an optional strategy.
// Null strategy defaults to AllFacesStrategy inside TessellateInput().
struct MeshTessellationInput {
    const PrimitiveSet*          primitiveSet = nullptr;
    const CoordTable*            coords       = nullptr;
    const ScalarTable*           scalars      = nullptr;
    const ColorTable*            colorTable   = nullptr;
    const ITessellationStrategy* strategy     = nullptr;
    size_t                       scalarOffset = 0;
    glm::mat4                    transform{1.f};
};

// Abstract tessellation strategy. Controls which faces of each element are emitted.
// Thread safety: TessellateRange() must be thread-safe for non-overlapping element
// ranges when IsParallelizable() returns true.
class ITessellationStrategy {
public:
    virtual ~ITessellationStrategy() = default;

    virtual TessellatedMesh TessellateRange(
        const MeshTessellationInput& inp,
        size_t                       elementBegin,
        size_t                       elementEnd,
        float                        minScalar,
        float                        maxScalar) const = 0;

    // Returns false if TessellateInput() must call TessellateRange on the full
    // element range rather than splitting across threads.
    virtual bool IsParallelizable() const { return true; }

protected:
    ITessellationStrategy()                                      = default;
    ITessellationStrategy(const ITessellationStrategy&)            = delete;
    ITessellationStrategy& operator=(const ITessellationStrategy&) = delete;
};

// Tessellate every face of every element (default behaviour).
class AllFacesStrategy : public ITessellationStrategy {
public:
    TessellatedMesh TessellateRange(const MeshTessellationInput& inp,
                                    size_t elementBegin, size_t elementEnd,
                                    float minScalar, float maxScalar) const override;
    bool IsParallelizable() const override { return true; }
};

// Tessellate only exterior (boundary) faces of 3-D elements.
// A face is exterior when it is not shared with any other element in the same
// PrimitiveSet. For 2-D elements (QUAD, TRI, LINE, POLYLINE) every face is
// exterior, so the result equals AllFacesStrategy for those types.
//
// Note: IsParallelizable() returns false because face-adjacency detection
// requires scanning the full element set before tessellating any range.
class WettedFacesStrategy : public ITessellationStrategy {
public:
    TessellatedMesh TessellateRange(const MeshTessellationInput& inp,
                                    size_t elementBegin, size_t elementEnd,
                                    float minScalar, float maxScalar) const override;
    bool IsParallelizable() const override { return false; }
};

} // namespace xcel
