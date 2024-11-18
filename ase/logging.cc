// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "logging.hh"
#include "platform.hh"
#include "path.hh"
#include "regex.hh"
#include <cstdarg>
#include <cstring>

namespace Ase {

LogFlags log_setup (int*) __attribute__ ((__weak__));

uint64_t
timestamp_now ()
{
  struct timeval tv = { 0, 0 };
  gettimeofday (&tv, nullptr);
  return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static uint64 programstart_timestamp = timestamp_now();
static uint32_t log_flags = 0;
static int      log_fd = -1;
static bool     log_colorize = true;

static void
logstart()
{
  if (log_flags) [[likely]] return;
  log_colorize = AnsiColors::colorize_tty();
  if (log_setup) {
    log_flags = log_setup (&log_fd);
    if (log_fd >= 0)
      log_flags |= LOG_FILE;
  } else
    log_flags |= LOG_STDERR;
  const time_t now = programstart_timestamp / 1000000;
  struct tm stm{};
  localtime_r (&now, &stm);
  char tbuf[128] = { 0, };
  strftime (tbuf, sizeof (tbuf) - 1, "%Y-%m-%d %H:%M:%S %z", &stm);
  const std::string exec = executable_path();
  const char *bexec = strrchr (exec.c_str(), '/');
  bexec = bexec ? bexec+1 : exec.c_str();
  char pidbuf[64] = { 0, };
  snprintf (pidbuf, sizeof (pidbuf) - 1, " pid=%u", getpid());
  std::string msg = std::string (bexec) + ": programstart=\"" + tbuf + "\"" + pidbuf + " executable=\"" + executable_path() + "\"";
  logmsg (msg, "", 0, "");
}

void
logmsg (const std::string &msg, const char *const filename, const uint64_t columnline, const char *const function_name)
{
  const uint32_t line = uint32_t (columnline), column = columnline >> 32;
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
  if (filename && filename[0]) {
    char linein[128] = { 0, };
    snprintf (linein, sizeof (linein) - 1, ":%u:%u: execution at: ", line, column);
    s = filename + std::string (linein) + function_name + "\n" + s;
  }
  if (log_fd == 2 || log_flags & LOG_STDERR)
    fflush (stderr);
  if (!log_colorize && log_flags & LOG_STDERR)
    write (2, s.data(), s.size());
  if (log_fd >= 0)
    write (log_fd, s.data(), s.size());
  if (log_colorize && log_flags & LOG_STDERR) {
    using namespace AnsiColors;
    const auto C = color (FG_CYAN), G = color (BOLD, FG_GREEN), B = color (FG_BLUE), Y = color (FG_YELLOW), R = color (RESET);
    const std::string HEXINT = "0[xX][0-9abcdefABCDEF]+";
    const std::string FULLFLOAT = "([1-9][0-9]*|0)([.][0-9]*)?([eE][+-]?[0-9]+)?";
    const std::string FRACTFLOAT = "[.][0-9]+([eE][+-]?[0-9]+)?";
    const std::string NUMBER = HEXINT + "|" + FULLFLOAT + "|" + FRACTFLOAT;
    s = Re::sub ("=(" + NUMBER + ")", "=" + Y + "$1" + R, s);
    s = Re::sub ("=(\"(?:[^\"\\\\]|\\\\.)*\")", "=" + B + "$1" + R, s);
    s = Re::sub (" (\\w+)=", " " + C + "$1" + R + "=", s);
    s = Re::sub (": ([a-zA-Z.0-9_:-]+): ", ": " + G + "$1:" + R + " ", s);
    s = Re::sub ("^(\\d+[.]\\d+):", Y + "$1:" + R, s, Re::M);
    write (2, s.data(), s.size());
  }
}

} // Ase
