// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DATAUTILS_HH__
#define __ASE_DATAUTILS_HH__

#include <ase/signalmath.hh>
#include <cstring>

namespace Ase {

/// Maximum number of values in the const_float_zeros block.
constexpr const uint AUDIO_BLOCK_FLOAT_ZEROS_SIZE = 16384;

/// Block of const floats allof value 0.
extern float const_float_zeros[AUDIO_BLOCK_FLOAT_ZEROS_SIZE];

/// Calculate suqare sum of a block of floats.
float square_sum (uint n_values, const float *ivalues);

/// Find the maximum suqared value in a block of floats.
float square_max (uint n_values, const float *ivalues);

// Convert integer to float samples.
template<class S, class D> inline void convert_samples (size_t n, S *src, D *dst, uint16 byte_order);

// Convert float to integer samples with clipping.
template<class S, class D> inline void convert_clip_samples (size_t n, S *src, D *dst, uint16 byte_order);

/// Fill `n` values of `dst` with `f`.
extern inline void
floatfill (float *dst, float f, size_t n)
{
  for (size_t i = 0; i < n; i++)
    dst[i] = f;
}

/// Copy a block of floats
extern inline void
fast_copy (size_t n, float *d, const float *s)
{
  static_assert (sizeof (float) == 4, "");
  static_assert (sizeof (wchar_t) == 4, "");
  wmemcpy ((wchar_t*) d, (const wchar_t*) s, n);
}

/// Copy a block of integers
extern inline void
fast_copy (size_t n, uint32_t *d, const uint32_t *s)
{
  static_assert (sizeof (uint32_t) == 4, "");
  static_assert (sizeof (wchar_t) == 4, "");
  wmemcpy ((wchar_t*) d, (const wchar_t*) s, n);
}

/// Copy a block of unsigned chars
extern inline void
fast_copy (size_t n, uint8_t *d, const uint8_t *s)
{
  memcpy (d, s, n);
}

// == Implementations ==
template<> inline void
convert_samples (size_t n, const int16_t *src, float *dst, uint16 byte_order)
{
  ASE_ASSERT_RETURN (__BYTE_ORDER__ == byte_order); // swapping __BYTE_ORDER__ not implemented
  const auto bound = dst + n;
  while (dst < bound)
    *dst++ = *src++ * (1. / 32768.);
}

template<> inline void
convert_clip_samples (size_t n, const float *src, int16_t *dst, uint16 byte_order)
{
  ASE_ASSERT_RETURN (__BYTE_ORDER__ == byte_order); // swapping __BYTE_ORDER__ not implemented
  static_assert (int16_t (0.99999990F * 32768.F) == 32767);
  const auto bound = src + n;
  while (src < bound)
    {
      auto v = *src++;
      v = std::min (0.99999990F, std::max (v, -1.0F));
      *dst++ = v * 32768.;
    }
}

} // Ase

#endif // __ASE_DATAUTILS_HH__
