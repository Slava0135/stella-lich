#include "mark_and_sweep.hpp"

#include <assert.h>

#include <algorithm>
#include <format>
#include <iostream>
#include <limits>

#include "tables.hpp"

namespace gc {

bool log(std::string msg) {
#ifndef NDEBUG
  std::cout << msg << std::endl;
#endif
  return false;
}

std::string pointer_to_hex(void *ptr) {
  auto v = reinterpret_cast<uintptr_t>(ptr);
  std::string res;
  for (size_t i = 0; i < 2 * sizeof(void *); i++) {
    if (i > 0 && i % 2 == 0) {
      res.append(" ");
    }
    auto digit = v % 16;
    switch (digit) {
    case 10:
      res.append("a");
      break;
    case 11:
      res.append("b");
      break;
    case 12:
      res.append("c");
      break;
    case 13:
      res.append("d");
      break;
    case 14:
      res.append("e");
      break;
    case 15:
      res.append("f");
      break;
    default:
      res.append(std::format("{}", digit));
      break;
    }
    v = v >> 4;
  }
  std::reverse(res.begin(), res.end());
  return res;
}

MarkAndSweep::MarkAndSweep(size_t max_memory, bool merge_blocks,
                           bool skip_first_field, bool incremental)
    : max_memory(max_memory), merge_blocks(merge_blocks),
      skip_first_field(skip_first_field), incremental(incremental),
      stats_(Stats{.n_blocks_used = 0,
                   .n_blocks_free = 1,
                   .n_blocks_total = 1,
                   .n_blocks_used_max = 0,
                   .bytes_used = 0,
                   .bytes_free = max_memory,
                   .bytes_used_max = 0,
                   .reads = 0,
                   .writes = 0,
                   .collections = 0,
                   .incremental_collections = 0,
                   .collected_objects = std::vector<void *>()}) {
  log("create space");
  space_ = std::make_unique<unsigned char[]>(max_memory);
  space_start_ = space_.get();
  space_end_ = &space_[max_memory];
  auto max_allowed_memory = static_cast<size_t>(1)
                            << (8 * sizeof(block_size_t));
  assert((max_memory < max_allowed_memory &&
          "max memory must be less than max block size") ||
         log(std::format("{} is >= {}", max_memory, max_allowed_memory)));
  assert(reinterpret_cast<uintptr_t>(space_start_) % sizeof(pointer_t) == 0 &&
         "space start address must be aligned to pointer size");

  log("create first block");
  auto first_block_idx = sizeof(Metadata);
  auto metadata = get_metadata(first_block_idx);
  metadata->block_size = max_memory;
  metadata->done = 0;
  metadata->mark = FREE;

  log("init freelist");
  *reinterpret_cast<void **>(&space_[first_block_idx]) = nullptr;
  freelist_ = &space_[first_block_idx];
}

Stats MarkAndSweep::get_stats() const { return this->stats_; }

void MarkAndSweep::push_root(void **root) {
  this->roots_.push_back(root);
  if (incremental && is_in_space(*root)) {
    if (phase_ == MARK) {
      mark_queue_.push(*root);
    } else if (phase_ == SWEEP && resume_sweep_from <= *root) {
      auto block_idx = pointer_to_idx(*root);
      auto block_meta = get_metadata(block_idx);
      block_meta->mark = MARKED;
    }
  }
}

void MarkAndSweep::pop_root(void **root) {
  assert(this->roots_.size() > 0 && "roots must not be empty when poping root");
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

  if (incremental) {
    incr_collect(bytes_to_free_per_alloc_ * to_allocate);
  }

  void **prev_free_block = &freelist_;
  void *free_block = freelist_;
  log("allocate");
  while (free_block) {
    auto block_idx = pointer_to_idx(free_block);
    auto block_meta = get_metadata(block_idx);
    assert(block_meta->mark == FREE);
    if (block_meta->block_size == to_allocate) {
      // update meta
      block_meta->done = 0;
      block_meta->mark = NOT_MARKED;
      if (incremental && phase_ == SWEEP && resume_sweep_from <= free_block) {
        block_meta->mark = MARKED;
      }
      // update stats
      stats_.n_blocks_free--;
      stats_.n_blocks_used++;
      stats_.n_blocks_used_max =
          std::max(stats_.n_blocks_used_max, stats_.n_blocks_used);
      stats_.bytes_free -= block_meta->block_size;
      stats_.bytes_used += block_meta->block_size;
      stats_.bytes_used_max =
          std::max(stats_.bytes_used_max, stats_.bytes_used);
      // remove from freelist
      *prev_free_block = *reinterpret_cast<void **>(&space_[block_idx]);
      assert(is_valid_free_block(*prev_free_block));
      log("same size block");
      log(pointer_to_hex(free_block));
      return free_block;
    } else if (block_meta->block_size > to_allocate &&
               block_meta->block_size - to_allocate >=
                   sizeof(Metadata) + sizeof(pointer_t)) {
      // take required space and split remaining into new block
      auto new_block_idx = block_idx + to_allocate;
      auto new_block_meta = reinterpret_cast<Metadata *>(
          &space_[new_block_idx - sizeof(Metadata)]);
      new_block_meta->block_size = block_meta->block_size - to_allocate;
      new_block_meta->done = 0;
      new_block_meta->mark = FREE;
      // save next free block
      *reinterpret_cast<void **>(&space_[new_block_idx]) =
          *reinterpret_cast<void **>(&space_[block_idx]);
      // update meta
      block_meta->block_size = to_allocate;
      block_meta->done = 0;
      block_meta->mark = NOT_MARKED;
      if (incremental && phase_ == SWEEP && resume_sweep_from <= free_block) {
        block_meta->mark = MARKED;
      }
      // update stats
      stats_.n_blocks_total++;
      stats_.n_blocks_used++;
      stats_.n_blocks_used_max =
          std::max(stats_.n_blocks_used_max, stats_.n_blocks_used);
      stats_.bytes_free -= block_meta->block_size;
      stats_.bytes_used += block_meta->block_size;
      stats_.bytes_used_max =
          std::max(stats_.bytes_used_max, stats_.bytes_used);
      // remove from freelist
      *prev_free_block = &space_[new_block_idx];
      assert(is_valid_free_block(*prev_free_block));
      log("new block");
      log(pointer_to_hex(free_block));
      return free_block;
    } else if (block_meta->block_size > to_allocate) {
      // can't split block, fill the block instead and zero unused part
      auto obj_size = block_meta->block_size - sizeof(Metadata);
      assert(obj_size % sizeof(pointer_t) == 0);
      auto field_n = obj_size / sizeof(pointer_t);
      for (size_t i = 0; i < field_n; i++) {
        auto field_i_addr = &space_[block_idx + i * sizeof(pointer_t)];
        *reinterpret_cast<void **>(field_i_addr) = nullptr;
      }
      // update meta
      block_meta->done = 0;
      block_meta->mark = NOT_MARKED;
      if (incremental && phase_ == SWEEP && resume_sweep_from <= free_block) {
        block_meta->mark = MARKED;
      }
      // update stats
      stats_.n_blocks_used++;
      stats_.n_blocks_free--;
      stats_.n_blocks_used_max =
          std::max(stats_.n_blocks_used_max, stats_.n_blocks_used);
      stats_.bytes_free -= block_meta->block_size;
      stats_.bytes_used += block_meta->block_size;
      stats_.bytes_used_max =
          std::max(stats_.bytes_used_max, stats_.bytes_used);
      // remove from freelist
      *prev_free_block = *reinterpret_cast<void **>(&space_[block_idx]);
      assert(is_valid_free_block(*prev_free_block));
      log("larger size block");
      log(pointer_to_hex(free_block));
      return free_block;
    }
    prev_free_block = reinterpret_cast<void **>(&space_[block_idx]);
    free_block = *prev_free_block;
  }
  log("out of free blocks");
  return nullptr;
}

void MarkAndSweep::collect() {
  log("collect");
  if (incremental) {
    auto p = reinterpret_cast<void *>(
        reinterpret_cast<uintptr_t>(space_start_) + sizeof(Metadata));
    while (p < space_end_) {
      auto block_idx = pointer_to_idx(p);
      auto block_meta = get_metadata(block_idx);
      if (block_meta->mark != FREE) {
        block_meta->mark = NOT_MARKED;
      }
      p = reinterpret_cast<void *>(&space_[block_idx + block_meta->block_size]);
    }
    while (!mark_queue_.empty()) {
      mark_queue_.pop();
    }
    phase_ = MARK;
    for (auto root : roots_) {
      if (is_in_space(*root)) {
        mark_queue_.push(*root);
      }
    }
  }
  stats_.collections++;
  mark();
  sweep();
  if (merge_blocks) {
    merge();
  }
}

void MarkAndSweep::mark() {
  log("mark");
  for (auto root : roots_) {
    auto x = *root;
    if (!is_in_space(x)) {
      continue;
    }
    auto x_i = pointer_to_idx(x);
    auto x_meta = get_metadata(x_i);
    if (x_meta->mark == NOT_MARKED) {
      dfs(x);
    }
  }
}

void MarkAndSweep::dfs(void *x) {
  auto x_i = pointer_to_idx(x);
  auto x_meta = get_metadata(x_i);
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
      if (i > 0 || !skip_first_field) {
        auto field_i_addr = &space_[x_i + i * sizeof(pointer_t)];
        auto y = *reinterpret_cast<void **>(field_i_addr);
        if (is_in_space(y)) {
          auto y_i = pointer_to_idx(y);
          auto y_meta = get_metadata(y_i);
          if (y_meta->mark == NOT_MARKED) {
            *reinterpret_cast<void **>(field_i_addr) = tmp;
            tmp = x;
            x = y;
            y_meta->mark = MARKED;
            y_meta->done = 0;
            continue;
          }
        }
      }
      x_meta->done++;
    } else {
      auto y = x;
      x = tmp;
      if (!x) {
        return;
      }
      x_i = pointer_to_idx(x);
      x_meta = get_metadata(x_i);
      auto i = x_meta->done;
      auto field_i_addr = &space_[x_i + i * sizeof(pointer_t)];
      tmp = *reinterpret_cast<void **>(field_i_addr);
      *reinterpret_cast<void **>(field_i_addr) = y;
      x_meta->done++;
    }
  }
}

