#include "Graphics/PrimitiveSet.h"

namespace xcel {

// ── HexPrimitiveSet ───────────────────────────────────────────────────────────

struct HexPrimitiveSet::Impl { std::vector<HexElement> elements; };

HexPrimitiveSet::HexPrimitiveSet()  : m_impl(std::make_unique<Impl>()) {}
HexPrimitiveSet::~HexPrimitiveSet() = default;

PrimitiveType HexPrimitiveSet::Type()         const { return PrimitiveType::PT_HEXAHEDRON; }
size_t        HexPrimitiveSet::ElementCount() const { return m_impl->elements.size(); }

void HexPrimitiveSet::AddElement(const HexElement& e) { m_impl->elements.push_back(e); }

const HexPrimitiveSet::HexElement& HexPrimitiveSet::Element(size_t i) const {
    return m_impl->elements[i];
}
const std::vector<HexPrimitiveSet::HexElement>& HexPrimitiveSet::Elements() const {
    return m_impl->elements;
}

// ── TetPrimitiveSet ───────────────────────────────────────────────────────────

struct TetPrimitiveSet::Impl { std::vector<TetElement> elements; };

TetPrimitiveSet::TetPrimitiveSet()  : m_impl(std::make_unique<Impl>()) {}
TetPrimitiveSet::~TetPrimitiveSet() = default;

PrimitiveType TetPrimitiveSet::Type()         const { return PrimitiveType::PT_TETRAHEDRON; }
size_t        TetPrimitiveSet::ElementCount() const { return m_impl->elements.size(); }

void TetPrimitiveSet::AddElement(const TetElement& e) { m_impl->elements.push_back(e); }

const TetPrimitiveSet::TetElement& TetPrimitiveSet::Element(size_t i) const {
    return m_impl->elements[i];
}
const std::vector<TetPrimitiveSet::TetElement>& TetPrimitiveSet::Elements() const {
    return m_impl->elements;
}

// ── QuadPrimitiveSet ──────────────────────────────────────────────────────────

struct QuadPrimitiveSet::Impl { std::vector<QuadElement> elements; };

QuadPrimitiveSet::QuadPrimitiveSet()  : m_impl(std::make_unique<Impl>()) {}
QuadPrimitiveSet::~QuadPrimitiveSet() = default;

PrimitiveType QuadPrimitiveSet::Type()         const { return PrimitiveType::PT_QUAD; }
size_t        QuadPrimitiveSet::ElementCount() const { return m_impl->elements.size(); }

void QuadPrimitiveSet::AddElement(const QuadElement& e) { m_impl->elements.push_back(e); }

const QuadPrimitiveSet::QuadElement& QuadPrimitiveSet::Element(size_t i) const {
    return m_impl->elements[i];
}
const std::vector<QuadPrimitiveSet::QuadElement>& QuadPrimitiveSet::Elements() const {
    return m_impl->elements;
}

// ── TrianglePrimitiveSet ──────────────────────────────────────────────────────

struct TrianglePrimitiveSet::Impl { std::vector<Triangle> triangles; };

TrianglePrimitiveSet::TrianglePrimitiveSet()  : m_impl(std::make_unique<Impl>()) {}
TrianglePrimitiveSet::~TrianglePrimitiveSet() = default;

PrimitiveType TrianglePrimitiveSet::Type()         const { return PrimitiveType::PT_TRIANGLE; }
size_t        TrianglePrimitiveSet::ElementCount() const { return m_impl->triangles.size(); }

void TrianglePrimitiveSet::AddTriangle(const Triangle& t) { m_impl->triangles.push_back(t); }

const std::vector<TrianglePrimitiveSet::Triangle>& TrianglePrimitiveSet::Triangles() const {
    return m_impl->triangles;
}

// ── LinePrimitiveSet ──────────────────────────────────────────────────────────

struct LinePrimitiveSet::Impl { std::vector<LineElement> elements; };

LinePrimitiveSet::LinePrimitiveSet()  : m_impl(std::make_unique<Impl>()) {}
LinePrimitiveSet::~LinePrimitiveSet() = default;

PrimitiveType LinePrimitiveSet::Type()         const { return PrimitiveType::PT_LINE; }
size_t        LinePrimitiveSet::ElementCount() const { return m_impl->elements.size(); }

void LinePrimitiveSet::AddElement(const LineElement& e) { m_impl->elements.push_back(e); }

const LinePrimitiveSet::LineElement& LinePrimitiveSet::Element(size_t i) const {
    return m_impl->elements[i];
}
const std::vector<LinePrimitiveSet::LineElement>& LinePrimitiveSet::Elements() const {
    return m_impl->elements;
}

// ── PolylinePrimitiveSet ──────────────────────────────────────────────────────

struct PolylinePrimitiveSet::Impl { std::vector<Polyline> elements; };

PolylinePrimitiveSet::PolylinePrimitiveSet()  : m_impl(std::make_unique<Impl>()) {}
PolylinePrimitiveSet::~PolylinePrimitiveSet() = default;

PrimitiveType PolylinePrimitiveSet::Type()         const { return PrimitiveType::PT_POLYLINE; }
size_t        PolylinePrimitiveSet::ElementCount() const { return m_impl->elements.size(); }

void PolylinePrimitiveSet::AddElement(const Polyline& pl) { m_impl->elements.push_back(pl); }

const PolylinePrimitiveSet::Polyline& PolylinePrimitiveSet::Element(size_t i) const {
    return m_impl->elements[i];
}
const std::vector<PolylinePrimitiveSet::Polyline>& PolylinePrimitiveSet::Elements() const {
    return m_impl->elements;
}

} // namespace xcel
