// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <type_traits>
#include <clocale>
#include <cstdint>
#include <sstream>
#include <variant>
#include <string>
#include <list>

namespace Ase {

/// Format a string similar to sprintf(3) with support for std::string and std::ostringstream convertible objects.
template<class ...Args> std::string     string_format (const char *format, const Args &...args) __attribute__ ((__format__ (__printf__, 1, 0)));

/// Class to push the POSIX/C locale_t (UTF-8) for the scope of its lifetime.
class ScopedPosixLocale final {
  locale_t saved_locale_ = {};
  void operator= (const ScopedPosixLocale&) = delete;
  ScopedPosixLocale (const ScopedPosixLocale&) = delete;
public:
  explicit ScopedPosixLocale ();
  virtual ~ScopedPosixLocale ();
  static locale_t posix_locale();
};

// == Implementation Details ==
namespace Impl {
using StringFormatVariant = std::variant<uint64_t,double,const char*>;
struct StringFormatArg : StringFormatVariant {
  using SL = std::list<std::string>; // use list to keep c_str() references stable
  using StringFormatVariant::operator=;
  void        assign (const char *s, SL &h)             { *this = s; }
  void        assign (const std::string &s, SL &h)      { *this = s.c_str(); }
  void        assign (std::nullptr_t, SL &h)            { *this = uint64_t (0); }
  void        assign (const void *p, SL &h)             { *this = uint64_t (ptrdiff_t (p)); }
  template<class T> typename std::enable_if<std::is_integral_v<T> || std::is_enum_v<T>, void>
  ::type      assign (const T &v, SL &h)                { *this = uint64_t (v); }
  template<class T> typename std::enable_if<std::is_floating_point_v<T>, void>
  ::type      assign (const T &v, SL &h)                { *this = double (v); }
  template<class T> typename std::enable_if<std::is_class<T>::value, void>
  ::type      assign (const T &o, SL &h) { std::ostringstream s; s << o; h.push_back (s.str()); *this = h.back().c_str(); }
  static std::string string_format_args (const char *format, size_t N, const StringFormatArg *args);
};
} // Impl

/** Format a string according to an sprintf() `format` string with `arguments`.
 * Refer to sprintf(3) for the format string details, this function is designed to
 * serve as an sprintf() replacement and mimick its behaviour as close as possible.
 * Supported format directive features are:
 * - Formatting flags (sign conversion, padding, alignment), i.e. the flags: [-#0+ ']
 * - Field width and precision specifications.
 * - Positional arguments for field width, precision and value.
 * - Length modifiers are tolerated: i.e. any of [hlLjztqZ].
 * - The conversion specifiers [spmcCdiouXxFfGgEeAa].
 *
 * @NOTE Format errors, e.g. missing arguments will produce a warning on stderr and
 * return the `format` string unmodified.
 * @returns A formatted string.
 */
template<class ...Args> __attribute__ ((noinline)) std::string
string_format (const char *format, const Args &...args)
{
  constexpr size_t N = sizeof... (Args);
  Impl::StringFormatArg sfa[N ? N : 1];
  size_t i = 0;
  Impl::StringFormatArg::SL templist; // keep temporary strings across string_format_args()
  (sfa[i++].assign (args, templist), ...);  // C++17 fold expression
  return sfa[0].string_format_args (format, N, sfa);
}

} // Ase

