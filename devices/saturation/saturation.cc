// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/internal.hh"
#include "saturationdsp.hh"

namespace {

using namespace Ase;

class Saturation : public AudioProcessor {
  IBusId stereoin;
  OBusId stereout;
  SaturationDSP saturation;
public:
  Saturation (const ProcessorSetup &psetup) :
    AudioProcessor (psetup)
  {}
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.version = "1";
    info.label = "Saturation";
    info.category = "Distortion";
    info.creator_name = "Stefan Westerfeld";
    info.website_url  = "https://anklang.testbit.eu";
  }
  enum Params { MODE = 1, MIX, DRIVE };
  void
  initialize (SpeakerArrangement busses) override
  {
    stereoin = add_input_bus  ("Stereo In",  SpeakerArrangement::STEREO);
    stereout = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);

    ParameterMap pmap;
    pmap.group = "Settings";

    ChoiceS centries;
    centries += { "Soft/tanh",  "Soft Saturation using the tanh function" };
    centries += { "Hard",       "Hard clipping" };
    pmap[MODE]  = Param { "mode",  "Mode",  "Mode", 0, "", std::move (centries), "", { String ("blurb=") + _("Saturation Function"), } };
    pmap[MIX]   = Param { "mix",   "Mix dry/wet", "Mix", 100, "%", { 0, 100 } };
    pmap[DRIVE] = Param { "drive", "Drive", "Drive", 0, "dB", { -6, 36 } };

    install_params (pmap);

    prepare_event_input();
  }
  SaturationDSP::Mode
  map_mode (int m)
  {
    if (m == 1)
      return SaturationDSP::Mode::HARD_CLIP;
    return SaturationDSP::Mode::TANH_CHEAP;
  }
  void
  adjust_param (uint32_t tag) override
  {
    switch (Params (tag))
      {
      case DRIVE:       saturation.set_drive (get_param (DRIVE), false);
                        return;
      case MIX:         saturation.set_mix (get_param (MIX), false);
                        return;
      case MODE:        saturation.set_mode (map_mode (get_param (MODE)));
                        return;
      }
  }
  void
  reset (uint64 target_stamp) override
  {
    saturation.reset (sample_rate());
    adjust_all_params();
  }
  void
  render_audio (float *left_in, float *right_in, float *left_out, float *right_out, uint n_frames)
  {
    if (!n_frames)
      return;

    saturation.process<true> (left_in, right_in, left_out, right_out, n_frames);
  }
  void
  render (uint n_frames) override
  {
    float *left_in = const_cast<float*> (ifloats (stereoin, 0));
    float *right_in = const_cast<float*> (ifloats (stereoin, 1));
    float *left_out = oblock (stereout, 0);
    float *right_out = oblock (stereout, 1);

    uint offset = 0;
    MidiEventInput evinput = midi_event_input();
    for (const auto &ev : evinput)
      {
        uint frame = std::max<int> (ev.frame, 0); // TODO: should be unsigned anyway, issue #26

        // process any audio that is before the event
        render_audio (left_in + offset, right_in + offset, left_out + offset, right_out + offset, frame - offset);
        offset = frame;

        switch (ev.message())
          {
          case MidiMessage::PARAM_VALUE:
            apply_event (ev);
            adjust_param (ev.param);
            break;
          default: ;
          }
      }
    // process frames after last event
    render_audio (left_in + offset, right_in + offset, left_out + offset, right_out + offset, n_frames - offset);
  }
};
static auto saturation = register_audio_processor<Saturation> ("Ase::Devices::Saturation");

}
