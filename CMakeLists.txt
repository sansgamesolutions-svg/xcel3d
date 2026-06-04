cmake_minimum_required(VERSION 3.15)

project(xcel3d
        VERSION 1.1.10
        LANGUAGES CXX C
)

include(External.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/utf-8" "/W4" "/WX")
else()
    add_compile_options("-Wall" "-Wextra" "-Wpedantic" "-Werror")
endif()

add_subdirectory(Common)
add_subdirectory(Graphics)
add_subdirectory(Viewer)
