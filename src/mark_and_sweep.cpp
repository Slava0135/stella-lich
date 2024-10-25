#include "mark_and_sweep.hpp"

#include <assert.h>

namespace gc {
MarkAndSweep::Stats const& MarkAndSweep::get_stats() const {
  return this->stats_;
}
void MarkAndSweep::push_root(void** root) {
  this->stats_.n_roots++;
  this->roots_.push_back(root);
}
void MarkAndSweep::pop_root(void** root) {
  assert(this->stats_.n_roots > 0 &&
         "roots must not be empty when poping root");
  this->stats_.n_roots--;
  assert(this->roots_.back() == root &&
         "the root must be at the top of the stack");
  this->roots_.pop_back();
}
void* MarkAndSweep::allocate(std::size_t bytes) {
  assert(bytes > 0 && "can't allocate 0 bytes");
  auto to_allocate = bytes;
  bool has_pointers = true;
  // if object size is less than pointer size then it can't contain pointers and
  // we don't need `done` array
  if (to_allocate < this->pointer_size) {
    to_allocate = this->pointer_size;
    has_pointers = false;
  } else {
    to_allocate += this->done_size;
    auto offset = to_allocate % this->pointer_size;
    if (offset) {
      to_allocate += (pointer_size - offset);
    }
  }
  assert(to_allocate % pointer_size == 0 &&
         "object address must be aligned to pointer size");
  if (has_pointers) {
    assert(bytes + done_size <= to_allocate &&
           "memory at all times must fit all object fields and `done` array");
    assert(to_allocate >= this->pointer_size + this->done_size &&
           "not enough memory for algorithm");
  }
  size_t next_bytes_allocated = this->stats_.bytes_allocated + to_allocate;
  if (next_bytes_allocated <= this->max_memory) {
    this->stats_.n_alive++;
    this->stats_.bytes_allocated = next_bytes_allocated;
    return reinterpret_cast<void*>(next_bytes_allocated);
  } else {
    return nullptr;
  }
}
const std::vector<void**>& MarkAndSweep::get_roots() const {
  return this->roots_;
}
}  // namespace gc
