add_library(XcelCommon STATIC
    Source/ThreadPool.cpp
)

target_include_directories(XcelCommon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include/xcel3d>
)

target_compile_features(XcelCommon PUBLIC cxx_std_20)

target_link_libraries(XcelCommon PUBLIC flecs_static)
