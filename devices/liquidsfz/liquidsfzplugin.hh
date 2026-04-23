// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first
#include "liquidsfz.hh"
#include <ase/cxxaux.hh>

namespace Ase
{

class LiquidSFZPlugin : public tracktion::Plugin
{
  class RTMutex
  {
    std::atomic_flag locked_flag = ATOMIC_FLAG_INIT;
  public:
    bool try_lock();
    void wait_for_lock();
    void unlock();
  };

  LiquidSFZ::Synth  synth_;
  RTMutex           rt_mutex_;
public:
  juce::String getName() const override               { return TRANS("LiquidSFZ"); }
  juce::String getPluginType() override               { return xmlTypeName; }
  juce::String getSelectableDescription() override    { return TRANS("LiquidSFZ"); }
  bool isSynth() override                             { return true; }
  bool needsConstantBufferSize() override             { return false; }
  bool takesMidiInput() override                      { return true; }

  static const char* xmlTypeName;

  LiquidSFZPlugin (tracktion::PluginCreationInfo info);
  void initialise (const tracktion::PluginInitialisationInfo& info) override;
  void deinitialise() override;
  void applyToBuffer (const tracktion::PluginRenderContext& fc) override;
  bool load (const String& filename);

};

}
