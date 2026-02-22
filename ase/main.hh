// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MAIN_HH__
#define __ASE_MAIN_HH__

#include <ase/platform.hh>
#include <ase/randomhash.hh>
#include <ase/regex.hh>
#include <ase/websocket.hh>
#include <ase/loop.hh>
#include <ase/callback.hh>

namespace Ase {

// == MainApp ==
struct MainApp {
  AudioEngine *engine = nullptr;
  String pcm_override, midi_override;
  WebSocketServer *web_socket_server = nullptr;
  const char         *outputfile = nullptr;
  std::vector<String> args;
  uint16 websocket_port = 0;
  int    jsonapi_logflags = 1;
  bool   norc = true;
  bool   allow_randomization = true;
  bool   list_drivers = false;
  bool   play_autostart = false;
  bool   no_devices = false;
  double play_autostop = D64MAX;
  enum ModeT { SYNTHENGINE, CHECK_INTEGRITY_TESTS };
  ModeT  mode = SYNTHENGINE;
};
extern const MainApp &App;

// == Jobs & main loop ==
extern LoopP main_loop;

/// Wake up the event loop.
void             main_loop_wakeup      ();
/// Stop the event loop after a timeout.
void             main_loop_autostop_mt ();

/// Execute a job callback in the event loop.
extern JobQueue main_jobs;

/// Add a simple callback to the event loop, without using malloc (obstruction free).
struct RtJobQueue { void operator+= (const RtCall&); };
/// Queue a callback for the event loop without invoking malloc(), addition is obstruction free.
extern RtJobQueue main_rt_jobs;

} // Ase

#endif // __ASE_MAIN_HH__
