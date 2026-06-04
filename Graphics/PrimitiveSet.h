#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <memory>

namespace xcel {

// Identifies the geometric primitive family of an element set.
// These map directly to VTK cell types used in FEM export files:
//   PT_LINE        → VTK_LINE (type 3)          — 1-D beam / truss element
//   PT_POLYLINE    → VTK_POLY_LINE (type 4)     — connected line strip
//   PT_TRIANGLE    → VTK_TRIANGLE (type 5)      — 2-D shell / surface element
//   PT_QUAD        → VTK_QUAD (type 9)           — 4-node quadrilateral shell
//   PT_TETRAHEDRON → VTK_TETRA (type 10)         — 4-node solid tet element
//   PT_HEXAHEDRON  → VTK_HEXAHEDRON (type 12)   — 8-node solid brick element
enum class PrimitiveType {
    PT_LINE,
    PT_POLYLINE,
    PT_TRIANGLE,
    PT_QUAD,
    PT_TETRAHEDRON,
    PT_HEXAHEDRON
};

// Abstract base for a homogeneous collection of elements of the same type.
// A Mesh can hold several PrimitiveSets (e.g., solids + shells in the same model).
// MeshTessellator dispatches on Type() to pick the correct tessellation algorithm.
class PrimitiveSet {
public:
    virtual ~PrimitiveSet() = default;
    virtual PrimitiveType Type()         const = 0;
    virtual size_t        ElementCount() const = 0;
};

// ── 8-node hexahedral solid (VTK_HEXAHEDRON, type 12) ────────────────────────
//
//        7 ---- 6
//       /|     /|
//      4 ---- 5 |         Z
//      | 3 ---| 2         |  Y
//      |/     |/          | /
//      0 ---- 1           0 ---- X
//
// Nodes 0-3: bottom face (Z-); nodes 4-7: top face (Z+) directly above 0-3.
class HexPrimitiveSet : public PrimitiveSet {
public:
    using HexElement = std::array<uint32_t, 8>;

    HexPrimitiveSet();
    ~HexPrimitiveSet() override;

    PrimitiveType Type()         const override;
    size_t        ElementCount() const override;

    void                           AddElement(const HexElement& e);
    const HexElement&              Element(size_t i) const;
    const std::vector<HexElement>& Elements()        const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ── 4-node tetrahedron (VTK_TETRA, type 10) ──────────────────────────────────
//
//         3
//        /|\
//       / | \
//      /  |  \
//     0---+---2
//      \  |  /
//       \ | /
//        \|/
//         1
//
// VTK canonical ordering: nodes 0-2 form the base; node 3 is the apex.
class TetPrimitiveSet : public PrimitiveSet {
public:
    using TetElement = std::array<uint32_t, 4>;

    TetPrimitiveSet();
    ~TetPrimitiveSet() override;

    PrimitiveType Type()         const override;
    size_t        ElementCount() const override;

    void                           AddElement(const TetElement& e);
    const TetElement&              Element(size_t i) const;
    const std::vector<TetElement>& Elements()        const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ── 4-node quadrilateral shell (VTK_QUAD, type 9) ────────────────────────────
//
//   3 ---- 2
//   |      |    Normal points out of the page when nodes are CCW.
//   |      |
//   0 ---- 1
//
// Nodes 0-3 in CCW order when viewed from the outward side.
class QuadPrimitiveSet : public PrimitiveSet {
public:
    using QuadElement = std::array<uint32_t, 4>;

    QuadPrimitiveSet();
    ~QuadPrimitiveSet() override;

    PrimitiveType Type()         const override;
    size_t        ElementCount() const override;

    void                            AddElement(const QuadElement& e);
    const QuadElement&              Element(size_t i) const;
    const std::vector<QuadElement>& Elements()        const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ── Pre-tessellated triangles (VTK_TRIANGLE, type 5) ─────────────────────────
//
// Useful when the importer has already triangulated the surface.
// Each triangle references 3 CoordTable nodes in CCW order.
class TrianglePrimitiveSet : public PrimitiveSet {
public:
    using Triangle = std::array<uint32_t, 3>;

    TrianglePrimitiveSet();
    ~TrianglePrimitiveSet() override;

    PrimitiveType Type()         const override;
    size_t        ElementCount() const override;

    void                         AddTriangle(const Triangle& t);
    const std::vector<Triangle>& Triangles() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ── 2-node line segment (VTK_LINE, type 3) ───────────────────────────────────
//
// Beam and truss elements. Rendered as world-space ribbon quads
// (2 triangles per segment) by MeshTessellator.
class LinePrimitiveSet : public PrimitiveSet {
public:
    using LineElement = std::array<uint32_t, 2>;

    LinePrimitiveSet();
    ~LinePrimitiveSet() override;

    PrimitiveType Type()         const override;
    size_t        ElementCount() const override;

    void                            AddElement(const LineElement& e);
    const LineElement&              Element(size_t i) const;
    const std::vector<LineElement>& Elements()        const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ── Variable-length connected line strip (VTK_POLY_LINE, type 4) ─────────────
//
// Each element is an ordered sequence of node indices forming a connected path.
// MeshTessellator generates one ribbon quad per consecutive node pair.
// One scalar value maps to the entire polyline (uniform color per element).
class PolylinePrimitiveSet : public PrimitiveSet {
public:
    using Polyline = std::vector<uint32_t>;

    PolylinePrimitiveSet();
    ~PolylinePrimitiveSet() override;

    PrimitiveType Type()         const override;
    size_t        ElementCount() const override;   // number of polylines

    void                          AddElement(const Polyline& pl);
    const Polyline&               Element(size_t i) const;
    const std::vector<Polyline>&  Elements()        const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
