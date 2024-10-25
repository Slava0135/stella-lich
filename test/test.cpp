#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>

TEST_CASE("no objects") {
  gc::MarkAndSweep collector(0);
  REQUIRE(collector.get_stats().n_alive == 0);
}

TEST_CASE("push/pop roots") {
  gc::MarkAndSweep collector(0);
  REQUIRE(collector.get_stats().n_roots == 0);
  void* objects[2];
  void** root_a = &objects[0];
  void** root_b = &objects[1];
  collector.push_root(root_a);
  REQUIRE(collector.get_stats().n_roots == 1);
  collector.push_root(root_b);
  REQUIRE(collector.get_stats().n_roots == 2);
  collector.pop_root(root_b);
  collector.pop_root(root_a);
  REQUIRE(collector.get_stats().n_roots == 0);
}
