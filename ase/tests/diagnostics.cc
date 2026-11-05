// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/atquit.hh>
#include <ase/testing.hh>
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
  const String program = "/nonexistent/anklang-missing-program";
  pid_t child = 0;
  int status = 0;
  const String output = capture_stderr ([&] {
    const ErrorReason error = spawn_process ({ program }, &child);
    TASSERT (error.error == 0);
    TASSERT (waitpid (child, &status, 0) == child);
  });
  TASSERT (WIFEXITED (status) && WEXITSTATUS (status) == 127);
  const String expected = "[pid=" + std::to_string (child) + "]: fork to exec " + program +
                          ": execvp: errno=" + std::to_string (ENOENT) + "\n";
  TASSERT (output.find (expected) != String::npos);
}
TEST_ADD (spawn_failure_diagnostics);

} // Anon
