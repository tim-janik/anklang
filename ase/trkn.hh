// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

#pragma once
#include <ase/cxxaux.hh>
#include <ase/../trkn/tracktion_decls.hh>

// Internal Trkn API
namespace Ase {

bool                trkn_init     (int argc, char *argv[], bool nodevs);
void                trkn_shutdown ();
tracktion::Engine*  trkn_engine   ();

} // Ase
