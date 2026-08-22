# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

# == MAKE setup ==
all:		# Default Rule
MAKEFLAGS      += -r
SHELL         ::= /bin/bash -o pipefail
RUNTS		?= node --experimental-strip-types
PARALLEL_MAKE   = $(filter JOBSERVER, $(subst -j, JOBSERVER , $(MFLAGS)))
LATE_EVAL	:=
CODEGEN.FILES	:=
WITH_CODEGEN	?= $(filter codegen,$(MAKECMDGOALS))

S ::= # Variable containing 1 space
S +=

# == Version ==
version_full    != misc/version.sh
version_long	:= $(word 1, $(version_full))
version_short	:= $(word 1, $(subst +, , $(version_long)))
version_hash	:= $(word 2, $(version_full))
version_date	:= $(wordlist 3, 999, $(version_full))
version_bits	:= $(subst _, , $(subst -, , $(subst ., , $(version_short))))
version_major	:= $(word 1, $(version_bits))
version_minor	:= $(word 2, $(version_bits))
version_micro	:= $(word 3, $(version_bits))
version_to_month = $(shell echo "$(version_date)" | sed -r -e 's/^([2-9][0-9][0-9][0-9])-([0-9][0-9])-.*/m\2 \1/' \
		     -e 's/m01/January/ ; s/m02/February/ ; s/m03/March/ ; s/m04/April/ ; s/m05/May/ ; s/m06/June/' \
		     -e 's/m07/July/ ; s/m08/August/ ; s/m09/September/ ; s/m10/October/ ; s/m11/November/ ; s/m12/December/')
version-info:
	@echo version_full: $(version_full)
	@echo version_long: $(version_long)
	@echo version_short: $(version_short)
	@echo version_hash: $(version_hash)
	@echo version_date: $(version_date)
	@echo version_major: $(version_major)
	@echo version_minor: $(version_minor)
	@echo version_micro: $(version_micro)
	@echo version_to_month: "$(version_to_month)"
ifeq ($(version_micro),)	# do we have any version?
$(error Missing version information, run: misc/version.sh)
endif

# == User Defaults ==
# see also 'make default' rule
-include config-defaults.mk

# == builddir ==
# Allow O= and builddir= on the command line
ifeq ("$(origin O)", "command line")
builddir	::= $O
builddir.origin ::= command line
endif
ifeq ('$(builddir)', '')
builddir	::= out
endif
# Provide $> as builddir shorthand, used in almost every rule
>		  = $(builddir)
# if 'realpath --relative-to' is missing, os.path.realpath could be used as fallback
build2srcdir	 != realpath --relative-to $(builddir) .

# == Mode ==
# determine build mode
MODE.origin ::= $(origin MODE) # before overriding, remember if MODE came from command line
override MODE !=  case "$(MODE)" in \
		    p*|pr*|pro*|prod*|produ*|produc*|product*)		MODE=production ;; \
		    producti*|productio*|production)			MODE=production ;; \
		    r*|re*|rel*|rele*|relea*|releas*|release)		MODE=production ;; \
		    cppt*|cpptr*|cpptra*|cpptrac*|cpptrace)		MODE=cpptrace ;; \
		    dev*|deve*|devel*|develo*|develop*|developm*)	MODE=devel ;; \
		    developme*|developmen*|development)			MODE=devel ;; \
		    d*|de*|deb*|debu*|debug|dbg)			MODE=debug ;; \
		    u*|ub*|ubs*|ubsa*|ubsan)				MODE=ubsan ;; \
		    a*|as*|asa*|asan)					MODE=asan ;; \
		    t*|ts*|tsa*|tsan)					MODE=tsan ;; \
		    l*|ls*|lsa*|lsan)					MODE=lsan ;; \
		    q*|qu*|qui*|quic*|quick)				MODE=quick ;; \
		    *)							MODE=production ;; \
		  esac ; echo "$$MODE"
.config.defaults += MODE
$(info $S  MODE     $(MODE))
.config.defaults += INSN
__DEV__		 := $(if $(filter production,$(MODE)),false,true)

