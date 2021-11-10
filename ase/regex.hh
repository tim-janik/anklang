// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_REGEX_HH__
#define __ASE_REGEX_HH__

#include <ase/cxxaux.hh>

namespace Ase {

/// Wrapper for std::regex to simplify usage and reduce compilation time
class Re final {
public:
  enum Flags : int32_t {
    DEFAULT     = 0,
    ERE         = 0x01, // Posix Extended
    I           = 0x10, // IGNORECASE
    // M        = 0x20, // MULTILINE
    // S        = 0, DOTALL not supported, use [\s\S] or [^\x0]
  };
  static ssize_t search (const String &regex, const String &input, Flags = DEFAULT);
  static String  sub    (const String &regex, const String &subst, const String &input, uint count = 0, Flags = DEFAULT);
};
extern constexpr inline Re::Flags operator| (Re::Flags a, Re::Flags b) { return Re::Flags (int32_t (a) | int32_t (b)); }

extern constexpr inline class Re Re = {};

} // Ase

#endif // __ASE_REGEX_HH__
