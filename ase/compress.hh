// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_COMPRESS_HH__
#define __ASE_COMPRESS_HH__

#include <ase/defs.hh>

namespace Ase {

bool   is_aiff         (const String &input);
bool   is_midi         (const String &input);
bool   is_pdf          (const String &input);
bool   is_wav          (const String &input);

bool   is_compressed   (const String &input);

bool   is_arj          (const String &input);
bool   is_avi          (const String &input);
bool   is_gz           (const String &input);
bool   is_isz          (const String &input);
bool   is_jpg          (const String &input);
bool   is_lz4          (const String &input);
bool   is_ogg          (const String &input);
bool   is_png          (const String &input);
bool   is_xz           (const String &input);
bool   is_zip          (const String &input);

bool   is_zstd         (const String &input);
String zstd_compress   (const String &input);
String zstd_uncompress (const String &input);

StreamWriterP stream_writer_zstd (const StreamWriterP &ostream, int level = 9);
StreamReaderP stream_reader_zstd (StreamReaderP &istream);

} // Ase

#endif  // __ASE_COMPRESS_HH__
