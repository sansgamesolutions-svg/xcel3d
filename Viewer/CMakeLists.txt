add_executable(XcelViewer
    main.cxx
)

target_include_directories(XcelViewer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${GLFW_INCLUDE_DIR}
)

target_link_libraries(XcelViewer PRIVATE
    XcelGraphics
    XcelPlatforms
    XcelCommon
)

if(WIN32)
    target_link_libraries(XcelViewer PRIVATE gdi32 user32 shell32)
endif()
