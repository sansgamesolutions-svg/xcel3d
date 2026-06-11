# External Dependencies

All third-party libraries live under this directory. The root is exposed to CMake as
`XCEL_EXTERNAL_ROOT` (default `D:/WorkSpace/Xcel3D/External`; override via cache).

---

## Library Inventory

### Vulkan SDK — `vulkan/1.4.335.0/`

| Item | Detail |
|---|---|
| Type | Pre-built SDK (Windows x64) |
| Version | 1.4.335.0 |
| Used by | `XcelRenderer`, `XcelPlatforms` |
| CMake | `find_package(Vulkan REQUIRED)` via `ENV{VULKAN_SDK}` |
| Shader compiler | `vulkan/1.4.335.0/Bin/glslc.exe` |
| Layout | `Bin/`, `Include/`, `Lib/`, `cmake/` |

Do not upgrade in place — update the path constant in `External.cmake` and test all
Vulkan feature usage before committing.

---

### GLFW — `glfw/`

| Item | Detail |
|---|---|
| Type | Pre-built binaries (Windows x64) + source tree |
| Version | Matches `binaries/wind/lib/cmake/glfw3/glfw3ConfigVersion.cmake` |
| Used by | `XcelPlatforms` (window creation, Vulkan surface) |
| CMake | Manual `find_library` / `find_path` — no `find_package` |
| Binary layout | `binaries/wind/include/GLFW/`, `binaries/wind/lib/glfw3.lib` |

The `binaries/wind/` suffix stands for Windows. If Linux/macOS support is added,
add a parallel `binaries/linux/` or `binaries/mac/` directory and guard the path in
`External.cmake` with `if(WIN32)`.

---

### GLM — `glm/`

| Item | Detail |
|---|---|
| Type | Header-only |
| Version | Bundled source (no build step) |
| Used by | `XcelKernel`, `XcelRenderer` |
| CMake | Include-dir only via `GLM_INCLUDE_DIR` |
| Required definitions | `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_FORCE_RADIANS` (set on `XcelKernel`) |

---

### Flecs ECS — (FetchContent, not on disk)

| Item | Detail |
|---|---|
| Type | Source, fetched automatically by CMake |
| Version | v4.0.4 |
| Used by | `XcelCommon` (entity-component system) |
| CMake | `FetchContent_Declare` + `FetchContent_MakeAvailable` in `External.cmake` |
| Cache | Stored in `out/build/<preset>/_deps/flecs-*` after first configure |

No files here; the source is downloaded on first `cmake` run.

---

### Parallel Hashmap — `phmap/`

| Item | Detail |
|---|---|
| Type | Header-only |
| Version | Bundled source |
| Used by | Available to all targets (not yet wired into a target_include explicitly) |
| CMake | Not yet referenced in `External.cmake` — add `find_path` when needed |

---

### Boost — `boost/boost_1_89_0/`

| Item | Detail |
|---|---|
| Type | Full source distribution |
| Version | 1.89.0 |
| Used by | Not currently linked into any core target |
| CMake | Not referenced in `External.cmake` — add `find_package(Boost ...)` when needed |

---

### Assimp — `assimp/` *(optional, built by script)*

| Item | Detail |
|---|---|
| Type | Static libs, built from source |
| Version | 5.4.3 |
| Used by | `XcelIO_Assimp` plugin DLL (optional; see `IO/Formats/Plugins/AssimpPlugin/`) |
| CMake | Auto-detected by `External.cmake` when `assimp/include/assimp/Importer.hpp` exists |
| Build script | `Scripts/build-assimp.ps1` (Windows x64, one-shot) |

**To set up:**
```powershell
.\Scripts\build-assimp.ps1
```
This downloads and builds assimp 5.4.3 (Debug + Release, static) and installs to
`External/assimp/`. After that, the `XcelIO_Assimp` plugin is compiled automatically
with no extra CMake flags.

**Layout after install:**
```
assimp/
├── include/assimp/       ← headers
└── lib/
    ├── assimp-vc143-mt.lib   ← Release
    ├── assimp-vc143-mtd.lib  ← Debug
    └── zlibstatic.lib        ← bundled zlib (linked by plugin)
```

---

## Conventions

- **Pre-built binaries** go in `<lib>/binaries/<platform>/` (e.g., `glfw/binaries/wind/`).
- **Header-only** libraries sit directly under their name folder (e.g., `glm/`, `phmap/`).
- **Source trees** are checked in only when they have no external build step required
  (GLM, phmap) or when pinning an exact version is critical (Boost).
- **Auto-fetched** libraries (Flecs) are declared in `External.cmake` with
  `FetchContent_Declare`; their source lands in the build tree, not here.
- **Manually built** optional libraries (Assimp) are built by scripts in `Scripts/`
  and installed here; `External.cmake` auto-detects them by sentinel file.
