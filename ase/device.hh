// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEVICE_HH__
#define __ASE_DEVICE_HH__

#include <ase/gadget.hh>

namespace Ase {

class DeviceImpl : public virtual GadgetImpl, public virtual Device {
  AudioProcessorP proc_;
  AudioComboP     combo_; // maybe null
  ASE_DEFINE_MAKE_SHARED (DeviceImpl);
  friend class AudioProcessor;
protected:
  virtual        ~DeviceImpl        ();
public:
  explicit        DeviceImpl        (AudioProcessor &proc);
  DeviceInfo      device_info       () override;
  StringS         list_properties   () override;
  PropertyP       access_property   (String ident) override;
  PropertyS       access_properties () override;
  AudioProcessorP audio_processor   () const       { return proc_; }
  AudioComboP     audio_combo       () const       { return combo_; }
};


} // Ase

#endif // __ASE_DEVICE_HH__
