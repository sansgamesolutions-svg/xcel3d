add_library(XcelPlatforms STATIC
    Source/GlfwWindowWidget.cpp
)

target_include_directories(XcelPlatforms
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${GLFW_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${Vulkan_INCLUDE_DIRS}>
        $<INSTALL_INTERFACE:include/xcel3d>
)

target_link_libraries(XcelPlatforms
    PUBLIC
        ${GLFW_LIBRARY}
        Vulkan::Vulkan
)

target_compile_features(XcelPlatforms PUBLIC cxx_std_20)
