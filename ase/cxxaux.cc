// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "cxxaux.hh"
#include "logging.hh"
#include <cxxabi.h>             // abi::__cxa_demangle
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

#ifdef ASE_WITH_CPPTRACE
#include <cpptrace/from_current.hpp>
#endif

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
  logging_abort (ASSERTION, msg ? msg : "", file, line, func);
}

/// Print instructive message, handle "breakpoint", "backtrace" and "fatal-warnings" in $ASE_DEBUG.
void
assertion_failed (const char *msg, const char *file, int line, const char *func) noexcept
{
  return logging (ASSERTION, msg ? msg : "", file, line, func);
}

void
ase_rethrow (std::exception_ptr exception)
{
#ifdef ASE_WITH_CPPTRACE
  cpptrace::rethrow (exception);
#endif
  std::rethrow_exception (exception);
}

} // Ase
