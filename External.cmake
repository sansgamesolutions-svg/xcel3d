include(FetchContent)

set(XCEL_EXTERNAL_ROOT "E:/Personal/Xcel3D/External"
    CACHE PATH "Root directory for Xcel3D external dependencies")

set(VULKAN_SDK "${XCEL_EXTERNAL_ROOT}/vulkan/1.4.335.0")
set(ENV{VULKAN_SDK} "${VULKAN_SDK}")
find_package(Vulkan REQUIRED)

set(GLFW_ROOT "${XCEL_EXTERNAL_ROOT}/glfw/binaries/wind")
find_library(GLFW_LIBRARY NAMES glfw3 glfw PATHS "${GLFW_ROOT}/lib" NO_DEFAULT_PATH)
find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h PATHS "${GLFW_ROOT}/include" NO_DEFAULT_PATH)

set(GLM_INCLUDE_DIR "${XCEL_EXTERNAL_ROOT}/glm")
set(XCEL_GLSLC_EXECUTABLE "${VULKAN_SDK}/Bin/glslc.exe"
    CACHE FILEPATH "Path to the glslc shader compiler (override for non-Windows builds)")
set(GLSLC_EXECUTABLE "${XCEL_GLSLC_EXECUTABLE}")

# Flecs ECS — static only, no tests
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
