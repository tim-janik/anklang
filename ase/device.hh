// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/gadget.hh>

namespace Ase {

class DeviceImpl : public GadgetImpl, public virtual Device {
protected:
  explicit        DeviceImpl           () {} // abstract base
};

} // Ase

