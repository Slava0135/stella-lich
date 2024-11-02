add_library(dev mark_and_sweep.hpp mark_and_sweep.cpp tables.cpp tables.hpp)
target_compile_options(dev PRIVATE -g -fsanitize=address -fno-omit-frame-pointer)
target_link_options(dev PRIVATE -g -fsanitize=address -fno-omit-frame-pointer)

add_library(lich gc.cpp runtime.c gc.h runtime.h mark_and_sweep.hpp mark_and_sweep.cpp tables.cpp tables.hpp)
target_compile_options(lich PRIVATE -g -O0)
target_link_options(lich PRIVATE -g -O0)

set_target_properties(lich
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out"
)
