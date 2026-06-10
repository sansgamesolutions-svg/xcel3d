add_executable(XcelKernelTests
    KernelTessellationTests.cpp
)

target_link_libraries(XcelKernelTests PRIVATE XcelKernel)

add_test(NAME XcelKernelTests COMMAND XcelKernelTests)
