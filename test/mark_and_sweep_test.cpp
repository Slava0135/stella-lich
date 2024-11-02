#include <assert.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <mark_and_sweep.hpp>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <vector>

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
  gc::MarkAndSweep collector(32, false, false, false);
  auto stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 0);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 0);
  REQUIRE(stats.bytes_free == 32);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
}

TEST_CASE("push/pop roots") {
  gc::MarkAndSweep collector(32, false, false, false);
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
  gc::MarkAndSweep collector(48, false, false, false);
  gc::Stats stats;
  REQUIRE(collector.allocate(1) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 16);
  REQUIRE(stats.bytes_free == 32);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 32);
  REQUIRE(stats.bytes_free == 16);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  REQUIRE(collector.allocate(9) == nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 32);
  REQUIRE(stats.bytes_free == 16);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
}

TEST_CASE("collect - no alive objects") {
  const size_t size = 256;
  gc::MarkAndSweep collector(size, false, false, false);
  gc::Stats stats;
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 16);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(collector.allocate(16) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 16 + 24);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(collector.allocate(24) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 3);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 16 + 24 + 32);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  collector.collect();
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 0);
  REQUIRE(stats.n_blocks_free == 4);
  REQUIRE(stats.bytes_used == 0);
  REQUIRE(stats.bytes_free == size);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
}

TEST_CASE("collect - one alive object") {
  const size_t size = 256;
  gc::MarkAndSweep collector(size, false, false, false);
  gc::Stats stats;
  void *obj = collector.allocate(8);
  stats = collector.get_stats();
  REQUIRE(obj != nullptr);
  REQUIRE(stats.n_blocks_used == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 16);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  collector.push_root(&obj);
  collector.collect();
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 1);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 16);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
}

TEST_CASE("collect - example 13.4 (A. Appel)") {
  const size_t size = 256;
  gc::MarkAndSweep collector(size, false, false, false);
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
  REQUIRE(stats.n_blocks_used == 7);
  REQUIRE(stats.n_blocks_total == 8);
  REQUIRE(stats.bytes_used == (5 * (8 + 16) + 2 * (8 + 8)));
  REQUIRE(stats.bytes_used + stats.bytes_free == size);

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

  REQUIRE(stats.n_blocks_used == 5);
  REQUIRE(stats.n_blocks_free == 3);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  REQUIRE(stats.bytes_used == 5 * (8 + 16));
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_used_max == 7);
  REQUIRE(stats.bytes_used_max == (5 * (8 + 16) + 2 * (8 + 8)));
}

TEST_CASE("allocate / collect - take all memory") {
  const size_t size = 64;
  gc::MarkAndSweep collector(size, false, false, false);
  gc::Stats stats;
  std::string dump;

  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_used == 4);
  REQUIRE(stats.n_blocks_free == 0);
  REQUIRE(stats.bytes_used == size);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) == nullptr);

  collector.collect();
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_used == 0);
  REQUIRE(stats.n_blocks_free == 4);
  REQUIRE(stats.bytes_used == 0);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);

  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_used == 4);
  REQUIRE(stats.n_blocks_free == 0);
  REQUIRE(stats.bytes_used == size);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) == nullptr);

  collector.collect();
  stats = collector.get_stats();
  dump = collector.dump();
  std::cout << dump << std::endl;
  REQUIRE(stats.n_blocks_used == 0);
  REQUIRE(stats.n_blocks_free == 4);
  REQUIRE(stats.bytes_used == 0);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
}

