// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "inifile.hh"
#include "unicode.hh"
#include "utils.hh"
#include <cstring>

#define IDEBUG(...)     Ase::debug ("inifile", __VA_ARGS__)

#define ISASCIINLSPACE(c)    (c == ' ' || (c >= 9 && c <= 13))                  // ' \t\n\v\f\r'
#define ISASCIIWHITESPACE(c) (c == ' ' || c == '\t' || (c >= 11 && c <= 13))    // ' \t\v\f\r'

namespace Ase {

/** @class IniFile
 * This class parses configuration files, commonly known as INI files.
 * The files contain "[Section]" markers and "attribute=value" definitions.
 * Comment lines are preceeded by a hash "#" sign.
 * For a detailed reference, see: http://wikipedia.org/wiki/INI_file <BR>
 * To write INI files, refer to the IniWriter class.
 * Localization of attributes is supported with the "attribute[locale]=value" syntax, in accordance
 * with the desktop file spec: http://freedesktop.org/Standards/desktop-entry-spec <BR>
 * Example:
 * @code
 * [Section]
 *   key = value  # definition of Section.key = "value"
 *   name = "quoted string with \n newlines and spaces"
 * @endcode
 */

/* FIXME: possible IniFile improvements
   - support \xUUUU unicode escapes in strings
   - support \s for space (desktop-entry-spec)
   - support value list parsing, using ';' as delimiters
   - support current locale matching, including locale aliases
   - support merging of duplicates
   - support %(var) interpolation like Pyton's configparser.ConfigParser
   - parse into vector<IniEntry> which are: { kind=(section|assignment|other); String text, comment; }
 */

static bool
parse_whitespaces (const char **stringp, int min_spaces)
{
  const char *p = *stringp;
  while (ISASCIIWHITESPACE (*p))
    p++;
  if (p - *stringp >= min_spaces)
    {
      *stringp = p;
      return true;
    }
  return false;
}

static bool
skip_whitespaces (const char **stringp)
{
  return parse_whitespaces (stringp, 0);
}

static inline bool
scan_escaped (const char **stringp, size_t *linenop, const char term)
{
  const char *p = *stringp;
  size_t lineno = *linenop;
  while (*p)
    if (*p == term)
      {
        p++;
        *stringp = p;
        *linenop = lineno;
        return true;
      }
    else if (p[0] == '\\' && p[1])
      p += 2;
    else // *p != 0
      {
        if (*p == '\n')
          lineno++;
        p++;
      }
  return false;
}

static String
string_rtrim (String &str)
{
  const char *s = str.data();
  const char *e = s + str.size();
  while (e > s && ISASCIINLSPACE (e[-1]))
    e--;
  const size_t l = e - s;
  str.erase (str.begin() + l, str.end());
  return String (str.data(), str.size()); // force copy
}

static bool
scan_value (const char **stringp, size_t *linenop, String *valuep, const char *termchars = "")
{ // read up to newline or comment or termchars, support line continuation and quoted strings
  const char *p = *stringp;
  size_t lineno = *linenop;
  String v;
  v.reserve (16);
  for (;; ++p)
    switch (p[0])
      {
        const char *d;
      case '\\':
        if (p[1] == '\n' || (p[1] == '\r' && p[2] == '\n'))
          {
            p += 1 + (p[1] == '\r');
            lineno++;
            break;
          }
        else if (!p[1])
          break; // ignore
        p++;
        v += String ("\\") + p[0];
        break;
      case '"': case '\'':
        d = p;
        p++;
        if (scan_escaped (&p, &lineno, d[0]))
          {
            v += String (d, p - d);
            p--; // back off to terminating \" or \'
          }
        else
          return false; // brr, unterminated string
        break;
      case '\r':
        if (p[1] && p[1] != '\n')
          v += ' '; // turn stray '\r' into space
        break;
      default:
        if (!strchr (termchars, p[0]))
          {
            v += p[0];
            break;
          }
        // fall through
      case 0: case '\n': case ';': case '#':
        *stringp = p;
        *linenop = lineno;
        *valuep = string_rtrim (v); // forces copy
        return true;
      }
}

static bool
skip_line (const char **stringp, size_t *linenop, String *textp)
{ // ( !'\n' )* '\n'
  const char *p = *stringp, *const start = p;
  size_t lineno = *linenop;
  while (*p && *p != '\n')
    p++;
  if (textp)
    *textp = String (start, p - start);
  if (*p == '\n')
    {
      lineno++;
      p++;
    }
  *stringp = p;
  *linenop = lineno;
  return true;
}

static bool
skip_commentline (const char **stringp, size_t *linenop, String *commentp = NULL)
{ // S* ( '#' | ';' ) ( !'\n' )* '\n'
  const char *p = *stringp;
  skip_whitespaces (&p);
  if (*p != '#' && *p != ';')
    return false;
  p++;
  *stringp = p;
  return skip_line (stringp, linenop, commentp);
}

static bool
skip_to_eol (const char **stringp, size_t *linenop)
{ // comment? '\r'? '\n', or EOF
  const char *p = *stringp;
  if (*p == '#' || *p == ';')
    return skip_commentline (stringp, linenop);
  if (*p == '\r')
    p++;
  if (*p == '\n')
    {
      p++;
      (*linenop) += 1;
      *stringp = p;
      return true;
    }
  if (!*p)
    {
      *stringp = p;
      return true;
    }
  return false;
}

static bool
parse_assignment (const char **stringp, size_t *linenop, String *keyp, String *localep, String *valuep)
{ // S* KEYCHARS+ S* ( '[' S* LOCALECHARS* S* ']' )? S* ( '=' | ':' ) scan_value comment? EOL
  const char *p = *stringp;
  size_t lineno = *linenop;
  String key, locale, value;
  bool success = true;
  success = success && skip_whitespaces (&p);
  success = success && scan_value (&p, &lineno, &key, "[]=:");
  success = success && skip_whitespaces (&p);
  if (success && *p == '[')
    {
      p++;
      success = success && skip_whitespaces (&p);
      success = success && scan_value (&p, &lineno, &locale, "[]");
      success = success && skip_whitespaces (&p);
      if (*p != ']')
        return false;
      p++;
      success = success && skip_whitespaces (&p);
    }
  if (!success)
    return false;
  if (*p != '=' && *p != ':')
    return false;
  p++;
  success = success && skip_whitespaces (&p);
  success = success && scan_value (&p, &lineno, &value);
  success = success && skip_to_eol (&p, &lineno);
  if (!success)
    return false;
  *stringp = p;
  *linenop = lineno;
  *keyp = key;
  *localep = locale;
  *valuep = value;
  return true;
}

static bool
parse_section (const char **stringp, size_t *linenop, String *sectionp)
{ // S* '[' S* SECTIONCHARS+ S* ']' S* comment? EOL
  const char *p = *stringp;
  size_t lineno = *linenop;
  String section;
  bool success = true;
  success = success && skip_whitespaces (&p);
  if (*p != '[')
    return false;
  p++;
  success = success && skip_whitespaces (&p);
  success = success && scan_value (&p, &lineno, &section, "[]");
  success = success && skip_whitespaces (&p);
  if (*p != ']')
    return false;
  p++;
  success = success && skip_whitespaces (&p);
  success = success && skip_to_eol (&p, &lineno);
  if (!success)
    return false;
  *stringp = p;
  *linenop = lineno;
  *sectionp = section;
  return true;
}

void
IniFile::load_ini (const String &inputname, const String &data)
{
  const char *p = data.c_str();
  size_t nextno = 1;
  String section = "";
  while (*p)
    {
      const size_t lineno = nextno;
      String text, key, locale, *debugp = 0 ? &text : NULL; // DEBUG parsing?
      if (skip_commentline (&p, &nextno, debugp))
        {
          if (debugp)
            printerr ("%s:%d: #%s\n", inputname.c_str(), lineno, debugp->c_str());
        }
      else if (parse_section (&p, &nextno, &text))
        {
          if (debugp)
            printerr ("%s:%d: %s\n", inputname.c_str(), lineno, text.c_str());
          section = text;
          if (strchr (section.c_str(), '"'))
            { // reconstruct section path from '[branch "devel.wip"]' syntax
              StringS sv = string_split (section);
              for (auto &s : sv)
                if (s.c_str()[0] == '"')
                  s = string_from_cquote (s);
              section = string_join (".", sv);
            }
        }
      else if (parse_assignment (&p, &nextno, &key, &locale, &text))
        {
          if (debugp)
            printerr ("%s:%d:\t%s[%s] = %s\n", inputname.c_str(), lineno, key.c_str(), locale.c_str(), string_to_cquote (text));
          String k (key);
          if (!locale.empty())
            k += "[" + locale + "]";
          if (strchr (section.c_str(), '=') || strchr (key.c_str(), '.'))
            IDEBUG ("%s:%d: invalid key name: %s.%s", inputname.c_str(), lineno, section.c_str(), k.c_str());
          else
            sections_[section].push_back (k + "=" + text);
        }
      else if (skip_line (&p, &nextno, debugp))
        {
          if (debugp)
            printerr ("%s:%d:~ %s\n", inputname.c_str(), lineno, debugp->c_str());
        }
      else
        break; // EOF if !skip_line
    }
}

IniFile::IniFile (const String &name, const String &inidata)
{
  load_ini (name, inidata);
  if (sections_.empty())
    IDEBUG ("empty INI file: %s", string_to_cquote (name));
}

IniFile::IniFile (Blob blob)
{
  if (blob)
    load_ini (blob.name(), blob.string());
  if (sections_.empty())
    IDEBUG ("empty INI file: %s", string_to_cquote (blob ? blob.name() : "<NULL>"));
}

IniFile::IniFile (const IniFile &source)
{
  *this = source;
}

IniFile&
IniFile::operator= (const IniFile &source)
{
  sections_  = source.sections_;
  return *this;
}

bool
IniFile::has_sections () const
{
  return !sections_.empty();
}

const StringS&
IniFile::section (const String &name) const
{
  SectionMap::const_iterator cit = sections_.find (name);
  if (cit != sections_.end())
    return cit->second;
  static const StringS empty_dummy;
  return empty_dummy;
}

bool
IniFile::has_section (const String &section) const
{
  SectionMap::const_iterator cit = sections_.find (section);
  return cit != sections_.end();
}

StringS
IniFile::sections () const
{
  StringS secs;
  for (auto it : sections_)
    secs.push_back (it.first);
  return secs;
}

StringS
IniFile::attributes (const String &section) const
{
  StringS opts;
  SectionMap::const_iterator cit = sections_.find (section);
  if (cit != sections_.end())
    for (auto s : cit->second)
      opts.push_back (s.substr (0, s.find ('=')));
  return opts;
}

bool
IniFile::has_attribute (const String &section, const String &key) const
{
  SectionMap::const_iterator cit = sections_.find (section);
  if (cit == sections_.end())
    return false;
  for (auto s : cit->second)
    if (s.size() > key.size() && s[key.size()] == '=' && memcmp (s.data(), key.data(), key.size()) == 0)
      return true;
  return false;
}

StringS
IniFile::raw_values () const
{
  StringS opts;
  for (auto it : sections_)
    for (auto s : it.second)
      opts.push_back (it.first + "." + s);
  return opts;
}

bool
IniFile::has_raw_value (const String &dotpath, String *valuep) const
{
  const char *p = dotpath.c_str(), *d = strrchr (p, '.');
  if (!d)
    return false;
  const String secname = String (p, d - p);
  d++; // point to key
  const StringS &sv = section (secname);
  if (!sv.size())
    return false;
  const size_t l = dotpath.size() - (d - p); // key length
  for (auto kv : sv)
    if (kv.size() > l && kv[l] == '=' && memcmp (kv.data(), d, l) == 0)
      {
        if (valuep)
          *valuep = kv.substr (l + 1);
        return true;
      }
  return false;
}

String
IniFile::raw_value (const String &dotpath) const
{
  String v;
  has_raw_value (dotpath, &v);
  return v;
}

String
IniFile::cook_string (const String &input)
{
  String v;
  size_t dummy = 0;
  for (const char *p = input.c_str(); *p; p++)
    switch (*p)
      {
        const char *start;
      case '\\':
        switch (p[1])
          {
          case 0:       break; // ignore trailing backslash
          case 'n':     v += '\n';      break;
          case 'r':     v += '\r';      break;
          case 't':     v += '\t';      break;
          case 'b':     v += '\b';      break;
          case 'f':     v += '\f';      break;
          case 'v':     v += '\v';      break;
          default:      v += p[1];      break;
          }
        p++;
        break;
      case '"': case '\'':
        start = ++p;
        if (scan_escaped (&p, &dummy, start[-1]))
          {
            p--; // back off to terminating \" or \'
            v += string_from_cquote (String (start, p - start));
            break;
          }
        // fall through
      default:
        v += p[0];
        break;
      }
  return v;
}

bool
IniFile::has_value (const String &dotpath, String *valuep) const
{
  const bool hasit = has_raw_value (dotpath, valuep);
  if (valuep && hasit)
    *valuep = cook_string (*valuep);
  return hasit;
}

String
IniFile::value_as_string (const String &dotpath) const
{
  String raw = raw_value (dotpath);
  String v = cook_string (raw);
  return v;
}


// == IniWriter ==
/** @class IniWriter
 * This class implements a simple section and value syntax writer as used in INI files.
 * For a detailed reference, see: http://wikipedia.org/wiki/INI_file <BR>
 * To parse the output of this class, refer to the IniFile class.
 */
IniWriter::Section*
IniWriter::find_section (String name, bool create)
{
  for (size_t i = 0; i < sections_.size(); i++)
    if (sections_[i].name == name)
      return &sections_[i];
  if (create)
    {
      const size_t i = sections_.size();
      sections_.resize (i + 1);
      sections_[i].name = name;
      return &sections_[i];
    }
  return NULL;
}

size_t
IniWriter::find_entry (IniWriter::Section &section, String name, bool create)
{
  for (size_t i = 0; i < section.entries.size(); i++)
    if (section.entries[i].size() > name.size() &&
        section.entries[i][name.size()] == '=' &&
        section.entries[i].compare (0, name.size(), name) == 0)
      return i;
  if (create)
    {
      const size_t i = section.entries.size();
      section.entries.push_back (name + "=");
      return i;
    }
  return size_t (-1);
}

/// Set (or add) a value with INI file semantics: `section.key = value.`
void
IniWriter::set (String key, String value)
{
  const size_t p = key.rfind ('.');
  if (p <= 0 || p + 1 >= key.size())
    {
      warning ("%s: invalid key: %s", __func__, key);
      return;   // invalid entry
    }
  Section *section = find_section (key.substr (0, p), true);
  const String entry_key = key.substr (p + 1);
  const size_t idx = find_entry (*section, entry_key, true);
  section->entries[idx] = entry_key + "=" + value;
}

/// Generate INI file syntax for all values store in the class.
String
IniWriter::output ()
{
  String s;
  for (size_t i = 0; i < sections_.size(); i++)
    if (!sections_[i].entries.empty())
      {
        String sec = sections_[i].name;
        const size_t d = sec.find ('.');
        if (d >= 0 && d < sec.size())
          sec = sec.substr (0, d) + " " + string_to_cquote (sec.substr (d + 1));
        s += String ("[") + sec + "]\n";
        for (size_t j = 0; j < sections_[i].entries.size(); j++)
          {
            const String raw = sections_[i].entries[j];
            const size_t p = raw.find ('=');
            const String k = raw.substr (0, p);
            String v = raw.substr (p + 1);
            static String allowed_chars = string_set_ascii_alnum() + "<>,;.:-_~*/+^!$=?";
            if (!string_is_canonified (v, allowed_chars))
              v = string_to_cquote (v);
            s += string_format ("\t%s = %s\n", k, v);
          }
      }
  return s;
}

} // Ase
