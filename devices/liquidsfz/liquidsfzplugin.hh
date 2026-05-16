// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first
#include "liquidsfz.hh"
#include <ase/cxxaux.hh>
#include <ase/plugin.hh>

namespace Ase
{

class LiquidSFZTracktionPlugin : public tracktion::Plugin
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

  LiquidSFZTracktionPlugin (tracktion::PluginCreationInfo info);
  void initialise (const tracktion::PluginInitialisationInfo& info) override;
  void deinitialise() override;
  void applyToBuffer (const tracktion::PluginRenderContext& fc) override;
  bool load (const String& filename);

};

class LiquidSFZPluginImpl : public PluginImpl, public virtual LiquidSFZPlugin
{
  ASE_DEFINE_MAKE_SHARED (LiquidSFZPluginImpl);
public:
  LiquidSFZPluginImpl (tracktion::Plugin& plugin) :
    PluginImpl (plugin)
  {
  }
  static LiquidSFZPluginImplP
  wrap (tracktion::Plugin& plugin)
  {
    if (dynamic_cast <LiquidSFZTracktionPlugin *> (&plugin))
      return LiquidSFZPluginImpl::make_shared (plugin);
    return nullptr;
  }
  void
  load (const String& filename)
  {
    /* TODO: this needs to be asynchronous and have some way to report result */
    if (auto p = dynamic_cast<LiquidSFZTracktionPlugin *> (plugin_.get()))
      p->load (filename);
  }
};

}
