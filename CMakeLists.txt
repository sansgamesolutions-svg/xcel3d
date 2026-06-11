cmake_minimum_required(VERSION 3.15)

project(xcel3d
        VERSION 1.1.10
        LANGUAGES CXX C
)

include(CTest)
include(External.cmake)
include(CMake/MsvcX64Crt.cmake)

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
add_subdirectory(Kernel)
add_subdirectory(Renderer)
add_subdirectory(IO)

# ── Standalone viewer ─────────────────────────────────────────────────────────
add_executable(XcelViewer Examples/GlfwStandalone/main.cxx)
target_include_directories(XcelViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Examples/GlfwStandalone
)
target_link_libraries(XcelViewer PRIVATE
    XcelRenderer XcelKernel XcelPlatforms XcelCommon XcelIO
)
if(WIN32)
    target_link_libraries(XcelViewer PRIVATE gdi32 user32 shell32)
endif()
xcel_fix_msvc_crt_paths(XcelViewer)

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()

# ── CMake package install / export ────────────────────────────────────────────
include(CMakePackageConfigHelpers)

install(TARGETS XcelCommon XcelPlatforms XcelKernel XcelRenderer XcelIO
    EXPORT XcelTargets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

install(DIRECTORY Common Platforms Kernel Renderer IO
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
    "${CMAKE_CURRENT_SOURCE_DIR}/CMake/XcelConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/XcelConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Xcel3D
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/XcelConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/XcelConfigVersion.cmake"
    DESTINATION lib/cmake/Xcel3D
)
