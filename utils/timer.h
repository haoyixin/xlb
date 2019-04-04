#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <limits>
#include <memory>

#include <glog/logging.h>

#include "utils/common.h"

namespace xlb::utils {

using Tick = uint64_t;

template <typename T>
class EventBase;

template <typename T>
class TimerWheel {
 public:
  explicit TimerWheel(Tick now = 0) : now_(), ticks_pending_(0), slots_() {
    if (now != 0)
      for (int i = 0; i < kNumLevels; ++i) now_[i] = now >> (kWidthBits * i);
  }
  ~TimerWheel() = default;

  void Advance(Tick delta, int level = 0) {
    while (delta--)
      process_current_slot(++now_[level], level);
  }

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
  }

  Tick now() const { return now_[0]; }

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

  void process_current_slot(Tick now, int level) {
    size_t slot_index = now & kMask;
    auto slot = &slots_[level][slot_index];
    if (slot_index == 0 && level < kMaxLevel)
      Advance(1, level + 1);

    if (level > 0) {
      DCHECK_EQ((now_[0] & kMask), 0);
      while (slot->events()) {
        auto event = slot->pop_event();
        if (now_[0] >= event->scheduled_at())
          event->execute();
        else
          Schedule(event, event->scheduled_at() - now_[0]);
      }
    } else {
      while (slot->events())
        slot->pop_event()->execute();
    }
  }

  static constexpr int kWidthBits = 8;
  static constexpr int kNumLevels = (64 + kWidthBits - 1) / kWidthBits;
  static constexpr int kMaxLevel = kNumLevels - 1;
  static constexpr int kNumSlots = 1 << kWidthBits;
  static constexpr int kMask = (kNumSlots - 1);

  std::array<Tick, kNumLevels> now_;
  Tick ticks_pending_;
  std::array<std::array<Slot, kNumSlots>, kNumLevels> slots_;

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
  virtual void execute() = 0;

  void set_scheduled_at(Tick ts) { scheduled_at_ = ts; }
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

    {
      if (new_slot) {
        auto old = new_slot->events_;
        next_ = old;
        if (old) old->prev_ = static_cast<T*>(this);

        new_slot->events_ = static_cast<T*>(this);
      } else {
        next_ = nullptr;
      }
      prev_ = nullptr;
    }
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
