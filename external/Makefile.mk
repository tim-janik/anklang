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

# == external/blake3/ ==
blake3/sha := 6b51aefe515969785da02e87befafc7fdc7a065cd3458cf1141f29267749e81f
external/blake3/.sha-$(blake3/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/blake3.tar.gz, $(blake3/sha), \
		https://github.com/BLAKE3-team/BLAKE3/archive/1.8.2/develop.tar.gz)
	$Q rm -rf external/blake3 && mkdir external/blake3
	$Q tar xf external/blake3.tar.gz --strip-components=1 -C external/blake3/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/blake3/.sha-$(blake3/sha)

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

# == external/rapidjson/ ==
rapidjson/sha := 2b521dba5c22eaae6e6e7d4d304cb317e2cf8c687c70046b02792c02f78c127e
external/rapidjson/.sha-$(rapidjson/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/rapidjson.tar.gz, $(rapidjson/sha), \
		https://github.com/Tencent/rapidjson/archive/f9d53419e912910fd8fa57d5705fa41425428c35/develop.tar.gz)
	$Q rm -rf external/rapidjson && mkdir external/rapidjson
	$Q tar xf external/rapidjson.tar.gz --strip-components=1 -C external/rapidjson/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/rapidjson/.sha-$(rapidjson/sha)

# == external/websocketpp/ ==
websocketpp/sha := 6ce889d85ecdc2d8fa07408d6787e7352510750daa66b5ad44aacb47bea76755
external/websocketpp/.sha-$(websocketpp/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/websocketpp.tar.gz, $(websocketpp/sha), \
		https://github.com/zaphoyd/websocketpp/archive/0.8.2/develop.tar.gz)
	$Q rm -rf external/websocketpp && mkdir external/websocketpp
	$Q tar xf external/websocketpp.tar.gz --strip-components=1 -C external/websocketpp/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/websocketpp/.sha-$(websocketpp/sha)

# == external/clean ==
external/clean:
	rm -r -f external/*/
.PHONY: external/clean
clean: external/clean


