// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "regex.hh"
#include "logging.hh"
#include "internal.hh"
#include <regex>
#include <cstring>

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

// use PCRE2_NO_UTF_CHECK if regex is validated
static uint32_t
flags_to_pcre2_compile_options (Re::Flags flags)
{
  uint32_t o =
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
  return o;
}

struct PcRe2 {
  pcre2_code *prcode = nullptr;
  int errorcode = 0;
  explicit
  PcRe2 (const std::string &pattern, Re::Flags flags)
  {
    pcre2_compile_context *const ccontext = pcre2compilecontext();
    size_t erroroffset = -1;
    prcode = pcre2_compile ((const uint8_t*) pattern.c_str(), PCRE2_ZERO_TERMINATED, flags_to_pcre2_compile_options (flags), &errorcode, &erroroffset, ccontext);
    if (!prcode)
      dprintf (2, "Re: failed to compile regex (%d): %s", errorcode, pattern.c_str());
  }
  ~PcRe2()
  {
    pcre2_code_free (prcode);
  }
  std::string
  sub (const std::string &substitution, const std::string &input, ssize_t maxsubst = SSIZE_MAX)
  {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern (prcode, nullptr);
    pcre2_match_context *mc = pcre2_match_context_create (nullptr);
    const uint32_t MATCH_OPTIONS =
      PCRE2_SUBSTITUTE_OVERFLOW_LENGTH |
      PCRE2_SUBSTITUTE_GLOBAL |
      0; // PCRE2_ANCHORED PCRE2_ENDANCHORED PCRE2_NOTEMPTY etc
    struct CalloutData {
      ssize_t max_substitutions = SSIZE_MAX;
    } callout_data;
    callout_data.max_substitutions = maxsubst;
    auto callout_function = [] (pcre2_substitute_callout_block*, void *callout_data_ptr) -> int {
      CalloutData &callout_data = *(CalloutData*) callout_data_ptr;
      return callout_data.max_substitutions-- >= 1 ? 0 : -1;
    };
    if (callout_data.max_substitutions < SSIZE_MAX)
      pcre2_set_substitute_callout (mc, callout_function, &callout_data);
    std::string result (input.size() + 4096, 0);
    PCRE2_SIZE outlength = result.size() - 1;
    int ret = pcre2_substitute (prcode, (const uint8_t*) input.c_str(), PCRE2_ZERO_TERMINATED, 0 /*startoffset*/, MATCH_OPTIONS, md, mc,
                                (const uint8_t*) substitution.c_str(), PCRE2_ZERO_TERMINATED, (uint8_t*) result.data(), &outlength);
    if (ret == PCRE2_ERROR_NOMEMORY) {
      result.resize (outlength + 128);
      ret = pcre2_substitute (prcode, (const uint8_t*) input.c_str(), PCRE2_ZERO_TERMINATED, 0 /*startoffset*/, MATCH_OPTIONS, md, mc,
                              (const uint8_t*) substitution.c_str(), PCRE2_ZERO_TERMINATED, (uint8_t*) result.data(), &outlength);
    }
    result.resize (strlen (result.data()));
    pcre2_match_data_free (md); md = nullptr;
    pcre2_match_context_free (mc); mc = nullptr;
    return result;
  }
};

/// Find `regex` in `input` and return matching string.
String
Re::grep (const String &regex, const String &input, int group, Flags flags)
{
  pcre2_compile_context *const ccontext = pcre2compilecontext();
  int errorcode = 0;
  size_t erroroffset = -1;
  pcre2_code *rx = pcre2_compile ((const uint8_t*) regex.c_str(), PCRE2_ZERO_TERMINATED, flags_to_pcre2_compile_options (flags), &errorcode, &erroroffset, ccontext);
  if (!rx) {
    logex ("Re: failed to compile regex (%d): %s", errorcode, regex);
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
Re::sub (const String &regex, const String &subst, const String &input, uint count, Flags flags)
{
  PcRe2 rx (regex, flags);
  return rx.sub (subst, input, count);
}

/// Substitute `regex` in `input` by `sbref` with backreferences `$00…$99` or `$&`.
String
Re::sub (const String &regex, const String &subst, const String &input, Flags flags)
{
  PcRe2 rx (regex, flags);
  return rx.sub (subst, input);
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
  u = "abc abc abc Abc"; v = Re::sub ("xyz", "ABC", u, 2);               TCMP (v, ==, "abc abc abc Abc");
  u = "abc abc abc Abc"; v = Re::sub ("abc", "ABC", u);                  TCMP (v, ==, "ABC ABC ABC Abc");
  u = "abc abc abc Abc"; v = Re::sub ("abc", "ABC", u, 2);               TCMP (v, ==, "ABC ABC abc Abc");
  u = "abc abc abc Abc"; v = Re::sub ("abc", "ABC", u, 999);             TCMP (v, ==, "ABC ABC ABC Abc");
  u = "abc abc abc Abc"; v = Re::sub ("abc", "ABC", u, 4, Re::I);        TCMP (v, ==, "ABC ABC ABC ABC");
  u = "abc abc abc Abc"; v = Re::sub (R"(\bA)", "-", u);                 TCMP (v, ==, "abc abc abc -bc");
  u = "abc abc abc Abc"; v = Re::sub (R"(\ba)", "-", u, 1);              TCMP (v, ==, "-bc abc abc Abc");
  u = "abc abc abc Abc"; v = Re::sub (R"(\bA\b)", "-", u);               TCMP (v, ==, "abc abc abc Abc");
  u = "a 1 0 2 b 3n 4 Z"; v = Re::sub (R"(([a-zA-Z]) ([0-9]+\b))", "$1$2", u);  TCMP (v, ==, "a1 0 2 b 3n4 Z");
  u = "abc 123 abc Abc"; ss = Re::findall (R"(\b\w)", u); TCMP (ss, ==, cstrings_to_vector ("a", "1", "a", "A", nullptr));
}

} // Anon
