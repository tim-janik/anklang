// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROJECT_HH__
#define __ASE_PROJECT_HH__

#include <ase/device.hh>
#include <ase/track.hh>
#include <ase/member.hh>
#include <ase/transport.hh>

namespace Ase {

class UndoScope {
  ProjectImplP projectp_;
  String scopename_;
  friend class ProjectImpl;
  explicit  UndoScope  (ProjectImplP projectp, const String &scopename);
public:
  /*copy*/  UndoScope  (const UndoScope&) = delete;
  UndoScope& operator= (const UndoScope&) = delete;
  /*dtor*/ ~UndoScope  ();
};

class ProjectImpl final : public DeviceImpl, public virtual Project {
  class TransportListener;
  std::unique_ptr<tracktion::Edit> edit_;
  std::unique_ptr<TransportListener> transport_listener_;
  std::vector<TrackImplP> tracks_;
  ASE_DEFINE_MAKE_SHARED (ProjectImpl);
  MusicalTuning musical_tuning_ = MusicalTuning::OD_12_TET;
  struct PStorage;
  PStorage *storage_ = nullptr;
  String saved_filename_;
  bool discarded_ = false;
  friend class UndoScope;
  UndoScope           add_undo_scope (const String &scopename);
protected:
  explicit            ProjectImpl     ();
  virtual            ~ProjectImpl     ();
  void                foreach_track   (const std::function<bool(Track&,int)> &cb);
  void                serialize       (WritNode &xs) override;
  void                update_tempo    ();
  Error               snapshot_project (String &json);
  String              match_serialized (const String &regex, int group) override;
  void                deactivate_edit ();
public:
  void                 set_bpm           (double bpm) override;
  double               get_bpm           () const override;
  void                 set_numerator     (double num) override;
  double               get_numerator     () const override;
  void                 set_denominator   (double den) override;
  double               get_denominator   () const override;
  String               get_name          () const override;
  void                 set_name          (const std::string &n) override;
  void                 _activate         () override;
  void                 _deactivate       () override;
  void                 discard           () override;
  AudioProcessorP      _audio_processor  () const override;
  void                 _set_event_source (AudioProcessorP esource) override;
  DeviceInfo           device_info       () override;
  UndoScope            undo_scope        (const String &scopename);
  void                 undo              () override;
  bool                 can_undo          () override;
  void                 redo              () override;
  bool                 can_redo          () override;
  double               get_length        () const override;
  double               get_master_volume () const override;
  void                 set_master_volume (double db) override;
  void                 group_undo        (const String &undoname) override;
  void                 ungroup_undo      () override;
  void                 clear_undo        ();
  void                 start_playback    (double autostop);
  void                 start_playback    () override    { start_playback (D64MAX); }
  void                 pause_playback    () override;
  void                 stop_playback     () override;
  bool                 is_playing        () const override;
  void                 is_playing        (bool play) override;
  TrackP               create_track      () override;
  bool                 remove_track      (Track &child) override;
  TrackS               all_tracks        () override;
  TrackP               master_track      () override;
  Error                load_project      (const String &utf8filename) override;
  StreamReaderP        load_blob         (const String &fspath);
  String               loader_resolve    (const String &hexhash);
  Error                save_project      (const String &utf8filename, bool collect) override;
  String               saved_filename    () override; // returns utf8filename
  String               writer_file_name  (const String &fspath) const;
  Error                writer_add_file   (const String &fspath);
  Error                writer_collect    (const String &fspath, String *hexhashp);
  TelemetryFieldS      telemetry         () const override;
  AudioProcessorP      master_processor  () const;
  ssize_t              track_index       (const Track &child) const;
  int64_t              bar_ticks         () const;
  static void          force_shutdown_all ();
  static ProjectImplP  create            (const String &projectname);
};
using ProjectImplP = std::shared_ptr<ProjectImpl>;

} // Ase

#endif // __ASE_PROJECT_HH__
