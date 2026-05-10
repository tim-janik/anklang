// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "main.hh"
#include "api.hh"
#include "path.hh"
#include "utils.hh"
#include "jsonapi.hh"
#include "driver.hh"
#include "project.hh"
#include "loft.hh"
#include "compress.hh"
#include "webui.hh"
#include "server.hh"
#include "internal.hh"
#include "testing.hh"

#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef ASE_WITH_CPPTRACE
#include <cpptrace/from_current.hpp>
#endif

#include "trkn.hh"

#undef B0 // undo pollution from termios.h

#define MDEBUG(...)             Ase::debug ("memory", __VA_ARGS__)

namespace Ase {

struct MainAppImpl : MainApp {
  MainAppImpl ();
};
MainAppImpl main_app;
const MainApp &App = main_app;

MainAppImpl::MainAppImpl()
{}

LoopP       main_loop = Loop::current();
static String   arg_ui_mode;
static int      arg_unauth_port = 0;

// == JobQueue ==
static void
call_main_loop (const std::function<void()> &fun)
{
  main_loop->add (fun);
}
JobQueue main_jobs (call_main_loop);

// == MainConfig and arguments ==
static void
print_usage (bool help)
{
  if (!help)
    {
      printout ("%s %s\n", executable_name(), ase_version());
      printout ("Build: %s\n", ase_build_id());
      return;
    }
  printout ("Usage: %s [OPTIONS] [project.anklang]\n", executable_name());
  printout ("  --check          Run integrity tests\n");
  printout ("  --disable-randomization Test mode for deterministic tests\n");
  printout ("  --fatal-warnings Abort on warnings and failing assertions\n");
  printout ("  --help           Print program usage and options\n");
  printout ("  --jsbin          Print Javascript IPC & binary messages\n");
  printout ("  --jsipc          Print Javascript IPC messages\n");
  printout ("  --jsonts         Print TypeScript bindings\n");
  printout ("  --list-drivers   Print PCM and MIDI drivers\n");
  printout ("  --list-tests     List all test names\n");
  printout ("  --list-ui-tests  List all TypeScript UI test function names\n");
  printout ("  --norc           Prevent loading of any rc files\n");
  printout ("  --ui-test=test   Specify TypeScript UI test(s) to run (comma-separated)\n");
  printout ("  --ui-js=script   Run JavaScript code after UI startup\n");
  printout ("  --play-autostart Automatically start playback of `project.anklang`\n");
  printout ("  --rand64         Produce 64bit random numbers on stdout\n");
  printout ("  --test[=test]    Run specific test(s) (comma-separated)\n");
  printout ("  --unauth-dev=NUM Open an unauthenticated websocket port for testing\n");
  printout ("  --headless[=bool]  Run browser in headless mode (default for --ui-test)\n");
  printout ("  --ui <none|chromium|google-chrome|htmlgui>\n");
  printout ("                   Open GUI in web browser [htmlgui]\n");
  printout ("  --version        Print program version\n");
  printout ("  -M mididriver    Force use of <mididriver>\n");
  printout ("  -P pcmdriver     Force use of <pcmdriver>\n");
  printout ("  -o wavfile       Capture output to OPUS/FLAC/WAV file\n");
  printout ("  -t <time>        Automatically play and stop after <time> has passed\n"); // -t <time>[{,|;}tailtime]
  printout ("Options set via $ASE_DEBUG:\n");
  printout ("  :no-logfile:     Disable logging to ~/.cache/anklang/ instead of stderr\n");
}

/// Parse CLI option with argument, sets argv[*ith]=nullptr
static bool
parse_option_arg (const char *option, char **argv, unsigned *ith, const char **argp)
{
  const size_t l = strlen (option);
  if (strncmp (option, argv[*ith], l) == 0) {
    *argp = argv[*ith] + l;
    argv[*ith] = nullptr;
    if ((*argp)[0] == '=')
      *argp += 1;
    else if ((*argp)[0] == 0) {
      *ith += 1;
      *argp = argv[*ith] ? argv[*ith] : "";
      argv[*ith] = nullptr;
    }
    return true;
  }
  return false;
}

// 1:ERROR 2:FAILED+REJECT 4:IO 8:MESSAGE 16:GET 256:BINARY
static constexpr int jsipc_logflags = 1 | 2 | 4 | 8 | 16;
static constexpr int jsbin_logflags = 1 | 256;

static StringS check_test_names;
static StringS ui_test_names;
static String ui_js_script;

static void
parse_args (int *argcp, char **argv, MainAppImpl &config)
{
  if (0) // allow jsipc logging via ASE_DEBUG ?
    {
      config.jsonapi_logflags |= debug_key_enabled ("jsbin") ? jsbin_logflags : 0;
      config.jsonapi_logflags |= debug_key_enabled ("jsipc") ? jsipc_logflags : 0;
    }

  config.norc = false;
  bool sep = false; // -- separator
  std::string default_ui_mode = "htmlgui";
  const uint argc = *argcp;
  for (uint i = 1; i < argc; i++)
    {
      const char *optarg = nullptr;
      if (sep)
        config.args.push_back (argv[i]);
      else if (strcmp (argv[i], "--fatal-warnings") == 0 || strcmp (argv[i], "--g-fatal-warnings") == 0)
        logging_fatal_warnings = true;
      else if (strcmp ("--disable-randomization", argv[i]) == 0)
        config.allow_randomization = false;
      else if (strcmp ("--norc", argv[i]) == 0)
        config.norc = true;
      else if (strcmp ("--rand64", argv[i]) == 0)
        {
          FastRng prng;
          constexpr int N = 8192;
          uint64_t buffer[N];
          while (1)
            {
              for (size_t i = 0; i < N; i++)
                buffer[i] = prng.next();
              fwrite (buffer, sizeof (buffer[0]), N, stdout);
            }
          exit (0);
        }
      else if (strcmp ("--check", argv[i]) == 0)
        {
          config.mode = MainApp::CHECK_INTEGRITY_TESTS;
          logging_fatal_warnings = true;
          printerr ("CHECK_INTEGRITY_TESTS…\n");
          default_ui_mode = "none";
        }
      else if (strcmp ("--list-tests", argv[i]) == 0)
        {
          std::vector<std::string> ids;
          for (const auto &t : Test::list_tests())
            ids.push_back (t.ident);
          std::sort (ids.begin(), ids.end());
          for (const auto &t : ids)
            printout ("%s\n", t);
          exit (0);
        }
      else if (strcmp ("--list-ui-tests", argv[i]) == 0)
        {
          const String testfile = anklang_runpath (RPath::INSTALLDIR, "/ui/assets/testcalls-list.txt");
          if (!Path::check (testfile, "e"))
            fatal_error ("missing UI test list: %s", testfile);
          const String content = Path::stringread (testfile);
          StringS lines = string_split (content, "\n");
          for (const String &line : lines)
            if (!line.empty () || string_startswith (line, "#"))
              printout ("%s\n", line);
          exit (0);
        }
      else if (strcmp ("--ui-test", argv[i]) == 0 || strncmp ("--ui-test=", argv[i], 9) == 0)
        {
          const char *eq = strchr (argv[i], '=');
          const char *arg = eq ? eq + 1 : i+1 < argc ? argv[++i] : nullptr;
          if (arg) {
            const auto tests = string_split (arg, ",");
            for (const auto &t : tests)
              ui_test_names.push_back (t);
          } else
            ui_test_names.push_back ("all");
          config.headless = true;
        }
      else if (strcmp ("--ui-js", argv[i]) == 0 || strncmp ("--ui-js=", argv[i], 8) == 0)
        {
          const char *eq = strchr (argv[i], '=');
          ui_js_script = eq ? eq + 1 : i+1 < argc ? argv[++i] : "";
        }
      else if (strcmp ("--headless", argv[i]) == 0 || strncmp ("--headless=", argv[i], 10) == 0)
        {
          const char *eq = strchr (argv[i], '=');
          config.headless = eq ? string_to_bool (eq + 1) : true;
        }
      else if (strcmp ("--test", argv[i]) == 0 || strncmp ("--test=", argv[i], 7) == 0)
        {
          const char *eq = strchr (argv[i], '=');
          const char *arg = eq ? eq + 1 : i+1 < argc ? argv[++i] : nullptr;
          config.mode = MainApp::CHECK_INTEGRITY_TESTS;
          logging_fatal_warnings = true;
          if (arg) {
            const auto tests = string_split (arg, ",");
            for (const auto &t : tests)
              check_test_names.push_back (t);
          }
          default_ui_mode = "none";
        }
      else if (argv[i] == String ("--blake3") && i + 1 < size_t (argc))
        {
          argv[i++] = nullptr;
          String hash = blake3_hash_file (argv[i]);
          if (hash.empty())
            printerr ("%s: failed to read: %s\n", argv[i], strerror (errno));
          else
            printout ("%s\n", string_to_hex (hash));
          exit (hash == "");
        }
      else if (strcmp ("--jsonts", argv[i]) == 0) {
        if (getenv ("ASE_JSONTS") == nullptr)
          fatal_error ("%s: environment must contain ASE_JSONTS for --jsonts", argv[0]);
        printout ("%s\n", Jsonipc::g_binding_printer->finish());
        exit (0);
      } else if (strcmp ("--jsipc", argv[i]) == 0)
        config.jsonapi_logflags |= jsipc_logflags;
      else if (strcmp ("--jsbin", argv[i]) == 0)
        config.jsonapi_logflags |= jsbin_logflags;
      else if (strcmp ("--list-drivers", argv[i]) == 0)
        config.list_drivers = true;
      else if (strcmp ("-M", argv[i])  == 0 && i + 1 < size_t (argc))
        {
          argv[i++]  = nullptr;
          config.midi_override = argv[i];
        }
      else if (strcmp ("-P", argv[i]) == 0  && i  + 1  < size_t (argc))
        {
          argv[i++]  = nullptr;
          config.pcm_override  = argv[i];
        }
      else if (strcmp ("--no-devices", argv[i]) == 0)
        {
          config.no_devices = true;
        }
      else if (strcmp ("-h", argv[i]) == 0 ||
               strcmp ("--help", argv[i]) == 0)
        {
          print_usage (true);
          exit (0);
        }
      else if (strcmp ("--version", argv[i]) == 0)
        {
          print_usage (false);
          exit (0);
        }
      else if (argv[i] == String ("-o") && i + 1 < size_t (argc))
        {
          argv[i++] = nullptr;
          config.outputfile = argv[i];
        }
      else if (argv[i] == String ("--play-autostart"))
        {
          config.play_autostart = true;
          default_ui_mode = "none";
        }
      else if (parse_option_arg ("--unauth-dev", argv, &i, &optarg))
        {
          arg_unauth_port = string_to_int (optarg);
          default_ui_mode = "wait";
        }
      else if (argv[i] == String ("-t") && i + 1 < size_t (argc))
        {
          config.play_autostart = true;
          argv[i++] = nullptr;
          config.play_autostop = string_to_seconds (argv[i]);
          default_ui_mode = "none";
        }
      else if (parse_option_arg ("--ui", argv, &i, &optarg))
        {
          arg_ui_mode = optarg;
        }
      else if (argv[i] == String ("--") && !sep)
        sep = true;
      else if (argv[i][0] == '-' && !sep)
        fatal_error ("invalid command line argument: %s", argv[i]);
      else
        config.args.push_back (argv[i]);
      argv[i] = nullptr;
    }
  if (arg_ui_mode.empty())
    arg_ui_mode = default_ui_mode;
  if (*argcp > 1)
    {
      uint e = 1;
      for (uint i = 1; i < argc; i++)
        if (argv[i])
          {
            argv[e++] = argv[i];
            if (i >= e)
              argv[i] = nullptr;
          }
      *argcp = e;
    }
}

static String
make_auth_string()
{
  const char *const c52 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz";
  /* We use WebScoket subprotocol randomization as authentication, so:
   * a) Authentication happens *before* message interpretation, so an
   *    unauthenticated sender cannot cause crahses via e.g. rapidjson exceptions.
   * b) To serve as working authentication measure, the subprotocol random string
   *    must be cryptographically-secure.
   */
  KeccakCryptoRng csprng;
  String auth = "sessC";
  for (size_t i = 0; i < 23; ++i)
    auth += c52[csprng.random() % 52];  // each step adds 5.7 bits
  return auth;
}

static void
run_tests_and_quit ()
{
  if (check_test_names.empty())
    Test::run();
  else
    Test::run (check_test_names);
  main_loop->quit (0);
}

void
main_loop_wakeup ()
{
  LoopP loop = main_loop;
  if (loop)
    loop->wakeup();
}

static std::atomic<bool> seen_autostop = false;

// Lock and obstruction-free autostop trigger.
void
main_loop_autostop_mt()
{
  if (!seen_autostop)
    {
      seen_autostop = true;
      main_loop_wakeup();
    }
}

static bool
handle_autostop (const LoopState &state)
{
  switch (state.phase)
    {
    case LoopState::PREPARE:    return seen_autostop;
    case LoopState::CHECK:      return seen_autostop;
    case LoopState::DISPATCH:
      info ("Main: stopping playback (auto)");
      main_loop->quit (0);
      return true; // keep alive
    default: ;
    }
  return false;
}

static void
init_sigpipe()
{
  // don't die if we write() data to a process and that process dies (i.e. jackd)
  sigset_t signal_mask;
  sigemptyset (&signal_mask);
  sigaddset (&signal_mask, SIGPIPE);

  int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
  if (rc != 0)
    Ase::warning ("Ase: pthread_sigmask for SIGPIPE failed: %s\n", strerror (errno));
}

static std::atomic<bool> loft_needs_preallocation_mt = false;

// handle watermark underrun notifications
static void
notify_loft_lowmem ()
{
  if (!loft_needs_preallocation_mt)
    {
      loft_needs_preallocation_mt = true;
      Ase::main_loop_wakeup();
    }
}

static size_t last_loft_preallocation = 0;

static void
preallocate_loft (size_t preallocation)
{
  using namespace Ase;
  last_loft_preallocation = preallocation;
  LoftConfig loftcfg = {
    .preallocate = last_loft_preallocation,
    .watermark = last_loft_preallocation / 2,
    .flags = Loft::PREFAULT_PAGES,
  };
  loft_set_config (loftcfg);
  loft_set_notifier (notify_loft_lowmem);
  loft_grow_preallocate();
}

static bool
dispatch_loft_lowmem (const Ase::LoopState &lstate)
{
  using namespace Ase;
  const bool keep_alive = lstate.phase == LoopState::DISPATCH;
  // generally, dispatch logic may only run in LoopState::DISPATCH, but this handler
  // makes a rare exception, because we try to get ahead of concurrently runnint RT-threads...
  return_unless (loft_needs_preallocation_mt, keep_alive);
  loft_needs_preallocation_mt = false;
  last_loft_preallocation *= 2;
  const size_t newalloc = loft_grow_preallocate (last_loft_preallocation);
  LoftConfig config;
  loft_get_config (config);
  config.watermark = last_loft_preallocation / 2;
  loft_set_config (config);
  if (newalloc > 0)
    MDEBUG ("Loft preallocation in main thread: %f MB", newalloc / (1024. * 1024));
  return keep_alive;
}

static void
prefault_pages (size_t stacksize, size_t heapsize)
{
  const size_t pagesize = sysconf (_SC_PAGESIZE);
  char *heap = (char*) malloc (heapsize);
  if (heap)
    for (size_t i = 0; i < heapsize; i += pagesize)
      heap[i] = 1;
  free (heap);
  char *stack = (char*) alloca (stacksize);
  if (stack)
    for (size_t i = 0; i < stacksize; i += pagesize)
      stack[i] = 1;
}

static int
main (int argc, char *argv[])
{
  using namespace Ase;
  using namespace AnsiColors;

  // setup thread identifier
  TaskRegistry::setup_ase ("AnklangMainProc");
  // use malloc to serve allocations via sbrk only (avoid mmap)
  mallopt (M_MMAP_MAX, 0);
  // avoid releasing sbrk memory back to the system (reduce page faults)
  mallopt (M_TRIM_THRESHOLD, -1);
  // reserve large sbrk area and reduce page faults for heap and stack
  prefault_pages ((1024 + 768) * 1024, 64 * 1024 * 1024);
  // preallocate memory for lock-free allocator
  preallocate_loft (64 * 1024 * 1024);
  // warn if preallocation is not sufficient
  loft_set_growth_notifier ([] (size_t total, size_t needed)
  {
    warning ("Loft.BumpAllocator: growing beyond preallocation: totalmem=%u needed=%d\n", total, needed);
  });

  // print stack trace for uncaught exceptions
  logging_handle_terminate();

  // SIGPIPE init: needs to be done before any child thread is created
  init_sigpipe();
  // Enable us to reap and kill any grand child processes
  atquit_make_subreaper();

  // apply user locale
  if (!setlocale (LC_ALL, ""))
    fatal_error ("setlocale: locale not supported by libc: %s", ::strerror (errno));

  // parse args and config
  parse_args (&argc, argv, main_app);
  main_app.ui_tests = ui_test_names;
  main_app.ui_js = ui_js_script;
  const int socket_port = arg_unauth_port > 0 ? arg_unauth_port : 0;
  const char *socket_host = "127.0.0.1";
  const auto socket_info = WebSocketServer::bind_port (socket_host, socket_port);
  logging_configure (arg_ui_mode != "none" ? string_format ("%u", socket_info.port) : "");

  // handle loft preallocation needs
  main_loop->exec_dispatcher (dispatch_loft_lowmem, LoopPriority::SYSALLOC);

  // load preferences unless --norc was given
  if (!App.norc)
    Preference::load_preferences (true);

  // Ensure Ase server exists
  ServerImpl::instancep();

  // tracktion initialisation
  if (!trkn_init (argc, argv, App.no_devices))
    fatal_error ("Main: failed to initialize tracktion engine");

  const auto B1 = color (BOLD);
  const auto B0 = color (BOLD_OFF);

  // load drivers and dump device list
  load_registered_drivers();
  if (App.list_drivers)
    {
      Ase::Driver::EntryVec entries;
      printout ("%s", _("Available PCM drivers:\n"));
      entries = Ase::PcmDriver::list_drivers();
      std::sort (entries.begin(), entries.end(), [] (auto &a, auto &b) { return a.priority < b.priority; });
      for (const auto &entry : entries)
        {
          printout ("  %-30s (%s, %08x)\n\t%s\n%s%s%s%s", entry.devid + ":",
                    entry.readonly ? "Input" : entry.writeonly ? "Output" : "Duplex",
                    entry.priority, entry.device_name,
                    entry.capabilities.empty() ? "" : "\t" + entry.capabilities + "\n",
                    entry.device_info.empty() ? "" : "\t" + entry.device_info + "\n",
                    entry.hints.empty() ? "" : "\t(" + entry.hints + ")\n",
                    entry.notice.empty() ? "" : "\t" + entry.notice + "\n");
          if (debug_key_enabled ("driver"))
            printerr ("  %08x: %s\n", entry.priority, Driver::priority_string (entry.priority));
        }
      printout ("%s", _("Available MIDI drivers:\n"));
      entries = Ase::MidiDriver::list_drivers();
      std::sort (entries.begin(), entries.end(), [] (auto &a, auto &b) { return a.priority < b.priority; });
      for (const auto &entry : entries)
        {
          printout ("  %-30s (%s, %08x)\n\t%s\n%s%s%s%s", entry.devid + ":",
                    entry.readonly ? "Input" : entry.writeonly ? "Output" : "Duplex",
                    entry.priority, entry.device_name,
                    entry.capabilities.empty() ? "" : "\t" + entry.capabilities + "\n",
                    entry.device_info.empty() ? "" : "\t" + entry.device_info + "\n",
                    entry.hints.empty() ? "" : "\t(" + entry.hints + ")\n",
                    entry.notice.empty() ? "" : "\t" + entry.notice + "\n");
          if (debug_key_enabled ("driver"))
            printerr ("  %08x: %s\n", entry.priority, Driver::priority_string (entry.priority));
        }
      return 0;
    }

  // load projects
  ProjectImplP preload_project;
  for (const auto &filename : App.args)
    {
      preload_project = ProjectImpl::create (Path::basename (filename));
      Error error = Error::NO_MEMORY;
      if (preload_project)
        error = preload_project->load_project (filename);
      diag ("Main: load project: %s: %s", filename, ase_error_blurb (error));
      if (!!error)
        warning ("%s: failed to load project: %s", filename, ase_error_blurb (error));
    }

  // open Jsonapi socket
  const String auth_token = arg_unauth_port > 0 ? "" : make_auth_string();
  auto wss = WebSocketServer::create (jsonapi_make_connection, App.jsonapi_logflags, auth_token);
  main_app.web_socket_server = &*wss;
  wss->http_dir (anklang_runpath (RPath::INSTALLDIR, "/ui/"));
  // wss->http_alias ("/User/Controller", anklang_home_dir ("/Controller"));
  wss->http_alias ("/Builtin/Controller", anklang_runpath (RPath::INSTALLDIR, "/Controller"));
  // wss->http_alias ("/User/Scripts", anklang_home_dir ("/Scripts"));
  wss->http_alias ("/Builtin/Scripts", anklang_runpath (RPath::INSTALLDIR,"/Scripts"));
  const String subprotocol = ""; // make_auth_string()
  jsonapi_set_subprotocol (subprotocol);
  if (App.mode == MainApp::SYNTHENGINE && arg_ui_mode != "none") {
    wss->listen (socket_info, [] () { main_loop->quit (-1); });
    std::string webui_url = wss->url();
    if (!socket_port) {
      String redirecthtml = webui_create_auth_redirect ("anklang", wss->listen_port(), auth_token, arg_ui_mode);
      if (errno)
        fatal_error ("%s: failed to create html redirect file in $HOME", redirecthtml);
      webui_url = "file://" + redirecthtml;
      wss->see_other (webui_url);
    }
    info ("Main: WebUI address: %s", webui_url);

    WebuiFlags webui_flags = main_app.headless ? WebuiFlags::HEADLESS : WebuiFlags::NONE;
    if (main_app.ui_tests.size())
      webui_flags = webui_flags | WebuiFlags::CONSOLE_LOGS; // WebuiFlags::STDIO_REDIRECT
    auto ereason = webui_start_browser (arg_ui_mode, main_loop, webui_url,
                                        [] (int exit_code)
                                        {
                                          main_loop->quit (exit_code);
                                        }, webui_flags);

    if (ereason.error)
      fatal_error ("Main: failed to run WebUI: %s: %s", ereason.what, ::strerror (ereason.error));
  }

  // run atquit handler on SIGHUP SIGINT
  for (int sigid : { SIGHUP, SIGINT, SIGQUIT, SIGABRT, SIGTERM, SIGSYS }) {
    main_loop->exec_usignal (sigid, [] (int8 sig) {
      info ("Main: got signal %d: terminate", sig);
      atquit_terminate (-1);
      return false;
    });
    USignalSource::install_sigaction (sigid);
  }

  // catch SIGUSR2 to close sockets
  main_loop->exec_usignal (SIGUSR2, [wss] (int8 sig) {
    info ("Main: got signal %d: reset WebSocket", sig);
    wss->reset();
    return true;
  });
  USignalSource::install_sigaction (SIGUSR2);

  // start output capturing
  if (App.outputfile)
    ; // TODO: implement capturing

  // start auto play
  if (App.play_autostart && preload_project)
    main_loop->add ([preload_project] ()
    {
      info ("Main: starting playback (auto)");
      preload_project->start_playback (App.play_autostop);
    }, LoopPriority::IDLE);
  // handle automatic shutdown
  main_loop->exec_dispatcher (handle_autostop);

  // prune old log files after some time
  main_loop->add ([] ()
  {
    logging_prune_old_logs (3.0 * 24.0 * 60.0 * 60.0);
  }, std::chrono::milliseconds (5000), LoopPriority::IDLE);

  // run test suite
  if (App.mode == MainApp::CHECK_INTEGRITY_TESTS)
    main_loop->add (run_tests_and_quit);

  // run main event loop and catch SIGUSR2
  const int exitcode = main_loop->run();
  assert_return (main_loop, -1); // ptr must be kept around
  diag ("Main: event loop quit: code=%d", exitcode);

  // cleanup
  wss->shutdown(); // close socket, allow no more calls
  main_app.web_socket_server = nullptr;
  wss = nullptr;

  // deactivate any projects, releases Audio resources
  ProjectImpl::force_shutdown_all();

  // halt audio engine, join its threads, dispatch cleanups
  main_loop->iterate_pending();

  // shutdown tracktion *after* main loop stopped
  trkn_shutdown ();

  diag ("Main: exiting: %d", exitcode);
  atquit_terminate (exitcode);
  return exitcode;
}

} // Ase

int
main (int argc, char *argv[])
{
  int r = -128;
#ifdef ASE_WITH_CPPTRACE
  CPPTRACE_TRY { r = Ase::main (argc, argv); }
  CPPTRACE_CATCH (const std::exception& e) {
    std::string msg = "Exception: ";
    msg += e.what();
    msg += "\n";
    fflush (stdout);
    fputs (msg.c_str(), stderr);
    fflush (stderr);
    cpptrace::from_current_exception().print();
  }
#else
  r =  Ase::main (argc, argv);
#endif
  return r;
}
