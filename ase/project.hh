// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROJECT_HH__
#define __ASE_PROJECT_HH__

#include <ase/gadget.hh>

namespace Ase {

struct ProjectImpl : public virtual Project, public virtual GadgetImpl {
  explicit ProjectImpl    (const String &projectname);
  virtual ~ProjectImpl    ();
  void     destroy        () override;
  void     start_playback () override;
  void     stop_playback  () override;
  bool     is_playing     () override;
};
using ProjectImplP = std::shared_ptr<ProjectImpl>;

} // Ase

#endif // __ASE_PROJECT_HH__
