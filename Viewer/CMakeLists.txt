add_executable(XcelViewer
    main.cxx
)

target_include_directories(XcelViewer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${GLFW_INCLUDE_DIR}
)

target_link_libraries(XcelViewer PRIVATE
    XcelGraphics
    XcelCommon
    ${GLFW_LIBRARY}
)

if(WIN32)
    target_link_libraries(XcelViewer PRIVATE gdi32 user32 shell32)
endif()
