cmake_minimum_required(VERSION 3.20)
project(XcelIO_Assimp CXX)

# Only build if the user has supplied ASSIMP_ROOT.
# Usage: cmake -DASSIMP_ROOT=E:/Personal/Xcel3D/External/assimp ...
if(NOT DEFINED ASSIMP_ROOT)
    message(STATUS "XcelIO_Assimp: ASSIMP_ROOT not set — plugin skipped.")
    return()
endif()

# ── Assimp paths ─────────────────────────────────────────────────────────────
set(ASSIMP_INCLUDE_DIR "${ASSIMP_ROOT}/include")
set(ASSIMP_LIB_DIR     "${ASSIMP_ROOT}/lib")

if(NOT EXISTS "${ASSIMP_INCLUDE_DIR}/assimp/Importer.hpp")
    message(FATAL_ERROR "XcelIO_Assimp: assimp/Importer.hpp not found at ${ASSIMP_INCLUDE_DIR}")
endif()

find_library(ASSIMP_LIB
    NAMES assimp assimp-vc143-mt assimp-vc142-mt
    HINTS "${ASSIMP_LIB_DIR}"
    REQUIRED
)

# ── Target ───────────────────────────────────────────────────────────────────
add_library(XcelIO_Assimp SHARED
    AssimpFormatReader.h
    AssimpFormatReader.cpp
)

target_include_directories(XcelIO_Assimp
    PRIVATE
        "${CMAKE_SOURCE_DIR}"        # lets headers use "IO/..." paths
        "${ASSIMP_INCLUDE_DIR}"
)

target_link_libraries(XcelIO_Assimp
    PRIVATE "${ASSIMP_LIB}"
)

target_compile_features(XcelIO_Assimp PRIVATE cxx_std_20)

# Place the DLL alongside XcelViewer.exe so ScanPluginDir(".") finds it.
set_target_properties(XcelIO_Assimp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
