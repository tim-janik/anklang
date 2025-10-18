# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ase/*.d)

# == ase/ *.cc file sets ==
lib/libsndfile.so		:= $>/lib/libsndfile.so.$(libsndfile/lt_current.lt_age.lt_revision)
ase/jackdriver.sources		:= ase/driver-jack.cc
ase/gtk2wrap.sources		:= ase/gtk2wrap.cc
ase/not-anklang.sources		:= $(ase/gtk2wrap.sources) $(ase/jackdriver.sources)
ase/anklang.sources		:= $(filter-out $(ase/not-anklang.sources), $(wildcard ase/*.cc)) $(wildcard ase/*.c)
lib/AnklangSynthEngine		:= $>/lib/AnklangSynthEngine
ase/generated.sources		:=
ASE_EXTRA_INCLUDES := $(strip			\
	-Iexternal				\
	-Iexternal/clap/include			\
	-Iexternal/libsndfile/include		\
	-Iexternal/liquidsfz/lib		\
	-Iexternal/nlohmann-json/include	\
	-Iexternal/pandaresampler/include	\
	-Iexternal/rapidjson/include		\
	-Iexternal/websocketpp			\
	$(ASEDEPS_CFLAGS)			\
) # also used by clang-tidy

# == ase/sysconfig.h ==
$>/ase/sysconfig.h: $(config-stamps)			| $>/ase/ # ase/Makefile.mk
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

# == codegen/ase/gen/api-jsonipc.json ==
$>/codegen/ase/gen/api-jsonipc.json: ase/api.hh $>/ase/sysconfig.h ase/Makefile.mk
	$(QGEN)
	$Q clang-20 -std=gnu++23 -I . -I out/ -extract-api $< -o $@

# == ase/gen/class-tree.g.md ==
$>/codegen/ase/gen/class-tree.g.md: $>/codegen/ase/gen/api-jsonipc.json jsonipc/jsonbindings.ts ase/Makefile.mk
	$(QGEN)
	$Q $(RUNTS) jsonipc/jsonbindings.ts --class-tree $<				>  $@.tmp
	$Q mv $@.tmp $@
CODEGEN.FILES += ase/gen/class-tree.g.md

# == ase/gen/api-jsonipc.g.cc ==
$>/codegen/ase/gen/api-jsonipc.g.cc: $>/codegen/ase/gen/api-jsonipc.json jsonipc/jsonbindings.ts ase/Makefile.mk
	$(QGEN)
	$Q echo '// Generated file, inputs: $(notdir $^)'				>  $@.tmp
	$Q echo '#include <ase/jsonapi.hh>'						>> $@.tmp
	$Q echo '#include <ase/api.hh>'							>> $@.tmp
	$Q $(RUNTS) jsonipc/jsonbindings.ts --cxx $<					>> $@.tmp
	$Q echo '[[maybe_unused]] static bool init_jsonipc = [] {'			>> $@.tmp
	$Q echo '  if (getenv ("ASE_JSONTS"))'						>> $@.tmp
	$Q echo '    Jsonipc::g_binding_printer = new Jsonipc::BindingPrinter();'	>> $@.tmp
	$Q echo '  jsonipc_for_api_jsonipc_json();'					>> $@.tmp
	$Q echo '  return 0;'								>> $@.tmp
	$Q echo '} ();'									>> $@.tmp
	$Q mv $@.tmp $@
CODEGEN.FILES += ase/gen/api-jsonipc.g.cc
# DEV_TARGETS - checks & helpers for development
$>/codegen/.jsonbindings.tscheck: jsonipc/jsonbindings.ts ase/Makefile.mk	| $>/codegen/ node_modules/.npm.done
	$(QGEN)
	$Q node_modules/.bin/tsc --noEmit --allowJs --moduleResolution bundler -m esnext --target esnext --erasableSyntaxOnly $<
	$Q touch $@
ALL_TARGETS += $>/codegen/.jsonbindings.tscheck
$>/codegen/ase/gen/api-jsonipc.pretty.json: $>/codegen/ase/gen/api-jsonipc.json ase/Makefile.mk
	$(QGEN) # JSON formatted for human inspection
	$Q python3 -m json.tool < $< > $@
ALL_TARGETS += $>/codegen/ase/gen/api-jsonipc.pretty.json

# == ase/gen/api-jsonipc.g.ts ==
$>/codegen/ase/gen/api-jsonipc.g.ts: ase/api.hh jsonipc/jsonipc.ts $(lib/AnklangSynthEngine) ase/Makefile.mk
	$(QGEN)
	$Q echo '// Generated from: $(notdir $^)'					>  $@.tmp
	$Q cat jsonipc/jsonipc.ts							>> $@.tmp
	$Q echo 'export class SharedBase // Ase::SharedBase'				>> $@.tmp
	$Q echo '  extends Jsonipc.Jsonipc_prototype {'					>> $@.tmp
	$Q echo '  constructor ($id) { super ($id);'					>> $@.tmp
	$Q echo ' if (new.target === SharedBase) Jsonipc.ofreeze (this); }'		>> $@.tmp
	$Q echo '};'									>> $@.tmp
	$Q echo 'Jsonipc.classes["Ase::SharedBase"] = SharedBase;'			>> $@.tmp
	$Q echo										>> $@.tmp
	$Q ASAN_OPTIONS=detect_leaks=0 ASE_JSONTS=1 \
	$(lib/AnklangSynthEngine) --norc  -P null -M null --jsonts			>> $@.tmp
	$Q echo '/**@type{ServerImpl}*/'						>> $@.tmp
	$Q echo -n 'export let server: Promise<Server> | Server ='			>> $@.tmp
	$Q echo 'Jsonipc.setup_promise_type (Server, s => server = s);'			>> $@.tmp
	$Q mv $@.tmp $@
CODEGEN.FILES += ase/gen/api-jsonipc.g.ts
# DEV_TARGETS - checks & helpers for development
$>/codegen/.api-jsonipc.tscheck: ase/gen/api-jsonipc.g.ts ase/Makefile.mk		| $>/codegen/ node_modules/.npm.done
	$(QGEN)
	$Q node_modules/.bin/tsc --noEmit --allowJs --moduleResolution bundler -m esnext --target esnext --erasableSyntaxOnly $<
	$Q touch $@

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

# == AnklangSynthEngine ==
ase/anklang.ccobjects := $(call BUILDDIR_O, $(filter %.cc, $(ase/anklang.sources)))
ase/anklang.objects := $(sort \
	$(call BUILDDIR_O, $(ase/anklang.sources))	\
	$(call SOURCE2_O, $(ase/generated.sources))	\
)
$(ase/anklang.objects) $(devices/4ase.objects): $>/ase/sysconfig.h $(EXTERNAL_CXX_STAMPS)
$(ase/anklang.objects) $(devices/4ase.objects): EXTRA_INCLUDES += $(ASE_EXTRA_INCLUDES) -I$>
# Work around legacy code in external/websocketpp/*.hpp
ase/websocket.cc.FLAGS = -Wno-deprecated-dynamic-exception-spec -Wno-sign-promo
# Allow tests in mathutils.cc
ase/mathutils.cc.CTIDY_FLAGS = --checks=-clang-analyzer-security.FloatLoopCounter
$(lib/AnklangSynthEngine): $(lib/libsndfile.so)				| $>/lib/
$(call BUILD_PROGRAM, \
	$(lib/AnklangSynthEngine), \
	$(ase/anklang.objects) $(devices/4ase.objects), \
	$(lib/libase.so), \
	$(BOOST_SYSTEM_LIBS) $(ASEDEPS_LIBS) $(ALSA_LIBS) -lzstd -ldl $(lib/libsndfile.so), \
	../lib)
ALL_TARGETS += $(lib/AnklangSynthEngine)

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
ALL_TARGETS += $(lib/jackdriver.so.MAYBE)

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
ALL_TARGETS += $(lib/gtk2wrap.so)

# == install binaries ==
$(call INSTALL_BIN_RULE, $(basename $(lib/AnklangSynthEngine)), $(DESTDIR)$(pkgdir)/lib, $(wildcard \
	$(lib/AnklangSynthEngine)	\
	$(lib/jackdriver.so.MAYBE)	\
	$(lib/libsndfile.so)		\
	$(lib/gtk2wrap.so)		\
  ))

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
ase/lint:
	$(QGEN)
	$Q $(RUNTS) misc/synsmell.ts $(wildcard ase/*.[hc] ase/*.*[hc] ase/*/*.*[hc] jsonipc/*.hh)
.PHONY: ase/lint
lint: ase/lint

# == Check Integrity Tests ==
check-ase-tests: $(lib/AnklangSynthEngine)
	$(eval xargs_parallel != P=`parallel --help 2>/dev/null` && \
	  [[ $$$$P =~ GNU.[Pp]arallel ]] && echo 'parallel --ungroup' || \
	  { echo 'xargs -n1'; echo "$$$$0: missing 'GNU parallel', falling back to 'xargs'" >&2; } )
	$(QGEN)
	$Q : $(lib/AnklangSynthEngine) --check
	$Q set -Eeuo pipefail \
	&& $(lib/AnklangSynthEngine) --list-tests \
	|  $(xargs_parallel) $(lib/AnklangSynthEngine) --norc -P null -M null --test
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
	$Q echo "				__has_include(<flac.h>) && \\"		>> $@.tmp
	$Q echo "				__has_include(<ogg.h>) && \\"		>> $@.tmp
	$Q echo "				__has_include(<vorbisfile.h>) && \\"	>> $@.tmp
	$Q echo "				__has_include(<vorbisenc.h>) && \\"	>> $@.tmp
	$Q echo "				__has_include(<opus.h>) )"		>> $@.tmp
	$Q mv $@.tmp $@
$(LIBSNDFILE_SOURCES): $>/libsndfile/config.h

# == lib/libsndfile.so ==
LIBSNDFILE_OBJECTS := $(LIBSNDFILE_SOURCES:external/libsndfile/src/%.c=$>/libsndfile/%.o)
$>/libsndfile/%.o: external/libsndfile/src/%.c		| $(dir $(sort $(LIBSNDFILE_OBJECTS)))
	$(QECHO) CC $@
	$Q $(CCACHE) $(CC) -fPIC $(compiledefs) $(compilecxxflags) \
		-I external/libsndfile/include -I external/libsndfile/src -I $>/libsndfile/ \
		-Dsndfile_EXPORTS -DNDEBUG -c $< -o $@
$(call BUILD_SHARED_LIB,			\
	$(lib/libsndfile.so),			\
	$(LIBSNDFILE_OBJECTS),			\
	ase/Makefile.mk | $>/lib/,		\
	-lmpg123 -lmp3lame,			\
	../lib)
ase/sndfile.cc: $>/libsndfile/config.h	# includes libsndfile/config.h
