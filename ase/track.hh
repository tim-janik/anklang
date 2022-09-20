// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_TRACK_HH__
#define __ASE_TRACK_HH__

#include <ase/gadget.hh>

namespace Ase {

/// Ase::Track implementation.
class TrackImpl : public GadgetImpl, public virtual Track {
  ProjectImpl *project_ = nullptr;
  DeviceP      chain_, midi_prod_;
  ClipImplS    clips_;
  const bool   masterflag_ = false;
  ASE_DEFINE_MAKE_SHARED (TrackImpl);
  friend class ProjectImpl;
  virtual ~TrackImpl    ();
  void     set_project    (ProjectImpl *project);
protected:
  String   fallback_name  () const override;
  void     serialize      (WritNode &xs) override;
  explicit TrackImpl      (bool masterflag);
public:
  class ClipScout;
  ProjectImpl*    project        () const               { return project_; }
  bool            is_master      () const override      { return masterflag_; }
  int32           midi_channel   () const override;
  void            midi_channel   (int32 midichannel) override;
  ClipS           list_clips     () override;
  DeviceP         access_device  () override;
  MonitorP        create_monitor (int32 ochannel) override;
  void            update_clips   ();
  ssize_t         clip_index     (const ClipImpl &clip) const;
  int             clip_succession (const ClipImpl &clip) const;
  TelemetryFieldS telemetry      () const override;
  enum Cmd { STOP, START, };
  void            queue_cmd      (CallbackS&, Cmd cmd, double arg = 0);
  void            queue_cmd      (DCallbackS&, Cmd cmd);
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
