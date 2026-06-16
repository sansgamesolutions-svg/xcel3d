include(FetchContent)

# ─────────────────────────────────────────────────────────────────────────────
# Platform-specific dependency discovery
# Windows : pre-built binaries under XCEL_EXTERNAL_ROOT (see CLAUDE.md)
# Linux   : system packages — install with:
#             sudo apt install libvulkan-dev libglfw3-dev libglm-dev glslc
#           or point VULKAN_SDK at a LunarG SDK directory.
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
    set(XCEL_EXTERNAL_ROOT "D:/WorkSpace/Xcel3D/External"
        CACHE PATH "Root directory for Xcel3D external dependencies")

    set(VULKAN_SDK "${XCEL_EXTERNAL_ROOT}/vulkan/1.4.335.0")
    set(ENV{VULKAN_SDK} "${VULKAN_SDK}")
    find_package(Vulkan REQUIRED)

    set(GLFW_ROOT "${XCEL_EXTERNAL_ROOT}/glfw/binaries/wind")
    find_library(GLFW_LIBRARY NAMES glfw3 glfw PATHS "${GLFW_ROOT}/lib" NO_DEFAULT_PATH)
    find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h PATHS "${GLFW_ROOT}/include" NO_DEFAULT_PATH)

    set(GLM_INCLUDE_DIR "${XCEL_EXTERNAL_ROOT}/glm")
    set(XCEL_GLSLC_EXECUTABLE "${VULKAN_SDK}/Bin/glslc.exe"
        CACHE FILEPATH "Path to the glslc shader compiler")
    set(GLSLC_EXECUTABLE "${XCEL_GLSLC_EXECUTABLE}")

else()
    # ── Linux / macOS ──────────────────────────────────────────────────────────

    # Vulkan — honours VULKAN_SDK env var set by the LunarG installer,
    # or falls back to the system libvulkan-dev package.
    find_package(Vulkan REQUIRED)

    # GLFW3 — uses pkg-config so it works with both system and SDK installs.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
    set(GLFW_LIBRARY PkgConfig::GLFW3)
    # Expose the first include path for targets that reference GLFW_INCLUDE_DIR
    # directly; the imported target carries the full INTERFACE_INCLUDE_DIRECTORIES.
    list(GET GLFW3_INCLUDE_DIRS 0 GLFW_INCLUDE_DIR)

    # GLM — header-only; find its root so the existing target_include_directories
    # in Kernel/CMakeLists.txt continues to work unchanged.
    find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp
        HINTS /usr/include /usr/local/include
        REQUIRED)

    # glslc — prefer the one discovered by FindVulkan (matches the SDK in use).
    if(Vulkan_GLSLC_EXECUTABLE)
        set(XCEL_GLSLC_EXECUTABLE "${Vulkan_GLSLC_EXECUTABLE}"
            CACHE FILEPATH "Path to the glslc shader compiler")
    else()
        find_program(XCEL_GLSLC_EXECUTABLE NAMES glslc REQUIRED)
    endif()
    set(GLSLC_EXECUTABLE "${XCEL_GLSLC_EXECUTABLE}")

endif()

# ── Flecs ECS (all platforms) ──────────────────────────────────────────────────
set(FLECS_STATIC  ON  CACHE BOOL "" FORCE)
set(FLECS_SHARED  OFF CACHE BOOL "" FORCE)
set(FLECS_TESTS   OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG        v4.0.4
)
FetchContent_MakeAvailable(flecs)

# Flecs uses C idioms that trigger C4127 (constant conditional) on MSVC.
# Suppress it for all consumers so /WX doesn't turn it into an error.
if(MSVC AND TARGET flecs_static)
    target_compile_options(flecs_static INTERFACE /wd4127)
endif()

# ── Assimp (optional — enables XcelIO_Assimp plugin) ─────────────────────────
# Run Scripts/build-assimp.ps1 once to populate External/assimp/.
# After that, cmake finds it automatically; no -DASSIMP_ROOT flag needed.
if(DEFINED XCEL_EXTERNAL_ROOT)
    set(_assimp_default "${XCEL_EXTERNAL_ROOT}/assimp")
    if(NOT DEFINED ASSIMP_ROOT AND
       EXISTS "${_assimp_default}/include/assimp/Importer.hpp")
        set(ASSIMP_ROOT "${_assimp_default}"
            CACHE PATH "Assimp install root (auto-detected from External/assimp)" FORCE)
        message(STATUS "Assimp: auto-detected at ${ASSIMP_ROOT}")
    endif()
    unset(_assimp_default)
endif()
