// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "signalmath.hh"
#include "internal.hh"

namespace Ase {

} // Ase

#include "testing.hh"

namespace { // Anon
using namespace Ase;

#define FEQUAL(a,b,eps)                                  do {   \
  auto __a = a; auto __b = b;                                   \
  if (std::abs (__a - __b) <= eps)                              \
    break;                                                      \
  std::string __m =                                             \
    Ase::string_format ("'%s ≈ %s': %+g ≈ %+g (Δ=%g)",          \
                        ASE_CPP_STRINGIFY (a),                  \
                        ASE_CPP_STRINGIFY (b),                  \
                        __a, __b, std::abs (__a - __b));        \
  Ase::assertion_failed (__m.c_str());                          \
  } while (0)

TEST_INTEGRITY (signalutils_tests);
static void
signalutils_tests()
{
  float f;
  double d;
  // check Voltage <-> Hertz
  constexpr auto fe = 0.005, ve = 0.000001;
  float w;
  w = -0.3; f =    +32.703;     FEQUAL (f, voltage2hz (w), fe); FEQUAL (w, hz2voltage (f), ve);
  ;                             FEQUAL (f, fast_voltage2hz (w), fe); FEQUAL (w, fast_hz2voltage (f), ve);
  w = +0.0; f =   +261.625;     FEQUAL (f, voltage2hz (w), fe); FEQUAL (w, hz2voltage (f), ve);
  ;                             FEQUAL (f, fast_voltage2hz (w), fe); FEQUAL (w, fast_hz2voltage (f), ve);
  w = +0.5; f =  +8372.018;     FEQUAL (f, voltage2hz (w), fe); FEQUAL (w, hz2voltage (f), ve);
  ;                             FEQUAL (f, fast_voltage2hz (w), fe); FEQUAL (w, fast_hz2voltage (f), ve);
  w = +0.6; f = +16744.036;     FEQUAL (f, voltage2hz (w), fe); FEQUAL (w, hz2voltage (f), ve);
  ;                             FEQUAL (f, fast_voltage2hz (w), fe); FEQUAL (w, fast_hz2voltage (f), ve);
  // check Voltage <-> dB
  constexpr auto de = 0.01;
  d = -6.0206; w = 0.5;         FEQUAL (d, voltage2db (w), de); FEQUAL (w, db2voltage (d), ve);
  ;                             FEQUAL (d, fast_voltage2db (w), de); FEQUAL (w, fast_db2voltage (d), ve);
  d = +0.0;    w = 1.0;         FEQUAL (d, voltage2db (w), de); FEQUAL (w, db2voltage (d), ve);
  ;                             FEQUAL (d, fast_voltage2db (w), de); FEQUAL (w, fast_db2voltage (d), ve);
  d = +7.9588; w = 2.5;         FEQUAL (d, voltage2db (w), de); FEQUAL (w, db2voltage (d), ve);
  ;                             FEQUAL (d, fast_voltage2db (w), de); FEQUAL (w, fast_db2voltage (d), ve);
}

} // Anon
