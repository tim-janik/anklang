// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/midievent.hh"
#include "devices/blepsynth/bleposc.hh"
#include "devices/blepsynth/laddervcf.hh"
#include "devices/blepsynth/skfilter.hh"
#include "devices/blepsynth/linearsmooth.hh"
#include "ase/internal.hh"

// based on liquidsfz envelope.hh

namespace {

using namespace Ase;

class FlexADSR
{
public:
  enum class Shape { FLEXIBLE, EXPONENTIAL, LINEAR };
private:
  float attack_ = 0;
  float attack_slope_ = 0;
  float decay_ = 0;
  float decay_slope_ = 0;
  float sustain_level_ = 0;
  float release_ = 0;
  float release_slope_ = 0;
  float level_ = 0;
  float release_start_ = 0;  /* initial level of release stage */
  int   sustain_steps_ = 0;  /* sustain smoothing */
  bool  params_changed_ = true;
  int   rate_ = 48000;

  enum class State { ATTACK, DECAY, SUSTAIN, RELEASE, DONE };

  State state_ = State::DONE;
  Shape shape_ = Shape::LINEAR;

  float a_ = 0;
  float b_ = 0;
  float c_ = 0;

  void
  init_abc (float time_s, float slope)
  {
    bool positive = slope > 0;
    slope = std::abs (slope);

    const float t1y = 0.5f + 0.25f * slope;

    a_ = slope * ( 1.0135809670870777f + slope * (-1.2970447050283254f + slope *   7.2390617313972063f));
    b_ = slope * (-5.8998946320566281f + slope * ( 5.7282487210570903f + slope * -15.525953208626062f));
    c_ = 1 - (t1y * a_ + b_) * t1y;

    if (!positive)
      {
        c_ += a_ + b_;
        b_ = -2 * a_ - b_;
      }

    const float time_factor = 1 / (rate_ * time_s);
    a_ *= time_factor;
    b_ *= time_factor;
    c_ *= time_factor;

    /* abc so far is for:
     *
     *   y += a * y * y + b * y + c
     *
     * now to save one addition later on, we add one to b, and update y using
     *
     *   y = a * y * y + b * y + c
     */
    b_ += 1;
  }

