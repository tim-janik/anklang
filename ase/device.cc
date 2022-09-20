// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "device.hh"
#include "nativedevice.hh"
#include "combo.hh"
#include "project.hh"
#include "jsonipc/jsonipc.hh"
#include "serialize.hh"
#include "internal.hh"

namespace Ase {

// == DeviceImpl ==
JSONIPC_INHERIT (DeviceImpl, Device);

void
DeviceImpl::_activate ()
{
  if (!activated_)
    activated_ = true;
}

template<typename E> std::pair<std::shared_ptr<E>,ssize_t>
find_shared_by_ref (const std::vector<std::shared_ptr<E> > &v, const E &e)
{
  for (ssize_t i = 0; i < v.size(); i++)
    if (&e == &*v[i])
      return std::make_pair (v[i], i);
  return std::make_pair (std::shared_ptr<E>{}, -1);
}

void
DeviceImpl::_disconnect_remove ()
{
  AudioProcessorP proc = _audio_processor();
  return_unless (proc);
  AudioEngine *engine = &proc->engine();
  auto j = [proc] () {
    proc->enable_engine_output (false);
    proc->disconnect_ibuses();
    proc->disconnect_obuses();
    proc->disconnect_event_input();
    // FIXME: remove from combo container if child
  };
  engine->async_jobs += j;
}

// == Device ==
void
Device::remove_self ()
{
  Gadget *parent = _parent();
  NativeDevice *device = dynamic_cast<NativeDevice*> (parent);
  if (device)
    device->remove_device (*this);
}

Track*
Device::_track () const
{
  for (Gadget *parent = _parent(); parent; parent = parent->_parent())
    {
      Track *track = dynamic_cast<Track*> (parent);
      if (track)
        return track;
    }
  return nullptr;
}

} // Ase
