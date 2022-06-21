// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLAP_DEVICE_HH__
#define __ASE_CLAP_DEVICE_HH__

#include <ase/device.hh>

namespace Ase {

class ClapDeviceImpl : public GadgetImpl, public virtual Device {
  ASE_DEFINE_MAKE_SHARED (ClapDeviceImpl);
  ClapPluginHandleP handle_;
protected:
  virtual           ~ClapDeviceImpl        ();
  void              _set_parent            (Gadget *parent) override;
public:
  explicit           ClapDeviceImpl        (ClapPluginHandleP claphandle);
  static DeviceInfoS list_clap_plugins     ();
  DeviceInfo         device_info           () override;
  bool               is_combo_device       () override                       { return false; }
  DeviceS            list_devices          () override                       { return {}; } // no children
  DeviceInfoS        list_device_types     () override                       { return {}; } // no children
  void               remove_device         (Device &sub) override            {} // no children
  DeviceP            append_device         (const String&) override          { return {}; }
  DeviceP            insert_device         (const String&, Device&) override { return {}; }
  AudioProcessorP    _audio_processor      () const override;
  void               _set_event_source     (AudioProcessorP esource) override;
  void               _disconnect_remove    () override;
  static String      clap_version          ();
  static DeviceP     create_clap_device    (AudioEngine &engine, const String &clapuri);
  static ClapPluginHandleP access_clap_handle (DeviceP device);
};

} // Ase

#endif // __ASE_CLAP_DEVICE_HH__