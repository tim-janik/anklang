// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MINIZIP_H__
#define __ASE_MINIZIP_H__

#include <stdint.h>

// Configuration
#define MZ_ZIP_NO_ENCRYPTION

// Minizip API
#include "external/minizip-ng/mz.h"
#include "external/minizip-ng/mz_os.h"
#include "external/minizip-ng/mz_zip.h"
#include "external/minizip-ng/mz_strm.h"
//#include "external/minizip-ng/mz_crypt.h"
#include "external/minizip-ng/mz_strm_buf.h"
#include "external/minizip-ng/mz_strm_mem.h"
#include "external/minizip-ng/mz_strm_zlib.h"
#include "external/minizip-ng/mz_strm_split.h"
#include "external/minizip-ng/mz_strm_os.h"
#include "external/minizip-ng/mz_zip_rw.h"
//#include "external/minizip-ng/mz_compat.h"

#endif // __ASE_MINIZIP_H__
