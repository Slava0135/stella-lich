#include <assert.h>
#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>
#include <set>
#include <string>

#define NAME_OF(v) #v

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
  REQUIRE(collector.get_roots().size() == 0);
  void *objects[2];
  void **root_a = &objects[0];
  void **root_b = &objects[1];
  collector.push_root(root_a);
  REQUIRE(collector.get_roots().size() == 1);
  collector.push_root(root_b);
  REQUIRE(collector.get_roots().size() == 2);
  collector.pop_root(root_b);
  collector.pop_root(root_a);
  REQUIRE(collector.get_roots().size() == 0);
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

  auto a_12_copy = a_12;
  auto a_15_copy = a_15;
  auto a_37_copy = a_37;
  auto a_59_copy = a_59;
  auto a_20_copy = a_20;

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

  REQUIRE(a_12 == a_12_copy);
  REQUIRE(a_15 == a_15_copy);
  REQUIRE(a_37 == a_37_copy);
  REQUIRE(a_59 == a_59_copy);
  REQUIRE(a_20 == a_20_copy);

  std::set<std::string> collected_objects;
  for (auto obj : collector.get_stats().collected_objects) {
    if (obj == a_12) {
      collected_objects.insert(NAME_OF(a_12));
    } else if (obj == a_15) {
      collected_objects.insert(NAME_OF(a_15));
    } else if (obj == b_7) {
      collected_objects.insert(NAME_OF(b_7));
    } else if (obj == a_37) {
      collected_objects.insert(NAME_OF(a_37));
    } else if (obj == a_59) {
      collected_objects.insert(NAME_OF(a_59));
    } else if (obj == b_9) {
      collected_objects.insert(NAME_OF(b_9));
    } else if (obj == a_20) {
      collected_objects.insert(NAME_OF(a_20));
    }
  }
  std::set<std::string> expected{NAME_OF(b_7), NAME_OF(b_9)};
  REQUIRE(collected_objects == expected);

  REQUIRE(collector.get_stats().n_alive == 5);
  REQUIRE(collector.get_stats().n_blocks == 8);
  REQUIRE(collector.get_stats().bytes_allocated == 5 * (8 + 16));
  REQUIRE(collector.get_stats().bytes_allocated +
              collector.get_stats().bytes_free ==
          1000);
}
