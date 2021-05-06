// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROJECT_HH__
#define __ASE_PROJECT_HH__

#include <ase/track.hh>

namespace Ase {

class ProjectImpl : public GadgetImpl, public virtual Project {
  std::vector<TrackImplP> tracks_;
protected:
  void     serialize      (WritNode &xs) override;
public:
  explicit ProjectImpl    (const String &projectname);
  virtual ~ProjectImpl    ();
  void     destroy        () override;
  Error    save_dir       (const String &dir, bool selfcontained) override;
  void     start_playback () override;
  void     stop_playback  () override;
  bool     is_playing     () override;
  TrackP   create_track   () override;
  bool     remove_track   (Track &child) override;
  TrackS   list_tracks    () override;
  TrackP   master_track   () override;
  ssize_t  track_index    (const Track &child) const;
};
using ProjectImplP = std::shared_ptr<ProjectImpl>;

} // Ase

#endif // __ASE_PROJECT_HH__
