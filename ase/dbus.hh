// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DBUS_HH__
#define __ASE_DBUS_HH__

#include <ase/defs.hh>

namespace Ase::DBus {

String  rtkit_make_high_priority (pid_t thread, int nice_level);
int     rtkit_get_min_nice_level ();

} // Ase::DBus

#endif // __ASE_DBUS_HH__
