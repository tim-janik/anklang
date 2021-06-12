// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLIP_HH__
#define __ASE_CLIP_HH__

#include <ase/gadget.hh>

namespace Ase {

class ClipImpl : public GadgetImpl, public virtual Clip {
protected:
  virtual ~ClipImpl   ();
public:
  int32     start_tick     () override;
  int32     stop_tick      () override;
  int32     end_tick       () override;
  void      assign_range   (int32 starttick, int32 stoptick) override;
  ClipNoteS list_all_notes () override;
  int32     change_note    (int32 id, int32 tick, int32 duration, int32 key, int32 fine_tune, double velocity) override;
};

} // Ase

#endif // __ASE_CLIP_HH__
