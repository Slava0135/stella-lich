#pragma once

#include <stddef.h>

#include <memory>

namespace gc {

class MarkAndSweep {
 public:
  struct Stats {
    size_t alive;
  };
  const size_t max_memory;

  MarkAndSweep(const size_t max_memory)
      : max_memory(max_memory), stats_(Stats{.alive = 0}) {
    space_ = new void*[max_memory];
  }

  Stats const& get_stats();

  ~MarkAndSweep() {
    delete[] space_;
  }

 private:
  Stats stats_;
  void** space_;
};

}  // namespace gc
