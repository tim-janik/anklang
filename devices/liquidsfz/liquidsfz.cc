// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/midievent.hh"
#include "ase/internal.hh"

#include <liquidsfz.hh>

namespace {

using namespace Ase;

using LiquidSFZ::Synth;

class LiquidSFZLoader
{
  enum { STATE_IDLE, STATE_LOAD };
  std::atomic<int>      state_ { STATE_IDLE };
  std::atomic<int>      quit_ { 0 };
  Ase::ScopedSemaphore  sem_;
  std::thread           thread_;

  Synth &synth_;
  String have_sfz_;
  String want_sfz_;
  uint   want_sample_rate_ = 0;
  uint   have_sample_rate_ = 0;

  void
  run()
  {
    while (!quit_.load())
      {
        sem_.wait();
        if (state_.load() == STATE_LOAD)
          {
            if (want_sfz_ != have_sfz_)
              {
                printf ("LiquidSFZ: loading %s...", want_sfz_.c_str());
                fflush (stdout);
                bool result = synth_.load (want_sfz_);
                printf ("%s\n", result ? "OK" : "FAIL");
                // TODO: handle load error

                have_sfz_ = want_sfz_;
              }
            if (want_sample_rate_ != have_sample_rate_)
              {
                synth_.set_sample_rate (want_sample_rate_);
                have_sample_rate_ = want_sample_rate_;
              }
            state_.store (STATE_IDLE);
          }
      }
    printf ("run() done\n");
  }
public:
  LiquidSFZLoader (Synth &synth) :
    synth_ (synth)
  {
    thread_ = std::thread (&LiquidSFZLoader::run, this);
    want_sfz_.reserve (4096); // avoid allocations in audio thread
    printf ("LiquidSFZLoader()\n");
  }
  ~LiquidSFZLoader()
  {
    quit_.store (1);
    sem_.post();
    thread_.join();
    printf ("~LiquidSFZLoader()\n");
  }
  // called from audio thread
  bool
  idle()
  {
    if (state_.load() == STATE_IDLE)
      {
        if (want_sfz_ == have_sfz_ && want_sample_rate_ == have_sample_rate_)
          return true;
      }
    state_.store (STATE_LOAD);
    sem_.post();
    return false;
  }
  // called from audio thread
  void
  load (const String &sfz)
  {
    want_sfz_ = sfz;
  }
  // called from audio thread
  void
  set_sample_rate (uint sample_rate)
  {
    want_sample_rate_ = sample_rate;
  }
};

// == LiquidSFZ ==
// SFZ sampler using liquidsfz library
class LiquidSFZ : public AudioProcessor {
  OBusId stereo_out_;
  Synth synth_;
  bool synth_need_reset_ = false;
  ChoiceS hardcoded_instruments_;
  LiquidSFZLoader loader_;

  enum Params { INSTRUMENT = 1 };

  static constexpr const int PID_CC_OFFSET = 1000;

  void
  initialize (SpeakerArrangement busses) override
  {
    auto insts = hardcoded_instruments_;

    ParameterMap pmap;
    pmap[INSTRUMENT] = Param { "instrument", "Instrument", "Instrument", 0, "", std::move (insts), "", "Instrument (should have a file selector)" };
    install_params (pmap);

    loader_.set_sample_rate (sample_rate());
    prepare_event_input();
    stereo_out_ = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);
    assert_return (bus_info (stereo_out_).ident == "stereo_out");
  }
  void
  reset (uint64 target_stamp) override
  {
    synth_need_reset_ = true;
    adjust_all_params();
  }
  void
  adjust_param (uint32_t tag) override
  {
    switch (tag)
      {
        case INSTRUMENT: loader_.load (hardcoded_instruments_[irintf (get_param (tag))].blurb);
                         break;
      }
#if 0
    /* TODO: after loading, we should query CCs and setup parameters for CCs */
    auto ccs = synth_.list_ccs();
    for (const auto& cc_info : ccs)
      {
        Id32 pid = cc_info.cc() + PID_CC_OFFSET;
        add_param (pid, cc_info.label(), cc_info.label(), 0, 100, cc_info.default_value() / 127. * 100, "%");
      }
    /* TODO: for adjust_param, we should handle SFZ specific parameters */
    if (tag >= PID_CC_OFFSET)
      {
        const int cc = tag - PID_CC_OFFSET;
        const int cc_value = std::clamp (irintf (get_param (tag) * 0.01 * 127), 0, 127);
        /* TODO: make a decision here how UI parameters (which ought to be automatable) map to CC values
         *
         *  - should the liquidsfz synth maintain different CC state for different midi channels?
         *  - should automating a parameter affect all notes (or only these on specific channels)?
         *  - should CC events affect all notes (or only those on specific channels)?
         *  - should we support per note modulation?
         *
         * Right now, we map parameter changes to CC events on channel 0 only.
         */
        synth_.add_event_cc (0, 0, cc, cc_value);
      }
#endif
  }
  void
  render (uint n_frames) override
  {
    if (loader_.idle())
      {
        if (synth_need_reset_)
          {
            synth_.system_reset();
            synth_need_reset_ = false;
          }

        MidiEventInput evinput = midi_event_input();
        for (const auto &ev : evinput)
          {
            const int time_stamp = std::max<int> (ev.frame, 0);
            switch (ev.message())
              {
              case MidiMessage::NOTE_OFF:
                synth_.add_event_note_off (time_stamp, ev.channel, ev.key);
                break;
              case MidiMessage::NOTE_ON:
                synth_.add_event_note_on (time_stamp, ev.channel, ev.key, std::clamp (irintf (ev.velocity * 127), 0, 127));
                break;
              case MidiMessage::ALL_NOTES_OFF:
              case MidiMessage::ALL_SOUND_OFF:
                synth_.all_sound_off();    // NOTE: there is no extra "all notes off" in liquidsfz
                break;
              case MidiMessage::PARAM_VALUE:
                apply_event (ev);
                adjust_param (ev.param);
                break;
              default: ;
              }
          }

        float *output[2] = {
          oblock (stereo_out_, 0),
          oblock (stereo_out_, 1)
        };
        synth_.process (output, n_frames);
      }
    else
      {
        float *left_out = oblock (stereo_out_, 0);
        float *right_out = oblock (stereo_out_, 1);

        floatfill (left_out, 0.f, n_frames);
        floatfill (right_out, 0.f, n_frames);
      }
  }
public:
  LiquidSFZ (const ProcessorSetup &psetup) :
    AudioProcessor (psetup),
    loader_ (synth_)
  {
    hardcoded_instruments_ += { "P", "Piano", "/home/stefan/sfz/SalamanderGrandPianoV3_44.1khz16bit/SalamanderGrandPianoV3.sfz" };
    hardcoded_instruments_ += { "C", "CelloEns", "/home/stefan/sfz/VSCO-2-CE-1.1.0/CelloEnsSusVib.sfz" };
    hardcoded_instruments_ += { "O", "Organ", "/home/stefan/sfz/VSCO-2-CE-1.1.0/OrganLoud.sfz" };
  }
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.version      = "1";
    info.label        = "LiquidSFZ";
    info.category     = "Synth";
    info.creator_name = "Stefan Westerfeld";
    info.website_url  = "https://anklang.testbit.eu";
  }
};

static auto liquidsfz = register_audio_processor<LiquidSFZ> ("Ase::Devices::LiquidSFZ");

} // Bse