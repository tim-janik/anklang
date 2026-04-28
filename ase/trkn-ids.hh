// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once
#include "trkn/tracktion.hh"   // PCH include must come first

namespace Ase {

namespace IDs
{
  #define DECLARE_ID(name)  const juce::Identifier name (#name);

  DECLARE_ID (drive)
  DECLARE_ID (mix)
  DECLARE_ID (mode)
}

}
