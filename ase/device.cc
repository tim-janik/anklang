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
void
DeviceImpl::_set_parent (GadgetImpl *parent)
{
  assert_warn (!is_active());
  GadgetImpl::_set_parent (parent);
}

void
DeviceImpl::_activate()
{
  assert_return (!activated_);
  activated_ = true;
}

void
DeviceImpl::_deactivate()
{
  assert_return (activated_);
  activated_ = false;
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

DeviceInfo
DeviceImpl::extract_info (const String &aseid, const AudioProcessor::StaticInfo &static_info)
{
  AudioProcessorInfo pinfo;
  static_info (pinfo);
  DeviceInfo info = {
    .uri          = aseid,
    .name         = pinfo.label,
    .category     = pinfo.category,
    .description  = pinfo.description,
    .website_url  = pinfo.website_url,
    .creator_name = pinfo.creator_name,
    .creator_url  = pinfo.creator_url,
  };
  return info;
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

DeviceInfo
extract_info (const String &aseid, const AudioProcessor::StaticInfo &static_info)
{
  AudioProcessorInfo pinfo;
  static_info (pinfo);
  DeviceInfo info = {
    .uri          = aseid,
    .name         = pinfo.label,
    .category     = pinfo.category,
    .description  = pinfo.description,
    .website_url  = pinfo.website_url,
    .creator_name = pinfo.creator_name,
    .creator_url  = pinfo.creator_url,
  };
  return info;
}

} // Ase
