# MsvcX64Crt.cmake
# Provides xcel_fix_msvc_crt_paths(<target>) for targets built without a VS
# Developer Command Prompt, where CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES is empty
# and the linker would otherwise pick up x86 CRT libs from the default paths.
# Derive x64 paths from the cached compiler and MT tool locations instead.

function(xcel_fix_msvc_crt_paths target)
    if(MSVC AND NOT CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
        get_filename_component(_cl_dir    "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_host_dir  "${_cl_dir}"            DIRECTORY)
        get_filename_component(_bin_dir   "${_host_dir}"          DIRECTORY)
        get_filename_component(_msvc_root "${_bin_dir}"           DIRECTORY)
        get_filename_component(_mt_x64    "${CMAKE_MT}"           DIRECTORY)
        get_filename_component(_sdk_ver   "${_mt_x64}"            DIRECTORY)
        get_filename_component(_sdk_bin   "${_sdk_ver}"           DIRECTORY)
        get_filename_component(_sdk_root  "${_sdk_bin}"           DIRECTORY)
        get_filename_component(_sdk_ver_name "${_sdk_ver}"        NAME)
        target_link_directories(${target} PRIVATE
            "${_msvc_root}/lib/x64"
            "${_msvc_root}/ATLMFC/lib/x64"
            "${_sdk_root}/lib/${_sdk_ver_name}/ucrt/x64"
            "${_sdk_root}/lib/${_sdk_ver_name}/um/x64"
        )
    endif()
endfunction()
