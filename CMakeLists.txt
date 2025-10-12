cmake_minimum_required(VERSION 3.31)

project(ida-patcher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ENV{IDASDK} ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/submodules/ida-sdk/src)

include(dependencies/submodules/ida-cmake/bootstrap.cmake)
find_package(idasdk REQUIRED)

add_subdirectory(dependencies)

add_library(ida-patcher SHARED
  src/plugin.cpp
  src/pattern.cpp
  src/pattern.hpp
)

target_link_libraries(ida-patcher PRIVATE
  idasdk::plugin
  nlohmann_json::nlohmann_json
)