  void
  compute_slope_params (float seconds, float start_x, float end_x)
  {
    if (!params_changed_)
      return;

    int steps = std::max<int> (seconds * rate_, 1);

    if (shape_ == Shape::LINEAR)
      {
        // linear
        a_ = 0;
        b_ = 1;
        c_ = (end_x - start_x) / steps;
      }
    else if (shape_ == Shape::EXPONENTIAL)
      {
        /* exponential: true exponential decay doesn't ever reach zero;
         * therefore we need to fade out early
         */
        const double RATIO = (state_ == State::ATTACK) ? 0.2 : 0.001;

        const double f = -log ((RATIO + 1) / RATIO) / steps;
        double factor = exp (f);
        c_ = (end_x - RATIO * (start_x - end_x)) * (1 - factor);
        b_ = factor;
        a_ = 0;
      }
    else if (shape_ == Shape::FLEXIBLE)
      {
        auto pos_time = [] (auto x) { return std::max (x, 0.0001f); /* 0.1ms */ };
        if (state_ == State::ATTACK)
          {
            init_abc (pos_time (attack_), attack_slope_);
          }
        else if (state_ == State::DECAY)
          {
            /* exact timing for linear decay slope */
            float stretch = 1 / std::max (1 - sustain_level_, 0.01f);
            init_abc (-pos_time (decay_ * stretch), decay_slope_);
          }
        else if (state_ == State::RELEASE)
          {
            init_abc (-pos_time (release_), release_slope_);

            /* stretch abc parameters to match release time */
            float l = std::max (release_start_, 0.01f);
            a_ /= l;
            c_ *= l;
          }
      }
    params_changed_ = false;
  }

public:
  void
  set_shape (Shape shape)
  {
    shape_ = shape;
    params_changed_ = true;
  }
  void
  set_attack (float f)
  {
    attack_ = f;
    params_changed_ = true;
  }
  void
  set_attack_slope (float f)
  {
    attack_slope_ = f;
    params_changed_ = true;
  }
  void
  set_decay (float f)
  {
    decay_ = f;
    params_changed_ = true;
  }
  void
  set_decay_slope (float f)
  {
    decay_slope_ = f;
    params_changed_ = true;
  }
  void
  set_sustain (float f)
  {
    sustain_level_ = f * 0.01f;
    params_changed_ = true;
  }
  void
  set_release (float f)
  {
    release_ = f;
    params_changed_ = true;
  }
  void
  set_release_slope (float f)
  {
    release_slope_ = f;
    params_changed_ = true;
  }
  void
  set_rate (int sample_rate)
  {
    rate_ = sample_rate;
    params_changed_ = true;
  }
  void
  start ()
  {
    level_          = 0;
    state_          = State::ATTACK;
    params_changed_ = true;
  }
  void
  stop()
  {
    state_          = State::RELEASE;
    release_start_  = level_;
    params_changed_ = true;
  }
private:
  template<State STATE, Shape SHAPE>
  void
  process (uint *iptr, float *samples, uint n_samples)
  {
    uint i = *iptr;

    const float a = a_;
    const float b = b_;
    const float c = c_;
    const float sustain_level = sustain_level_;

    float level = level_;

    while (i < n_samples)
      {
        samples[i++] = level;

        if (SHAPE == Shape::FLEXIBLE)
          level = (a * level + b) * level + c;

        if (SHAPE == Shape::EXPONENTIAL)
          level = b * level + c;

        if (SHAPE == Shape::LINEAR)
          level += c;

        if (STATE == State::ATTACK && level > 1)
          {
            level           = 1;
            state_          = State::DECAY;
            params_changed_ = true;
            break;
          }
        if (STATE == State::DECAY && level < sustain_level)
          {
            state_          = State::SUSTAIN;
            level           = sustain_level;
            params_changed_ = true;
            break;
          }
        if (STATE == State::RELEASE && level < 1e-5f)
          {
            state_ = State::DONE;
            level = 0;
            break;
          }
      }
    level_ = level;

    *iptr = i;
  }
  template<State STATE>
  void
  process (uint *iptr, float *samples, uint n_samples)
  {
    if (shape_ == Shape::LINEAR)
      process<STATE, Shape::LINEAR> (iptr, samples, n_samples);

    if (shape_ == Shape::EXPONENTIAL)
      process<STATE, Shape::EXPONENTIAL> (iptr, samples, n_samples);

    if (shape_ == Shape::FLEXIBLE)
      process<STATE, Shape::FLEXIBLE> (iptr, samples, n_samples);
  }
public:
  void
  process (float *samples, uint n_samples)
  {
    uint i = 0;
    if (state_ == State::ATTACK)
      {
        compute_slope_params (attack_, 0, 1);
        process<State::ATTACK> (&i, samples, n_samples);
      }
    if (state_ == State::DECAY)
      {
        compute_slope_params (decay_, 1, sustain_level_);
        process<State::DECAY> (&i, samples, n_samples);
      }
    if (state_ == State::RELEASE)
      {
        compute_slope_params (release_, release_start_, 0);
        process<State::RELEASE> (&i, samples, n_samples);
      }
    if (state_ == State::SUSTAIN)
      {
        if (params_changed_)
          {
            if (std::abs (sustain_level_ - level_) > 1e-5)
              {
                sustain_steps_ = std::max<int> (0.020f * rate_, 1);
                c_ = (sustain_level_ - level_) / sustain_steps_;
              }
            else
              {
                sustain_steps_ = 0;
              }
            params_changed_ = false;
          }
        while (sustain_steps_ && i < n_samples) /* sustain smoothing */
          {
            samples[i++] = level_;
            level_ += c_;
            sustain_steps_--;
            if (sustain_steps_ == 0)
              level_ = sustain_level_;
          }
        while (i < n_samples)
          samples[i++] = level_;
      }
    if (state_ == State::DONE)
      {
        while (i < n_samples)
          samples[i++] = 0;
      }
  }
  bool
  is_constant() const
  {
    if (state_ == State::SUSTAIN)
      {
        return !params_changed_ && sustain_steps_ == 0;
      }
    return state_ == State::DONE;
  }
  bool
  done() const
  {
    return state_ == State::DONE;
  }
};

// == BlepSynth ==
// subtractive synth based on band limited steps (MinBLEP):
// - aliasing-free square/saw and similar sounds including hard sync
class BlepSynth : public AudioProcessor {
  OBusId stereout_;
  bool   old_c_, old_d_, old_e_, old_f_, old_g_;

  enum ParamType : uint32_t {
    OSC1_SHAPE = 1, OSC1_PULSE_WIDTH, OSC1_SUB, OSC1_SUB_WIDTH, OSC1_SYNC, OSC1_PITCH, OSC1_OCTAVE, OSC1_UNISON_VOICES, OSC1_UNISON_DETUNE, OSC1_UNISON_STEREO,
    OSC2_SHAPE,     OSC2_PULSE_WIDTH, OSC2_SUB, OSC2_SUB_WIDTH, OSC2_SYNC, OSC2_PITCH, OSC2_OCTAVE, OSC2_UNISON_VOICES, OSC2_UNISON_DETUNE, OSC2_UNISON_STEREO,
    VE_MODEL, ATTACK, DECAY, SUSTAIN, RELEASE, ATTACK_SLOPE, DECAY_SLOPE, RELEASE_SLOPE,
    CUTOFF, RESONANCE, DRIVE, KEY_TRACK, FILTER_TYPE, LADDER_MODE, SKFILTER_MODE,
    FIL_ATTACK, FIL_DECAY, FIL_SUSTAIN, FIL_RELEASE, FIL_CUT_MOD,
    MIX, VEL_TRACK, POST_GAIN,
    KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,
  };

  enum { FILTER_TYPE_BYPASS, FILTER_TYPE_LADDER, FILTER_TYPE_SKFILTER };
  int filter_type_ = 0;

  static constexpr int CUTOFF_MIN_MIDI = 15;
  static constexpr int CUTOFF_MAX_MIDI = 144;

  class Voice
  {
  public:
    enum State {
      IDLE,
      ON,
      RELEASE
      // TODO: SUSTAIN / pedal
    };
    // TODO : enum class MonoType

