#pragma once

#include <stddef.h>

namespace gc {

class MarkAndSweep {
 public:
  struct Stats {
    size_t alive;
  };
  const size_t max_memory;

  MarkAndSweep(const size_t max_memory)
      : max_memory(max_memory), stats_(Stats{.alive = 0}) {}

  Stats const& get_stats();

 private:
  Stats stats_;
};

}  // namespace gc
