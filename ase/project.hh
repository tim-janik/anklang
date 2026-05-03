// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

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
  ASE_DEFINE_MAKE_SHARED (ProjectImpl);
  struct PStorage;
  PStorage *storage_ = nullptr;
  String saved_filename_;
  bool discarded_ = false;
  friend class TrackImpl;
  friend class UndoScope;
  friend void test_audio_sample_load();
  UndoScope           add_undo_scope (const String &scopename);
protected:
  explicit            ProjectImpl     ();
  virtual            ~ProjectImpl     ();
  void                foreach_track   (const std::function<bool(Track&,int)> &cb);
  Error               snapshot_project (String &json);
  String              match_serialized (const String &regex, int group) override;
  void                deactivate_edit ();
public:
  void                 bpm              (double bpm) override;
  double               bpm              () const override;
  void                 numerator        (double num) override;
  double               numerator        () const override;
  void                 denominator      (double den) override;
  double               denominator      () const override;
  String               name             () const override;
  void                 name             (const std::string &n) override;
  void                 discard           () override;
  DeviceInfo           device_info       () override;
  UndoScope            undo_scope        (const String &scopename);
  void                 undo              () override;
  bool                 can_undo          () override;
  void                 redo              () override;
  bool                 can_redo          () override;
  double               length           () const override;
  double               master_volume    () const override;
  void                 master_volume    (double db) override;
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
  void                 remove_self       () override;
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
  ssize_t              track_index       (const Track &child) const;
  int64_t              bar_ticks         () const;
  static void          force_shutdown_all ();
  static ProjectImplP  create            (const String &projectname);
};
using ProjectImplP = std::shared_ptr<ProjectImpl>;

} // Ase

