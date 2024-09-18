// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "formatter.hh"
#include <unistd.h>     // isatty
#include <cstring>
#include <atomic>
#include "testing.hh"
#include "internal.hh"

/** @TODO:
 * - StringFormatter: support directives: %%n %%S %%ls
 * - StringFormatter: support directive flags: I
 */

namespace Ase {

locale_t
ScopedPosixLocale::posix_locale()
{
  static locale_t cached_posix_locale = [] () {
    locale_t posix_locale = nullptr;
    if (!posix_locale)
      posix_locale = newlocale (LC_ALL_MASK, "POSIX.UTF-8", nullptr);
    if (!posix_locale)
      posix_locale = newlocale (LC_ALL_MASK, "C.UTF-8", nullptr);
    if (!posix_locale)
      posix_locale = newlocale (LC_ALL_MASK, "POSIX", nullptr);
    if (!posix_locale)
      posix_locale = newlocale (LC_ALL_MASK, "C", nullptr);
    if (!posix_locale)
      posix_locale = newlocale (LC_ALL_MASK, nullptr, nullptr);
    if (posix_locale == nullptr)
      fprintf (stderr, "%s: WARNING: newlocale() returned NULL\n", __FILE__);
    return posix_locale;
  } ();
  return cached_posix_locale;
}

ScopedPosixLocale::ScopedPosixLocale()
{
  saved_locale_ = uselocale (posix_locale());
}

ScopedPosixLocale::~ScopedPosixLocale()
{
  uselocale (saved_locale_);
}

namespace Impl {

template<class... Args> static std::string
system_string_printf (const char *format, Args... args)
{
  char *cstr = nullptr;
  int ret;
  {
    ScopedPosixLocale posix_locale;
    ret = asprintf (&cstr, format, args...);
  }
  if (ret >= 0 && cstr)
    {
      std::string result = cstr;
      free (cstr);
      return result;
    }
  return format;
}

static bool
parse_unsigned_integer (const char **stringp, uint64_t *up)
{ // '0' | [1-9] [0-9]* : <= 18446744073709551615
  const char *p = *stringp;
  // zero
  if (*p == '0' && !(p[1] >= '0' && p[1] <= '9'))
    {
      *up = 0;
      *stringp = p + 1;
      return true;
    }
  // first digit
  if (!(*p >= '1' && *p <= '9'))
    return false;
  uint64_t u = *p - '0';
  p++;
  // rest digits
  while (*p >= '0' && *p <= '9')
    {
      const uint64_t last = u;
      u = u * 10 + (*p - '0');
      p++;
      if (u < last) // overflow
        return false;
    }
  *up = u;
  *stringp = p;
  return true;
}

struct StringFormatDirective {
  using ll_t = long long;
  char     conversion = 0;
  uint32_t adjust_left : 1 = 0, add_sign : 1 = 0, use_width : 1 = 0, use_precision : 1 = 0;
  uint32_t alternate_form : 1 = 0, zero_padding : 1 = 0, add_space : 1 = 0, locale_grouping : 1 = 0;
  uint32_t field_width = 0, precision = 0, start = 0, end = 0, value_index = 0, width_index = 0, precision_index = 0;
  static bool
  parse_positional (const char **stringp, uint64_t *ap)
  { // [0-9]+ '$'
    const char *p = *stringp;
    uint64_t ui64 = 0;
    if (parse_unsigned_integer (&p, &ui64) && *p == '$')
      {
        p++;
        *ap = ui64;
        *stringp = p;
        return true;
      }
    return false;
  }
  const char*
  parse_directive (const char **stringp, size_t *indexp)
  { // '%' positional? [-+#0 '']* ([0-9]*|[*]positional?) ([.]([0-9]*|[*]positional?))? [hlLjztqZ]* [spmcCdiouXxFfGgEeAa]
    const char *p = *stringp;
    size_t index = *indexp;
    // '%' directive start
    if (*p != '%')
      return "missing '%' at start";
    p++;
    // positional argument
    uint64_t ui64 = -1;
    if (parse_positional (&p, &ui64))
      {
        if (ui64 > 0 && ui64 <= 2147483647)
          value_index = ui64;
        else
          return "invalid positional specification";
      }
    // flags
    const char *flags = "-+#0 '";
    while (strchr (flags, *p))
      switch (*p)
        {
        case '-': adjust_left = true;        goto default_case;
        case '+': add_sign = true;           goto default_case;
        case '#': alternate_form = true;     goto default_case;
        case '0': zero_padding = true;       goto default_case;
        case ' ': add_space = true;          goto default_case;
        case '\'': locale_grouping = true;   goto default_case;
        default: default_case:
          p++;
          break;
        }
    // field width
    ui64 = 0;
    if (*p == '*')
      {
        p++;
        if (parse_positional (&p, &ui64))
          {
            if (ui64 > 0 && ui64 <= 2147483647)
              width_index = ui64;
            else
              return "invalid positional specification";
          }
        else
          width_index = index++;
        use_width = true;
      }
    else if (parse_unsigned_integer (&p, &ui64))
      {
        if (ui64 <= 2147483647)
          field_width = ui64;
        else
          return "invalid field width specification";
        use_width = true;
      }
    // precision
    if (*p == '.')
      {
        use_precision = true;
        p++;
      }
    if (*p == '*')
      {
        p++;
        if (parse_positional (&p, &ui64))
          {
            if (ui64 > 0 && ui64 <= 2147483647)
              precision_index = ui64;
            else
              return "invalid positional specification";
          }
        else
          precision_index = index++;
      }
    else if (parse_unsigned_integer (&p, &ui64))
      {
        if (ui64 <= 2147483647)
          precision = ui64;
        else
          return "invalid precision specification";
      }
    // modifiers
    const char *modifiers = "hlLjztqZ";
    while (strchr (modifiers, *p))
      p++;
    // conversion
    const char *conversion_chars = "dioucCspmXxEeFfGgAa%";
    if (!strchr (conversion_chars, *p))
      return "missing conversion specifier";
    if (value_index == 0 && !strchr ("m%", *p))
      value_index = index++;
    conversion = *p++;
    if (conversion == 'C')   // %lc in SUSv2
      conversion = 'c';
    // success
    *indexp = index;
    *stringp = p;
    return nullptr; // OK
  }
  std::string
  render_directive (const size_t N, const StringFormatArg *args) const
  {
    switch (conversion)
      {
      case 'm':
        return render_value (N, args, "", int (0)); // dummy arg to silence compiler
      case 'p':
        return render_value (N, args, "", arg_as_ptr (N, args, value_index));
      case 's': // precision
        return render_value (N, args, "", arg_as_chars (N, args, value_index));
      case 'c': case 'd': case 'i': case 'o': case 'u': case 'X': case 'x':
        return render_value (N, args, "ll", arg_as_longlong (N, args, value_index));
      case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
        return render_value (N, args, "", arg_as_double (N, args, value_index));
      case '%':
        return "%";
      }
    return std::string ("%") + conversion;
  }
  static const StringFormatArg&
  format_arg (const size_t N, const StringFormatArg *args, size_t nth)
  {
    if (nth && nth <= N)
      return args[nth-1];
    static const StringFormatArg zero_arg;
    return zero_arg;
  }
  static const char*
  arg_as_chars (const size_t N, const StringFormatArg *args, size_t nth)
  {
    if (!(nth && nth <= N))
      return "";
    const StringFormatArg &arg = format_arg (N, args, nth);
    if (auto *p = std::get_if<const char*> (&arg))
      return *p ? *p : "(null)";
    if (auto *p = std::get_if<uint64_t> (&arg); *p == 0)
      return "(null)";
    return "";
  }
  static void*
  arg_as_ptr (const size_t N, const StringFormatArg *args, size_t nth)
  {
    return (void*) ptrdiff_t (arg_as_longlong (N, args, nth));
  }
  static uint32_t
  arg_as_width (const size_t N, const StringFormatArg *args, size_t nth)
  {
    int32_t w = arg_as_longlong (N, args, nth);
    w = std::abs (w);
    return w < 0 ? std::abs (w + 1) : w; // turn -2147483648 into +2147483647
  }
  static uint32_t
  arg_as_precision (const size_t N, const StringFormatArg *args, size_t nth)
  {
    const int32_t precision = arg_as_longlong (N, args, nth);
    return std::max (0, precision);
  }
  static ll_t
  arg_as_longlong (const size_t N, const StringFormatArg *args, size_t nth)
  {
    const StringFormatArg &arg = format_arg (N, args, nth);
    switch (arg.index())
      {
      case 0:   return std::get<uint64_t> (arg);
      case 1:   return ll_t (std::get<double> (arg));
      case 2:   return ll_t (std::get<const char*> (arg));
      }
    return 0;
  }
  static double
  arg_as_double (const size_t N, const StringFormatArg *args, size_t nth)
  {
    const StringFormatArg &arg = format_arg (N, args, nth);
    switch (arg.index())
      {
      case 0:   return std::get<uint64_t> (arg);
      case 1:   return std::get<double> (arg);
      case 2:   return double (ptrdiff_t (std::get<const char*> (arg)));
      }
    return 0.0;
  }
  template<class Value> std::string
  render_value (const size_t N, const StringFormatArg *args, const char *modifier, Value value) const
  {
    std::string format;
    const int field_width = !use_width || !width_index ? this->field_width : arg_as_width (N, args, width_index);
    const int field_precision = !use_precision || !precision_index ? std::max (uint32_t (0), precision) : arg_as_precision (N, args, precision_index);
    // format directive
    format += '%';
    if (adjust_left)
      format += '-';
    if (add_sign)
      format += '+';
    if (add_space)
      format += ' ';
    if (zero_padding && !adjust_left && strchr ("diouXx" "FfGgEeAa", conversion))
      format += '0';
    if (alternate_form && strchr ("oXx" "FfGgEeAa", conversion))
      format += '#';
    if (locale_grouping && strchr ("idu" "FfGg", conversion))
      format += '\'';
    if (use_width)
      format += '*';
    if (use_precision && strchr ("sm" "diouXx" "FfGgEeAa", conversion)) // !cp
      format += ".*";
    if (modifier)
      format += modifier;
    format += conversion;
    // printf formatting
    if (use_width && use_precision)
      return system_string_printf (format.c_str(), field_width, field_precision, value);
    else if (use_precision)
      return system_string_printf (format.c_str(), field_precision, value);
    else if (use_width)
      return system_string_printf (format.c_str(), field_width, value);
    else
      return system_string_printf (format.c_str(), value);
  }
};

static size_t
upper_directive_count (const char *format)
{
  size_t n = 0;
  for (const char *p = format; *p; p++)
    if (p[0] == '%')            // count %...
      {
        n++;
        if (p[1] == '%')        // dont count %% twice
          p++;
      }
  return n;
}

static std::string
format_error (const char *err, const char *format, size_t directive)
{
  const char *cyan = "", *cred = "", *cyel = "", *crst = "";
  if (isatty (fileno (stderr)))
    {
      const char *term = getenv ("TERM");
      if (term && strcmp (term, "dumb") != 0)
        {
          cyan = "\033[36m";
          cred = "\033[31m\033[1m";
          cyel = "\033[33m";
          crst = "\033[39m\033[22m";
        }
    }
  if (directive)
    fprintf (stderr, "%sStringFormatter: %sWARNING:%s%s %s in directive %zu:%s %s\n", cyan, cred, crst, cyel, err, directive, crst, format);
  else
    fprintf (stderr, "%sStringFormatter: %sWARNING:%s%s %s:%s %s\n", cyan, cred, crst, cyel, err, crst, format);
  return format;
}

std::string
StringFormatArg::string_format_args (const char *format, const size_t N, const StringFormatArg *args)
{
  if (!format)
    return format_error ("format is nullptr", "<nullptr>", 0);
  // allocate enough space to hold all directives possibly contained in format
  const size_t max_dirs = 1 + upper_directive_count (format);
  StringFormatDirective fdirs[max_dirs];
  // parse format into Directive stack
  size_t nextarg = 1, ndirs = 0;
  const char *p = format;
  while (*p)
    {
      do
        {
          if (p[0] == '%')
            break;
          p++;
        }
      while (*p);
      if (*p == 0)
        break;
      const size_t start = p - format;
      const char *err = fdirs[ndirs].parse_directive (&p, &nextarg);
      if (err)
        return format_error (err, format, ndirs + 1);
      fdirs[ndirs].start = start;
      fdirs[ndirs].end = p - format;
      ndirs++;
      TASSERT (ndirs < max_dirs);
    }
  const size_t argcounter = nextarg - 1;
  fdirs[ndirs].end = fdirs[ndirs].start = p - format;
  // check maximum argument reference and argument count
  size_t argmaxref = argcounter;
  for (size_t i = 0; i < ndirs; i++)
    {
      const StringFormatDirective &fdir = fdirs[i];
      argmaxref = std::max (argmaxref, size_t (fdir.value_index));
      argmaxref = std::max (argmaxref, size_t (fdir.width_index));
      argmaxref = std::max (argmaxref, size_t (fdir.precision_index));
    }
  if (argmaxref > N)
    return format_error ("too few arguments for format", format, 0);
  if (argmaxref < N)
    return format_error ("too many arguments for format", format, 0);
  // format pieces
  std::string result;
  p = format;
  for (size_t i = 0; i <= ndirs; i++)
    {
      const StringFormatDirective &fdir = fdirs[i];
      result += std::string (p, fdir.start - (p - format));
      if (fdir.conversion)
        result += fdir.render_directive (N, args);
      p = format + fdir.end;
    }
  return result;
}

} // Impl

} // Ase

