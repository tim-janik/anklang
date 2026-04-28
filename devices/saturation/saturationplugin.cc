// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "saturationplugin.hh"
#include <ase/trkn-ids.hh>

/* TODO: this is currently using internal (unstable) API; may want to duplicate this */
#include "tracktion_engine/model/automation/modifiers/tracktion_ModifierInternal.h"

namespace Ase
{

namespace te = tracktion::engine;

juce::StringArray
SaturationPlugin::getModeNames()
{
  return { "Soft (tanh)", "Hard" };
}

SaturationPlugin::SaturationPlugin (tracktion::PluginCreationInfo info) : Plugin (info)
{
  auto um = getUndoManager();

  drive_value.referTo (state, IDs::drive, um, 0.0f);
  mix_value.referTo (state, IDs::mix, um, 100.0f);
  mode_value.referTo (state, IDs::mode, um, 0.f);

  drive = addParam ("drive", TRANS("Drive"),    { -6.0f, 36.0f },
                    [] (float value)            { return juce::String (value, 1) + " dB"; },
                    [] (const juce::String& s)  { return s.getFloatValue(); });
  mix   = addParam ("mix", TRANS("Mix"),        { 0.0f, 100.0f },
                    [] (float value)            { return juce::String (value, 1) + " %"; },
                    [] (const juce::String& s)  { return s.getFloatValue(); });
  mode  = te::createDiscreteParameter (*this, "mode",  TRANS("Mode"),
                                                { 0.0f, (float)  getModeNames().size() - 1 },
                                                mode_value, getModeNames());
  addAutomatableParameter (mode);


  drive->attachToCurrentValue (drive_value);
  mix->attachToCurrentValue (mix_value);
}

SaturationPlugin::~SaturationPlugin()
{
  notifyListenersOfDeletion();

  drive->detachFromCurrentValue();
  mix->detachFromCurrentValue();
  mode->detachFromCurrentValue();
}

void
SaturationPlugin::initialise (const tracktion::PluginInitialisationInfo& info)
{
  saturation.reset (info.sampleRate);

  updateParams (true);
}

void
SaturationPlugin::deinitialise()
{
}

void
SaturationPlugin::reset()
{
  saturation.reset (sampleRate);
}

void
SaturationPlugin::updateParams (bool now)
{
  saturation.set_drive (drive->getCurrentValue(), now);
  saturation.set_mix (mix->getCurrentValue(), now);

  switch (int (mode->getCurrentValue()))
    {
      case 0: saturation.set_mode (SaturationDSP::Mode::TANH_CHEAP);
              break;
      case 1: saturation.set_mode (SaturationDSP::Mode::HARD_CLIP);
              break;
    }
}

void
SaturationPlugin::applyToBuffer (const tracktion::PluginRenderContext& fc)
{
  updateParams (false);
  if (fc.destBuffer != nullptr)
    {
      if (fc.destBuffer->getNumChannels() >= 2)
        {
          tracktion::clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

          float *left = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);
          float *right = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);

          saturation.process<true> (left, right, left, right, fc.bufferNumSamples);
        }
      else
        {
          float *mono = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);

          saturation.process<false> (mono, nullptr, mono, nullptr, fc.bufferNumSamples);
        }
    }
}

void
SaturationPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
  te::copyPropertiesToCachedValues (v, drive_value, mix_value, mode_value);

  for (auto p : getAutomatableParameters())
    p->updateFromAttachedValue();
}

const char* SaturationPlugin::xmlTypeName = "saturation";

}