void MarkAndSweep::sweep() {
  log("sweep");
  stats_.collected_objects.clear();
  auto p = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(space_start_) +
                                    sizeof(Metadata));
  while (p < space_end_) {
    auto block_idx = pointer_to_idx(p);
    auto block_meta = get_metadata(block_idx);
    if (block_meta->mark == MARKED) {
      block_meta->mark = NOT_MARKED;
    } else if (block_meta->mark == NOT_MARKED) {
      block_meta->mark = FREE;
      *reinterpret_cast<void **>(p) = freelist_;
      freelist_ = p;
      assert(stats_.n_blocks_used > 0);
      assert(stats_.bytes_used >= block_meta->block_size);
      stats_.collected_objects.push_back(p);
      stats_.n_blocks_used--;
      stats_.n_blocks_free++;
      stats_.bytes_used -= block_meta->block_size;
      stats_.bytes_free += block_meta->block_size;
    }
    p = reinterpret_cast<void *>(&space_[block_idx + block_meta->block_size]);
  }
}

void MarkAndSweep::merge() {
  log("merge");
  auto p = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(space_start_) +
                                    sizeof(Metadata));
  freelist_ = nullptr;
  void *merging_block = nullptr;
  while (p < space_end_) {
    auto block_idx = pointer_to_idx(p);
    auto block_meta = get_metadata(block_idx);
    if (block_meta->mark == FREE) {
      if (merging_block) {
        auto merge_idx = pointer_to_idx(merging_block);
        auto merge_meta = get_metadata(merge_idx);
        merge_meta->block_size += block_meta->block_size;
        stats_.n_blocks_total--;
        stats_.n_blocks_free--;
      } else {
        *reinterpret_cast<void **>(p) = freelist_;
        freelist_ = p;
        merging_block = p;
      }
    } else {
      merging_block = nullptr;
    }
    p = reinterpret_cast<void *>(&space_[block_idx + block_meta->block_size]);
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

bool MarkAndSweep::is_valid_free_block(void const *obj) const {
  if (obj == nullptr) {
    return true;
  }
  if (!is_in_space(obj)) {
    return false;
  }
  auto idx = pointer_to_idx(obj);
  auto meta = get_metadata(idx);
  return meta->mark == FREE;
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

MarkAndSweep::Metadata *MarkAndSweep::get_metadata(size_t obj_idx) const {
  size_t metadata_idx = obj_idx - sizeof(Metadata);
  auto res = reinterpret_cast<Metadata *>(&space_[metadata_idx]);
  assert((res->block_size <= max_memory &&
          "potential memory corruption detected") ||
         log(pointer_to_hex(res)));
  assert(
      ((res->mark == NOT_MARKED || res->mark == MARKED || res->mark == FREE) &&
       "potential memory corruption detected") ||
      log(pointer_to_hex(res)));
  return res;
}

void MarkAndSweep::read(void *obj) {
  stats_.reads++;
  if (is_in_space(obj)) {
    auto idx = pointer_to_idx(obj);
    auto meta = get_metadata(idx);
    assert((meta->mark != FREE && "tried to access unexisting object") ||
           log(pointer_to_hex(obj)));
  }
}

void MarkAndSweep::write(void *obj, void *contents) {
  stats_.writes++;
  if (is_in_space(obj) && is_in_space(contents)) {
    auto obj_idx = pointer_to_idx(obj);
    auto obj_meta = get_metadata(obj_idx);
    assert((obj_meta->mark != FREE && "tried to access unexisting object") ||
           log(pointer_to_hex(obj)));
    if (incremental && phase_ == MARK && obj_meta->mark == MARKED) {
      auto contents_idx = pointer_to_idx(contents);
      auto contents_meta = get_metadata(contents_idx);
      if (contents_meta->mark == NOT_MARKED) {
        mark_queue_.push(contents);
      }
    }
  }
}

std::string MarkAndSweep::dump() const {
  std::string dump;
  dump.append(dump_stats() + "\n\n");
  dump.append(dump_roots() + "\n\n");
  dump.append(dump_blocks() + "\n");
  return dump;
}

std::string MarkAndSweep::dump_stats() const {
  std::string dump;
  dump.append("STATS\n");
  tables::Table stats({26, 16, 17});
  stats.separator();
  stats.add_row({"COLLECTIONS (full)", "",
                 std::format("{:10} cycles", stats_.collections)});
  if (incremental) {
    stats.add_row(
        {"COLLECTIONS (incremental)", "",
         std::format("{:10} cycles", stats_.incremental_collections)});
  }
  stats.separator();
  stats.add_row({"MEMORY USED (max)",
                 std::format("{:10} bytes", stats_.bytes_used_max),
                 std::format("{:10} blocks", stats_.n_blocks_used_max)});
  stats.separator();
  stats.add_row({"MEMORY USED", std::format("{:10} bytes", stats_.bytes_used),
                 std::format("{:10} blocks", stats_.n_blocks_used)});
  stats.add_row(
      {"MEMORY USED (w/o metadata)",
       std::format("{:10} bytes",
                   stats_.bytes_used - stats_.n_blocks_used * sizeof(Metadata)),
       ""});
  stats.add_row({"MEMORY FREE", std::format("{:10} bytes", stats_.bytes_free),
                 std::format("{:10} blocks", stats_.n_blocks_free)});
  stats.add_row(
      {"MEMORY FREE (w/o metadata)",
       std::format("{:10} bytes",
                   stats_.bytes_free - stats_.n_blocks_free * sizeof(Metadata)),
       ""});
  stats.separator();
  stats.add_row({"READS / WRITES", std::format("{:10} reads", stats_.reads),
                 std::format("{:10} writes", stats_.writes)});
  stats.separator();
  dump.append(stats.to_string());
  return dump;
}

std::string MarkAndSweep::dump_roots() const {
  std::string dump;
  dump.append("ROOTS\n");
  tables::Table roots({3, 23, 23});
  roots.separator();
  roots.add_row({"IDX", "ADDRESS", "VALUE"});
  roots.separator();
  for (size_t i = 0; i < roots_.size(); i++) {
    roots.add_row({std::format("{:3}", i + 1), pointer_to_hex(roots_.at(i)),
                   pointer_to_hex(*roots_.at(i))});
  }
  roots.separator();
  dump.append(roots.to_string());
  return dump;
}

std::string MarkAndSweep::dump_blocks() const {
  std::string dump;
  dump.append("BLOCKS\n");
  tables::Table blocks({23, 23, 23});
  blocks.separator();
  blocks.add_row({"FREELIST", pointer_to_hex(freelist_), ""});
  blocks.separator();
  if (incremental) {
    switch (phase_) {
    case MARK:
      blocks.add_row({"PHASE", "MARK", ""});
      blocks.add_row(
          {"NEXT",
           pointer_to_hex(mark_queue_.empty() ? nullptr : mark_queue_.front()),
           ""});
      break;
    case SWEEP:
      blocks.add_row({"PHASE", "SWEEP", ""});
      blocks.add_row({"NEXT", pointer_to_hex(resume_sweep_from), ""});
      break;
    }
  }
  blocks.separator();
  blocks.add_row({"ADDRESS", "VALUE", "DESCRIPTION"});
  blocks.separator();
  auto p = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(space_start_) +
                                    sizeof(Metadata));
  while (p < space_end_) {
    auto block_idx = pointer_to_idx(p);
    auto block_meta = get_metadata(block_idx);
    for (size_t i = 0; i < block_meta->block_size; i += sizeof(pointer_t)) {
      auto v =
          reinterpret_cast<void *>(&space_[block_idx + i - sizeof(Metadata)]);
      if (i == 0) {
        auto status = block_meta->mark == FREE ? "FREE" : "USED";
        blocks.add_row(
            {pointer_to_hex(v), pointer_to_hex(*reinterpret_cast<void **>(v)),
             std::format("size: {:10}   {}", block_meta->block_size, status)});
      } else {
        if (block_meta->mark == FREE) {
          if (i == sizeof(pointer_t)) {
            blocks.add_row({pointer_to_hex(v),
                            pointer_to_hex(*reinterpret_cast<void **>(v)),
                            "next free block"});
          } else {
            blocks.add_row({pointer_to_hex(v),
                            pointer_to_hex(*reinterpret_cast<void **>(v)), ""});
          }
        } else {
          blocks.add_row({pointer_to_hex(v),
                          pointer_to_hex(*reinterpret_cast<void **>(v)),
                          std::format("field #{}", i / sizeof(pointer_t))});
        }
      }
    }
    blocks.separator();
    p = reinterpret_cast<void *>(&space_[block_idx + block_meta->block_size]);
  }
  dump.append(blocks.to_string());
  return dump;
}

// incremental collection

void MarkAndSweep::incr_collect(size_t bytes) {
  log("incremental collect");
  switch (phase_) {
  case MARK:
    incr_mark(bytes);
    break;
  case SWEEP:
    incr_sweep(bytes);
    break;
  }
}

void MarkAndSweep::incr_mark(size_t bytes) {
  log("incremental mark");
  size_t bytes_marked = 0;
  while (bytes_marked < bytes) {
    if (mark_queue_.empty()) {
      stats_.collected_objects.clear();
      phase_ = SWEEP;
      resume_sweep_from = reinterpret_cast<void *>(
          reinterpret_cast<uintptr_t>(space_start_) + sizeof(Metadata));
      return;
    }
    auto next = mark_queue_.front();
    mark_queue_.pop();
    auto next_idx = pointer_to_idx(next);
    auto next_meta = get_metadata(next_idx);
    if (next_meta->mark == NOT_MARKED) {
      bytes_marked += next_meta->block_size;
      next_meta->mark = MARKED;
      auto obj_size = next_meta->block_size - sizeof(Metadata);
      assert(obj_size % sizeof(pointer_t) == 0);
      auto field_n = obj_size / sizeof(pointer_t);
      for (size_t i = 0; i < field_n; i++) {
        auto field_i = *reinterpret_cast<void **>(
            &space_[next_idx + i * sizeof(pointer_t)]);
        if (is_in_space(field_i)) {
          mark_queue_.push(field_i);
        }
      }
    }
  }
}

void MarkAndSweep::incr_sweep(size_t bytes) {
  log("incremental sweep");
  auto p = resume_sweep_from;
  size_t bytes_marked = 0;
  while (bytes_marked < bytes) {
    auto block_idx = pointer_to_idx(p);
    auto block_meta = get_metadata(block_idx);
    bytes_marked += block_meta->block_size;
    if (block_meta->mark == MARKED) {
      block_meta->mark = NOT_MARKED;
    } else if (block_meta->mark == NOT_MARKED) {
      block_meta->mark = FREE;
      *reinterpret_cast<void **>(p) = freelist_;
      freelist_ = p;
      assert(stats_.n_blocks_used > 0);
      assert(stats_.bytes_used >= block_meta->block_size);
      stats_.collected_objects.push_back(p);
      stats_.n_blocks_used--;
      stats_.n_blocks_free++;
      stats_.bytes_used -= block_meta->block_size;
      stats_.bytes_free += block_meta->block_size;
    }
    p = reinterpret_cast<void *>(&space_[block_idx + block_meta->block_size]);
    if (p >= space_end_) {
      phase_ = MARK;
      for (auto root : roots_) {
        if (is_in_space(*root)) {
          mark_queue_.push(*root);
        }
      }
      stats_.incremental_collections++;
      return;
    }
    resume_sweep_from = p;
  }
}

} // namespace gc
