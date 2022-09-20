// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEVICE_HH__
#define __ASE_DEVICE_HH__

#include <ase/gadget.hh>
#include <ase/processor.hh>

namespace Ase {

class DeviceImpl : public GadgetImpl, public virtual Device {
  bool            activated_ = false;
protected:
  void            _activate            () override;
  explicit        DeviceImpl           () {}
  bool            is_active            () { return activated_; }
public:
  bool            gui_supported        () override { return false; }
  bool            gui_visible          () override { return false; }
  void            gui_toggle           () override {}
  void            _disconnect_remove   () override;
};

} // Ase

#endif // __ASE_DEVICE_HH__
