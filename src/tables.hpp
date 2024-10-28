#include <vector>
#include <string>

namespace tables {
class Table {
 public:
  Table(std::vector<size_t> column_sizes) : column_sizes(column_sizes) {}

  void add_row(std::vector<std::string> columns);
  void separator();
  std::string to_string() const;

 private:
  std::vector<size_t> column_sizes;
  std::vector<std::string> rows;
};
}  // namespace tables