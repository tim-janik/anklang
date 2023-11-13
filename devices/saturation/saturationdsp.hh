// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <array>
#include <cmath>
#include <cstdio>
#include <algorithm>

#define PANDA_RESAMPLER_HEADER_ONLY

#include "pandaresampler.hh"

namespace Ase {

class SaturationDSP
{
  /*
   * tanh function which is restricted in the range [-4:4]
   */
  double
  tanh_restricted (double x)
  {
    double sign = 1;
    if (x < 0)
      {
        x = -x;
        sign = -1;
      }
    if (x < 3)
      return sign * tanh (x);
    if (x > 4)
      return sign;

    // use polynomial:
    //  at (+/-)3 : match function value
    //  at (+/-)3 : identical first derivative
    //  at (+/-)4 : function value is 1.0
    //  at (+/-)4 : derivative is 0
    double th3 = tanh (3);
    double delta = 1 - th3;
    double deriv = 1 - th3 * th3;
    double b = 3 * delta - deriv;
    double a = delta - b;
    double x4 = 4 - x;
    return sign * (1 - (a * x4 + b) * x4 * x4);
  }

  static constexpr int table_size = 512;
  static constexpr int oversample = 8;
  std::array<float, table_size> table;
  float factor = 1;
  float wet_factor = 1;
  float dry_factor = 0;

  void
  fill_table()
  {
   for (size_t x = 0; x < table_size - 2; x++)
      {
        double d = (x / double (table_size - 3)) * 8 - 4;
        table[x + 1] = tanh_restricted (d);
      }
    table[0] = table[1];
    table[table_size - 1] = table[table_size - 2];
  }
  std::unique_ptr<PandaResampler::Resampler2> res_up;
  std::unique_ptr<PandaResampler::Resampler2> res_down;
public:
  enum class Mode {
    TANH_TABLE,
    TANH_TRUE,
    SIN_TRUE,
    HARD_CLIP
  };
  Mode mode = Mode::TANH_TABLE;
  SaturationDSP()
  {
    fill_table();

    using PandaResampler::Resampler2;
    res_up = std::make_unique<Resampler2> (Resampler2::UP, oversample, Resampler2::PREC_72DB);
    res_down = std::make_unique<Resampler2> (Resampler2::DOWN, oversample, Resampler2::PREC_72DB);
  }
  float
  lookup_table (float f)
  {
    float tbl_index = std::clamp ((f + 4) / 8 * (table_size - 3) + 1, 0.5f, table_size - 1.5f);
    int   itbl_index = tbl_index;
    float frac = tbl_index - itbl_index;
    return table[itbl_index] + frac * (table[itbl_index + 1] - table[itbl_index]);
  }
  void
  set_factor (float f)
  {
    factor = f;
  }
  void
  set_mix (float percent)
  {
    wet_factor = std::clamp (percent * 0.01, 0.0, 1.0);
    dry_factor = std::clamp (1 - percent * 0.01, 0.0, 1.0);
  }
  void
  set_mode (Mode new_mode)
  {
    mode = new_mode;
  }
  void
  process (float *input, float *output, int n_samples)
  {
    float over_samples[oversample * n_samples];
    res_up->process_block (input, n_samples, over_samples);

    if (mode == Mode::TANH_TABLE)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          over_samples[i] = lookup_table (over_samples[i] * factor) * wet_factor + over_samples[i] * dry_factor;
      }
    if (mode == Mode::TANH_TRUE)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          over_samples[i] = tanh (over_samples[i] * factor) * wet_factor + over_samples[i] * dry_factor;
      }
    if (mode == Mode::SIN_TRUE)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          over_samples[i] = sin (over_samples[i] * factor) * wet_factor + over_samples[i] * dry_factor;
      }
    if (mode == Mode::HARD_CLIP)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          over_samples[i] = std::clamp (over_samples[i] * factor, -1.f, 1.f) * wet_factor + over_samples[i] * dry_factor;
      }

    res_down->process_block (over_samples, oversample * n_samples, output);
  }
};


}
