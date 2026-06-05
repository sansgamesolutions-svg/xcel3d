add_library(XcelKernel STATIC
    Source/CoordTable.cpp
    Source/ScalarTable.cpp
    Source/ColorTable.cpp
    Source/PrimitiveSet.cpp
    Source/MeshTessellator.cpp
    Source/TessellationStrategy.cpp
)

target_include_directories(XcelKernel
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${GLM_INCLUDE_DIR}
)

target_link_libraries(XcelKernel
    PUBLIC
        XcelCommon
)

target_compile_definitions(XcelKernel
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)