    FlexADSR     envelope_;
    FlexADSR     fil_envelope_;
    State        state_       = IDLE;
    int          midi_note_   = -1;
    int          channel_     = 0;
    double       freq_        = 0;
    float        vel_gain_    = 0;
    bool         new_voice_   = false;

    LinearSmooth cutoff_smooth_;
    double       last_cutoff_;
    double       last_key_track_;

    LinearSmooth cut_mod_smooth_;
    double       last_cut_mod_;

    LinearSmooth reso_smooth_;
    double       last_reso_;

    LinearSmooth drive_smooth_;
    double       last_drive_;

    BlepUtils::OscImpl osc1_;
    BlepUtils::OscImpl osc2_;

    static constexpr int FILTER_OVERSAMPLE = 4;

    LadderVCF ladder_filter_ { FILTER_OVERSAMPLE };
    SKFilter  skfilter_ { FILTER_OVERSAMPLE };
  };
  std::vector<Voice>    voices_;
  std::vector<Voice *>  active_voices_;
  std::vector<Voice *>  idle_voices_;
  void
  initialize (SpeakerArrangement busses) override
  {
    using namespace MakeIcon;
    set_max_voices (32);

    ParameterMap pmap;

    auto oscparams = [&] (int oscnum) {
      const uint I = oscnum + 1;
      const uint O = oscnum * (OSC2_SHAPE - OSC1_SHAPE);
      const String o = string_format ("osc_%u_", I);

      const double shape_default  = oscnum ? -100 : 0;
      const double octave_default = oscnum;

      pmap.group = _("Oscillator %u", I);
      pmap[O+OSC1_SHAPE]       = Param { o+"shape",       _("Osc %u Shape", I),             _("Shp%u", I),  shape_default, "%", { -100, 100, }, };
      pmap[O+OSC1_PULSE_WIDTH] = Param { o+"pulse_width", _("Osc %u Pulse Width", I),       _("PW%u", I),  50, "%", { 0, 100, }, };
      pmap[O+OSC1_SUB]         = Param { o+"subharmonic", _("Osc %u Subharmonic", I),       _("Sub%u", I),  0, "%", { 0, 100, }, };
      pmap[O+OSC1_SUB_WIDTH]   = Param { o+"subharmonic_width", _("Osc %u Subharmonic Width", I), _("SbW%u", I), 50, "%", { 0, 100, }, };
      pmap[O+OSC1_SYNC]        = Param { o+"sync_slave",  _("Osc %u Sync Slave", I),        _("Syn%u", I),  0, "Semitones", { 0, 60, }, };

      pmap[O+OSC1_PITCH]  = Param { o+"pitch",  _("Osc %u Pitch", I),  _("Pit%u", I), 0,              "semitones", { -7, 7, }, };
      pmap[O+OSC1_OCTAVE] = Param { o+"octave", _("Osc %u Octave", I), _("Oct%u", I), octave_default, "octaves",   { -2, 3, }, };

      /* TODO: unison_voices property should have stepping set to 1 */
      pmap[O+OSC1_UNISON_VOICES] = Param { o+"unison_voices", _("Osc %u Unison Voices", I), _("Voi%u", I), 1, "Voices", { 1, 16, }, };
      pmap[O+OSC1_UNISON_DETUNE] = Param { o+"unison_detune", _("Osc %u Unison Detune", I), _("Dtu%u", I), 6, "%", { 0.5, 50, }, };
      pmap[O+OSC1_UNISON_STEREO] = Param { o+"unison_stereo", _("Osc %u Unison Stereo", I), _("Ste%u", I), 0, "%", { 0, 100, }, };
    };

    oscparams (0);

    pmap.group = _("Mix");
    pmap[MIX]       = Param { "mix", _("Mix"), _("Mix"), 30, "%", { 0, 100 }, };
    pmap[VEL_TRACK] = Param { "vel_track", _("Velocity Tracking"), _("VelTr"), 50, "%", { 0, 100, }, };
    // TODO: post_gain probably should default to 0dB once we have track/mixer volumes
    pmap[POST_GAIN] = Param { "post_gain", _("Post Gain"), _("Gain"), -12, "dB", { -24, 24, }, };

    oscparams (1);

    pmap.group = _("Volume Envelope");
    ChoiceS ve_model_cs;
    ve_model_cs += { "A", "Analog" };
    ve_model_cs += { "F", "Flexible" };
    pmap[VE_MODEL] = Param { "ve_model", _("Envelope Model"), _("Model"), 0, "", std::move (ve_model_cs), "", { String ("blurb=") + _("ADSR Model to be used"), } };

    pmap[ATTACK]  = Param { "attack",  _("Attack"),  _("A"), 20.0, "%", { 0, 100, }, };
    pmap[DECAY]   = Param { "decay",   _("Decay"),   _("D"), 30.0, "%", { 0, 100, }, };
    pmap[SUSTAIN] = Param { "sustain", _("Sustain"), _("S"), 50.0, "%", { 0, 100, }, };
    pmap[RELEASE] = Param { "release", _("Release"), _("R"), 30.0, "%", { 0, 100, }, };

    pmap[ATTACK_SLOPE]  = Param { "attack_slope", _("Attack Slope"), _("AS"), 50, "%", { -100, 100, }, };
    pmap[DECAY_SLOPE]   = Param { "decay_slope", _("Decay Slope"), _("DS"), -100, "%", { -100, 100, }, };
    pmap[RELEASE_SLOPE] = Param { "release_slope", _("Release Slope"), _("RS"), -100, "%", { -100, 100, }, };

    pmap.group = _("Filter");

    pmap[CUTOFF]    = Param { "cutoff", _("Cutoff"), _("Cutoff"), 60, "", { CUTOFF_MIN_MIDI, CUTOFF_MAX_MIDI, }, }; // cutoff as midi notes
    pmap[RESONANCE] = Param { "resonance", _("Resonance"), _("Reso"), 25.0, "%", { 0, 100, }, };
    pmap[DRIVE]     = Param { "drive", _("Drive"), _("Drive"), 0, "dB", { -24, 36, }, };
    pmap[KEY_TRACK] = Param { "key_tracking", _("Key Tracking"), _("KeyTr"), 50, "%", { 0, 100, }, };
    ChoiceS filter_type_choices;
    filter_type_choices += { "—"_uc, "Bypass Filter" };
    filter_type_choices += { "LD"_uc, "Ladder Filter" };
    filter_type_choices += { "SKF"_uc, "Sallen-Key Filter" };
    pmap[FILTER_TYPE] = Param { "filter_type", _("Filter Type"), _("Type"), FILTER_TYPE_LADDER, "", std::move (filter_type_choices), "", { String ("blurb=") + _("Filter Type to be used"), } };

    ChoiceS ladder_mode_choices;
    ladder_mode_choices += { "LP1"_uc, "1 Pole Lowpass, 6dB/Octave" };
    ladder_mode_choices += { "LP2"_uc, "2 Pole Lowpass, 12dB/Octave" };
    ladder_mode_choices += { "LP3"_uc, "3 Pole Lowpass, 18dB/Octave" };
    ladder_mode_choices += { "LP4"_uc, "4 Pole Lowpass, 24dB/Octave" };
    pmap[LADDER_MODE] = Param { "ladder_mode", _("Filter Mode"), _("Mode"), 1, "", std::move (ladder_mode_choices), "", { String ("blurb=") + _("Ladder Filter Mode to be used"), } };

    ChoiceS skfilter_mode_choices;
    skfilter_mode_choices += { "LP1"_uc, "1 Pole Lowpass, 6dB/Octave" };
    skfilter_mode_choices += { "LP2"_uc, "2 Pole Lowpass, 12dB/Octave" };
    skfilter_mode_choices += { "LP3"_uc, "3 Pole Lowpass, 18dB/Octave" };
    skfilter_mode_choices += { "LP4"_uc, "4 Pole Lowpass, 24dB/Octave" };
    skfilter_mode_choices += { "LP6"_uc, "6 Pole Lowpass, 36dB/Octave" };
    skfilter_mode_choices += { "LP8"_uc, "8 Pole Lowpass, 48dB/Octave" };
    skfilter_mode_choices += { "BP2"_uc, "2 Pole Bandpass, 6dB/Octave" };
    skfilter_mode_choices += { "BP4"_uc, "4 Pole Bandpass, 12dB/Octave" };
    skfilter_mode_choices += { "BP6"_uc, "6 Pole Bandpass, 18dB/Octave" };
    skfilter_mode_choices += { "BP8"_uc, "8 Pole Bandpass, 24dB/Octave" };
    skfilter_mode_choices += { "HP1"_uc, "1 Pole Highpass, 6dB/Octave" };
    skfilter_mode_choices += { "HP2"_uc, "2 Pole Highpass, 12dB/Octave" };
    skfilter_mode_choices += { "HP3"_uc, "3 Pole Highpass, 18dB/Octave" };
    skfilter_mode_choices += { "HP4"_uc, "4 Pole Highpass, 24dB/Octave" };
    skfilter_mode_choices += { "HP6"_uc, "6 Pole Highpass, 36dB/Octave" };
    skfilter_mode_choices += { "HP8"_uc, "8 Pole Highpass, 48dB/Octave" };
    pmap[SKFILTER_MODE] = Param { "skfilter_mode", _("SKFilter Mode"), _("Mode"), 2, "", std::move (skfilter_mode_choices), "", { String ("blurb=") + _("Sallen-Key Filter Mode to be used"), } };

    pmap.group = _("Filter Envelope");
    pmap[FIL_ATTACK]  = Param { "fil_attack", _("Attack"), _("A"), 40, "%", { 0, 100, }, };
    pmap[FIL_DECAY]   = Param { "fil_decay", _("Decay"), _("D"), 55, "%", { 0, 100, }, };
    pmap[FIL_SUSTAIN] = Param { "fil_sustain", _("Sustain"), _("S"), 30, "%", { 0, 100, }, };
    pmap[FIL_RELEASE] = Param { "fil_release", _("Release"), _("R"), 30, "%", { 0, 100, }, };
    pmap[FIL_CUT_MOD] = Param { "fil_cut_mod", _("Env Cutoff Modulation"), _("CutMod"), 36, "semitones", { -96, 96, }, }; /* 8 octaves range */

    pmap.group = _("Keyboard Input");
    pmap[KEY_C] = Param { "c", _("Main Input 1"), _("C"), false, "", {}, GUIONLY + ":toggle" };
    pmap[KEY_D] = Param { "d", _("Main Input 2"), _("D"), false, "", {}, GUIONLY + ":toggle" };
    pmap[KEY_E] = Param { "e", _("Main Input 3"), _("E"), false, "", {}, GUIONLY + ":toggle" };
    pmap[KEY_F] = Param { "f", _("Main Input 4"), _("F"), false, "", {}, GUIONLY + ":toggle" };
    pmap[KEY_G] = Param { "g", _("Main Input 5"), _("G"), false, "", {}, GUIONLY + ":toggle" };
    old_c_ = old_d_ = old_e_ = old_f_ = old_g_ = false;

    install_params (pmap);

    prepare_event_input();
    stereout_ = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);
    assert_return (bus_info (stereout_).ident == "stereo_out");
  }
  void
  set_max_voices (uint n_voices)
  {
    voices_.clear();
    voices_.resize (n_voices);

    active_voices_.clear();
    active_voices_.reserve (n_voices);

    idle_voices_.clear();
    for (auto& v : voices_)
      idle_voices_.push_back (&v);
  }
  Voice *
  alloc_voice()
  {
    if (idle_voices_.empty()) // out of voices?
      return nullptr;

    Voice *voice = idle_voices_.back();
    assert_return (voice->state_ == Voice::IDLE, nullptr);   // every item in idle_voices should be idle

    // move voice from idle to active list
    idle_voices_.pop_back();
    active_voices_.push_back (voice);

    return voice;
  }
  void
  free_unused_voices()
  {
    size_t new_voice_count = 0;

    for (size_t i = 0; i < active_voices_.size(); i++)
      {
        Voice *voice = active_voices_[i];

        if (voice->state_ == Voice::IDLE)    // voice used?
          {
            idle_voices_.push_back (voice);
          }
        else
          {
            active_voices_[new_voice_count++] = voice;
          }
      }
    active_voices_.resize (new_voice_count);
  }
  void
  reset (uint64 target_stamp) override
  {
    set_max_voices (0);
    set_max_voices (32);
    adjust_all_params();
  }
  void
  init_osc (BlepUtils::OscImpl& osc, float freq)
  {
    osc.frequency_base = freq;
    osc.set_rate (sample_rate());
#if 0
    osc.freq_mod_octaves  = properties->freq_mod_octaves;
#endif
  }
  void
  adjust_param (uint32_t tag) override
  {
    switch (tag)
      {
        case FILTER_TYPE:
          {
            int new_filter_type = irintf (get_param (FILTER_TYPE));
            if (new_filter_type != filter_type_)
              {
                filter_type_ = new_filter_type;
                for (Voice *voice : active_voices_)
                  {
                    if (filter_type_ == FILTER_TYPE_LADDER)
                      voice->ladder_filter_.reset();
                    if (filter_type_ == FILTER_TYPE_SKFILTER)
                      voice->skfilter_.reset();
                  }
              }
            set_parameter_used (LADDER_MODE, filter_type_ == FILTER_TYPE_LADDER);
            set_parameter_used (SKFILTER_MODE, filter_type_ == FILTER_TYPE_SKFILTER);
          }
          break;
        case ATTACK:
        case DECAY:
        case SUSTAIN:
        case RELEASE:
        case ATTACK_SLOPE:
        case DECAY_SLOPE:
        case RELEASE_SLOPE:
          {
            for (Voice *voice : active_voices_)
              update_volume_envelope (voice);
            break;
          }
        case FIL_ATTACK:
        case FIL_DECAY:
        case FIL_SUSTAIN:
        case FIL_RELEASE:
          {
            for (Voice *voice : active_voices_)
              update_filter_envelope (voice);
            break;
          }
        case VE_MODEL:
          {
            bool ve_has_slope = irintf (get_param (VE_MODEL)) > 0; // exponential envelope has no slope parameters

            set_parameter_used (ATTACK_SLOPE,  ve_has_slope);
            set_parameter_used (DECAY_SLOPE,   ve_has_slope);
            set_parameter_used (RELEASE_SLOPE, ve_has_slope);
            break;
          }
        case KEY_C: check_note (KEY_C, old_c_, 60); break;
        case KEY_D: check_note (KEY_D, old_d_, 62); break;
        case KEY_E: check_note (KEY_E, old_e_, 64); break;
        case KEY_F: check_note (KEY_F, old_f_, 65); break;
        case KEY_G: check_note (KEY_G, old_g_, 67); break;
      }
  }
  void
  update_osc (BlepUtils::OscImpl& osc, int oscnum)
  {
    const uint O = oscnum * (OSC2_SHAPE - OSC1_SHAPE);
    osc.shape_base          = get_param (O+OSC1_SHAPE) * 0.01;
    osc.pulse_width_base    = get_param (O+OSC1_PULSE_WIDTH) * 0.01;
    osc.sub_base            = get_param (O+OSC1_SUB) * 0.01;
    osc.sub_width_base      = get_param (O+OSC1_SUB_WIDTH) * 0.01;
    osc.sync_base           = get_param (O+OSC1_SYNC);

    int octave = irintf (get_param (O+OSC1_OCTAVE));
    octave = CLAMP (octave, -2, 3);
    osc.frequency_factor = fast_exp2 (octave + get_param (O+OSC1_PITCH) / 12.);

    int unison_voices = irintf (get_param (O+OSC1_UNISON_VOICES));
    unison_voices = CLAMP (unison_voices, 1, 16);
    osc.set_unison (unison_voices, get_param (O+OSC1_UNISON_DETUNE), get_param (O+OSC1_UNISON_STEREO) * 0.01);

    set_parameter_used (O + OSC1_UNISON_DETUNE, unison_voices > 1);
    set_parameter_used (O + OSC1_UNISON_STEREO, unison_voices > 1);
  }
  static double
  perc_to_s (double perc)
  {
    // 100% -> 8s; 50% -> 1s; 0% -> 0s
    const double x = perc * 0.01;
    return x * x * x * 8;
  }
  static String
  perc_to_str (double perc)
  {
    double ms = perc_to_s (perc) * 1000;
    if (ms > 1000)
      return string_format ("%.2f s", ms / 1000);
    if (ms > 100)
      return string_format ("%.0f ms", ms);
    if (ms > 10)
      return string_format ("%.1f ms", ms);
    return string_format ("%.2f ms", ms);
  }
  static String
  hz_to_str (double hz)
  {
    if (hz > 10000)
      return string_format ("%.1f kHz", hz / 1000);
    if (hz > 1000)
      return string_format ("%.2f kHz", hz / 1000);
    if (hz > 100)
      return string_format ("%.0f Hz", hz);
    return string_format ("%.1f Hz", hz);
  }
  static float
  velocity_to_gain (float velocity, float vel_track)
  {
    /* input: velocity  [0..1]
     *        vel_track [0..1]
     *
     * convert, so that
     *  - gain (0) is (1 - vel_track)^2
     *  - gain (1) is 1
     *  - sqrt(gain(velocity)) is a straight line
     *
     *  See Roger B. Dannenberg: The Interpretation of Midi Velocity
     */
    const float x = (1 - vel_track) + vel_track * velocity;

    return x * x;
  }
  void
  note_on (int channel, int midi_note, float vel)
  {
    Voice *voice = alloc_voice();
    if (voice)
      {
        voice->freq_ = note_to_freq (midi_note);
        voice->state_ = Voice::ON;
        voice->channel_ = channel;
        voice->midi_note_ = midi_note;
        voice->vel_gain_ = velocity_to_gain (vel, get_param (VEL_TRACK) * 0.01);

        // Volume Envelope
        /* TODO: maybe use non-linear translation between level and sustain % */
        switch (irintf (get_param (VE_MODEL)))
          {
            case 0:   voice->envelope_.set_shape (FlexADSR::Shape::EXPONENTIAL);
                      break;
            default:  voice->envelope_.set_shape (FlexADSR::Shape::FLEXIBLE);
                      break;
          }
        update_volume_envelope (voice);
        voice->envelope_.set_rate (sample_rate());
        voice->envelope_.start();

        // Filter Envelope
        voice->fil_envelope_.set_shape (FlexADSR::Shape::LINEAR);
        update_filter_envelope (voice);
        voice->fil_envelope_.set_rate (sample_rate());
        voice->fil_envelope_.start();

        init_osc (voice->osc1_, voice->freq_);
        init_osc (voice->osc2_, voice->freq_);

        voice->osc1_.reset();
        voice->osc2_.reset();

        const float cutoff_min_hz = convert_cutoff (CUTOFF_MIN_MIDI);
        const float cutoff_max_hz = convert_cutoff (CUTOFF_MAX_MIDI);

        voice->ladder_filter_.reset();
        voice->ladder_filter_.set_rate (sample_rate());
        voice->ladder_filter_.set_frequency_range (cutoff_min_hz, cutoff_max_hz);

        voice->skfilter_.reset();
        voice->skfilter_.set_rate (sample_rate());
        voice->skfilter_.set_frequency_range (cutoff_min_hz, cutoff_max_hz);
        voice->new_voice_ = true;

        voice->cutoff_smooth_.reset (sample_rate(), 0.020);
        voice->last_cutoff_ = -5000; // force reset

        voice->cut_mod_smooth_.reset (sample_rate(), 0.020);
        voice->last_cut_mod_ = -5000; // force reset
        voice->last_key_track_ = -5000;

        voice->reso_smooth_.reset (sample_rate(), 0.020);
        voice->last_reso_ = -5000; // force reset
                                   //
        voice->drive_smooth_.reset (sample_rate(), 0.020);
        voice->last_drive_ = -5000; // force reset
      }
  }
  void
  note_off (int channel, int midi_note)
  {
    for (auto voice : active_voices_)
      {
        if (voice->state_ == Voice::ON && voice->midi_note_ == midi_note && voice->channel_ == channel)
          {
            voice->state_ = Voice::RELEASE;
            voice->envelope_.stop();
            voice->fil_envelope_.stop();
          }
      }
  }
  void
  check_note (ParamType pid, bool& old_value, int note)
  {
    const bool value = get_param (pid) > 0.5;
    if (value != old_value)
      {
        constexpr int channel = 0;
        if (value)
          note_on (channel, note, 100./127.);
        else
          note_off (channel, note);
        old_value = value;
      }
  }
  void
  render_voice (Voice *voice, uint n_frames, float *mix_left_out, float *mix_right_out)
  {
    float osc1_left_out[n_frames];
    float osc1_right_out[n_frames];
    float osc2_left_out[n_frames];
    float osc2_right_out[n_frames];

    update_osc (voice->osc1_, 0);
    update_osc (voice->osc2_, 1);
    voice->osc1_.process_sample_stereo (osc1_left_out, osc1_right_out, n_frames);
    voice->osc2_.process_sample_stereo (osc2_left_out, osc2_right_out, n_frames);

    // apply volume envelope & mix
    const float mix_norm = get_param (MIX) * 0.01;
    const float v1 = voice->vel_gain_ * (1 - mix_norm);
    const float v2 = voice->vel_gain_ * mix_norm;
    for (uint i = 0; i < n_frames; i++)
      {
        mix_left_out[i]  = osc1_left_out[i] * v1 + osc2_left_out[i] * v2;
        mix_right_out[i] = osc1_right_out[i] * v1 + osc2_right_out[i] * v2;
      }
    /* --------- run filter - processing in place is ok --------- */
    double cutoff = convert_cutoff (get_param (CUTOFF));
    double key_track = get_param (KEY_TRACK) * 0.01;

    if (fabs (voice->last_cutoff_ - cutoff) > 1e-7 || fabs (voice->last_key_track_ - key_track) > 1e-7)
      {
        const bool reset = voice->last_cutoff_ < -1000;

        // original strategy for key tracking: cutoff * exp (amount * log (key / 261.63))
        // but since cutoff_smooth_ is already in log2-frequency space, we can do it better

        voice->cutoff_smooth_.set (fast_log2 (cutoff) + key_track * fast_log2 (voice->freq_ / c3_hertz), reset);
        voice->last_cutoff_ = cutoff;
        voice->last_key_track_ = key_track;
      }
    double cut_mod = get_param (FIL_CUT_MOD) / 12.; /* convert semitones to octaves */
    if (fabs (voice->last_cut_mod_ - cut_mod) > 1e-7)
      {
        const bool reset = voice->last_cut_mod_ < -1000;

        voice->cut_mod_smooth_.set (cut_mod, reset);
        voice->last_cut_mod_ = cut_mod;
      }
    double resonance = get_param (RESONANCE) * 0.01;
    if (fabs (voice->last_reso_ - resonance) > 1e-7)
      {
        const bool reset = voice->last_reso_ < -1000;

        voice->reso_smooth_.set (resonance, reset);
        voice->last_reso_ = resonance;
      }
    double drive = get_param (DRIVE);
    if (fabs (voice->last_drive_ - drive) > 1e-7)
      {
        const bool reset = voice->last_drive_ < -1000;

        voice->drive_smooth_.set (drive, reset);
        voice->last_drive_ = drive;
      }

    auto filter_process_block = [&] (auto& filter)
      {
        auto gen_filter_input = [&] (float *freq_in, float *reso_in, float *drive_in, uint n_frames)
          {
            voice->fil_envelope_.process (freq_in, n_frames);

            for (uint i = 0; i < n_frames; i++)
              {
                freq_in[i] = fast_exp2 (voice->cutoff_smooth_.get_next() + freq_in[i] * voice->cut_mod_smooth_.get_next());
                reso_in[i] = voice->reso_smooth_.get_next();
                drive_in[i] = voice->drive_smooth_.get_next();
              }
          };

        bool const_freq = voice->cutoff_smooth_.is_constant() && voice->fil_envelope_.is_constant() && voice->cut_mod_smooth_.is_constant();
        bool const_reso = voice->reso_smooth_.is_constant();
        bool const_drive = voice->drive_smooth_.is_constant();

        if (const_freq && const_reso && const_drive)
          {
            /* use more efficient version of the filter computation if all parameters are constants */
            float freq, reso, drive;
            gen_filter_input (&freq, &reso, &drive, 1);

            filter.set_freq (freq);
            filter.set_reso (reso);
            filter.set_drive (drive);
            filter.process_block (n_frames, mix_left_out, mix_right_out);
          }
        else
          {
            /* generic version: pass per-sample values for freq, reso and drive */
            float freq_in[n_frames], reso_in[n_frames], drive_in[n_frames];
            gen_filter_input (freq_in, reso_in, drive_in, n_frames);

            filter.process_block (n_frames, mix_left_out, mix_right_out, freq_in, reso_in, drive_in);
          }
      };

    if (filter_type_ == FILTER_TYPE_LADDER)
      {
        voice->ladder_filter_.set_mode (LadderVCF::Mode (irintf (get_param (LADDER_MODE))));
        filter_process_block (voice->ladder_filter_);
      }
    else if (filter_type_ == FILTER_TYPE_SKFILTER)
      {
        voice->skfilter_.set_mode (SKFilter::Mode (irintf (get_param (SKFILTER_MODE))));
        filter_process_block (voice->skfilter_);
      }
  }
  void
  set_parameter_used (uint32_t id, bool used)
  {
    // TODO: implement this function to enable/disable parameters in the gui
    // printf ("TODO: set parameter %d used flag to %s\n", int (id), used ? "true" : "false");
  }
  void
  update_volume_envelope (Voice *voice)
  {
    voice->envelope_.set_attack (perc_to_s (get_param (ATTACK)));
    voice->envelope_.set_decay (perc_to_s (get_param (DECAY)));
    voice->envelope_.set_sustain (get_param (SUSTAIN));         /* percent */
    voice->envelope_.set_release (perc_to_s (get_param (RELEASE)));
    voice->envelope_.set_attack_slope (get_param (ATTACK_SLOPE) * 0.01);
    voice->envelope_.set_decay_slope (get_param (DECAY_SLOPE) * 0.01);
    voice->envelope_.set_release_slope (get_param (RELEASE_SLOPE) * 0.01);
  }
  void
  update_filter_envelope (Voice *voice)
  {
    voice->fil_envelope_.set_attack (perc_to_s (get_param (FIL_ATTACK)));
    voice->fil_envelope_.set_decay (perc_to_s (get_param (FIL_DECAY)));
    voice->fil_envelope_.set_sustain (get_param (FIL_SUSTAIN));         /* percent */
    voice->fil_envelope_.set_release (perc_to_s (get_param (FIL_RELEASE)));
  }
  void
  render_audio (float *left_out, float *right_out, uint n_frames)
  {
    if (!n_frames)
      return;

    bool   need_free = false;

    for (Voice *voice : active_voices_)
      {
        if (voice->new_voice_)
          {
            int idelay = 0;
            if (filter_type_ == FILTER_TYPE_LADDER)
              idelay = voice->ladder_filter_.delay();
            if (filter_type_ == FILTER_TYPE_SKFILTER)
              idelay = voice->skfilter_.delay();
            if (idelay)
              {
                // compensate FIR oversampling filter latency
                float junk[idelay];
                render_voice (voice, idelay, junk, junk);
              }
            voice->new_voice_ = false;
          }
        float mix_left_out[n_frames];
        float mix_right_out[n_frames];

        render_voice (voice, n_frames, mix_left_out, mix_right_out);

        // apply volume envelope
        float volume_env[n_frames];
        voice->envelope_.process (volume_env, n_frames);
        float post_gain_factor = db2voltage (get_param (POST_GAIN));
        for (uint i = 0; i < n_frames; i++)
          {
            float amp = post_gain_factor * volume_env[i];
            left_out[i] += mix_left_out[i] * amp;
            right_out[i] += mix_right_out[i] * amp;
          }
        if (voice->envelope_.done())
          {
            voice->state_ = Voice::IDLE;
            need_free = true;
          }
      }
    if (need_free)
      free_unused_voices();
  }
  void
  render (uint n_frames) override
  {
    assert_return (n_ochannels (stereout_) == 2);

    float *left_out = oblock (stereout_, 0);
    float *right_out = oblock (stereout_, 1);

    floatfill (left_out, 0.f, n_frames);
    floatfill (right_out, 0.f, n_frames);

    uint offset = 0;
    MidiEventInput evinput = midi_event_input();
    for (const auto &ev : evinput)
      {
        uint frame = std::max<int> (ev.frame, 0); // TODO: should be unsigned anyway, issue #26

        // process any audio that is before the event
        render_audio (left_out + offset, right_out + offset, frame - offset);
        offset = frame;

        switch (ev.message())
          {
          case MidiMessage::NOTE_OFF:
            note_off (ev.channel, ev.key);
            break;
          case MidiMessage::NOTE_ON:
            note_on (ev.channel, ev.key, ev.velocity);
            break;
          case MidiMessage::ALL_NOTES_OFF:
            for (auto voice : active_voices_)
              if (voice->state_ == Voice::ON && voice->channel_ == ev.channel)
                note_off (voice->channel_, voice->midi_note_);
            break;
          case MidiMessage::PARAM_VALUE:
            apply_event (ev);
            adjust_param (ev.param);
            break;
          default: ;
          }
      }
    // process frames after last event
    render_audio (left_out + offset, right_out + offset, n_frames - offset);
  }

