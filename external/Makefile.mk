# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

EXTERNAL_BLOBS4ANKLANG_STAMPS :=
EXTERNAL_CXX_STAMPS :=

# == fetch-and-check ==
# $(call fetch-and-check, FILENAME, SHA256HASH, URL)
define fetch-and-check
( sha256sum -c --status <<<'$(strip $2)  $(strip $1)' 2>/dev/null || \
    curl --retry 2 -# -fSL '$(strip $3)' -o $(strip $1) ) && \
sha256sum -c <<<'$(strip $2)  $(strip $1)' || \
  { sha256sum "$(strip $1)"; echo '$(strip $1): ERROR: failed to fetch: $(strip $3)' >&2 ; exit 1 ; }
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

# == external/blobs4anklang/ ==
blobs4anklang/sha := 855de5461002326a47ecf85b51e3a7102e3a90e523a4424f5c8e13660c3eee09
external/blobs4anklang/.sha-$(blobs4anklang/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/blobs4anklang.tar.gz, $(blobs4anklang/sha), \
		https://github.com/tim-janik/blobs4anklang/archive/7b0a4a68a1e9efbe68fc9761bef080995f4b4d6b/develop.tar.gz)
	$Q rm -rf external/blobs4anklang && mkdir external/blobs4anklang
	$Q tar xf external/blobs4anklang.tar.gz --strip-components=1 -C external/blobs4anklang/
	$Q touch $@
EXTERNAL_BLOBS4ANKLANG_STAMPS += external/blobs4anklang/.sha-$(blobs4anklang/sha)

# == external/clap/ ==
clap/sha := eef67a38df6c20fd4cb79698772d35d30aefc2e1a8d5275a5169f58cd530333e
external/clap/.sha-$(clap/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/clap.tar.gz, $(clap/sha), \
		https://github.com/free-audio/clap/archive/1.1.1/develop.tar.gz)
	$Q rm -rf external/clap && mkdir external/clap
	$Q tar xf external/clap.tar.gz --strip-components=1 -C external/clap/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/clap/.sha-$(clap/sha)

# == external/nlohmann-json/ ==
nlohmann-json/sha := 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
external/nlohmann-json/.sha-$(nlohmann-json/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/nlohmann-json.tar.gz, $(nlohmann-json/sha), \
		https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
	$Q rm -rf external/nlohmann-json && mkdir external/nlohmann-json
	$Q tar xf external/nlohmann-json.tar.gz --strip-components=1 -C external/nlohmann-json/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/nlohmann-json/.sha-$(nlohmann-json/sha)

# == external/libsndfile/ ==
libsndfile/version := 1.2.2-51-g52b803f5
libsndfile/lt_current.lt_age.lt_revision := 1.0.37
libsndfile/sha := edb98c8fdba810768a95ba99b9c8f5fe38451ab35f3329564e0ff85b16d40220
external/libsndfile/.sha-$(libsndfile/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/libsndfile.tar.gz, $(libsndfile/sha), \
		https://github.com/libsndfile/libsndfile/archive/52b803f57a1f4d23471f5c5f77e1a21e0721ea0e/develop.tar.gz)
	$Q rm -rf external/libsndfile && mkdir external/libsndfile
	$Q tar xf external/libsndfile.tar.gz --strip-components=1 -C external/libsndfile/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/libsndfile/.sha-$(libsndfile/sha)

# == external/liquidsfz/ ==
liquidsfz/sha := 7718fbf707100b87dbfd3987e4a1b75d12e65685f0f6cf88573d00032459f8fc
external/liquidsfz/.sha-$(liquidsfz/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/liquidsfz.tar.gz, $(liquidsfz/sha), \
		https://github.com/swesterfeld/liquidsfz/archive/590149ea8c83588c17833d7b9b6653f0f6aab6fb/develop.tar.gz)
	$Q rm -rf external/liquidsfz && mkdir external/liquidsfz
	$Q tar xf external/liquidsfz.tar.gz --strip-components=1 -C external/liquidsfz/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/liquidsfz/.sha-$(liquidsfz/sha)

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

# == external/freepats-vorbis/ ==
freepats-vorbis/sha := 70ebb56aec0ac41def988f25be451c2836a95980c00c46b46da90b975f5a6b28
external/freepats-vorbis/.sha-$(freepats-vorbis/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/freepats-vorbis.tar.gz, $(freepats-vorbis/sha), \
		https://github.com/tim-janik/blobs4anklang/releases/download/v25.10.0/freepats-vorbis-25.10.0.tar.zst)
	$Q rm -rf external/freepats-vorbis && mkdir external/freepats-vorbis
	$Q tar xf external/freepats-vorbis.tar.gz --strip-components=1 -C external/freepats-vorbis/
	$Q touch $@
EXTERNAL_BLOBS4ANKLANG_STAMPS += external/freepats-vorbis/.sha-$(freepats-vorbis/sha)

# == external/pandaresampler/ ==
pandaresampler/sha := cda9d81463b1b30e3c835bc2725d20685236b30081423e286262e7937318a565
external/pandaresampler/.sha-$(pandaresampler/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/pandaresampler.tar.gz, $(pandaresampler/sha), \
		https://github.com/swesterfeld/pandaresampler/archive/0.2.1/develop.tar.gz)
	$Q rm -rf external/pandaresampler && mkdir external/pandaresampler
	$Q tar xf external/pandaresampler.tar.gz --strip-components=1 -C external/pandaresampler/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/pandaresampler/.sha-$(pandaresampler/sha)

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
$(EXTERNAL_CXX_STAMPS): external/Makefile.mk

