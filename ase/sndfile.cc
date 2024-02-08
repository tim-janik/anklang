// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "testing.hh"
#include "internal.hh"

#include "external/libsndfile/include/sndfile.hh"

#define SDEBUG(...)     Ase::debug ("sndfile", __VA_ARGS__)

// Check libsndfile-1.1.0 header features
static_assert (SF_FORMAT_MPEG >= 0x230000, "libsndfile required with MP3 support");

namespace Ase {

} // Ase

// == tests ==
namespace {
using namespace Ase;

TEST_INTEGRITY (sndfile_tests);
static void
sndfile_tests()
{
  char sndfileversion[256] = { 0, };
  sf_command (nullptr, SFC_GET_LIB_VERSION, sndfileversion, sizeof (sndfileversion));
  SDEBUG ("SFC_GET_LIB_VERSION: %s\n", sndfileversion);
}

} // Anon

// Check libsndfile configuration in local build
#include "sndfile/src/config.h"
static_assert (HAVE_EXTERNAL_XIPH_LIBS, "libsndfile requires Ogg/Vorbis and Opus");
static_assert (HAVE_MPEG, "libsndfile requires libmpg123 and libmp3lame");
