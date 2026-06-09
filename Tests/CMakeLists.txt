add_executable(XcelCoreTests
    CoreTests.cpp
)

target_link_libraries(XcelCoreTests
    PRIVATE
        XcelIO
        XcelCommon
)

add_test(NAME XcelCoreTests COMMAND XcelCoreTests)
