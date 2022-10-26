// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "main.hh"
#include "api.hh"
#include "path.hh"
#include "utils.hh"
#include "jsonapi.hh"
#include "driver.hh"
#include "engine.hh"
#include "project.hh"
#include "compress.hh"
#include "internal.hh"
#include "testing.hh"

#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>

#undef B0 // undo pollution from termios.h

namespace { // Anon
static void print_class_tree ();
} // Anon

namespace Ase {

MainLoopP          main_loop;
MainConfig         main_config_;
const MainConfig  &main_config = main_config_;
static int         embedding_fd = -1;
static bool        arg_js_api = false;
static bool        arg_class_tree = false;

// == JobQueue ==
static void
call_main_loop (JobQueue::Policy policy, const std::function<void()> &fun)
{
  if (policy == JobQueue::SYNC)
    {
      ScopedSemaphore sem;
      std::function<void()> wrapper = [&sem, &fun] () {
        fun();
        sem.post();
      };
      main_loop->exec_callback (wrapper);
      sem.wait();
    }
  else
    main_loop->exec_callback (fun);
}
JobQueue main_jobs (call_main_loop);

// == Feature Toggles ==
/// Find @a feature in @a config, return its value or @a fallback.
String
feature_toggle_find (const String &config, const String &feature, const String &fallback)
{
  String haystack = ":" + config + ":";
  String needle0 = ":no-" + feature + ":";
  String needle1 = ":" + feature + ":";
  String needle2 = ":" + feature + "=";
  const char *n0 = strrstr (haystack.c_str(), needle0.c_str());
  const char *n1 = strrstr (haystack.c_str(), needle1.c_str());
  const char *n2 = strrstr (haystack.c_str(), needle2.c_str());
  if (n0 && (!n1 || n0 > n1) && (!n2 || n0 > n2))
    return "0";         // ":no-feature:" is the last toggle in config
  if (n1 && (!n2 || n1 > n2))
    return "1";         // ":feature:" is the last toggle in config
  if (!n2)
    return fallback;    // no "feature" variant found
  const char *value = n2 + strlen (needle2.c_str());
  const char *end = strchr (value, ':');
  return end ? String (value, end - value) : String (value);
}

/// Check for @a feature in @a config, if @a feature is empty, checks for *any* feature.
bool
feature_toggle_bool (const char *config, const char *feature)
{
  if (feature && feature[0])
    return string_to_bool (feature_toggle_find (config ? config : "", feature));
  // check if *any* feature is enabled in config
  if (!config || !config[0])
    return false;
  const size_t l = strlen (config);
  for (size_t i = 0; i < l; i++)
    if (config[i] && !strchr (": \t\n\r=", config[i]))
      return true;      // found *some* non-space and non-separator config item
  return false;         // just whitespace
}

/// Check if `feature` is enabled via $ASE_FEATURE.
bool
feature_check (const char *feature)
{
  const char *const asefeature = getenv ("ASE_FEATURE");
  return asefeature ? feature_toggle_bool (asefeature, feature) : false;
}

// == MainConfig and arguments ==
static void
print_usage (bool help)
{
  if (!help)
    {
      printout ("%s version %s\n", executable_name(), ase_version());
      return;
    }
  printout ("Usage: %s [OPTIONS]\n", executable_name());
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
  printout ("  --preload <prj>  Preload project as current\n");
  printout ("  --rand64         Produce 64bit random numbers on stdout\n");
  printout ("  --version        Print program version\n");
}

// 1:ERROR 2:FAILED+REJECT 4:IO 8:MESSAGE 16:GET 256:BINARY
static constexpr int jsipc_logflags = 1 | 2 | 4 | 8 | 16;
static constexpr int jsbin_logflags = 1 | 256;

static MainConfig
parse_args (int *argcp, char **argv)
{
  MainConfig config;

  if (0) // allow jsipc logging via ASE_DEBUG ?
    {
      config.jsonapi_logflags |= debug_key_enabled ("jsbin") ? jsbin_logflags : 0;
      config.jsonapi_logflags |= debug_key_enabled ("jsipc") ? jsipc_logflags : 0;
    }
  config.fatal_warnings = feature_check ("fatal-warnings");

  bool sep = false; // -- separator
  const uint argc = *argcp;
  for (uint i = 1; i < argc; i++)
    {
      if (sep)
        config.args.push_back (argv[i]);
      else if (strcmp (argv[i], "--fatal-warnings") == 0 || strcmp (argv[i], "--g-fatal-warnings") == 0)
        config.fatal_warnings = true;
      else if (strcmp ("--disable-randomization", argv[i]) == 0)
        config.allow_randomization = false;
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
        }
      else if (strcmp ("--check", argv[i]) == 0)
        {
          config.mode = MainConfig::CHECK_INTEGRITY_TESTS;
          config.fatal_warnings = true;
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
      else if (argv[i] == String ("--preload") && i + 1 < size_t (argc))
        {
          argv[i++] = nullptr;
          config.preload = argv[i];
        }
      else if (argv[i] == String ("--") && !sep)
        sep = true;
      else if (argv[i][0] == '-' && !sep)
        fatal_error ("invalid command line argument: %s", argv[i]);
      else
        config.args.push_back (argv[i]);
      argv[i] = nullptr;
    }
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
  return config;
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
  String auth;
  for (size_t i = 0; i < 8; ++i)
    auth += c52[csprng.random() % 52];  // each step adds 5.7 bits
  return auth;
}

static void
run_tests_and_quit ()
{
  printerr ("CHECK_INTEGRITY_TESTS…\n");
  Test::run();
  main_loop->quit (0);
}

void
main_loop_wakeup ()
{
  MainLoopP loop = main_loop;
  if (loop)
    loop->wakeup();
}

} // Ase

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