// == Testing ==
namespace { // Anon
struct UncopyablePoint {
  double x, y;
  friend inline std::ostream&
  operator<< (std::ostream &s, const UncopyablePoint &p) { return s << "{" << p.x << ";" << p.y << "}"; }
  UncopyablePoint (double _x, double _y) : x (_x), y (_y) {}
  ASE_CLASS_NON_COPYABLE (UncopyablePoint);
};

TEST_INTEGRITY (ase_string_format);
static void
ase_string_format()
{
  using namespace Ase;
  std::atomic<bool> boolean = 1;
  // string_format
  TCMP (string_format ("%d", bool (1)), ==, "1");
  TCMP (string_format ("%d", char (-17)), ==, "-17");
  TCMP (string_format ("%s", nullptr), ==, "(null)");
  TCMP (string_format ("%s", boolean), ==, "1");
  TCMP (string_format ("%s", (char*) "FOO"), ==, "FOO");
  TCMP (string_format ("%s", (void*) "FOO"), ==, "");
  TCMP (string_format ("%d %s", -9223372036854775808uLL, "FOO"), ==, "-9223372036854775808 FOO");
  TCMP (string_format ("0x%08x", 0xc0ffee), ==, "0x00c0ffee");
  enum { TEST17 = 17 };
  TCMP (string_format ("%g %d", 0.5, TEST17), ==, "0.5 17");
  static_assert (TEST17 == 17, "!");
  TCMP (string_format ("Only %c%%", '3'), ==, "Only 3%");
  // ostream tests
  UncopyablePoint point { 1, 2 };
  TCMP (string_format ("%s", point), ==, "{1;2}");
  TCMP (string_format ("%s+%s+%s", point, point, point), ==, "{1;2}+{1;2}+{1;2}");
  String sfoo ("foo");
  typedef char MutableChar;
  MutableChar *foo = &sfoo[0];
  TCMP (string_format ("%s", foo), ==, "foo");
  // test robustness for arcane/rarely-used width modifiers
  const char *arcane_format = "| %qd %Zd %LF |";
  TCMP (string_format (arcane_format, (long long) 1234, size_t (4321), (long double) 1234.), ==, "| 1234 4321 1234.000000 |");
  TCMP (string_format ("- %C - %lc -", long ('X'), long ('x')), ==, "- X - x -");
  // TCMP (string_format ("+ %S +", (wchar_t*) "\1\1\1\1\0\0\0\0"), ==, "+ \1\1\1\1 +");
}

} // Anon
