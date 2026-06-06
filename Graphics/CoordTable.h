#pragma once
#include <vector>
#include <cstddef>
#include <memory>
#include <glm/glm.hpp>

namespace xcel {

// Stores the 3-D positions of every FEM mesh node (also called "grid points" or
// "vertices" in VTK terminology).
//
// In VTK/FEM terminology a *node* is a geometric point in space; it is shared by
// all elements that touch it.  The CoordTable therefore acts as the single source
// of truth for node positions — both HexPrimitiveSet elements and any future
// element types index into this table.
//
// Index semantics: CoordTable[i] is the world-space position of node i.
// An element stores its 8 corner node indices (see HexPrimitiveSet); the
// tessellator dereferences those indices here to obtain actual coordinates.
//
// Sharing: CoordTable is held by shared_ptr inside Mesh so multiple PrimitiveSets
// (and potentially multiple meshes representing the same geometry at different
// time steps) can reference the same node data without copying.
class CoordTable {
public:
    CoordTable();
    explicit CoordTable(std::vector<glm::vec3> positions);
    ~CoordTable();

    void             Reserve(size_t n);
    void             AddCoord(const glm::vec3& p);
    size_t           Size() const;
    const glm::vec3& operator[](size_t i) const;
    const std::vector<glm::vec3>& Data() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
