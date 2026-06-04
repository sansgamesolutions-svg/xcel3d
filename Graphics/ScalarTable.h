#pragma once
#include <vector>
#include <cstddef>
#include <memory>

namespace xcel {

// Stores one scalar value per FEM *element* (not per node).
//
// In a typical CAE result file (e.g., a VTK .vtu output) a field such as von
// Mises stress, temperature, or displacement magnitude is stored element-wise:
// every hex cell carries a single representative value for that cell.  This
// table holds those values in element order so that scalars[i] corresponds to
// the i-th element in the matching HexPrimitiveSet.
//
// Coloring: the tessellator reads scalars[elementIndex], normalises the value
// against the global [MinValue, MaxValue] range, and maps it through a ColorTable
// to produce a per-vertex RGB color that is baked into the GPU vertex buffer.
// Because the scalar is the same for every vertex of a given element, the result
// is a solid, flat-shaded color per element — which is the conventional display
// style for element-level FEM results.
//
// Index parity: ScalarTable and HexPrimitiveSet must contain the same number of
// entries; scalars[i] is the result value for hexSet.Element(i).
class ScalarTable {
public:
    ScalarTable();
    explicit ScalarTable(std::vector<float> scalars);
    ~ScalarTable();

    void   AddScalar(float v);
    size_t Size() const;
    float  operator[](size_t i) const;
    const std::vector<float>& Data() const;

    float MinValue() const;
    float MaxValue() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
