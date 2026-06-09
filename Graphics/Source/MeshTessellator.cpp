#include "Graphics/MeshTessellator.h"
#include "Graphics/TessellationStrategy.h"
#include "Common/ThreadPool.h"
#include <algorithm>
#include <array>
#include <future>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <stdexcept>
#include <vector>

namespace xcel {

using detail::PushQuad;
using detail::PushTriangle;

// -- Internal helpers ----------------------------------------------------------

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
    if (len < 1e-7f) return;
    dir /= len;

    glm::vec3 up = (glm::abs(glm::dot(dir, glm::vec3(0.f, 1.f, 0.f))) < 0.99f)
                 ? glm::vec3(0.f, 1.f, 0.f)
                 : glm::vec3(1.f, 0.f, 0.f);

    glm::vec3 right  = glm::normalize(glm::cross(dir, up));
    glm::vec3 normal = glm::normalize(glm::cross(right, dir));

    glm::vec3 offset = right * kLineHalfWidth;
    glm::vec3 quad[4] = {
        p0 - offset,
        p0 + offset,
        p1 + offset,
        p1 - offset,
    };
    PushQuad(out, quad, normal, rgb);
}

// -- Per-type tessellators -----------------------------------------------------

static TessellatedMesh TessellateHex(
    const HexPrimitiveSet& ps,
    const CoordTable&      coords,
    const ScalarTable&     scalars,
    const ColorTable&      colormap,
    size_t begin, size_t end, float minS, float maxS, size_t scalarOffset)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 24);
    result.indices.reserve((end - begin) * 36);

    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        const size_t scalarIndex = scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);

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

static TessellatedMesh TessellateTetra(
    const TetPrimitiveSet& ps,
    const CoordTable&      coords,
    const ScalarTable&     scalars,
    const ColorTable&      colormap,
    size_t begin, size_t end, float minS, float maxS, size_t scalarOffset)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 12);
    result.indices.reserve((end - begin) * 12);

    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        const size_t scalarIndex = scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);

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
    size_t begin, size_t end, float minS, float maxS, size_t scalarOffset)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 4);
    result.indices.reserve((end - begin) * 6);

    for (size_t e = begin; e < end; ++e) {
        const auto& elem = ps.Element(e);
        glm::vec3 p[4];
        for (int k = 0; k < 4; ++k)
            p[k] = coords[elem[k]];
        const size_t scalarIndex = scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);
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
    size_t begin, size_t end, float minS, float maxS, size_t scalarOffset)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 3);
    result.indices.reserve((end - begin) * 3);

    for (size_t e = begin; e < end; ++e) {
        const auto& tri = ps.Element(e);
        glm::vec3 p[3]  = { coords[tri[0]], coords[tri[1]], coords[tri[2]] };
        const size_t scalarIndex = scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);
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
    size_t begin, size_t end, float minS, float maxS, size_t scalarOffset)
{
    TessellatedMesh result;
    result.vertices.reserve((end - begin) * 4);
    result.indices.reserve((end - begin) * 6);

    for (size_t e = begin; e < end; ++e) {
        const auto& seg = ps.Element(e);
        const size_t scalarIndex = scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);
        PushLineRibbon(result, coords[seg[0]], coords[seg[1]], rgb);
    }
    return result;
}

