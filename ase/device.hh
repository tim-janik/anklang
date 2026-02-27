// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEVICE_HH__
#define __ASE_DEVICE_HH__

#include <ase/gadget.hh>

namespace Ase {

class DeviceImpl : public GadgetImpl, public virtual Device {
  bool            activated_ = false;
protected:
  explicit        DeviceImpl           () {} // abstract base
  void            _set_parent          (GadgetImpl *parent) override;
  virtual DeviceS list_devices         () { return {}; }
public:
  void            _activate            () override;
  void            _deactivate          () override;
  DeviceS         get_devices          () const override { return const_cast<DeviceImpl&> (*this).list_devices(); }
  void            set_devices          (const DeviceS &newdevices) override { emit_notify ("devices"); }
  bool            is_active            () override { return activated_; }
  bool            gui_supported        () override { return false; }
  bool            gui_visible          () override { return false; }
  void            gui_toggle           () override {}
  void            _disconnect_remove   () override;
  static DeviceInfo extract_info       (const String &aseid);
};

} // Ase

#endif // __ASE_DEVICE_HH__
