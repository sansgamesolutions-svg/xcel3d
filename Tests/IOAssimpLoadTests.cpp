#include "IO/Core/IOManager.h"
#include "IO/Core/ISceneReceiver.h"
#include "Common/ThreadPool.h"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

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

// Capturing receiver — stores mesh metadata for test assertions.
struct CapturingReceiver : public xcel::io::ISceneReceiver
{
    struct Mesh
    {
        std::string name;
        size_t      vertexCount = 0;
        size_t      elemCount   = 0;
    };
    std::vector<Mesh> meshes;

    void ReceiveMesh(
        std::string_view            name,
        std::span<const float>      positions,
        xcel::PrimitiveType         /*primType*/,
        std::span<const uint32_t>   indices,
        uint32_t                    indicesPerElement,
        std::span<const float>      /*scalars*/) override
    {
        meshes.push_back({
            std::string(name),
            positions.size() / 3,
            indices.size() / indicesPerElement
        });
    }
};

// Helper: scan plugins then synchronously load one file.
CapturingReceiver LoadFile(xcel::io::IOManager& io, const std::filesystem::path& path)
{
    CapturingReceiver recv;
    xcel::ThreadPool pool(2);
    io.LoadAsync(path, recv, pool).get();
    return recv;
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

    auto recv = LoadFile(io, obj);
    Require(!recv.meshes.empty(), "LoadAsync produced no meshes for cube.obj");
}

void TestCubeMeshHasVerticesAndFaces()
{
    const std::filesystem::path obj =
        std::filesystem::path(XCEL_TEST_ASSET_DIR) / "cube.obj";

    xcel::io::IOManager io;
    io.ScanPluginDir(XCEL_TEST_BIN_DIR);

    auto recv = LoadFile(io, obj);
    Require(!recv.meshes.empty(), "document has no meshes");

    const auto& mesh = recv.meshes[0];
    // Assimp may split vertices per face normal; 8 unique verts -> up to 24 split.
    Require(mesh.vertexCount >= 8, "cube has fewer than 8 vertices");
    // 6 faces x 2 triangles = 12 triangles minimum.
    Require(mesh.elemCount >= 12, "cube mesh has fewer than 12 triangles");
}

void TestMultipleLoadsSamePlugin()
{
    const std::filesystem::path obj =
        std::filesystem::path(XCEL_TEST_ASSET_DIR) / "cube.obj";

    xcel::io::IOManager io;
    io.ScanPluginDir(XCEL_TEST_BIN_DIR);

    auto recv1 = LoadFile(io, obj);
    auto recv2 = LoadFile(io, obj);

    Require(!recv1.meshes.empty() && !recv2.meshes.empty(),
            "one of two loads produced no meshes");
    Require(recv1.meshes.size() == recv2.meshes.size(),
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
