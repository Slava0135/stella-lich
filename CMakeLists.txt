cmake_minimum_required(VERSION 3.13)

project(
    stella-lich
    LANGUAGES C CXX
)

set(C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)
