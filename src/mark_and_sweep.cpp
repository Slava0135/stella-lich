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
  auto next_bytes_allocated = this->stats_.bytes_allocated + bytes;
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
