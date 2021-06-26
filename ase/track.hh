// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_TRACK_HH__
#define __ASE_TRACK_HH__

#include <ase/gadget.hh>

namespace Ase {

class TrackImpl : public GadgetImpl, public virtual Track {
  const bool masterflag_ = false;
  ProjectImpl *project_ = nullptr;
  DeviceImplP  chain_;
  ClipImplS    clips_;
  ASE_DEFINE_MAKE_SHARED (TrackImpl);
  friend class ProjectImpl;
  virtual ~TrackImpl    ();
  void     set_project    (ProjectImpl *project);
protected:
  String   fallback_name  () const override;
  void     serialize      (WritNode &xs) override;
  explicit TrackImpl      (bool masterflag);
public:
  bool     is_master      () const override       { return masterflag_; }
  int32    midi_channel   () const override;
  void     midi_channel   (int32 midichannel) override;
  ClipS    list_clips     () override;
  DeviceP  access_device  () override;
  MonitorP create_monitor (int32 ochannel) override;
  void     update_clip    ();
  ssize_t  clip_index     (const ClipImpl &clip) const;
};

} // Ase

#endif // __ASE_TRACK_HH__
