// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "regex.hh"
#include "logging.hh"
#include "internal.hh"
#include <cstring>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace Ase {

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

static uint32_t
flags_to_pcre2_compile_options (Re::Flags flags)
{
  uint32_t o =
    0                           // use PCRE2_NO_UTF_CHECK if regex is validated
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
      warning ("Re: failed to compile regex, error=%d: %s", errorcode, pattern);
  }
  ~PcRe2()
  {
    pcre2_code_free (prcode);
  }
  ssize_t
  search (const std::string &input)
  {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern (prcode, nullptr);
    const uint32_t MATCH_OPTIONS =
      0; // PCRE2_ANCHORED PCRE2_ENDANCHORED PCRE2_NOTEMPTY etc
    const int ret = pcre2_match (prcode, (const uint8_t*) input.c_str(), PCRE2_ZERO_TERMINATED, 0 /*startoffset*/, MATCH_OPTIONS, md, nullptr);
    ssize_t start = -1;
    if (ret >= 0) {
      const uint32_t ovecs = pcre2_get_ovector_count (md);
      if (ovecs > 0) {
        const size_t *ovector = pcre2_get_ovector_pointer (md);
        start = ovector[0];
      }
    }
    pcre2_match_data_free (md); md = nullptr;
    return start;
  }
  std::string
  grep (const String &input, int group)
  {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern (prcode, nullptr);
    const uint32_t MATCH_OPTIONS =
      0; // PCRE2_ANCHORED PCRE2_ENDANCHORED PCRE2_NOTEMPTY etc
    const int ret = pcre2_match (prcode, (const uint8_t*) input.c_str(), PCRE2_ZERO_TERMINATED, 0 /*startoffset*/, MATCH_OPTIONS, md, nullptr);
    std::string result;
    if (ret >= 0) {
      const uint32_t ovecs = pcre2_get_ovector_count (md);
      if (group < 0)
        group = uint (-group) < ovecs ? uint (-group) : 0;
      if (group < ovecs) {
        const size_t *ovector = pcre2_get_ovector_pointer (md);
        const size_t start = ovector[group*2], end = ovector[group*2+1];
        result.assign (&input[0] + start, &input[0] + end);
      }
    }
    pcre2_match_data_free (md); md = nullptr;
    return result;
  }
  std::vector<std::string>
  findall (const String &input_string)
  {
    std::vector<std::string> result;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern (prcode, nullptr);
    const uint32_t MATCH_OPTIONS =
      0; // PCRE2_ANCHORED PCRE2_ENDANCHORED PCRE2_NOTEMPTY etc
    const uint8_t *input = (const uint8_t*) input_string.c_str();
    const size_t input_length = strlen (input_string.c_str());
    int ret = pcre2_match (prcode, input, input_length, 0 /*startoffset*/, MATCH_OPTIONS, md, nullptr);
    size_t *ovector = ret <= 0 ? nullptr : pcre2_get_ovector_pointer (md);
    // guard against patterns such as /(?=.\K)/ that use \K to set match start>end, see pcre2pattern(3)
    if (ret < 1 || ovector[0] > ovector[1]) {
      errorcode = ret < 0 ? ret : ret == 0 ? PCRE2_ERROR_NOMEMORY : PCRE2_ERROR_BACKSLASH_K_IN_LOOKAROUND;
      if (ret != PCRE2_ERROR_NOMATCH)
        warning ("Re: findall matching error, error=%d", errorcode);
      pcre2_match_data_free (md); md = nullptr;
      return result;
    }
    result.push_back (std::string (input + ovector[0], input + ovector[1]));
    uint32_t bits = 0;
    pcre2_pattern_info (prcode, PCRE2_INFO_ALLOPTIONS, &bits);
    const bool UTF8 = bits & PCRE2_UTF;
    pcre2_pattern_info (prcode, PCRE2_INFO_NEWLINE, &bits);
    const bool CRLF_IS_NEWLINE = bits == PCRE2_NEWLINE_ANY || bits == PCRE2_NEWLINE_CRLF || bits == PCRE2_NEWLINE_ANYCRLF;
    while (ret >= 1)
      {
        PCRE2_SIZE start_offset = ovector[1];           // start at end of previous match
        uint32_t options = 0;
        if (ovector[0] == ovector[1]) {                 // previous match was for an empty string
          if (ovector[0] == input_length)
            break;                                      // end of input
          options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
        } else {                                        // previous match was non-empty
          // handle \K within a lookbehind assertion at the start, see: https://www.pcre.org/current/doc/html/pcre2demo.html
          const auto startchar = pcre2_get_startchar (md);
          if (start_offset <= startchar) {
            if (startchar >= input_length)
              break;                                  // end of input
            start_offset = startchar + 1;             // advance one code unit
            for (; UTF8 && start_offset < input_length; start_offset++)
              if ((input[start_offset] & 0xc0) != 0x80)
                break;                                // complete UTF8 code unit
          }
        }
        // try next match
        ret = pcre2_match (prcode, input, input_length, start_offset, options, md, nullptr);
        // advance in case we need to keep searching after empty string match
        if (ret == PCRE2_ERROR_NOMATCH) {
          if (options == 0)
            break;                                      // all matches found
          ovector[1] = start_offset + 1;                // advance one code unit
          if (CRLF_IS_NEWLINE &&                        // if CRLF is a newline &
              start_offset < input_length - 1 &&        // we are at CRLF
              input[start_offset] == '\r' &&
              input[start_offset + 1] == '\n')
            ovector[1] += 1;                            // skip over CR and LF
          else if (UTF8) {
            while (ovector[1] < input_length) {
              if ((input[ovector[1]] & 0xc0) != 0x80)
                break;                                  // complete UTF8 code unit
              ovector[1] += 1;
            }
          }
          continue;                                     // retry
        }
        // report match errors
        if (ret < 1 || ovector[0] > ovector[1]) {
          // guard against patterns such as /(?=.\K)/ that use \K to set match start>end, see pcre2pattern(3)
          errorcode = ret < 0 ? ret : ret == 0 ? PCRE2_ERROR_NOMEMORY : PCRE2_ERROR_BACKSLASH_K_IN_LOOKAROUND;
          warning ("Re: findall matching error, error=%d", errorcode);
          break;
        }
        // collect matched substring
        result.push_back (std::string (input + ovector[0], input + ovector[1]));
      }
    pcre2_match_data_free (md); md = nullptr;
    return result;
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

/// Find `regex` in `input` and return match position >= 0 or return < 0 otherwise.
ssize_t
Re::search (const String &regex, const String &input, Flags flags)
{
  PcRe2 rx (regex, flags);
  return rx.search (input);
}

/// Find `regex` in `input` and return matching string.
String
Re::grep (const String &regex, const String &input, int group, Flags flags)
{
  PcRe2 rx (regex, flags);
  return rx.grep (input, group);
}

/// Find `regex` in `input` and return non-overlapping matches.
StringS
Re::findall (const String &regex, const String &input, Flags flags)
{
  PcRe2 rx (regex, flags);
  return rx.findall (input);
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
  u = "a1 b2 c3 d4";     v = Re::grep ("(\\w+) *(\\w+) *(\\w+)", u, -2); TCMP (v, ==, "b2");
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
  u = "abc ABC aBc Abc"; ss = Re::findall ("abc", u, Re::I); TCMP (ss, ==, cstrings_to_vector ("abc", "ABC", "aBc", "Abc", nullptr));
  u = "a0bcd a1BC xa2bc a3cb"; ss = Re::findall ("a\\d(?=bc)", u); TCMP (ss, ==, cstrings_to_vector ("a0", "a2", nullptr));
}

} // Anon
