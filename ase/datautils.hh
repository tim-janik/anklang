// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DATAUTILS_HH__
#define __ASE_DATAUTILS_HH__

#include <ase/cxxaux.hh>

namespace Ase {

/// Double round-off error at 1.0, equals 2^-53
constexpr const double DOUBLE_EPSILON = 1.1102230246251565404236316680908203125e-16;

/// Maximum number of sample frames to calculate in Processor::render().
constexpr const uint AUDIO_BLOCK_MAX_RENDER_SIZE = 128;

/// Maximum number of values in the const_float_zeros block.
constexpr const uint AUDIO_BLOCK_FLOAT_ZEROS_SIZE = 16384;

/// Block of const floats allof value 0.
extern float const_float_zeros[AUDIO_BLOCK_FLOAT_ZEROS_SIZE];

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
