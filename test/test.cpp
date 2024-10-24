#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>

TEST_CASE("no objects", "[what]") {
  auto collector = gc::MarkAndSweep(0);
  REQUIRE(collector.get_stats().alive == 0);
}
