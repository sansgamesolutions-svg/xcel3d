add_library(XcelCommon STATIC
    Source/ThreadPool.cpp
)

target_include_directories(XcelCommon PUBLIC ${CMAKE_SOURCE_DIR})

target_compile_features(XcelCommon PUBLIC cxx_std_20)

target_link_libraries(XcelCommon PUBLIC flecs_static)
