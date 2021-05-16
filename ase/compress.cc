// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "compress.hh"
#include "utils.hh"
#include "internal.hh"

#if !__has_include(<zstd.h>)
#error "Missing <zstd.h> from libzstd-dev, please set CXXFLAGS and LDFLAGS"
#endif
#include <zstd.h>

namespace Ase {

bool
is_aiff (const String &input)
{
  return input.substr (0, 4) == "FORM" && input.substr (8, 4) == "AIFF";
}

bool
is_wav (const String &input)
{
  return input.substr (0, 4) == "RIFF" && input.substr (8, 4) == "WAVE";
}

bool
is_midi (const String &input)
{
  return input.substr (0, 4) == "MThd";
}

bool
is_pdf (const String &input)
{
  return input.substr (0, 5) == "%PDF-";
}

static int
guess_zstd_level (size_t input_size)
{
  const size_t MB = 1024 * 1024;
  if (input_size <= 8 * MB)
    return 17;          // slow, but OK for small sizes
  if (input_size <= 32 * MB)
    return 10;          // starts to get fairly slow
  return 4;             // acceptable fast compression
}

String
zstd_compress (const String &input)
{
  const size_t maxosize = ZSTD_compressBound (input.size());
  String data;
  data.resize (maxosize);
  const size_t osize = ZSTD_compress (&data[0], data.size(), &input[0], input.size(), guess_zstd_level (input.size()));
  if (ZSTD_isError (osize))
    {
      warning ("zstd compression failed (input=%zu): %s", input.size(), ZSTD_getErrorName (osize));
      return "";
    }
  data.resize (osize);
  data.reserve (data.size());
  return data;
}

String
zstd_uncompress (const String &input)
{
  const size_t maxosize = ZSTD_getFrameContentSize (&input[0], input.size());
  if (maxosize > 8 * 1024 * 1024 * 1024ull)
    {
      String err;
      switch (maxosize)
        {
        case ZSTD_CONTENTSIZE_ERROR:    err = "invalid zstd header"; break;
        case ZSTD_CONTENTSIZE_UNKNOWN:  err = "unknown zstd data size"; break;
        default:                        err = string_format ("uncompress limit exceeded (%u)", maxosize); break;
        }
      warning ("zstd decompression failed (input=%zu): %s", input.size(), err);
      return "";
    }
  String data;
  data.resize (maxosize);
  const size_t osize = ZSTD_decompress (&data[0], data.size(), &input[0], input.size());
  if (ZSTD_isError (osize))
    {
      warning ("zstd decompression failed (input=%zu): %s", input.size(), ZSTD_getErrorName (osize));
      return "";
    }
  data.resize (osize);
  data.reserve (data.size());
  assert_return (osize == maxosize, data); // paranoid, esnured by zstd
  return data;
}

bool
is_zstd (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == char (0x28) &&
          input[1] == char (0xb5) &&
          input[2] == char (0x2f) &&
          input[3] == char (0xfd));
}

bool
is_lz4 (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == char (0x04) &&
          input[1] == char (0x22) &&
          input[2] == char (0x4d) &&
          input[3] == char (0x18));
}

bool
is_zip (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == 'P' && input[1] == 'K' &&
          ((input[2] == 0x03 && input[3] == 0x04) ||
           (input[2] == 0x05 && input[3] == 0x06) ||
           (input[2] == 0x07 && input[3] == 0x08)));
}

bool
is_arj (const String &input)
{
  return input.size() >= 2 && input[0] == char (0x60) && input[1] == char (0xea);
}

bool
is_isz (const String &input)
{
  return input.substr (0, 4) == "IsZ!";
}

bool
is_ogg (const String &input)
{
  return input.substr (0, 4) == "OggS";
}

bool
is_avi (const String &input)
{
  return input.substr (0, 4) == "RIFF" && input.substr (8, 4) == "AVI ";
}

bool
is_gz (const String &input)
{
  return input.size() >= 2 && input[0] == char (0x1f) && input[1] == char (0x8B);
}

bool
is_xz (const String &input)
{
  return (input.size() >= 6 &&
          input[0] == char (0xfd) &&
          input[1] == char (0x37) &&
          input[2] == char (0x7a) &&
          input[3] == char (0x58) &&
          input[4] == char (0x5a) &&
          input[5] == char (0x00));
}

bool
is_png (const String &input)
{
  return (input.size() >= 8 &&
          input[0] == char (0x89) &&
          input.substr (1, 3) == "PNG" &&
          input[4] == char (0x0d) &&
          input[5] == char (0x0a) &&
          input[6] == char (0x1a) &&
          input[7] == char (0x0a));
}

bool
is_jpg (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == char (0xff) &&
          input[1] == char (0xd8) &&
          input[2] == char (0xff) &&
          (input[3] == char (0xdb) || input[3] == char (0xe0) ||
           input[3] == char (0xee) || input[3] == char (0xe1)));
}

bool
is_compressed (const String &input)
{
  return (is_zstd (input) ||
          is_lz4 (input) ||
          is_zip (input) ||
          is_arj (input) ||
          is_isz (input) ||
          is_ogg (input) ||
          is_avi (input) ||
          is_gz (input) ||
          is_xz (input) ||
          is_png (input) ||
          is_jpg (input));

}

} // Ase
