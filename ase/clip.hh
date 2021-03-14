// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLIP_HH__
#define __ASE_CLIP_HH__

#include <ase/gadget.hh>

namespace Ase {

class ClipImpl : public virtual Clip, public virtual GadgetImpl {
protected:
  virtual ~ClipImpl   ();
public:
  int32    start_tick () override;
  int32    stop_tick  () override;
  int32    end_tick   () override;
};

} // Ase

#endif // __ASE_CLIP_HH__
