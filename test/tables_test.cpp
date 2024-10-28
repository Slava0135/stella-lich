#include <catch2/catch_test_macros.hpp>
#include <tables.hpp>

TEST_CASE("tables") {
  tables::Table t({5, 6, 7});
  t.separator();
  t.add_row({"FIRST", "SECOND", "  THIRD"});
  t.separator();
  t.add_row({"123", "456789", " 13"});
  t.add_row({"", "idk", "  ?"});
  t.separator();
  auto expected =
      "+-------+--------+---------+\n"
      "| FIRST | SECOND |   THIRD |\n"
      "+-------+--------+---------+\n"
      "| 123   | 456789 |  13     |\n"
      "|       | idk    |   ?     |\n"
      "+-------+--------+---------+";
  REQUIRE(t.to_string() == expected);
}
