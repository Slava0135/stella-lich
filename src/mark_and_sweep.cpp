#include "mark_and_sweep.hpp"

#include <assert.h>
#include <limits>

namespace gc {

MarkAndSweep::MarkAndSweep(const size_t max_memory)
    : max_memory(max_memory),
      stats_(Stats{.n_alive = 0, .n_roots = 0, .n_blocks = 1, .bytes_allocated = 0, .bytes_free = max_memory}) {
  space_ = std::make_unique<unsigned char[]>(max_memory);
  space_start_ = space_.get();
  assert(reinterpret_cast<uintptr_t>(space_start_) % sizeof(pointer_t) == 0 &&
         "space start address must be aligned to pointer size");
  space_end_ = &space_[max_memory];
  auto first_block_idx = sizeof(Metadata);
  freelist_ = &space_[first_block_idx];
  auto metadata = get_metadata(first_block_idx);
  metadata->block_size = max_memory;
  metadata->done = 0;
  metadata->mark = FREE;
}

MarkAndSweep::Stats const &MarkAndSweep::get_stats() const {
  return this->stats_;
}

void MarkAndSweep::push_root(void **root) {
  this->stats_.n_roots++;
  this->roots_.push_back(root);
}

void MarkAndSweep::pop_root(void **root) {
  assert(this->stats_.n_roots > 0 &&
         "roots must not be empty when poping root");
  this->stats_.n_roots--;
  assert(this->roots_.back() == root &&
         "the root must be at the top of the stack");
  this->roots_.pop_back();
}

void *MarkAndSweep::allocate(std::size_t bytes) {
  assert(bytes > 0 && "can't allocate 0 bytes");
  // BLOCK STRUCTURE
  // everything aligned to pointer size (void*)
  //
  //                      -1 | metadata
  // object pointer   -->  0 | field
  //                       1 | ...
  auto allocate_at_least = sizeof(Metadata) + bytes;
  auto to_allocate = allocate_at_least;
  auto offset = to_allocate % sizeof(pointer_t);
  if (offset) {
    to_allocate += (sizeof(pointer_t) - offset);
  }
  assert(to_allocate % sizeof(pointer_t) == 0 &&
         "object address must be aligned to pointer size");
  assert(allocate_at_least <= to_allocate &&
         "allocated memory must fit all object fields and metadata");

  void **prev_free_block = &freelist_;
  void *free_block = freelist_;
  while (free_block) {
    auto block_idx = pointer_to_idx(free_block);
    auto block_meta = get_metadata(block_idx);
    assert(block_meta->mark == FREE);
    if (block_meta->block_size == to_allocate) {
      block_meta->done = 0;
      block_meta->mark = NOT_MARKED;
      stats_.n_alive += 1;
      stats_.bytes_free -= block_meta->block_size;
      stats_.bytes_allocated += block_meta->block_size;
      return free_block;
    } else if (block_meta->block_size > to_allocate &&
               block_meta->block_size - to_allocate >=
                   sizeof(Metadata) + sizeof(pointer_t)) {
      // take required space and split remaining into new block
      auto new_block_idx = block_idx + to_allocate;
      auto new_block_meta = get_metadata(new_block_idx);
      new_block_meta->block_size = block_meta->block_size - to_allocate;
      new_block_meta->done = 0;
      new_block_meta->mark = FREE;
      stats_.n_blocks += 1;
      *prev_free_block = &space_[new_block_idx];

      block_meta->block_size = to_allocate;
      block_meta->done = 0;
      block_meta->mark = NOT_MARKED;
      stats_.n_alive += 1;
      stats_.bytes_free -= block_meta->block_size;
      stats_.bytes_allocated += block_meta->block_size;
      return free_block;
    } else if (block_meta->block_size > to_allocate) {
      // can't split block, fill the block instead and zero unused part
      for (size_t idx = block_idx + to_allocate;
           idx < block_idx - block_meta->block_size; idx++) {
        space_[idx] = 0;
      }
      block_meta->done = 0;
      block_meta->mark = NOT_MARKED;
      stats_.bytes_free -= block_meta->block_size;
      stats_.n_alive += 1;
      stats_.bytes_allocated += block_meta->block_size;
      return free_block;
    }
    prev_free_block = reinterpret_cast<void **>(&space_[block_idx]);
    free_block = *prev_free_block;
  }
  // out of free blocks
  return nullptr;
}

void MarkAndSweep::collect() {
  mark();
  sweep();
}

void MarkAndSweep::mark() {
  for (auto root : roots_) {
    auto x = *root;
    if (!is_in_space(x)) {
      continue;
    }
    auto x_i = pointer_to_idx(x);
    auto x_meta = get_metadata(x_i);
    if (x_meta->mark == NOT_MARKED) {
      void *tmp = nullptr;
      x_meta->mark = MARKED;
      x_meta->done = 0;
      while (true) {
        x_i = pointer_to_idx(x);
        x_meta = get_metadata(x_i);
        auto i = x_meta->done;
        auto obj_size = x_meta->block_size - sizeof(Metadata);
        assert(obj_size % sizeof(pointer_t) == 0);
        auto field_n = obj_size / sizeof(pointer_t);
        if (i < field_n) {
          auto field_i_addr =
              reinterpret_cast<uintptr_t>(x) + i * sizeof(pointer_t);
          auto y = reinterpret_cast<void *>(field_i_addr);
          if (is_in_space(y)) {
            auto y_i = pointer_to_idx(y);
            auto y_meta = get_metadata(y_i);
            if (y_meta->mark == NOT_MARKED) {
              *reinterpret_cast<void **>(field_i_addr) = tmp;
              tmp = x;
              x = y;
              continue;
            }
          }
          x_meta->done++;
        } else {
          auto y = x;
          auto x = tmp;
          if (!x) {
            return;
          }
          x_i = pointer_to_idx(x);
          x_meta = get_metadata(x_i);
          auto i = x_meta->done;
          auto field_i_addr =
              reinterpret_cast<uintptr_t>(x) + i * sizeof(pointer_t);
          tmp = reinterpret_cast<void *>(field_i_addr);
          *reinterpret_cast<void **>(field_i_addr) = y;
          x_meta->done++;
        }
      }
    }
  }
}

void MarkAndSweep::sweep() {
  auto p = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(space_start_) +
                                    sizeof(Metadata));
  while (p < space_end_) {
    auto block_idx = pointer_to_idx(p);
    auto block_meta = get_metadata(block_idx);
    if (block_meta->mark == MARKED) {
      block_meta->mark = NOT_MARKED;
    } else if (block_meta->mark == NOT_MARKED) {
      *reinterpret_cast<void **>(p) = freelist_;
      freelist_ = p;
      assert(stats_.n_alive > 0);
      assert(stats_.bytes_allocated >= block_meta->block_size);
      stats_.n_alive -= 1;
      stats_.bytes_allocated -= block_meta->block_size;
      stats_.bytes_free += block_meta->block_size;
    }
    p = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(p) +
                                 block_meta->block_size);
  }
}

