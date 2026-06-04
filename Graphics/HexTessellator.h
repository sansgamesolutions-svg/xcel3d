#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "Graphics/CoordTable.h"
#include "Graphics/ScalarTable.h"
#include "Graphics/ColorTable.h"
#include "Graphics/PrimitiveSet.h"

namespace xcel {

// ============================================================================
// VTK Hex Tessellation — Pipeline Overview
// ============================================================================
//
// This header is the heart of the VTK-to-GPU conversion.  It converts the
// abstract FEM data model (node coordinates + element connectivity + scalar
// field) into a flat, GPU-ready triangle list.
//
// WHY tessellate at all?
//   A VTK hex element is defined by 8 corner nodes and encodes a volumetric
//   region.  Vulkan (and all rasterisation APIs) cannot directly draw solid
//   hexes — they can only shade triangles.  Tessellation converts each hex
//   into its 6 bounding quad faces, then splits each quad into 2 triangles.
//
// Input  → Output ratio per element:
//   1 hex element  → 6 quad faces → 12 triangles → 24 vertices
//   (Vertices are NOT shared across faces or across elements; see below.)
//
// Why duplicate vertices at element boundaries?
//   Two adjacent hex elements share an edge or face in the FEM sense, but
//   they may carry very different scalar values (e.g., a high-stress element
//   next to a low-stress one).  If vertices were shared, the GPU would
//   interpolate the per-vertex color smoothly across the boundary, which
//   would hide the discontinuity that exists in the underlying FEM result.
//   Duplicating all 24 vertices per element ensures each element displays a
//   flat, solid color that faithfully represents its own scalar result.
//   The same principle applies to face normals: sharing vertices would cause
//   smooth (Phong) shading, making all hex surfaces look rounded.  Flat-face
//   normals correctly convey the hard-edged solid geometry of a FEM brick mesh.
//
// Color mapping:
//   Before tessellation, min/max scalar values are computed across the entire
//   ScalarTable.  Each element's scalar is then normalised to [0,1] and looked
//   up in the ColorTable (a 256-entry cool-to-warm ramp).  The resulting RGB
//   is stored in all 24 vertices of that element, making the color uniform per
//   element on the GPU without any per-element uniform or instancing overhead.
//
// Thread safety:
//   TessellateHexMeshRange reads CoordTable, HexPrimitiveSet, ScalarTable, and
//   ColorTable — all read-only after construction.  Multiple threads may call
//   it simultaneously on non-overlapping element ranges (different [begin,end)
//   windows into the same HexPrimitiveSet) without any synchronisation.
//   StaticMesh::Build exploits this for parallel tessellation via ThreadPool.
// ============================================================================

// GPU vertex layout (matches shader attribute locations and Pipeline vertex
// input description in Pipeline.cpp):
//   location 0 — vec3 position  (byte offset  0, 12 bytes)
//   location 1 — vec3 normal    (byte offset 12, 12 bytes)
//   location 2 — vec3 color     (byte offset 24, 12 bytes)
//   total stride: 36 bytes
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;  // flat face normal; same for all 4 vertices of a quad face
    glm::vec3 color;   // RGB from ColorTable; same for all 24 vertices of an element
};

// Output of tessellation: a contiguous triangle list ready for direct upload to
// a device-local VkBuffer pair (vertex + index).
struct TessellatedMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t>   indices;
};

// Tessellates the element range [elementBegin, elementEnd) of the given hex set.
//
// minScalar / maxScalar must span the *full* ScalarTable (not just this range)
// so that color mapping is consistent when this function is called from multiple
// threads on different sub-ranges.  StaticMesh::Build pre-computes these once
// and passes the same values to every parallel chunk.
//
// Thread safety: safe to call concurrently on non-overlapping element ranges.
TessellatedMesh TessellateHexMeshRange(
    const CoordTable&      coords,
    const HexPrimitiveSet& hexSet,
    size_t                 elementBegin,
    size_t                 elementEnd,
    const ScalarTable&     scalars,
    const ColorTable&      colormap,
    float                  minScalar,
    float                  maxScalar
);

// Convenience overload that tessellates all elements and computes min/max
// internally.  Use this for single-threaded builds or when the element count
// is below the parallel threshold in StaticMesh::Build.
TessellatedMesh TessellateHexMesh(
    const CoordTable&      coords,
    const HexPrimitiveSet& hexSet,
    const ScalarTable&     scalars,
    const ColorTable&      colormap
);

} // namespace xcel
