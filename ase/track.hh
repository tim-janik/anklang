// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_TRACK_HH__
#define __ASE_TRACK_HH__

#include <ase/object.hh>

namespace Ase {

class TrackImpl : public virtual Track, public virtual ObjectImpl {
protected:
  virtual ~TrackImpl    ();
public:
  int32    midi_channel () const override;
  void     midi_channel (int32 midichannel) override;
};

} // Ase

#endif // __ASE_TRACK_HH__
