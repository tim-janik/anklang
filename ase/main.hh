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
  std::vector<String> args;
  uint16 websocket_port = 0;
  bool   fatal_warnings = false;
  bool   allow_randomization = true;
  bool   print_js_api = false;
  bool   jsipc = false;
  bool   jsbin = false;
  enum ModeT { SYNTHENGINE, CHECK_INTEGRITY_TESTS };
  ModeT  mode = SYNTHENGINE;
  AudioEngine *engine = nullptr;
};
extern const MainConfig &main_config;

// == Feature Toggles ==
String feature_toggle_find (const String &config, const String &feature, const String &fallback = "0");
bool   feature_toggle_bool (const char *config, const char *feature);
bool   feature_check       (const char *feature);

// == Jobs & main loop ==
extern MainLoopP main_loop;

/// Execute a lambda job in the Ase main loop and wait for its result.
extern JobQueue main_jobs;

} // Ase

#endif // __ASE_MAIN_HH__
