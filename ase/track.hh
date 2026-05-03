// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/trkn-utils.hh>
#include <ase/device.hh>

namespace Ase {

/// Ase::Track implementation.
class TrackImpl : public DeviceImpl, public virtual Track {
  class TrackStateListener;
  ProjectImpl *project_ = nullptr;
  SelectableWeakref<tracktion::Track> track_;
  std::unique_ptr<TrackStateListener> state_listener_;
  std::string  te_type_;
  uint         midi_channel_ = 0;
  ASE_DEFINE_MAKE_SHARED (TrackImpl);
  friend class ProjectImpl;
  friend class TrackStateListener;
  virtual         ~TrackImpl        ();
protected:
  String          fallback_name     () const override;
  void            update_telemetry  ();
public:
  class ClipScout;
  explicit        TrackImpl         (ProjectImpl&, bool masterflag);
  explicit        TrackImpl         (tracktion::Track &track);
  bool            is_folder         () const    { return "Folder" == te_type_; }
  String          name              () const override;
  void            name              (const std::string &n) override;
  DeviceInfo      device_info       () override;
  ProjectImpl*    project           () const;
  bool            is_master         () const override;
  bool            is_muted          () const override;
  void            set_muted         (bool muted) override;
  bool            is_hidden         () const override;
  void            set_hidden        (bool hidden) override;
  bool            is_solo           () const override;
  void            set_solo          (bool solo) override;
  double          volume            () const override;
  void            volume            (double db) override;
  double          pan               () const override;
  void            pan               (double pan) override;
  void            remove_self       () override;
  int32           midi_channel      () const override      { return midi_channel_; }
  void            midi_channel      (int32 midichannel) override;
  ClipS           launcher_clips    () override;
  DeviceP         access_device     () override;
  MonitorP        create_monitor    (int32 ochannel) override;
  ssize_t         clip_index        (const ClipImpl &clip) const;
  int             clip_succession   (const ClipImpl &clip) const;
  TelemetryFieldS telemetry         () const override;
  enum { NONE = -1 };
  ClipP           create_midi_clip  (const String &name, double start, double length) override;
  ClipP           create_audio_clip (const String &name, double start, double length) override;
  PluginP         create_plugin     (const String &type) override;
  PluginS         list_plugins      () override;
  static TrackImplP from_trkn (tracktion::Track&);
};

/// MIDI clip playback succession generator.
class TrackImpl::ClipScout {
  friend class TrackImpl;
  std::vector<int> indices_;
  int last_ = -1;
public:
  enum { NONE = TrackImpl::NONE, };
  // constructors
  explicit ClipScout () noexcept;
  void     setup     (const std::vector<int> &indices);
  int      advance   (int previous);
  void     update    (const ClipScout &other);
  void     reset     ();
};

} // Ase