const std::vector<void **> &MarkAndSweep::get_roots() const {
  return this->roots_;
}

bool MarkAndSweep::is_in_space(void const *obj) const {
  return reinterpret_cast<uintptr_t>(space_start_) + sizeof(Metadata) <=
             reinterpret_cast<uintptr_t>(obj) &&
         reinterpret_cast<uintptr_t>(obj) <
             reinterpret_cast<uintptr_t>(space_end_);
}

size_t MarkAndSweep::pointer_to_idx(void const *obj) const {
  assert(is_in_space(obj));
  auto idx = reinterpret_cast<uintptr_t>(obj) -
             reinterpret_cast<uintptr_t>(space_start_);
  assert(idx % sizeof(pointer_t) == 0 &&
         "all objects must be aligned to pointer size");
  assert(idx < max_memory);
  return idx;
}

MarkAndSweep::Metadata *MarkAndSweep::get_metadata(size_t obj_idx) {
  size_t metadata_idx = obj_idx - sizeof(Metadata);
  auto res = reinterpret_cast<Metadata *>(&space_[metadata_idx]);
  assert(res->block_size <= max_memory &&
         "potential memory corruption detected");
  assert(
      (res->mark == NOT_MARKED || res->mark == MARKED || res->mark == FREE) &&
      "potential memory corruption detected");
  return res;
}

} // namespace gc
