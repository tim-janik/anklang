// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_TRACK_HH__
#define __ASE_TRACK_HH__

#include <ase/device.hh>

namespace Ase {

/// Ase::Track implementation.
class TrackImpl : public DeviceImpl, public virtual Track {
  DeviceP      chain_, midi_prod_;
  ClipImplS    clips_;
  ASE_DEFINE_MAKE_SHARED (TrackImpl);
  friend class ProjectImpl;
  virtual         ~TrackImpl        ();
protected:
  String          fallback_name     () const override;
  void            serialize         (WritNode &xs) override;
public:
  class ClipScout;
  explicit        TrackImpl         (ProjectImpl&, bool masterflag);
  void            _activate         () override;
  void            _deactivate       () override;
  AudioProcessorP _audio_processor  () const override;
  void            _set_event_source (AudioProcessorP esource) override;
  void            _set_parent       (GadgetImpl *parent) override;
  DeviceInfo      device_info       () override;
  ProjectImpl*    project           () const;
  bool            is_master         () const override      { return MASTER_TRACK & gadget_flags(); }
  int32           midi_channel      () const override;
  void            midi_channel      (int32 midichannel) override;
  ClipS           list_clips        () override;
  DeviceP         access_device     () override;
  MonitorP        create_monitor    (int32 ochannel) override;
  void            update_clips      ();
  ssize_t         clip_index        (const ClipImpl &clip) const;
  int             clip_succession   (const ClipImpl &clip) const;
  TelemetryFieldS telemetry         () const override;
  enum Cmd { STOP, START, };
  void            queue_cmd         (CallbackS&, Cmd cmd, double arg = 0);
  void            queue_cmd         (DCallbackS&, Cmd cmd);
  enum { NONE = -1 };
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
