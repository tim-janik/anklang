// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "mathutils.hh"
#include "internal.hh"

namespace Ase {

} // Ase

#include "testing.hh"

namespace { // Anon
using namespace Ase;

TEST_INTEGRITY (mathutils_tests);
static void
mathutils_tests()
{
  float f;
  double d;
  // check proper Inf and NaN handling (depends on compiler flags)
  f = d = .5 * INFINITY; TASSERT (d > 0 && f > 0 && std::isinf (f) && std::isinf (d));
  f = d = -3 * INFINITY; TASSERT (d < 0 && f < 0 && std::isinf (f) && std::isinf (d));
  f = f - f;             TASSERT (!(f == f) && std::isnan (f));  // Infinity - Infinity yields NaN
  d = 0.0 / 0.0;         TASSERT (!(d == d) && std::isnan (d));
  // check rounding
  TASSERT (int (+0.40) == +0.0 && irintf (+0.40) == +0.0);
  TASSERT (int (-0.40) == -0.0 && irintf (-0.40) == -0.0);
  TASSERT (int (+0.51) == +0.0 && irintf (+0.51) == +1.0);
  TASSERT (int (-0.51) == -0.0 && irintf (-0.51) == -1.0);
  TASSERT (int (+1.90) == +1.0 && irintf (+1.90) == +2.0);
  TASSERT (int (-1.90) == -1.0 && irintf (-1.90) == -2.0);
  // check fast_exp2(int), 2^(-126..+127) must be calculated with 0 error
  volatile float mf = 1.0, pf = 1.0;
  for (ssize_t i = 0; i <= 127; i++)
    {
      // printerr ("2^±%-3d = %15.10g, %-.14g\n", i, fast_exp2 (i), fast_exp2 (-i));
      TASSERT (fast_exp2 (i) == pf);
      if (i != 127)
        TASSERT (fast_exp2 (-i) == mf);
      else
        TASSERT (fast_exp2 (-i) <= mf); // single precision result for 2^-127 is 0 or subnormal
      pf *= 2;
      mf /= 2;
    }
  // check fast_exp2 error margin in [-1..+1]
  const double exp2step = Test::slow() ? 0.0000001 : 0.0001;
  for (d = -1; d <= +1; d += exp2step)
    {
      const double r = std::exp2 (d);
      const double err = std::fabs (r - fast_exp2 (d));
      TASSERT (err < 4e-7);
    }
  // check fast_log2(int), log2(2^(1..127)) must be calculated with 0 error
  pf = 1.0;
  for (ssize_t i = 0; i <= 127; i++)
    {
      if (fast_log2 (pf) != float (i))
        printerr ("fast_log2(%.17g)=%.17g\n", pf, fast_log2 (pf));
      TASSERT (fast_log2 (pf) == float (i));
      pf *= 2;
    }
  TASSERT (-126 == fast_log2 (1.1754944e-38));
  TASSERT ( -64 == fast_log2 (5.421011e-20));
  TASSERT ( -16 == fast_log2 (1.525879e-5));
  TASSERT (  -8 == fast_log2 (0.00390625));
  TASSERT (  -4 == fast_log2 (0.0625));
  TASSERT (  -2 == fast_log2 (0.25));
  TASSERT (  -1 == fast_log2 (0.5));
  TASSERT (   0 == fast_log2 (1));
  // check fast_log2 error margin in [1/16..16]
  const double log2step = Test::slow() ? 0.0000001 : 0.0001;
  for (double d = 1 / 16.; d <= 16; d += log2step)
    {
      const double r = std::log2 (d);
      const double err = std::fabs (r - fast_log2 (d));
      if (!(err < 3.8e-6))
        printerr ("fast_log2(%.17g)=%.17g (diff=%.17g)\n", d, fast_log2 (d), r - fast_log2 (d));
      TASSERT (err < 3.8e-6);
    }
}

} // Anon
