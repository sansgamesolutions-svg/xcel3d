cmake_minimum_required(VERSION 3.15)

project(xcel3d
        VERSION 1.1.10
        LANGUAGES CXX C
)

include(External.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/utf-8" "/W4" "/WX")
else()
    add_compile_options("-Wall" "-Wextra" "-Wpedantic" "-Werror")
endif()

add_subdirectory(Common)
add_subdirectory(Platforms)
add_subdirectory(Graphics)
add_subdirectory(IO)
add_subdirectory(Viewer)

# ── CMake package install / export ────────────────────────────────────────────
include(CMakePackageConfigHelpers)

install(TARGETS XcelCommon XcelPlatforms XcelGraphics XcelIO
    EXPORT XcelTargets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

install(DIRECTORY Common Platforms Graphics IO
    DESTINATION include/xcel3d
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT XcelTargets
    FILE XcelTargets.cmake
    NAMESPACE Xcel::
    DESTINATION lib/cmake/Xcel3D
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/XcelConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/CMake/XcelConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/XcelConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Xcel3D
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/XcelConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/XcelConfigVersion.cmake"
    DESTINATION lib/cmake/Xcel3D
)
