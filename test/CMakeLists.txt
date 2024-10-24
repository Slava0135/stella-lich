include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)

FetchContent_MakeAvailable(Catch2)

add_executable(tests ./test.cpp)
target_link_libraries(tests PUBLIC dev)
target_link_libraries(tests PUBLIC Catch2::Catch2WithMain)
target_include_directories(tests PUBLIC ../src)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(tests)
