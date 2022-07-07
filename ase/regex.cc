// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "regex.hh"
#include "internal.hh"
#include <regex>

namespace Ase {

static inline constexpr std::regex_constants::syntax_option_type
regex_flags (Re::Flags flags, bool d = false)
{
  const bool ere = (flags & Re::ERE) == Re::ERE;
  auto o = ere ? std::regex::extended : std::regex::ECMAScript;
  if (!d)
    o |= std::regex::nosubs;    // () groups are always (?:)
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

/// Find `regex` in `input` and return non-overlapping matches.
StringS
Re::findall (const String &regex, const String &input, Flags flags)
{
  std::regex rex (regex, regex_flags (flags));
  std::sregex_iterator itb = std::sregex_iterator (input.begin(), input.end(), rex);
  std::sregex_iterator ite = std::sregex_iterator();
  StringS all;
  for (std::sregex_iterator it = itb; it != ite; ++it) {
    std::smatch match = *it;
    all.push_back (match.str());
  }
  return all;
}

/// Substitute `regex` in `input` with `subst` up to `count` times.
String
Re::subn (const String &regex, const String &subst, const String &input, uint count, Flags flags)
{
  const std::sregex_iterator end = std::sregex_iterator();
  std::regex rex (regex, regex_flags (flags));
  std::sregex_iterator matchiter = std::sregex_iterator (input.begin(), input.end(), rex);
  const size_t n = std::distance (matchiter, end); // number of matches
  return_unless (n, input);
  std::string result;
  auto out = std::back_inserter (result);
  std::sub_match<std::string::const_iterator> tail;
  for (std::sregex_iterator it = matchiter; it != end; it++)
    {
      // std::smatch match = *it;
      std::sub_match<std::string::const_iterator> prefix = it->prefix();
      out = std::copy (prefix.first, prefix.second, out);
      out = std::copy (subst.begin(), subst.end(), out);
      tail = it->suffix();
      if (count-- == 1)
        break;
    }
  out = std::copy (tail.first, tail.second, out);
  return result;
}

/// Substitute `regex` in `input` by `sbref` with backreferences `$00…$99` or `$&`.
String
Re::sub (const String &regex, const String &sbref, const String &input, Flags flags)
{
  std::regex rex (regex, regex_flags (flags, true));
  return std::regex_replace (input, rex, sbref);
}

} // Ase

#include "testing.hh"

namespace { // Anon
using namespace Ase;

TEST_INTEGRITY (regex_tests);
static void
regex_tests()
{
  ssize_t k;
  k = Re::search ("fail", "abc abc");                                   TCMP (k, ==, -1);
  k = Re::search (R"(\bb)", "abc bbc");                                 TCMP (k, ==, 4);
  k = Re::search (R"(\d\d?\b)", "a123 b");                              TCMP (k, ==, 2);
  String u, v;
  StringS ss;
  u = "abc abc abc Abc"; v = Re::sub ("xyz", "ABC", u);                  TCMP (v, ==, "abc abc abc Abc");
  u = "abc abc abc Abc"; v = Re::subn ("xyz", "ABC", u);                 TCMP (v, ==, "abc abc abc Abc");
  u = "abc abc abc Abc"; v = Re::sub ("abc", "ABC", u);                  TCMP (v, ==, "ABC ABC ABC Abc");
  u = "abc abc abc Abc"; v = Re::subn ("abc", "ABC", u);                 TCMP (v, ==, "ABC ABC ABC Abc");
  u = "abc abc abc Abc"; v = Re::subn ("abc", "ABC", u, 2);              TCMP (v, ==, "ABC ABC abc Abc");
  u = "abc abc abc Abc"; v = Re::subn ("abc", "ABC", u, 0, Re::I);       TCMP (v, ==, "ABC ABC ABC ABC");
  u = "abc abc abc Abc"; v = Re::sub (R"(\bA)", "-", u);                 TCMP (v, ==, "abc abc abc -bc");
  u = "abc abc abc Abc"; v = Re::subn (R"(\bA)", "-", u);                TCMP (v, ==, "abc abc abc -bc");
  u = "abc abc abc Abc"; v = Re::subn (R"(\bA\b)", "-", u);              TCMP (v, ==, "abc abc abc Abc");
  u = "a 1 0 2 b 3n 4 Z";  v = Re::sub (R"(([a-zA-Z]) ([0-9]+\b))", "$1$2", u);  TCMP (v, ==, "a1 0 2 b 3n4 Z");
  u = "abc 123 abc Abc"; ss = Re::findall (R"(\b\w)", u); TCMP (ss, ==, cstrings_to_vector ("a", "1", "a", "A", nullptr));
}

} // Anon
