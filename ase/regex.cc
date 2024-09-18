// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "regex.hh"
#include "logging.hh"
#include "internal.hh"
#include <regex>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

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

static pcre2_compile_context*
pcre2compilecontext ()
{
  static pcre2_compile_context *ccontext = [] {
    pcre2_compile_context *ccontext = pcre2_compile_context_create (nullptr);
    pcre2_set_compile_extra_options (ccontext, PCRE2_EXTRA_ALT_BSUX);   // \u{abcdef} (ECMAScript 6)
    pcre2_set_bsr (ccontext, PCRE2_BSR_UNICODE);
    pcre2_set_newline (ccontext, PCRE2_NEWLINE_ANY);
    return ccontext;
    // pcre2_compile_context_free (ccontext);
  } ();
  return ccontext;
}

/// Find `regex` in `input` and return matching string.
String
Re::grep (const String &regex, const String &input, int group, Flags flags)
{
  pcre2_compile_context *const ccontext = pcre2compilecontext();
  int errorcode = 0;
  size_t erroroffset = -1;
  // use PCRE2_NO_UTF_CHECK if regex is validated
  const uint32_t COMPILE_OPTIONS =
    0
    | PCRE2_UTF                 // UTF-8 Unicode mode
    | PCRE2_UCP                 // Unicode properties for \d \s \w
    | (flags & Re::I ? PCRE2_CASELESS : 0)
    | (flags & Re::M ? PCRE2_MULTILINE : 0)
    | (flags & Re::N ? PCRE2_NO_AUTO_CAPTURE : 0)
    | (flags & Re::S ? PCRE2_CASELESS : 0)
    | (flags & Re::X ? PCRE2_EXTENDED : 0)      // allows #comments\n
    | (flags & Re::XX ? PCRE2_EXTENDED_MORE : 0)
    | (flags & Re::J ? PCRE2_DUPNAMES : 0)
    | (flags & Re::U ? PCRE2_UNGREEDY : 0)
    | PCRE2_ALT_BSUX            // allow \x22 \u4444
    | PCRE2_NEVER_BACKSLASH_C;  // prevent matching point in the middle of UTF-8
  pcre2_code *rx = pcre2_compile ((const uint8_t*) regex.c_str(), PCRE2_ZERO_TERMINATED, COMPILE_OPTIONS, &errorcode, &erroroffset, ccontext);
  if (!rx) {
    logex ("Re", "failed to compile regex (%d): %s", errorcode, regex);
    return "";
  }
  pcre2_match_data *md = pcre2_match_data_create_from_pattern (rx, NULL);
  const size_t length = PCRE2_ZERO_TERMINATED;
  const size_t startoffset = 0; // in code units
  pcre2_match_context *mcontext = nullptr;
  const uint32_t MATCH_OPTIONS =
    0; // PCRE2_ANCHORED PCRE2_ENDANCHORED PCRE2_NOTEMPTY etc
  const int ret = pcre2_match (rx, (const uint8_t*) input.c_str(), length, startoffset, MATCH_OPTIONS, md, mcontext);
  String result;
  if (ret >= 0) {
    const size_t *ovector = pcre2_get_ovector_pointer (md);
    const uint32_t ovecs = pcre2_get_ovector_count (md);
    if (group < 0)
      group = uint (-group) < ovecs ? uint (-group) : 0;
    if (group < ovecs) {
      const size_t start = ovector[group*2], end = ovector[group*2+1];
      result.assign (&input[0] + start, &input[0] + end);
    }
  }
  pcre2_match_data_free (md); md = nullptr;
  pcre2_code_free (rx); rx = nullptr;
  return result;
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
