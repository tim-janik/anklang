// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/formatter.hh>
#include <source_location>

namespace Ase {

/// Wrap a string together with its source code location
struct LogFormat {
  std::source_location location;
  const char *const cstr = nullptr;
  LogFormat (const char *s, std::source_location l = std::source_location::current()) :
    location (l),
    cstr (s)
  {}
};

/// Current time in µseconds.
uint64_t                  timestamp_now   ();

/// Write a log message to the log file (or possibly stderr), using the POSIX/C locale.
template<class... A> void log (const LogFormat &format, const A &...args);

/// Flags to configure logging behaviour
enum LogFlags { LOG_FILE = 1, LOG_STDERR = 2, LOG_LOCATIONS = 4, };

/// Configurable handler to open log files
LogFlags                  log_setup       (int *logfd);

// Keep natural logarithmic function available
#ifdef _MATH_H
using ::log;
#endif

// == implementations ==
void logmsg (const std::string &msg, const char *file, uint64_t columnline, const char *func);

template<class... A> __attribute__ ((__noinline__)) void
logfmt (const char *file, uint64_t columnline, const char *func, const char *format, const A &...args)
{
  logmsg (string_format (format, args...), file, columnline, func);
}

template<class... A> __attribute__ ((__always_inline__)) inline void
log (const LogFormat &format, const A &...args)
{
  logfmt (format.location.file_name(),
          uint64_t (format.location.column()) << 32 | uint32_t (format.location.line()),
          format.location.function_name(),
          format.cstr, args...);
}

} // Ase
