// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <cstdio>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>
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
      loop->iterate (true);
  }

  TCHECK (counter == 1, "add_timer() callback should be called once");
}
TEST_ADD (loop_add_timer_test);

// === CoTask tests ===

static void
cotask_void_test()
{
  auto loop = Ase::Loop::current();
  auto completedp = std::make_shared<std::atomic<bool>> (false);

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    auto promise = loop->make_promise ([=] (std::function<void()> resolve)
    {
      // resolve asynchrnously via event loop
      loop->add ([=]() { resolve(); return false; });
    });
    co_await promise;
    *completedp = true;
    co_return;
  };
  loop->add (coroutine);

  for (int i = 0; i < 500 && !*completedp; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*completedp, "CoTask<void> should complete");
}
TEST_ADD (cotask_void_test);

static void
cotask_int64_result_test()
{
  auto loop = Ase::Loop::current();
  auto result = std::make_shared<std::atomic<int64_t>> (0);

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    auto promise = loop->make_promise<int64_t> ([=] (std::function<void(int64_t)> resolve) {
      loop->add ([=]() { resolve (42); return false; });
    });
    *result = co_await promise;
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && *result == 0; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*result == 42, "CoTask<int64_t> should return 42");
}
TEST_ADD (cotask_int64_result_test);

static void
cotask_string_result_test()
{
  auto loop = Ase::Loop::current();
  auto result = std::make_shared<std::string>();

  auto coroutine = [=]() -> Ase::CoTaskVoid {
    auto promise = loop->make_promise<std::string> ([=] (std::function<void(std::string)> resolve) {
      loop->add ([=]() { resolve (std::string {"hello"}); return false; });
    });
    *result = co_await promise;
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && result->empty(); i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*result == "hello", "CoTask<std::string> should return 'hello'");
}
TEST_ADD (cotask_string_result_test);

static void
promise_void_test()
{
  auto loop = Ase::Loop::current();
  auto completed = std::make_shared<std::atomic<bool>> (false);

  auto promise = loop->make_promise ([=] (std::function<void()> resolve)
  {
    loop->add ([=]() { resolve(); return false; });
  });

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    co_await promise;
    *completed = true;
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && !*completed; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*completed, "Promise<void> should resolve");
}
TEST_ADD (promise_void_test);

static void
promise_int64_test()
{
  auto loop = Ase::Loop::current();
  auto result = std::make_shared<std::atomic<int64_t>> (0);

  auto promise = loop->make_promise<int64_t> ([=] (std::function<void(int64_t)> resolve)
  {
    loop->add ([=]() { resolve (123); return false; });
  });

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    *result = co_await promise;
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && *result == 0; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*result == 123, "Promise<int64_t> should resolve with 123");
}
TEST_ADD (promise_int64_test);

static void
promise_string_test()
{
  auto loop = Ase::Loop::current();
  auto result = std::make_shared<std::string>();

  auto promise = loop->make_promise<std::string> ([=] (std::function<void(std::string)> resolve)
  {
    loop->add ([=]() { resolve (std::string {"world"}); return false; });
  });

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    *result = co_await promise;
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && result->empty(); i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*result == "world", "Promise<std::string> should resolve with 'world'");
}
TEST_ADD (promise_string_test);

static void
promise_multi_waiter_test()
{
  auto loop = Ase::Loop::current();
  auto promise = loop->make_promise<int64_t> ([] (std::function<void(int64_t)>) {});
  auto waiter1_done = std::make_shared<std::atomic<int>> (0);
  auto waiter2_done = std::make_shared<std::atomic<int>> (0);

  auto cotask1 = [=]() -> Ase::CoTaskVoid
  {
    *waiter1_done = co_await promise;
    co_return;
  };

  auto cotask2 = [=]() -> Ase::CoTaskVoid
  {
    *waiter2_done = co_await promise;
    co_return;
  };

  loop->add (cotask1);
  loop->add (cotask2);
  loop->add ([=]() { promise->resolve (99); return false; });

  for (int i = 0; i < 500 && (*waiter1_done == 0 || *waiter2_done == 0); i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*waiter1_done == 99, "First waiter should get 99");
  TCHECK (*waiter2_done == 99, "Second waiter should get 99");
}
TEST_ADD (promise_multi_waiter_test);

static void
promise_exception_test()
{
  auto loop = Ase::Loop::current();
  auto caught_exception = std::make_shared<std::atomic<bool>> (false);

  auto promise = loop->make_promise<int64_t> ([] (std::function<void(int64_t)>) {});
  loop->add ([=]()
  {
    promise->reject (std::make_exception_ptr (std::runtime_error ("test error")));
    return false;
  });

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    try {
      co_await promise;
    } catch (const std::runtime_error &) {
      *caught_exception = true;
    }
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && !*caught_exception; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*caught_exception, "Promise rejection should propagate exception");
}
TEST_ADD (promise_exception_test);

static Ase::CoTask<int64_t>
inner_int64_task (Ase::LoopP loop, int64_t value)
{
  auto promise = loop->make_promise<int64_t> ([loop, value] (std::function<void(int64_t)> resolve)
  {
    loop->add ([resolve, value]() { resolve (value * 2); return false; });
  });
  co_return co_await promise;
}

static void
nested_cotask_test()
{
  auto loop = Ase::Loop::current();
  auto result = std::make_shared<std::atomic<int64_t>> (0);

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    auto v1 = co_await inner_int64_task (loop, 10);
    TCHECK (v1 == 20, "Nested CoTask should chain results: 10 -> 20");
    auto v2 = co_await inner_int64_task (loop, v1);
    *result = v2;
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && *result == 0; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*result == 40, "Nested CoTask should chain results: 10 -> 20 -> 40");
}
TEST_ADD (nested_cotask_test);

static void
promise_already_resolved_test()
{
  auto loop = Ase::Loop::current();
  auto result = std::make_shared<std::atomic<int64_t>> (0);

  auto promise = loop->make_promise<int64_t> ([=] (std::function<void(int64_t)> resolve)
  {
    resolve (77);
  });

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    *result = co_await promise;
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && *result == 0; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*result == 77, "Already-resolved promise should return value immediately");
}
TEST_ADD (promise_already_resolved_test);

static void
delay_promise_test()
{
  auto loop = Ase::Loop::current();
  auto elapsed_ms = std::make_shared<std::atomic<int64_t>> (-1);

  auto coroutine = [=]() -> Ase::CoTaskVoid
  {
    *elapsed_ms = co_await loop->delay (std::chrono::milliseconds (50));
    co_return;
  };

  loop->add (coroutine);

  for (int i = 0; i < 500 && -1 == *elapsed_ms; i++) {
    usleep (1000);
    while (loop->pending())
      loop->iterate (true);
  }
  TCHECK (*elapsed_ms >= 0, "delay promise should resolve");
  TCHECK (*elapsed_ms >= 50 - 1, "delay should wait at least 50ms");
  TCHECK (*elapsed_ms < 1000, "delay should last less than a second");
}
TEST_ADD (delay_promise_test);

} // Anon
