// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first
#include "saturationdsp.hh"
#include <ase/cxxaux.hh>

namespace Ase
{

class SaturationPlugin : public tracktion::Plugin
{
  SaturationDSP saturation;

  juce::CachedValue<float> drive_value;
  juce::CachedValue<float> mix_value;
  juce::CachedValue<float> mode_value;

  tracktion::AutomatableParameter::Ptr drive;
  tracktion::AutomatableParameter::Ptr mix;
  tracktion::AutomatableParameter::Ptr mode;

  juce::StringArray getModeNames();
  void updateParams (bool now);
public:
  SaturationPlugin (tracktion::PluginCreationInfo info);
  ~SaturationPlugin() override;

  static const char *getPluginName()                  { return NEEDS_TRANS("Saturation"); }
  static const char* xmlTypeName;

  juce::String getName() const override               { return TRANS("Saturation"); }
  juce::String getPluginType() override               { return xmlTypeName; }
  juce::String getSelectableDescription() override    { return TRANS("Saturation Plugin"); }
  bool needsConstantBufferSize() override             { return false; }

  int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }

  void initialise (const tracktion::PluginInitialisationInfo& info) override;
  void deinitialise() override;
  void reset() override;
  void applyToBuffer (const tracktion::PluginRenderContext& fc) override;

  void restorePluginStateFromValueTree (const juce::ValueTree&) override;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturationPlugin)
};

}
