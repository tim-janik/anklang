// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#define HAVE_ZLIB
#define HAVE_ZSTD
#define ZLIB_COMPAT // Use the system zlib.h API and unprefixed symbols.
#include "minizip.h"

int32_t mz_zip_writer_zip_cd (void *handle);

#include "external/minizip-ng/mz_zip.c"
#include "external/minizip-ng/mz_strm.c"
#include "external/minizip-ng/mz_crypt.c" // mz_crypt_crc32_update
#include "external/minizip-ng/mz_zip_rw.c"
#include "external/minizip-ng/mz_strm_buf.c"
#include "external/minizip-ng/mz_strm_mem.c"
#include "external/minizip-ng/mz_strm_zlib.c"
#include "external/minizip-ng/mz_strm_zstd.c"
#include "external/minizip-ng/mz_strm_split.c"
#include "external/minizip-ng/mz_os.c"
#include "external/minizip-ng/mz_os_posix.c"
#include "external/minizip-ng/mz_strm_os_posix.c"
//#include "external/minizip-ng/mz_compat.c"
