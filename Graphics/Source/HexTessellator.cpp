#include "Graphics/HexTessellator.h"
#include <glm/glm.hpp>

namespace xcel {

// VTK canonical hex node numbering (VTK_HEXAHEDRON, cell type 12).
// Looking at the element from outside:
//
//        7 ---- 6
//       /|     /|         nodes 0-3: bottom face (Z-)
//      4 ---- 5 |         nodes 4-7: top face    (Z+), directly above 0-3
//      | 3 ---| 2
//      |/     |/
//      0 ---- 1
//
// This is the standard ordering used by VTK, ParaView, Abaqus C3D8, and most
// FEM solvers when writing .vtu / .vtk output files.  An importer reading one
// of those files stores the 8 node indices per element in exactly this order.
//
// Each face below is listed as a CCW quad when viewed from *outside* the hex.
// The face normal is then outward-pointing: normalize(cross(v1-v0, v3-v0)).
static constexpr std::array<std::array<int, 4>, 6> kHexFaces = {{
    {0, 3, 2, 1},   // bottom  (Z-): CCW when looking up   from below
    {4, 5, 6, 7},   // top     (Z+): CCW when looking down from above
    {0, 1, 5, 4},   // front   (Y-): CCW when looking toward +Y
    {3, 7, 6, 2},   // back    (Y+): CCW when looking toward -Y
    {0, 4, 7, 3},   // left    (X-): CCW when looking toward +X
    {1, 2, 6, 5},   // right   (X+): CCW when looking toward -X
}};

TessellatedMesh TessellateHexMeshRange(
    const CoordTable&      coords,
    const HexPrimitiveSet& hexSet,
    size_t                 elementBegin,
    size_t                 elementEnd,
    const ScalarTable&     scalars,
    const ColorTable&      colormap,
    float                  minScalar,
    float                  maxScalar)
{
    TessellatedMesh result;

    // Pre-allocate exactly the right capacity to avoid any reallocation during
    // the inner loop.  Each element produces 6 faces × 4 verts = 24 vertices
    // and 6 faces × 2 triangles × 3 indices = 36 indices.
    size_t numElements = elementEnd - elementBegin;
    result.vertices.reserve(numElements * 24);
    result.indices.reserve(numElements * 36);

    for (size_t e = elementBegin; e < elementEnd; ++e) {
        // elem holds 8 CoordTable indices in VTK canonical order (see diagram above).
        const auto& elem = hexSet.Element(e);

        // Map this element's scalar result to an RGB color using the global
        // [minScalar, maxScalar] range so that the color scale is uniform across
        // all elements, regardless of which thread or chunk processed them.
        glm::vec3 rgb = colormap.MapScalar(scalars[e], minScalar, maxScalar);

        // Iterate over all 6 quad faces.  Each face is described by 4 local node
        // indices (into elem[]) that form a CCW quad when viewed from outside.
        for (const auto& face : kHexFaces) {
            // Resolve the 4 quad corner positions from the shared CoordTable.
            glm::vec3 p[4];
            for (int k = 0; k < 4; ++k)
                p[k] = coords[elem[face[k]]];

            // Compute the outward face normal as the cross product of two edges
            // of the quad.  Using p[1]-p[0] and p[3]-p[0] (not p[2]-p[0]) keeps
            // the normal consistent with the CCW winding regardless of how planar
            // the face is.  All 4 vertices of this face share the same normal,
            // giving flat (faceted) shading that clearly shows element boundaries.
            glm::vec3 normal = glm::normalize(
                glm::cross(p[1] - p[0], p[3] - p[0])
            );

            // Record the starting index for the 4 new vertices we are about to add.
            uint32_t base = static_cast<uint32_t>(result.vertices.size());

            // Push all 4 quad vertices.  Position, normal, and color are the same
            // for every vertex of this face (flat shading, uniform element color).
            for (int k = 0; k < 4; ++k)
                result.vertices.push_back({p[k], normal, rgb});

            // Split the quad into two triangles using the indices relative to base:
            //   triangle 1: (base+0, base+1, base+2)
            //   triangle 2: (base+0, base+2, base+3)
            // This fan decomposition preserves CCW winding for both triangles.
            result.indices.push_back(base + 0);
            result.indices.push_back(base + 1);
            result.indices.push_back(base + 2);
            result.indices.push_back(base + 0);
            result.indices.push_back(base + 2);
            result.indices.push_back(base + 3);
        }
    }

    return result;
}

// Single-threaded convenience wrapper.  Computes the global scalar range from
// the full ScalarTable and delegates to TessellateHexMeshRange.
TessellatedMesh TessellateHexMesh(
    const CoordTable&      coords,
    const HexPrimitiveSet& hexSet,
    const ScalarTable&     scalars,
    const ColorTable&      colormap)
{
    return TessellateHexMeshRange(
        coords, hexSet,
        0, hexSet.ElementCount(),
        scalars, colormap,
        scalars.MinValue(), scalars.MaxValue()
    );
}

} // namespace xcel
