// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_SIGNALUTILS_HH__
#define __ASE_SIGNALUTILS_HH__

#include <ase/mathutils.hh>

namespace Ase {

/// Determine a significant frequency change (audible Hertz).
template<typename Float> Float hz_changed (Float a, Float b);

/// Convert synthesizer value (Voltage) to Hertz.
template<typename Float> Float voltage2hz (Float x);

/// Float precision variant of voltage2hz using fast_exp2().
float fast_voltage2hz (float x);

/// Convert Hertz to synthesizer value (Voltage).
template<typename Float> Float hz2voltage (Float x);

/// Float precision variant of hz2voltage using fast_log2().
float fast_hz2voltage (float x);

/// Determine a significant Decibel change.
template<typename Float> Float db_changed (Float a, Float b);

/// Convert synthesizer value (Voltage) to Decibel.
template<typename Float> Float voltage2db (Float x);

/// Float precision variant of voltage2db using fast_log2().
float fast_voltage2db (float x);

/// Convert Decibel to synthesizer value (Voltage).
template<typename Float> Float db2voltage (Float x);

/// Float precision variant of db2voltage using fast_exp2().
float fast_db2voltage (float x);

/// Determine a significant synthesizer value (Voltage) change.
template<typename Float> Float voltage_changed (Float a, Float b);

// == Implementations ==
static constexpr const long double c3_hertz = 261.6255653005986346778499935233; // 440 * 2^(-9/12)
static constexpr const long double c3_hertz_inv = 0.0038222564329714297410505703465146; // 1 / c3_hertz

template<typename Float> extern inline ASE_CONST Float
voltage2hz (Float x)
{
  return std::exp2 (x * Float (10.0)) * Float (c3_hertz);
}

extern inline ASE_CONST float
fast_voltage2hz (float x)
{
  return fast_exp2 (x * 10.0f) * float (c3_hertz);
}

template<typename Float> extern inline ASE_CONST Float
hz2voltage (Float x)
{
  return std::log2 (x * Float (c3_hertz_inv)) * Float (0.1);
}

extern inline ASE_CONST float
fast_hz2voltage (float x)
{
  return fast_log2 (x * float (c3_hertz_inv)) * 0.1f;
}

template<typename Float> extern inline ASE_CONST Float
voltage2db (Float x)
{
  return std::log10 (std::abs (x)) * Float (20);
}

extern inline ASE_CONST float
fast_voltage2db (float x)
{
  return fast_log2 (__builtin_fabsf (x)) * 6.02059991327962390427477789448986f;
}

template<typename Float> extern inline ASE_CONST Float
db2voltage (Float x)
{
  return std::pow (10, x * Float (0.05));
}

extern inline ASE_CONST float
fast_db2voltage (float x)
{
  return fast_exp2 (x * 0.1660964047443681173935159714744695f);
}

template<typename Float> extern inline ASE_CONST Float
hz_changed (Float a, Float b)
{
  return __builtin_fabsf (a - b) > 1e-3;
}

template<typename Float> extern inline ASE_CONST Float
db_changed (Float a, Float b)
{
  return __builtin_fabsf (a - b) > 1e-3;
}

template<typename Float> extern inline ASE_CONST Float
voltage_changed (Float a, Float b)
{
  return __builtin_fabsf (a - b) > 1e-7;
}

} // Ase

#endif // __ASE_SIGNALUTILS_HH__
