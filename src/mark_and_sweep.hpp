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
    size_t bytes_allocated;
  };
  const size_t max_memory;
  const size_t align_in_bytes;

  MarkAndSweep(const size_t max_memory, const size_t align_in_bytes)
      : max_memory(max_memory),
        align_in_bytes(align_in_bytes),
        stats_(Stats{.n_alive = 0, .n_roots = 0, .bytes_allocated = 0}) {
    space_ = std::make_unique<unsigned char[]>(max_memory);
  }

  Stats const& get_stats() const;
  const std::vector<void**>& get_roots() const;

  void push_root(void** root);
  void pop_root(void** root);

  void* allocate(std::size_t bytes);

 private:
  Stats stats_;
  std::unique_ptr<unsigned char[]> space_;
  std::vector<void**> roots_;
};

}  // namespace gc
