#include <assert.h>
#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>

static_assert(sizeof(gc::MarkAndSweep::pointer_t) == 8);

TEST_CASE("no objects") {
  gc::MarkAndSweep collector(11);
  REQUIRE(collector.get_stats().n_alive == 0);
  REQUIRE(collector.get_stats().n_blocks == 1);
  REQUIRE(collector.get_stats().bytes_allocated == 0);
  REQUIRE(collector.get_stats().bytes_free == 11);
}

TEST_CASE("push/pop roots") {
  gc::MarkAndSweep collector(42);
  REQUIRE(collector.get_stats().n_roots == 0);
  void *objects[2];
  void **root_a = &objects[0];
  void **root_b = &objects[1];
  collector.push_root(root_a);
  REQUIRE(collector.get_stats().n_roots == 1);
  collector.push_root(root_b);
  REQUIRE(collector.get_stats().n_roots == 2);
  collector.pop_root(root_b);
  collector.pop_root(root_a);
  REQUIRE(collector.get_stats().n_roots == 0);
}

TEST_CASE("allocate") {
  gc::MarkAndSweep collector(48);
  REQUIRE(collector.allocate(1) != nullptr);
  REQUIRE(collector.get_stats().n_alive == 1);
  REQUIRE(collector.get_stats().n_blocks == 2);
  REQUIRE(collector.get_stats().bytes_allocated == 16);
  REQUIRE(collector.get_stats().bytes_free == 32);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.get_stats().n_alive == 2);
  REQUIRE(collector.get_stats().n_blocks == 3);
  REQUIRE(collector.get_stats().bytes_allocated == 32);
  REQUIRE(collector.get_stats().bytes_free == 16);
  REQUIRE(collector.allocate(9) == nullptr);
  REQUIRE(collector.get_stats().n_alive == 2);
  REQUIRE(collector.get_stats().n_blocks == 3);
  REQUIRE(collector.get_stats().bytes_allocated == 32);
  REQUIRE(collector.get_stats().bytes_free == 16);
}

TEST_CASE("collect") {
  SECTION("no alive objects") {
    gc::MarkAndSweep collector(1000);
    REQUIRE(collector.allocate(8) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 1);
    REQUIRE(collector.get_stats().n_blocks == 2);
    REQUIRE(collector.get_stats().bytes_allocated == 16);
    REQUIRE(collector.get_stats().bytes_allocated + collector.get_stats().bytes_free == 1000);
    REQUIRE(collector.allocate(16) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 2);
    REQUIRE(collector.get_stats().n_blocks == 3);
    REQUIRE(collector.get_stats().bytes_allocated == 16 + 24);
    REQUIRE(collector.get_stats().bytes_allocated + collector.get_stats().bytes_free == 1000);
    REQUIRE(collector.allocate(24) != nullptr);
    REQUIRE(collector.get_stats().n_alive == 3);
    REQUIRE(collector.get_stats().n_blocks == 4);
    REQUIRE(collector.get_stats().bytes_allocated == 16 + 24 + 32);
    REQUIRE(collector.get_stats().bytes_allocated + collector.get_stats().bytes_free == 1000);
    collector.collect();
    REQUIRE(collector.get_stats().n_alive == 0);
    REQUIRE(collector.get_stats().n_blocks == 4);
    REQUIRE(collector.get_stats().bytes_allocated == 0);
    REQUIRE(collector.get_stats().bytes_allocated + collector.get_stats().bytes_free == 1000);
  }
  SECTION("one alive object") {
    gc::MarkAndSweep collector(1000);
    REQUIRE(collector.get_stats().n_alive == 0);
    void *obj = collector.allocate(8);
    REQUIRE(obj != nullptr);
    REQUIRE(collector.get_stats().n_alive == 1);
    REQUIRE(collector.get_stats().n_blocks == 2);
    REQUIRE(collector.get_stats().bytes_allocated == 16);
    REQUIRE(collector.get_stats().bytes_allocated + collector.get_stats().bytes_free == 1000);
    collector.push_root(&obj);
    collector.collect();
    REQUIRE(collector.get_stats().n_alive == 1);
    REQUIRE(collector.get_stats().n_blocks == 2);
    REQUIRE(collector.get_stats().bytes_allocated == 16);
    REQUIRE(collector.get_stats().bytes_allocated + collector.get_stats().bytes_free == 1000);
  }
}
