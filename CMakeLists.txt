cmake_minimum_required(VERSION 3.31)

project(ida-patcher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ENV{IDASDK} ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/submodules/ida-sdk/src)

include(dependencies/submodules/ida-cmake/bootstrap.cmake)
find_package(idasdk REQUIRED)

add_subdirectory(dependencies/submodules/json)

add_library(ida-patcher SHARED
  src/plugin.cpp
  src/pattern.cpp
  src/pattern.hpp
  src/platform.cpp
  src/platform.hpp
)

target_link_libraries(ida-patcher PRIVATE
  idasdk::plugin
  nlohmann_json::nlohmann_json
)

option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
  add_subdirectory(dependencies/submodules/Catch2)

  enable_testing()

  add_executable(ida-patcher-tests
    tests/test_pattern_parsing.cpp
    tests/test_pattern_matching.cpp
    tests/test_pattern_patching.cpp
    src/pattern.cpp
  )

  target_link_libraries(ida-patcher-tests PRIVATE
    Catch2::Catch2WithMain
  )

  target_include_directories(ida-patcher-tests PRIVATE
    src
  )

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/submodules/Catch2/extras")

  include(Catch)
  include(CTest)
  catch_discover_tests(ida-patcher-tests)
endif()
