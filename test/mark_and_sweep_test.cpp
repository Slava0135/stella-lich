#include <assert.h>

#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>
#include <set>
#include <string>
#include <iostream>

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
  gc::MarkAndSweep collector(32, false);
  auto stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 0);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 0);
  REQUIRE(stats.bytes_free == 32);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
}

TEST_CASE("push/pop roots") {
  gc::MarkAndSweep collector(32, false);
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
  gc::MarkAndSweep collector(48, false);
  gc::Stats stats;
  REQUIRE(collector.allocate(1) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 16);
  REQUIRE(stats.bytes_free == 32);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 32);
  REQUIRE(stats.bytes_free == 16);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  REQUIRE(collector.allocate(9) == nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 32);
  REQUIRE(stats.bytes_free == 16);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
}

TEST_CASE("collect - no alive objects") {
  const size_t size = 256;
  gc::MarkAndSweep collector(size, false);
  gc::Stats stats;
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 16);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(collector.allocate(16) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 16 + 24);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(collector.allocate(24) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 3);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 16 + 24 + 32);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  collector.collect();
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 0);
  REQUIRE(stats.n_blocks_free == 4);
  REQUIRE(stats.bytes_allocated == 0);
  REQUIRE(stats.bytes_free == size);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
}

TEST_CASE("collect - one alive object") {
  const size_t size = 256;
  gc::MarkAndSweep collector(size, false);
  gc::Stats stats;
  void *obj = collector.allocate(8);
  stats = collector.get_stats();
  REQUIRE(obj != nullptr);
  REQUIRE(stats.n_blocks_allocated == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 16);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  collector.push_root(&obj);
  collector.collect();
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 16);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
}

TEST_CASE("collect - example 13.4 (A. Appel)") {
  const size_t size = 256;
  gc::MarkAndSweep collector(size, false);
  gc::Stats stats;
  std::string dump;

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

  collector.push_root(reinterpret_cast<void **>(&a_15));
  collector.push_root(reinterpret_cast<void **>(&a_37));

  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_allocated == 7);
  REQUIRE(stats.n_blocks_total == 8);
  REQUIRE(stats.bytes_allocated == (5 * (8 + 16) + 2 * (8 + 8)));
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);

  collector.collect();

  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;

  REQUIRE(a_12 == a_12_copy);
  REQUIRE(a_15 == a_15_copy);
  REQUIRE(a_37 == a_37_copy);
  REQUIRE(a_59 == a_59_copy);
  REQUIRE(a_20 == a_20_copy);

  std::set<std::string> collected_objects;
  for (auto obj : stats.collected_objects) {
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

  REQUIRE(stats.n_blocks_allocated == 5);
  REQUIRE(stats.n_blocks_free == 3);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  REQUIRE(stats.bytes_allocated == 5 * (8 + 16));
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
}

TEST_CASE("allocate / collect - take all memory") {
  const size_t size = 64;
  gc::MarkAndSweep collector(size, false);
  gc::Stats stats;
  std::string dump;

  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_allocated == 4);
  REQUIRE(stats.n_blocks_free == 0);
  REQUIRE(stats.bytes_allocated == size);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) == nullptr);

  collector.collect();
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_allocated == 0);
  REQUIRE(stats.n_blocks_free == 4);
  REQUIRE(stats.bytes_allocated == 0);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);

  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_allocated == 4);
  REQUIRE(stats.n_blocks_free == 0);
  REQUIRE(stats.bytes_allocated == size);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) == nullptr);

  collector.collect();
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_allocated == 0);
  REQUIRE(stats.n_blocks_free == 4);
  REQUIRE(stats.bytes_allocated == 0);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
}

TEST_CASE("merge blocks") {
  const size_t size = 64;
  gc::MarkAndSweep collector(size, true);
  gc::Stats stats;
  std::string dump;

  REQUIRE(collector.allocate(8) != nullptr);
  void *obj = collector.allocate(8);
  REQUIRE(obj != nullptr);
  collector.push_root(&obj);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 4);
  REQUIRE(stats.n_blocks_free == 0);
  REQUIRE(stats.bytes_allocated == size);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) == nullptr);
  dump = collector.dump();
  std::cout << dump << std::endl;

  collector.collect();
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 1);
  REQUIRE(stats.n_blocks_free == 2);
  REQUIRE(stats.bytes_allocated == 16);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  dump = collector.dump();
  std::cout << dump << std::endl;

  REQUIRE(collector.allocate(24) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_allocated == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_allocated == 48);
  REQUIRE(stats.bytes_allocated + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total ==
          stats.n_blocks_allocated + stats.n_blocks_free);
  dump = collector.dump();
  std::cout << dump << std::endl;
}