# == Dirctories ==
prefix		 ?= /usr/local
bindir		 ?= $(prefix)/bin
sharedir 	 ?= $(prefix)/share
mandir		 ?= $(sharedir)/man
docdir		 ?= $(sharedir)/doc
libdir		 ?= $(prefix)/lib
pkgprefix	 ?= $(libdir)
pkgdir		 ?= $(pkgprefix)/anklang-$(version_major)-$(version_minor)
pkgsharedir	 ?= $(pkgdir)/share
.config.defaults += prefix bindir sharedir mandir docdir libdir pkgprefix pkgdir
pkgdocdir	 ?= $(pkgdir)/doc
pkgmediadir	 ?= $(pkgdir)/media
ifneq (1,$(words [$(prefix)])) # guard against `prefix := / / ` etc which will confuse `rm -rf`
$(error Variable `prefix` must expand to a single directory name, current value: `prefix:=$(prefix)`)
endif

# == Target Collections ==
ALL_TARGETS	::=
CXX_TARGETS	::=
LATE_TARGETS	::=
ALL_TESTS	::=
CHECK_TARGETS	::=
CLEANFILES	::=
CLEANDIRS	::=
MAKE_HELP	:=
NONBUILD_RULES	:= clean help

# == Defaults ==
INCLUDES	::= -I.
DEFS		::=

# == Compiler Setup ==
CXXSTD		::= -std=gnu++23 -pthread -pipe
CSTD		::= -std=gnu11 -pthread -pipe
EXTRA_DEFS	::= # target private defs, lesser precedence than CXXFLAGS
EXTRA_INCLUDES	::= # target private defs, lesser precedence than CXXFLAGS
EXTRA_FLAGS	::= # target private flags, precedence over CXXFLAGS
CLANG_TIDY	 ?= clang-tidy

# == Dependency Checks & Build Config ==
include misc/config-utils.mk
include misc/config-uname.mk
include misc/config-checks.mk
include external/Makefile.mk

NPM_INSTALL ?= $(XNPM) install
.config.defaults += CC CFLAGS CXX CLANG_TIDY CXXFLAGS LDFLAGS LDLIBS NPM_INSTALL

ifdef CCACHE	# sloppiness required for pre-compiled headers
export CCACHE_SLOPPINESS := $(CCACHE_SLOPPINESS),pch_defines,time_macros
endif

