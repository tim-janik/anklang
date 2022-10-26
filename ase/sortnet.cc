// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "sortnet.hh"
#include "defs.hh"
#include "internal.hh"

// == Testing ==
#include "testing.hh"

namespace { // Anon
using namespace Ase;

template<size_t N, class T = int> void
check_permutations ()
{
  std::array<T,N> permarray;
  for (size_t i = 0; i < N; i++)
    permarray[i] = i;
  do
    {
      std::array<T,N> array { permarray };
      fixed_sort (&array[0], &array[N], std::less<T>());
      for (size_t i = 1; i < N; i++)
        assert_return (array[i - 1] <= array[i]);
    }
  while (std::next_permutation (&permarray[0], &permarray[N]));
}

static uint64_t romu_state64a = 1, romu_state64b = 0xda942042e4dd58b5ULL;
static inline uint64_t
romumono2()
{
  const uint64_t result1 = romu_state64a, result2 = romu_state64b;
  romu_state64a = rotl (romu_state64a, 32) * 15241094284759029579u;
  romu_state64b = rotl (romu_state64b, 32) * 0x5851f42d4c957f2dUL;
  return result1 | (uint64_t (result2) << 32);
}

template<size_t N, class T = int> void
check_randomized (size_t runs)
{
  std::array<T,N> array;
  for (size_t j = 0; j < runs; j++)
    {
      for (size_t i = 0; i < N; i++)
        array[i] = romumono2();
      fixed_sort (&array[0], &array[N], std::less<T>());
      for (size_t i = 1; i < N; i++)
        assert_return (array[i - 1] <= array[i]);
    }
}

TEST_INTEGRITY (sortnet_tests);

static void
sortnet_tests()
{
  { // setup PRNG
    romu_state64a = Test::random_int64() | 1;
    romu_state64b = Test::random_int64();
    romumono2(); romumono2(); romumono2(); romumono2(); romumono2();
  }
  constexpr const size_t RUNS = 9999;
  check_permutations<1,int>();
  check_permutations<2,int>();
  check_permutations<3,int>();
  check_permutations<4,int>();
  check_permutations<5,int>();
  check_permutations<6,int>();
  check_permutations<7,int>();
  check_permutations<8,int>();
  check_permutations<9,int>();
  check_randomized<10,int> (RUNS);
  check_randomized<11,int> (RUNS);
  check_randomized<12,int> (RUNS);
  check_randomized<13,int> (RUNS);
  check_randomized<14,int> (RUNS);
  check_randomized<15,int> (RUNS);
  check_randomized<16,int> (RUNS);
}

} // Anon
