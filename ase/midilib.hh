// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MIDILIB_HH__
#define __ASE_MIDILIB_HH__

#include <ase/processor.hh>
#include <ase/track.hh>
#include <ase/clip.hh>

namespace Ase {

namespace MidiLib {

/// Aggregation of MIDI events and sequencing information.
struct MidiFeed {
  ClipImplGeneratorS generators;
  TrackImpl::ClipScout scout;
  int trigger = ~0;
};
using MidiFeedP = std::shared_ptr<MidiFeed>;

class MidiProducerIface : public AudioProcessor {
public:
  struct Position {
    int    next = -1;
    int    current = -1;
    double tick = -1;
  };
  virtual void      update_feed (MidiFeedP &feed) = 0;
  virtual Position* position    () const = 0; // MT-Safe
  virtual void      start       () = 0;
  virtual void      stop        (bool restart = false) = 0;
  MidiProducerIface (AudioEngine &engine) : AudioProcessor (engine) {}
};

using MidiProducerIfaceP = std::shared_ptr<MidiProducerIface>;

} // MidiLib
} // Ase

#endif // __ASE_MIDILIB_HH__

