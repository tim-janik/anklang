// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEVICE_HH__
#define __ASE_DEVICE_HH__

#include <ase/gadget.hh>
#include <ase/processor.hh>

namespace Ase {

class DeviceImpl : public GadgetImpl, public virtual Device {
  AudioProcessorP proc_;
  AudioComboP     combo_; // maybe null
  DeviceS         children_;
  DeviceInfo      info;
  ASE_DEFINE_MAKE_SHARED (DeviceImpl);
  friend class AudioProcessor;
  DeviceP         insert_device        (const String &uri, Device *sibling);
protected:
  virtual        ~DeviceImpl           ();
  void            serialize            (WritNode &xs) override;
  void            _set_parent          (Gadget *parent) override;
public:
  explicit        DeviceImpl           (const String &aseid, AudioProcessor::StaticInfo, AudioProcessorP);
  AudioComboP     audio_combo          () const         { return combo_; }
  DeviceInfo      device_info          () override      { return info; }
  PropertyP       access_property      (String ident) override;
  PropertyS       access_properties    () override;
  bool            is_combo_device      () override      { return combo_ != nullptr; }
  DeviceS         list_devices         () override;
  // Create sub Device
  DeviceInfoS     list_device_types    () override;
  void            remove_device        (Device &sub) override;
  DeviceP         append_device        (const String &uri) override;
  DeviceP         insert_device        (const String &uri, Device &beforesibling) override;
  AudioProcessorP _audio_processor     () const override { return proc_; }
  void            _set_event_source    (AudioProcessorP esource) override;
  void            _disconnect_remove   () override;
  static DeviceP  create_ase_device    (AudioEngine &engine, const String &registryuri);
};

DeviceP create_processor_device (AudioEngine &engine, const String &uri, bool engineproducer);

} // Ase

#endif // __ASE_DEVICE_HH__
