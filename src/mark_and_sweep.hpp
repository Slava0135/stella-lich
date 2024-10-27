#pragma once

#include <assert.h>
#include <cstdint>
#include <memory>
#include <stddef.h>
#include <vector>
#include <limits>

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

  MarkAndSweep(const size_t max_memory);

  Stats const &get_stats() const;
  const std::vector<void **> &get_roots() const;

  void push_root(void **root);
  void pop_root(void **root);

  void *allocate(std::size_t bytes);
  void collect();

private:
  const done_t FREE_BLOCK{std::numeric_limits<done_t>::max()};

  const void *space_start_;
  const void *space_end_;

  Stats stats_;
  std::unique_ptr<unsigned char[]> space_;
  std::vector<void **> roots_;
  void *freelist_;

  void mark();
  void sweep();

  bool is_in_space(void const *obj) const;
  size_t pointer_to_idx(void const *obj) const;

  void set_block_size(size_t obj_idx, block_size_t size);
  size_t get_block_size(size_t obj_idx) const;

  void set_done_value(size_t obj_idx, done_t value);
  size_t get_done_value(size_t obj_idx) const;

  constexpr size_t meta_info_size() const {
    return sizeof(done_t) + sizeof(block_size_t);
  }
};

} // namespace gc
