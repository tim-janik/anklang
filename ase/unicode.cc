// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "unicode.hh"
#include "blob.hh"
#include "platform.hh"
#include "websocket.hh"

#include <glib.h>

namespace Ase {

/* https://www.unicode.org/versions/Unicode15.0.0/ch03.pdf
 * Table 3-6. UTF-8 Bit Distribution
 * | Scalar Value               | First Byte | Second Byte | Third Byte | Fourth Byte
 * | 00000000 0xxxxxxx          | 0xxxxxxx   |             |            |
 * | 00000yyy yyxxxxxx          | 110yyyyy   | 10xxxxxx    |            |
 * | zzzzyyyy yyxxxxxx          | 1110zzzz   | 10yyyyyy    | 10xxxxxx   |
 * | 000uuuuu zzzzyyyy yyxxxxxx | 11110uuu   | 10uuzzzz    | 10yyyyyy   | 10xxxxxx
 *
 * Table 3-7. Well-Formed UTF-8 Byte Sequences
 * | Code Points        | First Byte | Second Byte | Third Byte | Fourth Byte
 * | U+0000..U+007F     | 00..7F     |             |            |
 * | U+0080..U+07FF     | C2..DF     | 80..BF      |            |
 * | U+0800..U+0FFF     | E0         | A0..BF      | 80..BF     |
 * | U+1000..U+CFFF     | E1..EC     | 80..BF      | 80..BF     |
 * | U+D000..U+D7FF     | ED         | 80..9F      | 80..BF     |
 * | U+E000..U+FFFF     | EE..EF     | 80..BF      | 80..BF     |
 * | U+10000..U+3FFFF   | F0         | 90..BF      | 80..BF     | 80..BF
 * | U+40000..U+FFFFF   | F1..F3     | 80..BF      | 80..BF     | 80..BF
 * | U+100000..U+10FFFF | F4         | 80..8F      | 80..BF     | 80..BF
 */

/** Decode valid UTF-8 sequences, invalid sequences are treated as Latin-1 characters.
 * - CODEPOINT=0: The return value indicates the number of bytes forgivingly parsed as a single UTF-8 character.
 * - CODEPOINT=1: The return value indicates the number of bytes forgivingly parsed as a single UTF-8 character and `*unicode` is assigned.
 *   If `*unicode` is >= 0x80, a Latin-1 character was encountered.
 * - CODEPOINT=2: Invalid characters are encoded as private code points in the range 0xEF80..0xEFFF, see also: https://en.wikipedia.org/wiki/UTF-8#PEP_383
 * - CODEPOINT=3: Validly encoded code points in the range 0xEF80..0xEFFF are also treated as invalid characters and are encoded into 0xEF80..0xEFFF.
*/
template<int CODEPOINT> static inline size_t // returns length of unicode char
utf8character (const char *str, uint32_t *unicode)
{
  /* https://en.wikipedia.org/wiki/UTF-8
    : 0000 0001 0010 0011 0100 0101 0110 0111 1000 1001 1010 1011 1100 1101 1110 1111
    :  000  001  002  003  004  005  006  007  010  011  012  013  014  015  016  017
    :    0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
    :  0x0  0x1  0x2  0x3  0x4  0x5  0x6  0x7  0x8  0x9  0xA  0xB  0xC  0xD  0xE  0xF
   */
  const uint8_t c = str[0];
  // optimized for one-byte sequences
  if (CODEPOINT <= 1 && __builtin_expect (c < 0xc0, true))
    {
      if (CODEPOINT)
        *unicode = c;                                   // valid if c <= 0x7F
      return 1;                                         // treat as Latin-1 otherwise
    }
  // multi-byte sequences
  switch (c & 0xF8)
    {
      uint8_t d, e, f;
    case 0xC0: case 0xC8: case 0xD0: case 0xD8:         // 2-byte sequence
      d = str[1];
      if (__builtin_expect ((d & 0xC0) != 0x80, false))
        goto one_byte;
      if (CODEPOINT) {
        *unicode = ((c & 0x1f) << 6) + (d & 0x3f);
      }
      return 2;                                         // valid
    case 0xE0: case 0xE8:                               // 3-byte sequence
      d = str[1];
      if (__builtin_expect ((d & 0xC0) != 0x80, false))
        goto one_byte;
      e = str[2];
      if (__builtin_expect ((e & 0xC0) != 0x80, false))
        goto one_byte;
      if (CODEPOINT) {
        *unicode = ((c & 0x0f) << 12) + ((d & 0x3f) << 6) + (e & 0x3f);
        if (CODEPOINT >= 2 && *unicode >= 0xd800 && *unicode <= 0xdfff)
          goto one_byte;                                // UTF-16 surrogates are invalid in UTF-8
        if (CODEPOINT >= 3 && *unicode >= 0xef80 && *unicode <= 0xefff)
          goto one_byte;                                // MirBSD OPTU-8/16 private use
      }
      return 3;                                         // valid
    case 0xF0:                                          // 4-byte sequence
      d = str[1];
      if (__builtin_expect ((d & 0xC0) != 0x80, false))
        goto one_byte;
      e = str[2];
      if (__builtin_expect ((e & 0xC0) != 0x80, false))
        goto one_byte;
      f = str[3];
      if (__builtin_expect ((f & 0xC0) != 0x80, false))
        goto one_byte;
      if (CODEPOINT) {
        *unicode = ((c & 0x07) << 18) + ((d & 0x3f) << 12) + ((e & 0x3f) << 6) + (f & 0x3f);
        if (CODEPOINT >= 2 && *unicode >= 0xd800 && *unicode <= 0xdfff)
          goto one_byte;                                // UTF-16 surrogates are invalid in UTF-8
        if (CODEPOINT >= 3 && *unicode >= 0xef80 && *unicode <= 0xefff)
          goto one_byte;                                // MirBSD OPTU-8/16 private use
      }
      return 4;                                         // valid
    default: one_byte:
      if (CODEPOINT >= 2 && c >= 0x80)
        *unicode = 0xef80 - 0x80 + c;                   // escape byte as surrogate
      else if (CODEPOINT)
        *unicode = c;                                   // treat as Latin-1 otherwise
      return 1;
    }
}

/// Encode a file system path consisting of bytes into UTF-8, using surrogate code points to store non UTF-8 bytes.
std::string
encodefs (const std::string &fschars)
{
  const char *fstr = fschars.c_str();
  std::string utf8str;
  utf8str.reserve (fschars.size());
  while (*fstr) {
    uint32_t unicode;
    const int w = utf8character<3> (fstr, &unicode);
    if (unicode >= 0xef80 && unicode <= 0xefff)
      utf8str += utf8encode (&unicode, 1);
    else
      utf8str += std::string_view (fstr, w);
    fstr += w;
  }
  return utf8str;
}

/// Decode UTF-8 string back into file system path representation, extracting surrogate code points as bytes.
std::string
decodefs (const std::string &utf8str)
{
  const char *ustr = utf8str.c_str();
  std::string fschars;
  fschars.reserve (utf8str.size());
  while (*ustr) {
    uint32_t unicode;
    const int w = utf8character<1> (ustr, &unicode);
    if (unicode >= 0xef80 && unicode <= 0xefff)
      fschars += char (unicode - (0xef80 - 0x80));
    else
      fschars += std::string_view (ustr, w);
    ustr += w;
  }
  return fschars;
}

/// Convert UTF-8 encoded file system path into human readable display format, the conversion is lossy but readable.
std::string
displayfs (const std::string &utf8str)
{
  const char *ustr = utf8str.c_str();
  std::string display;
  display.reserve (utf8str.size());
  while (*ustr) {
    uint32_t unicode;
    const int w = utf8character<2> (ustr, &unicode);
    if (unicode >= 0xef80 && unicode <= 0xefff) {
      unicode = unicode - (0xef80 - 0x80); // leaves 0x80..0xff
      display += utf8encode (&unicode, 1);
    } else
      display += std::string_view (ustr, w);
    ustr += w;
  }
  return display;
}

/// Returns length of unicode character in bytes
static inline size_t
utf8codepoint (const char *str, uint32_t *unicode)
{
  return utf8character<1> (str, unicode);
}

/// Returns length of unicode character in bytes
static inline size_t
utf8skip (const char *str)
{
  return utf8character<0> (str, NULL);
}

/// Count valid UTF-8 sequences, invalid sequences are counted as Latin-1 characters.
size_t
utf8len (const char *str)
{
  size_t l;
  for (l = 0; __builtin_expect (*str != 0, true); l++)
    str += utf8skip (str);
  return l;
}

/// Count valid UTF-8 sequences, invalid sequences are counted as Latin-1 characters.
size_t
utf8len (const std::string &str)
{
  const char *c = str.data(), *e = c + str.size();
  size_t l = 0;
  while (c < e)
    {
      c += utf8skip (c);
      l += 1;
    }
  return l;
}

/// Convert valid UTF-8 sequences to Unicode codepoints, invalid sequences are treated as Latin-1 characters.
std::vector<uint32_t>
utf8decode (const std::string &utf8str)
{
  std::vector<uint32_t> codepoints;
  codepoints.resize (utf8str.size());
  size_t l = utf8_to_unicode (utf8str.c_str(), &codepoints[0]);
  codepoints.resize (l);
  return codepoints;
}

/// Convert valid UTF-8 sequences to Unicode codepoints, invalid sequences are treated as Latin-1 characters.
/// The array @a codepoints must be able to hold at least as many elements as are characters stored in @a str.
/// Returns the number of codepoints stored in @a codepoints.
size_t
utf8_to_unicode (const char *str, uint32_t *codepoints)
{
  // assuming sizeof codepoints[] >= sizeof str[]
  size_t l;
  for (l = 0; __builtin_expect (*str != 0, true); l++)
    str += utf8codepoint (str, &codepoints[l]);
  return l;
}

/// Convert valid UTF-8 sequences to Unicode codepoints, invalid sequences are treated as Latin-1 characters.
/// Returns the number of codepoints newly stored in @a codepoints.
size_t
utf8_to_unicode (const std::string &str, std::vector<uint32_t> &codepoints)
{
  const size_t l = codepoints.size();
  codepoints.reserve (codepoints.size() + str.size());
  const char *c = str.data(), *const e = c + str.size();
  while (c < e)
    {
      uint32_t codepoint;
      c += utf8codepoint (c, &codepoint);
      codepoints.push_back (codepoint);
    }
  return codepoints.size() - l;
}

/// Convert @a codepoints into an UTF-8 string, using the shortest possible encoding.
std::string
utf8encode (const uint32_t *codepoints, size_t n_codepoints)
{
  std::string str;
  str.reserve (n_codepoints);
  for (size_t i = 0; i < n_codepoints; i++)
    {
      const uint32_t u = codepoints[i];
      if (__builtin_expect (u <= 0x7F, true))
        {
          str.push_back (u);
          continue;
        }
      switch (u)
        {
        case 0x00000080 ... 0x000007FF:
          str.push_back (0xC0 + (u >> 6));
          str.push_back (0x80 + (u & 0x3F));
          break;
        case 0x00000800 ... 0x0000FFFF:
          str.push_back (0xE0 +  (u >> 12));
          str.push_back (0x80 + ((u >>  6) & 0x3F));
          str.push_back (0x80 +  (u & 0x3F));
          break;
        case 0x00010000 ... 0x0010FFFF:
          str.push_back (0xF0 +  (u >> 18));
          str.push_back (0x80 + ((u >> 12) & 0x3F));
          str.push_back (0x80 + ((u >>  6) & 0x3F));
          str.push_back (0x80 +  (u & 0x3F));
          break;
        default:
          break;
        }
    }
  return str;
}

/// Convert @a codepoints into an UTF-8 string, using the shortest possible encoding.
std::string
utf8encode (const std::vector<uint32_t> &codepoints)
{
  return utf8encode (codepoints.data(), codepoints.size());
}

/** Check `c` to be a NameStartChar, according to the QName EBNF.
 * See https://en.wikipedia.org/wiki/QName
 */
static bool
codepoint_is_namestartchar (uint32_t c)
{
  const bool ok =
    std::isalpha (c) || c == '_' ||
    (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) ||
    (c >= 0xF8 && c <= 0x2FF) || (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) ||
    (c >= 0x200C && c <= 0x200D) || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) ||
    (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) ||
    (c >= 0x10000 && c <= 0xEFFFF);
  return ok;
}

