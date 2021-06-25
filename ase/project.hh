// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROJECT_HH__
#define __ASE_PROJECT_HH__

#include <ase/track.hh>

namespace Ase {

class ProjectImpl : public GadgetImpl, public virtual Project {
  std::vector<TrackImplP> tracks_;
  ASE_DEFINE_MAKE_SHARED (ProjectImpl);
protected:
  explicit            ProjectImpl    ();
  virtual            ~ProjectImpl    ();
  void                serialize      (WritNode &xs) override;
public:
  void                destroy        () override;
  Error               save_dir       (const String &dir, bool selfcontained) override;
  void                start_playback () override;
  void                stop_playback  () override;
  bool                is_playing     () override;
  TrackP              create_track   () override;
  bool                remove_track   (Track &child) override;
  TrackS              list_tracks    () override;
  TrackP              master_track   () override;
  Error               load_project   (const String &filename) override;
  TelemetryFieldS     telemetry      () const override;
  AudioProcessorP     master_processor () const;
  ssize_t             track_index    (const Track &child) const;
  static ProjectImplP create         (const String &projectname);
};
using ProjectImplP = std::shared_ptr<ProjectImpl>;

} // Ase

#endif // __ASE_PROJECT_HH__
