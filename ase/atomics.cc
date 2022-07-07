// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "atomics.hh"
#include "internal.hh"
#include "testing.hh"

#define ADEBUG(...)     Ase::debug ("atomics", __VA_ARGS__)

namespace {
using namespace Ase;

static const size_t n_threads = std::thread::hardware_concurrency();

TEST_INTEGRITY (atomic_bits_test);

static void
atomic_bits_test ()
{
  const size_t N = 37;
  AtomicBits a (37);
  TASSERT (a.all (0));
  TASSERT (!a.all (1));
  TASSERT (!a.any (1));
  TASSERT (a.any (0));
  for (size_t i = 0; i < N; i++) {
    TASSERT (a.any (0));
    TASSERT (a[i] == false);
    a.set (i, 1);
    TASSERT (a[i] == true);
    TASSERT (a.any (1));
    TASSERT (!a.all (0));
    a.set (i, 0);
    TASSERT (a[i] == false);
    TASSERT (!a.any (1));
    TASSERT (a.all (0));
  }
  // note, toggling all N bits is not sufficient to change bits in all blocks
  // i.e. toggle all a.size() bits for all() to flip
  for (size_t i = 0; i < a.size(); i++)
    a[i] ^= 1;
  TASSERT (a.all (1));
  TASSERT (!a.all (0));
  TASSERT (!a.any (0));
  TASSERT (a.any (1));
  for (size_t i = 0; i < N; i++) {
    TASSERT (a.all (1));
    TASSERT (a[i] == true);
    a[i] ^= 0;
    TASSERT (a.all (1));
    TASSERT (a[i] == true);
    a.set (i, 0);
    TASSERT (a[i] == false);
    TASSERT (a.any (0));
    TASSERT (!a.all (0));
    a[i] ^= 1;
    TASSERT (a[i] == true);
    TASSERT (!a.any (0));
    TASSERT (a.all (1));
  }
}

} // Anon
