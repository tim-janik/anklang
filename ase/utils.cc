// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "utils.hh"
#include "platform.hh"
#include "memory.hh"
#include "unicode.hh"
#include "internal.hh"
#include <signal.h>
#include <sys/time.h>
#include <poll.h>
#include <fcntl.h>
#include <cmath>
#include <atomic>
#include <unistd.h>

namespace Ase {

// == Debugging ==
bool ase_debugging_enabled = true;
bool ase_fatal_warnings = false;
static const char*
getenv_ase_debug()
{
  // cache $ASE_DEBUG and setup debug_any_enabled;
  static const char *const ase_debug = [] {
    const char *const d = getenv ("ASE_DEBUG");
    ase_debugging_enabled = d && d[0];
    ase_fatal_warnings = string_to_bool (String (string_option_find_value (d, "fatal-warnings", "0", "0", true)));
    // ase_sigquit_on_abort = string_to_bool (String (string_option_find_value (d, "sigquit-on-abort", "0", "0", true)));
    // ase_backtrace_on_error = string_to_bool (String (string_option_find_value (d, "backtrace", "0", "0", true)));
    return d;
  }();
  return ase_debug;
}

/// Check if `conditional` is enabled by $ASE_DEBUG.
bool
debug_key_enabled (const char *conditional)
{
  const std::string_view sv = string_option_find_value (getenv_ase_debug(), conditional, "0", "0", true);
  return string_to_bool (String (sv));
}

/// Check if `conditional` is enabled by $ASE_DEBUG.
bool
debug_key_enabled (const ::std::string &conditional)
{
  return debug_key_enabled (conditional.c_str());
}

/// Retrieve the value assigned to debug key `conditional` in $ASE_DEBUG.
::std::string
debug_key_value (const char *conditional)
{
  const std::string_view sv = string_option_find_value (getenv_ase_debug(), conditional, "", "", true);
  return String (sv);
}

/// Print a debug message, called from ::Ase::debug().
void
debug_message (const char *cond, const std::string &message)
{
  return_unless (cond && debug_key_enabled (cond));
  struct timeval tv = { 0, };
  gettimeofday (&tv, NULL);
  const char *const newline = !message.empty() && message.data()[message.size() - 1] == '\n' ? "" : "\n";
  using namespace AnsiColors;
  const std::string col = color (FG_CYAN, BOLD), reset = color (RESET);
  const std::string ul = cond ? color (UNDERLINE) : "", nl = cond ? color (UNDERLINE_OFF) : "";
  std::string sout;
  sout += string_format ("%s%u.%06u ", col, tv.tv_sec, tv.tv_usec); // cyan timestamp
  sout += string_format ("%s%s%s:", ul, cond ? cond : executable_name().c_str(), nl); // underlined cond
  sout += string_format ("%s %s", reset, message); // normal print message
  printerr ("%s%s", sout, newline);
}

/// Handle stdout and stderr printing with flushing.
void
diag_flush (uint8 code, const String &txt)
{
  fflush (stdout);      // preserve output ordering
  fputs (txt.c_str(), code == 'e' ? stderr : stdout);
  fflush (stderr);      // some platforms (_WIN32) don't properly flush on '\n'
}

/// Create prefix for warnings and errors.
String
diag_prefix (uint8 code)
{
  using namespace AnsiColors;
  String prefix;
  if (code == 'W')
    prefix = color (FG_YELLOW) + "warning:" + color (RESET) + " ";
  else if (code == 'F')
    prefix = color (BG_RED, FG_WHITE, BOLD) + "error:" + color (RESET) + " ";
  std::string executable = executable_path();
  if (!executable.empty())
    {
      size_t sep = executable.find (" ");
      if (sep != std::string::npos)
        executable = executable.substr (0, sep);        // strips electron command line args
      sep = executable.rfind ("/");
      if (sep != std::string::npos)
        executable = executable.substr (sep + 1);       // use simple basename
      if (!executable.empty())
        prefix = executable + ": " + prefix;
    }
  return prefix;
}

// == i18n & gettext ==
const char*
ase_gettext (const String &untranslated)
{
  CString translated = untranslated;
  return translated.c_str(); // relies on global CString storage
}

// == atquit ==
static std::mutex atquit_mutex;
static std::vector<std::function<void()>*> atquit_funcs;

void
atquit_add (std::function<void()> *func)
{
  std::lock_guard<std::mutex> locker (atquit_mutex);
  if (func)
    atquit_funcs.push_back (func);
}

void
atquit_del (std::function<void()> *func)
{
  std::lock_guard<std::mutex> locker (atquit_mutex);
  Aux::erase_first (atquit_funcs, [func] (std::function<void()> *ele) { return func == ele; });
}

static std::atomic<uint8_t> atquit_triggered_ = false;

void
atquit_run (int exitcode)
{
  atquit_triggered_ = true;
  while (atquit_funcs.size())
    {
      std::function<void()> *func = atquit_funcs.back();
      atquit_funcs.pop_back();
      if (func) // intentional leakage
        (*func) ();
    }
  _Exit (exitcode);
}

bool
atquit_triggered ()
{
  return atquit_triggered_;
}

// == Date & Time ==
String
now_strftime (const String &format)
{
  std::time_t t = std::time (nullptr);
  char buffer[4096] = { 0, };
  if (std::strftime (buffer, sizeof (buffer), format.c_str(), std::localtime (&t)))
    return buffer;
  return "";
}

// == MakeIcon ==
namespace MakeIcon {

/// Create an IconString consisting of keywords
IconString
KwIcon (const String &keywords)
{
  static const String keyword_charset = string_set_ascii_alnum() + "_-";
  String s = keywords;
  return_unless (!s.empty(), {});
  StringS words = string_split_any (keywords, " ,");
  Aux::erase_all (words, [] (const String &word) {
    if (word.empty()) return true;
    if (!string_is_canonified (word, keyword_charset))
      {
        warning ("%s: invalid icon keyword: '%s'", __func__, word);
        return true;
      }
    return false;
  });
  IconString is;
  is.assign (string_join (", ", words));
  if (!is.empty() && is.find (',') == is.npos)
    is += ",";  // ensure comma in keyword list
  return is;
}

/// Create an IconString consisting of keywords
IconString
operator""_icon (const char *key, size_t)
{
  return KwIcon (key);
}

/// Create an IconString consisting of a single/double unicode character
IconString
UcIcon (const String &unicode)
{
  std::vector<uint32_t> codepoints;
  if (utf8_to_unicode (unicode, codepoints) > 3 ||
      (codepoints.size() >= 1 && !unicode_is_character (codepoints[0])) ||
      (codepoints.size() >= 2 && !unicode_is_character (codepoints[1])) ||
      (codepoints.size() >= 3 && !unicode_is_character (codepoints[2])))
    warning ("%s: invalid icon unicode: '%s'", __func__, unicode);
  IconString is;
  is.assign (unicode);
  return is;
}

/// Create an IconString consisting of a single/double unicode character
IconString
operator""_uc (const char *key, size_t)
{
  return UcIcon (key);
}

/// Create an IconString consisting of an SVG string
IconString
SvgIcon (const String &svgdata)
{
  return_unless (!svgdata.empty(), {});
  if (!string_startswith (svgdata, "<svg") || !string_startswith (svgdata, "<SVG"))
    warning ("%s: invalid svg icon: %s…", __func__, svgdata.substr (0, 40));
  IconString is;
  is.assign (svgdata);
  return is;
}

} // MakeIcon

// == EventFd ==
EventFd::EventFd () :
  fds { -1, -1 }
{}

int
EventFd::open ()
{
  if (opened())
    return 0;
  ASE_UNUSED long nflags;
#ifdef HAVE_SYS_EVENTFD_H
  do
    fds[0] = eventfd (0 /*initval*/, EFD_CLOEXEC | EFD_NONBLOCK);
  while (fds[0] < 0 && (errno == EAGAIN || errno == EINTR));
#else
  int err;
  do
    err = pipe2 (fds, O_CLOEXEC | O_NONBLOCK);
  while (err < 0 && (errno == EAGAIN || errno == EINTR));
  if (fds[1] >= 0)
    {
      nflags = fcntl (fds[1], F_GETFL, 0);
      assert_return (nflags & O_NONBLOCK, -1);
      nflags = fcntl (fds[1], F_GETFD, 0);
      assert_return (nflags & FD_CLOEXEC, -1);
    }
#endif
  if (fds[0] >= 0)
    {
      nflags = fcntl (fds[0], F_GETFL, 0);
      assert_return (nflags & O_NONBLOCK, -1);
      nflags = fcntl (fds[0], F_GETFD, 0);
      assert_return (nflags & FD_CLOEXEC, -1);
      return 0;
    }
  return -errno;
}

int
EventFd::inputfd () // fd for POLLIN
{
  return fds[0];
}

bool
EventFd::opened ()
{
  return inputfd() >= 0;
}

bool
EventFd::pollin ()
{
  struct pollfd pfd = { inputfd(), POLLIN, 0 };
  int presult;
  do
    presult = poll (&pfd, 1, -1);
  while (presult < 0 && (errno == EAGAIN || errno == EINTR));
  return pfd.revents != 0;
}

void
EventFd::wakeup()
{
  int err;
#ifdef HAVE_SYS_EVENTFD_H
  do
    err = eventfd_write (fds[0], 1);
  while (err < 0 && errno == EINTR);
#else
  char w = 'w';
  do
    err = write (fds[1], &w, 1);
  while (err < 0 && errno == EINTR);
#endif
  // EAGAIN occours if too many wakeups are pending
}

void
EventFd::flush()
{
  int err;
#ifdef HAVE_SYS_EVENTFD_H
  eventfd_t bytes8;
  do
    err = eventfd_read (fds[0], &bytes8);
  while (err < 0 && errno == EINTR);
#else
  char buffer[512]; // 512 is POSIX pipe atomic read/write size
  do
    err = read (fds[0], buffer, sizeof (buffer));
  while (err == 512 || (err < 0 && errno == EINTR));
#endif
  // EAGAIN occours if no wakeups are pending
}

EventFd::~EventFd ()
{
#ifdef HAVE_SYS_EVENTFD_H
  close (fds[0]);
#else
  close (fds[0]);
  close (fds[1]);
#endif
  fds[0] = -1;
  fds[1] = -1;
}

// == CustomDataContainer ==
CustomDataContainer::CustomDataEntry&
CustomDataContainer::custom_data_entry (VirtualBase *key)
{
  if (!custom_data_)
    custom_data_ = std::make_unique<CustomDataS> (1);
  assert_return (key != nullptr, *custom_data_->end());
  for (auto &e : *custom_data_)
    if (e.key == key)
      return e;
  custom_data_->push_back ({ .key = key, });
  return custom_data_->back();
}

std::any&
CustomDataContainer::custom_data_get (VirtualBase *key) const
{
  if (custom_data_)
    for (auto &e : *custom_data_)
      if (e.key == key)
        return e;
  static std::any dummy;
  return dummy;
}

bool
CustomDataContainer::custom_data_del (VirtualBase *key)
{
  if (custom_data_)
    return Aux::erase_first (*custom_data_, [key] (auto &e) { return key == e.key; });
  return 0;
  static_assert (sizeof (CustomDataContainer) == sizeof (void*));
}

void
CustomDataContainer::custom_data_destroy()
{
  return_unless (custom_data_);
  while (!custom_data_->empty())
    {
      std::any old;
      std::swap (old, custom_data_->back());
      custom_data_->pop_back();
      // destroy old *after* removal from vector
    }
}

CustomDataContainer::~CustomDataContainer()
{
  custom_data_destroy();
}

} // Ase

#include "testing.hh"

namespace { // Anon

TEST_INTEGRITY (utils_tests);
static void
utils_tests()
{
  using namespace Ase;
  TASSERT (uint16_swap_le_be (0x1234) == 0x3412);
  TASSERT (uint32_swap_le_be (0xe23456f8) == 0xf85634e2);
  TASSERT (uint64_swap_le_be (0xf2345678a1b2c3d4) == 0xd4c3b2a1785634f2);
  std::vector<float> fv { 1, -2, 0, -0.5, 2, -1 };
  bool b;
  b = Aux::contains (fv, [] (auto v) { return v == 9; }); TASSERT (b == false);
  b = Aux::contains (fv, [] (auto v) { return v == 2; }); TASSERT (b == true);
  size_t j;
  j = Aux::erase_all (fv, [] (auto v) { return fabs (v) == 2; });
  TASSERT (j == 2 && fv == (std::vector<float> { 1, 0, -0.5, -1 }));
  j = Aux::erase_first (fv, [] (auto v) { return fabs (v) == 1; });
  TASSERT (j == 1 && fv == (std::vector<float> { 0, -0.5, -1 }));
}

} // Anon
