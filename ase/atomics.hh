// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ATOMICS_HH__
#define __ASE_ATOMICS_HH__

#include <ase/platform.hh>

namespace Ase {

/// Wait-free stack with atomic `push()` and `pop_all` operations.
/// Relies on unqualified calls to `atomic<T*>& atomic_next_ptrref(T*)`.
template<class T>
class AtomicIntrusiveStack {
  using AtomicTPtr = std::atomic<T*>;
  std::atomic<T*> head_ = nullptr;
public:
  AtomicIntrusiveStack()
  {
    static_assert (std::is_same<AtomicTPtr&, decltype (atomic_next_ptrref (std::declval<T*>()))>::value,
                   "Required: std::atomic<T*>& atomic_next_ptrref (T&)");
  }
  /// Check if poppingreturns null.
  bool
  empty () const
  {
    return head_.load() == nullptr;
  }
  /// Atomically push linked nodes `first->…->last` onto the stack, returns `was_empty`.
  bool
  push_chain (T *first, T *last)
  {
    AtomicTPtr &last_nextptr = atomic_next_ptrref (last);
    ASE_ASSERT_RETURN (last_nextptr == nullptr, false);
    T *exchange = head_.load();
    do // compare_exchange_strong() == false does: exchange = head_;
      last_nextptr = exchange;
    while (!head_.compare_exchange_strong (exchange, first));
    const bool was_empty = exchange == nullptr;
    return was_empty;
  }
  /// Atomically push `el` onto the stack, returns `was_empty`.
  bool
  push (T *el)
  {
    return push_chain (el, el);
  }
  /// Atomically pop all elements from the stack in LIFO order.
  /// Use `atomic_next_ptrref()` for iteration.
  T*
  pop_all ()
  {
    T *exchange = head_.load();
    do // compare_exchange_strong() == false does: exchange = head_;
      {}
    while (exchange && !head_.compare_exchange_strong (exchange, nullptr));
    return exchange;
  }
  /// Atomically pop all elements from the stack in FIFO order.
  /// Use `atomic_next_ptrref()` for iteration.
  T*
  pop_reversed()
  {
    T *current = pop_all(), *prev = nullptr;;
    while (current)
      {
        AtomicTPtr &el_nextptr = atomic_next_ptrref (current);
        T *next = el_nextptr;
        el_nextptr = prev;
        prev = current;
        current = next;
      }
    return prev;
  }
};

} // Ase

#endif /* __ASE_ATOMICS_HH__ */
