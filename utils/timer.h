#pragma once

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
  explicit TimerWheel(Tick now = 0) : now_(), slots_(), bitmap_() {
    if (now != 0)
      for (int i = 0; i < kNumLevels; ++i) now_[i] = now >> (kWidthBits * i);
  }
  ~TimerWheel() = default;

  // Safe to cancel or schedule from within execute, should not be called from
  // an execute
  void Advance(Tick delta, int level = 0) {
    size_t current_slot_index;
    size_t next_slot_index;
    size_t skip_slots;

    while (delta) {
      current_slot_index = now_[level] & kMask;
      next_slot_index = bitmap_[level]._Find_next(current_slot_index);
      skip_slots = next_slot_index - current_slot_index - 1;

      if (skip_slots >= delta) {
        now_[level] += delta;
        break;
      }

      delta -= skip_slots;
      now_[level] += (skip_slots + 1);

      if (next_slot_index == kNumSlots) {
        if (level < kMaxLevel) Advance(1, level + 1);
      } else {
        auto slot = &slots_[level][next_slot_index];
        if (level > 0) {
          DCHECK_EQ((now_[0] & kMask), 0);
          while (slot->events()) {
            auto event = slot->pop_event();
            if (now_[0] >= event->scheduled_at())
              event->execute(this);
            else
              Schedule(event, event->scheduled_at() - now_[0]);
          }
        } else {
          while (slot->events()) slot->pop_event()->execute(this);
        }
        bitmap_[level].reset(next_slot_index);
        delta--;
      }
    }
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
    event->relink(slot);
    bitmap_[level].set(slot_index);
  }

  // The actual time will be determined by the TimerWheel to minimize
  // rescheduling and promotion overhead
  void ScheduleInRange(T* event, Tick start, Tick end) {
    DCHECK_GT(start, 0);
    DCHECK_GT(end, 0);
    DCHECK_GT(end, start);

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
    Slot() : events_(nullptr) {}

   private:
    const T* events() const { return events_; }
    T* pop_event() {
      auto event = events_;
      events_ = event->next_;
      if (events_) events_->prev_ = nullptr;

      event->next_ = nullptr;
      event->slot_ = nullptr;
      return event;
    }

    friend EventBase<T>;
    friend TimerWheel<T>;

    T* events_;

    DISALLOW_COPY_AND_ASSIGN(Slot);
  };

  void process_next_slot(Tick now, int level) {
    size_t slot_index = now & kMask;
    if (slot_index == 0 && level < kMaxLevel) Advance(1, level + 1);

    auto slot = &slots_[level][slot_index];
    if (level > 0) {
      DCHECK_EQ((now_[0] & kMask), 0);
      while (slot->events()) {
        auto event = slot->pop_event();
        if (now_[0] >= event->scheduled_at())
          event->execute(this);
        else
          Schedule(event, event->scheduled_at() - now_[0]);
      }
    } else {
      while (slot->events()) slot->pop_event()->execute(this);
    }
  }

  static constexpr int kWidthBits = 8;
  static constexpr int kNumLevels = (64 + kWidthBits - 1) / kWidthBits;
  static constexpr int kMaxLevel = kNumLevels - 1;
  static constexpr int kNumSlots = 1 << kWidthBits;
  static constexpr int kMask = (kNumSlots - 1);

  std::array<Tick, kNumLevels> now_;
  std::array<std::array<Slot, kNumSlots>, kNumLevels> slots_;
  std::array<std::bitset<kNumSlots>, kNumLevels> bitmap_;

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

  void set_scheduled_at(Tick ts) { scheduled_at_ = ts; }
  // Move the event to another slot. (It's safe for either the current or new
  // slot to be NULL).
  void relink(typename TimerWheel<T>::Slot* new_slot) {
    if (new_slot == slot_) return;

    if (slot_) {
      auto prev = prev_;
      auto next = next_;
      if (next) next->prev_ = prev;
      if (prev)
        prev->next_ = next;
      else
        slot_->events_ = next;
    }

    if (new_slot) {
      auto old = new_slot->events_;
      next_ = old;
      if (old) old->prev_ = static_cast<T*>(this);

      new_slot->events_ = static_cast<T*>(this);
    } else {
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
