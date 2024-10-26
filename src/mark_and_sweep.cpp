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
  auto to_allocate = sizeof(done_t) + sizeof(block_size_t) + bytes;
  auto offset = to_allocate % sizeof(pointer_t);
  if (offset) {
    to_allocate += (sizeof(pointer_t) - offset);
  }
  assert(to_allocate % sizeof(pointer_t) == 0 &&
         "object address must be aligned to pointer size");
  assert(sizeof(done_t) + sizeof(block_size_t) + bytes <= to_allocate &&
         "memory at all times must fit all object fields and meta info");
  size_t next_bytes_allocated = this->stats_.bytes_allocated + to_allocate;
  if (next_bytes_allocated <= this->max_memory) {
    this->stats_.n_alive++;
    this->stats_.bytes_allocated = next_bytes_allocated;
    return reinterpret_cast<void *>(next_bytes_allocated);
  } else {
    return nullptr;
  }
}
const std::vector<void **> &MarkAndSweep::get_roots() const {
  return this->roots_;
}
void MarkAndSweep::set_block_size(size_t obj_idx, size_t size) {
  constexpr block_size_t max_size{std::numeric_limits<block_size_t>::max()};
  assert(size < max_size && "block size can't exceed limit");
  assert(sizeof(block_size_t) + sizeof(done_t) <= obj_idx);
  size_t block_size_idx = obj_idx - sizeof(block_size_t) - sizeof(done_t);
  space_[block_size_idx] = static_cast<block_size_t>(size);
}
} // namespace gc
