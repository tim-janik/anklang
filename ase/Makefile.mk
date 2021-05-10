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

# == ase/sysconfig.h ==
$>/ase/sysconfig.h: $(config-stamps) $>/ase/sysconfig2.h $>/ase/sysconfig3.h | $>/ase/ # ase/Makefile.mk
	$(QGEN)
	$Q echo '// make $@'							> $@.tmp
	$Q echo '#define ASE_MAJOR_VERSION		($(version_major))'	>>$@.tmp
	$Q echo '#define ASE_MINOR_VERSION		($(version_minor))'	>>$@.tmp
	$Q echo '#define ASE_MICRO_VERSION		($(version_micro))'	>>$@.tmp
	$Q echo '#define ASE_VERSION_STRING		"$(version_short)"'	>>$@.tmp
	$Q echo '#define ASE_GETTEXT_DOMAIN		"$(ASE_GETTEXT_DOMAIN)"'>>$@.tmp
	$Q echo '#define ASE_VORBISFILE_BAD_SEEK 	$(VORBISFILE_BAD_SEEK)'	>>$@.tmp
	$Q cat $>/ase/sysconfig2.h $>/ase/sysconfig3.h	>>$@.tmp
	$Q mv $@.tmp $@

# == ase/sysconfig2 ==
$>/ase/sysconfig2.h: $(config-stamps) ase/Makefile.mk	| $>/ase/
	$(QGEN)
	$Q : $(file > $>/ase/conftest_spinlock.c, $(ase/conftest_spinlock.c)) \
	&& $(CC) -Wall $>/ase/conftest_spinlock.c -pthread -o $>/ase/conftest_spinlock \
	&& (cd $> && ./ase/conftest_spinlock) \
	&& echo '#define ASE_SPINLOCK_INITIALIZER' "	$$(cat $>/ase/conftest_spinlock.txt)" > $@ \
	&& rm $>/ase/conftest_spinlock.c $>/ase/conftest_spinlock $>/ase/conftest_spinlock.txt
# ase/conftest_spinlock.c
define ase/conftest_spinlock.c
#include <stdio.h>
#include <string.h>
#include <pthread.h>
struct Spin { pthread_spinlock_t dummy1, s1, dummy2, s2, dummy3; };
int main (int argc, char *argv[]) {
  struct Spin spin;
  memset (&spin, 0xffffffff, sizeof (spin));
  if (pthread_spin_init (&spin.s1, 0) == 0 && pthread_spin_init (&spin.s2, 0) == 0 &&
      sizeof (pthread_spinlock_t) == 4 && spin.s1 == spin.s2)
    { // # sizeof==4 and location-independence are current implementation assumption
      FILE *f = fopen ("ase/conftest_spinlock.txt", "w");
      fprintf (f, "/*{*/ 0x%04x, /*}*/\n", *(int*) &spin.s1);
      fclose (f);
    }
  return 0;
}
endef

# == ase/sysconfig3 ==
$>/ase/sysconfig3.h: $(config-stamps) ase/Makefile.mk	| $>/ase/
	$(QGEN)
	$Q : $(file > $>/ase/conftest_pollval.c, $(ase/conftest_pollval.c)) \
	&& $(CC) -Wall $>/ase/conftest_pollval.c -pthread -o $>/ase/conftest_pollval \
	&& (cd $> && ./ase/conftest_pollval) \
	&& echo -e '#define ASE_SYSVAL_POLLINIT\t\t((const uint32_t[])' "$$(cat $>/ase/conftest_pollval.txt))" > $@ \
	&& rm $>/ase/conftest_pollval.c $>/ase/conftest_pollval $>/ase/conftest_pollval.txt
# ase/conftest_pollval.c
define ase/conftest_pollval.c
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <poll.h>
int main (int argc, char *argv[]) {
  FILE *f = fopen ("ase/conftest_pollval.txt", "w");
  fprintf (f, "{ 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x }\n",
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
	$(BOOST_SYSTEM_LIBS) $(ASEDEPS_LIBS) $(ALSA_LIBS), \
	../lib)
#	-lase-$(version_major)
$(call INSTALL_BIN_RULE, $(basename $(lib/AnklangSynthEngine)), $(DESTDIR)$(pkglibdir)/lib, $(lib/AnklangSynthEngine))

# == Check Integrity Tests ==
check-ase-tests: $(lib/AnklangSynthEngine)
	$(QGEN)
	$Q $(lib/AnklangSynthEngine) --check-integrity-tests
CHECK_TARGETS += check-ase-tests
