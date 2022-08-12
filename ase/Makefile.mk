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
ase/include.deps		 += $>/external/rapidjson/rapidjson.h

# == AnklangSynthEngine definitions ==
lib/AnklangSynthEngine		::= $>/lib/AnklangSynthEngine
ase/AnklangSynthEngine.sources	::= ase/main.cc $(ase/libsources.cc) $(ase/libsources.c)
ase/AnklangSynthEngine.gensrc	::= $>/ase/api.jsonipc.cc
ase/AnklangSynthEngine.objects	::= $(call BUILDDIR_O, $(ase/AnklangSynthEngine.sources)) $(ase/AnklangSynthEngine.gensrc:.cc=.o) $(ase/tests/objects)
ase/AnklangSynthEngine.objects	 += $(devices/4ase.objects)
ALL_TARGETS += $(lib/AnklangSynthEngine)

# Work around legacy code in external/websocketpp/*.hpp
ase/websocket.cc.FLAGS = -Wno-deprecated-dynamic-exception-spec

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
$>/ase/api.jsonipc.cc: ase/api.hh jsonipc/cxxjip.py $(ase/include.deps) ase/Makefile.mk	| $>/ase/
	$(QGEN)
	$Q echo '#include <ase/jsonapi.hh>'							>  $@.tmp
	$Q echo '#include <ase/api.hh>'								>> $@.tmp
	$Q $(PYTHON3) jsonipc/cxxjip.py $< -N Ase -I. -I$>/ -Iout/external/			>> $@.tmp
	$Q echo '[[maybe_unused]] static bool init_jsonipc = (jsonipc_4_api_hh(), 0);'		>> $@.tmp
	$Q mv $@.tmp $@

# == ase/buildversion-$(version_m.m.m).cc ==
$>/ase/buildversion-$(version_m.m.m).cc: ase/Makefile.mk					| $>/ase/
	$(QGEN)
	$Q echo '// make $@'							> $@.tmp
	$Q echo '#include <ase/platform.hh>'					>>$@.tmp
	$Q echo 'namespace Ase {'						>>$@.tmp
	$Q echo 'const int         ase_major_version = $(version_major);'	>>$@.tmp
	$Q echo 'const int         ase_minor_version = $(version_minor);'	>>$@.tmp
	$Q echo 'const int         ase_micro_version = $(version_micro);'	>>$@.tmp
	$Q echo 'const char *const ase_version_long = "$(version_buildid) ($(INSN))";'	>>$@.tmp
	$Q echo 'const char *const ase_version_short = "$(version_short)";'	>>$@.tmp
	$Q echo 'const char *const ase_gettext_domain = "anklang-$(version_m.m.m)";' >>$@.tmp
	$Q echo '} // Ase'							>>$@.tmp
	$Q mv $@.tmp $@
ase/AnklangSynthEngine.objects += $>/ase/buildversion-$(version_m.m.m).o
# $>/ase/buildversion-$(version_m.m.m).o: $>/ase/buildversion-$(version_m.m.m).cc

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

