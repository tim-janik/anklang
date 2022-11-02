// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MAIN_HH__
#define __ASE_MAIN_HH__

#include <ase/platform.hh>
#include <ase/randomhash.hh>
#include <ase/regex.hh>
#include <ase/websocket.hh>
#include <ase/loop.hh>
#include <ase/utils.hh>

namespace Ase {

// == MainConfig ==
struct MainConfig {
  AudioEngine *engine = nullptr;
  WebSocketServer *web_socket_server = nullptr;
  const char         *outputfile = nullptr;
  std::vector<String> args;
  uint16 websocket_port = 0;
  int    jsonapi_logflags = 1;
  bool   fatal_warnings = false;
  bool   allow_randomization = true;
  bool   list_drivers = false;
  bool   play_autostart = false;
  double play_autostop = D64MAX;
  enum ModeT { SYNTHENGINE, CHECK_INTEGRITY_TESTS };
  ModeT  mode = SYNTHENGINE;
};
extern const MainConfig &main_config;

// == Feature Toggles ==
String feature_toggle_find (const String &config, const String &feature, const String &fallback = "0");
bool   feature_toggle_bool (const char *config, const char *feature);
bool   feature_check       (const char *feature);

// == Jobs & main loop ==
extern MainLoopP main_loop;
void             main_loop_wakeup      ();
void             main_loop_autostop_mt ();

/// Execute a lambda job in the Ase main loop and wait for its result.
extern JobQueue main_jobs;

} // Ase

#endif // __ASE_MAIN_HH__
