#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>

TEST_CASE("no objects") {
  gc::MarkAndSweep collector(0, 1);
  REQUIRE(collector.get_stats().n_alive == 0);
}

TEST_CASE("push/pop roots") {
  gc::MarkAndSweep collector(0, 1);
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

TEST_CASE("allocate") {
  SECTION("one byte - one object") {
    gc::MarkAndSweep collector(1, 1);
    REQUIRE(collector.get_stats().n_alive == 0);
    REQUIRE(collector.allocate(1) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 1);
    REQUIRE(collector.get_stats().bytes_allocated == 1);
    REQUIRE(collector.allocate(1) == nullptr);
    REQUIRE(collector.get_stats().n_alive == 1);
    REQUIRE(collector.get_stats().bytes_allocated == 1);
  }
  SECTION("many bytes - many objects") {
    gc::MarkAndSweep collector(10, 1);
    REQUIRE(collector.get_stats().n_alive == 0);
    REQUIRE(collector.allocate(3) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 1);
    REQUIRE(collector.get_stats().bytes_allocated == 3);
    REQUIRE(collector.allocate(4) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 2);
    REQUIRE(collector.get_stats().bytes_allocated == 7);
    REQUIRE(collector.allocate(5) == nullptr);
    REQUIRE(collector.get_stats().n_alive == 2);
    REQUIRE(collector.get_stats().bytes_allocated == 7);
  }
  SECTION("align objects") {
    gc::MarkAndSweep collector(21, 4);
    REQUIRE(collector.get_stats().n_alive == 0);
    REQUIRE(collector.allocate(1) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 1);
    REQUIRE(collector.get_stats().bytes_allocated == 8);
    REQUIRE(collector.allocate(4) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 2);
    REQUIRE(collector.get_stats().bytes_allocated == 16);
    REQUIRE(collector.allocate(5) == nullptr);
    REQUIRE(collector.get_stats().n_alive == 2);
    REQUIRE(collector.get_stats().bytes_allocated == 16);
  }
}