TEST_CASE("merge blocks") {
  const size_t size = 64;
  gc::MarkAndSweep collector(size, true, false, false);
  gc::Stats stats;
  std::string dump;

  REQUIRE(collector.allocate(8) != nullptr);
  void *obj = collector.allocate(8);
  REQUIRE(obj != nullptr);
  collector.push_root(&obj);
  REQUIRE(collector.allocate(8) != nullptr);
  REQUIRE(collector.allocate(8) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 4);
  REQUIRE(stats.n_blocks_free == 0);
  REQUIRE(stats.bytes_used == size);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  REQUIRE(collector.allocate(8) == nullptr);
  dump = collector.dump();
  std::cout << dump << std::endl;

  collector.collect();
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 1);
  REQUIRE(stats.n_blocks_free == 2);
  REQUIRE(stats.bytes_used == 16);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  dump = collector.dump();
  std::cout << dump << std::endl;

  REQUIRE(collector.allocate(24) != nullptr);
  stats = collector.get_stats();
  REQUIRE(stats.n_blocks_used == 2);
  REQUIRE(stats.n_blocks_free == 1);
  REQUIRE(stats.bytes_used == 48);
  REQUIRE(stats.bytes_used + stats.bytes_free == size);
  REQUIRE(stats.n_blocks_total == stats.n_blocks_used + stats.n_blocks_free);
  dump = collector.dump();
  std::cout << dump << std::endl;
}

template <typename T>
std::ostream &operator<<(std::ostream &out, const std::set<T> &set) {
  if (set.empty())
    return out << "{}";
  out << "{ " << *set.begin();
  std::for_each(std::next(set.begin()), set.end(),
                [&out](const T &element) { out << ", " << element; });
  return out << " }";
}

TEST_CASE("random") {
  std::mt19937 gen(123);

  struct Object {
    size_t n_fields;
    void *fields[];
  };

  const size_t cycles = 1000;
  const size_t links_per_object = 2;
  const size_t objects_per_root = 10;
  const size_t max_fields = 5;
  const size_t size = 10 * 1024;

  gc::MarkAndSweep collector(size, true, true, false);
  gc::Stats stats;
  std::string dump;

  std::set<Object *> alive_objects;

  for (size_t c = 0; c < cycles; c++) {
    std::cout << "cycle: " << c << std::endl;
    std::vector<Object *> roots;
    // generate new objects until full
    Object *obj = nullptr;
    while (true) {
      std::uniform_int_distribution<> field_distr(0, max_fields - 1);
      auto n_fields = field_distr(gen);
      obj = reinterpret_cast<Object *>(
          collector.allocate(sizeof(size_t) + n_fields * sizeof(void *)));
      if (!obj) {
        break;
      }
      obj->n_fields = n_fields;
      for (auto i = 0; i < n_fields; i++) {
        obj->fields[i] = nullptr;
      }
      alive_objects.insert(obj);
    }
    // link objects randomly
    std::vector<Object *> out;
    assert(alive_objects.size() >= 2);
    for (size_t i = 0; i < links_per_object * alive_objects.size(); i++) {
      out.clear();
      std::sample(alive_objects.begin(), alive_objects.end(),
                  std::back_inserter(out), 2, gen);
      auto obj = out[0];
      if (obj->n_fields > 0) {
        std::uniform_int_distribution<> field_distr(0, obj->n_fields - 1);
        auto field_i = field_distr(gen);
        collector.write(obj, out[1]);
        obj->fields[field_i] = out[1];
      }
    }
    // choose roots randomly
    roots.clear();
    auto n_roots = 1 + alive_objects.size() / objects_per_root;
    std::sample(alive_objects.begin(), alive_objects.end(),
                std::back_inserter(roots), n_roots, gen);
    assert(!roots.empty());
    // push roots
    for (size_t i = 0; i < roots.size(); i++) {
      Object **root = &roots[i];
      collector.push_root(reinterpret_cast<void **>(root));
    }
    // save all alive objects
    size_t bytes_used = 0;
    alive_objects.clear();
    std::queue<Object *> queue;
    for (Object *root : roots) {
      queue.push(root);
    }
    while (!queue.empty()) {
      Object *next = queue.front();
      queue.pop();
      if (!next || alive_objects.contains(next)) {
        continue;
      }
      bytes_used += sizeof(size_t) + next->n_fields * sizeof(void *);
      alive_objects.insert(next);
      collector.read(next);
      for (size_t i = 0; i < next->n_fields; i++) {
        queue.push(reinterpret_cast<Object *>(next->fields[i]));
      }
    }
    // collect
    // dump = collector.dump();
    // std::cout << dump << std::endl;
    collector.collect();
    // dump = collector.dump();
    // std::cout << dump << std::endl;
    // std::cout << alive_objects << std::endl;
    stats = collector.get_stats();
    REQUIRE(stats.n_blocks_used == alive_objects.size());
    REQUIRE(stats.bytes_free + stats.bytes_used == size);
    // pop roots
    for (size_t i = 0; i < roots.size(); i++) {
      Object **root = &roots[roots.size() - 1 - i];
      collector.pop_root(reinterpret_cast<void **>(root));
    }
  }
}

