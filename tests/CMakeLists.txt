# Tests subdirectory

add_executable(audiofilter_tests
    filter_tests.cpp
)

target_link_libraries(audiofilter_tests
    PRIVATE
        audiofilter
        GTest::gtest_main
)

gtest_discover_tests(audiofilter_tests)
