add_library(XcelIO STATIC
    # Core
    Core/FormatRegistry.cpp
    Core/SceneBuilder.cpp
    Core/IOManager.cpp
    Core/PluginLoader.cpp
    # Scene
    Scene/SceneDocument.cpp
    # Animation
    Animation/AnimationCatalogue.cpp
    Animation/AnimationStream.cpp
    Animation/FrameCache.cpp
    # Formats (built-in)
    Formats/XcelFormat.cpp
)

target_include_directories(XcelIO
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include/xcel3d>
)

target_link_libraries(XcelIO
    PUBLIC
        XcelKernel     # CoordTable, ScalarTable, PrimitiveSet, ColorTable
        XcelCommon     # ThreadPool
)

# ── Assimp plugin (optional, separate SHARED target) ─────────────────────────
if(DEFINED ASSIMP_ROOT)
    add_subdirectory(Formats/Plugins/AssimpPlugin)
endif()
