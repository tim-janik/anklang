// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/api.hh>
#include <ase/storage.hh>
#include <ase/path.hh>
#include <ase/testing.hh>
#include <glib.h>

namespace {
using namespace Ase;

static String
command_output (StringS args)
{
  std::vector<char*> argv;
  for (auto &arg : args)
    argv.push_back (arg.data());
  argv.push_back (nullptr);
  char *output = nullptr;
  int status = 0;
  TASSERT (g_spawn_sync (nullptr, argv.data(), nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, &output, nullptr, &status, nullptr));
  const String result = output ? output : "";
  g_free (output);
  TASSERT (status == 0);
  return result;
}

static void
storage_mimetype()
{
  const String dir = anklang_cachedir_create();
  TASSERT (!dir.empty());
  const String mime_dir = Path::join (dir, "mime");
  TASSERT (Path::mkdirs (Path::join (mime_dir, "packages")));
  TASSERT (Path::copy_file ("misc/anklang-mime.xml", Path::join (mime_dir, "packages/anklang.xml")));
  command_output ({ "env", "XDG_DATA_HOME=" + dir, "update-mime-database", mime_dir });

  for (const bool anklang : { true, false })
    {
      const String archive = Path::join (dir, anklang ? "project" : "other");
      StorageWriter writer;
      TASSERT (writer.open_with_mimetype (archive, anklang ? "application/x-anklang" : "application/zip") == Error::NONE);
      TASSERT (writer.store_file_data ("text", "test data") == Error::NONE);
      TASSERT (writer.close() == Error::NONE);
      const String output = command_output ({ "env", "XDG_DATA_HOME=" + dir, "gio", "info", "-a", "standard::content-type", archive });
      TASSERT ((output.find ("standard::content-type: application/x-anklang") != String::npos) == anklang);
    }
  anklang_cachedir_cleanup (dir);
}
TEST_ADD (storage_mimetype);

}
