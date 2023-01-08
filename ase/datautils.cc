// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "datautils.hh"
#include "internal.hh"

namespace Ase {

float const_float_zeros[AUDIO_BLOCK_FLOAT_ZEROS_SIZE] = { 0, /*...*/ };

float
square_sum (uint n_values, const float *ivalues)
{
  float accu = 0.0;
  for (uint i = 0; i < n_values; i++)
    accu += ivalues[i] * ivalues[i];
  return accu;
}

float
square_max (uint n_values, const float *ivalues)
{
  float accu = 0.0;
  for (uint i = 0; i < n_values; i++)
    accu = std::max (accu, ivalues[i] * ivalues[i]);
  return accu;
}

} // Ase
