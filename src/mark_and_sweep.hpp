#pragma once

#include <assert.h>
#include <cstdint>
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

  using done_t = uint32_t;
  using block_size_t = uint32_t;
  using pointer_t = void *;

  MarkAndSweep(const size_t max_memory)
      : max_memory(max_memory),
        stats_(Stats{.n_alive = 0, .n_roots = 0, .bytes_allocated = 0}) {
    assert(meta_info_size() == sizeof(pointer_t));
    space_ = std::make_unique<unsigned char[]>(max_memory);
    space_start_ = space_.get();
    space_end_ = &space_[max_memory - 1];
    auto first_block_idx = meta_info_size();
    freelist_ = &space_[first_block_idx];
    set_block_size(first_block_idx, max_memory - meta_info_size());
  }

  Stats const &get_stats() const;
  const std::vector<void **> &get_roots() const;

  void push_root(void **root);
  void pop_root(void **root);

  void *allocate(std::size_t bytes);

private:
  const void *space_start_;
  const void *space_end_;

  Stats stats_;
  std::unique_ptr<unsigned char[]> space_;
  std::vector<void **> roots_;
  void *freelist_;

  bool is_in_space(void *obj) const {
    return space_start_ <= obj && obj <= space_end_;
  }

  void set_block_size(size_t obj_idx, size_t size);

  constexpr size_t meta_info_size() const {
    return sizeof(done_t) + sizeof(block_size_t);
  }
};

} // namespace gc
