#pragma once
#include "Kernel/PrimitiveSet.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include <glm/glm.hpp>
#include <cstddef>
#include <span>
#include <vector>

namespace xcel {

class ThreadPool;
struct MeshTessellationInput; // full definition in TessellationStrategy.h

// ============================================================================
// Tessellation pipeline overview
// ============================================================================
//
// WHY tessellate at all?
//   VTK element types (hex, tet, quad, etc.) encode volumetric or surface
//   regions.  Vulkan can only shade triangles, so every element must be
//   decomposed into a flat triangle list before it can be rendered.
//
// Vertex duplication at element boundaries:
//   Adjacent elements may carry very different scalar values (e.g. a
//   high-stress element next to a low-stress one).  Sharing vertices would
//   let the GPU interpolate color across the boundary, hiding the
//   discontinuity.  Duplicating vertices per element gives each element a
//   flat, uniform color that faithfully represents its own scalar result.
//   The same logic applies to face normals: per-element flat normals produce
//   the hard-edged shading that correctly conveys FEM brick geometry.
//
// Color mapping:
//   min/max scalar values are computed once across the full ScalarTable.
//   Each element's scalar is normalised to [0,1] and looked up in the
//   ColorTable (256-entry cool-to-warm ramp).  The RGB is baked into all
//   vertices of that element — no per-element uniform or instancing needed.
//
// Thread safety:
//   TessellateRange reads all inputs as read-only after construction.
//   Multiple threads may call it simultaneously on non-overlapping element
//   ranges of the same PrimitiveSet.
//
// Output per element type:
//   PT_HEXAHEDRON  -- 6 quad faces  -- 12 triangles, 24 verts
//   PT_TETRAHEDRON -- 4 tri  faces  --  4 triangles, 12 verts
//   PT_QUAD        -- 1 quad face   --  2 triangles,  4 verts
//   PT_TRIANGLE    -- 1 tri  face   --  1 triangle,   3 verts
//   PT_LINE        -- 1 ribbon quad --  2 triangles,  4 verts per segment
//   PT_POLYLINE    -- (N-1) ribbon quads per element (N = node count)
// ============================================================================

// GPU vertex layout (matches shader attribute locations in Pipeline.cpp):
//   location 0 -- vec3 position  (byte offset  0, 12 bytes)
//   location 1 -- vec3 normal    (byte offset 12, 12 bytes)
//   location 2 -- vec3 color     (byte offset 24, 12 bytes)
//   total stride: 36 bytes
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;   // flat face normal; same for all verts of a quad face
    glm::vec3 color;    // RGB from ColorTable; uniform per element
};

// Triangle list ready for direct upload to a device-local VkBuffer pair.
struct TessellatedMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t>   indices;
};

// World-space half-width of line ribbon quads (not true screen-space).
static constexpr float kLineHalfWidth = 0.005f;

// ── Face connectivity tables ───────────────────────────────────────────────
// Exposed here so tessellation strategies can use them directly.

// VTK HEX face table — 6 CCW quads viewed from outside, normal = cross(v1-v0, v3-v0).
constexpr std::array<std::array<int,4>,6> kHexFaces = {{
    {0, 3, 2, 1},  // bottom (Z-)
    {4, 5, 6, 7},  // top    (Z+)
    {0, 1, 5, 4},  // front  (Y-)
    {3, 7, 6, 2},  // back   (Y+)
    {0, 4, 7, 3},  // left   (X-)
    {1, 2, 6, 5},  // right  (X+)
}};

// VTK TET face table — 4 CCW triangles with outward normals.
constexpr std::array<std::array<int,3>,4> kTetFaces = {{
    {0, 2, 1},
    {0, 1, 3},
    {1, 2, 3},
    {2, 0, 3},
}};

// ── Geometry helpers (used by tessellators and tessellation strategies) ────

namespace detail {

// Append a quad as two CCW triangles (fan: (0,1,2) + (0,2,3)).
inline void PushQuad(TessellatedMesh& out,
                     const glm::vec3  p[4],
                     const glm::vec3& normal,
                     const glm::vec3& rgb)
{
    uint32_t base = static_cast<uint32_t>(out.vertices.size());
    for (int k = 0; k < 4; ++k)
        out.vertices.push_back({p[k], normal, rgb});
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 2);
    out.indices.push_back(base + 3);
}

// Append a triangle as one CCW triangle.
inline void PushTriangle(TessellatedMesh& out,
                         const glm::vec3  p[3],
                         const glm::vec3& normal,
                         const glm::vec3& rgb)
{
    uint32_t base = static_cast<uint32_t>(out.vertices.size());
    for (int k = 0; k < 3; ++k)
        out.vertices.push_back({p[k], normal, rgb});
    out.indices.push_back(base + 0);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
}

} // namespace detail

// ── Low-level dispatch ─────────────────────────────────────────────────────

// Tessellates [elementBegin, elementEnd) of any PrimitiveSet.
// Thread-safe for non-overlapping element ranges on the same PrimitiveSet.
// minScalar / maxScalar must span the *full* ScalarTable for consistent
// coloring across parallel chunks.
TessellatedMesh TessellateRange(
    const PrimitiveSet& ps,
    const CoordTable&   coords,
    const ScalarTable&  scalars,
    const ColorTable&   colormap,
    size_t              elementBegin,
    size_t              elementEnd,
    float               minScalar,
    float               maxScalar,
    size_t              scalarOffset = 0
);

// Convenience overload: tessellates all elements and computes min/max internally.
TessellatedMesh Tessellate(
    const PrimitiveSet& ps,
    const CoordTable&   coords,
    const ScalarTable&  scalars,
    const ColorTable&   colormap
);

// ── Strategy-based engine ──────────────────────────────────────────────────
// These functions use the strategy stored in MeshTessellationInput (or fall
// back to AllFacesStrategy when strategy == nullptr).
// Full definitions of MeshTessellationInput and strategies: TessellationStrategy.h

// Tessellate one input. Uses parallel chunking if pool != nullptr and element
// count exceeds the internal threshold, provided the strategy is parallelizable.
TessellatedMesh TessellateInput(const MeshTessellationInput& inp,
                                 ThreadPool* pool = nullptr);

// Tessellate multiple inputs and merge into one combined TessellatedMesh.
TessellatedMesh TessellateAndMerge(std::span<const MeshTessellationInput> inputs,
                                    ThreadPool* pool = nullptr);

} // namespace xcel
