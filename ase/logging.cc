// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "logging.hh"
#include "platform.hh"
#include "path.hh"
#include "strings.hh"
#include "regex.hh"
#include <fcntl.h>
#include <cstdarg>
#include <cstring>
#ifndef NDEBUG
#include <execinfo.h>
#endif
#ifdef ASE_WITH_CPPTRACE
#include <cpptrace/cpptrace.hpp>
#endif
#include "internal.hh"

/** Logging levels
 *
 * | Type          |  A  |  V  |  L  |  C  | Notes                            |
 * |---------------|-----|-----|-----|-----|----------------------------------|
 * | **NONE**      |  -  |  -  |  -  |  -  | Logging disabled                 |
 * | **DEBUG**     |  -  |  -  |  ✲  |  ◌  | Developer decisions              |
 * | **TRACE**     |  -  |  -  | 📍  |  ◌  | Flow tracing, light              |
 * | **DIAG**      |  -  |  -  |  -  |  ✔  | State dumps, actions taken       |
 * | **INFO**      |  -  | 📢  |  -  |  ✔  | Normal lifecycle events          |
 * | **HINT**      |  -  | 📢  |  -  |  ✔  | Guidance / suggestions           |
 * | **WARNING**   | ⚠️   | 📢  |  -  |  ✔  | Unexpected but handled           |
 * | **PARANOID**  | ⚠️   | 📢  | 📍  |  ◌  | Slow assertions; only in debug   |
 * | **ASSERTION** | ⚠️   | 📢  | 📍  |  ✔  | Runtime assertions               |
 * | **FATAL**     | 🚨  | 📢  | 📍  |  ✔  | Immediate termination            |
 *
 * Notes:
 * - T: Temporal: execution-flow; "Where / why did this code run?"
 * - S: Spatial: state-oriented; "What does the system look like right now?"
 * - V: Verbosity; 📢: Included in default log level
 * - A: Abort program: never, configurable (fatal-warnings) or always;
 *   🚨: Always abort, ⚠️ : Only abort on fatal-warnings
 * - L: Location; 📍: Show source code location, ✲: conditional location
 * - C: Compilation; ◌: Compiled unless NDEBUG
 * - UNREACHABLE: Variant of ASSERTION
 * - CRITICAL: Variant of WARNING
 * - ERROR: Variant of WARNING (or FATAL if non-recoverable)
 * - PANIC: Variant of FATAL
 */

