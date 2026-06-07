cmake_minimum_required(VERSION 3.15)
project(XcelExample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Consume xcel3d ─────────────────────────────────────────────────────────────
# Option A (source tree): uncomment the two lines below and comment out find_package.
#   add_subdirectory(../../ xcel3d)
#   set(XCEL_LIBS XcelGraphics XcelPlatforms XcelCommon XcelIO)
#
# Option B (installed package): cmake --install the xcel3d build first, then:
find_package(Xcel3D REQUIRED)
set(XCEL_LIBS Xcel::XcelGraphics Xcel::XcelPlatforms Xcel::XcelCommon Xcel::XcelIO)

add_executable(XcelExample main.cxx)

target_link_libraries(XcelExample PRIVATE ${XCEL_LIBS})

if(WIN32)
    target_link_libraries(XcelExample PRIVATE gdi32 user32 shell32)
endif()
