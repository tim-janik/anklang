// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/midievent.hh"
#include "ase/internal.hh"

#include <liquidsfz.hh>

namespace {

using namespace Ase;

using LiquidSFZ::Synth;

// == LiquidSFZ ==
// SFZ sampler using liquidsfz library
class LiquidSFZ : public AudioProcessor {
  OBusId stereo_out_;
  Synth synth_;

  static constexpr const int PID_CC_OFFSET = 1000;

  void
  initialize (SpeakerArrangement busses) override
  {
    synth_.set_sample_rate (sample_rate());
    // TODO: let the user choose using file dialog
    const char *l = getenv ("SFZ");
    if (!l)
      l = "/home/stefan/sfz/SalamanderGrandPianoV3_44.1khz16bit/SalamanderGrandPianoV3.sfz";
    synth_.load (l);
    // TODO: handle load error

    auto ccs = synth_.list_ccs();
    for (const auto& cc_info : ccs)
      {
        Id32 pid = cc_info.cc() + PID_CC_OFFSET;
        add_param (pid, cc_info.label(), cc_info.label(), 0, 100, cc_info.default_value() / 127. * 100, "%");
      }
    prepare_event_input();
    stereo_out_ = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);
    assert_return (bus_info (stereo_out_).ident == "stereo_out");
  }
  void
  reset (uint64 target_stamp) override
  {
    synth_.system_reset();

    adjust_params (true);
  }
  void
  adjust_param (Id32 tag) override
  {
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
  }
  void
  render (uint n_frames) override
  {
    adjust_params (false);

    MidiEventRange erange = get_event_input();
    for (const auto &ev : erange)
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
          default: ;
          }
      }

    float *output[2] = {
      oblock (stereo_out_, 0),
      oblock (stereo_out_, 1)
    };
    synth_.process (output, n_frames);
  }
public:
  LiquidSFZ (AudioEngine &engine) :
    AudioProcessor (engine)
  {
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
