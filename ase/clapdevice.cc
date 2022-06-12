// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapdevice.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace Ase {

// == ClapDeviceImpl ==
JSONIPC_INHERIT (ClapDeviceImpl, Device);

ClapDeviceImpl::ClapDeviceImpl ()
{}

ClapDeviceImpl::~ClapDeviceImpl()
{}

DeviceInfoS
ClapDeviceImpl::list_clap_plugins ()
{
  DeviceInfoS devs;
  return devs;
}

DeviceInfo
ClapDeviceImpl::device_info ()
{
  return {};
}

bool
ClapDeviceImpl::is_combo_device ()
{
  return false;
}

DeviceS
ClapDeviceImpl::list_devices ()
{
  return {};
}

DeviceInfoS
ClapDeviceImpl::list_device_types ()
{
  return {}; // FIXME: empty?
}

void
ClapDeviceImpl::remove_device (Device &sub)
{
}

DeviceP
ClapDeviceImpl::create_device (const String &uuiduri)
{
  return {};
}

DeviceP
ClapDeviceImpl::create_device_before (const String &uuiduri, Device &sibling)
{
  return {};
}

} // Ase
