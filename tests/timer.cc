#include <algorithm>
#include <functional>
#include <vector>

#include "utils/timer.h"

using namespace xlb::utils;

class Callback : public EventBase<Callback> {
 public:
  explicit Callback(std::function<void()> &&func) : func_(func) {}
  ~Callback() override = default;

  void execute(TimerWheel<Callback> *tw) override { func_(); }

 private:
  std::function<void()> func_;
};

#define TEST(fun)                    \
  do {                               \
    if (fun()) {                     \
      printf("[OK] %s\n", #fun);     \
    } else {                         \
      ok = false;                    \
      printf("[FAILED] %s\n", #fun); \
    }                                \
  } while (0)

#define EXPECT(expr)                                                   \
  do {                                                                 \
    if (!(expr)) {                                                     \
      printf("%s:%d: Expect failed: %s\n", __FILE__, __LINE__, #expr); \
      return false;                                                    \
    }                                                                  \
  } while (0)

#define EXPECT_INTEQ(actual, expect)                       \
  do {                                                     \
    if (expect != actual) {                                \
      printf(                                              \
          "%s:%d: Expect failed, wanted %ld"               \
          " got %ld\n",                                    \
          __FILE__, __LINE__, (long)expect, (long)actual); \
      return false;                                        \
    }                                                      \
  } while (0)

bool test_single_timer_no_hierarchy() {
  int count = 0;

  Callback timer{[&count]() { ++count; }};
  TimerWheel<Callback> timers;

  // Unscheduled timer does nothing.
  timers.Advance(10);
  EXPECT_INTEQ(count, 0);
  EXPECT(!timer.Active());

  // Schedule timer, should trigger at right time.
  timers.Schedule(&timer, 5);
  EXPECT(timer.Active());
  timers.Advance(5);
  EXPECT_INTEQ(count, 1);

  // Only trigger once, not repeatedly (even if wheel wraps
  // around).
  timers.Advance(256);
  EXPECT_INTEQ(count, 1);

  // ... unless, of course, the timer gets scheduled again.
  timers.Schedule(&timer, 5);
  timers.Advance(5);
  EXPECT_INTEQ(count, 2);

  // Canceled timers don't run.
  timers.Schedule(&timer, 5);
  timer.Cancel();
  EXPECT(!timer.Active());
  timers.Advance(10);
  EXPECT_INTEQ(count, 2);

  // Test wraparound
  timers.Advance(250);
  timers.Schedule(&timer, 5);
  timers.Advance(10);
  EXPECT_INTEQ(count, 3);

  // Timers that are scheduled multiple times only run at the last
  // scheduled tick.
  timers.Schedule(&timer, 5);
  timers.Schedule(&timer, 10);
  timers.Advance(5);
  EXPECT_INTEQ(count, 3);
  timers.Advance(5);
  EXPECT_INTEQ(count, 4);

  // Timer can safely be canceled multiple times.
  timers.Schedule(&timer, 5);
  timer.Cancel();
  timer.Cancel();
  EXPECT(!timer.Active());
  timers.Advance(10);
  EXPECT_INTEQ(count, 4);

  {
    Callback timer2{[&count]() { ++count; }};
    timers.Schedule(&timer2, 5);
  }
  timers.Advance(10);
  EXPECT_INTEQ(count, 4);

  return true;
}

bool test_single_timer_hierarchy() {
  int count = 0;

  Callback timer{[&count]() { ++count; }};
  TimerWheel<Callback> timers;

  EXPECT_INTEQ(count, 0);

  // Schedule timer one layer up (make sure timer ends up in slot 0 once
  // promoted to the innermost wheel, since that's a special case).
  timers.Schedule(&timer, 256);
  timers.Advance(255);
  EXPECT_INTEQ(count, 0);
  timers.Advance(1);
  EXPECT_INTEQ(count, 1);

  // Then schedule one that ends up in some other slot
  timers.Schedule(&timer, 257);
  timers.Advance(256);
  EXPECT_INTEQ(count, 1);
  timers.Advance(1);
  EXPECT_INTEQ(count, 2);

  // Schedule multiple rotations ahead in time, to slot 0.
  timers.Schedule(&timer, 256 * 4 - 1);
  timers.Advance(256 * 4 - 2);
  EXPECT_INTEQ(count, 2);
  timers.Advance(1);
  EXPECT_INTEQ(count, 3);

  // Schedule multiple rotations ahead in time, to non-0 slot. (Do this
  // twice, once starting from slot 0, once starting from slot 5);
  for (int i = 0; i < 2; ++i) {
    timers.Schedule(&timer, 256 * 4 + 5);
    timers.Advance(256 * 4 + 4);
    EXPECT_INTEQ(count, 3 + i);
    timers.Advance(1);
    EXPECT_INTEQ(count, 4 + i);
  }

  return true;
}

bool test_reschedule_from_timer() {
  int count = 0;

  Callback timer{[&count]() { ++count; }};
  TimerWheel<Callback> timers;

  // For every slot in the outermost wheel, try scheduling a timer from
  // a timer handler 258 ticks in the future. Then reschedule it in 257
  // ticks. It should never actually trigger.
  for (int i = 0; i < 256; ++i) {
    Callback rescheduler{[&timers, &timer]() { timers.Schedule(&timer, 258); }};

    timers.Schedule(&rescheduler, 1);
    timers.Advance(257);
    EXPECT_INTEQ(count, 0);
  }
  // But once we stop rescheduling the timer, it'll trigger as intended.
  timers.Advance(2);
  EXPECT_INTEQ(count, 1);

  return true;
}

bool test_single_timer_random() {
  int count = 0;

  Callback timer{[&count]() { ++count; }};
  TimerWheel<Callback> timers;

  for (int i = 0; i < 10000; ++i) {
    int len = rand() % 20;
    int r = 1 + rand() % (1 << len);

    timers.Schedule(&timer, r);
    if (r > 1) timers.Advance(r - 1);
    EXPECT_INTEQ(count, i);
    timers.Advance(1);
    EXPECT_INTEQ(count, i + 1);
  }

  return true;
}

int main(int argc, char **argv) {
  google::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  google::InitGoogleLogging(*argv);

  FLAGS_logtostderr = 1;
  FLAGS_colorlogtostderr = 1;
  FLAGS_v = 2;

  bool ok = true;
  TEST(test_single_timer_no_hierarchy);
  TEST(test_single_timer_hierarchy);
  //    TEST(test_schedule_in_range);
  TEST(test_single_timer_random);
  TEST(test_reschedule_from_timer);

  // Test canceling timer from within timer
  return ok ? 0 : 1;
}