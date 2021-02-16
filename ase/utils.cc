// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "utils.hh"
#include "platform.hh"
#include "memory.hh"
#include "internal.hh"
#include <signal.h>
#include <sys/time.h>
#include <poll.h>
#include <fcntl.h>
#include <cmath>
#include <unistd.h>

namespace Ase {

// == Debugging ==
bool ase_debugging_enabled = true;
static bool ase_fatal_warnings = false;

/// Check if `conditional` is enabled by $ASE_DEBUG.
bool
debug_key_enabled (const char *conditional)
{
  const std::string value = debug_key_value (conditional);
  return !value.empty() && (strchr ("123456789yYtT", value[0]) || strncasecmp (value.c_str(), "on", 2) == 0);
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
  // cache $ASE_DEBUG and setup debug_any_enabled;
  static const std::string debug_flags = [] () {
    const char *f = getenv ("ASE_DEBUG");
    const std::string flags = !f ? "" : ":" + std::string (f) + ":";
    ase_debugging_enabled = !flags.empty() && flags != ":none:";
    const ssize_t fw = flags.rfind (":fatal-warnings:");
    const ssize_t nf = flags.rfind (":no-fatal-warnings:");
    if (fw >= 0 && nf <= fw)
      ase_fatal_warnings = true;
    const ssize_t sq = flags.rfind (":sigquit-on-abort:");
    const ssize_t nq = flags.rfind (":no-sigquit-on-abort:");
    if (sq >= 0 && nq <= sq)
      ; // ase_sigquit_on_abort = true;
    const ssize_t wb = flags.rfind (":backtrace:");
    const ssize_t nb = flags.rfind (":no-backtrace:");
    if (wb > nb)
      ; // ase_backtrace_on_error = true;
    if (nb > wb)
      ; // ase_backtrace_on_error = false;
    return flags;
  } ();
  // find key in colon-separated debug flags
  const ::std::string key = conditional ? conditional : "";
  static const std::string all = ":all:", none = ":none:";
  const std::string condr = ":no-" + key + ":";
  const std::string condc = ":" + key + ":";
  const std::string conde = ":" + key + "=";
  const ssize_t pa = debug_flags.rfind (all);
  const ssize_t pn = debug_flags.rfind (none);
  const ssize_t pr = debug_flags.rfind (condr);
  const ssize_t pc = debug_flags.rfind (condc);
  const ssize_t pe = debug_flags.rfind (conde);
  const ssize_t pmax = std::max (pr, std::max (std::max (pa, pn), std::max (pc, pe)));
  if (pn == pmax || pr == pmax)
    return "false";     // found no key or ':none:' or ':no-key:'
  if (pa == pmax || pc == pmax)
    return "true";      // last setting is ':key:' or ':all:'
  // pe == pmax, assignment via equal sign
  const ssize_t pv = pe + conde.size();
  const ssize_t pw = debug_flags.find (":", pv);
  const std::string value = debug_flags.substr (pv, pw < 0 ? pw : pw - pv);
  return value;
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

// Mimick relevant parts of glibc's abort_msg_s
struct AbortMsg {
  const char *msg = NULL;
};
static AbortMsg abort_msg;

/// Handle various dignostics and stdout/stderr printing.
void
diag_message (uint8 code, const std::string &message)
{
  FILE *const output = code == 'o' ? stdout : stderr;
  String msg = message;
  if (code == 'W' || code == 'F')
    {
      using namespace AnsiColors;
      String prefix;
      if (code == 'W')
        prefix = color (FG_YELLOW) + "warning:" + color (RESET);
      else
        prefix = color (BG_RED, FG_WHITE, BOLD) + "error:" + color (RESET);
      msg = prefix + ' ' + msg;
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
            msg = executable + ": " + msg;
        }
      if (msg.size() && msg[msg.size()] != '\n')
        msg += '\n';
    }
  // some platforms (_WIN32) don't properly flush on '\n'
  fflush (output == stdout ? stderr : stdout); // preserve ordering
  fputs (msg.c_str(), output);
  fflush (output);
  if (code == 'F' || (code == 'W' && ase_fatal_warnings))
    {
      __sync_synchronize();
      Ase::abort_msg.msg = msg.c_str();
      __sync_synchronize();
      ::raise (SIGQUIT);        // evade apport
      ::abort();                // default action for SIGABRT is core dump
      ::_exit (-1);             // ensure noreturn
    }
}

// == i18n & gettext ==
const char*
ase_gettext (const String &untranslated)
{
  CString translated = untranslated;
  return translated.c_str(); // relies on global CString storage
}

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

// == JobQueue ==
JobQueue::JobQueue (const Caller &caller) :
  caller_ (caller)
{}

} // Ase

// == __abort_msg ==
::Ase::AbortMsg *ase_abort_msg = &::Ase::abort_msg;
#ifdef  __ELF__
// allow 'print __abort_msg->msg' when debugging core files for apport/gdb to pick up
extern "C" ::Ase::AbortMsg *__abort_msg __attribute__ ((weak, alias ("ase_abort_msg")));
#endif // __ELF__

#include "testing.hh"

namespace { // Anon
using namespace Ase;

TEST_INTEGRITY (utils_tests);
static void
utils_tests()
{
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
