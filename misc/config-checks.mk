# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

# == System Utils ==
PERL			?= perl
PYTHON3 		?= python3
PKG_CONFIG		?= pkg-config
GDK_PIXBUF_CSOURCE	?= gdk-pixbuf-csource
IMAGEMAGICK_CONVERT	?= convert
PANDOC			?= pandoc
CP			?= cp --reflink=auto
.config.defaults	+= CP PERL PYTHON3 PKG_CONFIG GDK_PIXBUF_CSOURCE PANDOC IMAGEMAGICK_CONVERT
HAVE_PANDOC1		 = $(shell $(PANDOC) --version | grep -q '^pandoc 1\.' && echo 1)

ABSPATH_DLCACHE		?= $(abspath ./.dlcache)
npm_config_cache        ?= $(abspath ./.dlcache/npm)
export ABSPATH_DLCACHE npm_config_cache

INSTALL 		:= /usr/bin/install -c
INSTALL_DATA 		:= $(INSTALL) -m 644
RMDIR_P			:= rmdir -p >/dev/null 2>&1
MSGFMT			:= /usr/bin/msgfmt
MSGMERGE		:= /usr/bin/msgmerge
XGETTEXT		:= /usr/bin/xgettext
UPDATE_DESKTOP_DATABASE	:= /usr/bin/update-desktop-database
UPDATE_MIME_DATABASE	:= /usr/bin/update-mime-database

# Check for fast linker
ifeq ($(MODE),quick)
# Generally, ld.gold is faster than ld.bfd, and ld.lld is often faster than ld.gold for linking
# executables. But ld.lld 6.0.0 has a bug that causes deletion of [abi:cxx11] symbols in
# combination with certain --version-script uses: https://bugs.llvm.org/show_bug.cgi?id=36777
# So avoid ld.lld <= 6 if --version-script is used.
useld_gold		!= ld.gold --version 2>&1 | grep -q '^GNU gold' && echo '-fuse-ld=gold'
useld_lld		!= ld.lld --version 2>&1 | grep -q '^LLD ' && echo '-fuse-ld=lld'
useld_fast		:= $(or $(useld_lld), $(useld_gold))
useld_lld+vs		!= ld.lld --version 2>&1 | grep -v '^LLD [0123456]\.' | grep -q '^LLD ' && echo '-fuse-ld=lld'
useld_fast+vs		:= $(or $(useld_lld+vs), $(useld_gold))
else
useld_fast		::= # keep default linker
useld_fast+vs		::= # keep default linker
# Keep the default linker for production mode, as usually, bfd optimizes better than lld,
# and lld optimizes better than gold in terms of resulting binary size.
endif

# == Cache downloads in ABSPATH_DLCACHE ==
# $(call AND_DOWNLOAD_SHAURL, sha256sum, url, filename) - Download and cache file via `url`, verify `sha256sum`
AND_DOWNLOAD_SHAURL = && ( : \
	&& ( test ! -e "$(ABSPATH_DLCACHE)/$(strip $(or $3,$(notdir $2)))" || $(CP) "$(ABSPATH_DLCACHE)/$(strip $(or $3,$(notdir $2)))" . ) \
	&& ( echo "$(strip $1) $(or $3,$(notdir $2))" | sha256sum -c - >/dev/null 2>&1 || curl -sfSL "$(strip $2)" -o "$(strip $(or $3,$(notdir $2)))" ) \
	&& ( echo "$(strip $1) $(or $3,$(notdir $2))" | sha256sum -c - || { echo "sha256sum $$PWD/$(strip $3)" && false; }) \
	&& ( test ! -x "$(ABSPATH_DLCACHE)/" \
	   || ( mkdir -p "$(ABSPATH_DLCACHE)/" \
	      && ( cmp -s "$(strip $(or $3,$(notdir $2)))" "$(ABSPATH_DLCACHE)/$(strip $(or $3,$(notdir $2)))" \
		 || $(CP) "$(strip $(or $3,$(notdir $2)))" "$(ABSPATH_DLCACHE)/" ) ) ) )

