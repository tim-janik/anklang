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
  // http://users.telenet.be/bertdobbelaere/SorterHunter/sorting_networks.html
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
  // http://users.telenet.be/bertdobbelaere/SorterHunter/sorting_networks.html
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
  // http://users.telenet.be/bertdobbelaere/SorterHunter/sorting_networks.html
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
  // http://users.telenet.be/bertdobbelaere/SorterHunter/sorting_networks.html
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

} // Ase

#endif // __ASE_SORTNET_HH__
