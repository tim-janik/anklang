// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "atquit.hh"
#include "utils.hh"
#include <algorithm>
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

static void atquit_run_terminate_handlers ();

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
    atquit_run_terminate_handlers();
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
  ~KillPids()
  {
    atquit_run_terminate_handlers();
  }
  void
  add (pid_t pid)
  {
    std::lock_guard<std::mutex> locker (mutex_);
    pids_.push_back (pid);
  }
  void
  del (pid_t pid)
  {
    std::lock_guard<std::mutex> locker (mutex_);
    Aux::erase_first (pids_, [&] (pid_t p) { return p == pid; });
  }
  /// Find previously orphaned child processes, optionally send signal
  void
  collect_children (int sig)
  {
    const auto current = read_children();
    std::lock_guard<std::mutex> locker (mutex_);
    for (pid_t pid : current) {
      if (std::find (pids_.begin(), pids_.end(), pid) == pids_.end()) {
        pids_.push_back (pid);
        if (sig > 0) {
          diag ("AtQuit: found orphan pid=%d, sending signal=%d", pid, sig);
          kill (pid, sig);
        }
      }
    }
  }
  /// Reap zombies and remove dead PIDs from the list.
  void
  reap()
  {
    int status = 0;
    pid_t result;
    while ((result = waitpid (-1, &status, WNOHANG)) > 0) {
      std::lock_guard<std::mutex> locker (mutex_);
      Aux::erase_first (pids_, [&] (pid_t p) { return p == result; });
    }
  }
  /// Send signal `sig` to all tracked PIDs. Does NOT clear the list,
  /// so the same PIDs can be targeted again with an escalated signal.
  void
  kill_all (int sig)
  {
    std::lock_guard<std::mutex> locker (mutex_);
    for (pid_t pid : pids_) {
      diag ("AtQuit: %s: pid=%d signal=%d", __func__, pid, sig);
      kill (pid, sig);
    }
  }
  /// Clear the PID list after both kill phases are complete.
  void
  clear()
  {
    std::lock_guard<std::mutex> locker (mutex_);
    pids_.clear();
  }
  /// Return true if no PIDs are tracked.
  bool
  empty()
  {
    std::lock_guard<std::mutex> locker (mutex_);
    return pids_.empty();
  }
  /// Read orphaned grandchildren reparented to this process as subreaper.
  static std::vector<pid_t>
  read_children()
  {
    std::vector<pid_t> children;
    char path[64];
    snprintf (path, sizeof (path), "/proc/self/task/%d/children", getpid());
    FILE *f = fopen (path, "r");
    if (!f) return children;
    pid_t pid;
    while (fscanf (f, "%d", &pid) == 1 && pid > 0)
      children.push_back (pid);
    fclose (f);
    return children;
  }
private:
  std::vector<pid_t> pids_;
  std::mutex mutex_;
};
static KillPids g_kill_pids;

/// Kill `pid` when the program terminates.
void
atquit_add_kill_pid (int pid)
{
  g_kill_pids.add (pid);
}

/// Undo a previous atquit_add_kill_pid() call.
void
atquit_del_kill_pid (int pid)
{
  g_kill_pids.del (pid);
}

/// Spawn a child process after cleaning up the environment
ErrorReason
spawn_process (const std::vector<std::string> &argv, pid_t *child_pid, int pdeathsig, int stdio_fd, const std::vector<int> &keep_fds)
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
    {
      bool keep = (i == stdio_fd);
      for (int kfd : keep_fds)
        if (i == kfd)
          {
            keep = true;
            break;
          }
      if (!keep)
        close (i);
    }
  ErrorReason ereason;
  const char *const home = getenv ("HOME");
  if (false && home && chdir (home) < 0) {
    ereason = { errno, "chdir" };
    goto exec_error;
  }
  // Redirect stdout/stderr to stdio_fd if provided
  if (stdio_fd >= 0) {
    if (dup2 (stdio_fd, STDOUT_FILENO) < 0) {
      ereason = { errno, "dup2 stdout" };
      goto exec_error;
    }
    if (dup2 (stdio_fd, STDERR_FILENO) < 0) {
      ereason = { errno, "dup2 stderr" };
      goto exec_error;
    }
    close (stdio_fd);
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
    atquit_run_terminate_handlers();
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
atquit_make_subreaper ()
{
  // Reparent orphaned grandchildren (e.g. htmlgui renderers) to this
  // process, so we can reap or kill them at exit.
  if (prctl (PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) < 0)
    warning ("Ase: prctl(PR_SET_CHILD_SUBREAPER) failed: %s\n", strerror (errno));
}

static void
atquit_run_terminate_handlers ()
{
  atquit_triggered_ = true;

  // Terminate immediate child processes
  g_kill_pids.reap();
  g_kill_pids.kill_all (SIGTERM);

  // Cleanups, like deleting temporary files
  atquit_handlers.call_hooks();

  // Wait in steps, reap zombies, kill new orphan processes
  for (int step = 0; step < 10; step++) {
    g_kill_pids.reap();
    // On Linux, we also collect grand children
    g_kill_pids.collect_children (SIGTERM);
    if (g_kill_pids.empty())
      break;
    usleep (50000);
  }

  // SIGKILL any surviving processes
  g_kill_pids.reap();
  g_kill_pids.kill_all (SIGKILL);
  g_kill_pids.collect_children (SIGKILL);
  if (!g_kill_pids.empty()) {
    usleep (50000);
    g_kill_pids.reap();
    /// Last resort: kill any remaining orphans
    g_kill_pids.clear();
    g_kill_pids.collect_children (SIGKILL);
    g_kill_pids.clear();
  }
}

void
atquit_terminate (int exitcode)
{
  atquit_run_terminate_handlers();
  _Exit (exitcode);
}

bool
atquit_triggered ()
{
  return atquit_triggered_;
}

} // Ase