/** Check `c` to be a NameChar, according to the QName EBNF.
 * See https://en.wikipedia.org/wiki/QName
 */
static bool
codepoint_is_ncname (uint32_t c)
{
  const bool ok =
    codepoint_is_namestartchar (c) ||
    c == '-' || c == '.' || (c >= '0' && c <= '9') ||
    c == 0xB7 || (c >= 0x0300 && c <= 0x036F) || (c >= 0x203F && c <= 0x2040);
    return ok;
}

/** Check `input` to be a NCName, according to the QName EBNF.
 * See https://en.wikipedia.org/wiki/QName
 */
bool
string_is_ncname (const String &input)
{
  std::vector<uint32_t> tmp;
  utf8_to_unicode (input, tmp);
  for (auto c : tmp)
    if (!codepoint_is_ncname (c))
      return false;
  return true;
}

/** Convert `input` to a NCName, according to the QName EBNF.
 * See https://en.wikipedia.org/wiki/QName
 */
String
string_to_ncname (const String &input, uint32_t substitute)
{
  std::vector<uint32_t> ucstring;
  utf8_to_unicode (input, ucstring);
  for (auto it = ucstring.begin(); it != ucstring.end(); /**/)
    if (!codepoint_is_ncname (*it)) {
      if (substitute)
        *it++ = substitute;
      else
        it = ucstring.erase (it);
    } else
      ++it;
  if (!ucstring.empty() && !codepoint_is_namestartchar (ucstring[0]))
    ucstring.insert (ucstring.begin(), '_');
  return utf8encode (ucstring);
}

} // Ase

