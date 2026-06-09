#pragma once
#include "IO/Scene/SceneDocument.h"
#include "Renderer/World.h"

namespace xcel::io {

// Transfers all meshes from a loaded SceneDocument into a Graphics World.
// MeshData carries the same shared_ptr types that World::AddMesh() accepts,
// so this is a thin adapter with no data copying.
inline void LoadIntoWorld(const SceneDocument& doc, xcel::World& world)
{
    for (size_t i = 0; i < doc.MeshCount(); ++i) {
        const MeshData& m = doc.Mesh(i);
        world.AddMesh(m.name, m.coords, m.scalars, m.colorTable, m.primSets);
    }
}

} // namespace xcel::io
