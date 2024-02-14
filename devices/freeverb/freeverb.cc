// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "ase/processor.hh"
#include "ase/internal.hh"
#include "devices/blepsynth/linearsmooth.hh"

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
  LinearSmooth mix_smooth_;
  bool mix_smooth_reset_ = false;
public:
  Freeverb (const ProcessorSetup &psetup) :
    AudioProcessor (psetup)
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
  enum Params { MODE = 1, MIX, ROOMSIZE, DAMPING, WIDTH };
  void
  initialize (SpeakerArrangement busses) override
  {
    remove_all_buses();

    stereoin = add_input_bus  ("Stereo In",  SpeakerArrangement::STEREO);
    stereout = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);

    ParameterMap pmap;

    pmap.group = _("Reverb Settings");
    pmap[MIX] = Param { "mix",   "Mix dry/wet", "Mix", 30, "%", { 0, 100 } };

    ChoiceS centries;
    centries += { "Signflip 2000",  _("Preserve May 2000 Freeverb damping sign flip") };
    centries += { "VLC Damping",    _("The VLC Freeverb version disables one damping feedback chain") };
    centries += { "Normal Damping", _("Damping with sign correction as implemented in STK Freeverb") };
    pmap[MODE] = Param ("mode", _("Mode"), _("Mode"), 2, "", std::move (centries), "",
                        { String ("blurb=") + _("Damping mode found in different Freeverb variants"), });

    pmap.group = _("Room Settings");
    pmap[ROOMSIZE] = Param ("roomsize", _("Room size"), _("RS"), offsetroom + scaleroom * initialroom, _("size"), { offsetroom, offsetroom + scaleroom });
    pmap[WIDTH]    = Param ("width", _("Width"), _("W"), 100 * initialwidth, "%", { 0, 100 });
    pmap[DAMPING]  = Param ("damping", _("Damping"), _("D"), 100 * initialdamp, "%", { 0, 100 });

    install_params (pmap);
  }
  void
  adjust_param (uint32_t paramid) override
  {
    switch (Params (paramid))
      {
      case MIX:         mix_smooth_.set (get_param (paramid) * 0.01, mix_smooth_reset_);
                        mix_smooth_reset_ = false;
                        break;
      case ROOMSIZE:    return model.setroomsize ((get_param (paramid) - offsetroom) / scaleroom);
      case WIDTH:       return model.setwidth (0.01 * get_param (paramid));
      case MODE:
      case DAMPING:     return model.setdamp (0.01 * get_param (ParamId (DAMPING)),
                                              1 - irintf (get_param (ParamId (MODE))));
      }
  }
  void
  reset (uint64 target_stamp) override
  {
    model.setmode (0);          // no-freeze, allow mute
    model.setdry (0);           // no dry, we mix the output signal during render
    model.setwet (1);           // only need reverb wet
    model.mute();               // silence internal buffers
    mix_smooth_.reset (sample_rate(), 0.020);
    mix_smooth_reset_ = true;
    adjust_all_params();
  }
  void
  render (uint n_frames) override
  {
    apply_input_events();
    float *input0 = const_cast<float*> (ifloats (stereoin, 0));
    float *input1 = const_cast<float*> (ifloats (stereoin, 1));
    float *output0 = oblock (stereout, 0);
    float *output1 = oblock (stereout, 1);
    // this only generates the wet signal
    model.processreplace (input0, input1, output0, output1, n_frames, 1);

    if (mix_smooth_.is_constant()) // faster version: mix parameter is constant during the block
      {
        float mix = mix_smooth_.get_next();
        for (uint i = 0; i < n_frames; i++)
          {
            output0[i] = input0[i] * (1 - mix) + output0[i] * mix;
            output1[i] = input1[i] * (1 - mix) + output1[i] * mix;
          }
      }
    else
      {
        for (uint i = 0; i < n_frames; i++)
          {
            float mix = mix_smooth_.get_next();
            output0[i] = input0[i] * (1 - mix) + output0[i] * mix;
            output1[i] = input1[i] * (1 - mix) + output1[i] * mix;
          }
      }
  }
};
static auto freeverb = register_audio_processor<Freeverb> ("Ase::Devices::Freeverb");

} // Anon
