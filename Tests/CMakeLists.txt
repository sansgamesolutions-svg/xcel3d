add_executable(XcelKernelTests
    KernelTessellationTests.cpp
)

target_link_libraries(XcelKernelTests PRIVATE XcelKernel)

if(MSVC AND NOT CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
    # cmake was configured without a VS Developer Command Prompt so the x64
    # CRT import libraries are not on the linker search path.  Derive the
    # paths from the cached compiler and MT tool locations.
    get_filename_component(_cl_dir   "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_host_dir "${_cl_dir}"            DIRECTORY)
    get_filename_component(_bin_dir  "${_host_dir}"          DIRECTORY)
    get_filename_component(_msvc_root "${_bin_dir}"          DIRECTORY)

    get_filename_component(_mt_x64_dir "${CMAKE_MT}" DIRECTORY)
    get_filename_component(_sdk_ver_dir "${_mt_x64_dir}" DIRECTORY)
    get_filename_component(_sdk_bin_dir "${_sdk_ver_dir}" DIRECTORY)
    get_filename_component(_sdk_root    "${_sdk_bin_dir}" DIRECTORY)
    get_filename_component(_sdk_version "${_sdk_ver_dir}" NAME)

    target_link_directories(XcelKernelTests PRIVATE
        "${_msvc_root}/lib/x64"
        "${_msvc_root}/ATLMFC/lib/x64"
        "${_sdk_root}/lib/${_sdk_version}/ucrt/x64"
        "${_sdk_root}/lib/${_sdk_version}/um/x64"
    )
endif()

add_test(NAME XcelKernelTests COMMAND XcelKernelTests)
