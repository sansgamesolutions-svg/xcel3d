#include "Kernel/MeshTessellator.h"
#include <array>
#include <glm/glm.hpp>
#include <stdexcept>

namespace xcel {

// -- Internal helpers ----------------------------------------------------------

// Append a quad (4 coplanar vertices) as two CCW triangles to result.
// Winding: (0,1,2) + (0,2,3) — fan decomposition.
static void PushQuad(TessellatedMesh& out,
                     const glm::vec3 p[4],
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

// Append a triangle (3 vertices) as one CCW triangle to result.
static void PushTriangle(TessellatedMesh& out,
                         const glm::vec3 p[3],
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

// Compute a world-space ribbon quad for the line segment [P0, P1] and push it.
// The ribbon is perpendicular to the segment direction, oriented using a global
// up-vector heuristic. Width is 2 * kLineHalfWidth in world space.
static void PushLineRibbon(TessellatedMesh& out,
                            const glm::vec3& p0,
                            const glm::vec3& p1,
                            const glm::vec3& rgb)
{
    glm::vec3 dir = p1 - p0;
    float len = glm::length(dir);
    if (len < 1e-7f) return; // degenerate segment — skip
    dir /= len;

    glm::vec3 up = (glm::abs(glm::dot(dir, glm::vec3(0.f, 1.f, 0.f))) < 0.99f)
                 ? glm::vec3(0.f, 1.f, 0.f)
                 : glm::vec3(1.f, 0.f, 0.f);

    glm::vec3 right  = glm::normalize(glm::cross(dir, up));
    glm::vec3 normal = glm::normalize(glm::cross(right, dir));

    glm::vec3 offset = right * kLineHalfWidth;
    glm::vec3 quad[4] = {
        p0 - offset,  // v0 — left  at P0
        p0 + offset,  // v1 — right at P0
        p1 + offset,  // v2 — right at P1
        p1 - offset,  // v3 — left  at P1
    };
    PushQuad(out, quad, normal, rgb);
}

// -- Per-type tessellators -----------------------------------------------------

// VTK hex face table (VTK_HEXAHEDRON, cell type 12).
// Each face is a CCW quad viewed from outside the hex; normal = cross(v1-v0, v3-v0).
//
//        7 ---- 6
//       /|     /|         nodes 0-3: bottom face (Z-)
//      4 ---- 5 |         nodes 4-7: top face    (Z+), directly above 0-3
//      | 3 ---| 2
//      |/     |/
//      0 ---- 1
static constexpr std::array<std::array<int, 4>, 6> kHexFaces = {{
    {0, 3, 2, 1},   // bottom  (Z-)
    {4, 5, 6, 7},   // top     (Z+)
    {0, 1, 5, 4},   // front   (Y-)
    {3, 7, 6, 2},   // back    (Y+)
    {0, 4, 7, 3},   // left    (X-)
    {1, 2, 6, 5},   // right   (X+)
}};

static TessellatedMesh TessellateHex(
    const HexPrimitiveSet& ps,
    const CoordTable&      coords,
    const ScalarTable&     scalars,
    const ColorTable&      colormap,
    size_t                 begin,
    size_t                 end,
    float                  minS,
    float                  maxS)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 24);
    result.indices.reserve((end - begin) * 36);

    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        glm::vec3 rgb = colormap.ColorForElement(e, scalars[e], minS, maxS);

        for (const auto& face : kHexFaces) {
            glm::vec3 p[4];
            for (int k = 0; k < 4; ++k)
                p[k] = coords[elem[face[k]]];

            glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[3] - p[0]));
            PushQuad(result, p, normal, rgb);
        }
    }
    return result;
}

// VTK tet face table (VTK_TETRA, cell type 10).
// Outward-pointing normals via CCW winding, verified against vtkTetra.cxx.
static constexpr std::array<std::array<int, 3>, 4> kTetFaces = {{
    {0, 2, 1},  // base (opposite to node 3)
    {0, 1, 3},
    {1, 2, 3},
    {2, 0, 3},
}};

static TessellatedMesh TessellateTetra(
    const TetPrimitiveSet& ps,
    const CoordTable&      coords,
    const ScalarTable&     scalars,
    const ColorTable&      colormap,
    size_t                 begin,
    size_t                 end,
    float                  minS,
    float                  maxS)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 12);
    result.indices.reserve((end - begin) * 12);

    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        glm::vec3 rgb = colormap.ColorForElement(e, scalars[e], minS, maxS);

        for (const auto& face : kTetFaces) {
            glm::vec3 p[3] = {
                coords[elem[face[0]]],
                coords[elem[face[1]]],
                coords[elem[face[2]]],
            };
            glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[2] - p[0]));
            PushTriangle(result, p, normal, rgb);
        }
    }
    return result;
}

