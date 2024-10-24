#include "mark_and_sweep.hpp"

namespace gc {
MarkAndSweep::Stats const& MarkAndSweep::get_stats() {
    return this->stats_;
}
}