int
main (int argc, char *argv[])
{
  using namespace Ase;
  using namespace AnsiColors;

  // use malloc to serve allocations via sbrk only (avoid mmap)
  mallopt (M_MMAP_MAX, 0);
  // avoid releasing sbrk memory back to the system (reduce page faults)
  mallopt (M_TRIM_THRESHOLD, -1);
  // reduce page faults for heap and stack
  prefault_pages ((1024 + 768) * 1024, 128 * 1024 * 1024);

  // setup thread and handle args and config
  TaskRegistry::setup_ase ("AnklangMainProc");
  main_config_ = parse_args (&argc, argv);
  const MainConfig &config = main_config_;

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

  // SIGPIPE init: needs to be done before any child thread is created
  init_sigpipe();

  // prepare main event loop
  main_loop = MainLoop::create();

  // load drivers and dump device list
  load_registered_drivers();
  if (config.list_drivers)
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
  AudioEngine &ae = make_audio_engine (main_loop_wakeup, 48000, SpeakerArrangement::STEREO);
  main_config_.engine = &ae;
  ae.start_threads ();
  const uint loopdispatcherid = main_loop->exec_dispatcher ([&ae] (const LoopState &state) -> bool {
    switch (state.phase)
      {
      case LoopState::PREPARE:
        return ae.ipc_pending();
      case LoopState::CHECK:
        return ae.ipc_pending();
      case LoopState::DISPATCH:
        ae.ipc_dispatch();
        return true;
      default:
        return false;
      }
  });

  // preload project
  ProjectP preload_project;
  if (config.preload)
    {
      preload_project = ProjectImpl::create (config.preload);
      Error error = Error::NO_MEMORY;
      if (preload_project)
        error = preload_project->load_project (config.preload);
      if (!!error)
        warning ("%s: failed to load project: %s", config.preload, ase_error_blurb (error));
    }

  // open Jsonapi socket
  auto wss = WebSocketServer::create (jsonapi_make_connection, config.jsonapi_logflags);
  main_config_.web_socket_server = &*wss;
  wss->http_dir (anklang_runpath (RPath::INSTALLDIR, "/ui/"));
  wss->http_alias ("/User/Controller", anklang_home_dir ("/Controller"));
  wss->http_alias ("/Builtin/Controller", anklang_runpath (RPath::INSTALLDIR, "/Controller"));
  wss->http_alias ("/User/Scripts", anklang_home_dir ("/Scripts"));
  wss->http_alias ("/Builtin/Scripts", anklang_runpath (RPath::INSTALLDIR,"/Scripts"));
  const int xport = embedding_fd >= 0 ? 0 : 1777;
  const String subprotocol = xport ? "" : make_auth_string();
  jsonapi_require_auth (subprotocol);
  if (main_config.mode == MainConfig::SYNTHENGINE)
    wss->listen ("127.0.0.1", xport, [] () { main_loop->quit (-1); });
  const String url = wss->url() + (subprotocol.empty() ? "" : "?subprotocol=" + subprotocol);
  if (embedding_fd < 0 && !url.empty())
    printout ("%sLISTEN:%s %s\n", B1, B0, url);

  // catch SIGUSR2 to close sockets
  {
    struct sigaction action;
    action.sa_handler = [] (int) { USignalSource::raise (SIGUSR2); };
    sigemptyset (&action.sa_mask);
    action.sa_flags = SA_NOMASK;
    sigaction (SIGUSR2, &action, nullptr);
  }
  main_loop->exec_usignal (SIGUSR2, [wss] (int8) { wss->reset(); return true; });

  // monitor and allow auth over keep-alive-fd
  if (embedding_fd >= 0)
    {
      const uint ioid = main_loop->exec_io_handler ([wss] (PollFD &pfd) {
        String msg (512, 0);
        if (pfd.revents & PollFD::IN)
          {
            ssize_t n = read (embedding_fd, &msg[0], msg.size()); // flush input
            msg.resize (n > 0 ? n : 0);
            if (!msg.empty())
              printerr ("Embedder: %s%s", msg, msg.back() == '\n' ? "" : "\n");
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

  // run test suite
  if (main_config.mode == MainConfig::CHECK_INTEGRITY_TESTS)
    main_loop->exec_now (run_tests_and_quit);

  // Debugging test
  if (0)
    {
      AudioEngine *e = main_config_.engine;
      std::shared_ptr<void> vp = { nullptr, [e] (void*) {
        printerr ("JOBTEST: Run Deleter (in_engine=%d)\n", e->thread_id == std::this_thread::get_id());
      } };
      e->async_jobs += [e,vp] () {
        printerr ("JOBTEST: Run Handler (in_engine=%d)\n", e->thread_id == std::this_thread::get_id());
      };
    }

  // run main event loop and catch SIGUSR2
  const int exitcode = main_loop->run();
  assert_return (main_loop, -1); // ptr must be kept around

  // loop ended, close socket and shutdown
  wss->shutdown();
  main_config_.web_socket_server = nullptr;
  wss = nullptr;

  // halt audio engine, join its threads, dispatch cleanups
  ae.stop_threads();
  main_loop->remove (loopdispatcherid);
  while (ae.ipc_pending())
    ae.ipc_dispatch();
  main_config_.engine = nullptr;

  return exitcode;
}

namespace { // Anon
using namespace Ase;

TEST_INTEGRITY (test_feature_toggles);
static void
test_feature_toggles()
{
  String r;
  r = feature_toggle_find ("a:b", "a"); TCMP (r, ==, "1");
  r = feature_toggle_find ("a:b", "b"); TCMP (r, ==, "1");
  r = feature_toggle_find ("a:b", "c"); TCMP (r, ==, "0");
  r = feature_toggle_find ("a:b", "c", "7"); TCMP (r, ==, "7");
  r = feature_toggle_find ("a:no-b", "b"); TCMP (r, ==, "0");
  r = feature_toggle_find ("no-a:b", "a"); TCMP (r, ==, "0");
  r = feature_toggle_find ("no-a:b:a", "a"); TCMP (r, ==, "1");
  r = feature_toggle_find ("no-a:b:a=5", "a"); TCMP (r, ==, "5");
  r = feature_toggle_find ("no-a:b:a=5:c", "a"); TCMP (r, ==, "5");
  bool b;
  b = feature_toggle_bool ("", "a"); TCMP (b, ==, false);
  b = feature_toggle_bool ("a:b:c", "a"); TCMP (b, ==, true);
  b = feature_toggle_bool ("no-a:b:c", "a"); TCMP (b, ==, false);
  b = feature_toggle_bool ("no-a:b:a=5:c", "b"); TCMP (b, ==, true);
  b = feature_toggle_bool ("x", ""); TCMP (b, ==, true); // *any* feature?
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
