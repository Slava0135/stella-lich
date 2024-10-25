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
  assert(this->stats_.n_roots > 0 && "roots must not be empty when poping root");
  this->stats_.n_roots--;
  assert(this->roots_.back() == root && "the root must be at the top of the stack");
  this->roots_.pop_back();
}
}  // namespace gc
