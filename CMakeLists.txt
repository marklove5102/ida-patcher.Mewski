cmake_minimum_required(VERSION 4.1)

project(ida-patcher)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(dependencies)

add_library(ida-patcher SHARED
    src/plugin.cpp
)
