// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/randomhash.hh"
#include "ase/internal.hh"

namespace {
using namespace Ase;

/// Pseudo random number generator optimized for white noise.
struct WhiteRand {
  uint64_t rmix_, next_;
  WhiteRand (uint64_t seed1 = random_int64(), uint64_t seed2 = random_int64()) :
    rmix_ (seed1 | 1), next_ (seed2 ? seed2 : 0x14057b7ef767814fU)
  {
    rand64(); // avoid seed2 handout
    rand64(); // avoid seed1 * M handout
    rand64(); // helps with bad seeds (e.g. 31-bit seeds from rand())
  }
  inline uint64_t
  rand64()
  {
    constexpr uint64_t M = 0xd3833e804f4c574bU;
    const uint64_t last = next_;
    next_ = rmix_ * M;
    rmix_ = rotr (rmix_ - last, 37);
    return last;
  }
  inline std::tuple<float,float>
  randf2()
  {
    const uint64_t r = rand64();
    const uint32_t h = r >> 32, l = r;
    constexpr float i2f = 4.65661287416159475086293992373695129617e-10; // 1.0 / 2147483647.5;
    return { int32_t (h) * i2f, int32_t (l) * i2f };
  }
};

/// IIR evaluation in transposed direct form II from `b[0…N], w[0…N-1], a[1…N]`
/// Where `b[N+1]` are the feed-forward coefficients, `a[N+1]` are the feed-back
/// coefficients, `w[N]` stores the z^-1 memory, and `x` is the current input.
template<size_t N, class D, class F> extern inline F
iir_eval_tdf2 (const D *b, const D *a, D *w, F x)
{ // https://en.wikipedia.org/wiki/Digital_biquad_filter#Transposed_Direct_form_2
  static_assert (N >= 1); // assumes a[0] == 1.0
  const D y = x * b[0] + w[0];
  D v = x * b[N] - y * a[N];
  size_t n = N;
  while (--n)
    {
      const D t = w[n];
      w[n] = v;
      v = x * b[n] + t - y * a[n];
    }
  w[0] = v;
  return y;
}

/// Pink noise filter implemented as IIR filter with order 3
template<class Float>
struct PinkFilter {
  // https://ccrma.stanford.edu/~jos/sasp/Example_Synthesis_1_F_Noise.html
  static constexpr size_t N = 3;
  static constexpr Float  B[N + 1] = { 0.049922035, -0.095993537, 0.050612699, -0.004408786 };
  static constexpr Float  A[N + 1] = { 1,           -2.494956002, 2.017265875, -0.522189400 };
  float  delays_[N] = { 0, };
  void   reset  ()              { floatfill (delays_, 0.0, N); }
  Float  eval   (Float x)       { return iir_eval_tdf2<N> (B, A, delays_, x); }
};

template<class F> static inline F
db2amp (F dB)
{
  constexpr F db2log2 = 0.166096404744368117393515971474469508793; // log2 (10) / 20
  return std::exp2 (dB * db2log2);
}

/// Noise generator for various colors of noise.
class ColoredNoise : public AudioProcessor {
  using Pink = PinkFilter<float>;
  OBusId    stereout_;
  WhiteRand white_rand_;
  Pink      pink0, pink1;
  float     gain_factor_ = 1.0;
  bool      mono_ = true;
  bool      pink_ = false;
  enum Params { GAIN = 1, MONO, PINK };
public:
  void
  query_info (AudioProcessorInfo &info) const override
  {
    info.uri          = "Anklang.Devices.ColoredNoise";
    info.version      = "1";
    info.label        = "Pink & White Noise";
    info.category     = "Generators";
    info.creator_name = "Tim Janik";
    info.website_url  = "https://anklang.testbit.eu";
  }
  void
  initialize (SpeakerArrangement busses) override
  {
    start_group ("Noise Settings");
    add_param (GAIN, "Gain",  "Gain", -96, 24, 0, "dB");
    add_param (MONO, "Mono",  "Monophonic", true);
    add_param (PINK, "Pink", "Pink Noise", false);
    remove_all_buses();
    stereout_ = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);
  }
  void
  adjust_param (Id32 tag) override
  {
    switch (Params (tag.id))
      {
      case GAIN:        gain_factor_ = db2amp (get_param (tag)); break;
      case MONO:        mono_ = get_param (tag);        break;
      case PINK:        pink_ = get_param (tag);        break;
      }
  }
  void
  reset() override
  {
    pink0.reset();
    pink1.reset();
    adjust_params (true);
  }
  void render (uint n_frames) override;
  // optimize rendering variants via template argument
  enum Cases { INSTEREO = 1, WITHPINK = 2, WITHGAIN = 4, MASK = 0x7 };
  template<Cases> void render_cases (float *out0, float *out1, uint n_frames, const float gain);
  using RenderF = void (ColoredNoise::*) (float*, float*, uint, float);
};

static auto noise_module = register_audio_processor<ColoredNoise>();

template<ColoredNoise::Cases CASES> void
ColoredNoise::render_cases (float *out0, float *out1, uint n_frames, const float gain)
{
  static_assert (CASES <= MASK);
  for (size_t i = 0; i < n_frames; i++)
    {
      auto [f0, f1] = white_rand_.randf2();
      if_constexpr (bool (CASES & WITHPINK) && bool (CASES & INSTEREO))
        {
          f0 = pink0.eval (f0); // left filter
          f1 = pink1.eval (f1); // right filter
        }
      if_constexpr (bool (CASES & WITHPINK) && !bool (CASES & INSTEREO))
        {
          f0 = pink0.eval (f0); // mono filter
          f1 = pink0.eval (f1); // mono filter
        }
      if_constexpr (bool (CASES & WITHGAIN))
        {
          f0 *= gain;
          f1 *= gain;
        }
      if_constexpr (bool (CASES & INSTEREO))
        {
          out0[i] = f0;
          out1[i] = f1;
        }
      else
        {
          out0[i] = f0;
          out1[i] = f0;
          i++;
          out0[i] = f1;
          out1[i] = f1;
        }
    }
}

// construct table *outside* of time critical render() method
static const auto render_table = make_case_table<ColoredNoise::MASK> ([] (auto CASE)
 { // decltype (CASE) is std::integral_constant<unsigned long, N...>
   return &ColoredNoise::render_cases<ColoredNoise::Cases (CASE.value)>;
 });

void
ColoredNoise::render (uint n_frames)
{
  adjust_params (false);
  float *out0 = oblock (stereout_, 0);
  float *out1 = oblock (stereout_, 1);
  const float gain = gain_factor_;
  const uint index = INSTEREO * !mono_ + WITHPINK * pink_ + WITHGAIN * (gain != 1.0);
  (this->*render_table[index]) (out0, out1, n_frames, gain);
}

} // Anon
