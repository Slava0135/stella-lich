#include "mark_and_sweep.hpp"

#include <assert.h>

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
  // everything aligned to pointer size
  //
  // meta information --> -1 | block_size | done array
  // object pointer   -->  0 | field
  //                       1 | ...
  auto to_allocate = meta_size_ + bytes;
  auto offset = to_allocate % this->pointer_size;
  if (offset) {
    to_allocate += (pointer_size - offset);
  }
  assert(to_allocate % pointer_size == 0 &&
         "object address must be aligned to pointer size");
  assert(this->block_size_size_ + this->done_size_ + bytes <= to_allocate &&
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
  assert(size < (static_cast<size_t>(1 << (block_size_size_))) &&
         "block size can't exceed limit");
  assert(block_size_size_ + done_size_ <= obj_idx);
  assert(block_size_size_ + done_size_ <= max_memory);
  size_t block_size_idx = obj_idx - done_size_ - block_size_size_;
  void *block_size_ref = &space_[block_size_idx];
  switch (block_size_size_) {
  case 0:
    break;
  case 1:
    *reinterpret_cast<uint8_t *>(block_size_ref) = size;
    break;
  case 2:
    *reinterpret_cast<uint16_t *>(block_size_ref) = size;
    break;
  case 4:
    *reinterpret_cast<uint16_t *>(block_size_ref) = size;
    break;
  default:
    assert("unsupported block size");
  }
}
} // namespace gc
