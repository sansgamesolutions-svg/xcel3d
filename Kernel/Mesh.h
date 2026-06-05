#pragma once
#include <memory>
#include <vector>

namespace xcel {

class CoordTable;
class ScalarTable;
class PrimitiveSet;

// CPU-side data model for one FEM mesh.
//
// A Mesh bundles the three tables that together fully describe a VTK/FEM dataset:
//
//   CoordTable   — world-space position of every node (grid point).
//   ScalarTable  — one scalar result value per element (e.g., von Mises stress).
//   PrimitiveSet — one or more element collections, each of a single type
//                  (hex solids, shell triangles, etc.).
//
// Ownership: CoordTable and ScalarTable are held as shared_ptr so they can be
// reused across multiple time steps or mesh variants without copying large arrays.
// PrimitiveSets are also shared_ptr; a model with multiple part materials might
// split one mesh into several PrimitiveSets that all index the same CoordTable.
//
// This class is purely a data holder.  All rendering-side logic (tessellation,
// GPU upload, draw calls) lives in the MeshDrawable subclass.  The separation means
// the CPU data model can be constructed and populated by a file importer before
// any Vulkan context exists.
//
// Typical construction sequence (e.g., from a VTK .vtu reader):
//   auto coords  = std::make_shared<CoordTable>(/* node positions */);
//   auto scalars = std::make_shared<ScalarTable>(/* per-element stress */);
//   auto hexSet  = std::make_shared<HexPrimitiveSet>();
//   hexSet->AddElement({ n0, n1, n2, n3, n4, n5, n6, n7 }); // VTK node order
//   auto mesh = std::make_shared<MeshDrawable>();
//   mesh->SetCoords(coords);
//   mesh->SetScalars(scalars);
//   mesh->AddPrimitiveSet(hexSet);
//   // Then call mesh->Build(ctx, colormap) once the Vulkan context is ready.
class Mesh {
public:
    Mesh();
    virtual ~Mesh();

    void SetCoords(std::shared_ptr<CoordTable> c);
    void SetScalars(std::shared_ptr<ScalarTable> s);
    void AddPrimitiveSet(std::shared_ptr<PrimitiveSet> ps);

    const CoordTable*  Coords()       const;
    const ScalarTable* Scalars()      const;
    const std::vector<std::shared_ptr<PrimitiveSet>>& PrimitiveSets() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
