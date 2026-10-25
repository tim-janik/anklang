// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/storage.hh>
#include <ase/api.hh>
#include <ase/path.hh>
#include <ase/compress.hh>
#include <ase/minizip.h>
#include <ase/testing.hh>

namespace {
using namespace Ase;

static int
zip_method (const String &archive, const String &member)
{
  void *reader = mz_zip_reader_create();
  TASSERT (mz_zip_reader_open_file (reader, archive.c_str()) == MZ_OK);
  TASSERT (mz_zip_reader_locate_entry (reader, member.c_str(), false) == MZ_OK);
  mz_zip_file *info = nullptr;
  TASSERT (mz_zip_reader_entry_get_info (reader, &info) == MZ_OK);
  const int method = info->compression_method;
  TASSERT (mz_zip_reader_close (reader) == MZ_OK);
  mz_zip_reader_delete (&reader);
  return method;
}

static void
storage_roundtrip()
{
  const String dir = anklang_cachedir_create();
  TASSERT (!dir.empty());
  const String archive = Path::join (dir, "test.anklang");
  const String text = String (4096, 'x') + "\n";
  const String binary ("a\0b", 3);
  const String compressed = zstd_compress (text);
  const String text_file = Path::join (dir, "text");
  const String compressed_file = Path::join (dir, "compressed");
  TASSERT (Path::stringwrite (text_file, text));
  TASSERT (Path::stringwrite (compressed_file, compressed));

  StorageWriter writer;
  TASSERT (writer.open_with_mimetype (archive, "application/x-anklang") == Error::NONE);
  TASSERT (writer.store_file_data ("text", text) == Error::NONE);
  TASSERT (writer.store_file_data ("binary", binary) == Error::NONE);
  TASSERT (writer.store_file_data ("empty", "") == Error::NONE);
  TASSERT (writer.store_file_data ("compressed", compressed) == Error::NONE);
  TASSERT (writer.store_file ("text-file", text_file) == Error::NONE);
  TASSERT (writer.store_file ("compressed-file", compressed_file) == Error::NONE);
  TASSERT (writer.store_file ("text-after-compressed", text_file) == Error::NONE);
  TASSERT (writer.store_file_data ("literal.zst", "plain text") == Error::NONE);
  TASSERT (writer.close() == Error::NONE);

  const String zip = Path::stringread (archive);
  TCMP (zip.substr (0, 4), ==, String ("PK\3\4", 4));
  TCMP (zip.substr (26, 33), ==, String ("\10\0\0\0", 4) + "mimetypeapplication/x-anklang");
  TCMP (zip_method (archive, "mimetype"), ==, 0);
  for (const String member : { "text", "binary", "empty", "text-file", "text-after-compressed", "literal.zst" })
    TCMP (zip_method (archive, member), ==, 93);
  for (const String member : { "compressed", "compressed-file" })
    TCMP (zip_method (archive, member), ==, 0);

  StorageReader reader;
  TASSERT (reader.open_for_reading (archive) == Error::NONE);
  TASSERT (reader.has_file ("text"));
  TASSERT (!reader.has_file ("text.zst"));
  TASSERT (!reader.has_file ("literal"));
  TCMP (reader.stringread ("literal.zst"), ==, "plain text");
  TCMP (reader.stringread ("literal"), ==, "");
  TASSERT (errno == ENOENT);
  TCMP (reader.stringread ("mimetype"), ==, "application/x-anklang");
  TCMP (reader.stringread ("text"), ==, text);
  TCMP (reader.stringread ("text", 7), ==, text.substr (0, 7));
  TCMP (reader.stringread ("binary"), ==, binary);
  TCMP (reader.stringread ("empty"), ==, "");
  TASSERT (errno == 0);
  TCMP (reader.stringread ("compressed"), ==, compressed);
  TCMP (reader.stringread ("text-file"), ==, text);
  TCMP (reader.stringread ("compressed-file"), ==, compressed);
  TASSERT (reader.close() == Error::NONE);

  auto stream = stream_reader_zip_member (archive, "text");
  TASSERT (stream);
  String streamed;
  char buffer[31];
  ssize_t count;
  while ((count = stream->read (buffer, sizeof (buffer))) > 0)
    streamed.append (buffer, count);
  TASSERT (count == 0);
  TCMP (streamed, ==, text);
  TASSERT (stream->close());
  TASSERT (!stream_reader_zip_member (archive, "literal"));
  anklang_cachedir_cleanup (dir);
}
TEST_ADD (storage_roundtrip);

}
