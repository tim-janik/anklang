// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/loop.hh>

namespace Ase {

struct ErrorReason {
  int error = 0;
  std::string what;
};

// == atquit ==
[[noreturn]]
void    atquit_terminate        (int exitcode, int pgroup = -1) __attribute__ ((noreturn));
bool    atquit_triggered        ();
void    atquit_add              (std::function<void()> *func);
void    atquit_del              (std::function<void()> *func);
void    atquit_add_removal      (const std::string &filename);
void    atquit_del_removal      (const std::string &filename);
void    atquit_add_killl_pid    (int pid);
void    atquit_del_killl_pid    (int pid);

std::string     create_tempfile_dir             (const std::string &basename = "");
void            cleanup_orphaned_tempfiles      (const std::string &directory);
ErrorReason     spawn_process                   (const std::vector<std::string> &argv, pid_t *child_pid, int pdeathsig = -1);

} // Ase
