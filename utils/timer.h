#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <bitset>
#include <limits>
#include <memory>

#include <glog/logging.h>

#include "utils/boost.h"
#include "utils/common.h"

namespace xlb::utils {

using Tick = uint64_t;

template <typename T>
class EventBase;

template <typename T>
class TimerWheel {
 public:
  explicit TimerWheel(Tick now = 0) : slots_(), bitmap_(), now_() {
    if (now != 0)
      for (int i = 0; i < kNumLevels; ++i) now_[i] = now >> (kWidthBits * i);

    for (auto l : irange(kNumLevels))
      for (auto i : irange(kNumSlots)) {
        slots_[l][i].level_ = l;
        slots_[l][i].index_ = i;
      }
  }
  ~TimerWheel() = default;

  // Safe to cancel or schedule from within execute, should not be called from
  // an execute
  void Advance(Tick delta) {
    Tick now;
    Tick partial_delta;
    int outermost_changed_level;
    int level, slot_index;

    auto update_now = [&] {
      DLOG(INFO) << "[update_now] now: " << now_[0]
                 << " partial_delta: " << partial_delta << " level: " << level;

      now_[0] += partial_delta;
      if (level > 0)
        for (auto i : irange(1, level + 1))
          now_[i] = now_[0] >> (kWidthBits * i);
    };
    auto found = [&] {
      DLOG(INFO) << "[found] level: " << level << " slot_index: " << slot_index;
      update_now();
      slots_[level][slot_index].execute();
    };
    auto update_partial = [&] {
      DLOG(INFO) << "[update_partial] partial_delta: " << partial_delta;
      partial_delta +=
          (slot_index - (now_[level] & kMask) << kWidthBits * level);
    };
    auto carry = [&] {
      DLOG(INFO) << "[carry] level: " << level;
      ++now_[level + 1];
    };
    auto find_first = [&] {
      DLOG(INFO) << "[find_first] level: " << level;
      slot_index = bitmap_[level]._Find_first();
    };

  START:
    if (delta == 0) {
      DLOG(INFO) << "[START] returned";
      return;
    }

    DLOG(INFO) << "[START] delta: " << delta << " now: " << now_[0];

    now = now_[0] + delta;
    partial_delta = 0;

    for (auto i : irange(kMaxLevel, -1, -1))
      if (now >> kWidthBits * i != now_[i]) {
        outermost_changed_level = i;
        break;
      }

    DLOG(INFO) << "[START] outermost_changed_level: "
               << outermost_changed_level;

    DCHECK_LE(outermost_changed_level, kMaxLevel);

    if (outermost_changed_level > 0) {
      level = 0;
      DLOG(INFO) << "[STEP1] process first level: " << level;
      find_first();
      DCHECK_NE(slot_index, 0);

      update_partial();
      if (slot_index != kNumSlots) {
        found();
        goto START;
      }

      carry();
      for (auto i : irange(1, outermost_changed_level)) {
        level = i;
        DLOG(INFO) << "[STEP2] process middle level: " << level;
        if ((now_[level] & kMask) == 0) goto NEXT_LEVEL;

        find_first();
        // TODO: From a technical point of view, I don’t know why it works.
        DCHECK_GE(slot_index, now_[level] & kMask);

        update_partial();
        if (slot_index != kNumSlots) {
          found();
          goto START;
        }

      NEXT_LEVEL:
        carry();
      }
    }

    level = outermost_changed_level;
    DLOG(INFO) << "[STEP3] process outermost_changed_level: " << level;
    find_first();
    update_partial();

    if (partial_delta > delta) {
      partial_delta = delta;
      delta = 0;
      update_now();
      return;
    }

    delta -= partial_delta;
    found();
    goto START;
  }

  void AdvanceTo(Tick abs) { Advance(abs - now_[0]); }

  void Schedule(T* event, Tick delta) {
    DCHECK_GT(delta, 0);

    event->set_scheduled_at(now_[0] + delta);

    int level = 0;
    while (delta >= kNumSlots) {
      delta = (delta + (now_[level] & kMask)) >> kWidthBits;
      ++level;
    }

    size_t slot_index = (now_[level] + delta) & kMask;
    auto slot = &slots_[level][slot_index];
    DLOG(INFO) << "[Schedule] at level: " << level
               << " slot_index: " << slot_index;
    event->relink(slot);
  }

  // The actual time will be determined by the TimerWheel to minimize
  // rescheduling and promotion overhead
  void ScheduleInRange(T* event, Tick start, Tick end) {
    DCHECK_GT(start, 0);
    DCHECK_GT(end, 0);
    DCHECK_GT(end, start);

    DLOG(INFO) << "[ScheduleInRange] start: " << start << " end: " << end;

    if (event->active()) {
      auto current = event->scheduled_at() - now_[0];
      if (current >= start && current <= end) return;
    }

    Tick mask = ~0;
    while ((start & mask) != (end & mask)) mask = (mask << kWidthBits);

    Tick delta = end & (mask >> kWidthBits);

    Schedule(event, delta);
  }

  // During the execution of the event callback, Now() will return the tick that
  // the event was scheduled to run on.
  Tick Now() const { return now_[0]; }

