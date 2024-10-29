#pragma once

#include <assert.h>
#include <stddef.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace gc {

struct Stats {
  size_t n_blocks_used;
  size_t n_blocks_free;
  size_t n_blocks_total;
  size_t n_blocks_used_max;

  size_t bytes_used;
  size_t bytes_free;
  size_t bytes_used_max;

  size_t reads;
  size_t writes;

  size_t collections;
  std::vector<void *> collected_objects;
};

class MarkAndSweep {
 public:
  const size_t max_memory;
  const bool merge_blocks;
  const bool skip_first_field;

  using block_size_t = uint32_t;
  using done_t = uint16_t;
  using mark_t = uint16_t;
  using pointer_t = void *;

  MarkAndSweep(size_t max_memory, bool merge_blocks, bool skip_first_field);

  Stats get_stats() const;
  const std::vector<void **> &get_roots() const;

  void push_root(void **root);
  void pop_root(void **root);

  void *allocate(std::size_t bytes);
  void collect();

  void read();
  void write();

  std::string dump() const;
  std::string dump_stats() const;
  std::string dump_roots() const;
  std::string dump_blocks() const;

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

  void dfs(void *x);
  void mark();
  void sweep();
  void merge();

  bool is_in_space(void const *obj) const;
  bool is_valid_free_block(void const *obj) const;

  size_t pointer_to_idx(void const *obj) const;
  Metadata *get_metadata(size_t obj_idx) const;
};

}  // namespace gc
