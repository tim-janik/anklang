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
  ParamId pid_c_, pid_d_, pid_e_, pid_f_, pid_g_;
  bool    old_c_, old_d_, old_e_, old_f_, old_g_;

  struct OscParams {
    ParamId shape;
    ParamId pulse_width;
    ParamId sub;
    ParamId sub_width;
    ParamId sync;
    ParamId octave;
    ParamId pitch;

    ParamId unison_voices;
    ParamId unison_detune;
    ParamId unison_stereo;
  };
  OscParams osc_params[2];
  ParamId pid_mix_;
  ParamId pid_vel_track_;
  ParamId pid_post_gain_;

  ParamId pid_cutoff_;
  ParamId pid_resonance_;
  ParamId pid_drive_;
  ParamId pid_key_track_;
  ParamId pid_filter_type_;
  ParamId pid_ladder_mode_;
  ParamId pid_skfilter_mode_;

  enum { FILTER_TYPE_BYPASS, FILTER_TYPE_LADDER, FILTER_TYPE_SKFILTER };

  static constexpr int CUTOFF_MIN_MIDI = 15;
  static constexpr int CUTOFF_MAX_MIDI = 144;

  ParamId pid_attack_;
  ParamId pid_decay_;
  ParamId pid_sustain_;
  ParamId pid_release_;
  ParamId pid_ve_model_;
  ParamId pid_attack_slope_;
  ParamId pid_decay_slope_;
  ParamId pid_release_slope_;
  bool    need_update_volume_envelope_;

  ParamId pid_fil_attack_;
  ParamId pid_fil_decay_;
  ParamId pid_fil_sustain_;
  ParamId pid_fil_release_;
  ParamId pid_fil_cut_mod_;
  bool    need_update_filter_envelope_;

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

    auto oscparams = [&] (int o) {
      start_group (string_format ("Oscillator %d", o + 1));
      osc_params[o].shape = add_param (string_format ("Osc %d Shape", o + 1), "Shape", -100, 100, 0, "%");
      osc_params[o].pulse_width = add_param (string_format ("Osc %d Pulse Width", o + 1), "P.W", 0, 100, 50, "%");
      osc_params[o].sub = add_param (string_format ("Osc %d Subharmonic", o + 1), "Sub", 0, 100, 0, "%");
      osc_params[o].sub_width = add_param (string_format ("Osc %d Subharmonic Width", o + 1), "Sub.W", 0, 100, 50, "%");
      osc_params[o].sync = add_param (string_format ("Osc %d Sync Slave", o + 1), "Sync", 0, 60, 0, "semitones");

      osc_params[o].pitch  = add_param (string_format ("Osc %d Pitch", o + 1), "Pitch", -7, 7, 0, "semitones");
      osc_params[o].octave = add_param (string_format ("Osc %d Octave", o + 1), "Octave", -2, 3, 0, "octaves");

      /* TODO: unison_voices property should have stepping set to 1 */
      osc_params[o].unison_voices = add_param (string_format ("Osc %d Unison Voices", o + 1), "Voices", 1, 16, 1, "voices");
      osc_params[o].unison_detune = add_param (string_format ("Osc %d Unison Detune", o + 1), "Detune", 0.5, 50, 6, "%");
      osc_params[o].unison_stereo = add_param (string_format ("Osc %d Unison Stereo", o + 1), "Stereo", 0, 100, 0, "%");
    };

    oscparams (0);

    start_group ("Mix");
    pid_mix_ = add_param ("Mix", "Mix", 0, 100, 0, "%");
    pid_vel_track_ = add_param ("Velocity Tracking", "VelTr", 0, 100, 50, "%");
    // TODO: this probably should default to 0dB once we have track/mixer volumes
    pid_post_gain_ = add_param ("Post Gain", "Gain", -24, 24, -12, "dB");

    oscparams (1);

    start_group ("Volume Envelope");
    ChoiceS ve_model_cs;
    ve_model_cs += { "A", "Analog" };
    ve_model_cs += { "F", "Flexible" };
    pid_ve_model_ = add_param ("Envelope Model", "Model", std::move (ve_model_cs), 0, "", "ADSR Model to be used");

    pid_attack_  = add_param ("Attack",  "A", 0, 100, 20.0, "%");
    pid_decay_   = add_param ("Decay",   "D", 0, 100, 30.0, "%");
    pid_sustain_ = add_param ("Sustain", "S", 0, 100, 50.0, "%");
    pid_release_ = add_param ("Release", "R", 0, 100, 30.0, "%");

    pid_attack_slope_ = add_param ("Attack Slope", "AS", -100, 100, 50, "%");
    pid_decay_slope_ = add_param ("Decay Slope", "DS", -100, 100, -100, "%");
    pid_release_slope_ = add_param ("Release Slope", "RS", -100, 100, -100, "%");

    start_group ("Filter");

    pid_cutoff_ = add_param ("Cutoff", "Cutoff", CUTOFF_MIN_MIDI, CUTOFF_MAX_MIDI, 60); // cutoff as midi notes
    pid_resonance_ = add_param ("Resonance", "Reso", 0, 100, 25.0, "%");
    pid_drive_ = add_param ("Drive", "Drive", -24, 36, 0, "dB");
    pid_key_track_ = add_param ("Key Tracking", "KeyTr", 0, 100, 50, "%");
    ChoiceS filter_type_choices;
    filter_type_choices += { "—"_uc, "Bypass Filter" };
    filter_type_choices += { "LD"_uc, "Ladder Filter" };
    filter_type_choices += { "SKF"_uc, "Sallen-Key Filter" };
    pid_filter_type_ = add_param ("Filter Type", "Type", std::move (filter_type_choices), FILTER_TYPE_LADDER, "", "Filter Type to be used");

    ChoiceS ladder_mode_choices;
    ladder_mode_choices += { "LP1"_uc, "1 Pole Lowpass, 6dB/Octave" };
    ladder_mode_choices += { "LP2"_uc, "2 Pole Lowpass, 12dB/Octave" };
    ladder_mode_choices += { "LP3"_uc, "3 Pole Lowpass, 18dB/Octave" };
    ladder_mode_choices += { "LP4"_uc, "4 Pole Lowpass, 24dB/Octave" };
    pid_ladder_mode_ = add_param ("Filter Mode", "Mode", std::move (ladder_mode_choices), 1, "", "Ladder Filter Mode to be used");

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
    pid_skfilter_mode_ = add_param ("SKFilter Mode", "Mode", std::move (skfilter_mode_choices), 2, "", "Sallen-Key Filter Mode to be used");

    start_group ("Filter Envelope");
    pid_fil_attack_   = add_param ("Attack",  "A", 0, 100, 40, "%");
    pid_fil_decay_    = add_param ("Decay",   "D", 0, 100, 55, "%");
    pid_fil_sustain_  = add_param ("Sustain", "S", 0, 100, 30, "%");
    pid_fil_release_  = add_param ("Release", "R", 0, 100, 30, "%");
    pid_fil_cut_mod_  = add_param ("Env Cutoff Modulation", "CutMod", -96, 96, 36, "semitones"); /* 8 octaves range */

    start_group ("Keyboard Input");
    pid_c_ = add_param ("Main Input  1",  "C", false, GUIONLY);
    pid_d_ = add_param ("Main Input  2",  "D", false, GUIONLY);
    pid_e_ = add_param ("Main Input  3",  "E", false, GUIONLY);
    pid_f_ = add_param ("Main Input  4",  "F", false, GUIONLY);
    pid_g_ = add_param ("Main Input  5",  "G", false, GUIONLY);
    old_c_ = old_d_ = old_e_ = old_f_ = old_g_ = false;

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
    adjust_params (true);

    need_update_filter_envelope_ = false;
    need_update_volume_envelope_ = false;
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
  adjust_param (Id32 tag) override
  {
    if (tag == pid_filter_type_)
      {
        for (Voice *voice : active_voices_)
          {
            voice->ladder_filter_.reset();
            voice->skfilter_.reset();
          }
      }
    if (tag == pid_attack_ || tag == pid_decay_ || tag == pid_sustain_ || tag == pid_release_ ||
        tag == pid_attack_slope_ || tag == pid_decay_slope_ || tag == pid_release_slope_)
      {
        need_update_volume_envelope_ = true;
      }
    if (tag == pid_fil_attack_ || tag == pid_fil_decay_ || tag == pid_fil_sustain_ || tag == pid_fil_release_)
      {
        need_update_filter_envelope_ = true;
      }
    if (tag == pid_ve_model_)
      {
        bool ve_has_slope = irintf (get_param (pid_ve_model_)) > 0; // exponential envelope has no slope parameters

        set_parameter_used (pid_attack_slope_,  ve_has_slope);
        set_parameter_used (pid_decay_slope_,   ve_has_slope);
        set_parameter_used (pid_release_slope_, ve_has_slope);
      }
  }
  void
  update_osc (BlepUtils::OscImpl& osc, const OscParams& params)
  {
    osc.shape_base          = get_param (params.shape) * 0.01;
    osc.pulse_width_base    = get_param (params.pulse_width) * 0.01;
    osc.sub_base            = get_param (params.sub) * 0.01;
    osc.sub_width_base      = get_param (params.sub_width) * 0.01;
    osc.sync_base           = get_param (params.sync);

    int octave = irintf (get_param (params.octave));
    octave = CLAMP (octave, -2, 3);
    osc.frequency_factor = fast_exp2 (octave + get_param (params.pitch) / 12.);

    int unison_voices = irintf (get_param (params.unison_voices));
    unison_voices = CLAMP (unison_voices, 1, 16);
    osc.set_unison (unison_voices, get_param (params.unison_detune), get_param (params.unison_stereo) * 0.01);
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
        voice->vel_gain_ = velocity_to_gain (vel, get_param (pid_vel_track_) * 0.01);

        // Volume Envelope
        /* TODO: maybe use non-linear translation between level and sustain % */
        switch (irintf (get_param (pid_ve_model_)))
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
  check_note (ParamId pid, bool& old_value, int note)
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

    update_osc (voice->osc1_, osc_params[0]);
    update_osc (voice->osc2_, osc_params[1]);
    voice->osc1_.process_sample_stereo (osc1_left_out, osc1_right_out, n_frames);
    voice->osc2_.process_sample_stereo (osc2_left_out, osc2_right_out, n_frames);

    // apply volume envelope & mix
    const float mix_norm = get_param (pid_mix_) * 0.01;
    const float v1 = voice->vel_gain_ * (1 - mix_norm);
    const float v2 = voice->vel_gain_ * mix_norm;
    for (uint i = 0; i < n_frames; i++)
      {
        mix_left_out[i]  = osc1_left_out[i] * v1 + osc2_left_out[i] * v2;
        mix_right_out[i] = osc1_right_out[i] * v1 + osc2_right_out[i] * v2;
      }
    /* --------- run filter - processing in place is ok --------- */
    double cutoff = convert_cutoff (get_param (pid_cutoff_));
    double key_track = get_param (pid_key_track_) * 0.01;

    if (fabs (voice->last_cutoff_ - cutoff) > 1e-7 || fabs (voice->last_key_track_ - key_track) > 1e-7)
      {
        const bool reset = voice->last_cutoff_ < -1000;

        // original strategy for key tracking: cutoff * exp (amount * log (key / 261.63))
        // but since cutoff_smooth_ is already in log2-frequency space, we can do it better

        voice->cutoff_smooth_.set (fast_log2 (cutoff) + key_track * fast_log2 (voice->freq_ / c3_hertz), reset);
        voice->last_cutoff_ = cutoff;
        voice->last_key_track_ = key_track;
      }
    double cut_mod = get_param (pid_fil_cut_mod_) / 12.; /* convert semitones to octaves */
    if (fabs (voice->last_cut_mod_ - cut_mod) > 1e-7)
      {
        const bool reset = voice->last_cut_mod_ < -1000;

        voice->cut_mod_smooth_.set (cut_mod, reset);
        voice->last_cut_mod_ = cut_mod;
      }
    double resonance = get_param (pid_resonance_) * 0.01;
    if (fabs (voice->last_reso_ - resonance) > 1e-7)
      {
        const bool reset = voice->last_reso_ < -1000;

        voice->reso_smooth_.set (resonance, reset);
        voice->last_reso_ = resonance;
      }
    double drive = get_param (pid_drive_);
    if (fabs (voice->last_drive_ - drive) > 1e-7)
      {
        const bool reset = voice->last_drive_ < -1000;

        voice->drive_smooth_.set (drive, reset);
        voice->last_drive_ = drive;
      }

    auto filter_process_block = [&] (auto& filter)
      {
        if (need_update_filter_envelope_)
          update_filter_envelope (voice);

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

    int filter_type = irintf (get_param (pid_filter_type_));
    if (filter_type == FILTER_TYPE_LADDER)
      {
        voice->ladder_filter_.set_mode (LadderVCF::Mode (irintf (get_param (pid_ladder_mode_))));
        filter_process_block (voice->ladder_filter_);
      }
    else if (filter_type == FILTER_TYPE_SKFILTER)
      {
        voice->skfilter_.set_mode (SKFilter::Mode (irintf (get_param (pid_skfilter_mode_))));
        filter_process_block (voice->skfilter_);
      }
  }
  void
  set_parameter_used (ParamId id, bool used)
  {
    // TODO: implement this function to enable/disable parameters in the gui
    // printf ("TODO: set parameter %d used flag to %s\n", int (id), used ? "true" : "false");
  }
  void
  update_volume_envelope (Voice *voice)
  {
    voice->envelope_.set_attack (perc_to_s (get_param (pid_attack_)));
    voice->envelope_.set_decay (perc_to_s (get_param (pid_decay_)));
    voice->envelope_.set_sustain (get_param (pid_sustain_));         /* percent */
    voice->envelope_.set_release (perc_to_s (get_param (pid_release_)));
    voice->envelope_.set_attack_slope (get_param (pid_attack_slope_) * 0.01);
    voice->envelope_.set_decay_slope (get_param (pid_decay_slope_) * 0.01);
    voice->envelope_.set_release_slope (get_param (pid_release_slope_) * 0.01);
  }
  void
  update_filter_envelope (Voice *voice)
  {
    voice->fil_envelope_.set_attack (perc_to_s (get_param (pid_fil_attack_)));
    voice->fil_envelope_.set_decay (perc_to_s (get_param (pid_fil_decay_)));
    voice->fil_envelope_.set_sustain (get_param (pid_fil_sustain_));         /* percent */
    voice->fil_envelope_.set_release (perc_to_s (get_param (pid_fil_release_)));
  }
  void
  render (uint n_frames) override
  {
    adjust_params (false);

    /* TODO: replace this with true midi input */
    check_note (pid_c_, old_c_, 60);
    check_note (pid_d_, old_d_, 62);
    check_note (pid_e_, old_e_, 64);
    check_note (pid_f_, old_f_, 65);
    check_note (pid_g_, old_g_, 67);

    MidiEventRange erange = get_event_input();
    for (const auto &ev : erange)
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
        default: ;
        }

    assert_return (n_ochannels (stereout_) == 2);
    bool   need_free = false;
    float *left_out = oblock (stereout_, 0);
    float *right_out = oblock (stereout_, 1);

    floatfill (left_out, 0.f, n_frames);
    floatfill (right_out, 0.f, n_frames);

    for (Voice *voice : active_voices_)
      {
        if (voice->new_voice_)
          {
            int filter_type = irintf (get_param (pid_filter_type_));
            int idelay = 0;
            if (filter_type == FILTER_TYPE_LADDER)
              idelay = voice->ladder_filter_.delay();
            if (filter_type == FILTER_TYPE_SKFILTER)
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
        if (need_update_volume_envelope_)
          update_volume_envelope (voice);
        voice->envelope_.process (volume_env, n_frames);
        float post_gain_factor = db2voltage (get_param (pid_post_gain_));
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
    need_update_volume_envelope_ = false;
    need_update_filter_envelope_ = false;
  }
  static double
  convert_cutoff (double midi_note)
  {
    return 440 * std::pow (2, (midi_note - 69) / 12.);
  }
  std::string
  param_value_to_text (Id32 paramid, double value) const override
  {
    /* fake step=1 */
    for (int o = 0; o < 2; o++)
      {
        if (paramid == osc_params[o].unison_voices)
          return string_format ("%d voices", irintf (value));
        if (paramid == osc_params[o].octave)
          return string_format ("%d octaves", irintf (value));
      }
    for (auto p : { pid_attack_, pid_decay_, pid_release_, pid_fil_attack_, pid_fil_decay_, pid_fil_release_ })
      if (paramid == p)
        return perc_to_str (value);
    if (paramid == pid_cutoff_)
      return hz_to_str (convert_cutoff (value));

    return AudioProcessor::param_value_to_text (paramid, value);
  }
public:
  BlepSynth (AudioEngine &engine) :
    AudioProcessor (engine)
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
