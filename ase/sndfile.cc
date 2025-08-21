// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "testing.hh"
#include "internal.hh"

#include "external/libsndfile/include/sndfile.hh"

#include "libsndfile/config.h"

#define SDEBUG(...)     Ase::debug ("sndfile", __VA_ARGS__)

// Check libsndfile configuration in local build
#ifndef HAVE_EXTERNAL_XIPH_LIBS
#error "libsndfile requires Flac, Ogg, Vorbis, Opus headers"
#endif
#ifndef HAVE_MPEG
#error "libsndfile requires libmpg123 and libmp3lame"
#endif

// Check libsndfile-1.1.0 header features
static_assert (SF_FORMAT_MPEG >= 0x230000, "libsndfile required with MP3 support");

namespace Ase {

String
libsndfile_version ()
{
  char sndfileversion[256] = { 0, };
  sf_command (nullptr, SFC_GET_LIB_VERSION, sndfileversion, sizeof (sndfileversion));
  return sndfileversion;
}

} // Ase

// == tests ==
namespace {
using namespace Ase;

TEST_INTEGRITY (sndfile_tests);
static void
sndfile_tests()
{
  const String sndfileversion = libsndfile_version();
  SDEBUG ("SFC_GET_LIB_VERSION: %s\n", sndfileversion);
}

} // Anon