# == conftest_lib & conftest_require_lib ==
# $(call conftest_lib, header, symbol, lib) -> $CONFTEST
conftest_lib = { { echo '\#include <$(strip $1)>' \
                && echo 'int main() { return 0 == (int) (long) (void*) &($2); }' ; } > "$>/conftest_lib-$$$$.cc" \
		&& { CONFTEST_LOG=$$($(CXX) -fpermissive "$>/conftest_lib-$$$$.cc" -o "$>/conftest_lib-$$$$" $(LDFLAGS) $3 $(LDLIBS) 2>&1) \
		     && CONFTEST=true || CONFTEST=false ; } \
		&& rm -f "$>/conftest_lib-$$$$.cc" "$>/conftest_lib-$$$$" ; }
conftest_lib.makefile ::= $(lastword $(MAKEFILE_LIST))
# $(call conftest_require_lib, header, symbol, lib) -> errors if $CONFTEST != true
conftest_require_lib = { $(call conftest_lib,$1,$2,$3) && $$CONFTEST \
	|| { echo "$$CONFTEST_LOG" | sed 's/^/> /'; \
	     echo '$(conftest_lib.makefile):' "ERROR: Failed to link with '$3' against symbol:" '$2'; \
	     exit 7; } >&2 ; }

# == config-checks.requirements ==
config-checks.require.pkgconfig ::= $(strip	\
	alsa			>= 1.0.5	\
	flac			>= 1.2.1	\
	ogg			>= 1.3.4	\
	opus			>= 1.3.1	\
	glib-2.0        	>= 2.32.3	\
	gmodule-no-export-2.0	>= 2.32.3	\
	gthread-2.0		>= 2.32.3	\
	gobject-2.0		>= 2.32.3	\
	dbus-1			>= 1.12.16	\
	lilv-0			>= 0.24.0	\
)
# boost libraries have no .pc files
# Unused: fluidsynth		>= 2.0.5

# == pkg-config variables ==
# use for Gtk+2 X11 Window embedding
GTK2_PACKAGES	 ::= gtk+-2.0 suil-0
# used for ASEDEPS_CFLAGS ASEDEPS_LIBS
ASEDEPS_PACKAGES ::= ogg opus flac zlib dbus-1 lilv-0 suil-0 \
		     glib-2.0 gobject-2.0 gmodule-no-export-2.0
# used for ANKLANG_JACK_LIBS
ANKLANGDEP_JACK  ::= jack >= 0.125.0

# == config-cache.mk ==
# Note, using '-include config-cache.mk' will ignore errors during config-cache.mk creation,
# e.g. due to missing pkg-config requirements. So we have to use 'include config-cache.mk'
# and then need to work around the file not existing initially by creating a dummy and
# forcing a remake via 'FORCE'.
ifeq ('','$(wildcard $>/config-cache.mk)')
  $(shell mkdir -p $>/ && echo '$>/config-cache.mk: FORCE' > $>/config-cache.mk)
