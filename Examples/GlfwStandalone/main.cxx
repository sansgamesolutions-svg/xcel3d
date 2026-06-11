//
// XcelExample — demonstrates three routes for getting data into the renderer.
//
//  Route A  Direct construction of Graphics objects; no IO layer needed.
//  Route B  Load a .xcel file via IOManager, bridge into World via LoadIntoWorld().
//  Route C  ComputedScalarTable + ISystem for per-frame live data updates.
//
// Build instructions:
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=<xcel3d-install-dir>
//   cmake --build build
//   cd build/bin && ./XcelExample
//

#include "Renderer/Application.h"
#include "Platforms/GlfwWindowWidget.h"
#include "Kernel/CoordTable.h"
#include "Kernel/ScalarTable.h"
#include "Kernel/ColorTable.h"
#include "Kernel/PrimitiveSet.h"
#include "Renderer/Camera.h"
#include "Common/ISystem.h"
#include "IO/Core/IOManager.h"
#include "SceneLoader.h"
#include "Renderer/Component.h"
#include <optional>
#include <glm/glm.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Route A: build the demo mesh directly from application data.
// ─────────────────────────────────────────────────────────────────────────────
static xcel::Entity RouteA_DirectMesh(xcel::Application& app)
{
    auto coords  = std::make_shared<xcel::CoordTable>();
    auto scalars = std::make_shared<xcel::ElementScalarTable>();
    auto hexSet  = std::make_shared<xcel::HexPrimitiveSet>();

    for (int k = 0; k < 3; ++k)
    for (int j = 0; j < 3; ++j)
    for (int i = 0; i < 3; ++i)
        coords->AddCoord({(float)i, (float)j, (float)k});

    auto idx = [](int i, int j, int k) -> uint32_t {
        return (uint32_t)(i + j * 3 + k * 9);
    };

    const glm::vec3 center(1.f, 1.f, 1.f);
    for (int k = 0; k < 2; ++k)
    for (int j = 0; j < 2; ++j)
    for (int i = 0; i < 2; ++i) {
        xcel::HexPrimitiveSet::value_type e = {
            idx(i,   j,   k  ), idx(i+1, j,   k  ),
            idx(i+1, j+1, k  ), idx(i,   j+1, k  ),
            idx(i,   j,   k+1), idx(i+1, j,   k+1),
            idx(i+1, j+1, k+1), idx(i,   j+1, k+1),
        };
        hexSet->AddElement(e);
        scalars->AddScalar(glm::length(glm::vec3(i+.5f, j+.5f, k+.5f) - center));
    }

    return app.GetWorld().AddMesh("route_a_direct",
        coords, scalars, std::make_shared<xcel::PaletteColor>(), {hexSet});
}

// ─────────────────────────────────────────────────────────────────────────────
// Route B: load from any assimp-supported file and bridge into the World.
// io: caller-owned IOManager (must outlive all ECS objects from this load).
// pluginDir: directory containing XcelIO_*.dll plugin files.
// ─────────────────────────────────────────────────────────────────────────────
static void RouteB_FileLoad(xcel::Application& app,
                            const std::filesystem::path& path,
                            xcel::io::IOManager& io,
                            const std::filesystem::path& pluginDir,
                            xcel::ThreadPool& pool)
{
    if (path.empty() || !std::filesystem::exists(path)) return;

    io.ScanPluginDir(pluginDir);

    auto doc = io.LoadAsync(path, pool).get();
    if (doc) xcel::io::LoadIntoWorld(*doc, app.GetWorld());
}

// ─────────────────────────────────────────────────────────────────────────────
// Route C: ComputedScalarTable driven by a shared atomic buffer.
// A background thread (or simulation engine) writes to the buffer; the lambda
// reads from it each time the GPU tessellation runs.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr size_t kLiveElements = 8;
static std::atomic<float> g_liveScalars[kLiveElements];