TEST_CASE("random (incremental)") {
  std::mt19937 gen(123);

  struct Object {
    size_t n_fields;
    void *fields[];
  };

  const size_t size = 128;
  const size_t iterations = 10000;

  const size_t max_fields = 3;

  const size_t target_roots_n = 5;

  const auto remove_root_chance = 0.1;
  const auto add_links_per_iteration = max_fields;

  gc::MarkAndSweep collector(size, true, true, true);
  gc::Stats stats;
  std::string dump;

  std::vector<Object *> roots;
  std::set<Object *> alive_objects;

  for (size_t i = 0; i < iterations; i++) {
    alive_objects.clear();
    // check invariants
    // dump = collector.dump();
    // std::cout << dump << std::endl;
    stats = collector.get_stats();
    REQUIRE(stats.n_blocks_used >= alive_objects.size());
    REQUIRE(stats.bytes_free + stats.bytes_used == size);
    // allocate new object
    std::uniform_int_distribution<> field_distr(0, max_fields - 1);
    auto n_fields = field_distr(gen);
    auto new_obj = reinterpret_cast<Object *>(
        collector.allocate(sizeof(size_t) + n_fields * sizeof(void *)));
    if (!new_obj) {
      collector.collect();
      stats = collector.get_stats();
      REQUIRE(stats.n_blocks_used == alive_objects.size());
      REQUIRE(stats.bytes_free + stats.bytes_used == size);
    } else {
      alive_objects.insert(new_obj);
      if (roots.size() < target_roots_n) {
        roots.push_back(new_obj);
        Object **root = &roots[roots.size() - 1];
        collector.push_root(reinterpret_cast<void **>(root));
      }
      dump = collector.dump();
      std::cout << dump << std::endl;
    }
    // find alive objects
    std::queue<Object *> queue;
    for (Object *root : roots) {
      queue.push(root);
    }
    while (!queue.empty()) {
      Object *next = queue.front();
      queue.pop();
      if (!next || alive_objects.contains(next)) {
        continue;
      }
      alive_objects.insert(next);
      collector.read(next);
      for (size_t i = 0; i < next->n_fields; i++) {
        queue.push(reinterpret_cast<Object *>(next->fields[i]));
      }
    }
    // remove root randomly
    std::uniform_real_distribution<> chance_distr(0.0, 1.0);
    if (roots.size() > 2 && chance_distr(gen) <= remove_root_chance) {
      std::uniform_int_distribution<> root_distr(0, roots.size() - 1);
      auto root_i = root_distr(gen);
      auto tmp = roots;
      for (size_t i = 0; i < roots.size(); i++) {
        Object **root = &roots[roots.size() - i - 1];
        collector.pop_root(reinterpret_cast<void **>(root));
        if (i != static_cast<size_t>(root_i)) {
          tmp.push_back(*root);
        }
      }
      assert(tmp.size() == roots.size() - 1);
      roots = tmp;
      for (size_t i = 0; i < roots.size(); i++) {
        Object **root = &roots[i];
        collector.push_root(reinterpret_cast<void **>(root));
      }
    }
    assert(roots.size() > 0);
    // link objects randomly
    std::vector<Object *> out;
    if (alive_objects.size() >= 2) {
      for (size_t i = 0; i < add_links_per_iteration; i++) {
        out.clear();
        std::sample(alive_objects.begin(), alive_objects.end(),
                    std::back_inserter(out), 2, gen);
        auto obj = out[0];
        if (obj->n_fields > 0) {
          std::uniform_int_distribution<> field_distr(0, obj->n_fields - 1);
          auto field_i = field_distr(gen);
          collector.write(obj, out[1]);
          obj->fields[field_i] = out[1];
        }
      }
    }
  }
}
