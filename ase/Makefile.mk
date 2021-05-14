# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ase/*.d)
CLEANDIRS     += $(wildcard $>/ase/ $>/lib/)

include ase/tests/Makefile.mk

# == ase/ *.cc file sets ==
ase/AnklangSynthEngine.main.cc	::= ase/main.cc
ase/nonlib.cc			::= $(ase/AnklangSynthEngine.main.cc)
ase/libsources.cc		::= $(filter-out $(ase/nonlib.cc), $(wildcard ase/*.cc))
ase/libsources.c		::= $(wildcard ase/*.c)

# == AnklangSynthEngine definitions ==
lib/AnklangSynthEngine		::= $>/lib/AnklangSynthEngine-$(version_m.m.m)
ase/AnklangSynthEngine.sources	::= $(ase/AnklangSynthEngine.main.cc) $(ase/libsources.cc) $(ase/libsources.c)
ase/AnklangSynthEngine.gensrc	::= $>/ase/api.jsonipc.cc
ase/AnklangSynthEngine.deps	::= $>/ase/sysconfig.h
ase/AnklangSynthEngine.deps	 += $>/external/websocketpp/server.hpp $>/external/rapidjson/rapidjson.h
ase/AnklangSynthEngine.objects	::= $(call BUILDDIR_O, $(ase/AnklangSynthEngine.sources)) $(ase/AnklangSynthEngine.gensrc:.cc=.o) $(ase/tests/objects)
ase/AnklangSynthEngine.objects	 += $(devices/4ase.objects)
ALL_TARGETS += $(lib/AnklangSynthEngine)
# $(wildcard $>/ase/*.d): $>/external/rapidjson/rapidjson.h # fix deps on rapidjson/ internal headers

# Work around legacy code in external/websocketpp/*.hpp
ase/websocket.cc.FLAGS = -Wno-deprecated-dynamic-exception-spec

# == ase/api.jsonipc.cc ==
$>/ase/api.jsonipc.cc: ase/api.hh jsonipc/cxxjip.py $(ase/AnklangSynthEngine.deps) ase/Makefile.mk	| $>/ase/
	$(QGEN)
	$Q echo '#include <ase/jsonapi.hh>'							>  $@.tmp
	$Q echo '#include <ase/api.hh>'								>> $@.tmp
	$Q $(PYTHON3) jsonipc/cxxjip.py $< -N Ase -I. -I$>/ -Iout/external/			>> $@.tmp
	$Q echo '[[maybe_unused]] static bool init_jsonipc = (jsonipc_4_api_hh(), 0);'		>> $@.tmp
	$Q mv $@.tmp $@

# == ase/buildversion.cc ==
$>/ase/buildversion.cc: ase/Makefile.mk					| $>/ase/
	$(QGEN)
	$Q echo '// make $@'							> $@.tmp
	$Q echo '#include <ase/platform.hh>'					>>$@.tmp
	$Q echo 'namespace Ase {'						>>$@.tmp
	$Q echo 'const int         ase_major_version = $(version_major);'	>>$@.tmp
	$Q echo 'const int         ase_minor_version = $(version_minor);'	>>$@.tmp
	$Q echo 'const int         ase_micro_version = $(version_micro);'	>>$@.tmp
	$Q echo 'const char *const ase_short_version = "$(version_short)";'	>>$@.tmp
	$Q echo 'const char *const ase_gettext_domain = "anklang-$(version_m.m.m)";'	>>$@.tmp
	$Q echo '} // Ase'							>>$@.tmp
	$Q mv $@.tmp $@
ase/AnklangSynthEngine.objects += $>/ase/buildversion.o
# $>/ase/buildversion.o: $>/ase/buildversion.cc

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

# == AnklangSynthEngine ==
$(ase/AnklangSynthEngine.objects): $(ase/AnklangSynthEngine.deps) $(ase/libase.deps)
$(ase/AnklangSynthEngine.objects): EXTRA_INCLUDES ::= -Iexternal/ -I$> -I$>/external/ $(GLIB_CFLAGS)
$(ase/AnklangSynthEngine.objects): EXTRA_FLAGS ::= -Wno-sign-promo
$(lib/AnklangSynthEngine):						| $>/lib/
$(call BUILD_PROGRAM, \
	$(lib/AnklangSynthEngine), \
	$(ase/AnklangSynthEngine.objects), \
	$(lib/libase.so), \
	$(BOOST_SYSTEM_LIBS) $(ASEDEPS_LIBS) $(ALSA_LIBS) -lzstd, \
	../lib)
#	-lase-$(version_major)
$(call INSTALL_BIN_RULE, $(basename $(lib/AnklangSynthEngine)), $(DESTDIR)$(pkglibdir)/lib, $(lib/AnklangSynthEngine))

# == Check Integrity Tests ==
check-ase-tests: $(lib/AnklangSynthEngine)
	$(QGEN)
	$Q $(lib/AnklangSynthEngine) --check-integrity-tests
CHECK_TARGETS += check-ase-tests
