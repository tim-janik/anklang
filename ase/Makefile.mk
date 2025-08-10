# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ase/*.d)

# == ase/ *.cc file sets ==
ase/AnklangSynthEngine.sources	::= ase/main.cc
ase/jackdriver.sources		::= ase/driver-jack.cc
ase/gtk2wrap.sources		::= ase/gtk2wrap.cc
ase/noglob.sources		::= $(ase/AnklangSynthEngine.sources) $(ase/gtk2wrap.sources) $(ase/jackdriver.sources)
ase/common.ccsources		::= $(filter-out $(ase/noglob.sources), $(wildcard ase/*.cc))
ase/common.csources		::= $(wildcard ase/*.c)
ase/generated.sources		:=
ase/object.deps			:=
ase/sysconfig.dep		::= $>/ase/sysconfig.h
ASE_EXTERNAL_INCLUDES := $(strip		\
	-Iexternal/clap/include			\
	-Iexternal/libsndfile/include		\
	-Iexternal/liquidsfz/lib		\
	-Iexternal/nlohmann-json/include	\
	-Iexternal/pandaresampler/include	\
	-Iexternal/rapidjson/include		\
	-Iexternal/websocketpp			\
) # also used by clang-tidy
ase/object.includes		::= $(ASE_EXTERNAL_INCLUDES) -I$> -I$>/external/ $(ASEDEPS_CFLAGS)

# == ase/gen/api-jsonipc.g.cc ==
$>/codegen/ase/gen/api-jsonipc.g.cc: ase/api.hh jsonipc/cxxjip.py ase/Makefile.mk
	$(QGEN)
	$Q echo '// Generated file, inputs: $^'						>  $@.tmp
	$Q echo '#include <ase/jsonapi.hh>'						>> $@.tmp
	$Q echo '#include <ase/api.hh>'							>> $@.tmp
	$Q $(PYTHON3) jsonipc/cxxjip.py $< -N Ase -I. -I$>/ -Iout/external/		>> $@.tmp
	$Q echo '[[maybe_unused]] static bool init_jsonipc = (jsonipc_4_api_hh(), 0);'	>> $@.tmp
	$Q mv $@.tmp $@
CODEGEN.FILES += ase/gen/api-jsonipc.g.cc

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
$>/lib/libsndfile.so: $(EXTERNAL_CXX_STAMPS)	| $>/lib/
	$(QGEN)
	$Q cmake \
		-B $>/sndfile/ -S external/libsndfile/ -DCMAKE_BUILD_TYPE=MINSIZEREL \
		-DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=ON
	$Q $(MAKE) -C $>/sndfile/
	$Q $(CP) -P $>/sndfile/libsndfile.so* $>/lib/
ase/sndfile.cc: $>/lib/libsndfile.so # includes $>/sndfile/src/config.h

# == Common Sources ==
ase/common.sources	::= $(ase/common.ccsources) $(ase/common.csources)
ase/generated.sources	+= $(strip	\
	$>/ase/blake3impl.c		\
	$>/ase/blake3avx512.c		\
	$>/ase/blake3avx2.c		\
	$>/ase/blake3sse41.c		\
	$>/ase/blake3sse2.c		\
)

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
ase/generated.sources += $(ase/buildversion.cc)

# == object deps ==
ase/object.deps	+= $(ase/sysconfig.dep) $(ase/libase.deps) $(EXTERNAL_CXX_STAMPS)
ase/common.objects := $(sort \
	$(call BUILDDIR_O, $(ase/common.sources)) \
	$(call SOURCE2_O, $(ase/generated.sources))   \
)
$(ase/common.objects): $(ase/object.deps)
$(ase/common.objects): EXTRA_INCLUDES ::= $(ase/object.includes)
$(devices/4ase.objects): $(ase/object.deps)
$(devices/4ase.objects): EXTRA_INCLUDES ::= $(ase/object.includes)
# Work around legacy code in external/websocketpp/*.hpp
ase/websocket.cc.FLAGS = -Wno-deprecated-dynamic-exception-spec -Wno-sign-promo
# Allow tests in mathutils.cc
ase/mathutils.cc.CTIDY_FLAGS = --checks=-clang-analyzer-security.FloatLoopCounter

# == AnklangSynthEngine ==
lib/AnklangSynthEngine		::= $>/lib/AnklangSynthEngine
ase/AnklangSynthEngine.objects	::= $(call BUILDDIR_O, $(ase/AnklangSynthEngine.sources))
$(ase/AnklangSynthEngine.objects): $(ase/object.deps)
$(ase/AnklangSynthEngine.objects): EXTRA_INCLUDES ::= $(ase/object.includes)
ase/AnklangSynthEngine.objects	 += $(ase/common.objects) $(devices/4ase.objects)
$(lib/AnklangSynthEngine): $>/lib/libsndfile.so					| $>/lib/
$(call BUILD_PROGRAM, \
	$(lib/AnklangSynthEngine), \
	$(ase/AnklangSynthEngine.objects), \
	$(lib/libase.so), \
	$(BOOST_SYSTEM_LIBS) $(ASEDEPS_LIBS) $(ALSA_LIBS) -lzstd -ldl $>/lib/libsndfile.so, \
	../lib)
ALL_TARGETS += $(lib/AnklangSynthEngine)

# == jackdriver.so ==
lib/jackdriver.so	     ::= $>/lib/jackdriver.so
ase/jackdriver.objects	     ::= $(call BUILDDIR_O, $(ase/jackdriver.sources))
$(ase/jackdriver.objects):   $(ase/sysconfig.dep)
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
$(ase/gtk2wrap.objects): $(ase/sysconfig.dep)
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
	$(lib/jackdriver.so.MAYBE)	\
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
