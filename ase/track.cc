// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "track.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace Ase {

// == TrackImpl ==
JSONIPC_INHERIT (TrackImpl, Track);

TrackImpl::TrackImpl (bool masterflag) :
  masterflag_ (masterflag)
{}

TrackImpl::~TrackImpl()
{}

int32
TrackImpl::midi_channel () const
{
  return 0;
}

void
TrackImpl::midi_channel (int32 midichannel)
{}

} // Ase
