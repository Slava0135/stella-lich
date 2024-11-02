#include "gc.h"

#include <stdio.h>
#include <stdlib.h>

#include <iostream>

#include "mark_and_sweep.hpp"
#include "runtime.h"

#ifndef MAX_ALLOC_SIZE
#define MAX_ALLOC_SIZE 1024
#endif

#ifndef INCREMENTAL
#define INCREMENTAL 0
#endif

static_assert(MAX_ALLOC_SIZE > 0);

gc::MarkAndSweep gcc(MAX_ALLOC_SIZE, true, true, INCREMENTAL);

void *gc_alloc(size_t size_in_bytes) {
  auto try_alloc = gcc.allocate(size_in_bytes);
  if (try_alloc) {
    return try_alloc;
  }
  gcc.collect();
  try_alloc = gcc.allocate(size_in_bytes);
  if (try_alloc) {
    return try_alloc;
  }
  std::cerr << "[ERROR] out of memory!" << std::endl;
  print_gc_alloc_stats();
  exit(1);
}

void print_gc_roots() { std::cout << gcc.dump_roots() << std::endl; }

void print_gc_alloc_stats() { std::cout << gcc.dump_stats() << std::endl; }

void print_gc_state() { std::cout << gcc.dump() << std::endl; }

void gc_read_barrier(void *obj, int) { gcc.read(obj); }

void gc_write_barrier(void *obj, int, void *contents) { gcc.write(obj, contents); }

void gc_push_root(void **ptr) { gcc.push_root(ptr); }

void gc_pop_root(void **ptr) { gcc.pop_root(ptr); }
