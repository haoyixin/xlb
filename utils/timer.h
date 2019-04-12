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
class alignas(64) TimerWheel {
 public:
  explicit TimerWheel(Tick now = 0) : slots_(), bitmap_(), now_() {
    if (now != 0)
      for (size_t i = 0; i < kNumLevels; ++i) now_[i] = now >> (kWidthBits * i);

    for (auto l : irange(kNumLevels))
      for (auto i : irange(kNumSlots)) {
        slots_[l][i].level_ = l;
        slots_[l][i].index_ = i;
      }
  }
  ~TimerWheel() = default;

  // Safe to cancel or schedule from within execute, should not be called from
  // an execute (I don't want to write or read similar code anymore)
  void Advance(Tick delta) {
    Tick partial_delta;
    size_t outermost_changed_level;
    size_t level, slot_index;

    auto update_now = [&] {
      now_[0] += partial_delta;
      delta -= partial_delta;
      DVLOG(4) << "[update_now] now: " << now_[0]
               << " partial_delta: " << partial_delta << " delta: " << delta
               << " level: " << level;
      if (level > 0)
        for (auto i : irange(1ul, level + 1))
          now_[i] = now_[0] >> (kWidthBits * i);
    };
    auto found = [&] {
      DVLOG(4) << "[found] level: " << level << " slot_index: " << slot_index;
      update_now();
      slots_[level][slot_index].execute();
    };
    auto update_partial = [&] {
      partial_delta +=
          (slot_index - (now_[level] & kMask) << kWidthBits * level);
      DVLOG(4) << "[update_partial] partial_delta: " << partial_delta;
    };
    auto carry = [&] {
      DVLOG(4) << "[carry] level: " << level;
      ++now_[level + 1];
    };
    auto find_first = [&] {
      slot_index = bitmap_[level]._Find_first();
      DVLOG(4) << "[find_first] level: " << level
               << " slot_index: " << slot_index;
    };

  START:
    if (delta == 0) {
      DVLOG(4) << "[START] returned";
      return;
    }

    DVLOG(4) << "[START] delta: " << delta << " now: " << now_[0];

    outermost_changed_level =
        63 - __builtin_clzl(now_[0] + delta ^ now_[0]) >> 3;
    partial_delta = 0;

    DVLOG(4) << "[START] outermost_changed_level: " << outermost_changed_level;

    DCHECK_LE(outermost_changed_level, kMaxLevel);

    if (outermost_changed_level > 0) {
      level = 0;
      DVLOG(4) << "[STEP1] process first level: " << level;
      find_first();
      DCHECK_NE(slot_index, 0);
      DCHECK_GE(slot_index, now_[level] & kMask);

      update_partial();
      if (slot_index != kNumSlots) {
        found();
        DVLOG(4) << "[STEP1] restart delta: " << delta;
        goto START;
      }

      carry();
      for (auto i : irange(1ul, outermost_changed_level)) {
        level = i;
        DVLOG(4) << "[STEP2] process middle level: " << level;
        if ((now_[level] & kMask) == 0) goto NEXT_LEVEL;

        find_first();
        // TODO: From a technical point of view, I don’t know why it works.
        DCHECK_GE(slot_index, now_[level] & kMask);

        update_partial();
        if (slot_index != kNumSlots) {
          found();
          DVLOG(4) << "[STEP2] restart delta: " << delta;
          goto START;
        }

      NEXT_LEVEL:
        carry();
      }
    }

    level = outermost_changed_level;
    DVLOG(4) << "[STEP3] process outermost_changed_level: " << level;
    find_first();
    update_partial();

    if (partial_delta > delta) {
      partial_delta = delta;
      update_now();
      DVLOG(4) << "[STEP3] returned";
      return;
    }

    found();
    DVLOG(4) << "[STEP3] restart delta: " << delta;
    goto START;
  }

  void AdvanceTo(Tick abs) {
    DCHECK_GT(abs, now_[0]);

    DVLOG(2) << "[AdvanceTo] tick: " << abs;

    Advance(abs - now_[0]);
    DCHECK_EQ(now_[0], abs);
  }

  void Schedule(T* event, Tick delta) {
    DCHECK_NE(delta, 0);

    Tick abs = now_[0] + delta;

    schedule_to(event, abs);
  }

