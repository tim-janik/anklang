// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_SERVER_HH__
#define __ASE_SERVER_HH__

#include <ase/gadget.hh>

namespace Ase {

class ServerImpl : public GadgetImpl, public virtual Server {
  Preferences  prefs_;
public:
  static ServerImplP instancep ();
  explicit     ServerImpl           ();
  virtual     ~ServerImpl           ();
  String       get_version          () override;
  String       get_vorbis_version   () override;
  String       get_mp3_version      () override;
  String       error_blurb          (Error error) const override;
  String       musical_tuning_blurb (MusicalTuning musicaltuning) const override;
  String       musical_tuning_desc  (MusicalTuning musicaltuning) const override;
  void         shutdown             () override;
  ProjectP     last_project         () override;
  ProjectP     create_project       (String projectname) override;
  PropertyS    access_prefs         () override;
  const Preferences& preferences    () const    { return prefs_; }
};

} // Ase

#endif // __ASE_SERVER_HH__
