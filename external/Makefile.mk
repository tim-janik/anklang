# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

EXTERNAL_CXX_STAMPS :=

# == fetch-and-check ==
# $(call fetch-and-check, FILENAME, SHA256HASH, URL)
define fetch-and-check
( sha256sum -c --status <<<'$(strip $2)  $(strip $1)' 2>/dev/null || \
    curl -# -fSL '$(strip $3)' -o $(strip $1) ) && \
sha256sum -c <<<'$(strip $2)  $(strip $1)' || \
  { echo '$(strip $1): ERROR: failed to fetch: $(strip $3)' >&2 ; exit 1 ; }
endef

# == external/minizip-ng/ ==
minizip-ng/sha := 80d745e1c8caf6f81f6457403b0d9212e8a138b2badd6060e8a5da8583da2551
external/minizip-ng/.sha-$(minizip-ng/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/minizip-ng.tar.gz, $(minizip-ng/sha), \
		https://github.com/zlib-ng/minizip-ng/archive/2.9.0/develop.tar.gz)
	$Q rm -rf external/minizip-ng && mkdir external/minizip-ng
	$Q tar xf external/minizip-ng.tar.gz --strip-components=1 -C external/minizip-ng/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/minizip-ng/.sha-$(minizip-ng/sha) # external/minizip-ng/mz_zip.h

# == external/clean ==
external/clean:
	rm -r -f external/*/
.PHONY: external/clean
clean: external/clean


