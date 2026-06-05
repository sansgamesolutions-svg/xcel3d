add_library(XcelKernel STATIC
    Source/CoordTable.cpp
    Source/ScalarTable.cpp
    Source/ColorTable.cpp
    Source/PrimitiveSet.cpp
    Source/Mesh.cpp
    Source/MeshTessellator.cpp
)

target_include_directories(XcelKernel
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${GLM_INCLUDE_DIR}
)

target_compile_definitions(XcelKernel
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)
