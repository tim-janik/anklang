// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "atquit.hh"
#include <atomic>
#include <cstring>
#include <cerrno>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <filesystem>
#include <dirent.h>
#include "path.hh"
#include "strings.hh"

#define QDEBUG(...)             Ase::debug ("AtQuit", __VA_ARGS__)

namespace Ase {

/// Delete all files that contain @@TEMPFILE_PID=%d@@ without a running pid_t %d
void
cleanup_orphaned_tempfiles (const std::string &directory)
{
  DIR* dir = opendir (directory.c_str());
  if (!dir)
    return;
  struct dirent *entry;
  while ((entry = readdir (dir)) != nullptr) {
    char fullpath[PATH_MAX + 1] = { 0, };
    snprintf (fullpath, PATH_MAX, "%s/%s", directory.c_str(), entry->d_name);
    struct stat sb;
    if (stat (fullpath, &sb) == -1 || !S_ISREG (sb.st_mode))
      continue; // also skips . and ..
    FILE *file = fopen (fullpath, "r");
    if (file) {
      const int BUFFER_SIZE = 8192;
      char buffer[BUFFER_SIZE + 1] = { 0, };
      fread (buffer, BUFFER_SIZE, 1, file);
      fclose (file);
      const char *tag = "@@TEMPFILE_PID=";
      char *tagp = strstr (buffer, tag);
      if (tagp) {
        const pid_t pid = strtol (tagp + strlen (tag), nullptr, 10);
        if (pid > 1) { // check if pid_t exists and accessible for this user
          char procpath[PATH_MAX + 1] = { 0, };
          snprintf (procpath, PATH_MAX, "/proc/%u/environ", pid);
          if (access (procpath, F_OK) != 0) {
            if (unlink (fullpath) == 0)
              errno = 0;
            diag ("AtQuit: %s: remove \"%s\": %s", __func__, fullpath, strerror (errno));
          }
        }
      }
    }
  }
  closedir (dir);
}

/// Cleanup list of temporary files/dirs to be removed at exit
struct PendingRemovals final {
  std::function<void()> *atquit_handler = nullptr;
  PendingRemovals()
  {
    atquit_handler = new std::function<void()> ([&] {
      this->unlink_all();
    });
    atquit_add (atquit_handler);
  }
  ~PendingRemovals()
  {
    atquit_del (atquit_handler);
    (*atquit_handler) ();
    delete atquit_handler;
  }
  void
  add (const std::string &filename)
  {
    std::lock_guard<std::mutex> locker (mutex);
    tentries.push_back (filename);
  }
  void
  del (const std::string &filename)
  {
    std::lock_guard<std::mutex> locker (mutex);
    Aux::erase_first (tentries, [&] (const std::string &e) { return e == filename; });
  }
  void
  unlink_all()
  {
    std::lock_guard<std::mutex> locker (mutex);
    while (tentries.size()) {
      const std::string tentry = tentries.back();
      tentries.pop_back();
      std::error_code ec;
      std::filesystem::remove_all (tentry, ec);
      errno = ec.value();
      diag ("AtQuit: %s: remove \"%s\": %s", __func__, tentry, strerror (errno));
    }
  }
private:
  std::vector<std::string> tentries;
  std::mutex mutex;
};
static PendingRemovals g_pending_removals;

/// Remove filename (or directory) when the program terminates.
void
atquit_add_removal (const std::string &filename)
{
  g_pending_removals.add (filename);
}

/// Undo a previous atquit_add_removal() call.
void
atquit_del_removal (const std::string &filename)
{
  g_pending_removals.del (filename);
}

/// Cleanup list of child processes still running at exit
struct KillPids final {
  std::function<void()> *atquit_handler = nullptr;
  KillPids()
  {
    atquit_handler = new std::function<void()> ([&] {
      this->kill_all (SIGTERM);
    });
    atquit_add (atquit_handler);
  }
  ~KillPids()
  {
    atquit_del (atquit_handler);
    (*atquit_handler) ();
    delete atquit_handler;
  }
  void
  add (pid_t pid)
  {
    std::lock_guard<std::mutex> locker (mutex);
    pids.push_back (pid);
  }
  void
  del (pid_t pid)
  {
    std::lock_guard<std::mutex> locker (mutex);
    Aux::erase_first (pids, [&] (pid_t p) { return p == pid; });
  }
  void
  kill_all (int sig)
  {
    std::lock_guard<std::mutex> locker (mutex);
    while (pids.size()) {
      const pid_t pid = pids.back();
      pids.pop_back();
      diag ("AtQuit: %s: pid=%d signal=%d", __func__, pid, sig);
      kill (pid, sig);
    }
  }
private:
  std::vector<pid_t> pids;
  std::mutex mutex;
};
static KillPids g_kill_pids;

/// Kill `pid` when the program terminates.
void
atquit_add_killl_pid (int pid)
{
  g_kill_pids.add (pid);
}

/// Undo a previous atquit_add_killl_pid() call.
void
atquit_del_killl_pid (int pid)
{
  g_kill_pids.del (pid);
}

/// Span a child process after cleaning up the environment
ErrorReason
spawn_process (const std::vector<std::string> &argv, pid_t *child_pid, int pdeathsig)
{
  std::vector<const char*> argvptr;
  for (const auto &arg : argv)
    argvptr.push_back (arg.c_str());
  argvptr.push_back (nullptr);
  const char **child_argv = &argvptr[0];
  // parent process
  pid_t pid = fork();
  if (pid < 0)
    return { errno, "fork" };
  if (pid) {
    *child_pid = pid;
    return { 0, "" }; // Success
  }
  // child process
  pid = getpid();
  int max_fd = sysconf (_SC_OPEN_MAX);
  if (max_fd < 0)
    max_fd = 1024; // fallback
  for (int i = 3; i < max_fd; i++)
    close (i);
  ErrorReason ereason;
  const char *const home = getenv ("HOME");
  if (home && chdir (home) < 0) {
    ereason = { errno, "chdir" };
    goto exec_error;
  }
  if (unsetenv ("GTK_MODULES") < 0) {
    ereason = { errno, "unsetenv" };
    goto exec_error;
  }
  sigset_t empty_mask;
  sigemptyset (&empty_mask);
  if (sigprocmask (SIG_SETMASK, &empty_mask, nullptr) < 0) {
    ereason = { errno, "sigprocmask" };
    goto exec_error;
  }
  // Kill the child when the parent dies (though Chromium resets this)
  if (pdeathsig > 0 && prctl (PR_SET_PDEATHSIG, SIGKILL) < 0) {
    ereason = { errno, "prctl(PR_SET_PDEATHSIG)" };
    goto exec_error;
  }
  execvp (child_argv[0], const_cast<char *const *> (child_argv));
  // exec failed
  ereason = { errno, "execvp" };
 exec_error:
  fprintf (stderr, "%s[pid=%d]: fork to exec %s: %s: %s\n", program_invocation_short_name, pid,
           child_argv[0], ereason.what.c_str(), strerror (ereason.error));
  _exit (127); // avoid atexit handlers that might destroy parent resources (e.g. xlib DISPLAY fd)
}

/// Create temporary directory under /tmp, scheduled for removal atquit.
std::string // or returns errno
create_tempfile_dir (const std::string &basename)
{
  std::string middlename = basename.size() ? basename : string_format ("anklang-%u", getuid());
  middlename += "XXXXXX";
  std::string tempname = std::filesystem::temp_directory_path() / middlename;
  std::string dirname = mkdtemp (tempname.data());
  if (dirname.size())
    atquit_add_removal (dirname);
  return dirname;
}

// == atquit ==
static std::mutex atquit_mutex;
struct AtquitHandlers {
  std::vector<std::function<void()>*> atquit_funcs;
  ~AtquitHandlers()
  {
    // run atquit handlers also atexit
    call_hooks();
  }
  void call_hooks();
};
static AtquitHandlers atquit_handlers;

void
atquit_add (std::function<void()> *func)
{
  std::lock_guard<std::mutex> locker (atquit_mutex);
  if (func)
    atquit_handlers.atquit_funcs.push_back (func);
}

void
atquit_del (std::function<void()> *func)
{
  std::lock_guard<std::mutex> locker (atquit_mutex);
  Aux::erase_first (atquit_handlers.atquit_funcs, [func] (std::function<void()> *ele) { return func == ele; });
}

static std::atomic<uint8_t> atquit_triggered_ = false;

void
AtquitHandlers::call_hooks()
{
  std::lock_guard<std::mutex> locker (atquit_mutex);
  while (atquit_funcs.size())
    {
      std::function<void()> *func = atquit_funcs.back();
      atquit_funcs.pop_back();
      if (!func)
        continue;
      atquit_mutex.unlock();
      (*func) ();
      atquit_mutex.lock();
      // intentionally leak func, we run only the bare minimum cleanups
    }
}

void
atquit_terminate (int exitcode, int pgroup)
{
  atquit_triggered_ = true;
  atquit_handlers.call_hooks();
  if (pgroup > 1) {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction (SIGTERM, &sa, nullptr);
    diag ("AtQuit: killing process group: pid=%d signal=%d", pgroup, SIGTERM);
    kill (-pgroup, SIGTERM);
  }
  _Exit (exitcode); // ends all threads with exit_group()
}

bool
atquit_triggered ()
{
  // atquit_tester(); // FIXME
  return atquit_triggered_;
}

} // Ase
