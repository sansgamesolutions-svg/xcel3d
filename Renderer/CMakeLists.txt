set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
set(SHADER_VERT_SPV "${SHADER_OUTPUT_DIR}/mesh.vert.spv")
set(SHADER_FRAG_SPV "${SHADER_OUTPUT_DIR}/mesh.frag.spv")

add_custom_command(
    OUTPUT "${SHADER_VERT_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/mesh.vert" -o "${SHADER_VERT_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/mesh.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/mesh.frag" -o "${SHADER_FRAG_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/mesh.frag"
    VERBATIM
)

add_library(XcelRenderer STATIC
    Source/DeviceContext.cpp
    Source/GpuBuffer.cpp
    Source/RenderPass.cpp
    Source/Pipeline.cpp
    Source/DescriptorManager.cpp
    Source/CommandRecorder.cpp
    Source/Swapchain.cpp
    Source/MeshDrawable.cpp
    Source/WindowContext.cpp
    Source/Camera.cpp
    "${SHADER_VERT_SPV}"
    "${SHADER_FRAG_SPV}"
)

target_include_directories(XcelRenderer
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${GLM_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(XcelRenderer
    PUBLIC
        XcelKernel
        Vulkan::Vulkan
        XcelPlatforms
        XcelCommon
)

target_compile_definitions(XcelRenderer
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)
