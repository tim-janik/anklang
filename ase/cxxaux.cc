// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "cxxaux.hh"
#include <cxxabi.h>             // abi::__cxa_demangle
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace Ase {

VirtualBase::~VirtualBase() noexcept
{}

/** Demangle a std::typeinfo.name() string into a proper C++ type name.
 * This function uses abi::__cxa_demangle() from <cxxabi.h> to demangle C++ type names,
 * which works for g++, libstdc++, clang++, libc++.
 */
String
string_demangle_cxx (const char *mangled_identifier) noexcept
{
  int status = 0;
  char *malloced_result = abi::__cxa_demangle (mangled_identifier, NULL, NULL, &status);
  String result = malloced_result && !status ? malloced_result : mangled_identifier;
  if (malloced_result)
    free (malloced_result);
  return result;
}

/// Find GDB and construct command line
static std::string
backtrace_command()
{
  bool allow_ptrace = false;
#ifdef  __linux__
  const char *const ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope";
  int fd = open (ptrace_scope, 0);
  char b[8] = { 0 };
  if (read (fd, b, 8) > 0)
    allow_ptrace = b[0] == '0';
  close (fd);
#else
  allow_ptrace = true;
#endif
  const char *const usr_bin_gdb = "/usr/bin/gdb";
  if (!allow_ptrace || access (usr_bin_gdb, X_OK) != 0)
    return "";
  char cmd[1024];
  snprintf (cmd, sizeof (cmd),
            "%s -q -n -p %u --batch "
            "-iex 'set auto-load python-scripts off' "
            "-iex 'set script-extension off' "
            "-ex 'set print address off' "
            // "-ex 'set print frame-arguments none' "
            "-ex 'thread apply all backtrace 21' " // max frames
            ">&2 2>/dev/null",
            usr_bin_gdb, gettid());
  return cmd;
}

/// Quick boolean check for a colon separated key in a haystack.
static bool
has_debug_key (const char *const debugkeys, const char *const key)
{
  if (!debugkeys) return false;
  const auto l = strlen (key);
  const auto d = strstr (debugkeys, key);
  return d && (d == debugkeys || d[-1] == ':') && (d[l] == 0 || d[l] == ':');
}

/// Global flag to force aborting on assertion warnings.
bool assertion_failed_fatal = false;

/// Print a debug message via assertion_failed() and abort the program.
void
assertion_fatal (const char *msg, const char *file, int line, const char *func) noexcept
{
  assertion_failed_fatal = true;
  assertion_failed (msg, file, line, func);
  for (;;)
    abort();
}

static void assertion_abort (const char *msg, const char *file, int line, const char *func) noexcept ASE_NORETURN;

/// Print instructive message, handle "breakpoint", "backtrace" and "fatal-warnings" in $ASE_DEBUG.
void
assertion_failed (const char *msg, const char *file, int line, const char *func) noexcept
{
  std::string m;
  if (file && line > 0 && func)
    m += std::string (file) + ":" + std::to_string (unsigned (line)) + ":" + func + ": ";
  else if (file && line > 0)
    m += std::string (file) + ":" + std::to_string (unsigned (line)) + ": ";
  else if (file || func)
    m += std::string (file ? file : func) + ": ";
  if (!msg || !msg[0])
    m += "assertion unreachable\n";
  else {
    if (line >= 0)
      m += "assertion failed: ";
    m += msg;
    if (!m.empty() && m.back() != '\n')
      m += "\n";
  }
  fflush (stdout);      // preserve output ordering
  fputs (m.c_str(), stderr);
  fflush (stderr);      // some platforms (_WIN32) don't properly flush on '\n'
  const char *const d = getenv ("ASE_DEBUG");
  if (!assertion_failed_fatal && has_debug_key (d, "fatal-warnings"))
    assertion_failed_fatal = true;
  if (assertion_failed_fatal || has_debug_key (d, "breakpoint")) {
#if (defined __i386__ || defined __x86_64__)
    __asm__ __volatile__ ("int $03");
#else
    __builtin_trap();
#endif
  } else if (has_debug_key (d, "backtrace")) {
    const std::string gdb_cmd = backtrace_command();
    if (!gdb_cmd.empty()) {
      const auto res = system (gdb_cmd.c_str());
      (void) res;
    }
  }
  if (assertion_failed_fatal)
    assertion_abort (msg, file, line, func);
}

} // Ase

#undef NDEBUG           // enable __GLIBC__ __assert_fail()
#include <cassert>
namespace Ase {
static void
assertion_abort (const char *msg, const char *file, int line, const char *func) noexcept
{
#if defined (_ASSERT_H_DECLS) && defined(__GLIBC__)
  // abort via GLIBC if possible, which allows 'print __abort_msg->msg' from apport/gdb
  __assert_fail (msg && msg[0] ? msg : "assertion unreachable\n", file, line, func);
#else
  for (;;)
    abort();
#endif
}
} // Ase
