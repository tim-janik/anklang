// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "regex.hh"
#include <regex>

namespace Ase {

static inline constexpr std::regex_constants::syntax_option_type
regex_flags (Re::Flags flags, bool d = false)
{
  const bool ere = (flags & Re::ERE) == Re::ERE;
  auto o = ere ? std::regex::extended : std::regex::ECMAScript;
  if (!d)
    o |= std::regex::nosubs;
  if (Re::I & flags)    o |= std::regex::icase;
  // if (Re::M & flags) o |= std::regex::multiline;
  // if (Re::S & flags) o |= std::regex::dotall;
  return o;
}

/// Find `regex` in `input` and return match position >= 0 or return < 0 otherwise.
ssize_t
Re::search (const String &regex, const String &input, Flags flags)
{
  std::regex rex (regex, regex_flags (flags));
  std::smatch m;
  if (std::regex_search (input, m, rex))
    return m.position();
  return -1;
}

/// Substitute `regex` in `input` with `subst`.
String
Re::sub (const String &regex, const String &subst, const String &input, Flags flags)
{
  std::regex rex (regex, regex_flags (flags));
  return std::regex_replace (input, rex, subst);
}

} // Ase
