// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_TRACK_HH__
#define __ASE_TRACK_HH__

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
  DeviceP      chain_, midi_prod_;
  ClipImplS    clips_;
  uint         midi_channel_ = 0;
  uint         is_folder_ : 1;
  ASE_DEFINE_MAKE_SHARED (TrackImpl);
  friend class ProjectImpl;
  virtual         ~TrackImpl        ();
protected:
  String          fallback_name     () const override;
  void            serialize         (WritNode &xs) override;
public:
  class ClipScout;
  explicit        TrackImpl         (ProjectImpl&, bool masterflag);
  explicit        TrackImpl         (tracktion::Track &track);
  bool            is_folder         () const    { return "Folder" == te_type_; }
  String          get_name          () const override;
  void            set_name          (const std::string &n) override;
  void            _activate         () override;
  void            _deactivate       () override;
  AudioProcessorP _audio_processor  () const override;
  void            _set_event_source (AudioProcessorP esource) override;
  void            _set_parent       (GadgetImpl *parent) override;
  DeviceInfo      device_info       () override;
  ProjectImpl*    project           () const;
  bool            is_master         () const override      { return MASTER_TRACK & gadget_flags(); }
  int32           midi_channel      () const override      { return midi_channel_; }
  void            midi_channel      (int32 midichannel) override;
  ClipS           launcher_clips    () override;
  DeviceP         access_device     () override;
  MonitorP        create_monitor    (int32 ochannel) override;
  ssize_t         clip_index        (const ClipImpl &clip) const;
  int             clip_succession   (const ClipImpl &clip) const;
  TelemetryFieldS telemetry         () const override;
  enum Cmd { STOP, START, };
  void            queue_cmd         (CallbackS&, Cmd cmd, double arg = 0);
  void            queue_cmd         (DCallbackS&, Cmd cmd);
  enum { NONE = -1 };
  ClipImplP       create_midi_clip  (const String &name, double start, double length);
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

#endif // __ASE_TRACK_HH__
