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
  // https://www.musicdsp.org/en/latest/Other/238-rational-tanh-approximation.html
  float
  cheap_tanh (float x)
  {
    x = std::clamp (x, -3.0f, 3.0f);

    return (x * (27.0f + x * x) / (27.0f + 9.0f * x * x));
  }

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
  float current_drive = 0;
  float dest_drive = 0;
  float drive_max_step = 0;
  float current_mix = 1;
  float dest_mix = 1;
  float mix_max_step = 0;

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
  std::unique_ptr<PandaResampler::Resampler2> res_up_left;
  std::unique_ptr<PandaResampler::Resampler2> res_up_right;
  std::unique_ptr<PandaResampler::Resampler2> res_down_left;
  std::unique_ptr<PandaResampler::Resampler2> res_down_right;
public:
  enum class Mode {
    TANH_TABLE,
    TANH_TRUE,
    TANH_CHEAP,
    HARD_CLIP
  };
  Mode mode = Mode::TANH_TABLE;
  SaturationDSP()
  {
    fill_table();

    using PandaResampler::Resampler2;
    res_up_left    = std::make_unique<Resampler2> (Resampler2::UP, oversample, Resampler2::PREC_72DB);
    res_up_right   = std::make_unique<Resampler2> (Resampler2::UP, oversample, Resampler2::PREC_72DB);
    res_down_left  = std::make_unique<Resampler2> (Resampler2::DOWN, oversample, Resampler2::PREC_72DB);
    res_down_right = std::make_unique<Resampler2> (Resampler2::DOWN, oversample, Resampler2::PREC_72DB);
  }
  void
  reset (unsigned int sample_rate)
  {
    mix_max_step = 1 / (0.050 * sample_rate * oversample);    // smooth mix range over 50ms
    drive_max_step = 6 / (0.020 * sample_rate * oversample);  // smooth factor delta of 6dB over 20ms

    res_up_left->reset();
    res_up_right->reset();
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
  set_drive (float d, bool now)
  {
    dest_drive = d;
    if (now)
      current_drive = dest_drive;
  }
  void
  set_mix (float percent, bool now)
  {
    dest_mix = std::clamp (percent * 0.01, 0.0, 1.0);
    if (now)
      current_mix = dest_mix;
  }
  void
  set_mode (Mode new_mode)
  {
    mode = new_mode;
  }
  template<bool STEREO, bool INCREMENT>
  void
  process_sub_block (float *left_over, float *right_over, int n_samples)
  {
    float mix_step = std::clamp ((dest_mix - current_mix) / (n_samples * oversample), -mix_max_step, mix_max_step);
    float drive_step = std::clamp ((dest_drive - current_drive) / (n_samples * oversample), -drive_max_step, drive_max_step);

    float current_factor = exp2f (current_drive / 6);
    current_drive += drive_step * n_samples * oversample;

    float end_factor = exp2f (current_drive / 6);
    float factor_step = (end_factor - current_factor) / (n_samples * oversample);

    if (mode == Mode::TANH_TABLE)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          {
            left_over[i] = lookup_table (left_over[i] * current_factor) * current_mix + left_over[i] * (1 - current_mix);
            if (STEREO)
              right_over[i] = lookup_table (right_over[i] * current_factor) * current_mix + right_over[i] * (1 - current_mix);
            if (INCREMENT)
              {
                current_mix += mix_step;
                current_factor += factor_step;
              }
          }
      }
    if (mode == Mode::TANH_TRUE)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          {
            left_over[i] = std::tanh (left_over[i] * current_factor) * current_mix + left_over[i] * (1 - current_mix);
            if (STEREO)
              right_over[i] = std::tanh (right_over[i] * current_factor) * current_mix + right_over[i] * (1 - current_mix);
            if (INCREMENT)
              {
                current_mix += mix_step;
                current_factor += factor_step;
              }
          }
      }
    if (mode == Mode::TANH_CHEAP)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          {
            left_over[i] = cheap_tanh (left_over[i] * current_factor) * current_mix + left_over[i] * (1 - current_mix);
            if (STEREO)
              right_over[i] = cheap_tanh (right_over[i] * current_factor) * current_mix + right_over[i] * (1 - current_mix);
            if (INCREMENT)
              {
                current_mix += mix_step;
                current_factor += factor_step;
              }
          }
      }
    if (mode == Mode::HARD_CLIP)
      {
        for (int i = 0; i < n_samples * oversample; i++)
          {
            left_over[i] = std::clamp (left_over[i] * current_factor, -1.f, 1.f) * current_mix + left_over[i] * (1 - current_mix);
            if (STEREO)
              right_over[i] = std::clamp (right_over[i] * current_factor, -1.f, 1.f) * current_mix + right_over[i] * (1 - current_mix);
            if (INCREMENT)
              {
                current_mix += mix_step;
                current_factor += factor_step;
              }
          }
      }
  }
  template<bool STEREO>
  void
  process (float *left_in, float *right_in, float *left_out, float *right_out, int n_samples)
  {
    float left_over[oversample * n_samples];
    float right_over[oversample * n_samples];

    res_up_left->process_block (left_in, n_samples, left_over);
    if (STEREO)
      res_up_right->process_block (right_in, n_samples, right_over);

    int pos = 0;
    while (pos < n_samples)
      {
        if (std::abs (dest_drive - current_drive) > 0.001 || std::abs (dest_mix - current_mix) > 0.001)
          {
            // SLOW: drive or mix change within the block
            int todo = std::min (n_samples - pos, 64);
            process_sub_block<STEREO, true> (left_over + pos * oversample, right_over + pos * oversample, todo);
            pos += todo;
          }
        else
          {
            // FAST: drive and mix remain constant during the block
            process_sub_block<STEREO, false> (left_over + pos * oversample, right_over + pos * oversample, n_samples - pos);
            pos = n_samples;
          }
      }

    res_down_left->process_block (left_over, oversample * n_samples, left_out);
    if (STEREO)
      res_down_right->process_block (right_over, oversample * n_samples, right_out);
  }
};


}
