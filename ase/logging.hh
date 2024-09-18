// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/formatter.hh>
#include <source_location>

namespace Ase {

/// Wrap a string together with its source code location
struct LString : public std::string {
  std::source_location location;
  LString (const std::string &s, std::source_location l = std::source_location::current()) :
    std::string (s),
    location (l)
  {}
  LString (const char *s, std::source_location l = std::source_location::current()) :
    std::string (s),
    location (l)
  {}
};

// Current time in µseconds.
uint64_t                        timestamp_now   ();

/// Write a log message to the log file (or possibly stderr), using the POSIX/C locale.
template<class... A> void       log             (const char *format, const A &...args) __attribute__ ((__noinline__, __format__ (__printf__, 1, 0)));

/// Write a log message to the log with source code location.
template<class... A> void       logex           (const LString &format, const A &...args) __attribute__ ((__noinline__));

enum LogFlags { LOG_FILE = 1, LOG_STDERR = 2, LOG_LOCATIONS = 4, };

/// Configurable handler to open log files
LogFlags                        log_setup       (int *logfd);

#ifdef _MATH_H
using ::log;
#endif

// == implementations ==
void logmsg (const std::string &msg, const std::source_location &loc);

template<class... A> __attribute__ ((__noinline__)) void
log (const char *format, const A &...args)
{
  logmsg (string_format (format, args...), {});
}

template<class... A> __attribute__ ((__noinline__)) void
logex (const LString &format, const A &...args)
{
  logmsg (string_format (format.c_str(), args...), format.location);
}

} // Ase