# == external/minizip ==
$>/external/minizip/mz_zip.h: ase/Makefile.mk		| $>/external/
	@ $(eval H := 80d745e1c8caf6f81f6457403b0d9212e8a138b2badd6060e8a5da8583da2551)
	@ $(eval U := https://github.com/zlib-ng/minizip-ng/archive/refs/tags/2.9.0.tar.gz)
	@ $(eval T := minizip-ng-2.9.0.tar.gz)
	$(QECHO) FETCH "$U"
	$Q cd $>/external/ && rm -rf minizip* \
		$(call AND_DOWNLOAD_SHAURL, $H, $U, $T) && tar xf $T && rm $T
	$Q ln -s $(T:.tar.gz=) $>/external/minizip
	$Q test -e $@ && touch $@
$(wildcard ase/*.cc ase/*.c): $>/external/minizip/mz_zip.h

# == external/websocketpp ==
$>/external/websocketpp/server.hpp: ase/Makefile.mk	| $>/external/
	@ $(eval H := 6ce889d85ecdc2d8fa07408d6787e7352510750daa66b5ad44aacb47bea76755)
	@ $(eval U := https://github.com/zaphoyd/websocketpp/archive/0.8.2.tar.gz)
	@ $(eval T := websocketpp-0.8.2.tar.gz)
	$(QECHO) FETCH "$U"
	$Q cd $>/external/ && rm -rf websocketpp* \
		$(call AND_DOWNLOAD_SHAURL, $H, $U, $T) && tar xf $T && rm $T
	$Q ln -s $(T:.tar.gz=)/websocketpp $>/external/websocketpp
	$Q test -e $@ && touch $@
$>/external/websocketpp/config/asio_no_tls.hpp: $>/external/websocketpp/server.hpp
ase/websocket.cc: $>/external/websocketpp/config/asio_no_tls.hpp

# == external/rapidjson ==
$>/external/rapidjson/rapidjson.h: ase/Makefile.mk	| $>/external/
	@ $(eval H := b9290a9a6d444c8e049bd589ab804e0ccf2b05dc5984a19ed5ae75d090064806)
	@ $(eval U := https://github.com/Tencent/rapidjson/archive/232389d4f1012dddec4ef84861face2d2ba85709.tar.gz)
	@ $(eval T := rapidjson-232389d4f1012dddec4ef84861face2d2ba85709.tar.gz)
	$(QECHO) FETCH "$U"
	$Q cd $>/external/ && rm -rf rapidjson* \
		$(call AND_DOWNLOAD_SHAURL, $H, $U, $T) && tar xf $T && rm $T
	$Q ln -s $(T:.tar.gz=)/include/rapidjson $>/external/rapidjson
	$Q test -e $@ && touch $@
$(wildcard ase/*.cc): $>/external/rapidjson/rapidjson.h

# == external/clap ==
$>/external/clap/clap.h: ase/Makefile.mk		| $>/external/
	@ $(eval H := e99d297b9bca8dd71c4528f836840173e2415e2d5c800f0d475eed151924279d)
	@ $(eval U := https://github.com/free-audio/clap/archive/refs/tags/1.0.2.tar.gz)
	@ $(eval T := clap-1.0.2.tar.gz)
	$(QECHO) FETCH "$U"
	$Q cd $>/external/ && rm -rf clap* \
	     $(call AND_DOWNLOAD_SHAURL, $H, $U, $T) && tar xf $T && rm $T
	$Q ln -s $(T:.tar.gz=)/include/clap $>/external/clap
	$Q test -e $@ && touch $@
$(wildcard ase/clap*.cc): $>/external/clap/clap.h

# == AnklangSynthEngine ==
$(ase/AnklangSynthEngine.objects): $(ase/include.deps) $(ase/libase.deps)
$(ase/AnklangSynthEngine.objects): EXTRA_INCLUDES ::= -Iexternal/ -I$> -I$>/external/ $(GLIB_CFLAGS)
$(lib/AnklangSynthEngine):						| $>/lib/
$(call BUILD_PROGRAM, \
	$(lib/AnklangSynthEngine), \
	$(ase/AnklangSynthEngine.objects), \
	$(lib/libase.so), \
	$(BOOST_SYSTEM_LIBS) $(ASEDEPS_LIBS) $(ALSA_LIBS) -lzstd -ldl, \
	../lib)
# silence some websocketpp warnings
$(ase/AnklangSynthEngine.objects): EXTRA_CXXFLAGS ::= -Wno-sign-promo

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

# == Check Integrity Tests ==
check-ase-tests: $(lib/AnklangSynthEngine)
	$(QGEN)
	$Q $(lib/AnklangSynthEngine) --check
CHECK_TARGETS += check-ase-tests
