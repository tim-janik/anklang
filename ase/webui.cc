// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "webui.hh"
#include "logging.hh"
#include "platform.hh"
#include "strings.hh"
#include <cerrno>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <filesystem>
#include <dirent.h>
#include "path.hh"

#define WDEBUG(...)             Ase::debug ("webui", __VA_ARGS__)

namespace Ase {

String // check errno
webui_create_auth_redirect (const std::string &executable, unsigned port, const std::string &token, const std::string &snapmode)
{
  String basedir = Path::cache_home() + "/" + executable;
  /* The canonical path for the redirect is $XDG_CACHE_HOME/<executable>/<executable>-<port>.html
   * But snap based browsers cannot read from ~/.* $XDG_RUNTIME_DIR ~/snap/… /var/tmp or /tmp/.
   * This means we must find a path under ~/[^.]* or ~/<subdir>/.something that comes close.
   * In any case, we only do this on systems with snapd.
   */
  bool snap_workaround = true;  // __linux__
  if (!Path::check ("/tmp/snap-private-tmp/", "d") ||
      (snapmode == "htmlgui" && Path::check (anklang_runpath (RPath::ELECTRONDIR, "htmlgui"), "x")))
    snap_workaround = false;    // not starting a snap browser
  if (snap_workaround) {
    basedir = Path::xdg_dir ("DOWNLOAD") + "/." + executable;
    const auto readme =
      "This directory only exists to enable temporary file exchange with\n"
      "snap packages which are unable to read from ~/.cache/.\n"
      "Feel free to remove this directory in its entirety.\n";
    if (!Path::check (basedir, "dw") &&
        !Path::stringwrite (basedir + "/README", readme, true)) {
      errno = errno ? errno : EIO;
      return basedir;
    }
  }
  cleanup_orphaned_tempfiles (basedir);
  const String link = string_format ("http://localhost:%u/~auth", port);
  const String query = token.empty() ? "" : "?token=" + token;
  const String ua = string_format ("%s-%u", string_capitalize (executable), port);
  const String html_text =
    string_format ("<!DOCTYPE html>\n"
                   "<html><!--@@TEMPFILE_PID=%d@@-->\n"
                   "<head><title>%s Authentication Redirect</title>\n" // 307 Temporary Redirect
                   "<meta http-equiv=\"refresh\" content=\"0; url=%s\">\n"
                   "</head>\n<body>\n"
                   "<h1>%s Authentication Redirect</h1>\n\n"
                   "<p>Redirecting to %s: <a href=\"%s\">%s</a></p>\n"
                   "<hr><address>%s</address>\n"
                   "<hr></body></html>\n",
                   getpid(), ua, link + query, ua, ua, link + query, link, ua);
  const String html_file = string_format ("%s/%s-%u.html", basedir, executable, port);
  if (!Path::stringwrite (html_file, html_text, true, 0600))
    errno = errno ? errno : EIO;
  else {
    atquit_add_removal (html_file);
    errno = 0;
  }
  return html_file;
}

ErrorReason
webui_start_browser (const std::string &mode, LoopP loop, const std::string &url, const std::function<void()> &onclose, WebuiFlags flags)
{
  std::vector<std::string> argv;
  std::string browser_name;

  if (mode == "chromium" || mode == "google-chrome")
    {
      browser_name = mode;
      argv.push_back (browser_name);
      std::string temp_dir = create_tempfile_dir();
      if (temp_dir.empty())
        return { errno, "mkdtemp" };
      const std::string user_data_dir_arg = "--user-data-dir=" + temp_dir;
      std::string app = "--app=";
      app += url;
      for (const auto arg : {
          "--incognito", "--no-first-run", "--no-experiments",
          "--no-default-browser-check", "--disable-extensions", "--disable-sync",
          // "--auto-open-devtools-for-tabs",
          "--bwsi", "--new-window" })
        argv.push_back (arg);
      if (!! (flags & WebuiFlags::HEADLESS))
        argv.push_back ("--headless");
      argv.push_back (user_data_dir_arg);
      argv.push_back (app);
    }
  else if (mode == "htmlgui")
    {
      browser_name = mode;
      argv.push_back (anklang_runpath (RPath::ELECTRONDIR, "htmlgui"));
      argv.push_back ("--no-sandbox");
      if (!! (flags & WebuiFlags::HEADLESS))
        argv.push_back ("--headless");
      argv.push_back (url);
    }
  else if (mode == "none" or mode == "" or mode == "wait")
    return { 0 }; // none
  else
    return { EINVAL, string_format ("unknown webui: %s", mode) };

  pid_t child_pid = 0;
  ErrorReason ereason = spawn_process (argv, &child_pid, SIGTERM);
  if (ereason.error)
    return ereason;
  atquit_add_killl_pid (child_pid);
  loop->exec_sigchld (child_pid,
                      [onclose] (pid_t pid, int status)
                      {
                        std::string state;
                        if (WIFEXITED (status))
                          state = string_format ("status=%d", WEXITSTATUS (status));
                        else if (WIFSIGNALED (status))
                          state = string_format ("signal=%d", WTERMSIG (status));
                        if (state.size()) {
                          info ("WebUI: child process pid=%d exited: %s", pid, state);
                          atquit_del_killl_pid (pid);
                        }
                        if (onclose)
                          onclose();
                      });
  info ("WebUI: started %s pid=%d: %s", browser_name, child_pid, url);
  return { 0, "" }; // Success
}

} // Ase
