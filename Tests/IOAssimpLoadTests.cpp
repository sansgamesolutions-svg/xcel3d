#include "IO/Core/IOManager.h"
#include "IO/Scene/SceneDocument.h"
#include "Common/ThreadPool.h"
#include <filesystem>
#include <stdexcept>

#ifndef XCEL_TEST_BIN_DIR
#  define XCEL_TEST_BIN_DIR "."
#endif
#ifndef XCEL_TEST_ASSET_DIR
#  define XCEL_TEST_ASSET_DIR "."
#endif

namespace {

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// Helper: scan plugins then synchronously load one file.
std::shared_ptr<xcel::io::SceneDocument>
LoadFile(xcel::io::IOManager& io, const std::filesystem::path& path)
{
    xcel::ThreadPool pool(2);
    return io.LoadAsync(path, pool).get();
}

// ── Tests ─────────────────────────────────────────────────────────────────────

void TestAssimpPluginLoads()
{
    xcel::io::IOManager io;
    io.ScanPluginDir(XCEL_TEST_BIN_DIR);
}

void TestObjLoadReturnsDocument()
{
    const std::filesystem::path obj =
        std::filesystem::path(XCEL_TEST_ASSET_DIR) / "cube.obj";

    xcel::io::IOManager io;
    io.ScanPluginDir(XCEL_TEST_BIN_DIR);

    auto doc = LoadFile(io, obj);
    Require(doc != nullptr, "LoadAsync returned null for cube.obj");
    Require(doc->MeshCount() > 0, "cube.obj document has no meshes");
}

void TestCubeMeshHasVerticesAndFaces()
{
    const std::filesystem::path obj =
        std::filesystem::path(XCEL_TEST_ASSET_DIR) / "cube.obj";

    xcel::io::IOManager io;
    io.ScanPluginDir(XCEL_TEST_BIN_DIR);

    auto doc = LoadFile(io, obj);
    Require(doc != nullptr, "document is null");

    const xcel::io::MeshData& mesh = doc->Mesh(0);

    Require(mesh.coords != nullptr, "mesh has no coordinate table");
    // Assimp may split vertices per face normal; 8 unique verts → up to 24 split.
    Require(mesh.coords->Size() >= 8, "cube has fewer than 8 vertices");

    Require(!mesh.primSets.empty(), "mesh has no primitive sets");
    // 6 faces × 2 triangles = 12 triangles minimum.
    Require(mesh.primSets[0]->ElementCount() >= 12,
            "cube mesh has fewer than 12 triangles");
}

void TestMultipleLoadsSamePlugin()
{
    const std::filesystem::path obj =
        std::filesystem::path(XCEL_TEST_ASSET_DIR) / "cube.obj";

    xcel::io::IOManager io;
    io.ScanPluginDir(XCEL_TEST_BIN_DIR);

    auto doc1 = LoadFile(io, obj);
    auto doc2 = LoadFile(io, obj);

    Require(doc1 != nullptr && doc2 != nullptr, "one of two loads returned null");
    Require(doc1->MeshCount() == doc2->MeshCount(),
            "repeated loads of the same file yielded different mesh counts");
}

} // namespace

int main()
{
    TestAssimpPluginLoads();
    TestObjLoadReturnsDocument();
    TestCubeMeshHasVerticesAndFaces();
    TestMultipleLoadsSamePlugin();
    return 0;
}
