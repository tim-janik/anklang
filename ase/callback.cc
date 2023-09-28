// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "callback.hh"

#define CDEBUG(...)             Ase::debug ("callback", __VA_ARGS__)

namespace Ase {

// == JobQueue ==
JobQueue::JobQueue (const Caller &caller) :
  caller_ (caller)
{}

} // Ase
