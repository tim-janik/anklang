// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEVICE_HH__
#define __ASE_DEVICE_HH__

#include <ase/gadget.hh>
#include <ase/processor.hh>

namespace Ase {

class DeviceImpl : public GadgetImpl, public virtual Device {
  bool            activated_ = false;
protected:
  explicit        DeviceImpl           () {} // abstract base
  void            _set_parent          (GadgetImpl *parent) override;
  void            _activate            () override;
  void            _deactivate          () override;
  bool            is_active            () override { return activated_; }
public:
  bool            gui_supported        () override { return false; }
  bool            gui_visible          () override { return false; }
  void            gui_toggle           () override {}
  void            _disconnect_remove   () override;
  static DeviceInfo extract_info       (const String &aseid, const AudioProcessor::StaticInfo &static_info);
};

} // Ase

#endif // __ASE_DEVICE_HH__
