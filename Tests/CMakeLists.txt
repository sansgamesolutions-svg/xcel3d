# ── Kernel unit tests ─────────────────────────────────────────────────────────
add_executable(XcelKernelTests
    KernelTessellationTests.cpp
)

target_link_libraries(XcelKernelTests PRIVATE XcelKernel)
xcel_fix_msvc_crt_paths(XcelKernelTests)

add_test(NAME XcelKernelTests COMMAND XcelKernelTests)

# ── Assimp IO integration tests (built only when ASSIMP_ROOT is available) ────
if(DEFINED ASSIMP_ROOT)
    add_executable(XcelIOAssimpTests
        IOAssimpLoadTests.cpp
    )

    target_link_libraries(XcelIOAssimpTests PRIVATE XcelIO)

    # Inject the bin/ directory (where XcelIO_Assimp.dll lands) and the local
    # test assets directory as compile-time constants.
    target_compile_definitions(XcelIOAssimpTests PRIVATE
        XCEL_TEST_BIN_DIR="${CMAKE_BINARY_DIR}/bin"
        XCEL_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
    )

    xcel_fix_msvc_crt_paths(XcelIOAssimpTests)

    add_test(NAME XcelIOAssimpTests
             COMMAND XcelIOAssimpTests
             WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
