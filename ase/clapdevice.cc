// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapdevice.hh"
#include "clapplugin.hh"
#include "jsonipc/jsonipc.hh"
#include "processor.hh"
#include "path.hh"
#include "main.hh"
#include "internal.hh"
#include "gtk2wrap.hh"
#include <dlfcn.h>

#define CDEBUG(...)     Ase::debug ("Clap", __VA_ARGS__)
#define CDEBUG_ENABLED() Ase::debug_key_enabled ("Clap")

namespace Ase {

// == ClapDeviceImpl ==
JSONIPC_INHERIT (ClapDeviceImpl, Device);

ClapDeviceImpl::ClapDeviceImpl (ClapPluginHandleP claphandle) :
  handle_ (claphandle)
{
  assert_return (claphandle != nullptr);
}

ClapDeviceImpl::~ClapDeviceImpl()
{
  if (handle_)
    handle_->destroy();
  handle_ = nullptr;
}

DeviceInfo
ClapDeviceImpl::device_info ()
{
  return clap_device_info (handle_->descriptor);
}

void
ClapDeviceImpl::_set_parent (Gadget *parent)
{
  GadgetImpl::_set_parent (parent);
  ClapDeviceImplP selfp = shared_ptr_cast<ClapDeviceImpl> (this);
  // activate plugin, show GUI, start processing
  if (parent && handle_->activate())
    handle_->show_gui();
  // deactivate and destroy plugin
  if (!parent && handle_)
    {
      handle_->destroy_gui();
      handle_->deactivate();
      handle_->destroy();
    }
}

DeviceInfoS
ClapDeviceImpl::list_clap_plugins ()
{
  static DeviceInfoS devs;
  if (devs.size())
    return devs;
  for (ClapPluginDescriptor *descriptor : ClapPluginDescriptor::collect_descriptors()) {
    std::string title = descriptor->name; // FIXME
    if (!descriptor->version.empty())
      title = title + " " + descriptor->version;
    if (!descriptor->vendor.empty())
      title = title + " - " + descriptor->vendor;
    devs.push_back (clap_device_info (*descriptor));
  }
  return devs;
}

AudioProcessorP
ClapDeviceImpl::_audio_processor () const
{
  return handle_->audio_processor();
}

void
ClapDeviceImpl::_set_event_source (AudioProcessorP esource)
{
  // FIXME: implement
}

void
ClapDeviceImpl::_disconnect_remove ()
{
  // FIXME: implement
}

String
ClapDeviceImpl::clap_version ()
{
  const String clapversion = string_format ("%u.%u.%u", CLAP_VERSION.major, CLAP_VERSION.minor, CLAP_VERSION.revision);
  return clapversion;
}

ClapPluginHandleP
ClapDeviceImpl::access_clap_handle (DeviceP device)
{
  ClapDeviceImpl *clapdevice = dynamic_cast<ClapDeviceImpl*> (&*device);
  return clapdevice ? clapdevice->handle_ : nullptr;
}

DeviceP
ClapDeviceImpl::create_clap_device (AudioEngine &engine, const String &clapuri)
{
  assert_return (string_startswith (clapuri, "CLAP:"), nullptr);
  const String clapid = clapuri.substr (5);
  ClapPluginDescriptor *descriptor = nullptr;
  for (ClapPluginDescriptor *desc : ClapPluginDescriptor::collect_descriptors())
    if (clapid == desc->id) {
      descriptor = desc;
      break;
    }
  if (descriptor)
    {
      auto make_clap_device = [&descriptor] (const String &aseid, AudioProcessor::StaticInfo static_info, AudioProcessorP aproc) -> DeviceP {
        ClapPluginHandleP handlep = ClapPluginHandle::make_clap_handle (*descriptor, aproc);
        return ClapDeviceImpl::make_shared (handlep);
      };
      DeviceP devicep = AudioProcessor::registry_create (ClapPluginHandle::audio_processor_type(), engine, make_clap_device);
      ClapDeviceImplP clapdevicep = shared_ptr_cast<ClapDeviceImpl> (devicep);
      return clapdevicep;
    }
  return nullptr;
}

} // Ase
