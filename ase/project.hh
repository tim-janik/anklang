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
  String project_dir_;
  bool discarded_ = false;
  friend class TrackImpl;
  friend class UndoScope;
  friend void test_audio_sample_load();
  UndoScope           add_undo_scope (const String &scopename);
protected:
  explicit            ProjectImpl     (const juce::ValueTree &state, const String &dir, bool new_project);
  virtual            ~ProjectImpl     ();
  void                foreach_track   (const std::function<bool(Track&,int)> &cb) const;
  String              grep_serialized (const String &regex, int group) override;
  void                deactivate_edit ();
  String              create_project_dir (const String &basename);
  void                project_dir_changed ();
public:
  void                 bpm              (double bpm) override;
  double               bpm              () const override;
  void                 numerator        (int num) override;
  int                  numerator        () const override;
  void                 denominator      (int den) override;
  int                  denominator      () const override;
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
  void                 length           (double) override;
  StringS              read_only_properties () const override;
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
  TrackS               all_tracks        () const override;
  void                 all_tracks        (const TrackS &) override;
  TrackP               master_track      () override;
  Error                save_project      (const String &utf8name) override;
  Error                save_project_as   (const String &utf8path) override;
  String               project_dir       () const override;
  void                 project_dir       (const String&) override;
  bool                 unnamed           () const override;
  void                 unnamed           (bool) override;
  Error                remove_project_dir () override;
  String               project_dir       (bool create);
  String               link_sample       (const String &file, bool &created);
  TelemetryFieldS      telemetry         () const override;
  static void          force_shutdown_all ();
  static ProjectImplP  create            (const String &projectname);
  static ProjectImplP  from_xml          (const String &xml, const String &dir = "");
  static Error         open_project      (const String &utf8path);
};
using ProjectImplP = std::shared_ptr<ProjectImpl>;

} // Ase

