// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/api.hh>
#include <ase/atquit.hh>
#include <ase/jsonapi.hh>
#include <ase/path.hh>
#include <ase/storage.hh>
#include <ase/testing.hh>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using namespace Ase;

static String
capture_stderr (const std::function<void()> &func)
{
  FILE *file = tmpfile();
  TASSERT (file);
  const int saved = dup (STDERR_FILENO);
  TASSERT (saved >= 0);
  fflush (stderr);
  TASSERT (dup2 (fileno (file), STDERR_FILENO) == STDERR_FILENO);
  func();
  fflush (stderr);
  TASSERT (dup2 (saved, STDERR_FILENO) == STDERR_FILENO);
  close (saved);
  rewind (file);
  String output;
  char buffer[4096];
  while (const size_t count = fread (buffer, 1, sizeof (buffer), file))
    output.append (buffer, count);
  TASSERT (!ferror (file));
  fclose (file);
  return output;
}

static int
wait_child (pid_t child)
{
  int status = 0;
  pid_t waited;
  do
    waited = waitpid (child, &status, 0);
  while (waited < 0 && errno == EINTR);
  TASSERT (waited == child);
  return status;
}

static String
capture_abort (const std::function<void()> &func)
{
  int status = 0;
  const String output = capture_stderr ([&] {
    const pid_t child = fork();
    TASSERT (child >= 0);
    if (!child)
      {
        const rlimit limit { 0, 0 };
        setrlimit (RLIMIT_CORE, &limit);
        unsetenv ("ASE_DEBUG");
        func();
        _exit (0);
      }
    status = wait_child (child);
  });
  TASSERT (WIFSIGNALED (status) && WTERMSIG (status) == SIGABRT);
  return output;
}

static void
formatter_diagnostics()
{
  String missing, invalid, null_format;
  const String output = capture_stderr ([&] {
    missing = string_format ("%d %s", 5);
    const char *bad_format = "%!";
    invalid = string_format (bad_format);
    null_format = string_format (nullptr);
  });
  TCMP (missing, ==, "%d %s");
  TCMP (invalid, ==, "%!");
  TCMP (null_format, ==, "<nullptr>");
  TASSERT (output.find ("StringFormatter: WARNING: too few arguments for format: %d %s\n") != String::npos);
  TASSERT (output.find ("in directive 1: %!\n") != String::npos);
  TASSERT (output.find ("StringFormatter: WARNING: format is nullptr: <nullptr>\n") != String::npos);
}
TEST_ADD (formatter_diagnostics);

static void
spawn_failure_diagnostics()
{
  const String dir = anklang_cachedir_create();
  TASSERT (!dir.empty());
  const String program = Path::join (dir, "missing-program");
  pid_t child = 0;
  int status = 0;
  const String output = capture_stderr ([&] {
    const ErrorReason error = spawn_process ({ program }, &child);
    TASSERT (error.error == 0);
    status = wait_child (child);
  });
  TASSERT (WIFEXITED (status) && WEXITSTATUS (status) == 127);
  const String expected = "[pid=" + std::to_string (child) + "]: fork to exec " + program +
                          ": execvp: errno=" + std::to_string (ENOENT) + "\n";
  TASSERT (output.find (expected) != String::npos);
  anklang_cachedir_cleanup (dir);
}
TEST_ADD (spawn_failure_diagnostics);

struct DiagnosticObject : Jsonipc::SharedBase {};

static void
jsonipc_diagnostics()
{
  Jsonipc::Class<DiagnosticObject> type;
  type.inherit<Jsonipc::SharedBase>();
  Jsonipc::InstanceMap instances;
  Jsonipc::Scope scope (instances);
  Jsonipc::JsonAllocator allocator;
  auto object = std::make_shared<DiagnosticObject>();
  const auto value = Jsonipc::to_json (object, allocator);
  const String id_text = "$id=" + std::to_string (value["$id"].GetUint64()) + "\n";
  const String output = capture_stderr ([&] {
    JSONIPC_WARNING ("%s %d", "diagnostic", 17);
    JSONIPC_WARNING ("plain diagnostic");
    instances.clear (true);
  });
  TASSERT (output.find (": warning: diagnostic 17\n") != String::npos);
  TASSERT (output.find (": warning: plain diagnostic\n") != String::npos);
  TASSERT (output.find ("Jsonipc::~Wrapper:") != String::npos);
  TASSERT (output.find ("DiagnosticObject") != String::npos);
  TASSERT (output.find (id_text) != String::npos);
  const String assertion = capture_abort ([] { JSONIPC_ASSERT_RETURN (false); });
  TASSERT (assertion.find (": assertion failed: false\n") != String::npos);
}
TEST_ADD (jsonipc_diagnostics);

static void
stacktrace_diagnostics()
{
  const String output = capture_abort ([] { fatal_error ("stacktrace diagnostic"); });
  TASSERT (output.find ("stacktrace diagnostic") != String::npos);
  TASSERT (output.find ("Stack Trace") != String::npos || output.find ("Stack trace") != String::npos);
}
TEST_ADD (stacktrace_diagnostics);

} // Anon
