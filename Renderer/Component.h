#pragma once
#include "Kernel/PrimitiveSet.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include "Kernel/TessellationStrategy.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace xcel {

class Drawable;

// Human-readable tag attached to every entity.
struct NameComponent {
    std::string name;
};

// Holds any GPU-resident drawable for an entity (used by page entities and legacy path).
struct MeshComponent {
    std::shared_ptr<Drawable> mesh;
};

// Per-entity model-space transform. Defaults to identity.
struct TransformComponent {
    glm::mat4 matrix{1.0f};
};

// Controls whether the entity is submitted to the draw call list each frame.
struct VisibilityComponent {
    bool visible = true;
};

// ── Mesh data components ────────────────────────────────────────────────────
// All four components live directly on the mesh entity.
// Sharing is via shared_ptr reference counting across multiple entities.

struct CoordTableComponent {
    std::shared_ptr<CoordTable> coords;
};

struct ScalarTableComponent {
    std::shared_ptr<ScalarTable> scalars;
};

struct ColorTableComponent {
    std::shared_ptr<ColorTable> colorTable;
};

// Element connectivity for a mesh entity. Each entry is a single-type set;
// a mesh with mixed element types has multiple entries of different types.
struct PrimitiveSetsComponent {
    std::vector<std::shared_ptr<PrimitiveSet>> sets;
};

// ── Page metadata ───────────────────────────────────────────────────────────

// Metadata attached to page entities created by BatchingSystem.
struct PageMetaComponent {
    PrimitiveType primitiveType;
    uint64_t      capacityBytes = 0;
    uint64_t      usedBytes     = 0;
};

// Zero-size relationship tag: mesh entity → page entity.
// A mesh with N distinct PrimitiveTypes has N (BelongsToPage, pageEntity) pairs.
struct BelongsToPage {};

// Zero-size relationship tag: instance entity → template entity (InstanceDrawable).
struct InstanceOf {};

// Optional tessellation strategy for batched mesh entities.
// Null (absent component) defaults to AllFacesStrategy.
struct TessellationStrategyComponent {
    std::shared_ptr<ITessellationStrategy> strategy;
};

} // namespace xcel
