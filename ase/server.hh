// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_SERVER_HH__
#define __ASE_SERVER_HH__

#include <ase/gadget.hh>

namespace Ase {

class ServerImpl : public virtual Server, public virtual GadgetImpl {
  Preferences  prefs_;
public:
  static ServerImplP instancep ();
  explicit     ServerImpl           ();
  virtual     ~ServerImpl           ();
  String       get_version          () override;
  String       get_vorbis_version   () override;
  String       get_mp3_version      () override;
  String       musical_tuning_blurb (MusicalTuning musicaltuning) override;
  String       musical_tuning_desc  (MusicalTuning musicaltuning) override;
  void         shutdown             () override;
  ProjectP     last_project         () override;
  ProjectP     create_project       (String projectname) override;
  PropertyS    access_prefs         () override;
  DriverEntryS list_pcm_drivers     () override;
  DriverEntryS list_midi_drivers    () override;
};

} // Ase

#endif // __ASE_SERVER_HH__
