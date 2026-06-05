set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

# ── Graphics shaders ─────────────────────────────────────────────────────────
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

# ── Culling / pre-pass shaders ───────────────────────────────────────────────
set(DEPTH_PREPASS_VERT_SPV  "${SHADER_OUTPUT_DIR}/depth_prepass.vert.spv")
set(DEPTH_PREPASS_FRAG_SPV  "${SHADER_OUTPUT_DIR}/depth_prepass.frag.spv")
set(FRUSTUM_CULL_COMP_SPV   "${SHADER_OUTPUT_DIR}/frustum_cull.comp.spv")
set(HIZ_BUILD_COMP_SPV      "${SHADER_OUTPUT_DIR}/hiz_build.comp.spv")
set(OCCLUSION_CULL_COMP_SPV "${SHADER_OUTPUT_DIR}/occlusion_cull.comp.spv")

add_custom_command(
    OUTPUT "${DEPTH_PREPASS_VERT_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/depth_prepass.vert" -o "${DEPTH_PREPASS_VERT_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/depth_prepass.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${DEPTH_PREPASS_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/depth_prepass.frag" -o "${DEPTH_PREPASS_FRAG_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/depth_prepass.frag"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FRUSTUM_CULL_COMP_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/frustum_cull.comp" -o "${FRUSTUM_CULL_COMP_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/frustum_cull.comp"
    VERBATIM
)

add_custom_command(
    OUTPUT "${HIZ_BUILD_COMP_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/hiz_build.comp" -o "${HIZ_BUILD_COMP_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/hiz_build.comp"
    VERBATIM
)

add_custom_command(
    OUTPUT "${OCCLUSION_CULL_COMP_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/occlusion_cull.comp" -o "${OCCLUSION_CULL_COMP_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/occlusion_cull.comp"
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
    Source/BatchDrawable.cpp
    Source/BatchingSystem.cpp
    Source/InstanceDrawable.cpp
    Source/WindowContext.cpp
    Source/Camera.cpp
    Source/RenderGraph.cpp
    Source/RenderGraphBuilder.cpp
    Source/ForwardRenderPass.cpp
    Source/FrustumCullPass.cpp
    Source/OcclusionCullPass.cpp
    "${SHADER_VERT_SPV}"
    "${SHADER_FRAG_SPV}"
    "${DEPTH_PREPASS_VERT_SPV}"
    "${DEPTH_PREPASS_FRAG_SPV}"
    "${FRUSTUM_CULL_COMP_SPV}"
    "${HIZ_BUILD_COMP_SPV}"
    "${OCCLUSION_CULL_COMP_SPV}"
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
