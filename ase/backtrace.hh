// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_BACKTRACE_HH__
#define __ASE_BACKTRACE_HH__

#include <ase/platform.hh>

namespace Ase {

/// Helper to generate backtraces externally via system(3).
struct BacktraceCommand {
  explicit    BacktraceCommand ();      ///< Setup, currently facilitates just gdb.
  bool        can_backtrace ();         ///< Assess whether backtrace printing is possible.
  const char* command ();               ///< Command for system(3) to print backtrace.
  const char* message ();               ///< Notice about errors or inability to backtrace.
  const char* heading (const char *file, int line, const char *func,
                       const char *prefix = "", const char *postfix = "");
private:
  static constexpr uint32 tlen_ = 3075; ///< Length of `cmdbuf`.
  char txtbuf_[tlen_ + 1] = { 0, };     ///< Command for system() iff `can_backtrace==true`.
};

/// Print a bactrace to stderr if possible.
#define ASE_PRINT_BACKTRACE(file, line, func)                      do { \
  using namespace AnsiColors;                                           \
  const std::string col = color (FG_YELLOW /*, BOLD*/);                 \
  const std::string red = color (FG_RED, BOLD);                         \
  const std::string reset = color (RESET);                              \
  BacktraceCommand btrace;                                              \
  bool btrace_ok = false;                                               \
  if (btrace.can_backtrace()) {                                         \
    const char *heading = btrace.heading (file, line, func,             \
                                          col.c_str(), reset.c_str());  \
    printerr (heading);                                                 \
    const char *btrace_cmd = btrace.command();                          \
    btrace_ok = btrace_cmd[0] && system (btrace_cmd) == 0;              \
  }                                                                     \
  const char *btrace_msg = btrace.message();                            \
  if (!btrace_ok && btrace_msg[0])                                      \
    printerr ("%s%s%s", red, btrace_msg, reset); /* print errors */     \
} while (0)

} // Ase

#endif // __ASE_BACKTRACE_HH__
