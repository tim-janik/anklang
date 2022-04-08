// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROJECT_HH__
#define __ASE_PROJECT_HH__

#include <ase/track.hh>
#include <ase/transport.hh>

namespace Ase {

class UndoScope {
  ProjectImplP projectp_;
  friend class ProjectImpl;
  explicit  UndoScope  (ProjectImplP projectp);
public:
  /*copy*/  UndoScope  (const UndoScope&);
  /*dtor*/ ~UndoScope  ();
  void      operator+= (const VoidF &func);
};

class ProjectImpl : public GadgetImpl, public virtual Project {
  std::vector<TrackImplP> tracks_;
  ASE_DEFINE_MAKE_SHARED (ProjectImpl);
  TickSignature tick_sig_;
  MusicalTuning musical_tuning_ = MusicalTuning::OD_12_TET;
  uint autoplay_timer_ = 0;
  uint undo_scopes_open_ = 0;
  uint undo_groups_open_ = 0;
  String undo_group_name_;
  struct UndoFunc { VoidF func; String name; };
  std::vector<UndoFunc> undostack_, redostack_;
  friend class UndoScope;
  UndoScope           add_undo_scope (const String &scopename);
protected:
  explicit            ProjectImpl    ();
  virtual            ~ProjectImpl    ();
  void                serialize      (WritNode &xs) override;
  void                update_tempo   ();
public:
  PropertyS            access_properties () override;
  const TickSignature& signature         () const       { return tick_sig_; }
  void                 destroy           () override;
  UndoScope            undo_scope        (const String &scopename);
  void                 push_undo         (const VoidF &func);
  void                 undo              () override;
  bool                 can_undo          () override;
  void                 redo              () override;
  bool                 can_redo          () override;
  void                 group_undo        (const String &undoname) override;
  void                 ungroup_undo      () override;
  void                 clear_undo        ();
  Error                save_dir          (const String &dir, bool selfcontained) override;
  void                 start_playback    () override;
  void                 stop_playback     () override;
  bool                 set_bpm           (double bpm);
  bool                 set_numerator     (uint8 numerator);
  bool                 set_denominator   (uint8 denominator);
  bool                 is_playing        () override;
  TrackP               create_track      () override;
  bool                 remove_track      (Track &child) override;
  TrackS               list_tracks       () override;
  TrackP               master_track      () override;
  Error                load_project      (const String &filename) override;
  TelemetryFieldS      telemetry         () const override;
  AudioProcessorP      master_processor  () const;
  ssize_t              track_index       (const Track &child) const;
  static ProjectImplP  create            (const String &projectname);
};
using ProjectImplP = std::shared_ptr<ProjectImpl>;

} // Ase

#endif // __ASE_PROJECT_HH__
