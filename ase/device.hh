// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/gadget.hh>

namespace Ase {

class DeviceImpl : public GadgetImpl, public virtual Device {
protected:
  explicit        DeviceImpl           () {} // abstract base
  virtual DeviceS list_devices         () { return {}; }
public:
  DeviceS         get_devices          () const override { return const_cast<DeviceImpl&> (*this).list_devices(); }
  void            set_devices          (const DeviceS &newdevices) override { emit_notify ("devices"); }
  bool            gui_supported        () override { return false; }
  bool            gui_visible          () override { return false; }
  void            gui_toggle           () override {}
  void            _disconnect_remove   () override;
  static DeviceInfo extract_info       (const String &aseid);
};

} // Ase

