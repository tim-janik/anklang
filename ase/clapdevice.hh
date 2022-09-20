// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLAP_DEVICE_HH__
#define __ASE_CLAP_DEVICE_HH__

#include <ase/device.hh>

namespace Ase {

class ClapDeviceImpl : public DeviceImpl {
  ASE_DEFINE_MAKE_SHARED (ClapDeviceImpl);
  ClapPluginHandleP handle_;
  Connection        paramschange_;
protected:
  virtual           ~ClapDeviceImpl        ();
  void               serialize             (WritNode &xs) override;
  void               proc_params_change    (const Event &event);
  void               _set_parent           (Gadget *parent) override;
  void               _activate             () override;
public:
  explicit           ClapDeviceImpl        (ClapPluginHandleP claphandle);
  static DeviceInfoS list_clap_plugins     ();
  DeviceInfo         device_info           () override;
  PropertyS          access_properties     () override;
  void               gui_toggle            () override;
  bool               gui_supported         () override;
  bool               gui_visible           () override;
  AudioProcessorP    _audio_processor      () const override;
  void               _set_event_source     (AudioProcessorP esource) override;
  void               _disconnect_remove    () override;
  String             get_device_path       ();
  static String      clap_version          ();
  static DeviceP     create_clap_device    (AudioEngine &engine, const String &clapuri);
  static ClapPluginHandleP access_clap_handle (DeviceP device);
  friend struct ClapPropertyImpl;
};

} // Ase

#endif // __ASE_CLAP_DEVICE_HH__
