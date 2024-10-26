#include "mark_and_sweep.hpp"

#include <assert.h>
#include <limits>

namespace gc {

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
  // meta information --> -1 | block_size | done array
  // object pointer   -->  0 | field
  //                       1 | ...
  auto allocate_at_least = meta_info_size() + bytes;
  auto to_allocate = allocate_at_least;
  auto offset = to_allocate % sizeof(pointer_t);
  if (offset) {
    to_allocate += (sizeof(pointer_t) - offset);
  }
  assert(to_allocate % sizeof(pointer_t) == 0 &&
         "object address must be aligned to pointer size");
  assert(allocate_at_least <= to_allocate &&
         "allocated memory must fit all object fields and meta info");

  void **prev_free_block = &freelist_;
  void *free_block = freelist_;
  while (free_block) {
    auto block_idx = pointer_to_idx(free_block);
    auto block_size = get_block_size(block_idx);
    if (block_size == to_allocate) {
      set_block_size(block_idx, to_allocate);
      stats_.n_alive += 1;
      stats_.bytes_allocated += to_allocate;
      return free_block;
    } else if (block_size >=
               to_allocate + (meta_info_size() + sizeof(pointer_t))) {
      // take required space and split remaining into new block
      set_block_size(block_idx, to_allocate);
      auto new_block_idx = block_idx + to_allocate + meta_info_size();
      auto new_block_size = block_size - to_allocate - meta_info_size();
      set_block_size(new_block_idx, new_block_size);
      *prev_free_block = &space_[new_block_idx];
      stats_.n_alive += 1;
      stats_.bytes_allocated += to_allocate;
      return free_block;
    } else if (block_size > to_allocate) {
      // can't split block, fill the block fully instead and zero unused part
      for (size_t idx = block_idx + to_allocate; idx < block_idx - block_size;
           idx++) {
        space_[idx] = 0;
      }
      stats_.n_alive += 1;
      stats_.bytes_allocated += block_size;
      return free_block;
    }
    prev_free_block = reinterpret_cast<void **>(&space_[block_idx]);
    free_block = *prev_free_block;
  }
  // out of free blocks
  return nullptr;
}

void MarkAndSweep::collect() {}

const std::vector<void **> &MarkAndSweep::get_roots() const {
  return this->roots_;
}

bool MarkAndSweep::is_in_space(void *obj) const {
  return reinterpret_cast<uintptr_t>(space_start_) + meta_info_size() <=
             reinterpret_cast<uintptr_t>(obj) &&
         reinterpret_cast<uintptr_t>(obj) <
             reinterpret_cast<uintptr_t>(space_end_);
}

size_t MarkAndSweep::pointer_to_idx(void *obj) const {
  assert(is_in_space(obj));
  auto idx = reinterpret_cast<uintptr_t>(obj) - reinterpret_cast<uintptr_t>(space_start_);
  assert(idx % sizeof(pointer_t) == 0 &&
         "all objects must be aligned to pointer size");
  assert(idx < max_memory);
  return idx;
}

void MarkAndSweep::set_block_size(size_t obj_idx, size_t size) {
  constexpr block_size_t max_size{std::numeric_limits<block_size_t>::max()};
  assert(size < max_size && "block size can't exceed limit");
  size_t block_size_idx = obj_idx - sizeof(block_size_t) - sizeof(done_t);
  *reinterpret_cast<block_size_t *>(&space_[block_size_idx]) = size;
  assert(get_block_size(obj_idx) == size);
}

size_t MarkAndSweep::get_block_size(size_t obj_idx) const {
  size_t block_size_idx = obj_idx - sizeof(block_size_t) - sizeof(done_t);
  return *reinterpret_cast<block_size_t *>(&space_[block_size_idx]);
}

} // namespace gc