static TessellatedMesh TessellateQuads(
    const QuadPrimitiveSet& ps,
    const CoordTable&       coords,
    const ScalarTable&      scalars,
    const ColorTable&       colormap,
    size_t                  begin,
    size_t                  end,
    float                   minS,
    float                   maxS)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 4);
    result.indices.reserve((end - begin) * 6);

    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        glm::vec3 p[4];
        for (int k = 0; k < 4; ++k)
            p[k] = coords[elem[k]];

        glm::vec3 rgb    = colormap.ColorForElement(e, scalars[e], minS, maxS);
        glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[3] - p[0]));
        PushQuad(result, p, normal, rgb);
    }
    return result;
}

static TessellatedMesh TessellateTriangles(
    const TrianglePrimitiveSet& ps,
    const CoordTable&           coords,
    const ScalarTable&          scalars,
    const ColorTable&           colormap,
    size_t                      begin,
    size_t                      end,
    float                       minS,
    float                       maxS)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 3);
    result.indices.reserve((end - begin) * 3);

    for (size_t e = begin; e < end; ++e) {
        const auto& tri = ps.Element(e);
        glm::vec3 p[3] = { coords[tri[0]], coords[tri[1]], coords[tri[2]] };
        glm::vec3 rgb    = colormap.ColorForElement(e, scalars[e], minS, maxS);
        glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[2] - p[0]));
        PushTriangle(result, p, normal, rgb);
    }
    return result;
}

static TessellatedMesh TessellateLines(
    const LinePrimitiveSet& ps,
    const CoordTable&       coords,
    const ScalarTable&      scalars,
    const ColorTable&       colormap,
    size_t                  begin,
    size_t                  end,
    float                   minS,
    float                   maxS)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 4);
    result.indices.reserve((end - begin) * 6);

    for (size_t e = begin; e < end; ++e) {
        const auto& seg = ps.Element(e);
        glm::vec3 rgb   = colormap.ColorForElement(e, scalars[e], minS, maxS);
        PushLineRibbon(result, coords[seg[0]], coords[seg[1]], rgb);
    }
    return result;
}

static TessellatedMesh TessellatePolylines(
    const PolylinePrimitiveSet& ps,
    const CoordTable&           coords,
    const ScalarTable&          scalars,
    const ColorTable&           colormap,
    size_t                      begin,
    size_t                      end,
    float                       minS,
    float                       maxS)
{
    TessellatedMesh result;

    for (size_t e = begin; e < end; ++e) {
        const auto& pl  = ps.Element(e);
        glm::vec3   rgb = colormap.ColorForElement(e, scalars[e], minS, maxS);
        for (size_t i = 0; i + 1 < pl.size(); ++i)
            PushLineRibbon(result, coords[pl[i]], coords[pl[i + 1]], rgb);
    }
    return result;
}

// -- Public dispatch -----------------------------------------------------------

TessellatedMesh TessellateRange(
    const PrimitiveSet& ps,
    const CoordTable&   coords,
    const ScalarTable&  scalars,
    const ColorTable&   colormap,
    size_t              elementBegin,
    size_t              elementEnd,
    float               minScalar,
    float               maxScalar)
{
    switch (ps.Type()) {
    case PrimitiveType::PT_HEXAHEDRON:
        return TessellateHex(
            static_cast<const HexPrimitiveSet&>(ps),
            coords, scalars, colormap,
            elementBegin, elementEnd,
            minScalar, maxScalar);

    case PrimitiveType::PT_TETRAHEDRON:
        return TessellateTetra(
            static_cast<const TetPrimitiveSet&>(ps),
            coords, scalars, colormap,
            elementBegin, elementEnd,
            minScalar, maxScalar);

    case PrimitiveType::PT_QUAD:
        return TessellateQuads(
            static_cast<const QuadPrimitiveSet&>(ps),
            coords, scalars, colormap,
            elementBegin, elementEnd,
            minScalar, maxScalar);

    case PrimitiveType::PT_TRIANGLE:
        return TessellateTriangles(
            static_cast<const TrianglePrimitiveSet&>(ps),
            coords, scalars, colormap,
            elementBegin, elementEnd,
            minScalar, maxScalar);

    case PrimitiveType::PT_LINE:
        return TessellateLines(
            static_cast<const LinePrimitiveSet&>(ps),
            coords, scalars, colormap,
            elementBegin, elementEnd,
            minScalar, maxScalar);

    case PrimitiveType::PT_POLYLINE:
        return TessellatePolylines(
            static_cast<const PolylinePrimitiveSet&>(ps),
            coords, scalars, colormap,
            elementBegin, elementEnd,
            minScalar, maxScalar);
    }

    throw std::runtime_error("TessellateRange: unhandled PrimitiveType");
}

TessellatedMesh Tessellate(
    const PrimitiveSet& ps,
    const CoordTable&   coords,
    const ScalarTable&  scalars,
    const ColorTable&   colormap)
{
    return TessellateRange(
        ps, coords, scalars, colormap,
        0, ps.ElementCount(),
        scalars.MinValue(), scalars.MaxValue());
}

} // namespace xcel