  // The actual time will be determined by the TimerWheel to minimize
  // rescheduling and promotion overhead
  void ScheduleInRange(T* event, Tick start, Tick end) {
    DCHECK_GT(end, start);

    DVLOG(2) << "[ScheduleInRange] start: " << start << " end: " << end;

    if (event->Active()) {
      auto current = event->scheduled_at() - now_[0];
      if (current >= start && current <= end) return;
    }

    Tick mask = ~0ul;
    Tick start_abs = now_[0] + start;
    Tick end_abs = now_[0] + end;

    while ((start_abs & mask) != (end_abs & mask)) {
      mask = (mask << kWidthBits);
    }

    schedule_to(event, end_abs & mask >> kWidthBits);
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
    Slot() : events_(nullptr), level_(0), index_(0) {}

   private:
    T* pop_event() {
      auto event = events_;
      events_ = event->next_;
      if (events_) {
        events_->prev_ = nullptr;
      }

      DVLOG(4) << "[pop_event] level: " << level_ << " index: " << index_
               << " scheduled_at: " << event->scheduled_at();

      event->next_ = nullptr;
      event->slot_ = nullptr;
      return event;
    }

    void set() {
      DVLOG(4) << "[set] level: " << level_ << " index: " << index_;
      TimerWheel::from_slot(this)->bitmap_[level_].set(index_);
    }

    void reset() {
      DVLOG(4) << "[reset] level: " << level_ << " index: " << index_;
      events_ = nullptr;
      TimerWheel::from_slot(this)->bitmap_[level_].reset(index_);
    }

    void execute() {
      auto tw = TimerWheel::from_slot(this);

      if (level_ > 0) {
        while (events_) {
          auto event = pop_event();
          if (tw->now_[0] >= event->scheduled_at()) {
            DVLOG(4) << "[execute] now: " << tw->now_[0] << " level: " << level_
                     << " index: " << index_;
            event->execute(tw);
          } else {
            DVLOG(4) << "[reschedule] now: " << tw->now_[0]
                     << " to: " << event->scheduled_at();
            tw->Schedule(event, event->scheduled_at() - tw->now_[0]);
          }
        }
      } else {
        while (events_) {
          DVLOG(4) << "[execute] now: " << tw->now_[0] << " level: " << level_
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

    friend EventBase<T>;
    friend TimerWheel<T>;

    DISALLOW_COPY_AND_ASSIGN(Slot);
  };

  static_assert(sizeof(Slot) == 16);

  void schedule_to(T* event, Tick abs) {
    size_t level = 63 - __builtin_clzl(abs ^ now_[0]) >> 3;
    size_t slot_index = (abs >> kWidthBits * level) & kMask;
    auto slot = &slots_[level][slot_index];

    DVLOG(2) << "[schedule_to] at level: " << level
             << " slot_index: " << slot_index << " scheduled_at: " << abs
             << " now: " << now_[0];

    event->scheduled_at_ = abs;
    event->relink(slot);
  }

  static TimerWheel* from_slot(Slot* slot) {
    size_t slots_offset = offsetof(class TimerWheel, slots_);
    size_t slot_offset =
        (slot->level_ * kNumSlots + slot->index_) * sizeof(Slot);

    return reinterpret_cast<TimerWheel*>(reinterpret_cast<uint8_t*>(slot) -
                                         (slots_offset + slot_offset));
  }

  static constexpr size_t kWidthBits = 8;
  static constexpr size_t kNumLevels = (64 + kWidthBits - 1) / kWidthBits;
  static constexpr size_t kMaxLevel = kNumLevels - 1;
  static constexpr size_t kNumSlots = 1u << kWidthBits;
  static constexpr size_t kMask = (kNumSlots - 1);

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
    DVLOG(4) << "[Cancel] scheduled_at: " << scheduled_at_;
    if (!Active()) {
      DVLOG(4) << "[Cancel] not scheduled";
      return;
    }

    relink(nullptr);
  }

  void Schedule(Tick delta) {
    DCHECK_NOTNULL(slot_);
    TimerWheel<T>::from_slot(slot_)->Schedule(this, delta);
  }

  void ScheduleInRange(Tick start, Tick end) {
    DCHECK_NOTNULL(slot_);
    TimerWheel<T>::from_slot(slot_)->ScheduleInRange(static_cast<T*>(this),
                                                     start, end);
  }

  bool Active() const { return slot_ != nullptr; }

  Tick scheduled_at() const { return scheduled_at_; }

 private:
  virtual void execute(TimerWheel<T>* timer) = 0;

  // Move the event to another slot. (It's safe for either the current or new
  // slot to be NULL).
  void relink(typename TimerWheel<T>::Slot* new_slot) {
    if (new_slot == slot_) {
      DVLOG(4) << "[relink] returned";
      return;
    }

    if (slot_) {
      DVLOG(4) << "[relink] slot_ != nullptr, level: " << slot_->level_
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
      DVLOG(4) << "[relink] new_slot_ != nullptr, level: " << new_slot->level_
               << " index: " << new_slot->index_;
      auto old = new_slot->events_;
      next_ = old;
      if (old) old->prev_ = static_cast<T*>(this);

      new_slot->events_ = static_cast<T*>(this);
      new_slot->set();
    } else {
      DVLOG(4) << "[relink] new_slot_ == nullptr";
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
