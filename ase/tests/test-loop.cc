// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <cstdio>
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <ase/loop.hh>
#include "../testing.hh"

namespace { // Anon

static void
loop_current_test()
{
  using namespace Ase;
  auto loop1 = Loop::current();
  auto loop2 = Loop::current();
  TCHECK (loop1 == loop2, "Loop::current() should return the same loop instance");
  auto loop3 = Loop::current();
  TCHECK (loop1 == loop3, "Loop::current() should always return the same instance");
}
TEST_ADD (loop_current_test);

static void
loop_current_multithread_test()
{
  using namespace Ase;
  auto main_thread_loop = Loop::current();
  std::atomic<bool> thread1_ready{false}, thread2_ready{false};
  Ase::LoopP thread1_loop, thread2_loop;

  std::thread t1 ([&]() {
    thread1_loop = Loop::current();
    TCHECK (thread1_loop != nullptr, "Thread 1 should get a valid loop");
    auto same = Loop::current();
    TCHECK (thread1_loop == same, "Thread 1 should always get the same loop");
    thread1_ready = true;
  });

  std::thread t2 ([&]() {
    thread2_loop = Loop::current();
    TCHECK (thread2_loop != nullptr, "Thread 2 should get a valid loop");
    auto same = Loop::current();
    TCHECK (thread2_loop == same, "Thread 2 should always get the same loop");
    thread2_ready = true;
  });

  t1.join();
  t2.join();

  TCHECK (thread1_ready && thread2_ready, "Both threads should complete");
  TCHECK (thread1_loop && thread2_loop, "Both thread loops should exist");
  TCHECK (thread1_loop != main_thread_loop, "Thread 1 loop differs from main");
  TCHECK (thread2_loop != main_thread_loop, "Thread 2 loop differs from main");
  TCHECK (thread1_loop != thread2_loop, "Each thread gets its own loop");
}
TEST_ADD (loop_current_multithread_test);

static void
loop_add_timer_test()
{
  using namespace Ase;
  auto interval = std::chrono::milliseconds (1);
  auto priority = Ase::LoopPriority::NORMAL;
  auto loop = Loop::current();

  int counter = 0;
  loop->add ([&counter] ()
  {
    counter++;
    return false;
  }, interval, priority);

  for (int i = 0; i < 4; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (false);
  }

  TCHECK (counter == 1, "add_timer() callback should be called once");
}
TEST_ADD (loop_add_timer_test);

} // Anon