endif
include $>/config-cache.mk
# Rule for the actual checks:
$>/config-cache.mk: misc/config-checks.mk misc/version.sh $(GITCOMMITDEPS) | $>/./
	$(QECHO) CHECK    Configuration dependencies...
	$Q $(PKG_CONFIG) --exists --print-errors '$(config-checks.require.pkgconfig)'
	$Q $(IMAGEMAGICK_CONVERT) --version 2>&1 | grep -q 'Version:.*\bImageMagick\b' \
	  || { echo "$@: failed to detect ImageMagick convert: $(IMAGEMAGICK_CONVERT)" >&2 ; false ; }
	$Q $(PANDOC) --version 2>&1 | grep -q '\bpandoc\b' \
	  || { echo "$@: failed to detect pandoc: $(PANDOC)" >&2 ; false ; }
	$(QGEN)
	$Q echo '# make $@'					> $@.tmp
	$Q echo "ANKLANG_GETTEXT_DOMAIN ::=" \
		'anklang-$$(version_short)'			>>$@.tmp
	$Q bun --version 2>&1 | grep -qE '^([1-9]+[0-9]*)\.[0-9]+\.[0-9]+$$' \
	  && echo 'XNPM ::= bun'				>>$@.tmp \
	  || { pnpm --version 2>&1 | grep -qE '^([89]|[1-9]+[0-9]+)\.[0-9]+\.[0-9]+$$' \
	       && echo 'XNPM ::= pnpm'				>>$@.tmp ; } \
	  || echo 'XNPM ::= npm'				>>$@.tmp
	$Q GTK2_CFLAGS=$$($(PKG_CONFIG) --cflags $(GTK2_PACKAGES)) \
	  && echo "GTK2_CFLAGS ::= $$GTK2_CFLAGS"		>>$@.tmp
	$Q GTK2_LIBS=$$($(PKG_CONFIG) --libs $(GTK2_PACKAGES)) \
	  && echo "GTK2_LIBS ::= $$GTK2_LIBS"			>>$@.tmp
	$Q ASEDEPS_CFLAGS=$$($(PKG_CONFIG) --cflags $(ASEDEPS_PACKAGES)) \
	  && echo "ASEDEPS_CFLAGS ::= $$ASEDEPS_CFLAGS"		>>$@.tmp
	$Q ASEDEPS_LIBS=$$($(PKG_CONFIG) --libs $(ASEDEPS_PACKAGES)) \
	  && echo "ASEDEPS_LIBS ::= $$ASEDEPS_LIBS"		>>$@.tmp
	$Q ALSA_LIBS='-lasound' \
	  && echo "ALSA_LIBS ::= $$ALSA_LIBS"			>>$@.tmp \
	  && $(call conftest_require_lib, alsa/asoundlib.h, snd_asoundlib_version, $$ALSA_LIBS)
	$Q ANKLANG_JACK_LIBS=$$($(PKG_CONFIG) --libs '$(ANKLANGDEP_JACK)' 2>/dev/null) \
	  && echo "ANKLANG_JACK_LIBS ::= $$ANKLANG_JACK_LIBS"		>>$@.tmp \
	  || echo "ANKLANG_JACK_LIBS ::= # missing: $(ANKLANGDEP_JACK)" >>$@.tmp
	$Q BOOST_SYSTEM_LIBS='-lboost_system' \
	  && echo "BOOST_SYSTEM_LIBS ::= $$BOOST_SYSTEM_LIBS"	>>$@.tmp \
	  && $(call conftest_require_lib, boost/system/error_code.hpp, boost::system::system_category, $$BOOST_SYSTEM_LIBS)
	$Q echo 'config-stamps ::= $$>/config-stamps.sha256'	>>$@.tmp \
	  && OLDSUM=$$(cat "$>/config-stamps.sha256" 2>/dev/null || :) \
	  && TMPSUM=$$(sha256sum < $@.tmp) && CFGSUM="$${TMPSUM%-}$(@F)" \
	  && (test "$$OLDSUM" = "$$CFGSUM" \
	      &&   echo '  KEEP     $>/config-stamps.sha256' \
	      || { echo '  UPDATE   $>/config-stamps.sha256' \
		   && echo 'all $${MAKECMDGOALS}:'						> $>/GNUmakefile \
		   && echo '	@ exec $$(MAKE) -C '\''$(abspath .)'\'' $${MAKECMDGOALS}'	>>$>/GNUmakefile \
		   && echo '.PHONY: all'							>>$>/GNUmakefile \
		   && echo "$$CFGSUM" > $>/config-stamps.sha256 \
		   ; } )
	$Q mv $>/config-cache.mk $>/config-cache.old 2>/dev/null || true
	$Q mv $@.tmp $@
$>/config-stamps.sha256: $>/config-cache.mk
CLEANFILES += $>/config-stamps.sha256 $>/config-cache.mk $>/config-cache.old
# About config-stamps.sha256: For a variety of reasons, config-cache.mk may be
# often regenerated. To efficiently detect changes in the build configuration,
# use $(config-stamps) as dependency.
# Note, consider variables defined in config-cache.mk as stale within the recipe
# for config-cache.mk. I.e. use "$$ALSA_LIBS" instead of "$(ALSA_LIBS)" to avoid
# 2-phase regeneration of config-cache.mk that trips up config-stamps.sha256.
config-calc-hash = $(firstword $(shell echo ' $(foreach V, $(.config.defaults) $(config-hash-vars), $($V)) ' | sha256sum))
# About config-calc-hash: though a bit costly, comparing the config-calc-hash
# result to previous runs is a simple test to check if the configuration between
# 'make' invocations has been changed through command line variables.
config-calc-hash.dep = $$(if $$(filter $$(config-calc-hash), $(config-calc-hash)), , FORCE)
# About config-calc-hash.dep: storing $(config-calc-hash.dep) in a .d file will
# evaluate to 'FORCE' whenever $(config-calc-hash) changes.
