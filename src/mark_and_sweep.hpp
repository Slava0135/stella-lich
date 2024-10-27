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
    size_t n_blocks;
    size_t bytes_allocated;
    size_t bytes_free;
  };
  const size_t max_memory;

  using block_size_t = uint32_t;
  using done_t = uint16_t;
  using mark_t = uint16_t;
  using pointer_t = void *;

  MarkAndSweep(const size_t max_memory);

  Stats const &get_stats() const;
  const std::vector<void **> &get_roots() const;

  void push_root(void **root);
  void pop_root(void **root);

  void *allocate(std::size_t bytes);
  void collect();

private:
  enum Mark : mark_t {
    NOT_MARKED,
    MARKED,
    FREE,
  };

  struct Metadata {
    block_size_t block_size;
    done_t done;
    Mark mark;
  };

  static_assert(sizeof(Metadata) == sizeof(pointer_t));

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
  Metadata* get_metadata(size_t obj_idx);
};

} // namespace gc
