// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <cstdio>
#include <atomic>
#include <thread>
#include <chrono>
#include <ase/platform.hh>
#include "../testing.hh"

namespace { // Anon

// Test semaphore with multiple posts and waits
static void
semaphore_multiple_test()
{
  using namespace Ase;
  ScopedSemaphore sem;

  // Post twice
  sem.post();
  sem.post();

  // Two waits should succeed
  TCHECK (sem.wait() == 0, "First sem.wait() should succeed");
  TCHECK (sem.wait() == 0, "Second sem.wait() should succeed");

  // Third wait should timeout (no more posts)
  int ret = sem.wait_for (20000); // 20ms timeout
  TCHECK (ret != 0, "Third sem.wait_for() should timeout");
}
TEST_ADD (semaphore_multiple_test);

// Test semaphore with concurrent waiters (first waiter gets the post)
static void
semaphore_concurrent_test()
{
  using namespace Ase;
  ScopedSemaphore sem;

  std::atomic<bool> waiter1_done {false};
  std::atomic<bool> waiter2_done {false};
  std::atomic<int>  waiter1_ret {0};
  std::atomic<int>  waiter2_ret {0};

  std::thread t1 ([&]() {
    waiter1_ret = sem.wait_for (100000); // 100ms
    waiter1_done = true;
  });

  std::thread t2 ([&]() {
    waiter2_ret = sem.wait_for (100000); // 100ms
    waiter2_done = true;
  });

  // Small delay to ensure both threads are waiting
  std::this_thread::sleep_for (std::chrono::milliseconds (10));

  // Post once - only one waiter should get it
  sem.post();

  t1.join();
  t2.join();

  // One waiter should succeed (ret == 0), the other should timeout
  TCHECK (waiter1_done && waiter2_done, "Both waiters should complete");
  TCHECK ((waiter1_ret == 0) != (waiter2_ret == 0), "Exactly one waiter should succeed");
}
TEST_ADD (semaphore_concurrent_test);

} // Anon
