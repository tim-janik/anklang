// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "monitor.hh"
#include "track.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace Ase {

// == MonitorImpl ==
JSONIPC_INHERIT (MonitorImpl, Monitor);

MonitorImpl::MonitorImpl()
{}

MonitorImpl::~MonitorImpl()
{}

DeviceP
MonitorImpl::get_output () // TODO: implement
{
  return nullptr;
}

int32
MonitorImpl::get_ochannel () // TODO: implement
{
  return -1;
}

int64
MonitorImpl::get_mix_freq () // TODO: implement
{
  return 0;
}

int64
MonitorImpl::get_frame_duration () // TODO: implement
{
  return 0;
}

} // Ase
