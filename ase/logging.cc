// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "logging.hh"
#include "platform.hh"
#include "path.hh"
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace Ase {

static uint64 logstart_timestamp = 0;
static constexpr double USEC2SEC = 1.0 / 1000000;
static bool info2stderr = true;
static int log_fd = -1;

static std::array<int,2>
log_fds (bool iserror)
{
  if (!logstart_timestamp)
    logstart_timestamp = timestamp_startup();
  return { info2stderr || iserror ? 2 : -1, log_fd };
}

static char*
sntime (char *buffer, size_t bsize)
{
  buffer[0] = 0;
  snprintf (buffer, bsize, "[%+11.6f] ", USEC2SEC * (timestamp_realtime() - logstart_timestamp));
  return buffer + strlen (buffer);
}

static String
ilog_dir (bool mkdirs = false)
{
  const String ilogdir = Path::join (Path::xdg_dir ("CACHE"), "anklang");
  if (mkdirs)
    Path::mkdirs (ilogdir);
  return ilogdir;
}

void
log_setup (bool inf2stderr, bool log2file)
{
  if (log_fd >= 0) return;
  info2stderr = inf2stderr;
  if (log2file) {
    const String dir = ilog_dir (true);
    const String fname = string_format ("%s/%s-%08x.log", dir, program_alias(), gethostid());
    errno = EBUSY;
    const int OFLAGS = O_CREAT | O_EXCL | O_WRONLY | O_NOCTTY | O_NOFOLLOW | O_CLOEXEC; // O_TRUNC
    const int OMODE = 0640;
    int fd = open (fname.c_str(), OFLAGS, OMODE);
    if (fd < 0 && errno == EEXIST) {
      const String oldname = fname + ".old";
      if (rename (fname.c_str(), oldname.c_str()) < 0)
        perror (string_format ("%s: failed to rename \"%s\"", program_alias(), oldname.c_str()).c_str());
      fd = open (fname.c_str(), OFLAGS, OMODE);
    }
    if (fd < 0)
      perror (string_format ("%s: failed to open log file \"%s\"", program_alias(), fname.c_str()).c_str());
    else {
      log_fd = fd;
      const auto lfds = log_fds (false); // does on demand setup
      if (lfds[1] >= 0) {
        constexpr const int MAXBUFFER = 1024;
        char buffer[MAXBUFFER] = { 0, }, *b = sntime (buffer, MAXBUFFER);
        snprintf (b, MAXBUFFER - (b - buffer),
                  "%s %s: pid=%u startup=%.6f\n",
                  program_alias().c_str(), ase_build_id(),
                  getpid(), USEC2SEC * logstart_timestamp);
        write (lfds[1], buffer, strlen (buffer));
      }
    }
  }
}

void
logmsg (const char c, const String &dept, const String &msg)
{
  ScopedPosixLocale posix_locale; // use POSIX locale for this scope
  if (msg.empty()) return;
  String s = msg;
  if (s[s.size()-1] != '\n')
    s += "\n";
  if (c == 'E')
    s = dept + (dept.empty() ? "" : " ") + "Error: " + s;
  else if (!dept.empty())
    s = dept + ": " + s;
  for (auto fd : log_fds (c == 'E')) {
    if (fd == 1) fflush (stdout);
    if (fd == 2) fflush (stderr);
    if (c == 'T' && fd <= 2)
      continue;
    write (fd, s.data(), s.size());
    // fdatasync (fd);
  }
}

} // Ase
