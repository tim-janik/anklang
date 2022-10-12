// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_NATIVE_DEVICE_HH__
#define __ASE_NATIVE_DEVICE_HH__

#include <ase/device.hh>
#include <ase/processor.hh>

namespace Ase {

class NativeDeviceImpl : public DeviceImpl, public virtual NativeDevice {
  AudioProcessorP const proc_;
  AudioComboP     combo_; // maybe null
  DeviceS         children_;
  DeviceInfo      const info_;
  ASE_DEFINE_MAKE_SHARED (NativeDeviceImpl);
  using DeviceFunc = std::function<void (DeviceP)>;
  DeviceP              insert_device      (const String &uri, Device *sibling, const DeviceFunc &loader);
protected:
  static DeviceP       create_native_device (AudioEngine &engine, const String &registryuri);
  void                 serialize          (WritNode &xs) override;
  void                 _set_parent        (GadgetImpl *parent) override;
  void                 _activate          () override;
  void                 _deactivate        () override;
  explicit             NativeDeviceImpl   (const String &aseid, AudioProcessor::StaticInfo, AudioProcessorP);
public:
  PropertyS            access_properties  () override;
  AudioProcessorP      _audio_processor   () const override { return proc_; }
  AudioComboP          audio_combo        () const          { return combo_; }
  bool                 is_combo_device    () override       { return combo_ != nullptr; }
  DeviceInfo           device_info        () override       { return info_; }
  // handle sub Devices
  DeviceS              list_devices       () override      { return children_; }
  void                 remove_device      (Device &sub) override;
  DeviceP              append_device      (const String &uri) override;
  DeviceP              insert_device      (const String &uri, Device &beforesibling) override;
  void                 remove_all_devices ();
  void                 _set_event_source  (AudioProcessorP esource) override;
  void                 _disconnect_remove () override;
};

DeviceP create_processor_device (AudioEngine &engine, const String &uri, bool engineproducer);

} // Ase

#endif // __ASE_NATIVE_DEVICE_HH__
