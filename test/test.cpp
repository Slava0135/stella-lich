#include <catch2/catch_test_macros.hpp>
#include <mark_and_sweep.hpp>

TEST_CASE("no objects") {
  gc::MarkAndSweep collector(0);
  REQUIRE(collector.get_stats().alive == 0);
}
