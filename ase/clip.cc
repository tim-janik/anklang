// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clip.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace Ase {

// == ClipImpl ==
JSONIPC_INHERIT (ClipImpl, Clip);

ClipImpl::~ClipImpl()
{}

int32
ClipImpl::start_tick ()
{
  return ~0;
}

int32
ClipImpl::stop_tick ()
{
  return 0;
}

int32
ClipImpl::end_tick ()
{
  return 0;
}

void
ClipImpl::assign_range (int32 starttick, int32 stoptick)
{}

ClipNoteS
ClipImpl::list_all_notes ()
{
  return {};
}

int32
ClipImpl::change_note (int32 id, int32 tick, int32 duration, int32 key, int32 fine_tune, double velocity)
{
  return {};
}

} // Ase
