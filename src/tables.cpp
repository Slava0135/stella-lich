#include "tables.hpp"

#include <assert.h>

namespace tables {
void Table::add_row(std::vector<std::string> columns) {
  assert(columns.size() == column_sizes.size());
  std::string res;
  for (size_t i = 0; i < columns.size(); i++) {
    auto size = column_sizes.at(i);
    auto str = columns.at(i).substr(0, size);
    auto pad = std::string(1 + size - str.size(), ' ');
    res.append("| ");
    res.append(str + pad);
  }
  res.push_back('|');
  rows.push_back(res);
}
void Table::separator() {
  std::string res;
  for (size_t s : column_sizes) {
    res.push_back('+');
    res.append(std::string(2 + s, '-'));
  }
  res.push_back('+');
  rows.push_back(res);
}
std::string Table::to_string() const {
  std::string res;
  for (size_t i = 0; i < rows.size(); i++) {
    if (i > 0) {
      res.append("\n");
    }
    res.append(rows.at(i));
  }
  return res;
}
}  // namespace tables