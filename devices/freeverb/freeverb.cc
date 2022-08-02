// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/internal.hh"

namespace {

#include "revmodel.hpp"
#include "revmodel.cpp"
#include "allpass.cpp"
#include "comb.cpp"

using namespace Ase;

class Freeverb : public AudioProcessor {
  IBusId stereoin;
  OBusId stereout;
  revmodel model;
public:
  Freeverb (AudioEngine &engine) :
    AudioProcessor (engine)
  {}
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.version = "1";
    info.label = "Freeverb3";
    info.category = "Reverb";
    info.website_url = "https://beast.testbit.eu";
    info.creator_name = "Jezar at Dreampoint";
  }
  enum Params { MODE = 1, DRY, WET, ROOMSIZE, DAMPING, WIDTH };
  void
  initialize (SpeakerArrangement busses) override
  {
    remove_all_buses();

    stereoin = add_input_bus  ("Stereo In",  SpeakerArrangement::STEREO);
    stereout = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);

    start_group ("Reverb Settings");
    add_param (DRY, "Dry level",  "Dry", 0, scaledry, scaledry * initialdry, "dB");

    add_param (WET, "Wet level",  "Wet", 0, scalewet, scalewet * initialwet, "dB");

    start_group ("Room Settings");
    add_param (ROOMSIZE, "Room size",  "RS", offsetroom, offsetroom + scaleroom, offsetroom + scaleroom * initialroom, "size");

    add_param (WIDTH, "Width",  "W", 0, 100, 100 * initialwidth, "%");

    add_param (DAMPING, "Damping",  "D", 0, 100, 100 * initialdamp, "%");

    ChoiceS centries;
    centries += { "Signflip 2000",  "Preserve May 2000 Freeverb damping sign flip" };
    centries += { "VLC Damping",    "The VLC Freeverb version disables one damping feedback chain" };
    centries += { "Normal Damping", "Damping with sign correction as implemented in STK Freeverb" };
    add_param (MODE, "Mode",  "M", std::move (centries), 2, "", "Damping mode found in different Freeverb variants");
  }
  void
  adjust_param (Id32 tag) override
  {
    switch (Params (tag.id))
      {
      case WET:         return model.setwet (get_param (tag) / scalewet);
      case DRY:         return model.setdry (get_param (tag) / scaledry);
      case ROOMSIZE:    return model.setroomsize ((get_param (tag) - offsetroom) / scaleroom);
      case WIDTH:       return model.setwidth (0.01 * get_param (tag));
      case MODE:
      case DAMPING:     return model.setdamp (0.01 * get_param (ParamId (DAMPING)),
                                              1 - irintf (get_param (ParamId (MODE))));
      }
  }
  void
  reset (uint64 target_stamp) override
  {
    model.setmode (0);          // no-freeze, allow mute
    model.mute();               // silence internal buffers
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
    model.processreplace (input0, input1, output0, output1, n_frames, 1);
  }
};
static auto freeverb = register_audio_processor<Freeverb> ("Ase::Devices::Freeverb");

} // Anon