// == Testing ==
#include "testing.hh"
#include "internal.hh"

namespace { // Anon
using namespace Ase;

TEST_INTEGRITY (unicode_displayfs_tests);
static void
unicode_displayfs_tests()
{
  // ASCII is fully preserved in encodefs, decodefs, displayfs
  const char *const asciistr = "\001\t09AZaz|~\177";
  TASSERT (WebSocketServer::utf8_validate (asciistr));
  TASSERT (WebSocketServer::utf8_validate (encodefs (asciistr)));
  TASSERT (decodefs (encodefs (asciistr)) == asciistr);
  TASSERT (displayfs (encodefs (asciistr)) == asciistr);
  TASSERT (WebSocketServer::utf8_validate (displayfs (encodefs (asciistr))));
  TASSERT (displayfs (asciistr) == asciistr);
  TASSERT (WebSocketServer::utf8_validate (displayfs (asciistr)));
  TASSERT (encodefs (asciistr) == asciistr);
  TASSERT (decodefs (asciistr) == asciistr);
  // non UTF-8 bytes need conversions
  const char *const lowbytes = "\x80\x87\x88\x8f\x90\x97\x98\x9f\xa0\xa7\xa8\xaf\xb0\xb7\xb8\xbf";
  const char *const low2utf8 = "\u0080\u0087\u0088\u008f\u0090\u0097\u0098\u009f\u00a0\u00a7\u00a8\u00af\u00b0\u00b7\u00b8\u00bf";
  TASSERT (WebSocketServer::utf8_validate (encodefs (lowbytes)));
  TASSERT (decodefs (encodefs (lowbytes)) == lowbytes);
  TASSERT (displayfs (encodefs (lowbytes)) == low2utf8);
  TASSERT (WebSocketServer::utf8_validate (encodefs (lowbytes)));
  TASSERT (displayfs (lowbytes) == low2utf8);
  TASSERT (WebSocketServer::utf8_validate (displayfs (lowbytes)));
  TASSERT (encodefs (lowbytes) != lowbytes);
  // non UTF-8 sequences need conversions
  const char *const highbytes = "\xc0 \xc7 \xc8 \xcf \xd0 \xd7 \xd8 \xdf \xe0 \xe7 \xe8 \xef \xf0 \xf7 \xf8 \xff";
  const char *const high2utf8 = "\u00c0 \u00c7 \u00c8 \u00cf \u00d0 \u00d7 \u00d8 \u00df \u00e0 \u00e7 \u00e8 \u00ef \u00f0 \u00f7 \u00f8 \u00ff";
  TASSERT (WebSocketServer::utf8_validate (encodefs (highbytes)));
  TASSERT (decodefs (encodefs (highbytes)) == highbytes);
  TASSERT (WebSocketServer::utf8_validate (displayfs (highbytes)));
  TASSERT (displayfs (highbytes) == high2utf8);
  TASSERT (displayfs (encodefs (highbytes)) == high2utf8);
  TASSERT (WebSocketServer::utf8_validate (displayfs (encodefs (highbytes))));
  TASSERT (encodefs (highbytes) != highbytes);
  TASSERT (WebSocketServer::utf8_validate (encodefs (highbytes)));
  // UTF-8 characters are fully preserved in encodefs, decodefs, displayfs
  const char *const utf8str = "äöüßÄÖÜïÿ";
  TASSERT (WebSocketServer::utf8_validate (encodefs (utf8str)));
  TASSERT (decodefs (encodefs (utf8str)) == utf8str);
  TASSERT (WebSocketServer::utf8_validate (displayfs (utf8str)));
  TASSERT (displayfs (utf8str) == utf8str);
  TASSERT (displayfs (encodefs (utf8str)) == utf8str);
  TASSERT (WebSocketServer::utf8_validate (encodefs (utf8str)));
  TASSERT (encodefs (utf8str) == utf8str);
  TASSERT (decodefs (utf8str) == utf8str);
  const char *const lat1str = "\xe4\xf6\xfc\xdf\xc4\xd6\xdc\xef\xff";
  TASSERT (WebSocketServer::utf8_validate (encodefs (lat1str)));
  TASSERT (displayfs (encodefs (lat1str)) == utf8str);
  TASSERT (displayfs (lat1str) == utf8str);
  // Preserve filenames containing UTF-8 encoded surrogates and private use codes
  const char *const srg8str = "\xed\xb2\x80\xed\xb3\xbf\xee\xbf\xa4\xee\xbf\xbf";
  TASSERT (encodefs (srg8str) != srg8str);
  TASSERT (WebSocketServer::utf8_validate (encodefs (srg8str)));
  const std::string srg8enc = encodefs (srg8str);
  TASSERT (decodefs (srg8enc) == srg8str);
  TASSERT (displayfs (srg8enc) != srg8enc);
  TASSERT (WebSocketServer::utf8_validate (displayfs (srg8enc)));
  TASSERT (displayfs (srg8enc) != displayfs (srg8str));
}

TEST_INTEGRITY (unicode_tests);
static void
unicode_tests()
{
  Blob b = Blob::from_file ("/etc/mailcap");
  const std::string str = b.string();
  size_t ase_utf8len, glib_utf8len;
  ase_utf8len = utf8len (str.c_str());
  glib_utf8len = g_utf8_strlen (str.c_str(), -1);
  TCMP (ase_utf8len, ==, glib_utf8len);
  std::vector<uint32_t> codepoints;
  size_t nc = 0, cc = 0, pc = 0;
  for (size_t i = 0; i <= unicode_last_codepoint; i++)
    {
      TASSERT (unicode_is_assigned (i) <= unicode_is_valid (i));
      TASSERT (unicode_is_noncharacter (i) != unicode_is_character (i));
      nc += unicode_is_noncharacter (i);
      cc += unicode_is_control_code (i);
      pc += unicode_is_private (i);
      if (i && unicode_is_assigned (i))
        codepoints.push_back (i);
    }
  TASSERT (nc == 66);
  TASSERT (cc == 65);
  TASSERT (pc == 6400 + 65534 + 65534);
  std::string big = utf8encode (codepoints);
  ase_utf8len = utf8len (big.c_str());
  glib_utf8len = g_utf8_strlen (big.c_str(), -1);
  TCMP (ase_utf8len, ==, glib_utf8len);
  TCMP (ase_utf8len, ==, codepoints.size());
  if (true)
    {
      std::vector<uint32_t> tmp;
      const size_t tmp_result = utf8_to_unicode (big, tmp);
      TASSERT (tmp_result == tmp.size() && codepoints.size() == tmp_result);
      for (size_t i = 0; i < codepoints.size(); ++i)
        TASSERT (tmp[i] == codepoints[i]);
    }
  TCMP (false, ==, string_is_ncname ("0abc@def^foo"));
  TCMP ("_0abcdeffoo", ==, string_to_ncname ("0abc@def^foo"));
  TCMP ("abc_def_foo", ==, string_to_ncname ("abc@def^foo", '_'));
  TCMP (true, ==, string_is_ncname ("_0abc_def_foo"));
}

} // Anon