  static double
  convert_cutoff (double midi_note)
  {
    return 440 * std::pow (2, (midi_note - 69) / 12.);
  }
  std::string
  param_value_to_text (uint32_t paramid, double value) const override
  {
    /* fake step=1 */
    for (int oscnum = 0; oscnum < 2; oscnum++)
      {
        const uint O = oscnum * (OSC2_SHAPE - OSC1_SHAPE);
        if (paramid == O+OSC1_UNISON_VOICES)
          return string_format ("%d Voices", irintf (value));
        if (paramid == O+OSC1_OCTAVE)
          return string_format ("%d Octaves", irintf (value));
      }
    for (auto p : { ATTACK, DECAY, RELEASE, FIL_ATTACK, FIL_DECAY, FIL_RELEASE })
      if (paramid == p)
        return perc_to_str (value);
    if (paramid == CUTOFF)
      return hz_to_str (convert_cutoff (value));

    return AudioProcessor::param_value_to_text (paramid, value);
  }
public:
  BlepSynth (const ProcessorSetup &psetup) :
    AudioProcessor (psetup)
  {}
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.version      = "1";
    info.label        = "BlepSynth";
    info.category     = "Synth";
    info.creator_name = "Stefan Westerfeld";
    info.website_url  = "https://anklang.testbit.eu";
  }
};
static auto blepsynth = register_audio_processor<BlepSynth> ("Ase::Devices::BlepSynth");

} // Anon
