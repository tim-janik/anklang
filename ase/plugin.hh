// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/trkn-utils.hh>
#include <ase/device.hh>

namespace Ase {

/// Ase::Plugin implementation wrapping tracktion::Plugin.
class PluginImpl : public DeviceImpl, public virtual Plugin {
  class PluginStateListener;
  std::unique_ptr<PluginStateListener> state_listener_;
  ASE_DEFINE_MAKE_SHARED (PluginImpl);
  friend class TrackImpl;
  friend class PluginStateListener;
protected:
  SelectableWeakref<tracktion::Plugin> plugin_;
  std::string                          plugin_type_;
  virtual        ~PluginImpl        ();
  String          fallback_name     () const override;
public:
  explicit        PluginImpl         (tracktion::Plugin &plugin);
  String          name              () const override;
  DeviceInfo      device_info       () override;
  String          plugin_type       () const override;
  bool            is_enabled        () const override;
  void            set_enabled       (bool enabled) override;
  bool            is_frozen         () const override;
  void            set_frozen        (bool frozen) override;
  void            remove_self       () override;
  tracktion::Plugin* plugin () const noexcept { return plugin_.get(); } ///< Access underlying tracktion::Plugin (for internal use).
  static PluginImplP from_trkn (tracktion::Plugin&);
};

} // Ase

