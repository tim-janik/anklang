# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ase/*.d $>/ase/tests/*.d)

# == ase/ *.cc file sets ==
lib/libsndfile.so		:= $>/lib/libsndfile.so.$(libsndfile/lt_current.lt_age.lt_revision)
ase/jackdriver.sources		:= ase/driver-jack.cc
ase/gtk2wrap.sources		:= ase/gtk2wrap.cc
ase/not-anklang.sources		:= $(ase/gtk2wrap.sources) $(ase/jackdriver.sources)
ase/anklang.sources		:= $(filter-out $(ase/not-anklang.sources), $(wildcard ase/*.cc)) $(wildcard ase/*.c ase/tests/*.cc)
lib/AnklangSynthEngine		:= $>/lib/AnklangSynthEngine
ase/generated.sources		:=
ASE_EXTRA_INCLUDES := $(strip			\
	-Itrkn				\
	-Iexternal				\
	-Iexternal/clap/include			\
	-Iexternal/crill/include		\
	-Iexternal/libsndfile/include		\
	-Iexternal/libsamplerate/include	\
	-Iexternal/liquidsfz/lib		\
	-Iexternal/nlohmann-json/include	\
	-Iexternal/pandaresampler/include	\
	-Iexternal/rapidjson/include		\
	-Iexternal/websocketpp			\
	$(ASEDEPS_CFLAGS)			\
) # also used by clang-tidy
PROMPT := prompt() ( test "$${CODEGEN-}" == y && return 0; read -p "$$1 [y/N] " A && test "$$A" == y ; ) && prompt

# == ase/sysconfig.h ==
$>/ase/sysconfig.h: $(config-stamps)			| $>/ase/tests/ # ase/Makefile.mk
	$(QGEN)
	$Q : $(file > $>/ase/conftest_sysconfigh.cc, $(ase/conftest_sysconfigh.cc)) \
	&& $(CXX) -Wall $>/ase/conftest_sysconfigh.cc -pthread -o $>/ase/conftest_sysconfigh \
	&& (cd $> && ./ase/conftest_sysconfigh)
	$Q echo '// make $@'				> $@.tmp
	$Q cat $>/ase/conftest_sysconfigh.txt		>>$@.tmp
	$Q mv $@.tmp $@
# ase/conftest_sysconfigh.cc
define ase/conftest_sysconfigh.cc
// #define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
struct Spin { pthread_spinlock_t dummy1, s1, dummy2, s2, dummy3; };
int main (int argc, const char *argv[]) {
  FILE *f = fopen ("ase/conftest_sysconfigh.txt", "w");
  assert (f);
  struct Spin spin;
  memset (&spin, 0xffffffff, sizeof (spin));
  if (pthread_spin_init (&spin.s1, 0) == 0 && pthread_spin_init (&spin.s2, 0) == 0 &&
      sizeof (pthread_spinlock_t) == 4 && spin.s1 == spin.s2)
    { // # sizeof==4 and location-independence are current implementation assumption
      fprintf (f, "#define ASE_SPINLOCK_INITIALIZER  0x%04x \n", *(int*) &spin.s1);
    }
  fprintf (f, "#define ASE_SYSVAL_POLLINIT  ((const uint32_t[]) ");
  fprintf (f, "{ 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x } )\n",
           POLLIN, POLLPRI, POLLOUT, POLLRDNORM, POLLRDBAND, POLLWRNORM, POLLWRBAND, POLLERR, POLLHUP, POLLNVAL);
  return ferror (f) || fclose (f) != 0;
}
endef

# == $>/ase/api-jsonipc.json ==
$>/ase/api-jsonipc.json: ase/api.hh $>/ase/sysconfig.h ase/Makefile.mk
	$(QGEN)
	$Q CLANG=clang-20; command -v $$CLANG >/dev/null || CLANG=clang-21; \
		$$CLANG -std=gnu++23 -I . -I out/ -extract-api $< -o $@
$>/ase/api-jsonipc.pretty.json: $>/ase/api-jsonipc.json ase/Makefile.mk
	$(QGEN)		# JSON formatted for human inspection
	$Q python3 -m json.tool < $< > $@
ALL_TARGETS += $>/ase/api-jsonipc.pretty.json

# == check-jsonipc/jsonbindings.ts ==
check-jsonipc/jsonbindings.ts: jsonipc/jsonbindings.ts			| node_modules/.npm.done
	$(QGEN)
	$Q node_modules/.bin/tsc --noEmit --allowJs --moduleResolution bundler -m esnext --target esnext --erasableSyntaxOnly $<
.PHONY: check-jsonipc/jsonbindings.ts
check: check-jsonipc/jsonbindings.ts

# == ase/gen/api-jsonipc.g.cc ==
ase/gen/api-jsonipc.g.cc: $>/ase/api-jsonipc.json jsonipc/jsonbindings.ts ase/Makefile.mk
	$(QECHO) REGEN $@
	$Q echo '// Generated from: $(notdir $<)'					>  $@.tmp
	$Q echo '#include <ase/jsonapi.hh>'						>> $@.tmp
	$Q echo '#include <ase/api.hh>'							>> $@.tmp
	$Q $(RUNTS) jsonipc/jsonbindings.ts --cxx $<					>> $@.tmp
	$Q echo '[[maybe_unused]] static bool init_jsonipc = [] {'			>> $@.tmp
	$Q echo '  if (getenv ("ASE_JSONTS"))'						>> $@.tmp
	$Q echo '    Jsonipc::g_binding_printer = new Jsonipc::BindingPrinter();'	>> $@.tmp
	$Q echo '  jsonipc_for_api_jsonipc_json();'					>> $@.tmp
	$Q echo '  return 0;'								>> $@.tmp
	$Q echo '} ();'									>> $@.tmp
	$Q cmp -s $@ $@.tmp || { git -P diff --no-index -- $@ $@.tmp ; echo "  UPDATING" $@ ; }
	$Q mv $@.tmp $@

# == ase/gen/api-jsonipc.g.ts ==
ase/gen/api-jsonipc.g.ts: ase/api.hh jsonipc/jsonipc.ts ase/Makefile.mk $(lib/AnklangSynthEngine)
	$(QECHO) REGEN $@
	$Q echo '// Generated from: $(notdir $<)'					>  $@.tmp
	$Q cat jsonipc/jsonipc.ts							>> $@.tmp
	$Q echo 'export class SharedBase // Ase::SharedBase'				>> $@.tmp
	$Q echo '  extends Jsonipc.Jsonipc_prototype {'					>> $@.tmp
	$Q echo '  constructor ($id) { super ($id);'					>> $@.tmp
	$Q echo ' if (new.target === SharedBase) Jsonipc.ofreeze (this); }'		>> $@.tmp
	$Q echo '};'									>> $@.tmp
	$Q echo 'Jsonipc.classes["Ase::SharedBase"] = SharedBase;'			>> $@.tmp
	$Q echo										>> $@.tmp
	$Q ASAN_OPTIONS=detect_leaks=0 ASE_JSONTS=1 \
	$(lib/AnklangSynthEngine) --norc --no-devices --jsonts				>> $@.tmp
	$Q echo '/**@type{ServerImpl}*/'						>> $@.tmp
	$Q echo -n 'export let server: Server ='				>> $@.tmp
	$Q echo 'Jsonipc.setup_promise_type (Server, s => server = s) as unknown as Server;'	>> $@.tmp
	$Q cmp -s $@ $@.tmp || { git -P diff --no-index -- $@ $@.tmp ; echo "  UPDATING" $@ ; }
	$Q mv $@.tmp $@
check-ase/gen/api-jsonipc.g.ts: ase/gen/api-jsonipc.g.ts		| node_modules/.npm.done
	$(QGEN)
	$Q node_modules/.bin/tsc --noEmit --allowJs --moduleResolution bundler -m esnext --target esnext --erasableSyntaxOnly $<
PHONY: check-ase/gen/api-jsonipc.g.ts
check: check-ase/gen/api-jsonipc.g.ts

# == ase/gen/class-tree.g.md ==
ase/gen/class-tree.g.md: $>/ase/api-jsonipc.json jsonipc/jsonbindings.ts ase/Makefile.mk
	$(QECHO) REGEN $@
	$Q $(RUNTS) jsonipc/jsonbindings.ts --class-tree $<				>  $@.tmp
	$Q cmp -s $@ $@.tmp || { git -P diff --no-index -- $@ $@.tmp ; echo "  UPDATING" $@ ; }
	$Q mv $@.tmp $@
# for doc/Makefile.mk

# == blake3impl.c ==
$>/ase/blake3impl.c:		| $>/ase/
	$(QGEN)
	$Q echo -e '#ifdef __AVX512F__\n' ' #include "external/blake3/c/blake3_avx512.c"\n' '#endif' > $>/ase/blake3avx512.c
	$Q echo -e '#ifdef __AVX2__\n'    ' #include "external/blake3/c/blake3_avx2.c"\n' '#endif'   > $>/ase/blake3avx2.c
	$Q echo -e '#ifdef __SSE4_1__\n'  ' #include "external/blake3/c/blake3_sse41.c"\n' '#endif'  > $>/ase/blake3sse41.c
	$Q echo -e '#ifdef __SSE2__\n'    ' #include "external/blake3/c/blake3_sse2.c"\n' '#endif'   > $>/ase/blake3sse2.c
	$Q echo -e '#ifndef __AVX512F__\n' ' #define BLAKE3_NO_AVX512\n' '#endif' >  $>/ase/blake3impl.c
	$Q echo -e '#ifndef __AVX2__\n'    ' #define BLAKE3_NO_AVX2\n'   '#endif' >> $>/ase/blake3impl.c
	$Q echo -e '#ifndef __SSE4_1__\n'  ' #define BLAKE3_NO_SSE41\n'  '#endif' >> $>/ase/blake3impl.c
	$Q echo -e '#ifndef __SSE2__\n'    ' #define BLAKE3_NO_SSE2\n'   '#endif' >> $>/ase/blake3impl.c
	$Q echo -e '#include "external/blake3/c/blake3.c"'                                   >> $>/ase/blake3impl.c
	$Q echo -e '#include "external/blake3/c/blake3_portable.c"'                          >> $>/ase/blake3impl.c
	$Q echo -e '#include "external/blake3/c/blake3_dispatch.c"'                          >> $>/ase/blake3impl.c
$>/ase/blake3avx512.c $>/ase/blake3avx2.c $>/ase/blake3sse41.c $>/ase/blake3sse2.c: $>/ase/blake3impl.c

# == Common Sources ==
ase/generated.sources	+= $(strip	\
	$>/ase/blake3impl.c		\
	$>/ase/blake3avx512.c		\
	$>/ase/blake3avx2.c		\
	$>/ase/blake3sse41.c		\
	$>/ase/blake3sse2.c		\
)

# == ase/buildversion-*.cc ==
$>/ase/buildversion-$(version_hash).cc:						| $>/ase/
	$(QGEN)
	$Q echo '// make $@'							> $@.tmp
	$Q echo '#include <ase/platform.hh>'					>>$@.tmp
	$Q echo 'namespace Ase {'						>>$@.tmp
	$Q echo 'const int         ase_major_version = $(version_major);'	>>$@.tmp
	$Q echo 'const int         ase_minor_version = $(version_minor);'	>>$@.tmp
	$Q echo 'const int         ase_micro_version = $(version_micro);'	>>$@.tmp
	$Q echo 'const char *const ase_version_long = "$(version_short)+g$(version_hash) ($(INSN))";'	>>$@.tmp
	$Q echo 'const char *const ase_version_short = "$(version_short)";'	>>$@.tmp
	$Q echo 'const char *const ase_gettext_domain = "anklang-$(version_short)";' >>$@.tmp
	$Q echo '} // Ase'							>>$@.tmp
	$Q mv $@.tmp $@
ase/generated.sources += $>/ase/buildversion-$(version_hash).cc

# == ase/tests/TestList.g.mk ==
ase/tests/TestList.g.INPUTS := $(wildcard ase/*.cc ase/*/*.cc devices/*.cc devices/*/*.cc)
ase/tests/TestList.g.mk:	# any deps here are forced to be rebuilt during Makefile parsing
	$(QGEN)
	$Q echo 'ASE_TEST_LIST := '\\			> $@.tmp
	$Q cat $(ase/tests/TestList.g.INPUTS) | \
		grep -Eo '^\s*TEST_\w+ ?\((\w+)\)' | \
		sed 's/.*(/  /; s/)/ \\/' | \
		sort -d		>> $@.tmp
	$Q uniq $@.tmp > $@.uniq.tmp && cmp -s $@.uniq.tmp $@.tmp && rm $@.uniq.tmp || \
		{ git -P diff --no-index -- $@.uniq.tmp $@.tmp ; \
		  echo "$@: error: test functions are not unique" >&2 ; false ; }
	$Q mv $@.tmp $@
ifneq (,$(shell find $(ase/tests/TestList.g.INPUTS) -newer ase/tests/TestList.g.mk))
.PHONY: ase/tests/TestList.g.mk		# update generated file without forcing rebuild of INPUTS
endif
include ase/tests/TestList.g.mk		# ASE_TEST_LIST

# == Tracktion Engine Objects ==
include trkn/trkn.g.mk
TRKN_OBJECTS ::=
# TRKN_DEFS  = -pthread -Oz -ffast-math -fvisibility=hidden
TRKN_DEFS += $(patsubst -I%, -Itrkn/%, $(TRACKTION_INTERNAL_INCLUDES))
TRKN_DEFS += $(ASE_EXTRA_INCLUDES) # $(TRACKTION_EXTERNAL_INCLUDES)
TRKN_DEFS += -Iexternal/soundtouch/include/
TRKN_DEFS += -D JUCE_APP_CONFIG_HEADER='"trkn/config.hh"' # $(TRACKTION_HEADER_DEFINES)
TRKN_DEFS += $(TRACKTION_CCBODY_DEFINES)
TRKN_DEFS += -DJUCE_INCLUDE_OGGVORBIS_CODE=0
TRKN_DEFS += -DJUCE_INCLUDE_PNGLIB_CODE=0 -DJUCE_INCLUDE_JPEGLIB_CODE=0
TRKN_DEFS += -DJUCE_INCLUDE_ZLIB_CODE=0 -DJUCE_ZLIB_INCLUDE_PATH='<zlib.h>'
define TRKN_CXXOBJECT_RULE
$$>/trkn/$$(notdir $(1:.cpp=.o)): $1	| $$>/trkn/
	$$(QECHO) CXX $$@
	$$(Q) $$(CCACHE) $$(CXX) $$(CXXSTD) $$(TRKN_DEFS) -fPIC $$(compiledefs) $$(compilecxxflags) -c $$< -o $$@
TRKN_OBJECTS += $$>/trkn/$$(notdir $(1:.cpp=.o))
endef	# $$(compilecxxflags)
$(foreach F, $(filter %.cpp, $(JUCE_SOURCES) $(TRACKTION_SOURCES)), $(eval $(call TRKN_CXXOBJECT_RULE, trkn/$F)))
$(TRKN_OBJECTS): $(EXTERNAL_CXX_STAMPS)
include $(wildcard $>/trkn/*.d)

# == ase/PchList.g.mk ==
ase/PchList.g.INPUTS := $(wildcard ase/*.cc)
ase/PchList.g.mk:	# any deps here are forced to be rebuilt during Makefile parsing
	$(QGEN)
	$Q echo 'ASE_PCH_FILES := '\\			> $@.tmp
	$Q grep -l '^#include "trkn/tracktion.hh"' \
		$(ase/PchList.g.INPUTS) | \
		sed -r 's/(.*)/  \1 \\/' | \
		sort -d		>> $@.tmp
	$Q mv $@.tmp $@
ifneq (,$(shell find $(ase/PchList.g.INPUTS) -newer ase/PchList.g.mk))
.PHONY: ase/PchList.g.mk	# update generated file without forcing rebuild of INPUTS
endif
include ase/PchList.g.mk	# ASE_PCH_FILES
# Precompiled Headers for trkn/tracktion.hh
$(addprefix $>/, $(ASE_PCH_FILES:.cc=.o)): $(call INCLUDE_PCH, trkn/tracktion.hh )
# Precompiled Headers for JUCE
$>/ase/juce-linux.o:	$(call INCLUDE_PCH, trkn/juce.hh )

# == ase/anklang.objects ==
ase/anklang.ccobjects := $(call BUILDDIR_O, $(filter %.cc, $(ase/anklang.sources)))
ase/anklang.objects := $(sort \
	$(call BUILDDIR_O, $(ase/anklang.sources))	\
	$(call SOURCE2_O, $(ase/generated.sources))	\
)
$(ase/anklang.objects) $(devices/4ase.objects): $>/ase/sysconfig.h $(EXTERNAL_CXX_STAMPS)
$(ase/anklang.objects) $(devices/4ase.objects): EXTRA_INCLUDES += $(ASE_EXTRA_INCLUDES) -I$>

# == cpptrace ==
ifeq ($(MODE),cpptrace)
LIBCPPTRACE_HEADERS := $>/cpptrace/include/cpptrace/cpptrace.hpp $>/cpptrace/include/cpptrace/from_current.hpp
$(LIBCPPTRACE_HEADERS): $>/cpptrace/lib/libcpptrace.a
$(ase/anklang.objects): $(LIBCPPTRACE_HEADERS)
$(ase/anklang.objects): EXTRA_INCLUDES += -I$>/cpptrace/include -DASE_WITH_CPPTRACE=1  # also provides addiotnal zstd.h
$>/cpptrace/lib/libcpptrace.a:		| $(EXTERNAL_CXX_STAMPS)
	$(QGEN)
	$Q rm -rf $>/cpptrace/ && mkdir -p $>/cpptrace/
	$Q cd $>/cpptrace/ && cmake ../../external/cpptrace -DCMAKE_INSTALL_PREFIX="$$PWD" -DCMAKE_BUILD_TYPE=Release
	$Q cd $>/cpptrace/ && $(MAKE) -j`nproc`
	$Q cd $>/cpptrace/ && $(MAKE) install
ASEDEPS_LIBS += -L$>/cpptrace/lib -lcpptrace -ldwarf
endif

# == AnklangSynthEngine ==
# Work around legacy code in external/websocketpp/*.hpp
ase/websocket.cc.FLAGS = -Wno-deprecated-dynamic-exception-spec -Wno-sign-promo
# Allow tests in mathutils.cc
ase/mathutils.cc.CTIDY_FLAGS = --checks=-clang-analyzer-security.FloatLoopCounter
$(lib/AnklangSynthEngine): $(lib/libsndfile.so)				| $>/lib/
$(call BUILD_PROGRAM, \
	$(lib/AnklangSynthEngine), \
	$(ase/anklang.objects) $(devices/4ase.objects) $(TRKN_OBJECTS), \
	$(lib/libase.so), \
	$(BOOST_SYSTEM_LIBS) $(ASEDEPS_LIBS) $(ALSA_LIBS) -lzstd -ldl $(lib/libsndfile.so), \
	../lib)
CXX_TARGETS += $(lib/AnklangSynthEngine)

# == jackdriver.so ==
lib/jackdriver.so	     ::= $>/lib/jackdriver.so
ase/jackdriver.objects	     ::= $(call BUILDDIR_O, $(ase/jackdriver.sources))
$(ase/jackdriver.objects):   $>/ase/sysconfig.h
$(ase/jackdriver.objects):   EXTRA_INCLUDES += -I$>
$(lib/jackdriver.so).LDFLAGS ::= -Wl,--unresolved-symbols=ignore-in-object-files
ifneq ('','$(ANKLANG_JACK_LIBS)')
lib/jackdriver.so.MAYBE ::= $(lib/jackdriver.so)
$(call BUILD_SHARED_LIB,		\
	$(lib/jackdriver.so),		\
	$(ase/jackdriver.objects),	\
	$(lib/libase.so) | $>/lib/,	\
	$(ANKLANG_JACK_LIBS) $(lib/libase.so), \
	../lib)
endif
CXX_TARGETS += $(lib/jackdriver.so.MAYBE)

# == gtk2wrap.so ==
lib/gtk2wrap.so         ::= $>/lib/gtk2wrap.so
ase/gtk2wrap.objects    ::= $(call BUILDDIR_O, $(ase/gtk2wrap.sources))
$(ase/gtk2wrap.objects): EXTRA_INCLUDES += -I$> $(GTK2_CFLAGS)
$(ase/gtk2wrap.objects): EXTRA_CXXFLAGS += -Wno-deprecated -Wno-deprecated-declarations
$(ase/gtk2wrap.objects): $>/ase/sysconfig.h
$(call BUILD_SHARED_LIB, \
	$(lib/gtk2wrap.so), \
	$(ase/gtk2wrap.objects), \
	$(lib/libase.so) | $>/lib/, \
	$(GTK2_LIBS), \
	../lib)
CXX_TARGETS += $(lib/gtk2wrap.so)

# == install binaries ==
$(call INSTALL_BIN_RULE, $(basename $(lib/AnklangSynthEngine)), $(DESTDIR)$(pkgdir)/lib, $(wildcard \
	$(lib/AnklangSynthEngine)	\
	$(lib/jackdriver.so.MAYBE)	\
	$(lib/libsndfile.so)		\
	$(lib/gtk2wrap.so)		\
  ))

# == build media/Samples ==
# See: platform.cc:SAMPLEDIR
$>/.media.done: $(EXTERNAL_BLOBS4ANKLANG_STAMPS) $>/media/Samples/
	$(QGEN)
	$Q rm -rf $>/media/Samples/freepats-vorbis/
	$Q mkdir -p $>/media/Samples/freepats-vorbis/Drum/ $>/media/Samples/freepats-vorbis/Tone/
	$Q ln -s $(abspath external/freepats-vorbis/Drum/*.ogg) $>/media/Samples/freepats-vorbis/Drum/
	$Q ln -s $(abspath external/freepats-vorbis/Tone/*.ogg) $>/media/Samples/freepats-vorbis/Tone/
	$Q touch $@
CXX_TARGETS += $>/.media.done

# == install media/Samples ==
media/install:
	@$(QECHO) INSTALL '$(DESTDIR)$(pkgmediadir)'
	$Q rm -rf '$(DESTDIR)$(pkgmediadir)' && mkdir -p '$(DESTDIR)$(pkgmediadir)'
	$Q cp -RL $>/media/Samples '$(DESTDIR)$(pkgmediadir)'
install: media/install
.PHONY: media/install

# == uninstall media/Samples ==
media/uninstall:
	@$(QECHO) UNINSTALL '$(DESTDIR)$(pkgmediadir)'
	$Q rm -rf '$(DESTDIR)$(pkgmediadir)'
uninstall: media/uninstall
.PHONY: media/uninstall

# == install ==
ase/install: $(lib/AnklangSynthEngine)
	@$(QECHO) INSTALL '$(DESTDIR)$(bindir)/anklang'
	$Q rm -f '$(DESTDIR)$(pkgdir)/bin/anklang'
	$Q rm -f '$(DESTDIR)$(bindir)/anklang' && mkdir -p '$(DESTDIR)$(bindir)' \
	&& ln -s -r '$(DESTDIR)$(pkgdir)/bin/anklang' '$(DESTDIR)$(bindir)/anklang'
	$Q mkdir -p '$(DESTDIR)$(pkgdir)/bin' \
	&& ln -s '../lib/AnklangSynthEngine' '$(DESTDIR)$(pkgdir)/bin/anklang'
install: ase/install
.PHONY: ase/install

# == uninstall ==
ase/uninstall:
	@$(QECHO) REMOVE '$(DESTDIR)$(bindir)/anklang'
	$Q rm -f '$(DESTDIR)$(pkgdir)/bin/anklang'
	$Q $(RMDIR_P) '$(DESTDIR)$(pkgdir)/bin' || true
	$Q rm -f '$(DESTDIR)$(bindir)/anklang'
.PHONY: ase/uninstall
uninstall: ase/uninstall

# == ase/lint ==
ase/lint: node_modules/.npm.done
	$(QGEN)
	$Q $(RUNTS) misc/synsmell.ts $(wildcard ase/*.[hc] ase/*.*[hc] ase/*/*.*[hc] jsonipc/*.hh)
.PHONY: ase/lint
lint: ase/lint

# == check-ase-tests ==
.PHONY: check-ase-tests
define ASE_TEST_CHECK
check-$1: $$(lib/AnklangSynthEngine)
	$$(QECHO) CHECK '$1'
	$$Q $$(lib/AnklangSynthEngine) --norc --no-devices --test '$1'
.PHONY: check-$1
check-ase-tests: check-$1
endef
$(foreach T, $(ASE_TEST_LIST), $(eval $(call ASE_TEST_CHECK,$T)))
CHECK_TARGETS += check-ase-tests

# == libsndfile ==
# cmake -B out/sndfile/ -S external/libsndfile/ -DBUILD_SHARED_LIBS=ON -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=MINSIZEREL
# (cd out/sndfile/ && bear -- make -j)
# sed -nr '/"file":/{ s/.* "//; s/", */ \\/; s|/src/anklang/external/libsndfile/src/|\t|; p }' out/sndfile/compile_commands.json
LIBSNDFILE_CFILES := $(strip \
	ALAC/ALACBitUtilities.c \
	ALAC/ag_dec.c \
	ALAC/ag_enc.c \
	ALAC/alac_decoder.c \
	ALAC/alac_encoder.c \
	ALAC/dp_dec.c \
	ALAC/dp_enc.c \
	ALAC/matrix_dec.c \
	ALAC/matrix_enc.c \
	G72x/g721.c \
	G72x/g723_16.c \
	G72x/g723_24.c \
	G72x/g723_40.c \
	G72x/g72x.c \
	GSM610/add.c \
	GSM610/code.c \
	GSM610/decode.c \
	GSM610/gsm_create.c \
	GSM610/gsm_decode.c \
	GSM610/gsm_destroy.c \
	GSM610/gsm_encode.c \
	GSM610/gsm_option.c \
	GSM610/long_term.c \
	GSM610/lpc.c \
	GSM610/preprocess.c \
	GSM610/rpe.c \
	GSM610/short_term.c \
	GSM610/table.c \
	aiff.c \
	alac.c \
	alaw.c \
	au.c \
	audio_detect.c \
	avr.c \
	broadcast.c \
	caf.c \
	cart.c \
	chanmap.c \
	chunk.c \
	command.c \
	common.c \
	dither.c \
	double64.c \
	dwd.c \
	dwvw.c \
	file_io.c \
	flac.c \
	float32.c \
	g72x.c \
	gsm610.c \
	htk.c \
	id3.c \
	ima_adpcm.c \
	ima_oki_adpcm.c \
	interleave.c \
	ircam.c \
	macos.c \
	mat4.c \
	mat5.c \
	mpc2k.c \
	mpeg.c \
	mpeg_decode.c \
	mpeg_l3_encode.c \
	ms_adpcm.c \
	nist.c \
	nms_adpcm.c \
	ogg.c \
	ogg_opus.c \
	ogg_pcm.c \
	ogg_speex.c \
	ogg_vcomment.c \
	ogg_vorbis.c \
	paf.c \
	pcm.c \
	pvf.c \
	raw.c \
	rf64.c \
	rx2.c \
	sd2.c \
	sds.c \
	sndfile.c \
	strings.c \
	svx.c \
	txw.c \
	ulaw.c \
	voc.c \
	vox_adpcm.c \
	w64.c \
	wav.c \
	wavlike.c \
	wve.c \
	xi.c \
)
LIBSNDFILE_SOURCES := $(LIBSNDFILE_CFILES:%=external/libsndfile/src/%)

# == libsndfile/config.h ==
$>/libsndfile/config.h: ase/Makefile.mk $(EXTERNAL_CXX_STAMPS)		| $>/libsndfile/
	$(QGEN)
	$Q echo ''									>  $@.tmp
	$Q echo '#define PACKAGE_NAME "libsndfile"'					>> $@.tmp
	$Q echo '#define PACKAGE_VERSION "$(libsndfile/version)"'			>> $@.tmp
	$Q echo '#define CPU_IS_BIG_ENDIAN 0'						>> $@.tmp
	$Q echo '#define CPU_IS_LITTLE_ENDIAN 1'					>> $@.tmp
	$Q echo '#define COMPILER_IS_GCC	__GNUC__'				>> $@.tmp
	$Q echo '#define OS_IS_OPENBSD			0'				>> $@.tmp
	$Q echo '#define OS_IS_WIN32			0'				>> $@.tmp
	$Q echo '#define OS_IS_LINUX			1'				>> $@.tmp
	$Q echo '#define _MINIX				0'				>> $@.tmp
	$Q echo '#define SIZEOF_INT64_T			8'				>> $@.tmp
	$Q echo '#define SIZEOF_OFF_T		__SIZEOF_SIZE_T__'			>> $@.tmp
	$Q echo '#define SIZEOF_VOIDP		__SIZEOF_SIZE_T__'			>> $@.tmp
	$Q echo '#define SIZEOF_WCHAR_T		__SIZEOF_WCHAR_T__'			>> $@.tmp
	$Q echo '#define HAVE_ALSA_ASOUNDLIB_H	__has_include(<alsa/asoundlib.h>)'	>> $@.tmp
	$Q echo '#define HAVE_BYTESWAP_H	__has_include(<byteswap.h>)'		>> $@.tmp
	$Q echo '#define HAVE_DLFCN_H		__has_include(<dlfcn.h>)'		>> $@.tmp
	$Q echo '#define HAVE_ENDIAN_H		__has_include(<endian.h>)'		>> $@.tmp
	$Q echo '#define HAVE_IMMINTRIN_H	__has_include(<immintrin.h>)'		>> $@.tmp
	$Q echo '#define HAVE_INTTYPES_H	__has_include(<inttypes.h>)'		>> $@.tmp
	$Q echo '#define HAVE_LOCALE_H		__has_include(<locale.h>)'		>> $@.tmp
	$Q echo '#define HAVE_SNDIO_H		__has_include(<sndio.h>)'		>> $@.tmp
	$Q echo '#define HAVE_STDBOOL_H		__has_include(<stdbool.h>)'		>> $@.tmp
	$Q echo '#define HAVE_STDINT_H		__has_include(<stdint.h>)'		>> $@.tmp
	$Q echo '#define HAVE_SYS_TIME_H	__has_include(<sys/time.h>)'		>> $@.tmp
	$Q echo '#define HAVE_SYS_TIME_H	__has_include(<sys/time.h>)'		>> $@.tmp
	$Q echo '#define HAVE_SYS_TYPES_H	__has_include(<sys/types.h>)'		>> $@.tmp
	$Q echo '#define HAVE_UNISTD_H		__has_include(<unistd.h>)'		>> $@.tmp
	$Q echo '#define HAVE_DECL_S_IRGRP	__has_include(<sys/stat.h>)'		>> $@.tmp
	$Q echo '#define HAVE_FSTAT			1'				>> $@.tmp
	$Q echo '#define HAVE_FSTAT64			1'				>> $@.tmp
	$Q echo '#define HAVE_FSYNC			1'				>> $@.tmp
	$Q echo '#define HAVE_FTRUNCATE			1'				>> $@.tmp
	$Q echo '#define HAVE_GETPAGESIZE	__has_include(<unistd.h>)'		>> $@.tmp
	$Q echo '#define HAVE_GETTIMEOFDAY		1'				>> $@.tmp
	$Q echo '#define HAVE_GMTIME			1'				>> $@.tmp
	$Q echo '#define HAVE_GMTIME_R		OS_IS_LINUX'				>> $@.tmp
	$Q echo '#define HAVE_LRINT			1'				>> $@.tmp
	$Q echo '#define HAVE_LRINTF			1'				>> $@.tmp
	$Q echo '#define HAVE_LSEEK			1'				>> $@.tmp
	$Q echo '#define HAVE_OPEN			1'				>> $@.tmp
	$Q echo '#define HAVE_READ			1'				>> $@.tmp
	$Q echo '#define HAVE_WRITE			1'				>> $@.tmp
	$Q echo "#define HAVE_MPEG		( __has_include(<lame/lame.h>) && \\"	>> $@.tmp
	$Q echo "				  __has_include(<mpg123.h>) )"		>> $@.tmp
	$Q echo "#define HAVE_EXTERNAL_XIPH_LIBS	( \\"				>> $@.tmp
	$Q echo "				__has_include(<FLAC/all.h>) && \\"	>> $@.tmp
	$Q echo "				__has_include(<ogg/ogg.h>) && \\"	>> $@.tmp
	$Q echo "				__has_include(<vorbis/vorbisfile.h>) && \\"	>> $@.tmp
	$Q echo "				__has_include(<vorbis/vorbisenc.h>) && \\"	>> $@.tmp
	$Q echo "				__has_include(<opus/opus.h>) )"		>> $@.tmp
	$Q mv $@.tmp $@
$(LIBSNDFILE_SOURCES): $>/libsndfile/config.h

# == lib/libsndfile.so ==
LIBSNDFILE_OBJECTS := $(LIBSNDFILE_SOURCES:external/libsndfile/src/%.c=$>/libsndfile/%.o)
$>/libsndfile/%.o: external/libsndfile/src/%.c		| $(dir $(sort $(LIBSNDFILE_OBJECTS)))
	$(QECHO) CC $@
	$Q $(CCACHE) $(CC) -fPIC $(compiledefs) $(compilecxxflags) \
		-I external/libsndfile/include -I external/libsndfile/src -I $>/libsndfile/ \
		$(SNDFILEDEPS_CFLAGS) -Dsndfile_EXPORTS -DNDEBUG -c $< -o $@
$(call BUILD_SHARED_LIB,			\
	$(lib/libsndfile.so),			\
	$(LIBSNDFILE_OBJECTS),			\
	ase/Makefile.mk | $>/lib/,		\
	$(SNDFILEDEPS_LIBS),				\
	../lib)
ase/sndfile.cc: $>/libsndfile/config.h	# includes libsndfile/config.h
