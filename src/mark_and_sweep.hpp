#pragma once

#include <assert.h>
#include <memory>
#include <stddef.h>
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
  const size_t pointer_size;

  MarkAndSweep(const size_t max_memory, const size_t pointer_size)
      : max_memory(max_memory), pointer_size(pointer_size),
        done_size_(pointer_size / 2), block_size_size_(pointer_size / 2),
        stats_(Stats{.n_alive = 0, .n_roots = 0, .bytes_allocated = 0}) {
    assert((pointer_size & (pointer_size - 1)) == 0 &&
           "pointer size must be power of 2");
    space_ = std::make_unique<unsigned char[]>(max_memory);
    space_start_ = space_.get();
    space_end_ = &space_[max_memory - 1];
  }

  Stats const &get_stats() const;
  const std::vector<void **> &get_roots() const;

  void push_root(void **root);
  void pop_root(void **root);

  void *allocate(std::size_t bytes);

private:
  const size_t done_size_;
  const size_t block_size_size_;
  const void *space_start_;
  const void *space_end_;

  Stats stats_;
  std::unique_ptr<unsigned char[]> space_;
  std::vector<void **> roots_;

  bool is_in_space(void *address) {
    return space_start_ <= address && address <= space_end_;
  }
};

} // namespace gc