static TessellatedMesh TessellatePolylines(
    const PolylinePrimitiveSet& ps,
    const CoordTable&           coords,
    const ScalarTable&          scalars,
    const ColorTable&           colormap,
    size_t begin, size_t end, float minS, float maxS, size_t scalarOffset)
{
    TessellatedMesh result;

    for (size_t e = begin; e < end; ++e) {
        const auto& pl  = ps.Element(e);
        const size_t scalarIndex = scalarOffset + e;
        glm::vec3 rgb = colormap.ColorForElement(
            scalarIndex, scalars[scalarIndex], minS, maxS);
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
    float               maxScalar,
    size_t              scalarOffset)
{
    switch (ps.Type()) {
    case PrimitiveType::PT_HEXAHEDRON:
        return TessellateHex(static_cast<const HexPrimitiveSet&>(ps),
            coords, scalars, colormap, elementBegin, elementEnd,
            minScalar, maxScalar, scalarOffset);
    case PrimitiveType::PT_TETRAHEDRON:
        return TessellateTetra(static_cast<const TetPrimitiveSet&>(ps),
            coords, scalars, colormap, elementBegin, elementEnd,
            minScalar, maxScalar, scalarOffset);
    case PrimitiveType::PT_QUAD:
        return TessellateQuads(static_cast<const QuadPrimitiveSet&>(ps),
            coords, scalars, colormap, elementBegin, elementEnd,
            minScalar, maxScalar, scalarOffset);
    case PrimitiveType::PT_TRIANGLE:
        return TessellateTriangles(static_cast<const TrianglePrimitiveSet&>(ps),
            coords, scalars, colormap, elementBegin, elementEnd,
            minScalar, maxScalar, scalarOffset);
    case PrimitiveType::PT_LINE:
        return TessellateLines(static_cast<const LinePrimitiveSet&>(ps),
            coords, scalars, colormap, elementBegin, elementEnd,
            minScalar, maxScalar, scalarOffset);
    case PrimitiveType::PT_POLYLINE:
        return TessellatePolylines(static_cast<const PolylinePrimitiveSet&>(ps),
            coords, scalars, colormap, elementBegin, elementEnd,
            minScalar, maxScalar, scalarOffset);
    }
    throw std::runtime_error("TessellateRange: unhandled PrimitiveType");
}

TessellatedMesh Tessellate(
    const PrimitiveSet& ps,
    const CoordTable&   coords,
    const ScalarTable&  scalars,
    const ColorTable&   colormap)
{
    return TessellateRange(ps, coords, scalars, colormap,
        0, ps.ElementCount(), scalars.MinValue(), scalars.MaxValue(), 0);
}

// -- Strategy-based engine ----------------------------------------------------

static constexpr size_t kParallelThreshold = 1'024;

static void ApplyTransform(TessellatedMesh& mesh, const glm::mat4& transform)
{
    const glm::mat3 linear(transform);
    const float determinant = glm::determinant(linear);
    const glm::mat3 normalMatrix = glm::abs(determinant) > 1e-8f
        ? glm::inverseTranspose(linear)
        : linear;

    for (auto& vertex : mesh.vertices) {
        vertex.position = glm::vec3(transform * glm::vec4(vertex.position, 1.f));
        const glm::vec3 transformedNormal = normalMatrix * vertex.normal;
        const float normalLength = glm::length(transformedNormal);
        if (normalLength > 1e-8f)
            vertex.normal = transformedNormal / normalLength;
    }
}

TessellatedMesh TessellateInput(const MeshTessellationInput& inp, ThreadPool* pool)
{
    if (!inp.primitiveSet || !inp.coords || !inp.scalars || !inp.colorTable)
        throw std::invalid_argument("TessellateInput: incomplete input");

    static const AllFacesStrategy defaultStrategy;
    const ITessellationStrategy& s = inp.strategy ? *inp.strategy : defaultStrategy;

    float  minS = inp.scalars->MinValue();
    float  maxS = inp.scalars->MaxValue();
    size_t N    = inp.primitiveSet->ElementCount();
    if (inp.scalarOffset > inp.scalars->Size()
        || N > inp.scalars->Size() - inp.scalarOffset)
        throw std::out_of_range("TessellateInput: scalar table is too small");

    if (pool && N > kParallelThreshold && s.IsParallelizable()) {
        size_t T         = pool->ThreadCount();
        size_t chunkSize = (N + T - 1) / T;

        std::vector<std::future<TessellatedMesh>> futures;
        futures.reserve(T);

        for (size_t t = 0; t < T; ++t) {
            size_t begin = t * chunkSize;
            size_t end   = std::min(begin + chunkSize, N);
            if (begin >= N) break;
            futures.push_back(pool->Submit([&inp, &s, begin, end, minS, maxS] {
                return s.TessellateRange(inp, begin, end, minS, maxS);
            }));
        }

        TessellatedMesh part;
        for (auto& f : futures) {
            auto partial = f.get();
            auto base    = static_cast<uint32_t>(part.vertices.size());
            for (auto idx : partial.indices)
                part.indices.push_back(base + idx);
            part.vertices.insert(part.vertices.end(),
                                  partial.vertices.begin(), partial.vertices.end());
        }
        ApplyTransform(part, inp.transform);
        return part;
    }

    auto result = s.TessellateRange(inp, 0, N, minS, maxS);
    ApplyTransform(result, inp.transform);
    return result;
}

TessellatedMesh TessellateAndMerge(std::span<const MeshTessellationInput> inputs,
                                    ThreadPool* pool)
{
    TessellatedMesh combined;
    for (const auto& inp : inputs) {
        if (!inp.primitiveSet || !inp.coords || !inp.scalars || !inp.colorTable)
            continue;
        auto     part = TessellateInput(inp, pool);
        uint32_t base = static_cast<uint32_t>(combined.vertices.size());
        for (auto idx : part.indices)
            combined.indices.push_back(base + idx);
        combined.vertices.insert(combined.vertices.end(),
                                  part.vertices.begin(), part.vertices.end());
    }
    return combined;
}

} // namespace xcel
