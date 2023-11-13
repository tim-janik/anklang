// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_QUEUEMUX_HH__
#define __ASE_QUEUEMUX_HH__

#include <ase/cxxaux.hh>

namespace Ase {

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
    n_queues = 0;
    ASE_ASSERT_RETURN (nqueues <= MAXQUEUES, false);
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
    ASE_ASSERT_RETURN (more(), []() -> const ValueType& { static const ValueType empty {}; return empty; }());
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
    // dprintf (2, "%s: n_queues=%zd current=%zd first=%ld next=%ld\n", __func__, n_queues, current, long (first), long (next));
  }
};

} // Ase

#endif /* __ASE_QUEUEMUX_HH__ */
