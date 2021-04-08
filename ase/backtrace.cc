// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "backtrace.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

namespace Ase {

// == GDB Backtrace ==
BacktraceCommand::BacktraceCommand()
{}

static const char *const usr_bin_gdb = "/usr/bin/gdb";
static const char *const ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope";

/// Check /proc/sys/kernel/yama/ptrace_scope for working ptrace().
static bool
backtrace_may_ptrace()
{
  bool allow_ptrace = false;
#ifdef  __linux__
  int fd = open (ptrace_scope, 0);
  char b[8] = { 0 };
  if (read (fd, b, 8) > 0)
    allow_ptrace = b[0] == '0';
  close (fd);
#else
  allow_ptrace = true;
#endif
  return allow_ptrace;
}

/// Check for executable /usr/bin/gdb
static bool
backtrace_have_gdb()
{
  return access (usr_bin_gdb, X_OK) == 0;
}

bool
BacktraceCommand::can_backtrace ()
{
  return backtrace_may_ptrace() && backtrace_have_gdb();
}

const char*
BacktraceCommand::command ()
{
  txtbuf_[0] = 0;
  if (can_backtrace())
    snprintf (txtbuf_, tlen_,
              "%s -q -n -p %u --batch "
              "-iex 'set auto-load python-scripts off' "
              "-iex 'set script-extension off' "
              "-ex 'set print address off' "
              // "-ex 'set print frame-arguments none' "
              "-ex 'thread apply all backtrace 21' >&2 2>/dev/null",
              usr_bin_gdb, this_thread_gettid());
  return txtbuf_;
}

const char*
BacktraceCommand::message ()
{
  txtbuf_[0] = 0;
  if (!backtrace_have_gdb())
    {
      strncat (txtbuf_, "Backtrace requires a debugger, e.g.: ", tlen_);
      strncat (txtbuf_, usr_bin_gdb, tlen_);
      strncat (txtbuf_, "\n", tlen_);
    }
  else if (!backtrace_may_ptrace())
    {
      strncat (txtbuf_, "Backtrace needs ptrace permissions, ", tlen_);
      strncat (txtbuf_, "try: echo 0 > /proc/sys/kernel/yama/ptrace_scope\n", tlen_);
    }
  return txtbuf_;
}

///< Heading to print before backtrace.
const char*
BacktraceCommand::heading (const char *const file, const int line, const char *const func,
                           const char *prefix, const char *postfix)
{
  txtbuf_[0] = 0;
  strncat (txtbuf_, prefix, tlen_);
  int l = strlen (txtbuf_);
  snprintf (txtbuf_ + l, tlen_ - l, "Backtrace[%u]", getpid());
  if (!file)
    {
      strncat (txtbuf_, ":", tlen_);
      strncat (txtbuf_, postfix, tlen_);
      strncat (txtbuf_, "\n", tlen_);
      return txtbuf_;
    }
  strncat (txtbuf_, " from ", tlen_);
  strncat (txtbuf_, file, tlen_);
  strncat (txtbuf_, ":", tlen_);
  if (line > 0)
    {
      l = strlen (txtbuf_);
      snprintf (txtbuf_ + l, tlen_ - l, "%d:", line);
    }
  if (func)
    {
      strncat (txtbuf_, func, tlen_);
      strncat (txtbuf_, "():", tlen_);
    }
  strncat (txtbuf_, postfix, tlen_);
  strncat (txtbuf_, "\n", tlen_);
  return txtbuf_;
}

} // Ase
