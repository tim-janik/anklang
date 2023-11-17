// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_QUEUEMUX_HH__
#define __ASE_QUEUEMUX_HH__

#include <ase/cxxaux.hh>

namespace Ase {

/// Multiplexer to pop from multiple Queues, while preserving priorities.
/// Order for values at the same priority is unstable.
/// Relies on unqualified calls to `Priority QueueMultiplexer_priority (const ValueType&)`.
template<size_t MAXQUEUES, class ForwardIterator>
  requires std::forward_iterator<ForwardIterator>
struct QueueMultiplexer {
  using ValueType = std::remove_reference_t<decltype (*std::declval<ForwardIterator>())>;
  using Priority = decltype (QueueMultiplexer_priority (std::declval<const ValueType&>()));
  struct Ptr { ForwardIterator it, end; };
  ssize_t  n_queues = 0, current = -1;
  Priority first = {}, next = {};
  std::array<Ptr, MAXQUEUES> ptrs;
  QueueMultiplexer () {}
  template<class IterableContainer> bool
  assign (const std::array<const IterableContainer*, MAXQUEUES> &queues)
  {
    static_assert (std::is_same<
                   ForwardIterator,
                   decltype (std::begin (std::declval<const IterableContainer&>()))
                   >::value);
    n_queues = 0;
    for (size_t i = 0; i < queues.size(); i++)
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
  size_t
  count_pending() const noexcept
  {
    size_t c = 0;
    for (ssize_t i = 0; i < n_queues; i++)
      c += ptrs[i].end - ptrs[i].it;
    return c;
  }
  bool
  more() const noexcept
  {
    return n_queues > 0;
  }
  const ValueType&
  peek () noexcept
  {
    if (!more()) [[unlikely]]
      return empty();
    return *ptrs[current].it;
  }
  const ValueType&
  pop () noexcept
  {
    ASE_ASSERT_RETURN (more(), empty());
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
  class Iter {
    QueueMultiplexer *mux_ = nullptr;
  public:
    using difference_type = ssize_t;
    using value_type = const ValueType;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = std::input_iterator_tag;
    /*ctor*/    Iter       (QueueMultiplexer *u = nullptr) : mux_ (u && u->more() ? u : nullptr) {}
    bool        more       () const { return mux_ && mux_->more(); }
    friend bool operator== (const Iter &a, const Iter &b) { return a.more() == b.more(); }
    value_type& operator*  () const { return mux_ ? mux_->peek() : empty(); }
    Iter        operator++ (int) { Iter copy (*this); this->operator++(); return copy; }
    Iter&       operator++ ()
    {
      if (mux_) [[likely]] {
        if (mux_->more()) [[likely]]
          mux_->pop();
        else
          mux_ = nullptr;
      }
      return *this;
    }
  };
  using iterator = Iter;
  iterator begin ()     { return Iter (this); }
  iterator end   ()     { return {}; }
private:
  void
  seek() noexcept
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
  static const ValueType&
  empty() noexcept ASE_NOINLINE
  {
    static const ValueType empty_ {};
    return empty_;
  }
};

} // Ase

#endif /* __ASE_QUEUEMUX_HH__ */
