
set(EXTERNAL_ROOT "${CMAKE_SOURCE_DIR}/External")

set(VULKAN_SDK "${EXTERNAL_ROOT}/vulkan/1.4.335.0")
set(ENV{VULKAN_SDK} "${VULKAN_SDK}")
find_package(Vulkan REQUIRED)

set(GLFW_ROOT "${EXTERNAL_ROOT}/glfw/binaries/wind")
find_library(GLFW_LIBRARY NAMES glfw3 glfw PATHS "${GLFW_ROOT}/lib" NO_DEFAULT_PATH)
find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h PATHS "${GLFW_ROOT}/include" NO_DEFAULT_PATH)

set(GLM_INCLUDE_DIR "${EXTERNAL_ROOT}/glm")
set(GLSLC_EXECUTABLE "${VULKAN_SDK}/Bin/glslc.exe")
