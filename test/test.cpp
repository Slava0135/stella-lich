#include <assert.h>
#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>

static_assert(sizeof(gc::MarkAndSweep::pointer_t) == 8);

struct A {
  A *x = nullptr;
  A *y = nullptr;
};

struct B {
  B *z = nullptr;
};

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

TEST_CASE("collect - no alive objects") {
  gc::MarkAndSweep collector(1000);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.get_stats().n_alive == 1);
  REQUIRE(collector.get_stats().n_blocks == 2);
  REQUIRE(collector.get_stats().bytes_allocated == 16);
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
  REQUIRE(collector.allocate(16) != nullptr);
  REQUIRE(collector.get_stats().n_alive == 2);
  REQUIRE(collector.get_stats().n_blocks == 3);
  REQUIRE(collector.get_stats().bytes_allocated == 16 + 24);
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
  REQUIRE(collector.allocate(24) != nullptr);
  REQUIRE(collector.get_stats().n_alive == 3);
  REQUIRE(collector.get_stats().n_blocks == 4);
  REQUIRE(collector.get_stats().bytes_allocated == 16 + 24 + 32);
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
  collector.collect();
  REQUIRE(collector.get_stats().n_alive == 0);
  REQUIRE(collector.get_stats().n_blocks == 4);
  REQUIRE(collector.get_stats().bytes_allocated == 0);
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
}

TEST_CASE("collect - one alive object") {
  gc::MarkAndSweep collector(1000);
  void *obj = collector.allocate(8);
  REQUIRE(obj != nullptr);
  REQUIRE(collector.get_stats().n_alive == 1);
  REQUIRE(collector.get_stats().n_blocks == 2);
  REQUIRE(collector.get_stats().bytes_allocated == 16);
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
  collector.push_root(&obj);
  collector.collect();
  REQUIRE(collector.get_stats().n_alive == 1);
  REQUIRE(collector.get_stats().n_blocks == 2);
  REQUIRE(collector.get_stats().bytes_allocated == 16);
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
}

TEST_CASE("collect - example 13.4 (A. Appel)") {
  gc::MarkAndSweep collector(1000);

  auto a_12 = reinterpret_cast<A *>(collector.allocate(sizeof(A)));
  auto a_15 = reinterpret_cast<A *>(collector.allocate(sizeof(A)));
  auto b_7 = reinterpret_cast<B *>(collector.allocate(sizeof(B)));
  auto a_37 = reinterpret_cast<A *>(collector.allocate(sizeof(A)));
  auto a_59 = reinterpret_cast<A *>(collector.allocate(sizeof(A)));
  auto b_9 = reinterpret_cast<B *>(collector.allocate(sizeof(B)));
  auto a_20 = reinterpret_cast<A *>(collector.allocate(sizeof(A)));

  a_15->x = a_12;
  a_15->y = a_37;
  a_37->x = a_20;
  a_37->y = a_59;

  b_7->z = b_9;
  b_9->z = b_7;

  REQUIRE(collector.get_stats().n_alive == 7);
  REQUIRE(collector.get_stats().n_blocks == 8);
  REQUIRE(collector.get_stats().bytes_allocated ==
          (5 * (8 + 16) + 2 * (8 + 8)));
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);

  collector.push_root(reinterpret_cast<void **>(&a_15));
  collector.push_root(reinterpret_cast<void **>(&a_37));

  collector.collect();
  REQUIRE(collector.get_stats().n_alive == 5);
  REQUIRE(collector.get_stats().n_blocks == 8);
  REQUIRE(collector.get_stats().bytes_allocated == 5 * (8 + 16));
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
}
