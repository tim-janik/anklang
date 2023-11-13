// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "queuemux.hh"
#include "randomhash.hh"
#include "internal.hh"
#include "testing.hh"

#define QDEBUG(...)     Ase::debug ("queuemux", __VA_ARGS__)

namespace {
using namespace Ase;

struct SomeValue {
  int i;
};

static __attribute__ ((always_inline)) inline long
QueueMultiplexer_priority (const SomeValue &o)
{
  return o.i;
}

TEST_INTEGRITY (queuemux_test);
static void
queuemux_test()
{
  // generate ascending (sorted) sample values
  int ascending_counter = -17;
  auto ascending_values = [&ascending_counter] () {
    if (random_int64() & 1)
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
    queues[random_int64() % N].push_back (SomeValue { v });

  // task: fetch values from all queues in sorted order
  std::vector<Queue*> queue_ptrs;
  for (Queue &queue : queues)
    queue_ptrs.push_back (&queue);
  QueueMultiplexer<N, Queue::const_iterator> mux;
  mux.assign (queue_ptrs.size(), &queue_ptrs[0]);
  int last = -2147483648;
  size_t sc = 0;
  while (mux.more())
    {
      const SomeValue &current = mux.pop();
      QDEBUG ("QueueMultiplexer: %d\n", current.i);
      TASSERT (current.i >= last);
      last = current.i;
      TASSERT (sc < samples.size() && samples[sc++] == current.i);
    }
  TASSERT (sc == samples.size());
}

} // Anon