static xcel::Entity RouteC_LiveScalars(xcel::Application& app)
{
    auto coords = std::make_shared<xcel::CoordTable>();
    auto hexSet = std::make_shared<xcel::HexPrimitiveSet>();

    for (int k = 0; k < 3; ++k)
    for (int j = 0; j < 3; ++j)
    for (int i = 0; i < 3; ++i)
        coords->AddCoord({(float)i + 4.f, (float)j, (float)k});  // offset so it doesn't overlap A

    auto idx = [](int i, int j, int k) -> uint32_t {
        return (uint32_t)(i + j * 3 + k * 9);
    };
    for (int k = 0; k < 2; ++k)
    for (int j = 0; j < 2; ++j)
    for (int i = 0; i < 2; ++i) {
        xcel::HexPrimitiveSet::value_type e = {
            idx(i,   j,   k  ), idx(i+1, j,   k  ),
            idx(i+1, j+1, k  ), idx(i,   j+1, k  ),
            idx(i,   j,   k+1), idx(i+1, j,   k+1),
            idx(i+1, j+1, k+1), idx(i,   j+1, k+1),
        };
        hexSet->AddElement(e);
        g_liveScalars[k*4 + j*2 + i].store(0.f, std::memory_order_relaxed);
    }

    // The lambda captures the atomic array by pointer; no copy of scalar data.
    auto liveTable = std::make_shared<xcel::ComputedScalarTable>(
        kLiveElements,
        [](size_t i) -> float {
            return g_liveScalars[i].load(std::memory_order_relaxed);
        });

    return app.GetWorld().AddMesh("route_c_live",
        coords, liveTable, std::make_shared<xcel::PaletteColor>(), {hexSet});
}

// ─────────────────────────────────────────────────────────────────────────────
// ISystem that animates the live scalars each frame (simulates a DB/solver push).
// ─────────────────────────────────────────────────────────────────────────────
class AnimateScalarsSystem : public xcel::ISystem
{
public:
    void Update(flecs::world&) override
    {
        using Clock = std::chrono::steady_clock;
        const float t = std::chrono::duration<float>(Clock::now().time_since_epoch()).count();
        for (size_t i = 0; i < kLiveElements; ++i)
            g_liveScalars[i].store(0.5f + 0.5f * std::sin(t + (float)i * 0.8f),
                                   std::memory_order_relaxed);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// main
//   Usage:  XcelViewer [file]
//   With a file argument: loads the file via the assimp plugin (FBX, GLB, OBJ…).
//   Without arguments:    shows the built-in demo (Route A + Route C).
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    try {
        const std::filesystem::path exeDir =
            std::filesystem::path(argv[0]).parent_path();

        // io must be declared before app so it outlives app's destructor.
        // ScalarTable, PrimitiveSet, and ColorTable are virtual types whose
        // vtables live in the plugin DLL. FreeLibrary must not be called until
        // after the ECS world (inside app) finishes destroying those objects.
        std::optional<xcel::io::IOManager> io;

        auto widget = std::make_unique<xcel::GlfwWindowWidget>(1280, 720, "Xcel3D Viewer");
        xcel::Application app(std::move(widget));

        app.SetShaderDir("shaders/");

        if (argc > 1) {
            io.emplace();
            xcel::ThreadPool pool(4);
            RouteB_FileLoad(app, std::filesystem::path(argv[1]),
                            *io, exeDir, pool);
            app.GetCamera().FitToSphere({0.f, 0.f, 0.f}, 2.f);
        } else {
            RouteA_DirectMesh(app);
            RouteC_LiveScalars(app);
            app.AddSystem<AnimateScalarsSystem>();
            app.GetCamera().FitToSphere({2.5f, 1.f, 1.f}, 3.f);
        }

        app.GetWorld().AddLight("key",  { 6.f,  6.f,  5.f}, {1.0f, 0.95f, 0.85f}, 1.2f);
        app.GetWorld().AddLight("fill", {-3.f, -2.f, -2.f}, {0.6f, 0.75f, 1.0f},  0.4f);

        app.Run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