namespace Ase {

[[noreturn]] static void abort_debug_friendly (const char *msg, const char *file, int line, const char *func) noexcept;

// == stdout stderr ==
/// Handle stdout and stderr printing with flushing.
void
stdio_flush (uint8 code, const String &txt) noexcept
{
  fflush (code != 'e' ? stderr : stdout);       // preserve output ordering
  fputs (txt.c_str(), code == 'e' ? stderr : stdout);
  fflush (code == 'e' ? stderr : stdout);
}

// == Timestamp ==
uint64_t
timestamp_now ()
{
  struct timeval tv = { 0, 0 };
  gettimeofday (&tv, nullptr);
  return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

struct StartupStats {
  uint64_t      timestamp = 0;
};

static const StartupStats&
logging_startup_stats()
{
  static StartupStats stats = {
    .timestamp = timestamp_now(),
  };
  return stats;
}
static bool init_startup_stats = [] { logging_startup_stats(); return 1; } ();

static std::string
logging_timestamp (uint64_t stamp)
{
  const unsigned long long secs = stamp / 1000000ULL;
  const unsigned long long usec = stamp - (secs * 1000000ULL);
  return string_format ("%llu.%06llu", secs, usec);
}

// == Log File ==
static std::mutex logging_buffer_mutex;
static std::vector<std::string> *logging_buffer = nullptr;
static int logging_fd = -2;

static void
loging_setup()
{
  if (logging_fd >= -1)
    return;
  std::lock_guard<std::mutex> locker (logging_buffer_mutex);
  if (logging_fd >= -1)
    return;
  const uint64 programstart_timestamp = logging_startup_stats().timestamp;
  if (!logging_buffer) {
    logging_buffer = new std::vector<std::string>();
    const time_t now = programstart_timestamp / 1000000;
    struct tm stm{};
    localtime_r (&now, &stm);
    char tbuf[128] = { 0, };
    strftime (tbuf, sizeof (tbuf) - 1, "%Y-%m-%d %H:%M:%S %z", &stm);
    char pidbuf[64] = { 0, };
    snprintf (pidbuf, sizeof (pidbuf) - 1, " pid=%u", getpid());
    auto start_msg = logging_timestamp (programstart_timestamp) + " " + executable_name() + ": programstart=\"" + tbuf + "\"" + pidbuf + " executable=\"" + executable_path() + "\"\n";
    logging_buffer->push_back (start_msg);
  }
  logging_fatal_warnings |= string_to_bool (String (string_option_find_value (getenv_ase_debug(), "fatal-warnings", "0", "0", true)));
  // sigquit_on_abort = string_to_bool (String (string_option_find_value (d, "sigquit-on-abort", "0", "0", true)));
}

static void
logging_to_file (const std::string &lines)
{
  loging_setup();
  if (logging_fd == -1)
    return;
  if (logging_fd >= 0) {
    long r;
    do
      r = write (logging_fd, lines.data(), lines.size());
    while (r < 0 && (errno == EINTR || errno == EAGAIN));
    return;
  }
  std::lock_guard<std::mutex> locker (logging_buffer_mutex);
  if (logging_buffer)
    logging_buffer->push_back (lines);
}

const char*
rfind_debug_value (const char *kvlist, const char *key, const char *fallback)
{
  const std::string_view sv = string_option_find_value (kvlist, key, fallback, fallback, false);
  return sv.data();
}

static Logging logging_level = TRACE;

static Logging
parse_log_level (const char *lvl, Logging fallback)
{
  static const struct { const char *name; Logging level; } levels[] = {
    { "FATAL",     FATAL     },
    { "ASSERTION", ASSERTION },
    { "HINT",      HINT      },
    { "INFO",      INFO      },
    { "DIAG",      DIAG      },
    { "TRACE",     TRACE     },
    { "DEBUGALL",  DEBUGALL  },
  };
  for (int i = 0; i < sizeof (levels) / sizeof (levels[0]); i++)
    if (strncasecmp (lvl, levels[i].name, strlen (levels[i].name)) == 0)
      return levels[i].level;
  return fallback;
}

bool
logging_configure (bool to_file, Logging level)
{
  logging_level = level;
  if (logging_level < FATAL)
    logging_level = parse_log_level (rfind_debug_value (getenv_ase_debug(), "loglevel", ""), INFO);
  loging_setup();
  if (!to_file) {
    std::lock_guard<std::mutex> locker (logging_buffer_mutex);
    if (logging_fd >= 0)
      return false;     // logging file already configured
    logging_fd = -1;
    if (logging_buffer) {
      delete logging_buffer;
      logging_buffer = nullptr;
    }
    return true;
  }
  const String logdir = Path::join (Path::xdg_dir ("CACHE"), "anklang");
  const String fname = string_format ("%s/%s-%08x.log", logdir, program_alias(), gethostid());
  const int OFLAGS = O_CREAT | O_EXCL | O_WRONLY | O_NOCTTY | O_NOFOLLOW | O_CLOEXEC; // O_TRUNC
  const int OMODE = 0640;
  std::lock_guard<std::mutex> locker (logging_buffer_mutex);
  if (logging_fd >= 0)
    return false;       // logging file already configured
  errno = EBUSY;
  if (!Path::mkdirs (logdir)) {
    perror (string_format ("%s: failed to open log dir \"%s\"", program_alias(), logdir.c_str()).c_str());
    return false;
  }
  logging_fd = open (fname.c_str(), OFLAGS, OMODE);
  if (logging_fd < 0 && errno == EEXIST) {
    const String oldname = fname + ".old";
    if (rename (fname.c_str(), oldname.c_str()) < 0) {
      perror (string_format ("%s: failed to rename \"%s\"", program_alias(), oldname.c_str()).c_str());
      return false;
    }
    logging_fd = open (fname.c_str(), OFLAGS, OMODE);
  }
  if (logging_fd < 0) {
    perror (string_format ("%s: failed to open log file \"%s\"", program_alias(), fname.c_str()).c_str());
    return false;
  }
  // flush buffered messages
  if (logging_buffer) {
    long r;
    for (const auto &lines : *logging_buffer)
      do
        r = write (logging_fd, lines.data(), lines.size());
      while (r < 0 && (errno == EINTR || errno == EAGAIN));
    delete logging_buffer;
    logging_buffer = nullptr;
  }
  return true;
}

// == Stacktrace ==
/// Find GDB and construct command line
[[maybe_unused]] static std::string
backtrace_command (const char *dbgr)
{
#if 0 && defined (__linux__)
  bool allow_ptrace = true;
  // disabling this check, so the debugger can show an appropriate error message
  const char *const ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope";
  int fd = open (ptrace_scope, 0);
  char b[8] = { 0 };
  if (read (fd, b, 8) > 0)
    allow_ptrace = b[0] == '0';
  close (fd);
  if (!allow_ptrace)
    return "";
#endif
  char cmd[3192];
  const char *const usr_bin_lldb = "/usr/bin/lldb";
  if ((!dbgr || strcmp (dbgr, "lldb") == 0) &&
      access (usr_bin_lldb, X_OK) == 0) {
    snprintf (cmd, sizeof (cmd),
              "%s -Q -x --batch -p %u "
              "-o 'bt'", // 'bt all'
              usr_bin_lldb, gettid());
    return cmd;
  }
  const char *const usr_bin_gdb = "/usr/bin/gdb";
  if ((!dbgr || strcmp (dbgr, "gdb") == 0) &&
      access (usr_bin_gdb, X_OK) == 0) {
    snprintf (cmd, sizeof (cmd),
              "%s -q -n --nx -p %u --batch "
              "-iex 'set auto-load python-scripts off' "
              "-iex 'set script-extension off' "
              "-ex 'set print address off' "
              // "-ex 'set print frame-arguments none' "
              "-ex 'backtrace 99' " // "-ex 'thread apply all backtrace 99' "
              ">&2 2>/dev/null",
              usr_bin_gdb, gettid());
    return cmd;
  }
  return "";
}

// == Debugging ==
/// Flag to optimize checks for debugging.
bool ase_debugging_enabled = true;

/// Global flag to cause the program to abort on warnings.
bool logging_fatal_warnings = false;

const char*
getenv_ase_debug()
{
  // cache $ASE_DEBUG and setup debug_any_enabled;
  static const char *const ase_debug = [] {
    const char *const d = getenv ("ASE_DEBUG");
    ase_debugging_enabled = d && d[0];
    return d ? d : "";
  }();
  return ase_debug;
}

/// Check if `conditional` is enabled by $ASE_DEBUG.
bool
debug_key_enabled (const char *conditional) noexcept
{
  const std::string_view sv = string_option_find_value (getenv_ase_debug(), conditional, "0", "0", true);
  return string_to_bool (String (sv));
}

/// Retrieve the value assigned to debug key `conditional` in $ASE_DEBUG.
::std::string
debug_key_value (const char *conditional)
{
  const std::string_view sv = string_option_find_value (getenv_ase_debug(), conditional, "", "", true);
  return String (sv);
}

#ifndef NDEBUG
static void
print_backtrace (FILE *stdio, const std::vector<void*> &frames)
{
  using namespace AnsiColors;
  const auto G = color (FG_GREEN), U = color (FG_BLUE), E = color (FG_RED), R = color (RESET);
  const std::size_t n = frames.size();
  char **symbols = backtrace_symbols (&frames[0], n);
  if (!symbols) return;
  static constexpr size_t MAXLEN = 4096;
  char exe[MAXLEN], symb[MAXLEN], addr[MAXLEN];
  bool indots = false;
  for (std::size_t i = 0; i < n; i++) {
    if (strlen (symbols[i]) <= MAXLEN) {
      // scan: /bin/executable(_ZMangled+0x123) [0x456]
      if (sscanf (symbols[i], "%[^(](%[^)]) [%[^]]", exe, symb, addr) == 3) {
        char *offs = strchr (symb, '+');
        if (offs)
          *offs = 0;
        std::string demangled = symb[0] ? cxx_demangle (symb) : "";
        if (demangled.size()) {
          if (demangled == symb)
            demangled += "()";
          fprintf (stdio, "#%zu%s %s in %s%s%s from %s\n", i, i < 10 ? " " : "", (U + addr + R).c_str(), (E + demangled + R).c_str(), offs[0] ? "+" : "", offs, (G + exe + R).c_str());
          indots = false;
          continue;
        }
      }
    }
    if (0) { // ugly fallback
      fprintf (stdio, "#%zu%s %s\n", i, i < 10 ? " " : "", symbols[i]);
      continue;
    }
    if (!indots) {
      fprintf (stdio, "      ...\n");
      indots = true;
    }
  }
  free (symbols);
}
#endif // NDEBUG

/// Print confiogurable stack trace.
static void
logging_print_backtrace (const char *func)
{
  fflush (stdout);
  fflush (stderr);
#ifndef NDEBUG
  const char *asedebug = getenv ("ASE_DEBUG"), *btr = asedebug ? strstr (asedebug, "backtrace") : nullptr;
  std::string xdb_cmd;
  if (btr) {
    const char *xdb = !strncmp (&btr[9], "=lldb", 5) ? "lldb" : !strncmp (&btr[9], "=gdb", 4) ? "gdb" : nullptr;
    xdb_cmd = backtrace_command (xdb);
  }
  if (!xdb_cmd.empty()) {
    (void) system (xdb_cmd.c_str());
    return;
  }
#ifdef ASE_WITH_CPPTRACE
  cpptrace::generate_trace().print();
  fflush (stdout);
  fflush (stderr);
  return;
#endif // ASE_WITH_CPPTRACE
  {
    std::vector<void*> addrs (128);
    const int n = backtrace (&addrs[0], addrs.size());
    addrs.resize (n);
    if (n) {
      fprintf (stderr, "Stack Trace (most recent call first):\n");
      print_backtrace (stderr, addrs);
      fflush (stdout);
      fflush (stderr);
      return;
    }
  }
#endif // NDEBUG
  fprintf (stderr, "Stack Trace:\n");
  fprintf (stderr, "#0 %p in %s\n", __builtin_return_address (0), func);
  fprintf (stderr, "      ...\n");
  fflush (stderr);
}

/// Handle std::terminate() and print stack trace for uncaught exceptions
[[noreturn]] static void
logging_terminate_handler()
{
  String msg, what;
  if (auto eptr = std::current_exception()) {
    try {
      std::rethrow_exception (eptr);
    } catch (const std::exception &e) {
      msg = "Uncaught exception";
      what = cxx_demangle (typeid (e).name());
      if (e.what())
        what += std::string (": ") + e.what();
    } catch (...) {
      msg = "Uncaught non-std exception";
    }
  } else
    msg = "Terminate called without exception";
  {
    using namespace AnsiColors;
    const auto R = color (FG_RED), B = color (BOLD), S = color (RESET);
    ScopedPosixLocale posix_locale; // use POSIX locale for this scope
    msg = B + executable_name() + ":" + S + " " + R + msg;
    if (what.size())
      msg += ":" + S + " " + what;
    else
      msg += S;
  }
  fflush (stdout);
  fputs (string_format ("%s\n", msg).c_str(), stderr);
  logging_print_backtrace (__func__);
  abort();
}

void
logging_handle_terminate ()
{
  std::set_terminate (logging_terminate_handler);
#ifdef ASE_WITH_CPPTRACE
  cpptrace::register_terminate_handler();
#endif
}

static void
logging (Logging level, const std::string &cond, std::string message, const char *filename, uint32_t line, const char *function_name) noexcept
{
  const bool pabort = level == FATAL || (logging_fatal_warnings && level <= WARN);
  if (!pabort && level > logging_level)
    return;
  loging_setup();
  using namespace AnsiColors;
  const auto C = color (FG_CYAN), G = color (BOLD, FG_GREEN), U = color (FG_BLUE), Y = color (FG_YELLOW);
  const auto R = color (FG_RED), B = color (BOLD), S = color (RESET);
  ScopedPosixLocale posix_locale; // use POSIX locale for this scope
  auto location = [&] {
    String s;
    if (filename) {
      s = B + filename;
      s += ":" + string_from_uint (line) + ":" + S;
      if (function_name)
        s += function_name + String (":");
    } else
      s = B + executable_name() + ":" + S;
    return s;
  };
  String logprefix = logging_timestamp (timestamp_now()) + ':', printprefix = Y + logprefix + S, kind;
  switch (level)
    {
    case FATAL:
      printprefix = "";
      kind = location() + ' ' + B + R + "fatal:" + S;
      break;
    case ASSERTION:
      printprefix = "";
      kind = location() + ' ' + B + R + "assertion failed:" + S;
      if (message.empty())
        message = R + "code unreached" + S;
      break;
    case WARN:
      printprefix = executable_name() + ':';
      kind = Y + "warning:" + S;
      break;
    case HINT:
      printprefix = "";
      kind = C + "Hint:" + S;
      break;
    case INFO:
      kind = executable_name() + ':';
      break;
    case DIAG:
      kind = executable_name() + ':';
      break;
    case TRACE:
      kind = location();
      break;
    case DEBUGALL:
      kind = cond.empty() ? location() : U + cond + ':' + S;
      break;
    }
  String sout = printprefix.empty() ? "" : printprefix + ' ';
  if (kind.size())
    sout += kind + ' ';
  if (AnsiColors::colorize_tty()) {
    const std::string HEXINT = "0[xX][0-9abcdefABCDEF]+";
    const std::string FULLFLOAT = "([1-9][0-9]*|0)([.][0-9]*)?([eE][+-]?[0-9]+)?";
    const std::string FRACTFLOAT = "[.][0-9]+([eE][+-]?[0-9]+)?";
    const std::string NUMBER = HEXINT + "|" + FULLFLOAT + "|" + FRACTFLOAT;
    std::string w = message;
    w = Re::sub ("=(" + NUMBER + ")", "=" + Y + "$1" + S, w);
    w = Re::sub ("=(\"(?:[^\"\\\\]|\\\\.)*\")", "=" + U + "$1" + S, w);
    w = Re::sub (" (\\w+)=", " " + C + "$1" + S + "=", w);
    w = Re::sub (": ([a-zA-Z.0-9_:-]+): ", ": " + G + "$1:" + S + " ", w);
    w = Re::sub ("^(\\d+[.]\\d+):", Y + "$1:" + S, w, Re::M);
    sout += w;
  } else
    sout += message;
  if (sout.size() && sout[sout.size()-1] != '\n')
    sout += '\n';
  if (pabort && level > FATAL)
    sout += "Aborting... (fatal-warnings)\n";
  fflush (stdout);      // preserve output ordering
  fputs (sout.c_str(), stderr);
  fflush (stderr);      // some platforms (_WIN32) don't properly flush on '\n'
  if (logprefix.size())
    sout = logprefix + ' ' + sout;
  sout = Re::sub ("\\x1b\\[[0-9;]*[mK]", "", sout, Re::M);      // strip ansi-colors
  logging_to_file (pabort ? sout + logging_timestamp (timestamp_now()) + ' ' + executable_name() + ": Aborting...\n" : sout);
  if (pabort) {
    logging_print_backtrace (__func__);
    abort_debug_friendly (message.c_str(), filename, line, function_name);
  }
}

void
logging (Logging level, const std::string &message, const char *file, uint32_t line, const char *func) noexcept
{
  logging (level, "", message, file, line, func);
}

/// Print a debug/diag message, called from ::Ase::debug().
void
logging_debug (const char *cond, const std::string &message) noexcept
{
  return_unless (!cond || debug_key_enabled (cond));
  logging (cond ? DEBUGALL : DIAG, cond ? cond : "", message, nullptr, 0, nullptr);
}

void
logging_abort (Logging level, const std::string &message, const char *file, uint32_t line, const char *func) noexcept
{
  logging (level, "", message, file, line, func);
  for (;;)
    abort();
}

} // Ase


#undef NDEBUG // enable __GLIBC__ __assert_fail()
#include <cassert>
namespace Ase {
static void
abort_debug_friendly (const char *msg, const char *file, int line, const char *func) noexcept
{
#if defined (_ASSERT_H_DECLS) && defined(__GLIBC__)
  // abort via GLIBC if possible, which allows 'print __abort_msg->msg' from apport/gdb
  __assert_fail (msg && msg[0] ? msg : "assertion unreachable\n", file, line, func);
#endif
  for (;;)
    abort();
}
} // Ase
