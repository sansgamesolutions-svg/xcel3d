set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
set(SHADER_VERT_SPV "${SHADER_OUTPUT_DIR}/mesh.vert.spv")
set(SHADER_FRAG_SPV "${SHADER_OUTPUT_DIR}/mesh.frag.spv")
set(SHADER_FRUSTUM_CULL_SPV "${SHADER_OUTPUT_DIR}/frustum_cull.comp.spv")

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

add_custom_command(
    OUTPUT "${SHADER_FRUSTUM_CULL_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/frustum_cull.comp" -o "${SHADER_FRUSTUM_CULL_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/frustum_cull.comp"
    VERBATIM
)

add_library(XcelGraphics STATIC
    # ex-Kernel
    Source/CoordTable.cpp
    Source/ColorTable.cpp
    Source/ScalarTable.cpp
    Source/PrimitiveSet.cpp
    Source/MeshTessellator.cpp
    Source/TessellationStrategy.cpp
    # ex-Renderer
    Source/DeviceContext.cpp
    Source/GpuBuffer.cpp
    Source/RenderPass.cpp
    Source/Pipeline.cpp
    Source/DescriptorManager.cpp
    Source/CommandRecorder.cpp
    Source/Swapchain.cpp
    Source/BatchDrawable.cpp
    Source/BatchingSystem.cpp
    Source/InstanceDrawable.cpp
    Source/Camera.cpp
    Source/ForwardRenderPass.cpp
    Source/RenderGraph.cpp
    Source/RenderGraphBuilder.cpp
    Source/FrustumCullPass.cpp
    Source/WindowContext.cpp
    # new
    Source/World.cpp
    "${SHADER_VERT_SPV}"
    "${SHADER_FRAG_SPV}"
    "${SHADER_FRUSTUM_CULL_SPV}"
)

target_include_directories(XcelGraphics
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${GLM_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(XcelGraphics
    PUBLIC
        XcelCommon
        XcelPlatforms
        Vulkan::Vulkan
)

target_compile_definitions(XcelGraphics
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)
