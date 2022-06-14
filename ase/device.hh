// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEVICE_HH__
#define __ASE_DEVICE_HH__

#include <ase/gadget.hh>

namespace Ase {

class DeviceImpl : public GadgetImpl, public virtual Device {
  AudioProcessorP proc_;
  AudioComboP     combo_; // maybe null
  ASE_DEFINE_MAKE_SHARED (DeviceImpl);
  friend class AudioProcessor;
  DeviceP         create_device_before (const String &uuiduri, Device *sibling);
protected:
  virtual        ~DeviceImpl           ();
  void            serialize            (WritNode &xs) override;
public:
  AudioComboP     audio_combo          () const          { return combo_; }
  explicit        DeviceImpl           (AudioProcessor &proc);
  DeviceInfo      device_info          () override;
  PropertyP       access_property      (String ident) override;
  PropertyS       access_properties    () override;
  bool            is_combo_device      () override      { return combo_ != nullptr; }
  DeviceS         list_devices         () override;
  // Create sub Device
  DeviceInfoS     list_device_types    () override;
  void            remove_device        (Device &sub) override;
  DeviceP         create_device        (const String &uuiduri) override;
  DeviceP         create_device_before (const String &uuiduri, Device &sibling) override;
  AudioProcessorP _audio_processor     () const override { return proc_; }
  void            _set_event_source    (AudioProcessorP esource) override;
  void            _disconnect_remove   () override;
  static DeviceP  create_output        (const String &uuiduri);
};

} // Ase

#endif // __ASE_DEVICE_HH__
