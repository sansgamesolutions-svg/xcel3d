set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

set(SHADER_VERT_SRC "${SHADER_SOURCE_DIR}/mesh.vert")
set(SHADER_FRAG_SRC "${SHADER_SOURCE_DIR}/mesh.frag")
set(SHADER_VERT_SPV "${SHADER_OUTPUT_DIR}/mesh.vert.spv")
set(SHADER_FRAG_SPV "${SHADER_OUTPUT_DIR}/mesh.frag.spv")

add_custom_command(
    OUTPUT "${SHADER_VERT_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_VERT_SRC}" -o "${SHADER_VERT_SPV}"
    DEPENDS "${SHADER_VERT_SRC}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_FRAG_SRC}" -o "${SHADER_FRAG_SPV}"
    DEPENDS "${SHADER_FRAG_SRC}"
    VERBATIM
)

add_library(XcelGraphics STATIC
    Source/DeviceContext.cpp
    Source/CoordTable.cpp
    Source/ScalarTable.cpp
    Source/ColorTable.cpp
    Source/PrimitiveSet.cpp
    Source/Mesh.cpp
    Source/HexTessellator.cpp
    Source/MeshTessellator.cpp
    Source/StaticMesh.cpp
    Source/Camera.cpp
    Source/GpuBuffer.cpp
    Source/Swapchain.cpp
    Source/RenderPass.cpp
    Source/DescriptorManager.cpp
    Source/Pipeline.cpp
    Source/CommandRecorder.cpp
    Source/WindowContext.cpp
    "${SHADER_VERT_SPV}"
    "${SHADER_FRAG_SPV}"
)

target_include_directories(XcelGraphics
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${GLM_INCLUDE_DIR}
        ${GLFW_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(XcelGraphics
    PUBLIC
        Vulkan::Vulkan
        ${GLFW_LIBRARY}
        XcelCommon
)

target_compile_definitions(XcelGraphics
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)
