// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/cxxaux.hh>
#include <ase/formatter.hh>
#include <source_location>

namespace Ase {

// == stdio ==
template<class... A> void printout          (const char *format, const A &...args) ASE_PRINTF (1, 0);
template<class... A> void printerr          (const char *format, const A &...args) ASE_PRINTF (1, 0);

/// Current time in µseconds.
uint64_t                  timestamp_now   ();

// == Sincere Messages ==
/// Wrap a string together with its source code location
struct LogFormat {
  std::source_location location;
  const char *const cstr = nullptr;
  LogFormat (const char *s, std::source_location l = std::source_location::current()) :
    location (l),
    cstr (s)
  {}
};

template<class ...A> void fatal_error (const LogFormat &format, const A &...args) ASE_NORETURN;
template<class ...A> void warning     (const char *format, const A &...args);
template<class ...A> void info        (const char *format, const A &...args) ASE_ALWAYS_INLINE;
template<class ...A> void diag        (const char *format, const A &...args) ASE_ALWAYS_INLINE;

// == Debugging ==
void                      logging_handle_terminate ();
template<class ...A> void debug             (const char *cond, const char *format, const A &...args) ASE_ALWAYS_INLINE;
inline bool               debug_enabled     () ASE_ALWAYS_INLINE ASE_PURE;
bool                      debug_key_enabled (const char *conditional) noexcept ASE_PURE;
const char*               getenv_ase_debug  ();
extern bool ase_debugging_enabled;      ///< Global boolean to reduce debugging penalty where possible

// == Implementations ==
enum Logging : int { FATAL, ASSERTION, WARN, HINT, INFO, DIAG, TRACE, DEBUGALL, };
[[noreturn]]
void    logging_abort (Logging level, const std::string &message, const char *file, uint32_t line, const char *func) noexcept;
void    logging       (Logging level, const std::string &message, const char *file, uint32_t line, const char *func) noexcept;
void    logging_debug (const char *cond, const std::string &message) noexcept;
void    stdio_flush   (uint8 code, const String &txt) noexcept;
extern bool logging_fatal_warnings;

bool    logging_configure (const std::string &log_file_ident, Logging level = Logging (-1));

/// Check if any kind of debugging is enabled by $ASE_DEBUG.
inline bool ASE_ALWAYS_INLINE ASE_PURE
debug_enabled()
{
  return ASE_UNLIKELY (ase_debugging_enabled);
}

/// Issue a printf-like debugging message if `cond` is enabled by $ASE_DEBUG.
template<class ...Args> inline void ASE_ALWAYS_INLINE
debug (const char *cond, const char *format, const Args &...args)
{
#ifndef NDEBUG
  if (debug_enabled())
    {
      if (ASE_UNLIKELY (debug_key_enabled (cond)))
        logging_debug (cond, string_format (format, args...));
    }
#endif
}

/// Issue a printf-like diagnostics message, usually enabled.
template<class ...Args> inline void ASE_ALWAYS_INLINE
info (const char *format, const Args &...args)
{
  logging (INFO, string_format (format, args...), nullptr, -1, nullptr);
}

/// Issue a printf-like diagnostics message, usually below verbosity threshold.
template<class ...Args> inline void ASE_ALWAYS_INLINE
diag (const char *format, const Args &...args)
{
#ifndef NDEBUG
  logging_debug (nullptr, string_format (format, args...));
#endif
}

/// Issue a printf-like message and abort the program.
template<class ...Args> void
fatal_error (const LogFormat &format, const Args &...args)
{
  logging_abort (FATAL, string_format (format.cstr, args...),
                 format.location.file_name(), format.location.line(), format.location.function_name());
}

/// Warn about unexpected / internal error, usually detached from the cause
template<class ...Args> void
warning (const char *format, const Args &...args)
{
  logging (WARN, string_format (format, args...), nullptr, -1, nullptr);
}

/// Print a message on stdout (and flush stdout) ala printf(), using the POSIX/C locale.
template<class... Args> void
printout (const char *format, const Args &...args)
{
  stdio_flush ('o', string_format (format, args...));
}

/// Print a message on stderr (and flush stderr) ala printf(), using the POSIX/C locale.
template<class... Args> void
printerr (const char *format, const Args &...args)
{
  stdio_flush ('e', string_format (format, args...));
}

} // Ase