  /*
  TODO: Tick TicksToNextEvent(Tick max, int level);
   */

 private:
  class Slot {
   public:
    Slot() : events_(nullptr), offset_(0), level_(0), index_(0) {}

   private:
    T* pop_event() {
      auto event = events_;
      events_ = event->next_;
      if (events_) events_->prev_ = nullptr;

      event->next_ = nullptr;
      event->slot_ = nullptr;
      return event;
    }

    void set() {
      DLOG(INFO) << "[set] level: " << level_ << " index: " << index_;
      TimerWheel::from_slot(this)->bitmap_[level_].set(index_);
    }

    void reset() {
      DLOG(INFO) << "[reset] level: " << level_ << " index: " << index_;
      events_ = nullptr;
      TimerWheel::from_slot(this)->bitmap_[level_].reset(index_);
    }

    void execute() {
      auto tw = TimerWheel::from_slot(this);

      if (level_ > 0) {
        while (events_) {
          auto event = pop_event();
          if (tw->now_[0] >= event->scheduled_at()) {
            DLOG(INFO) << "[execute] now: " << tw->now_[0]
                       << " level: " << level_ << " index: " << index_;
            event->execute(tw);
          } else {
            DLOG(INFO) << "[reschedule] now: " << tw->now_[0]
                       << " to: " << event->scheduled_at();
            tw->Schedule(event, event->scheduled_at() - tw->now_[0]);
          }
        }
      } else {
        while (events_) {
          DLOG(INFO) << "[execute] now: " << tw->now_[0] << " level: " << level_
                     << " index: " << index_;
          pop_event()->execute(tw);
        }
      }
      reset();
    }

    T* events_;

    struct [[gnu::packed]] {
      uint32_t level_;
      uint32_t index_;
    };

    size_t offset_;

    friend EventBase<T>;
    friend TimerWheel<T>;

    DISALLOW_COPY_AND_ASSIGN(Slot);
  };

  static_assert(sizeof(Slot) == 24);

  static TimerWheel* from_slot(Slot* slot) {
    size_t slots_offset = offsetof(class TimerWheel, slots_);
    size_t slot_offset =
        (slot->level_ * kNumSlots + slot->index_) * sizeof(Slot);

    return reinterpret_cast<TimerWheel*>(reinterpret_cast<uint8_t*>(slot) -
                                         (slots_offset + slot_offset));
  }

  static constexpr int kWidthBits = 8;
  static constexpr int kNumLevels = (64 + kWidthBits - 1) / kWidthBits;
  static constexpr int kMaxLevel = kNumLevels - 1;
  static constexpr int kNumSlots = 1 << kWidthBits;
  static constexpr int kMask = (kNumSlots - 1);

  std::array<std::array<Slot, kNumSlots>, kNumLevels> slots_;
  std::array<std::bitset<kNumSlots>, kNumLevels> bitmap_;
  std::array<Tick, kNumLevels> now_;

  friend class EventBase<T>;

  DISALLOW_COPY_AND_ASSIGN(TimerWheel);
};

template <typename T>
class EventBase {
 public:
  EventBase()
      : scheduled_at_(0), slot_(nullptr), next_(nullptr), prev_(nullptr){};

  virtual ~EventBase() { Cancel(); }

  void Cancel() {
    if (!slot_) {
      return;
    }

    relink(nullptr);
  }

  bool Active() const { return slot_ != nullptr; }

  Tick scheduled_at() const { return scheduled_at_; }

 private:
  virtual void execute(TimerWheel<T>* timer) = 0;

  void set_scheduled_at(Tick ts) {
    DLOG(INFO) << "[set_scheduled_at] tick: " << ts;
    scheduled_at_ = ts;
  }
  // Move the event to another slot. (It's safe for either the current or new
  // slot to be NULL).
  void relink(typename TimerWheel<T>::Slot* new_slot) {
    if (new_slot == slot_) {
      DLOG(INFO) << "[relink] returned";
      return;
    }

    if (slot_) {
      DLOG(INFO) << "[relink] slot_ != nullptr, level: " << slot_->level_
                 << " index: " << slot_->index_;

      auto prev = prev_;
      auto next = next_;

      if (next) {
        next->prev_ = prev;
        if (prev)
          prev->next_ = next;
        else
          slot_->events_ = next;
      } else {
        if (prev)
          prev->next_ = next;
        else
          slot_->reset();
      }
    }

    if (new_slot) {
      DLOG(INFO) << "[relink] new_slot_ != nullptr, level: " << new_slot->level_
                 << " index: " << new_slot->index_;
      auto old = new_slot->events_;
      next_ = old;
      if (old) old->prev_ = static_cast<T*>(this);

      new_slot->events_ = static_cast<T*>(this);
      new_slot->set();
    } else {
      DLOG(INFO) << "[relink] new_slot_ == nullptr";
      next_ = nullptr;
    }

    prev_ = nullptr;
    slot_ = new_slot;
  }

  Tick scheduled_at_;
  typename TimerWheel<T>::Slot* slot_;
  T* next_;
  T* prev_;

  friend TimerWheel<T>;

  DISALLOW_COPY_AND_ASSIGN(EventBase);
};

}  // namespace xlb::utils
