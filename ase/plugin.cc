// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "plugin.hh"
#include "server.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"
#include "devices/liquidsfz/liquidsfzplugin.hh"

namespace te = tracktion::engine;

namespace Ase {

// == PluginStateListener ==
class PluginImpl::PluginStateListener : public juce::ValueTree::Listener {
  PluginImpl &aseplugin_;
  juce::ValueTree plugin_state_;
public:
  PluginStateListener (PluginImpl &aseplugin) :
    aseplugin_ (aseplugin), plugin_state_ (aseplugin_.plugin_->state)
  {
    plugin_state_.addListener (this);
  }
  ~PluginStateListener() override
  {
    plugin_state_.removeListener (this);
  }
  void
  valueTreePropertyChanged (juce::ValueTree &tree, const juce::Identifier &property) override
  {
    assert_return (tree == plugin_state_);
    if (property == tracktion::engine::IDs::name)
      aseplugin_.emit_notify ("name");
    else if (property == tracktion::engine::IDs::enabled)
      aseplugin_.emit_notify ("enabled");
    else if (property == tracktion::engine::IDs::frozen)
      aseplugin_.emit_notify ("frozen");
  }
  void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
  void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
  void valueTreeParentChanged (juce::ValueTree&) override {}
  void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
};

// == PluginImpl ==
PluginImplP
PluginImpl::from_trkn (tracktion::Plugin &p)
{
  PluginImpl *plugin = find_ase_obj<PluginImpl> (p);
  if (plugin)
    return shared_ptr_cast<PluginImpl> (plugin);
  if (auto impl = LiquidSFZPluginImpl::wrap (p))
    return impl;
  PluginImplP pluginp = PluginImpl::make_shared (p);
  return pluginp;
}

PluginImpl::PluginImpl (tracktion::Plugin &plugin) :
  plugin_ (&plugin), plugin_type_ (plugin.getPluginType().toStdString())
{
  register_ase_obj (this, plugin);
  state_listener_ = std::make_unique<PluginStateListener> (*this);
}

PluginImpl::~PluginImpl()
{
  unregister_ase_obj (this, plugin_.get());
  state_listener_ = nullptr;
  assert_return (_parent() == nullptr);
}

String
PluginImpl::name() const
{
  if (auto pluginp = plugin_.get())
    return pluginp->getName().toStdString();
  return "";
}

String
PluginImpl::fallback_name() const
{
  if (auto pluginp = plugin_.get())
    return pluginp->getName().toStdString();
  return DeviceImpl::fallback_name();
}

String
PluginImpl::plugin_type() const
{
  return plugin_type_;
}

bool
PluginImpl::is_enabled() const
{
  if (auto pluginp = plugin_.get())
    return pluginp->isEnabled();
  return false;
}

void
PluginImpl::set_enabled (bool enabled)
{
  if (auto pluginp = plugin_.get())
    pluginp->setEnabled (enabled);
}

bool
PluginImpl::is_frozen() const
{
  if (auto pluginp = plugin_.get())
    return pluginp->isFrozen();
  return false;
}

void
PluginImpl::set_frozen (bool frozen)
{
  if (auto pluginp = plugin_.get())
    pluginp->setFrozen (frozen);
}

DeviceInfo
PluginImpl::device_info()
{
  DeviceInfo info;
  if (auto pluginp = plugin_.get()) {
    info.uri = pluginp->getIdentifierString().toStdString();
    info.name = pluginp->getName().toStdString();
    info.description = pluginp->getTooltip().toStdString();
  }
  return info;
}

void
PluginImpl::remove_self ()
{
  auto *plugin = plugin_.get();
  if (plugin) {
    plugin->removeFromParent();
    plugin_ = SelectableWeakref<tracktion::Plugin>{};
    state_listener_ = nullptr;
  }
  GadgetImpl::remove_self();
}

} // Ase
