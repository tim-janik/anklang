// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "main.hh"
#include "api.hh"
#include "path.hh"
#include "utils.hh"
#include "jsonapi.hh"
#include "driver.hh"
#include "engine.hh"
#include "project.hh"
#include "loft.hh"
#include "compress.hh"
#include "webui.hh"
#include "internal.hh"
#include "testing.hh"

#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>

#undef B0 // undo pollution from termios.h

#define MDEBUG(...)             Ase::debug ("memory", __VA_ARGS__)

namespace { // Anon
static void print_class_tree ();
} // Anon

namespace Ase {

struct MainAppImpl : MainApp {
  MainAppImpl ();
};
MainAppImpl main_app;
const MainApp &App = main_app;

MainAppImpl::MainAppImpl()
{}

MainLoopP          main_loop;
static int         embedding_fd = -1;
static bool        arg_js_api = false;
static bool        arg_class_tree = false;
static String      arg_ui_mode;
static int         arg_unauth_port = 0;

// == JobQueue ==
static void
call_main_loop (const std::function<void()> &fun)
{
  main_loop->exec_callback (fun);
}
JobQueue main_jobs (call_main_loop);

// == RtCall::Callable ==
struct RtCallJob {
  explicit RtCallJob (const RtCall &ucall) : call (ucall) {}
  LoftPtr<RtCallJob>      loftptr;
  std::atomic<RtCallJob*> next = nullptr;
  RtCall                  call;
};
static inline std::atomic<RtCallJob*>&
atomic_next_ptrref (RtCallJob *j)
{
  return j->next;
}

static AtomicIntrusiveStack<RtCallJob> main_rt_jobs_;
RtJobQueue main_rt_jobs;

void
RtJobQueue::operator+= (const RtCall &call)
{
  LoftPtr<RtCallJob> loftptr = loft_make_unique<RtCallJob> (call);
  RtCallJob *const calljob = &*loftptr;
  calljob->loftptr = std::move (loftptr); // keeps itself alive
  const bool was_empty = main_rt_jobs_.push (calljob);
  if (was_empty)
    main_loop_wakeup();
}

static bool
main_rt_jobs_pending()
{
  return !main_rt_jobs_.empty();
}

static void
main_rt_jobs_process()
{
  RtCallJob *calljob = main_rt_jobs_.pop_reversed();
  while (calljob) {
    LoftPtr<RtCallJob> loftptr = std::move (calljob->loftptr); // assume ownership
    calljob = calljob->next;
    loftptr->call.invoke();
  }
}

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
  printout ("  --class-tree     Print exported class tree\n");
  printout ("  --disable-randomization Test mode for deterministic tests\n");
  printout ("  --embed <fd>     Parent process socket for embedding\n");
  printout ("  --fatal-warnings Abort on warnings and failing assertions\n");
  printout ("  --help           Print program usage and options\n");
  printout ("  --js-api         Print Javascript bindings\n");
  printout ("  --jsbin          Print Javascript IPC & binary messages\n");
  printout ("  --jsipc          Print Javascript IPC messages\n");
  printout ("  --list-drivers   Print PCM and MIDI drivers\n");
  printout ("  --list-tests     List all test names\n");
  printout ("  --norc           Prevent loading of any rc files\n");
  printout ("  --play-autostart Automatically start playback of `project.anklang`\n");
  printout ("  --rand64         Produce 64bit random numbers on stdout\n");
  printout ("  --test[=test]    Run specific tests\n");
  printout ("  --unauth-dev=NUM Open an unauthenticated websocket port for testing\n");
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

LogFlags
log_setup (int *logfd)
{
  int flags = LOG_STDERR;
  const char *asedebug = getenv ("ASE_DEBUG");
  if (!asedebug || !strstr (asedebug, "no-log2file")) {
    const String logdir = Path::join (Path::xdg_dir ("CACHE"), "anklang");
    if (Path::mkdirs (logdir)) {
      const String fname = string_format ("%s/%s-%08x.log", logdir, program_alias(), gethostid());
      const int OFLAGS = O_CREAT | O_EXCL | O_WRONLY | O_NOCTTY | O_NOFOLLOW | O_CLOEXEC; // O_TRUNC
      const int OMODE = 0640;
      errno = EBUSY;
      *logfd = open (fname.c_str(), OFLAGS, OMODE);
      if (*logfd < 0 && errno == EEXIST) {
        const String oldname = fname + ".old";
        if (rename (fname.c_str(), oldname.c_str()) < 0)
          perror (string_format ("%s: failed to rename \"%s\"", program_alias(), oldname.c_str()).c_str());
        *logfd = open (fname.c_str(), OFLAGS, OMODE);
        if (*logfd < 0)
          perror (string_format ("%s: failed to open log file \"%s\"", program_alias(), fname.c_str()).c_str());
      }
    }
  }
  return LogFlags (flags);
}

// 1:ERROR 2:FAILED+REJECT 4:IO 8:MESSAGE 16:GET 256:BINARY
static constexpr int jsipc_logflags = 1 | 2 | 4 | 8 | 16;
static constexpr int jsbin_logflags = 1 | 256;

static StringS check_test_names;

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
        ase_fatal_warnings = assertion_failed_fatal = true;
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
          ase_fatal_warnings = assertion_failed_fatal = true;
          printerr ("CHECK_INTEGRITY_TESTS…\n");
          default_ui_mode = "none";
        }
      else if (strcmp ("--list-tests", argv[i]) == 0)
        {
          for (const auto &t : Test::list_tests())
            printout ("%s\n", t.ident);
          exit (0);
        }
      else if (strcmp ("--test", argv[i]) == 0 || strncmp ("--test=", argv[i], 7) == 0)
        {
          const char *eq = strchr (argv[i], '=');
          const char *arg = eq ? eq + 1 : i+1 < argc ? argv[++i] : nullptr;
          config.mode = MainApp::CHECK_INTEGRITY_TESTS;
          ase_fatal_warnings = assertion_failed_fatal = true;
          if (arg)
            check_test_names.push_back (arg);
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
      else if (strcmp ("--js-api", argv[i]) == 0)
        arg_js_api = true;
      else if (strcmp ("--class-tree", argv[i]) == 0)
        arg_class_tree = true;
      else if (strcmp ("--jsipc", argv[i]) == 0)
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
      else if (argv[i] == String ("--embed") && i + 1 < size_t (argc))
        {
          argv[i++] = nullptr;
          embedding_fd = string_to_int (argv[i]);
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
  MainLoopP loop = main_loop;
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
      log ("Main: stopping playback (auto)");
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

} // Ase

int
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

  // SIGPIPE init: needs to be done before any child thread is created
  init_sigpipe();

  // apply user locale
  if (!setlocale (LC_ALL, ""))
    perror ("setlocale: locale not supported by libc");

  // parse args and config
  parse_args (&argc, argv, main_app);

  // prepare main event loop (needed before parse_args)
  main_loop = MainLoop::create();
  // handle loft preallocation needs
  main_loop->exec_dispatcher (dispatch_loft_lowmem, EventLoop::PRIORITY_CEILING);

  // load preferences unless --norc was given
  if (!App.norc)
    Preference::load_preferences (true);

  const auto B1 = color (BOLD);
  const auto B0 = color (BOLD_OFF);

  // CLI printout commands
  if (arg_js_api)
    {
      printout ("%s\n", Jsonipc::ClassPrinter::to_string());
      return 0;
    }
  if (arg_class_tree)
    {
      print_class_tree();
      return 0;
    }

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

  // start audio engine
  AudioEngine &audio_engine = make_audio_engine (main_loop_wakeup, 48000, SpeakerArrangement::STEREO);
  main_app.engine = &audio_engine;
  audio_engine.start_threads ();
  /*const uint loopdispatcherid =*/
  main_loop->exec_dispatcher ([&audio_engine] (const LoopState &state) -> bool {
    switch (state.phase)
      {
      case LoopState::PREPARE:
        return main_rt_jobs_pending() || audio_engine.ipc_pending();
      case LoopState::CHECK:
        return main_rt_jobs_pending() || audio_engine.ipc_pending();
      case LoopState::DISPATCH:
        audio_engine.ipc_dispatch();
        main_rt_jobs_process();
        return true;
      default:
        return false;
      }
  });

  // load projects
  ProjectImplP preload_project;
  for (const auto &filename : App.args)
    {
      preload_project = ProjectImpl::create (Path::basename (filename));
      Error error = Error::NO_MEMORY;
      if (preload_project)
        error = preload_project->load_project (filename);
      log ("Main: load project: %s: %s", filename, ase_error_blurb (error));
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
  const int xport = embedding_fd >= 0 ? 0 : (arg_unauth_port > 0 ? arg_unauth_port : 0);
  const String subprotocol = ""; // make_auth_string()
  jsonapi_set_subprotocol (subprotocol);
  if (App.mode == MainApp::SYNTHENGINE && arg_ui_mode != "none") {
    const char *host = "127.0.0.1";
    wss->listen (host, xport, [] () { main_loop->quit (-1); });
    std::string webui_url = wss->url();
    if (!xport) {
      String redirecthtml = webui_create_auth_redirect ("anklang", wss->listen_port(), auth_token, arg_ui_mode);
      if (errno)
        fatal_error ("%s: failed to create html redirect file in $HOME", redirecthtml);
      webui_url = "file://" + redirecthtml;
      wss->see_other (webui_url);
    }
    log ("Main: WebUI address: %s", webui_url);
    auto ereason = webui_start_browser (arg_ui_mode, main_loop, webui_url, [] () { main_loop->quit (0); });
    if (ereason.error)
      fatal_error ("Main: failed to run WebUI: %s: %s", ereason.what, ::strerror (ereason.error));
  }
  const String url = wss->url() + (subprotocol.empty() ? "" : "?subprotocol=" + subprotocol);
  if (embedding_fd < 0 && !url.empty())
    printout ("%sLISTEN:%s %s\n", B1, B0, url);

  // run atquit handler on SIGHUP SIGINT
  for (int sigid : { SIGHUP, SIGINT }) {
    main_loop->exec_usignal (sigid, [] (int8 sig) {
      log ("Main: got signal %d: aborting", sig);
      atquit_terminate (-1);
      return false;
    });
    USignalSource::install_sigaction (sigid);
  }

  // catch SIGUSR2 to close sockets
  main_loop->exec_usignal (SIGUSR2, [wss] (int8 sig) {
    log ("Main: got signal %d: reset WebSocket", sig);
    wss->reset();
    return true;
  });
  USignalSource::install_sigaction (SIGUSR2);

  // monitor and allow auth over keep-alive-fd
  if (embedding_fd >= 0)
    {
      const uint ioid = main_loop->exec_io_handler ([wss] (PollFD &pfd) {
        String msg (512, 0);
        if (pfd.revents & PollFD::IN)
          {
            ssize_t n = read (embedding_fd, &msg[0], msg.size()); // flush input
            msg.resize (n > 0 ? n : 0);
            log ("Main: Embedder Msg: %s", msg);
          }
        if (string_strip (msg) == "QUIT" || (pfd.revents & (PollFD::ERR | PollFD::HUP | PollFD::NVAL)))
          wss->shutdown();
        return true;
      }, embedding_fd, "rB");
      (void) ioid;

      const String jsonurl = "{ \"url\": \"" + url + "\" }";
      ssize_t n;
      do
        n = write (embedding_fd, jsonurl.data(), jsonurl.size());
      while (n < 0 && errno == EINTR);
    }

  // start output capturing
  if (App.outputfile)
    {
      std::shared_ptr<CallbackS> callbacks = std::make_shared<CallbackS>();
      log ("Main: Start caputure: %s", App.outputfile);
      App.engine->queue_capture_start (*callbacks, App.outputfile, true);
      auto job = [callbacks] () {
        for (const auto &callback : *callbacks)
          callback();
      };
      App.engine->async_jobs += job;
    }

  // start auto play
  if (App.play_autostart && preload_project)
    main_loop->exec_idle ([preload_project] () {
      log ("Main: starting playback (auto)");
      preload_project->start_playback (App.play_autostop);
    });
  // handle automatic shutdown
  main_loop->exec_dispatcher (handle_autostop);

  // run test suite
  if (App.mode == MainApp::CHECK_INTEGRITY_TESTS)
    main_loop->exec_now (run_tests_and_quit);

  // run main event loop and catch SIGUSR2
  const int exitcode = main_loop->run();
  assert_return (main_loop, -1); // ptr must be kept around
  log ("Main: event loop quit: code=%d", exitcode);

  // cleanup
  wss->shutdown(); // close socket, allow no more calls
  main_app.web_socket_server = nullptr;
  wss = nullptr;

  // halt audio engine, join its threads, dispatch cleanups
  audio_engine.set_project (nullptr);
  audio_engine.stop_threads();
  main_loop->iterate_pending();
  main_app.engine = nullptr;

  log ("Main: exiting: %d", exitcode);
  return exitcode;
}

namespace { // Anon
using namespace Ase;

extern "C" __attribute__ ((__noinline__)) void
tlog1 (const char *s)
{
  log ("foo: %s+%d", s, 0x11111111);
}

extern "C" __attribute__ ((__noinline__)) void
tlog2 (const char *s)
{
  log ("foo: %s+%d", s, 0x11111111);
}

TEST_INTEGRITY (job_queue_tests);
static void
job_queue_tests()
{
  bool seen_engine_job = false, seen_deleter = false;
  // enqueue job with deleter into engine
  AudioEngine *e = App.engine;
  std::shared_ptr<void> vp = { nullptr, [e,&seen_deleter] (void*) {
    printerr ("  job_queue_tests: Run Deleter (in_engine=%d)\n", e->thread_id == std::this_thread::get_id());
    seen_deleter = true;
  } };
  e->async_jobs += [e,vp,&seen_engine_job] () {
    printerr ("  job_queue_tests: Run Handler (in_engine=%d)\n", e->thread_id == std::this_thread::get_id());
    seen_engine_job = true;
  };
  vp.reset(); // required to allow deleter execution further down
  // enqueue jobs into main loop
  main_rt_jobs += RtCall ([]() { printerr ("  job_queue_tests: Hello %s!\n", "void()"); });
  main_rt_jobs += RtCall ((void(*)(const char*)) [] (const char *a) { printerr ("  job_queue_tests: Hello %s!\n", a); }, "RtJobQueue");
  struct Test1 { const char *a_; void print() { printerr ("  job_queue_tests: Hello %s!\n", a_); } };
  static Test1 test1 { "MemFn" };
  main_rt_jobs += RtCall (test1, &Test1::print);
  // lame busy looping to give the engine a chance at the job queue
  uint64 start_usecs = timestamp_realtime();
  do {
    usleep (1500);      // give the audio engine some time
    main_loop->iterate (false);
  } while (timestamp_realtime() < start_usecs + 1871 * 1000 && !seen_deleter);
  assert_return (seen_engine_job == true);
  assert_return (seen_deleter == true);
}

struct JWalker : Jsonipc::ClassWalker {
  struct Class { String name; int depth = 0; Class *base = nullptr; StringS derived; };
  std::map<std::string,Class> classmap;
  void
  new_class (const std::string &classname, const std::string &base)
  {
    Class *basep = nullptr;
    int depth = 0;
    if (!base.empty())
      {
        Class &b = classmap[base];
        depth = b.depth + 1;
        b.derived.push_back (classname);
        basep = &b;
      }
    classmap[classname] = { classname, depth, basep };
  }
  void
  print_class (const Class &c, bool sibling, const std::string &indent)
  {
    if (c.depth)
      printout ("%s|\n", indent);
    printout ("%s%s\n", c.depth ? indent + "+" : indent, c.name);
    for (size_t i = 0; i < c.derived.size(); i++)
      print_class (classmap[c.derived[i]], i + 1 < c.derived.size(),
                   sibling ? indent + "|  " : indent + "   ");
  }
  void
  print_recursive()
  {
    for (const auto &c : classmap)
      if (c.second.depth == 0)
        print_class (c.second, 0, "");
  }
};

static void
print_class_tree()
{
  JWalker walk;
  Jsonipc::ClassPrinter::walk (walk);
  walk.print_recursive();
}

} // Anon
