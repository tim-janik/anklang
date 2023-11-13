// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/internal.hh"
#include "saturationdsp.hh"

namespace {

using namespace Ase;

class Saturation : public AudioProcessor {
  IBusId stereoin;
  OBusId stereout;
  SaturationDSP dleft;
  SaturationDSP dright;
public:
  Saturation (AudioEngine &engine) :
    AudioProcessor (engine)
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
    remove_all_buses();

    stereoin = add_input_bus  ("Stereo In",  SpeakerArrangement::STEREO);
    stereout = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);

    start_group ("Settings");

    ChoiceS centries;
    centries += { "Soft/tanh",  "Soft Saturation using the tanh function" };
    centries += { "True tanh",  "Soft Saturation using the tanh function" };
    centries += { "Hard",       "Hard clipping" };
    add_param (MODE, "Mode",  "M", std::move (centries), 0, "", "Saturation Function");

    add_param (MIX, "Mix dry/wet", "Mix", 0, 100, 100, "%");
    add_param (DRIVE, "Drive", "Drive", -6, 36, 0, "dB");
  }
  SaturationDSP::Mode
  map_mode (int m)
  {
    if (m == 2)
      return SaturationDSP::Mode::HARD_CLIP;
    if (m == 1)
      return SaturationDSP::Mode::TANH_TRUE;
    return SaturationDSP::Mode::TANH_TABLE;
  }
  void
  adjust_param (Id32 tag) override
  {
    switch (Params (tag.id))
      {
      case DRIVE:       dleft.set_factor (pow (2, get_param (DRIVE) / 6));
                        dright.set_factor (pow (2, get_param (DRIVE) / 6));
                        return;
      case MIX:         dleft.set_mix (get_param (MIX));
                        dright.set_mix (get_param (MIX));
                        return;
      case MODE:        dleft.set_mode (map_mode (get_param (MODE)));
                        dright.set_mode (map_mode (get_param (MODE)));
                        return;
      }
  }
  void
  reset (uint64 target_stamp) override
  {
    adjust_params (true);
  }
  void
  render (uint n_frames) override
  {
    adjust_params (false);
    float *input0 = const_cast<float*> (ifloats (stereoin, 0));
    float *input1 = const_cast<float*> (ifloats (stereoin, 1));
    float *output0 = oblock (stereout, 0);
    float *output1 = oblock (stereout, 1);
    dleft.process (input0, output0, n_frames);
    dright.process (input1, output1, n_frames);
  }
};
static auto saturation = register_audio_processor<Saturation> ("Ase::Devices::Saturation");

}
