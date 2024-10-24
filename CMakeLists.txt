cmake_minimum_required(VERSION 3.13)

project(
    stella-lich
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    lich STATIC
    src/mark_and_sweep.hpp
    src/mark_and_sweep.cpp
)

target_include_directories(
    lich PUBLIC
    $<INSTALL_INTERFACE:src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

add_subdirectory(test)
