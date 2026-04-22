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

# == external/choc/ ==
choc/version := 20240625.114431-0-g426c7ae
choc/sha := 3664bc1ff8268271ff69f19dca6355dd53bf069c02a30698ac12e2132d66d31d
external/choc/.sha-$(choc/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/choc.tar.gz, $(choc/sha), \
		https://github.com/Tracktion/choc/archive/426c7ae538f8a4709bb381c6727a83812c262962/develop.tar.gz)
	$Q rm -rf external/choc && mkdir external/choc
	$Q tar xf external/choc.tar.gz --strip-components=1 -C external/choc/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/choc/.sha-$(choc/sha)

# == external/cpptrace/ ==
cpptrace/sha := 5c9f5b301e903714a4d01f1057b9543fa540f7bfcc5e3f8bd1748e652e24f9ea
external/cpptrace/.sha-$(cpptrace/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/cpptrace.tar.gz, $(cpptrace/sha), \
		https://github.com/jeremy-rifkin/cpptrace/archive/refs/tags/v1.0.4.tar.gz)
	$Q rm -rf external/cpptrace && mkdir external/cpptrace
	$Q tar xf external/cpptrace.tar.gz --strip-components=1 -C external/cpptrace/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/cpptrace/.sha-$(cpptrace/sha)

# == external/crill/ ==
crill/version := 20230208.142844-0-gbedcf27
crill/sha := 7f54d046fbf1839c68a4de0d07886b977527867cb6c89cb7d2c0f0cdcc9c89e4
external/crill/.sha-$(crill/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/crill.tar.gz, $(crill/sha), \
		https://github.com/crill-dev/crill/archive/bedcf278625ffbe6ebfcd2d71aed20d78fd838ce/develop.tar.gz)
	$Q rm -rf external/crill && mkdir external/crill
	$Q tar xf external/crill.tar.gz --strip-components=1 -C external/crill/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/crill/.sha-$(crill/sha)

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

# == external/expected/ ==
expected/version := v1.1.0
expected/sha := fe3b18aecb849029b6af94922be0c25eee1b7b86565b1c8350692ed776cf42fb
external/expected/.sha-$(expected/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/expected.tar.gz, $(expected/sha), \
		https://github.com/TartanLlama/expected/archive/292eff8bd8ee230a7df1d6a1c00c4ea0eb2f0362/develop.tar.gz)
	$Q rm -rf external/expected && mkdir external/expected
	$Q tar xf external/expected.tar.gz --strip-components=1 -C external/expected/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/expected/.sha-$(expected/sha)

# == external/mpmcqueue/ ==
mpmcqueue/version := v1.0-8-gb9808ed
mpmcqueue/sha := bdfcf2429aebe892eb7b4a2edbc2d889365fa52264bf9501d937e8484166ec6a
external/mpmcqueue/.sha-$(mpmcqueue/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/mpmcqueue.tar.gz, $(mpmcqueue/sha), \
		https://github.com/rigtorp/MPMCQueue/archive/b9808ede08f26fa9df4df4e081d19cace8f6c6ea/develop.tar.gz)
	$Q rm -rf external/mpmcqueue && mkdir external/mpmcqueue
	$Q tar xf external/mpmcqueue.tar.gz --strip-components=1 -C external/mpmcqueue/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/mpmcqueue/.sha-$(mpmcqueue/sha)

# == external/nanorange/ ==
nanorange/version := 20200706.105018-0-gbf32251
nanorange/sha := 28cf187174b3097c00aa12cc2a3b554f8f768a3b50a9703174af03ee99c3397c
external/nanorange/.sha-$(nanorange/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/nanorange.tar.gz, $(nanorange/sha), \
		https://github.com/tcbrindle/NanoRange/archive/bf32251d65673fe170d602777c087786c529ead8/develop.tar.gz)
	$Q rm -rf external/nanorange && mkdir external/nanorange
	$Q tar xf external/nanorange.tar.gz --strip-components=1 -C external/nanorange/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/nanorange/.sha-$(nanorange/sha)

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

# == external/libsamplerate/ ==
libsamplerate/version := 0.2.2-18-g15c392d
libsamplerate/sha := 2172280f3427504571a7888f28dc02f276ce6b3ccb57d0c74094f87709a7b237
external/libsamplerate/.sha-$(libsamplerate/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/libsamplerate.tar.gz, $(libsamplerate/sha), \
		https://github.com/libsndfile/libsamplerate/archive/15c392d47e71b9395a759544b3818a1235fe1a1d/develop.tar.gz)
	$Q rm -rf external/libsamplerate && mkdir external/libsamplerate
	$Q tar xf external/libsamplerate.tar.gz --strip-components=1 -C external/libsamplerate/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/libsamplerate/.sha-$(libsamplerate/sha)

# == external/liquidsfz/ ==
liquidsfz/sha := fe779175ee9d992988dfdf94f0a8dd3f4dae69102d221262a5bc98f98f0528e0
external/liquidsfz/.sha-$(liquidsfz/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/liquidsfz.tar.gz, $(liquidsfz/sha), \
		https://github.com/swesterfeld/liquidsfz/archive/c695cddcf7626d868fa1503712172b117108ef04/develop.tar.gz)
	$Q rm -rf external/liquidsfz && mkdir external/liquidsfz
	$Q tar xf external/liquidsfz.tar.gz --strip-components=1 -C external/liquidsfz/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/liquidsfz/.sha-$(liquidsfz/sha)

# == external/magic_enum/ ==
magic_enum/version := v0.9.7
magic_enum/sha := a06b11989a7802de62a405c373b23c3fcc4caca27e0a0df5f9905bc322a14a40
external/magic_enum/.sha-$(magic_enum/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/magic_enum.tar.gz, $(magic_enum/sha), \
		https://github.com/Neargye/magic_enum/archive/e046b69a3736d314fad813e159b1c192eaef92cd/develop.tar.gz)
	$Q rm -rf external/magic_enum && mkdir external/magic_enum
	$Q tar xf external/magic_enum.tar.gz --strip-components=1 -C external/magic_enum/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/magic_enum/.sha-$(magic_enum/sha)

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

# == external/soundtouch/ ==
soundtouch/version := 2.4.0
soundtouch/sha := 3dda3c9ab1e287f15028c010a66ab7145fa855dfa62763538f341e70b4d10abd
external/soundtouch/.sha-$(soundtouch/sha):
	$(QGEN)
	$Q $(call fetch-and-check, external/soundtouch.tar.gz, $(soundtouch/sha), \
		https://github.com/tim-janik/blobs4anklang/releases/download/soundtouch-v2.4.0/soundtouch-2.4.0.tar.gz)
	$Q rm -rf external/soundtouch && mkdir external/soundtouch
	$Q tar xf external/soundtouch.tar.gz --strip-components=1 -C external/soundtouch/
	$Q touch $@
EXTERNAL_CXX_STAMPS += external/soundtouch/.sha-$(soundtouch/sha)

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

# == fonts ==
external/font_files :=
external/font_list  := \
	693b77d4f32ee9b8bfc995589b5fad5e99adf2832738661f5402f9978429a8e3 external/Inter_VF.woff2 \
	https://raw.githubusercontent.com/tim-janik/blobs4anklang/refs/heads/trunk/fonts/InterVF-4.1.woff2 \
	145e9fc086d13403528384bdace7f2a4d5ecef72a2b10a749e99382dbecfce79 external/Recursive_VF.woff2 \
	https://raw.githubusercontent.com/arrowtype/recursive/refs/heads/main/fonts/ArrowType-Recursive-1.085/Recursive_Web/woff2_variable/Recursive_VF_1.085.woff2 \
	b2d31b687a774116f95f2c46324e446acce7e4b7bdb94c34bdf1e97c9829cdb0 external/MonaspaceKrypton_NF.woff2 \
	https://raw.githubusercontent.com/githubnext/monaspace/refs/heads/main/fonts/Web%20Fonts/NerdFonts%20Web%20Fonts/Monaspace%20Krypton/MonaspaceKryptonNF-Medium.woff2 \
	1f0fae614197cac4175537dd02eabdd535ca3beaab652a36bd81534f664c0815 external/nerd-fonts-3.4.0.css \
	https://github.com/tim-janik/blobs4anklang/raw/8f357c02c8412555f1bd3ffda38721d284aaa9d9/fonts/nerd-fonts-3.4.0.css \
	3c8f81b3e44b6e8ee4dc5cd928544124fc9e28580cd280dae343bbf54e66b2d8 external/nerd-fonts-3.4.0.woff2 \
	https://github.com/tim-janik/blobs4anklang/raw/8f357c02c8412555f1bd3ffda38721d284aaa9d9/fonts/nerd-fonts-3.4.0.woff2 \
	eea00c02f4c00f09fd50e80efdb8341975f9eb8c82720ce50f44c19a962996c5 external/MonaspaceXenon_VF.woff2 \
	https://raw.githubusercontent.com/githubnext/monaspace/refs/heads/main/fonts/Web%20Fonts/Variable%20Web%20Fonts/Monaspace%20Xenon/Monaspace%20Xenon%20Var.woff2
external/font_files_add = $(eval external/font_files += $2)	# build external/font_files
$(call triplewise, external/font_files_add, $(external/font_list))
external/font_fetch_and_check = && { $(call fetch-and-check,$2,$1,$3) ; }
external/fonts/.done: external/Makefile.mk
	$(QGEN)
	$Q rm -rf external/fonts/ && mkdir external/fonts/
	$Q : $(call triplewise, external/font_fetch_and_check, $(external/font_list))
	$Q touch $@
EXTERNAL_BLOBS4ANKLANG_STAMPS += external/fonts/.done

# == external/clean ==
external/clean:
	rm -r -f external/*/
.PHONY: external/clean
clean: external/clean
$(EXTERNAL_CXX_STAMPS): external/Makefile.mk

