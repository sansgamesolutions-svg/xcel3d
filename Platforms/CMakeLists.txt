add_library(XcelPlatforms STATIC
    Source/GlfwWindowWidget.cpp
)

target_include_directories(XcelPlatforms
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${GLFW_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(XcelPlatforms
    PUBLIC
        ${GLFW_LIBRARY}
        Vulkan::Vulkan
)

target_compile_features(XcelPlatforms PUBLIC cxx_std_20)