# == WILDCARD_FILES ==
# WILDCARD_SUBDIRS contains repository subdirs for source files, and
# WILDCARD_FILES lists all source files not vendored from external sources
WILDCARD_SUBDIRS := .github/ ase/ devices/ doc/ electron/ images/ jsonipc/ misc/ rand/ ui/ x11test/ # external/ trkn/
WILDCARD_SUBDIRS := $(wildcard $(WILDCARD_SUBDIRS) $(WILDCARD_SUBDIRS:%=%*/) $(WILDCARD_SUBDIRS:%=%*/*/) $(WILDCARD_SUBDIRS:%=%*/*/*/))
WILDCARD_IGNORE  := $(wildcard *tmp* *LOG* TODO* *lock* config-defaults.mk TAGS .jj .git .cache trkn/*all.hh trkn/x*.hh *.gz)
WILDCARD_IGNORE  += .#% #% %~   # ignore Emacs lock/auto-save/backup files (also fixes shell '#' comment hazard in recipes)
WILDCARD_FILES   := $(filter-out $(WILDCARD_IGNORE), $(wildcard external/Makefile.mk trkn/* $(WILDCARD_SUBDIRS:%=%.[^.]*) $(WILDCARD_SUBDIRS:%=%*) .[^.]* *)) # glob
WILDCARD_FILES   := $(filter-out $(patsubst %/./,%, $(wildcard $(WILDCARD_FILES:%=%/./))), $(WILDCARD_FILES)) # remove subdir names
WILDCARD_FILES   := $(sort $(WILDCARD_FILES))
WILDCARD_FILES:
	@echo $(WILDCARD_FILES)
	@echo WILDCARD_IGNORE:  $(WILDCARD_IGNORE)
	@echo XDIRS: $(wildcard trkn/3rd_party/./ trkn/config.hh/./ )
check-WILDCARD_FILES:
	$(QCHECK)
	$Q echo $(WILDCARD_FILES) | tr ' ' '\n' | sort > $@.tmpfind
	@ # List files via jj/git, validate that WILDCARD_FILES contains a superset
	$Q rm -f $@.tmprepo ; \
	   if   jj workspace root >/dev/null 2>&1 ; \
	   then jj --no-pager file list | sort > $@.tmprepo ; \
	   elif git rev-parse --is-inside-work-tree >/dev/null 2>&1 ; \
	   then git ls-tree -r --name-only HEAD | sort > $@.tmprepo ; \
	   fi
	$Q test -r $@.tmprepo || exit 0; grep -vE '^trkn/.*/' < $@.tmprepo > $@.tmpfilt ; mv $@.tmpfilt $@.tmprepo
	$Q test -r $@.tmprepo && grep -vFxf $@.tmpfind $@.tmprepo || exit 0 && \
	     ( echo "ERROR: WILDCARD_FILES misses some files tracked by Git" ; false ) >&2
	$Q test -r $@.tmprepo && grep -vFxf $@.tmprepo $@.tmpfind || exit 0 && echo "WARNING: WILDCARD_FILES contains files not tracked by Git" >&2
	$Q rm -f $@.tmprepo $@.tmpfilt $@.tmpfind
.PHONY: WILDCARD_FILES check-WILDCARD_FILES
check: check-WILDCARD_FILES
# WILDCARD_FILES

# == enduser targets ==
.PHONY: all codegen check check-audio install uninstall installcheck lint strict

# == subdirs ==
include devices/Makefile.mk
include ase/Makefile.mk
include images/knobs/Makefile.mk
include ui/Makefile.mk
include electron/Makefile.mk
include misc/Makefile.mk
include doc/Makefile.mk

# == run ==
run: FORCE all
	export ELECTRON_ENABLE_LOGGING=1 && \
	  $>/lib/AnklangSynthEngine

# == clean rules ==
clean: FORCE
	rm -fr $(builddir)/* $(builddir)/.[^.]* $(builddir)/..?* $(CLEANDIRS)
	rm -f $(CLEANFILES)
CLEANDIRS += .cache poxy/ html/ assets/

# == 'default' settings ==
# Allow value defaults to be adjusted via: make default builddir=... CXX=...
default: FORCE
	$(QECHO) WRITE config-defaults.mk
	$Q echo -e '# make $@\n'					> $@.tmp
	$Q echo 'builddir = $(builddir)'				>>$@.tmp
	$Q : $(foreach VAR, $(.config.defaults), &&					\
	  if $(if $(filter command, $(origin $(VAR)) $($(VAR).origin)),			\
		true, false) ; then							\
	    echo '$(VAR) = $(value $(VAR))'				>>$@.tmp ;	\
	  elif ! grep -sEm1 '^\s*$(VAR)\s*:?[:!?]?=' config-defaults.mk	>>$@.tmp ; then	\
	    echo '# $(VAR) = $(value $(VAR))'				>>$@.tmp ;	\
	  fi )
	$Q mv $@.tmp config-defaults.mk

# == output directory rules ==
# rule to create output directories from order only dependencies, trailing slash required
$>/: ; $Q mkdir -p $@
$>/%/: ; $Q mkdir -p $@
.PRECIOUS: $>/%/ # prevent MAKE's 'rm ...' for automatically created dirs

# == FORCE rules ==
# Use FORCE to mark phony targets via a dependency
.PHONY:	FORCE

# == make codegen ==
# Considerations for `make codegen`:
# 1. Keep codegen sources in Git to simplify tarball builds, track history and monitor changes.
# 2. For development builds, validate correctness of generated code.
# 3. Use `make codegen` builds to force-update generated code.
define CODEGEN_CHECK
$2: | $$(dir $2)/	# Create $>/codegen/ subdirs
$2.check: $2		# Check or force-update generated file
	$Q cmp -s $1 $2 && touch $$@ && echo "  KEEP    $1" && exit; \
	echo '**WARNING**: detected codegen changes: $$(strip $1)'; \
	git -P diff --no-index -- $1 $2; \
	if test -n "$(WITH_CODEGEN)" ; \
	then mv $2 $1 && echo "  CODEGEN  $$(strip $1) !!FORCED-UPDATE!!  "; \
	else echo '**ERROR**: outdated target (need `make codegen`): $$(strip $1)'; false; \
	fi
ifneq (,$(REPOCOMMITDEPS)$(WITH_CODEGEN))
$1: $2.check		# Touch $1 if check (or regeneration) was ok
	$Q touch $1
endif
.PHONY: $(if $(WITH_CODEGEN), $2.check $2)
codegen: $1
ALL_TARGETS += $1
endef
$(foreach F, $(CODEGEN.FILES), $(eval $(call CODEGEN_CHECK, $F, $>/codegen/$F)))
CLEANDIRS += $>/codegen/

# == PACKAGE_VERSIONS ==
define PACKAGE_VERSIONS
  "version": "$(version_short)",
  "revdate": "$(version_date)",
  "__DEV__": $(__DEV__)
endef

# == config.sh ==
$>/config.sh: $(wildcard config-defaults.mk)				| $>/
	$(QGEN)
	$Q echo 'srcdir="$(abspath .)"'					>  $@.tmp
	$Q echo 'outdir="$(abspath $>)"'				>> $@.tmp
	$Q echo 'pkgdir="$(abspath $(pkgdir))"'				>> $@.tmp
	$Q mv $@.tmp $@
ALL_TARGETS += $>/config.sh

# == version.json ==
$>/version.json:							| $>/
	$(QGEN)
	$Q echo '{ $(strip $(PACKAGE_VERSIONS)) }'			> $@.tmp
	$Q mv $@.tmp $@
ALL_TARGETS += $>/version.json

# == npm.done ==
node_modules/.npm.done: $(if $(NPMBLOCK),, package.json Makefile.mk)		| $>/
	$(QGEN)
	$Q rm -f -r node_modules/
	@: # Install all node_modules and anonymize build path
	$Q : \
	  && { POFFLINE= && test ! -d node_modules/ || POFFLINE=--prefer-offline ; } \
	  && $(NPM_INSTALL) $$POFFLINE
	@: # Anonymize build paths in node_modules
	$Q find node_modules/ -name package.json -print0 | xargs -0 sed -r "\|$$PWD|s|^(\s*(\"_where\":\s*)?)\"$$PWD|\1\"/...|" -i
	@: # Silence annoying warnings
	$Q for f in ./node_modules/lit-html/development/lit-html.js ./node_modules/lit-html/node/development/lit-html.js \
		./node_modules/@lit/reactive-element/development/reactive-element.js ./node_modules/@lit/reactive-element/node/development/reactive-element.js \
		; do \
	  test -e "$$f" || continue; \
	  sed -r 's/(\bissueWarning..dev-mode., *.Lit is in dev mode. Not recommended)/if(0)\1/' -i "$$f" || break ; \
	done
	@: # Fix bun installation, see: https://github.com/oven-sh/bun/pull/5077
	$Q test ! -d node_modules/sharp/ -o -d node_modules/sharp/build/Release/ || (cd node_modules/sharp/ && $(NPM_INSTALL))
	$Q test -d node_modules/electron/dist/ || $(XNPM) node_modules/electron/install.js
	$Q touch $@
NODE_PATH ::= $(abspath node_modules/)
export NODE_PATH
CLEANDIRS += node_modules/
CLEANFILES += bun.lock bun.lockb package-lock.json

# == uninstall ==
uninstall:
	$Q $(RMDIR_P) '$(DESTDIR)$(pkgdir)/' ; true

# == x11test ==
x11test/files.json := $(wildcard x11test/*.json)
x11test x11test-v: $(x11test/files.json) $(lib/AnklangSynthEngine)
	$(QGEN)
	$Q rm -f -r $>/x11test/
	$Q mkdir -p $>/x11test/
	$Q $(CP) $(x11test/files.json) $>/x11test/
	$Q cd $>/x11test/ \
	&& { test "$@" == x11test-v && OPT=-v || OPT=-p ; } \
	&& for json in *.json ; do \
		echo "$$json" \
		&& $(abspath x11test/replay.sh) $$OPT $$json || exit $$? \
	 ; done
.PHONY: x11test x11test-v

# == tscheck ==
check: tscheck
# run tsc on all JS + TS files, except for ui/ x11test/
tscheck.files := $(filter-out ui/% x11test/%, $(filter %.cts %.cjs %.d.cts %.js %.jsx %.mts %.mjs %.d.mts %.ts %.tsx %.d.ts, $(WILDCARD_FILES)))
# --skipLibCheck: electron-39.2.7 node_modules/electron/electron.d.ts:noDeprecation?:boolean conflicts with node_modules/@types/node/process.d.ts
TSC_CHECK_FLAGS := --allowJs --skipLibCheck
$>/.tscheck.done: $(tscheck.files)	| node_modules/.npm.done
	$(QGEN)
	$Q node_modules/.bin/tsc --noEmit --moduleResolution bundler -m esnext --target esnext --erasableSyntaxOnly $(TSC_CHECK_FLAGS) $(tscheck.files)
$>/.tscheck.done: $(if $(filter check tscheck,$(MAKECMDGOALS)), FORCE) # force on 'make tscheck'
tscheck: $>/.tscheck.done FORCE

# == eslint ==
check: eslint
eslint.files := $(filter %.htm %.html %.cts %.cjs %.d.cts %.js %.jsx %.mts %.mjs %.d.mts %.ts %.tsx %.d.ts, $(WILDCARD_FILES))
eslint.skip  := %/javascript/mathjax.js
$>/.eslint.done: ui/eslintrc.js $(eslint.files) Makefile.mk	| node_modules/.npm.done
	$(QECHO) RUN eslint
	-$Q node_modules/.bin/eslint -c $< --no-warn-ignored $${INSIDE_EMACS+-f unix} --cache --cache-location $>/.eslintcache \
		$(abspath $(filter-out $(eslint.skip), $(eslint.files))) \
	&& touch $@
$>/.eslint.done: $(if $(filter check eslint,$(MAKECMDGOALS)), FORCE) # force on 'make eslint'
eslint: $>/.eslint.done FORCE

# == stylelint ==
stylelint: FORCE
check: stylelint
	$Q echo "  OK      " "All $(words $^) checks passed"

# == strict ==
strict: check

# == check ==
# Macro to generate test runs as 'check' dependencies
define CHECK_ALL_TESTS_TEST
CHECK_TARGETS += $$(dir $1)check-$$(notdir $1)
$$(dir $1)check-$$(notdir $1): $1
	$$(QECHO) RUN… $$@
	$$Q $1
endef
$(foreach TEST, $(ALL_TESTS), $(eval $(call CHECK_ALL_TESTS_TEST, $(TEST))))
CHECK_TARGETS += check-audio check-bench
check: $(CHECK_TARGETS) lint
$(CHECK_TARGETS): FORCE
check-bench: FORCE

# == installcheck ==
installcheck:
	$(QGEN)
	$Q $(DESTDIR)$(bindir)/anklang --version | grep -qi '^anklang'
	$Q grep -qE 'Anklang\b' $(DESTDIR)$(mandir)/man1/anklang.1
.PHONY: installcheck
# conftest_buildtest.c
define conftest_buildtest.c
#include <engine/engine.hh>
extern "C"
int main (int argc, char *argv[])
{
  Anklang::start_server (&argc, argv);
  return 0;
}
endef

# == dist ==
dist_exclude := $(strip			\
	external/rapidjson/bin		\
	external/rapidjson/doc		\
	external/websocketpp/test	\
	external/clap/artwork		\
	external/minizip-ng/test	\
	external/minizip-ng/lib		\
)
dist: TAGS
	$(eval distname := anklang-$(version_short))
	$(QECHO) MAKE $(distname).tar.zst
	$Q git describe --dirty | grep -qve -dirty || echo -e "#\n# $@: WARNING: working tree is dirty\n#"
	$Q rm -r -f artifacts/ && mkdir -p artifacts/
	$Q # Generate ChangeLog with ^^-prefixed records. Tab-indent commit bodies.
	$Q # Kill trailing whitespaces. Compress multiple newlines.
	$Q git log --abbrev=13 --date=short --first-parent HEAD	\
		--pretty='^^%ad  %an 	# %h%n%n%B%n'		>  artifacts/ChangeLog \
	&& sed 's/^/	/; s/^	^^// ; s/[[:space:]]\+$$// '	-i artifacts/ChangeLog \
	&& sed '/^\s*$$/{ N; /^\s*\n\s*$$/D }'			-i artifacts/ChangeLog
	$Q # Generate and compress artifacts/anklang-*.tar.zst
	$Q git archive --prefix=$(distname)/ --add-file artifacts/ChangeLog --add-file TAGS -o artifacts/$(distname).tar HEAD
	$Q rm -f artifacts/$(distname).tar.zst && zstd --ultra -22 --rm artifacts/$(distname).tar && ls -lh artifacts/$(distname).tar.zst
	$Q echo "Archive ready: artifacts/$(distname).tar.zst" | sed '1h; 1s/./=/g; 1p; 1x; $$p; $$x'
CLEANDIRS += artifacts/
.PHONY: dist

# == TAGS ==
# ctags --print-language `git ls-tree -r --name-only HEAD`
TAGS: $(REPOCOMMITDEPS)
	$(file > $>/tags.lst, $(WILDCARD_FILES))
	$(QGEN)
	$Q etags --version 2>/dev/null | grep -qE 'Exuberant|Universal' || exit 0 >$@ \
	&& tr ' ' '\n' < $>/tags.lst \
	| grep -E '\.([mc]?jsx?|tsx?|py|go|hh|cc|[HChc]|[HChc]pp)$$' \
	| etags -o $@ -L - 2> >(grep -vF 'Warning: ignoring null tag in')
	$Q rm -f $>/tags.lst
CLEANFILES += TAGS
ALL_TARGETS += TAGS

# == compile_commands.json ==
compile_commands.json: Makefile.mk
	$(QGEN)
	$Q rm -f $>/*/*.o $>/*/*/*.o
	bear -- $(MAKE) CC=clang CXX=clang++ -j
CLEANFILES += compile_commands.json

# == grep-f_i_x_m_e ==
$>/.grep-f_i_x_m_e: $(WILDCARD_FILES)
	$Q grep --color=auto -n -E '(/[*/]+[*/ ]*|[#*]+ *)?(FI[X]ME).*' $(WILDCARD_FILES) || true
	$Q touch $@
LATE_TARGETS += $>/.grep-f_i_x_m_e

# == grep-pch ==
$>/.grep-pch: $(WILDCARD_FILES)
	$Q grep --color=auto -n -E '\btrkn/tracktion.hh\b|\btrkn/juce.hh\b' $(filter ase/%.hh, $(WILDCARD_FILES)) || exit 0 \
	&& { echo "$(firstword $(MAKEFILE_LIST)): error: invalid PCH includes in header files"; false; }
	$Q touch $@
LATE_TARGETS += $>/.grep-pch

# == help rules ==
help: FORCE
	@echo 'Make targets:'
	@: #   12345678911234567892123456789312345678941234567895123456789612345678971234567898
	@echo '  all             - Build all targets, uses config-defaults.mk if present.'
	@echo '  codegen         - Force code regeneration.'
	@echo '  clean           - Remove build directory, but keeps config-defaults.mk.'
	@echo '  install         - Install binaries and data files under $$(prefix)'
	@echo '  uninstall       - Uninstall binaries, aliases and data files'
	@echo '  installcheck    - Run checks on the installed project files.'
	@echo '  default         - Create config-defaults.mk with variables set via the MAKE'
	@echo '                    command line. Inspect the file for a list of variables to'
	@echo '                    be customized. Deleting it will undo any customizations.'
	@echo '  check           - Run selfttests and unit tests'
	@echo '  strict          - Run `make check` and additional CI test'
	@echo '  x11test         - Replay all JSON recordings from x11test/ in Electron.'
	@echo '  x11test-v       - Run "x11test" in virtual XServer (headless).'
	@echo '  check-audio     - Validate Anklang rendering against reference files'
	@echo '  check-bench     - Run the benchmark tests'
	@echo '  check-loading   - Check all distributed Anklang files load properly'
	@echo '  serve           - Start Anklang and serve assets via Vite with hot-reloading'
	@echo '  run             - Start Anklang without installation'
	@echo -e ' '$(MAKE_HELP)
	@echo 'Invocation:'
	@echo '  make V=1        - Enable verbose output from MAKE and subcommands'
	@echo '  make O=DIR      - Create all output files in DIR, see also config-defaults.mk'
	@echo '                    for related variables like CXXFLAGS'
	@echo '  make DESTDIR=/  - Absolute path prepended to all install/uninstall locations'
	@echo '  make INSN=...   - Optimize instructions: sse (ca 2008), fma (ca 2015), [native]'
	@echo "  make MODE=...   - Run 'quick' build or make 'production' mode binaries."
	@echo '                    Other modes: cpptrace, debug, devel, asan, lsan, tsan, ubsan'
	@echo 'Notes:'
	@echo '  .git            - Building in a Git (or Jujutsu) repo triggers additional'
	@echo '                    checks for e.g. copyright attribution or codegen'

# == all rules ==
cxx-all: $(CXX_TARGETS)
$(eval $(LATE_EVAL))
all: cxx-all $(ALL_TARGETS) $(ALL_TESTS) $(LATE_TARGETS)
	$Q echo "  DONE    " "All targets remade successfully"
