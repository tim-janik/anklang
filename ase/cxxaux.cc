// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "cxxaux.hh"
#include "utils.hh"             // ase_fatal_warnings
#include "backtrace.hh"
#include <cxxabi.h>             // abi::__cxa_demangle
#include <signal.h>

namespace Ase {

/** Demangle a std::typeinfo.name() string into a proper C++ type name.
 * This function uses abi::__cxa_demangle() from <cxxabi.h> to demangle C++ type names,
 * which works for g++, libstdc++, clang++, libc++.
 */
String
string_demangle_cxx (const char *mangled_identifier)
{
  int status = 0;
  char *malloced_result = abi::__cxa_demangle (mangled_identifier, NULL, NULL, &status);
  String result = malloced_result && !status ? malloced_result : mangled_identifier;
  if (malloced_result)
    free (malloced_result);
  return result;
}

void
assertion_failed (const std::string &msg, const char *file, int line, const char *func)
{
  if (file && line > 0 && func)
    fprintf (stderr, "%s:%u:%s: ", file, line, func);
  else if (file && line > 0)
    fprintf (stderr, "%s:%u: ", file, line);
  else if (file)
    fprintf (stderr, "%s: ", file);
  if (msg.empty())
    fputs ("state unreachable\n", stderr);
  else
    {
      fputs ("assertion failed: ", stderr);
      fputs (msg.c_str(), stderr);
      if (msg.size() && msg[msg.size() - 1] != '\n')
        fputc ('\n', stderr);
    }
  fflush (stderr);
  if (debug_key_enabled ("backtrace"))
    ASE_PRINT_BACKTRACE (__FILE__, __LINE__, __func__);
  else if (debug_key_enabled ("break"))
    breakpoint();
  if (ase_fatal_warnings)
    return breakpoint();
}

VirtualBase::~VirtualBase()
{}

} // Ase
