// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "cxxaux.hh"
#include "logging.hh"
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
const char*
cxx_demangle (const std::type_info &typeinfo) noexcept
{
  static auto &m2d = *new std::unordered_map<const char*, const char*>();
  static std::mutex mtx;
  const char *mangled_identifier = typeinfo.name();
  { std::lock_guard<std::mutex> locker (mtx);
    auto it = m2d.find (mangled_identifier);
    if (it != m2d.end())
      return it->second;
  }
  int status = 0;
  char *malloced_result = abi::__cxa_demangle (mangled_identifier, NULL, NULL, &status);
  if (malloced_result && !status) {
    std::lock_guard<std::mutex> locker (mtx);
    auto it = m2d.find (mangled_identifier);
    if (it != m2d.end()) {
      free (malloced_result);
      return it->second;
    }
    m2d[mangled_identifier] = malloced_result;
    return malloced_result;
  }
  return mangled_identifier;
}

std::string
cxx_demangle (const char *mangled_identifier) noexcept
{
  int status = 0;
  char *malloced_result = abi::__cxa_demangle (mangled_identifier, NULL, NULL, &status);
  std::string result;
  if (malloced_result && !status) {
    result = malloced_result;
    free (malloced_result);
  }
  return result.empty() ? mangled_identifier : result;
}

/// Find GDB and construct command line
std::string
backtrace_command()
{
  bool allow_ptrace = true;
#if 0 && defined (__linux__)
  // disabling this check, so the debugger can showman appropriate error message
  const char *const ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope";
  int fd = open (ptrace_scope, 0);
  char b[8] = { 0 };
  if (read (fd, b, 8) > 0)
    allow_ptrace = b[0] == '0';
  close (fd);
#endif
  if (!allow_ptrace)
    return "";
  char cmd[3192];
  const char *const usr_bin_lldb = "/usr/bin/lldb";
  if (access (usr_bin_lldb, X_OK) == 0) {
    snprintf (cmd, sizeof (cmd),
              "%s -Q -x --batch -p %u "
              "-o 'bt all'",
              usr_bin_lldb, gettid());
    return cmd;
  }
  const char *const usr_bin_gdb = "/usr/bin/gdb";
  if (access (usr_bin_gdb, X_OK) == 0) {
    snprintf (cmd, sizeof (cmd),
              "%s -q -n --nx -p %u --batch "
              "-iex 'set auto-load python-scripts off' "
              "-iex 'set script-extension off' "
              "-ex 'set print address off' "
              // "-ex 'set print frame-arguments none' "
              "-ex 'thread apply all backtrace 99' " // max frames
              ">&2 2>/dev/null",
              usr_bin_gdb, gettid());
    return cmd;
  }
  return "";
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

void
perror_die (const std::string &msg) noexcept
{
  std::string message = msg;
  if (errno)
    message += std::string (": ") + strerror (errno);
  assertion_failed (message.c_str(), nullptr, 0, nullptr);
  for (;;)
    abort();
}

/// Print a debug message via assertion_failed() and abort the program.
void
assertion_fatal (const char *msg, const char *file, int line, const char *func) noexcept
{
  logging_abort ('A', msg ? msg : "", file, line, func);
}

/// Print instructive message, handle "breakpoint", "backtrace" and "fatal-warnings" in $ASE_DEBUG.
void
assertion_failed (const char *msg, const char *file, int line, const char *func) noexcept
{
  return logging ('A', msg ? msg : "", file, line, func);
}

} // Ase
