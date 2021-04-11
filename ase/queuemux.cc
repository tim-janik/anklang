// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <array>
#include <functional>
#include <limits>
#include <sys/time.h>

/// Multiplexer to pop from multiple Queues, while preserving priorities.
/// Order for values at the same priority is unstable.
/// Relies on unqualified calls to `Priority QueueMultiplexer_priority (const ValueType&)`.
template<size_t MAXQUEUES, class Queue, class ValueType = typename Queue::value_type>
struct QueueMultiplexer {
  using Priority = decltype (QueueMultiplexer_priority (std::declval<const ValueType&>()));
  struct Ptr { typename Queue::const_iterator it, end; };
  ssize_t  n_queues = 0, current = -1;
  Priority first = {}, next = {};
  std::array<Ptr, MAXQUEUES> ptrs;
  QueueMultiplexer (size_t nqueues, Queue **queues)
  {
    assign (nqueues, queues);
  }
  bool
  assign (size_t nqueues, Queue **queues)
  {
    assert (nqueues <= MAXQUEUES);
    n_queues = 0;
    for (size_t i = 0; i < nqueues; i++)
      if (queues[i]) [[likely]]
        {
          ptrs[n_queues].it = std::begin (*queues[i]);
          ptrs[n_queues].end = std::end (*queues[i]);
          if (ptrs[n_queues].it != ptrs[n_queues].end)
            n_queues++;
        }
    seek();
    return more();
  }
  bool
  more() const
  {
    return n_queues > 0;
  }
  const ValueType&
  pop ()
  {
    assert (more());
    const ValueType &result = *ptrs[current].it++;
    if (ptrs[current].it == ptrs[current].end) [[unlikely]]
      {                                         // remove emptied queue
        if (current < n_queues - 1) [[unlikely]]
          ptrs[current] = ptrs[n_queues - 1];    // shuffles queue order, preserves prios
        n_queues--;
        seek();
      }
    else if (QueueMultiplexer_priority (*ptrs[current].it)
             > next) [[unlikely]]               // next is in other queue
      seek();
    return result;
  }
private:
  void
  seek()
  {
    if (n_queues == 0) [[likely]]
      return;
    current = 0;                                  // picks first queue if all contain max Priority
    next = std::numeric_limits<Priority>::max();
    first = next;                               // Priority to start with
    for (ssize_t i = 0; i < n_queues; i++)
      {
        const Priority prio = QueueMultiplexer_priority (*ptrs[i].it);
        if (prio < first)                       // prio comes before first
          {
            next = first;
            first = prio;
            current = i;                          // pick first matching Queue
          }
        else if (prio < next)                   // found next prio
          next = prio;
      }
    dprintf (2, "%s: n_queues=%zd current=%zd first=%ld next=%ld\n", __func__, n_queues, current, long (first), long (next));
  }
};

struct SomeValue {
  int i;
};
static __attribute__ ((always_inline)) inline long
QueueMultiplexer_priority (const SomeValue &o)
{
  return o.i;
}

int
main (int argc, char *argv[])
{
  // generate ascending (sorted) sample values
  struct timeval tv;
  gettimeofday (&tv, NULL);
  srand (tv.tv_sec ^ tv.tv_usec);
  int ascending_counter = -17;
  auto ascending_values = [&ascending_counter] () {
    if (rand() >= RAND_MAX / 2)
      ascending_counter++;
    return ascending_counter;
  };
  std::vector<int> samples;
  for (size_t i = 0; i < 39; i++)
    samples.push_back (ascending_values());

  // setting: N queues contain ascending (sorted) values
  constexpr size_t N = 4;
  using Queue = std::vector<SomeValue>;
  std::vector<Queue> queues;
  queues.resize (N);
  for (int v : samples)
    queues[rand() % N].push_back (SomeValue { v });

  // task: fetch values from all queues in sorted order
  std::vector<Queue*> queue_ptrs;
  for (Queue &queue : queues)
    queue_ptrs.push_back (&queue);
  QueueMultiplexer<N, Queue> mux (queue_ptrs.size(), &queue_ptrs[0]);
  int last = -2147483648;
  size_t sc = 0;
  while (mux.more())
    {
      const SomeValue &current = mux.pop();
      printf ("%d\n", current.i);
      assert (current.i >= last);
      last = current.i;
      assert (sc < samples.size() && samples[sc++] == current.i);
    }
  assert (sc == samples.size());
  return 0;
}
// clang++ -std=gnu++20 -Wall -O3 -fverbose-asm queuemux.cc && ./a.out
