// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ATOMICS_HH__
#define __ASE_ATOMICS_HH__

#include <ase/platform.hh>
#include <atomic>

namespace Ase {

// == AtomicIntrusiveStack ==

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

// == AtomicBits ==
using AtomicU64 = std::atomic<uint64>;

/// Vector of atomic bits, operates in blocks of 64 bits.
class AtomicBits : protected std::vector<AtomicU64> {
protected:
  std::vector<AtomicU64>&       base()       { return *this; }
  const std::vector<AtomicU64>& base() const { return *this; }
public:
  class Iter {
    AtomicBits *atomics_ = nullptr;
    size_t u_ = ~size_t (0), s_ = 0;
    AtomicU64& ubits() const { return const_cast<Iter*> (this)->atomics_->u64 (u_); }
    size_t     usize() const { return atomics_ ? atomics_->usize() : 0; }
  public:
    explicit Iter     (AtomicBits *a, size_t p) : atomics_ (a), u_ (p >> 6), s_ (p & 63) {}
    /*ctor*/ Iter     () = default;
    /*copy*/ Iter     (const Iter&) = default;
    Iter&    operator= (const Iter&) = default;
    size_t   position () const { return u_ + s_; }
    bool     is_set   () const { return valid() && ubits().load() & (uint64 (1) << s_); }
    bool     done     () const { return !valid(); }
    bool     valid    () const { return atomics_ && u_ < usize(); }
    bool     clear    ();
    bool     set      (bool toggle);
    bool     xor_     (bool toggle);
    Iter&    big_inc1 ();
    Iter&    operator=  (bool toggle) { set (toggle); return *this; }
    Iter&    operator^= (bool toggle) { xor_ (toggle); return *this; }
    Iter&    operator|= (bool toggle) { if (toggle) set (1); return *this; }
    Iter&    operator&= (bool toggle) { if (!toggle) set (0); return *this; }
    Iter&    operator++ () { if (!done()) { s_ = ++s_ & 63; u_ += s_ == 0; } return *this;}
    Iter&    operator++ (int) { return this->operator++(); }
    bool     operator== (const Iter &o) const { return (done() && o.done()) || position() == o.position(); }
    bool     operator!= (const Iter &o) const { return !operator== (o); }
    bool     operator== (bool b) const { return b == is_set(); }
    bool     operator!= (bool b) const { return b != is_set(); }
    explicit operator bool () const { return is_set(); }
  };
  /*dtor*/   ~AtomicBits () {}
  explicit    AtomicBits (size_t nbits) : std::vector<AtomicU64> ((nbits + 63) / 64) {}
  explicit    AtomicBits (const AtomicBits&) = delete;
  size_t      usize      () const { return base().size(); }
  size_t      size       () const { return 64 * usize(); }
  uint64      u64        (size_t upos) const { return base()[upos]; }
  AtomicU64&  u64        (size_t upos) { return base()[upos]; }
  AtomicBits& operator=  (const AtomicBits&) = delete;
  Iter        iter       (size_t pos) { return Iter (this, pos); }
  Iter        begin      () { return iter (0); }
  Iter        end        () const { return {}; }
  bool        all        (bool toggle) const;
  bool        any        (bool toggle) const;
  bool        set        (size_t pos, bool toggle) { return iter (pos).set (toggle); }
  bool        operator[] (size_t pos) const { return const_cast<AtomicBits*> (this)->iter (pos).is_set(); }
  Iter        operator[] (size_t pos) { return iter (pos); }
};

inline bool
AtomicBits::all (bool toggle) const
{
  const uint64 match = toggle * ~uint64 (0);
  for (size_t u = 0; u < usize(); u++)
    if (u64 (u) != match)
      return false;
  return true;
}

inline bool
AtomicBits::any (bool toggle) const
{
  if (toggle) {
    for (size_t u = 0; u < usize(); u++)
      if (u64 (u))
        return true;
  } else {
    for (size_t u = 0; u < usize(); u++)
      if (u64 (u) != ~uint64 (0))
        return true;
  }
  return false;
}

inline bool
AtomicBits::Iter::set (bool toggle)
{
  ASE_ASSERT_RETURN (!done(), false);
  bool last;
  if (toggle)
    last = ubits().fetch_or (uint64 (1) << s_);
  else
    last = ubits().fetch_and (~(uint64 (1) << s_));
  return last;
}

inline bool
AtomicBits::Iter::clear()
{
  if (ASE_ISLIKELY (valid()) &&
      ASE_ISLIKELY (!(ubits().load() & (uint64 (1) << s_))))
    return false; // fast path
  return set (0);
}

inline bool
AtomicBits::Iter::xor_ (bool toggle)
{
  ASE_ASSERT_RETURN (!done(), false);
  bool last;
  last = ubits().fetch_xor (toggle ? uint64 (1) << s_ : 0);
  return last;
}

/// Increment iterator by 1, allow big increments skipping zero bits.
inline AtomicBits::Iter&
AtomicBits::Iter::big_inc1 ()
{
  ASE_RETURN_UNLESS (valid(), *this);
  // if any bit is set in current block, increment by one
  if (ubits().load())
    return ++*this;
  // else, jump to next block
  s_ = 0;
  do
    u_++;
  while (!ubits().load() && u_ < usize());
  return *this;
}

} // Ase

#endif /* __ASE_ATOMICS_HH__ */
