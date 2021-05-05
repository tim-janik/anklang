// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MONITOR_HH__
#define __ASE_MONITOR_HH__

#include <ase/gadget.hh>

namespace Ase {

class MonitorImpl : public GadgetImpl, public virtual Monitor {
  ASE_DEFINE_MAKE_SHARED (MonitorImpl);
  friend class TrackImpl;
  virtual ~MonitorImpl        ();
public:
  explicit MonitorImpl        ();
  DeviceP  get_output         () override;
  int32    get_ochannel       () override;
  int64    get_mix_freq       () override;
  int64    get_frame_duration () override;
};
using MonitorImplP = std::shared_ptr<MonitorImpl>;

} // Ase

#endif // __ASE_MONITOR_HH__
