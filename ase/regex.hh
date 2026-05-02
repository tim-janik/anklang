// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/cxxaux.hh>

namespace Ase {

/// Wrapper for std::regex to simplify usage and reduce compilation time
class Re final {
public:
  enum Flags : int32_t {
    DEFAULT     = 0,
    ERE         = 1 <<  0, // Posix Extended
    I           = 1 <<  4, // IGNORECASE
    M           = 1 <<  5, // MULTILINE
    N           = 1 <<  6, // NO_AUTO_CAPTURE
    S           = 1 <<  7, // DOTALL
    X           = 1 <<  8, // EXTENDED
    XX          = 1 <<  9, // EXTENDED_MORE
    J           = 1 << 10, // DUPNAMES
    U           = 1 << 11, // UNGREEDY
  };
  static StringS findall (const String &regex, const String &input, Flags = DEFAULT);
  static ssize_t search  (const String &regex, const String &input, Flags = DEFAULT);
  static String  grep    (const String &regex, const String &input, int group = 0, Flags = DEFAULT);
  static String  sub     (const String &regex, const String &subst, const String &input, Flags = DEFAULT);
  static String  sub     (const String &regex, const String &subst, const String &input, uint count, Flags = DEFAULT);
};
extern constexpr inline Re::Flags operator| (Re::Flags a, Re::Flags b) { return Re::Flags (int32_t (a) | int32_t (b)); }

} // Ase

