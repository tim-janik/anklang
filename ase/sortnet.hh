// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_SORTNET_HH__
#define __ASE_SORTNET_HH__

#include <functional>

namespace Ase {

namespace SortingNetworks {

template<class RandomIt, class Compare> inline __attribute__ ((always_inline)) void
srt (RandomIt &v1, RandomIt &v2, Compare lesser)
{
  // optimize for *not* needing to swap
  if (__builtin_expect (lesser (v2, v1), 0))
    std::swap (v2, v1);
}

template<class RandomIt, class Compare> void
sort_3 (RandomIt v, Compare c)
{
  srt (v[0], v[1], c); srt (v[0], v[2], c); srt (v[1], v[2], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_4 (RandomIt v, Compare c)
{
  // oddevenmerge 4
  srt (v[0], v[1], c); srt (v[2], v[3], c); srt (v[0], v[2], c);
  srt (v[1], v[3], c); srt (v[1], v[2], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_5 (RandomIt v, Compare c)
{
  // bitonic 5
  srt (v[0], v[1], c); srt (v[3], v[4], c); srt (v[2], v[4], c); srt (v[2], v[3], c); srt (v[1], v[4], c);
  srt (v[1], v[2], c); srt (v[0], v[3], c); srt (v[0], v[1], c); srt (v[2], v[3], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_6 (RandomIt v, Compare c)
{
  // oddevenmerge 6
  srt (v[0], v[1], c); srt (v[2], v[3], c); srt (v[4], v[5], c); srt (v[0], v[2], c);
  srt (v[1], v[3], c); srt (v[1], v[2], c); srt (v[0], v[4], c); srt (v[2], v[4], c);
  srt (v[1], v[5], c); srt (v[3], v[5], c); srt (v[1], v[2], c); srt (v[3], v[4], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_7 (RandomIt v, Compare c)
{
  // oddevenmerge 7
  srt (v[0], v[1], c); srt (v[2], v[3], c); srt (v[4], v[5], c); srt (v[0], v[2], c);
  srt (v[1], v[3], c); srt (v[4], v[6], c); srt (v[1], v[2], c); srt (v[5], v[6], c);
  srt (v[0], v[4], c); srt (v[2], v[6], c); srt (v[1], v[5], c); srt (v[2], v[4], c);
  srt (v[3], v[5], c); srt (v[1], v[2], c); srt (v[3], v[4], c); srt (v[5], v[6], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_8 (RandomIt v, Compare c)
{
  // oddevenmerge 8
  srt (v[0], v[1], c); srt (v[2], v[3], c); srt (v[4], v[5], c); srt (v[6], v[7], c); srt (v[0], v[2], c);
  srt (v[1], v[3], c); srt (v[4], v[6], c); srt (v[5], v[7], c); srt (v[1], v[2], c); srt (v[5], v[6], c);
  srt (v[0], v[4], c); srt (v[3], v[7], c); srt (v[2], v[6], c); srt (v[1], v[5], c); srt (v[2], v[4], c);
  srt (v[3], v[5], c); srt (v[1], v[2], c); srt (v[3], v[4], c); srt (v[5], v[6], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_9 (RandomIt v, Compare c)
{
  // https://imada.sdu.dk/~petersk/sn/
  srt (v[0], v[1], c); srt (v[2], v[3], c); srt (v[4], v[5], c); srt (v[6], v[7], c); srt (v[1], v[3], c);
  srt (v[5], v[7], c); srt (v[0], v[2], c); srt (v[4], v[6], c); srt (v[1], v[5], c); srt (v[3], v[7], c);
  srt (v[0], v[4], c); srt (v[2], v[6], c); srt (v[1], v[8], c); srt (v[2], v[4], c); srt (v[3], v[5], c);
  srt (v[1], v[2], c); srt (v[4], v[8], c); srt (v[2], v[4], c); srt (v[6], v[8], c); srt (v[0], v[1], c);
  srt (v[5], v[8], c); srt (v[3], v[6], c); srt (v[3], v[4], c); srt (v[5], v[6], c); srt (v[7], v[8], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_10 (RandomIt v, Compare c)
{
  // D. E. Knuth. The art of computer programming, vol. 3: Sorting and Searching, 2nd edition. Addison-Wesley, 1998
  srt (v[0], v[8], c); srt (v[1], v[9], c); srt (v[2], v[7], c); srt (v[3], v[5], c); srt (v[4], v[6], c);
  srt (v[0], v[2], c); srt (v[1], v[4], c); srt (v[5], v[8], c); srt (v[7], v[9], c); srt (v[0], v[3], c);
  srt (v[2], v[4], c); srt (v[5], v[7], c); srt (v[6], v[9], c); srt (v[0], v[1], c); srt (v[3], v[6], c);
  srt (v[8], v[9], c); srt (v[1], v[5], c); srt (v[2], v[3], c); srt (v[4], v[8], c); srt (v[6], v[7], c);
  srt (v[1], v[2], c); srt (v[3], v[5], c); srt (v[4], v[6], c); srt (v[7], v[8], c); srt (v[2], v[3], c);
  srt (v[4], v[5], c); srt (v[6], v[7], c); srt (v[3], v[4], c); srt (v[5], v[6], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_11 (RandomIt v, Compare c)
{
  // Harder, Jannis; https://github.com/jix/sortnetopt
  srt (v[0], v[9], c); srt (v[1], v[6], c); srt (v[2],  v[4], c); srt (v[3],  v[7], c); srt (v[5], v[8], c);
  srt (v[0], v[1], c); srt (v[3], v[5], c); srt (v[4], v[10], c); srt (v[6],  v[9], c); srt (v[7], v[8], c);
  srt (v[1], v[3], c); srt (v[2], v[5], c); srt (v[4],  v[7], c); srt (v[8], v[10], c); srt (v[0], v[4], c);
  srt (v[1], v[2], c); srt (v[3], v[7], c); srt (v[5],  v[9], c); srt (v[6],  v[8], c); srt (v[0], v[1], c);
  srt (v[2], v[6], c); srt (v[4], v[5], c); srt (v[7],  v[8], c); srt (v[9], v[10], c); srt (v[2], v[4], c);
  srt (v[3], v[6], c); srt (v[5], v[7], c); srt (v[8],  v[9], c); srt (v[1],  v[2], c); srt (v[3], v[4], c);
  srt (v[5], v[6], c); srt (v[7], v[8], c); srt (v[2],  v[3], c); srt (v[4],  v[5], c); srt (v[6], v[7], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_12 (RandomIt v, Compare c)
{
  // Harder, Jannis; https://github.com/jix/sortnetopt
  srt (v[0],  v[8], c); srt (v[1],   v[7], c); srt (v[2], v[6], c); srt (v[3], v[11], c); srt (v[4], v[10], c);
  srt (v[5],  v[9], c); srt (v[0],   v[1], c); srt (v[2], v[5], c); srt (v[3],  v[4], c); srt (v[6],  v[9], c);
  srt (v[7],  v[8], c); srt (v[10], v[11], c); srt (v[0], v[2], c); srt (v[1],  v[6], c); srt (v[5], v[10], c);
  srt (v[9], v[11], c); srt (v[0],   v[3], c); srt (v[1], v[2], c); srt (v[4],  v[6], c); srt (v[5],  v[7], c);
  srt (v[8], v[11], c); srt (v[9],  v[10], c); srt (v[1], v[4], c); srt (v[3],  v[5], c); srt (v[6],  v[8], c);
  srt (v[7], v[10], c); srt (v[1],   v[3], c); srt (v[2], v[5], c); srt (v[6],  v[9], c); srt (v[8], v[10], c);
  srt (v[2],  v[3], c); srt (v[4],   v[5], c); srt (v[6], v[7], c); srt (v[8],  v[9], c); srt (v[4],  v[6], c);
  srt (v[5],  v[7], c); srt (v[3],   v[4], c); srt (v[5], v[6], c); srt (v[7],  v[8], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_13 (RandomIt v, Compare c)
{
  // http://bertdobbelaere.github.io/sorting_networks.html
  srt (v[0],  v[12], c); srt (v[1],  v[10], c); srt (v[2], v[9], c); srt (v[3],  v[7], c); srt (v[5], v[11], c);
  srt (v[6],   v[8], c); srt (v[1],   v[6], c); srt (v[2], v[3], c); srt (v[4], v[11], c); srt (v[7],  v[9], c);
  srt (v[8],  v[10], c); srt (v[0],   v[4], c); srt (v[1], v[2], c); srt (v[3],  v[6], c); srt (v[7],  v[8], c);
  srt (v[9],  v[10], c); srt (v[11], v[12], c); srt (v[4], v[6], c); srt (v[5],  v[9], c); srt (v[8], v[11], c);
  srt (v[10], v[12], c); srt (v[0],   v[5], c); srt (v[3], v[8], c); srt (v[4],  v[7], c); srt (v[6], v[11], c);
  srt (v[9],  v[10], c); srt (v[0],   v[1], c); srt (v[2], v[5], c); srt (v[6],  v[9], c); srt (v[7],  v[8], c);
  srt (v[10], v[11], c); srt (v[1],   v[3], c); srt (v[2], v[4], c); srt (v[5],  v[6], c); srt (v[9], v[10], c);
  srt (v[1],   v[2], c); srt (v[3],   v[4], c); srt (v[5], v[7], c); srt (v[6],  v[8], c); srt (v[2],  v[3], c);
  srt (v[4],   v[5], c); srt (v[6],   v[7], c); srt (v[8], v[9], c); srt (v[3],  v[4], c); srt (v[5],  v[6], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_14 (RandomIt v, Compare c)
{
  // http://bertdobbelaere.github.io/sorting_networks.html
  srt (v[0],  v[6], c); srt (v[1], v[11], c); srt (v[2],  v[12], c); srt (v[3],  v[10], c); srt (v[4],   v[5], c);
  srt (v[7], v[13], c); srt (v[8],  v[9], c); srt (v[1],   v[2], c); srt (v[3],   v[7], c); srt (v[4],   v[8], c);
  srt (v[5],  v[9], c); srt (v[6], v[10], c); srt (v[11], v[12], c); srt (v[0],   v[4], c); srt (v[1],   v[3], c);
  srt (v[5],  v[6], c); srt (v[7],  v[8], c); srt (v[9],  v[13], c); srt (v[10], v[12], c); srt (v[0],   v[1], c);
  srt (v[2],  v[9], c); srt (v[3],  v[7], c); srt (v[4],  v[11], c); srt (v[6],  v[10], c); srt (v[12], v[13], c);
  srt (v[2],  v[5], c); srt (v[4],  v[7], c); srt (v[6],   v[9], c); srt (v[8],  v[11], c); srt (v[1],   v[2], c);
  srt (v[3],  v[4], c); srt (v[6],  v[7], c); srt (v[9],  v[10], c); srt (v[11], v[12], c); srt (v[1],   v[3], c);
  srt (v[2],  v[4], c); srt (v[5],  v[6], c); srt (v[7],   v[8], c); srt (v[9],  v[11], c); srt (v[10], v[12], c);
  srt (v[2],  v[3], c); srt (v[4],  v[7], c); srt (v[6],   v[9], c); srt (v[10], v[11], c); srt (v[4],   v[5], c);
  srt (v[6],  v[7], c); srt (v[8],  v[9], c); srt (v[3],   v[4], c); srt (v[5],   v[6], c); srt (v[7],   v[8], c);
  srt (v[9], v[10], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_15 (RandomIt v, Compare c)
{
  // http://bertdobbelaere.github.io/sorting_networks.html
  srt (v[1],   v[2], c); srt (v[3], v[10], c); srt (v[4],  v[14], c); srt (v[5],   v[8], c); srt (v[6],  v[13], c);
  srt (v[7],  v[12], c); srt (v[9], v[11], c); srt (v[0],  v[14], c); srt (v[1],   v[5], c); srt (v[2],   v[8], c);
  srt (v[3],   v[7], c); srt (v[6],  v[9], c); srt (v[10], v[12], c); srt (v[11], v[13], c); srt (v[0],   v[7], c);
  srt (v[1],   v[6], c); srt (v[2],  v[9], c); srt (v[4],  v[10], c); srt (v[5],  v[11], c); srt (v[8],  v[13], c);
  srt (v[12], v[14], c); srt (v[0],  v[6], c); srt (v[2],   v[4], c); srt (v[3],   v[5], c); srt (v[7],  v[11], c);
  srt (v[8],  v[10], c); srt (v[9], v[12], c); srt (v[13], v[14], c); srt (v[0],   v[3], c); srt (v[1],   v[2], c);
  srt (v[4],   v[7], c); srt (v[5],  v[9], c); srt (v[6],   v[8], c); srt (v[10], v[11], c); srt (v[12], v[13], c);
  srt (v[0],   v[1], c); srt (v[2],  v[3], c); srt (v[4],   v[6], c); srt (v[7],   v[9], c); srt (v[10], v[12], c);
  srt (v[11], v[13], c); srt (v[1],  v[2], c); srt (v[3],   v[5], c); srt (v[8],  v[10], c); srt (v[11], v[12], c);
  srt (v[3],   v[4], c); srt (v[5],  v[6], c); srt (v[7],   v[8], c); srt (v[9],  v[10], c); srt (v[2],   v[3], c);
  srt (v[4],   v[5], c); srt (v[6],  v[7], c); srt (v[8],   v[9], c); srt (v[10], v[11], c); srt (v[5],   v[6], c);
  srt (v[7],   v[8], c);
}

template<class RandomIt, class Compare> void __attribute__ ((noinline))
sort_16 (RandomIt v, Compare c)
{
  // http://bertdobbelaere.github.io/sorting_networks.html
  srt (v[0],  v[13], c); srt (v[1],  v[12], c); srt (v[2],  v[15], c); srt (v[3],  v[14], c); srt (v[4],   v[8], c);
  srt (v[5],   v[6], c); srt (v[7],  v[11], c); srt (v[9],  v[10], c); srt (v[0],   v[5], c); srt (v[1],   v[7], c);
  srt (v[2],   v[9], c); srt (v[3],   v[4], c); srt (v[6],  v[13], c); srt (v[8],  v[14], c); srt (v[10], v[15], c);
  srt (v[11], v[12], c); srt (v[0],   v[1], c); srt (v[2],   v[3], c); srt (v[4],   v[5], c); srt (v[6],   v[8], c);
  srt (v[7],   v[9], c); srt (v[10], v[11], c); srt (v[12], v[13], c); srt (v[14], v[15], c); srt (v[0],   v[2], c);
  srt (v[1],   v[3], c); srt (v[4],  v[10], c); srt (v[5],  v[11], c); srt (v[6],   v[7], c); srt (v[8],   v[9], c);
  srt (v[12], v[14], c); srt (v[13], v[15], c); srt (v[1],   v[2], c); srt (v[3],  v[12], c); srt (v[4],   v[6], c);
  srt (v[5],   v[7], c); srt (v[8],  v[10], c); srt (v[9],  v[11], c); srt (v[13], v[14], c); srt (v[1],   v[4], c);
  srt (v[2],   v[6], c); srt (v[5],   v[8], c); srt (v[7],  v[10], c); srt (v[9],  v[13], c); srt (v[11], v[14], c);
  srt (v[2],   v[4], c); srt (v[3],   v[6], c); srt (v[9],  v[12], c); srt (v[11], v[13], c); srt (v[3],   v[5], c);
  srt (v[6],   v[8], c); srt (v[7],   v[9], c); srt (v[10], v[12], c); srt (v[3],   v[4], c); srt (v[5],   v[6], c);
  srt (v[7],   v[8], c); srt (v[9],  v[10], c); srt (v[11], v[12], c); srt (v[6],   v[7], c); srt (v[8],   v[9], c);
}

} // SortingNetworks

/// Use sorting networks to sort containers <= 16 elements without allocations.
template<class RandomIt, class Compare> inline void
fixed_sort (RandomIt first, RandomIt last,
            Compare comp = std::less<typename std::iterator_traits<RandomIt>::value_type>())
{
  using namespace SortingNetworks;
  const size_t n = last - first;
  switch (n)
    {
    case 0:     break;
    case 1:     break;
    case 2:     return srt (first[0], first[1], comp);
    case 3:     return sort_3 (first, comp);
    case 4:     return sort_4 (first, comp);
    case 5:     return sort_5 (first, comp);
    case 6:     return sort_6 (first, comp);
    case 7:     return sort_7 (first, comp);
    case 8:     return sort_8 (first, comp);
    case 9:     return sort_9 (first, comp);
    case 10:    return sort_10 (first, comp);
    case 11:    return sort_11 (first, comp);
    case 12:    return sort_12 (first, comp);
    case 13:    return sort_13 (first, comp);
    case 14:    return sort_14 (first, comp);
    case 15:    return sort_15 (first, comp);
    case 16:    return sort_16 (first, comp);
    default:    return std::sort (first, last, comp);
    }
}

/// Vector that keeps its elements sorted.
template<class T, class Less = std::less<T>>
class SortedVector : private Less {
  std::vector<T> v_;
public:
  using size_type = typename std::vector<T>::size_type;
  using value_type = typename std::vector<T>::value_type;
  using iterator = typename std::vector<T>::iterator;
  using const_iterator = typename std::vector<T>::const_iterator;
  using reverse_iterator = typename std::vector<T>::reverse_iterator;
  using const_reverse_iterator = typename std::vector<T>::const_reverse_iterator;
  using const_reference = typename std::vector<T>::const_reference;
  constexpr void                   reserve       (size_type n) { v_.reserve (n); }
  constexpr size_type              capacity      () const noexcept { return v_.capacity(); }
  constexpr const_iterator         begin         () const noexcept { return v_.begin(); }
  constexpr const_iterator         end           () const noexcept { return v_.end(); }
  constexpr const_reverse_iterator crbegin       () const noexcept { return v_.crbegin(); }
  constexpr const_reverse_iterator crend         () const noexcept { return v_.crend(); }
  constexpr const_reverse_iterator rbegin        () const noexcept { return v_.rbegin(); }
  constexpr const_reverse_iterator rend          () const noexcept { return v_.rend(); }
  constexpr void                   clear         () noexcept { v_.clear(); }
  constexpr bool                   empty         () const noexcept { return v_.empty(); }
  constexpr iterator               erase         (const_iterator first, const_iterator last) { return v_.erase (first, last); }
  constexpr iterator               erase         (const_iterator position) { return v_.erase (position); }
  constexpr iterator               replace       (const value_type &val) { return insert (val, true); }
  constexpr size_type              size          () const noexcept { return v_.size(); }
  constexpr void                   shrink_to_fit () { v_.shrink_to_fit(); }
  constexpr bool                   sorted        (const bool allow_multiple = false) const { return sorted (*this, allow_multiple); }
  constexpr bool                   contains      (const value_type &val) const { return end() != find (val); }
  constexpr const_iterator         find          (const value_type &val) const { return const_cast<SortedVector&> (*this).find (val); }
  constexpr const_reference        front         () const noexcept { return v_.front(); }
  constexpr const_reference        back          () const noexcept { return v_.back(); }
  constexpr const T*               data          () const noexcept { return v_.data(); }
  constexpr T*                     data          () noexcept { return v_.data(); }
  constexpr void                   swap          (std::vector<T> &other) noexcept { v_.swap (other); sort(); }
  constexpr void                   swap          (SortedVector &other) noexcept { v_.swap (other); }
  constexpr const_reference        at            (size_type n) const { return v_.at (n); }
  constexpr const_reference        operator[]    (size_type n) const noexcept { return v_[n]; }
  constexpr SortedVector&          operator=     (const SortedVector &other) { v_ = other.v_; return *this; }
  constexpr SortedVector&
  operator= (const std::vector<T> &other)
  {
    v_ = other;
    sort();
    return *this;
  }
  constexpr SortedVector&
  operator= (std::initializer_list<value_type> l)
  {
    v_.assign (l);
    sort();
    return *this;
  }
  constexpr SortedVector&
  operator= (SortedVector &&other)
  {
    v_.clear();
    v_.swap (other);
    return *this;
  }
  constexpr SortedVector&
  operator= (std::vector<T> &&other)
  {
    v_.clear();
    v_.swap (other);
    sort();
    return *this;
  }
  constexpr void
  resize (size_type n, const value_type &el)
  {
    const size_type o = v_.size();
    v_.resize (n, el);
    if (v_.size() > o)
      sort();
  }
  constexpr void
  assign (std::initializer_list<value_type> l)
  {
    v_.assign (l);
    sort();
  }
  constexpr void
  sort ()
  {
    const Less &lesser = *this;
    fixed_sort (v_.begin(), v_.end(), lesser);
  }
  constexpr iterator
  find (const value_type &val)
  {
    const Less &lesser = *this;
    iterator it = std::lower_bound (v_.begin(), v_.end(), val, lesser);
    if (it == v_.end() || lesser (val, *it))
      return v_.end();                          // not found
    return it;                                  // found
  }
  constexpr iterator
  insert (const value_type &val, bool replace = false)
  {
    const Less &lesser = *this;
    iterator it = std::lower_bound (v_.begin(), v_.end(), val, lesser);
    if (it != end() && !lesser (val, *it))      // val == *it
      {
        if (!replace)
          return v_.end();                      // insertion failed, duplicate
        *it = val;
        return it;                              // duplicate replaced
      }
    return v_.insert (it, val);                 // insert or append
  }
  constexpr bool
  sorted (const Less &lesser, const bool allow_multiple = false) const
  {
    const size_type l = size();
    for (size_type i = 1; i < l; i++)
      if (lesser (v_[i-1], v_[i]) == false)
        {
          if (allow_multiple && lesser (v_[i], v_[i-1]) == false)
            continue; // v_[i-1] == v_[i]
          return false;
        }
    return true;
  }
  constexpr size_type
  collapse (bool delete_first = true)
  {
    const Less &lesser = *this;
    const size_type l = size();
    for (size_type i = 1; i < size(); i++)
      if (lesser (v_[i-1], v_[i]) == false)
        {
          erase (begin() + i - delete_first);
          i--;
        }
    return l - size();
  }
  template<class Pred> constexpr size_type
  erase_if (Pred pred)
  {
    const size_type l = size();
    for (iterator it = v_.begin(); it != v_.end(); )
      if (pred (*it))
        it = v_.erase (it);
      else
        ++it;
    return l - size();
  }
  template<class InputIterator> constexpr
  SortedVector (InputIterator first, InputIterator last) :
    v_ (first, last)
  {
    sort();
  }
  constexpr
  SortedVector (std::initializer_list<value_type> l = {})
  {
    *this = l;
  }
  operator const std::vector<T> () const { return v_; }
  template<class Pred> friend constexpr size_type erase_if (SortedVector &self, Pred pred) { return self.erase_if (pred); }
};

} // Ase

#endif // __ASE_SORTNET_HH__
