#pragma once
#include "Graphics/HexTessellator.h"  // for MeshVertex, TessellatedMesh
#include "Graphics/PrimitiveSet.h"
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include <cstddef>

namespace xcel {

// Unified tessellator — dispatches on ps.Type() to the correct algorithm.
//
// Supported types and their output:
//   PT_HEXAHEDRON  → 6 quad faces → 12 triangles, 24 verts per element
//   PT_TETRAHEDRON → 4 tri  faces →  4 triangles, 12 verts per element
//   PT_QUAD        → 1 quad face  →  2 triangles,  4 verts per element
//   PT_TRIANGLE    → 1 tri  face  →  1 triangle,   3 verts per element
//   PT_LINE        → 1 ribbon quad →  2 triangles,  4 verts per segment
//   PT_POLYLINE    → (N-1) ribbon quads per element (N = node count)
//
// Color: one scalar per element maps to a uniform RGB for all its vertices.
// Line ribbons: world-space quads of constant half-width kLineHalfWidth;
//   not true screen-space (width varies with distance from camera).

static constexpr float kLineHalfWidth = 0.005f;

// Tessellates [elementBegin, elementEnd) of any PrimitiveSet.
// Thread-safe for non-overlapping element ranges on the same PrimitiveSet.
// minScalar / maxScalar must span the *full* ScalarTable for consistent coloring
// across parallel chunks.
TessellatedMesh TessellateRange(
    const PrimitiveSet& ps,
    const CoordTable&   coords,
    const ScalarTable&  scalars,
    const ColorTable&   colormap,
    size_t              elementBegin,
    size_t              elementEnd,
    float               minScalar,
    float               maxScalar
);

// Convenience overload: tessellates all elements and computes min/max internally.
TessellatedMesh Tessellate(
    const PrimitiveSet& ps,
    const CoordTable&   coords,
    const ScalarTable&  scalars,
    const ColorTable&   colormap
);

} // namespace xcel
