#pragma once

#include <stddef.h>

#include <memory>
#include <vector>

namespace gc {

class MarkAndSweep {
 public:
  struct Stats {
    size_t n_alive;
    size_t n_roots;
  };
  const size_t max_memory;

  MarkAndSweep(const size_t max_memory)
      : max_memory(max_memory), stats_(Stats{.n_alive = 0, .n_roots = 0}) {
    space_ = new void*[max_memory];
  }

  Stats const& get_stats() const;

  void push_root(void** root);
  void pop_root(void** root);

  ~MarkAndSweep() { delete[] space_; }

 private:
  Stats stats_;
  void** space_;
  std::vector<void **> roots_;
};

}  // namespace gc
