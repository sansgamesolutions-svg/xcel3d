#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <vector>

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

// Constrains the element storage type: any sized range whose elements are
// convertible to uint32_t node indices.  Satisfied by std::array<uint32_t, N>
// (fixed-arity elements) and std::vector<uint32_t> (polylines).
template<typename T>
concept ElementConnectivity =
    std::ranges::sized_range<T> &&
    std::convertible_to<std::ranges::range_value_t<T>, uint32_t>;

// Generic element-set implementation.  All six concrete primitive types are
// instantiations of this template — no per-type boilerplate needed.
//
// value_type — the per-element storage type (replaces the per-class HexElement,
//              TetElement, etc. aliases); use MySet::value_type at call sites.
template<PrimitiveType PT, typename ElementT>
    requires ElementConnectivity<ElementT>
class PrimitiveSetImpl : public PrimitiveSet {
public:
    using value_type = ElementT;

    PrimitiveType Type()         const override { return PT; }
    size_t        ElementCount() const override { return m_elements.size(); }

    void                         AddElement(const ElementT& e) { m_elements.push_back(e); }
    const ElementT&              Element(size_t i)   const    { return m_elements[i]; }
    const std::vector<ElementT>& Elements()          const    { return m_elements; }

private:
    std::vector<ElementT> m_elements;
};

// ── Named aliases ──────────────────────────────────────────────────────────
//
//        7 ---- 6
//       /|     /|
//      4 ---- 5 |         Z
//      | 3 ---| 2         |  Y
//      |/     |/          | /
//      0 ---- 1           0 ---- X
//
// Nodes 0-3: bottom face (Z-); nodes 4-7: top face (Z+) directly above 0-3.
using HexPrimitiveSet      = PrimitiveSetImpl<PrimitiveType::PT_HEXAHEDRON,  std::array<uint32_t, 8>>;

//         3
//        /|\
//      0---+---2   VTK ordering: nodes 0-2 form the base; node 3 is the apex.
//        1
using TetPrimitiveSet      = PrimitiveSetImpl<PrimitiveType::PT_TETRAHEDRON, std::array<uint32_t, 4>>;

// 3--2  Nodes in CCW order when viewed from outward normal side.
// 0--1
using QuadPrimitiveSet     = PrimitiveSetImpl<PrimitiveType::PT_QUAD,        std::array<uint32_t, 4>>;

// Nodes in CCW order; useful when the importer has already triangulated.
using TrianglePrimitiveSet = PrimitiveSetImpl<PrimitiveType::PT_TRIANGLE,    std::array<uint32_t, 3>>;

// Beam / truss elements; rendered as world-space ribbon quads by MeshTessellator.
using LinePrimitiveSet     = PrimitiveSetImpl<PrimitiveType::PT_LINE,        std::array<uint32_t, 2>>;

// Variable-length ordered node sequence; each element spans (N-1) ribbon segments.
using PolylinePrimitiveSet = PrimitiveSetImpl<PrimitiveType::PT_POLYLINE,    std::vector<uint32_t>>;

} // namespace xcel
