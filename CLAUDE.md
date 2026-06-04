# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Xcel3D is a C++17 CAE/FEM 3D visualization application. The Graphics layer uses **raw Vulkan** (not VulkanSceneGraph) with GLFW windowing and GLM math. It renders hexahedral FEM meshes with per-element scalar field coloring (cool-to-warm colormap) and Blinn-Phong lighting.

## Build System

**CMake + Ninja, targeting MSVC x64 on Windows.**

External dependencies are rooted at `E:/Personal/Xcel3D/External` (hardcoded in `External.cmake`).

Required external libraries at that root:
| Library | Expected path | Purpose |
|---|---|---|
| Vulkan SDK 1.4.335.0 | `External/vulkan/1.4.335.0/` | Raw Vulkan API + glslc compiler |
| GLFW | `External/glfw/` (lib + include) | Window + Vulkan surface creation |
| GLM | `External/glm/` (header-only) | Math (mat4, vec3, etc.) |

### Configure and build

```powershell
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/x64-Debug
```

Binaries land in `out/build/x64-Debug/bin/`. SPIR-V shaders compile automatically to `bin/shaders/`.

### Run the viewer

```powershell
# Run from the bin directory so shaders/ is on the relative path
cd out/build/x64-Debug/bin
./XcelViewer.exe
```

### C++ standard and compiler flags

- MSVC `/utf-8` for consistent source encoding
- GLM compile definitions: `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_FORCE_RADIANS` (set on the `XcelGraphics` target)
- Standard : **C++20** (`-std=c++20`)
- Warnings : `-Wall -Wextra -Wpedantic -Werror`
- Concepts  : use `requires` clauses to constrain every template parameter
- Modules   : prefer `#pragma once` in headers; no `#ifndef` guards

### Memory & ownership
- **No raw `new` / `delete`** outside of smart pointer internals
- Prefer `std::unique_ptr` for single ownership; `std::shared_ptr` only when shared ownership is genuinely needed
- Use `std::make_unique` / `std::make_shared` — never construct smart pointers directly
- No owning raw pointers in public APIs; raw pointers are non-owning observers only

## C++ idioms

## Notes for Claude

- When generating any class, apply PIMPL to its header automatically
- C++ Functions start with capital letter 
- When generating any resource-owning class, apply RAII automatically
- Never use raw `new` / `delete` in generated code
- Prefer `std::unique_ptr` over `std::shared_ptr` unless shared ownership is explicitly requested
- Always add `requires` clauses to template parameters
- After scaffolding, offer to generate Catch2 tests for the new class

### RAII
- **Every resource has an owner** — file handles, sockets, locks, GPU buffers
- No ad-hoc `try/catch` for cleanup; RAII wrappers handle it
- `std::lock_guard` / `std::scoped_lock` for mutexes — never manual `lock()` / `unlock()`
- Prompt rule: *When generating any class that acquires a resource, always pair acquisition in the constructor with release in the destructor.*

### CRTP (Curiously Recurring Template Pattern)
- Use for **static polymorphism** — avoids vtable overhead in hot paths
- Constrain the derived type with `requires std::derived_from<Derived, Base<Derived>>`
- Prompt template: *"Plan a CRTP base `<Base>` with static interface `<methods>`. No implementation — interface only."*

```cpp
template<typename Derived>
class Serialisable
{
public:
    std::string serialise() const
    {
        return static_cast<const Derived*>(this)->serialise_impl();
    }
};
```

### Policy-based design
- Policies are template parameters; default policies live in a `defaults` namespace
- Use `requires` clauses to document the expected interface of each policy
- Prompt template: *"Generate a policy-based `<Class>` with policies `<P1>`, `<P2>`. Use C++20 concepts to constrain each policy."*

```cpp
template<typename StoragePolicy, typename LoggingPolicy>
    requires StoragePolicy<StoragePolicy> && LoggingPolicy<LoggingPolicy>
class DataManager : public StoragePolicy, public LoggingPolicy
{
    // ...
};
```

### .claudeignore

```
build/
build-asan/
CMakeCache.txt
CMakeFiles/
*.o
*.a
*.so
*.dylib
compile_commands.json
.cache/
```


## Architecture
- Each folder has CLAUDE.md specific to thae module

## External
- All third party libraries



