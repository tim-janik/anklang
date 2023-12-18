// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_LV2_DEVICE_HH__
#define __ASE_LV2_DEVICE_HH__

#include <ase/device.hh>

namespace Ase
{

class LV2DeviceImpl : public DeviceImpl
{
  AudioProcessorP const proc_;
  DeviceInfo      const info_;
  ASE_DEFINE_MAKE_SHARED (LV2DeviceImpl);
  explicit           LV2DeviceImpl     (const String &lv2_uri, AudioProcessorP);
protected:
  void               serialize         (WritNode &xs) override;
public:
  AudioProcessorP    _audio_processor  () const override                  { return proc_; }
  void               _set_event_source (AudioProcessorP esource) override { /* FIXME: implement */ }
  DeviceInfo         device_info       () override                        { return info_; }
  bool               gui_supported     () override;
  void               gui_toggle        () override;
  PropertyS          access_properties () override;
  String             get_device_path   ();

  static DeviceInfoS list_lv2_plugins  ();
  static DeviceP     create_lv2_device (AudioEngine &engine, const String &lv2_uri_with_prefix);
};

} // Ase

#endif // __ASE_LV2_DEVICE_HH__
