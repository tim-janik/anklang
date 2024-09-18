// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "logging.hh"
#include "platform.hh"
#include "path.hh"
#include <cstdarg>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace Ase {

uint64_t
timestamp_now ()
{
  struct timeval tv = { 0, 0 };
  gettimeofday (&tv, nullptr);
  return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static uint64 programstart_timestamp = timestamp_now();
static bool log_started = false;

static void
logstart()
{
  if (log_started) [[likely]] return;
  log_started = timestamp_now();
  const time_t now = programstart_timestamp / 1000000;
  struct tm stm{};
  localtime_r (&now, &stm);
  char tbuf[128] = { 0, };
  strftime (tbuf, sizeof (tbuf) - 1, "%Y-%m-%d %H:%M:%S %z", &stm);
  const std::string exec = executable_path();
  const char *bexec = strrchr (exec.c_str(), '/');
  bexec = bexec ? bexec+1 : exec.c_str();
  std::string msg = std::string (bexec) + ": programstart=\"" + tbuf + "\" executable=\"" + executable_path() + "\"";
  logmsg (msg, {});
}

void
logmsg (const std::string &msg, const std::source_location &loc)
{
  logstart();
  ScopedPosixLocale posix_locale; // use POSIX locale for this scope
  if (msg.empty()) return;
  String s = msg;
  if (s[s.size()-1] != '\n')
    s += "\n";
  {
    char tstamp[64] = { 0, };
    snprintf (tstamp, sizeof (tstamp) - 1, "%.6f: ", 0.000001 * (timestamp_now() - programstart_timestamp));
    s = tstamp + s;
  }
  const char *const filename = loc.file_name();
  if (filename && filename[0]) {
    char linein[128] = { 0, };
    snprintf (linein, sizeof (linein) - 1, ":%u:%u: execution at: ", loc.line(), loc.column());
    s = filename + std::string (linein) + loc.function_name() + "\n" + s;
  }
  fflush (stderr);
  write (2, s.data(), s.size());
  // fdatasync (2);
}

void
log_setup (bool inf2stderr, bool log2file)
{
}

} // Ase
