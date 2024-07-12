# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ase/*.d)
CLEANDIRS     += $(wildcard $>/ase/ $>/lib/)

include ase/tests/Makefile.mk

# == ase/ *.cc file sets ==
ase/jackdriver.sources		::= ase/driver-jack.cc
ase/gtk2wrap.sources		::= ase/gtk2wrap.cc
ase/noglob.cc			::= ase/main.cc $(ase/gtk2wrap.sources) $(ase/jackdriver.sources)
ase/libsources.cc		::= $(filter-out $(ase/noglob.cc), $(wildcard ase/*.cc))
ase/libsources.c		::= $(wildcard ase/*.c)
ase/include.deps		::= $>/ase/sysconfig.h

# == AnklangSynthEngine definitions ==
lib/AnklangSynthEngine		::= $>/lib/AnklangSynthEngine
ase/AnklangSynthEngine.sources	::= ase/main.cc $(ase/libsources.cc) $(ase/libsources.c)
ase/AnklangSynthEngine.gensrc	::= $(strip \
	$>/ase/api.jsonipc.cc		\
	$>/ase/blake3impl.c		\
	$>/ase/blake3avx512.c		\
	$>/ase/blake3avx2.c		\
	$>/ase/blake3sse41.c		\
	$>/ase/blake3sse2.c		\
)
ase/AnklangSynthEngine.objects	::= $(sort \
	$(call BUILDDIR_O, $(ase/AnklangSynthEngine.sources)) \
	$(call SOURCE2_O, $(ase/AnklangSynthEngine.gensrc))   \
	$(ase/tests/objects) \
)
ase/AnklangSynthEngine.objects	 += $(devices/4ase.objects)
ALL_TARGETS += $(lib/AnklangSynthEngine)

# == AnklangSynthEngine-fma ==
$(lib/AnklangSynthEngine)-fma:
	$(QGEN)
	$Q $(MAKE) INSN=fma builddir=$>/fma $>/fma/lib/AnklangSynthEngine
	$Q $(CP) -v $>/fma/lib/AnklangSynthEngine.map $@.map
	$Q $(CP) -v $>/fma/lib/AnklangSynthEngine $@
ifeq ($(MODE)+$(INSN),production+sse)
ALL_TARGETS += $(lib/AnklangSynthEngine)-fma
# Iff MODE=production and INSN=sse (i.e. release builds),
# also build an FMA variant of the sound engine.
endif

# == ase/api.jsonipc.cc ==
$>/ase/api.jsonipc.cc: ase/api.hh jsonipc/cxxjip.py $(ase/include.deps) | $>/ase/ # ase/Makefile.mk
	$(QGEN)
	$Q echo '#include <ase/jsonapi.hh>'							>  $@.tmp
	$Q echo '#include <ase/api.hh>'								>> $@.tmp
	$Q $(PYTHON3) jsonipc/cxxjip.py $< -N Ase -I. -I$>/ -Iout/external/			>> $@.tmp
	$Q echo '[[maybe_unused]] static bool init_jsonipc = (jsonipc_4_api_hh(), 0);'		>> $@.tmp
	$Q mv $@.tmp $@

# == ase/buildversion-$(version_short).cc ==
ase/buildsum != echo '$(sharedir)' | sha256sum - | sed -r 's/(.{12}).*/$(version_short).mk\1/'
ase/buildversion.cc := $>/ase/buildversion-$(ase/buildsum).cc
$(ase/buildversion.cc):								| $>/ase/ # $(GITCOMMITDEPS)
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
	$Q echo 'const char *const ase_sharedir = "$(sharedir)";'		>>$@.tmp
	$Q echo '} // Ase'							>>$@.tmp
	$Q mv $@.tmp $@
ase/AnklangSynthEngine.objects += $(ase/buildversion.cc:.cc=.o)

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

# == libsndfile ==
$>/lib/libsndfile.so: external/libsndfile/include/sndfile.hh		| $>/lib/ .submodule-stamp
	$(QGEN)
	$Q cmake \
		-B $>/sndfile/ -S external/libsndfile/ -DCMAKE_BUILD_TYPE=MINSIZEREL \
		-DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=ON
	$Q $(MAKE) -C $>/sndfile/
	$Q $(CP) -P $>/sndfile/libsndfile.so* $>/lib/
CLEANDIRS += $>/sndfile/
CLEANFILES += $>/lib/libsndfile.*
ase/sndfile.cc: $>/lib/libsndfile.so # includes $>/sndfile/src/config.h

# == AnklangSynthEngine ==
ASE_EXTERNAL_INCLUDES := $(strip		\
	-Iexternal/clap/include			\
	-Iexternal/libsndfile/include		\
	-Iexternal/liquidsfz/lib		\
	-Iexternal/pandaresampler/include	\
	-Iexternal/rapidjson/include		\
	-Iexternal/websocketpp			\
) # also used by clang-tidy
$(ase/AnklangSynthEngine.objects): $(ase/include.deps) $(ase/libase.deps)
$(ase/AnklangSynthEngine.objects): EXTRA_INCLUDES ::= $(ASE_EXTERNAL_INCLUDES) -I$> -I$>/external/ $(ASEDEPS_CFLAGS)
$(lib/AnklangSynthEngine): $>/lib/libsndfile.so					| $>/lib/
$(call BUILD_PROGRAM, \
	$(lib/AnklangSynthEngine), \
	$(ase/AnklangSynthEngine.objects), \
	$(lib/libase.so), \
	$(BOOST_SYSTEM_LIBS) $(ASEDEPS_LIBS) $(ALSA_LIBS) -lzstd -ldl $>/lib/libsndfile.so, \
	../lib)
# Work around legacy code in external/websocketpp/*.hpp
ase/websocket.cc.FLAGS = -Wno-deprecated-dynamic-exception-spec -Wno-sign-promo
# Allow tests in mathutils.cc
ase/mathutils.cc.CTIDY_FLAGS = --checks=-clang-analyzer-security.FloatLoopCounter

# == jackdriver.so ==
lib/jackdriver.so	     ::= $>/lib/jackdriver.so
ase/jackdriver.objects	     ::= $(call BUILDDIR_O, $(ase/jackdriver.sources))
$(ase/jackdriver.objects):   $(ase/include.deps)
$(ase/jackdriver.objects):   EXTRA_INCLUDES ::= -I$>
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
$(ALL_TARGETS) += $(lib/jackdriver.so.MAYBE)

# == gtk2wrap.so ==
lib/gtk2wrap.so         ::= $>/lib/gtk2wrap.so
ase/gtk2wrap.objects    ::= $(call BUILDDIR_O, $(ase/gtk2wrap.sources))
$(ase/gtk2wrap.objects): EXTRA_INCLUDES ::= -I$> $(GTK2_CFLAGS)
$(ase/gtk2wrap.objects): EXTRA_CXXFLAGS ::= -Wno-deprecated -Wno-deprecated-declarations
$(ase/gtk2wrap.objects): $(ase/include.deps)
$(call BUILD_SHARED_LIB, \
	$(lib/gtk2wrap.so), \
	$(ase/gtk2wrap.objects), \
	$(lib/libase.so) | $>/lib/, \
	$(GTK2_LIBS), \
	../lib)
$(ALL_TARGETS) += $(lib/gtk2wrap.so)

# == install binaries ==
$(call INSTALL_BIN_RULE, $(basename $(lib/AnklangSynthEngine)), $(DESTDIR)$(pkgdir)/lib, $(wildcard \
	$(lib/AnklangSynthEngine)	\
	$(lib/AnklangSynthEngine)-fma	\
	$(lib/jackdriver.so.MAYBE)	\
	$(lib/gtk2wrap.so)		\
  ))

# == ase/lint ==
ase/lint:
	$(QGEN)
	$Q misc/synsmell.py $(wildcard ase/*.[hc] ase/*.*[hc] ase/*/*.*[hc] jsonipc/*.hh)
.PHONY: ase/lint
lint: ase/lint

# == Check Integrity Tests ==
check-ase-tests: $(lib/AnklangSynthEngine)
	$(eval xargs_parallel != P=`parallel --help 2>/dev/null` && \
	  [[ $$$$P =~ GNU.[Pp]arallel ]] && echo parallel || \
	  { echo xargs -n1; echo "$$$$0: missing 'GNU parallel', falling back to 'xargs'" >&2; } )
	$(QGEN)
	$Q : $(lib/AnklangSynthEngine) --check
	$Q set -Eeuo pipefail \
	&& $(lib/AnklangSynthEngine) --list-tests \
	|  $(xargs_parallel) $(lib/AnklangSynthEngine) --norc -P null -M null --test
CHECK_TARGETS += check-ase-tests
