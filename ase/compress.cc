// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "compress.hh"
#include "utils.hh"
#include "internal.hh"

#if !__has_include(<zstd.h>)
#error "Missing <zstd.h> from libzstd-dev, please set CXXFLAGS and LDFLAGS"
#endif
#include <zstd.h>

namespace Ase {

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
compress (const String &input)
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
uncompress (const String &input)
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
is_compressed (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == char (0x28) &&
          input[1] == char (0xb5) &&
          input[2] == char (0x2f) &&
          input[3] == char (0xfd));
}

} // Ase
