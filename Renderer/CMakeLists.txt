set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
set(SHADER_VERT_SPV "${SHADER_OUTPUT_DIR}/mesh.vert.spv")
set(SHADER_FRAG_SPV "${SHADER_OUTPUT_DIR}/mesh.frag.spv")
set(SHADER_FRUSTUM_CULL_SPV "${SHADER_OUTPUT_DIR}/frustum_cull.comp.spv")
set(SHADER_MANIP_VERT_SPV "${SHADER_OUTPUT_DIR}/manipulator.vert.spv")
set(SHADER_MANIP_FRAG_SPV "${SHADER_OUTPUT_DIR}/manipulator.frag.spv")
set(SHADER_OIT_ACCUMULATE_FRAG_SPV "${SHADER_OUTPUT_DIR}/oit_accumulate.frag.spv")
set(SHADER_OIT_COMPOSITE_VERT_SPV "${SHADER_OUTPUT_DIR}/oit_composite.vert.spv")
set(SHADER_OIT_COMPOSITE_FRAG_SPV "${SHADER_OUTPUT_DIR}/oit_composite.frag.spv")

# Shared GLSL headers pulled in via #include; listed as DEPENDS on every shader
# that includes them so header edits trigger recompilation.
set(SHADER_COMMON_HEADERS
    "${SHADER_SOURCE_DIR}/lighting.glsl"
    "${SHADER_SOURCE_DIR}/scene_ubo.glsl"
    "${SHADER_SOURCE_DIR}/bindless_textures.glsl"
    "${SHADER_SOURCE_DIR}/material.glsl"
)

add_custom_command(
    OUTPUT "${SHADER_VERT_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.2 "${SHADER_SOURCE_DIR}/mesh.vert" -o "${SHADER_VERT_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/mesh.vert" ${SHADER_COMMON_HEADERS}
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.2 "${SHADER_SOURCE_DIR}/mesh.frag" -o "${SHADER_FRAG_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/mesh.frag" ${SHADER_COMMON_HEADERS}
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_OIT_ACCUMULATE_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.2 "${SHADER_SOURCE_DIR}/oit_accumulate.frag" -o "${SHADER_OIT_ACCUMULATE_FRAG_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/oit_accumulate.frag" ${SHADER_COMMON_HEADERS}
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_OIT_COMPOSITE_VERT_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/oit_composite.vert" -o "${SHADER_OIT_COMPOSITE_VERT_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/oit_composite.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_OIT_COMPOSITE_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/oit_composite.frag" -o "${SHADER_OIT_COMPOSITE_FRAG_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/oit_composite.frag"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_FRUSTUM_CULL_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/frustum_cull.comp" -o "${SHADER_FRUSTUM_CULL_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/frustum_cull.comp"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_MANIP_VERT_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/manipulator.vert" -o "${SHADER_MANIP_VERT_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/manipulator.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SHADER_MANIP_FRAG_SPV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/manipulator.frag" -o "${SHADER_MANIP_FRAG_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/manipulator.frag"
    VERBATIM
)

add_library(XcelRenderer STATIC
    Source/DeviceContext.cpp
    Source/GpuBuffer.cpp
    Source/GpuImage.cpp
    Source/TextureManager.cpp
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
    Source/OitPass.cpp
    Source/RenderGraph.cpp
    Source/RenderGraphBuilder.cpp
    Source/FrustumCullPass.cpp
    Source/WindowContext.cpp
    Source/VulkanContext.cpp
    Source/World.cpp
    Source/RenderOptions.cpp
    # Manipulators
    Source/Manipulator/ManipulatorGeometry.cpp
    Source/Manipulator/ViewManipulator.cpp
    Source/Manipulator/TranslateManipulator.cpp
    Source/Manipulator/SectionCutManipulator.cpp
    Source/Manipulator/ManipulatorController.cpp
    Source/Manipulator/ManipulatorPass.cpp
    # Shaders (SPIR-V targets ensure compilation before linking)
    "${SHADER_VERT_SPV}"
    "${SHADER_FRAG_SPV}"
    "${SHADER_FRUSTUM_CULL_SPV}"
    "${SHADER_MANIP_VERT_SPV}"
    "${SHADER_MANIP_FRAG_SPV}"
    "${SHADER_OIT_ACCUMULATE_FRAG_SPV}"
    "${SHADER_OIT_COMPOSITE_VERT_SPV}"
    "${SHADER_OIT_COMPOSITE_FRAG_SPV}"
)

target_include_directories(XcelRenderer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${Vulkan_INCLUDE_DIRS}>
        $<INSTALL_INTERFACE:include/xcel3d>
)

target_link_libraries(XcelRenderer
    PUBLIC
        XcelKernel
        XcelCommon
        XcelPlatforms
        Vulkan::Vulkan
)
