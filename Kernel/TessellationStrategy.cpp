#include "Kernel/TessellationStrategy.h"
#include "Kernel/MeshTessellator.h"
#include <algorithm>
#include <array>
#include <unordered_map>

namespace xcel {

// ── AllFacesStrategy ──────────────────────────────────────────────────────────

TessellatedMesh AllFacesStrategy::TessellateRange(
    const MeshTessellationInput& inp,
    size_t begin, size_t end, float minS, float maxS) const
{
    return xcel::TessellateRange(*inp.primitiveSet, *inp.coords, *inp.scalars,
                                  *inp.colorTable, begin, end, minS, maxS,
                                  inp.scalarOffset);
}

// ── WettedFacesStrategy ───────────────────────────────────────────────────────
// Exterior-face detection via face adjacency counting.
//
// Algorithm (HEX):
//   1. For every element in the full PrimitiveSet, enumerate its 6 quad faces.
//      Represent each face as a sorted 4-tuple of node IDs (canonical key).
//   2. Count how many times each key appears — faces shared by two elements
//      appear twice; boundary faces appear once.
//   3. For each element in [begin, end), emit only the faces with count == 1.
//
// TET uses the same algorithm with its 4 triangular faces.
// 2-D element types (QUAD, TRI, LINE, POLYLINE) are always boundary elements,
// so they fall through to AllFacesStrategy unchanged.

namespace {

struct HexFaceKey {
    std::array<uint32_t, 4> nodes;
    bool operator==(const HexFaceKey& o) const noexcept { return nodes == o.nodes; }
};

struct HexFaceKeyHash {
    size_t operator()(const HexFaceKey& k) const noexcept {
        size_t h = 0;
        for (auto n : k.nodes) h = h * 2654435761u + n;
        return h;
    }
};

struct TetFaceKey {
    std::array<uint32_t, 3> nodes;
    bool operator==(const TetFaceKey& o) const noexcept { return nodes == o.nodes; }
};

struct TetFaceKeyHash {
    size_t operator()(const TetFaceKey& k) const noexcept {
        size_t h = 0;
        for (auto n : k.nodes) h = h * 2654435761u + n;
        return h;
    }
};

TessellatedMesh WettedHex(const MeshTessellationInput& inp,
                           size_t begin, size_t end, float minS, float maxS)
{
    const auto& ps     = static_cast<const HexPrimitiveSet&>(*inp.primitiveSet);
    const auto& coords = *inp.coords;
    const auto& scalars = *inp.scalars;
    const auto& colormap = *inp.colorTable;

    size_t N = ps.ElementCount();

    // Build face count over the full element set.
    std::unordered_map<HexFaceKey, int, HexFaceKeyHash> faceCount;
    faceCount.reserve(N * 6);

    for (size_t e = 0; e < N; ++e) {
        const auto& elem = ps.Element(e);
        for (const auto& face : kHexFaces) {
            HexFaceKey key;
            for (int k = 0; k < 4; ++k) key.nodes[k] = elem[face[k]];
            std::sort(key.nodes.begin(), key.nodes.end());
            faceCount[key]++;
        }
    }

    // Emit only exterior faces for elements in [begin, end).
    TessellatedMesh result;
    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        const size_t scalarIndex = inp.scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);

        for (const auto& face : kHexFaces) {
            HexFaceKey key;
            for (int k = 0; k < 4; ++k) key.nodes[k] = elem[face[k]];
            std::sort(key.nodes.begin(), key.nodes.end());

            if (faceCount[key] == 1) {
                glm::vec3 p[4];
                for (int k = 0; k < 4; ++k)
                    p[k] = coords[elem[face[k]]];
                glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[3] - p[0]));
                detail::PushQuad(result, p, normal, rgb);
            }
        }
    }
    return result;
}

TessellatedMesh WettedTet(const MeshTessellationInput& inp,
                           size_t begin, size_t end, float minS, float maxS)
{
    const auto& ps      = static_cast<const TetPrimitiveSet&>(*inp.primitiveSet);
    const auto& coords  = *inp.coords;
    const auto& scalars = *inp.scalars;
    const auto& colormap = *inp.colorTable;

    size_t N = ps.ElementCount();

    std::unordered_map<TetFaceKey, int, TetFaceKeyHash> faceCount;
    faceCount.reserve(N * 4);

    for (size_t e = 0; e < N; ++e) {
        const auto& elem = ps.Element(e);
        for (const auto& face : kTetFaces) {
            TetFaceKey key;
            for (int k = 0; k < 3; ++k) key.nodes[k] = elem[face[k]];
            std::sort(key.nodes.begin(), key.nodes.end());
            faceCount[key]++;
        }
    }

    TessellatedMesh result;
    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        const size_t scalarIndex = inp.scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);

        for (const auto& face : kTetFaces) {
            TetFaceKey key;
            for (int k = 0; k < 3; ++k) key.nodes[k] = elem[face[k]];
            std::sort(key.nodes.begin(), key.nodes.end());

            if (faceCount[key] == 1) {
                glm::vec3 p[3] = {
                    coords[elem[face[0]]],
                    coords[elem[face[1]]],
                    coords[elem[face[2]]],
                };
                glm::vec3 normal = glm::normalize(glm::cross(p[1] - p[0], p[2] - p[0]));
                detail::PushTriangle(result, p, normal, rgb);
            }
        }
    }
    return result;
}

} // anonymous namespace

TessellatedMesh WettedFacesStrategy::TessellateRange(
    const MeshTessellationInput& inp,
    size_t begin, size_t end, float minS, float maxS) const
{
    switch (inp.primitiveSet->Type()) {
    case PrimitiveType::PT_HEXAHEDRON:
        return WettedHex(inp, begin, end, minS, maxS);
    case PrimitiveType::PT_TETRAHEDRON:
        return WettedTet(inp, begin, end, minS, maxS);
    default:
        // Surface elements are inherently exterior — AllFaces is correct.
        return AllFacesStrategy{}.TessellateRange(inp, begin, end, minS, maxS);
    }
}

} // namespace xcel
