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
  explicit           LV2DeviceImpl     (const String &lv2_uri, AudioProcessor::StaticInfo, AudioProcessorP);
public:
  AudioProcessorP    _audio_processor  () const override            { return proc_; }
  void               _set_event_source (AudioProcessorP esource)    { /* FIXME: implement */ }
  DeviceInfo         device_info       () override                  { return info_; }

  static DeviceInfoS list_lv2_plugins  ();
  static DeviceP     create_lv2_device (AudioEngine &engine, const String &clapuri);
};

} // Ase

#endif // __ASE_CLAP_DEVICE_HH__
