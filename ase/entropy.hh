// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENTROPY_HH__
#define __ASE_ENTROPY_HH__

#include <ase/cxxaux.hh>

namespace Ase {

/// Collect entropy from the current process, usually quicker than collect_system_entropy().
void collect_runtime_entropy (uint64 *data, size_t n);

/// Collect entropy from system devices, like interrupt counters, clocks and random devices.
void collect_system_entropy  (uint64 *data, size_t n);

} // Ase

#endif // __ASE_ENTROPY_HH__
