// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_COMPRESS_HH__
#define __ASE_COMPRESS_HH__

#include <ase/cxxaux.hh>

namespace Ase {

String compress      (const String &input);
String uncompress    (const String &input);
bool   is_compressed (const String &input);

} // Ase

#endif  // __ASE_COMPRESS_HH__
