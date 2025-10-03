cmake_minimum_required(VERSION 4.1)

# Use GoogleTest from submodule
# GoogleTest is built as part of dependencies, so we can link directly

add_executable(pattern_tests
    test_pattern.cpp
)

target_link_libraries(pattern_tests PRIVATE 
    gtest
    gtest_main
    ida-patcher-core
)

target_include_directories(pattern_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME PatternTests COMMAND pattern_tests)

if(MSVC)
    target_compile_options(pattern_tests PRIVATE /W4)
else()
    target_compile_options(pattern_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
