// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLAP_DEVICE_HH__
#define __ASE_CLAP_DEVICE_HH__

#include <ase/device.hh>

namespace Ase {

class ClapDeviceImpl : public GadgetImpl, public virtual Device {
  ASE_DEFINE_MAKE_SHARED (ClapDeviceImpl);
protected:
  virtual           ~ClapDeviceImpl       ();
public:
  explicit           ClapDeviceImpl       ();
  static DeviceInfoS list_clap_plugins    ();
  DeviceInfo         device_info          () override;
  bool               is_combo_device      () override;
  DeviceS            list_devices         () override;
  DeviceInfoS        list_device_types    () override;
  void               remove_device        (Device &sub) override;
  DeviceP            create_device        (const String &uuiduri) override;
  DeviceP            create_device_before (const String &uuiduri,
                                           Device &sibling) override;
};

} // Ase

#endif // __ASE_CLAP_DEVICE_HH__
