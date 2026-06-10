add_library(XcelKernel STATIC
    CoordTable.cpp
    ScalarTable.cpp
    ColorTable.cpp
    PrimitiveSet.cpp
    MeshTessellator.cpp
    TessellationStrategy.cpp
)

target_include_directories(XcelKernel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include/xcel3d>
        $<BUILD_INTERFACE:${GLM_INCLUDE_DIR}>
)

target_link_libraries(XcelKernel PUBLIC XcelCommon)

target_compile_definitions(XcelKernel
    PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS
)
