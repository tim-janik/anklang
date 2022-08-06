# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/misc/*.d)
misc/cleandirs ::= $(wildcard $>/misc/)
CLEANDIRS       += $(misc/cleandirs)
ALL_TARGETS     += misc/all
misc/all:

# == clean-misc ==
clean-misc:
	rm -rf $(misc/cleandirs)
.PHONY: clean-misc

# == git-ls-tree.lst ==
$>/misc/git-ls-tree.lst: $(GITCOMMITDEPS)					| $>/misc/
	$Q git ls-tree -r --name-only HEAD	> $@ || touch $@

# == lint-cppcheck ==
CPPCHECK ?= cppcheck
CPPCHECK_CCENABLE := warning,style,performance,portability
lint-cppcheck: $>/misc/git-ls-tree.lst misc/Makefile.mk		| $>/misc/cppcheck/
	$Q egrep $(CLANGTIDY_GLOB) < $<		> $>/misc/cppcheck/sources.lst
	$Q $(CPPCHECK) --enable=$(CPPCHECK_CCENABLE) $(CPPCHECK_DEFS) \
		$$(cat $>/misc/cppcheck/sources.lst)
CPPCHECK_DEFS := -D__SIZEOF_LONG__=8 -D__SIZEOF_WCHAR_T__=4 -D__linux__ -U_SC_NPROCESSORS_ONLN -U_WIN32 -U__clang__
.PHONY: lint-cppcheck

# == lint-unused ==
lint-unused: $>/misc/git-ls-tree.lst misc/Makefile.mk		| $>/misc/cppcheck/
	$Q egrep $(CLANGTIDY_GLOB) < $<			> $>/misc/cppcheck/sources.lst
	$Q $(CPPCHECK) --enable=unusedFunction,$(CPPCHECK_CCENABLE) $(CPPCHECK_DEFS) \
		$$(cat $>/misc/cppcheck/sources.lst)	2>&1 | \
	   grep -E '(\bunuse|reach)' | sort | tee $>/misc/cppcheck/lint-unused.log
.PHONY: lint-unused

# == ls-lint.d ==
CLANGTIDY_GLOB	:= "^(ase|devices|jsonipc|ui)/.*\.(cc)$$"
CLANGTIDY_IGNORE	:= "^(ase)/.*\.(cpp)$$"
CLANGTIDY_SRC	:= # added to by ls-lint.d
$>/misc/ls-lint.d: $>/misc/git-ls-tree.lst misc/Makefile.mk
	$Q egrep $(CLANGTIDY_GLOB) < $< | egrep -v $(CLANGTIDY_IGNORE)	> $@1
	$Q while read L ; do			\
		echo "CLANGTIDY_SRC += $$L" ;	\
	   done							< $@1	> $@2
	$Q mv $@2 $@ && rm $@1
-include $>/misc/ls-lint.d
CLANGTIDY_LOGS = $(CLANGTIDY_SRC:%=$>/misc/clang-tidy/%.log)

# == lint ==
lint: $(CLANGTIDY_LOGS)
	$Q for F in $(CLANGTIDY_LOGS) ; do \
		test -s "$$F" || continue ; \
		echo "$$F:" && cat "$$F" || exit -1 ; done
lint-clean:
	rm -f $(CLANGTIDY_LOGS)
.PHONY: lint lint-clean
# Note, 'make lint' requires a successfuly built source tree to access generated sources.

# == clang-tidy logs ==
$>/misc/clang-tidy/%.log: % $(GITCOMMITDEPS) # misc/Makefile.mk
	$(QECHO) LINTING $@
	$Q mkdir -p $(dir $@)
	$Q set +o pipefail ; clang-tidy $< $($<.LINT_FLAGS) -- $($<.LINT_CCFLAGS) $(misc/clang-tidy/DEFS) 2>&1 | tee $@~
	$Q mv $@~ $@
misc/clang-tidy/DEFS := -std=gnu++17 -I. -I$> -isystem external/ -isystem $>/external/ -DASE_COMPILATION `$(PKG_CONFIG) --cflags glib-2.0`
# Example for file specific LINT_FLAGS:
# ase/jsonapi.cc.LINT_FLAGS ::= --checks=-clang-analyzer-core.NullDereference
jsonipc/testjsonipc.cc.LINT_CCFLAGS ::= -D__JSONIPC_NULL_REFERENCE_THROWS__

# == scan-build ==
scan-build:								| $>/misc/scan-build/
	$(QGEN)
	$Q rm -rf $>/misc/scan-tmp/ && mkdir -p $>/misc/scan-tmp/
	$Q echo "  CHECK   " "for CXX to resemble clang++"
	$Q $(CXX) --version | grep '\bclang\b'
	scan-build -o $>/misc/scan-build/ --use-cc "$(CC)" --use-c++ "$(CXX)" $(MAKE) CCACHE= -j`nproc`
	$Q shopt -s nullglob ; \
	      for r in $>/misc/scan-build/20??-??-??-*/report-*.html ; do \
		D=$$(sed -nr '/<!-- BUGDESC/ { s/^<!-- \w+ (.+) -->/\1/	   ; p }' $$r) && \
		F=$$(sed -nr '/<!-- BUGFILE/ { s/^<!-- \w+ ([^ ]+) -->/\1/ ; p }' $$r) && \
		L=$$(sed -nr '/<!-- BUGLINE/ { s/^<!-- \w+ ([^ ]+) -->/\1/ ; p }' $$r) && \
		echo "$$F:$$L: $$D" | sed "s,^`pwd`/,," ; \
	      done > $>/misc/scan-build/scan-build.log
	misc/blame-lines -b $>/misc/scan-build/scan-build.log
.PHONY: scan-build
# Note, 'make scan-build' requires 'make default CC=clang CXX=clang++' to generate any reports.

# == versioned-manuals ==
versioned-manuals: $>/doc/anklang-manual.pdf $>/doc/anklang-internals.pdf
	$Q cp -v $>/doc/anklang-manual.pdf $>/anklang-manual-$(version_short).pdf
	$Q cp -v $>/doc/anklang-internals.pdf $>/anklang-internals-$(version_short).pdf

# == build-version ==
$>/build-version: misc/version.sh
	$(QGEN)
	$Q misc/version.sh > $@.tmp && mv $@.tmp $@
build-version: $>/build-version
	$Q cat $>/build-version

# == anklang-deb ==
$>/anklang_$(version_short)_amd64.deb: $>/TAGS $(GITCOMMITDEPS)
	$(QGEN)
	$Q BUILDDIR=$> misc/mkdeb.sh
	$Q ls -l -h $@
anklang-deb: $>/anklang_$(version_short)_amd64.deb
.PHONY: anklang-deb

# == appimage ==
APPINST = $>/appinst/
APPBASE = $>/appbase/
$>/anklang-$(version_short)-x64.AppImage: $>/misc/appaux/appimage-runtime-zstd $>/TAGS $(GITCOMMITDEPS) | $>/misc/bin/
	$(QGEN)
	@: # Installation Step
	@echo '  INSTALL ' AppImage files
	$Q rm -fr $(APPINST) $(APPBASE) && make install DESTDIR=$(APPINST)
	@: # Populate appinst/, linuxdeploy expects libraries under usr/lib, binaries under usr/bin, etc
	@: # We achieve that by treating the anklang-$MAJOR-$MINOR/ installation directory as /usr/.
	@: # Also, we hand-pick extra libs for Anklang to keep the AppImage small.
	$Q $(eval APPIMAGEPKGDIR ::= $(APPBASE)/anklang-$(version_major)-$(version_minor))
	$Q mkdir $(APPBASE) && cp -a $(APPINST)$(pkgdir) $(APPIMAGEPKGDIR)
	$Q rm -f Anklang-x86_64.AppImage
	@echo '  RUN     ' linuxdeploy...
	$Q if test -e /usr/lib64/libc_nonshared.a ; \
	   then LIB64=/usr/lib64/ ; \
	   else LIB64=/usr/lib/x86_64-linux-gnu/ ; fi \
	   && LD_LIBRARY_PATH=$(APPIMAGEPKGDIR)/lib DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1 \
	     $>/misc/appaux/linuxdeploy-x86_64.AppImage --appimage-extract-and-run \
		$(if $(findstring 1, $(V)), -v1, -v2) \
		--appdir=$(APPBASE) \
		-e $(APPIMAGEPKGDIR)/bin/anklang \
		-i $(APPIMAGEPKGDIR)/ui/anklang.png \
		-d $(APPIMAGEPKGDIR)/share/applications/anklang.desktop \
		-l $$LIB64/libXss.so.1 \
		-l $$LIB64/libXtst.so.6 \
		--exclude-library="libnss3.so" \
		--exclude-library="libnssutil3.so" \
		--custom-apprun=misc/AppRun
	@: # 'linuxdeploy -e bin/anklang' creates an executable copy in usr/bin/, which electron does not support
	$Q rm $(APPBASE)/usr/bin/anklang && ln -s -r $(APPIMAGEPKGDIR)/bin/anklang $(APPBASE)/usr/bin/ # enforce bin/* as link
	@: # linuxdeploy collects too many libs for electron/anklang, remove duplictaes present in electron/
	$Q cd $(APPBASE)/usr/lib/ && rm -vf $(notdir $(wildcard $(APPIMAGEPKGDIR)/electron/lib*.so*))
	@: # Create AppImage executable
	@echo '  BUILD   ' appimage-runtime...
	$Q mksquashfs $(APPBASE) $>/Anklang-x86_64.AppImage $(misc/squashfsopts)
	$Q cat $>/misc/appaux/appimage-runtime-zstd $>/Anklang-x86_64.AppImage > $@.tmp && rm -f $>/Anklang-x86_64.AppImage
	$Q chmod +x $@.tmp
	$Q mv $@.tmp $@ && ls -l -h $@
misc/squashfsopts ::= -root-owned -noappend -mkfs-time 0 -no-exports -no-recovery -noI -always-use-fragments -b 1048576 -comp zstd -Xcompression-level 22
$>/misc/appaux/appimage-runtime-zstd:					| $>/misc/appaux/
	$(QECHO) FETCH $(@F), linuxdeploy # fetch AppImage tools
	$Q cd $(@D) $(call foreachpair, AND_DOWNLOAD_SHAURL, \
		0c4c18bb44e011e8416fc74fb067fe37a7de97a8548ee8e5350985ddee1c0164 https://github.com/tim-janik/appimage-runtime/releases/download/21.6.0/appimage-runtime-zstd )
	$Q cd $>/misc/appaux/ && \
		curl -sfSOL https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage && \
		chmod +x linuxdeploy-x86_64.AppImage
appimage: $>/anklang-$(version_short)-x64.AppImage
.PHONY: appimage

# == misc/anklang.desktop ==
# https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
$>/misc/anklang.desktop: misc/anklang.desktop
	@$(QGEN)
	$Q sed 's|\$$(pkgdir)|$(pkgdir)|g' $< > $@.tmp
	$Q mv $@.tmp $@
misc/all: $>/misc/anklang.desktop

# == installation ==
misc/svgdir ::= $(sharedir)/icons/hicolor/scalable/apps
misc/install:
	@$(QECHO) INSTALL '$(DESTDIR)$(pkgsharedir)/.'
	$Q rm -f -r $(DESTDIR)$(pkgsharedir)/applications/ $(DESTDIR)$(pkgsharedir)/mime/packages/
	$Q $(INSTALL) -d $(DESTDIR)$(pkgsharedir)/mime/packages/ && cp -p misc/anklang-mime.xml $(DESTDIR)$(pkgsharedir)/mime/packages/anklang.xml
	$Q $(INSTALL) -d $(DESTDIR)$(sharedir)/mime/packages/ && ln -fs -r $(DESTDIR)$(pkgsharedir)/mime/packages/anklang.xml $(DESTDIR)$(sharedir)/mime/packages/anklang.xml
	$Q $(INSTALL) -d $(DESTDIR)$(pkgsharedir)/applications/ && cp -p $>/misc/anklang.desktop $(DESTDIR)$(pkgsharedir)/applications/
	$Q $(INSTALL) -d $(DESTDIR)$(sharedir)/applications/ && ln -fs -r $(DESTDIR)$(pkgsharedir)/applications/anklang.desktop $(DESTDIR)$(sharedir)/applications/anklang.desktop
	$Q $(INSTALL) -d $(DESTDIR)$(misc/svgdir)/ && ln -fs -r $(DESTDIR)$(pkgdir)/ui/assets/favicon.svg $(DESTDIR)$(misc/svgdir)/anklang.svg

.PHONY: misc/install
install: misc/install
misc/uninstall: FORCE
	@$(QECHO) REMOVE '$(DESTDIR)$(pkgsharedir)/.'
	$Q rm -f -r $(DESTDIR)$(pkgsharedir)/applications/ $(DESTDIR)$(pkgsharedir)/mime/packages/
	$Q rm -f $(DESTDIR)$(sharedir)/mime/packages/anklang.xml
	$Q rm -f $(DESTDIR)$(sharedir)/applications/anklang.desktop
	$Q rm -f $(DESTDIR)$(misc/svgdir)/anklang.svg
.PHONY: misc/uninstall
uninstall: misc/uninstall

# == Check Copyright Notices ==
check-copyright: misc/mkcopyright.py doc/copyright.ini $>/misc/git-ls-tree.lst
	$(QGEN)
	$Q misc/mkcopyright.py -b -u -e -c doc/copyright.ini $$(cat $>/misc/git-ls-tree.lst)
CHECK_TARGETS += $(WITHGIT) check-copyright

# == ChangeLog ==
$>/ChangeLog-$(version_short).txt: $(GITCOMMITDEPS) misc/Makefile.mk		| $>/
	$(QGEN)
	$Q LAST_TAG=`misc/version.sh --news-tag2`				\
	&& { LAST_COMMIT=`git log -1 --pretty=%H "$$LAST_TAG" 2>/dev/null`	\
	  || LAST_COMMIT=96e7881fac0a2cd7f4d20a3f0666f1295ff4ee77 ; }		\
	&& git log --pretty='^^%ad  %an 	# %h%n%n%B%n'			\
		--topo-order --full-history \
		--abbrev=13 --date=short $$LAST_COMMIT..HEAD	 > $@.tmp	# Generate ChangeLog with ^^-prefixed records
	$Q sed 's/^/	/; s/^	^^// ; s/[[:space:]]\+$$// '    -i $@.tmp	# Tab-indent commit bodies, kill trailing whitespaces
	$Q sed '/^\s*$$/{ N; /^\s*\n\s*$$/D }'			-i $@.tmp	# Compress multiple newlines
	$Q mv $@.tmp $@
CLEANFILES += $>/ChangeLog-$(version_short).txt
release-changelog: $>/ChangeLog-$(version_short).txt
.PHONY: release-changelog

# == release-news ==
release-news:
	$Q LAST_TAG=`./misc/version.sh --news-tag2` && ( set -x && \
	  git log --first-parent --date=short --pretty='%s    # %cd %an %h%d%n%w(0,4,4)%b' --reverse HEAD "$$LAST_TAG^!" ) | \
		sed -e '/^\s*Signed-off-by:.*<.*@.*>/d' -e '/^\s*$$/d'
.PHONY: release-news

# == insn-build ==
# Build binary variants with INSN=sse and build 'all'
insn-build-sse:
	$Q $(MAKE) INSN=sse builddir=out-sse all release-changelog
	$Q $(MAKE) INSN=sse builddir=out-sse check
# Build binary variants with INSN=fma
insn-build-fma:
	$Q $(MAKE) INSN=fma builddir=out-fma insn-targets INSNDEST=out-sse/

# == release rules ==
RELEASE_TEST         ?= false
OOTBUILD             ?= /tmp/anklang-ootbuild$(shell id -u)
RELEASE_SSEDIR      ::= $(OOTBUILD)/out-sse

# == build-assets ==
build-assets:
	@: # Clear out-of-tree build directory (but keep .dlcache/), note that
	@: # ABSPATH_DLCACHE points to $(abspath .dlcache); checkout current HEAD
	$Q :										\
		&& rm -rf .tmp.dlcache							\
		&& mkdir -p $(OOTBUILD) && cd $(OOTBUILD)				\
		&& (mv .dlcache/ $(abspath .tmp.dlcache) || : ) 2>/dev/null		\
		&& rm -rf * .[^.]* ..?*							\
		&& (mv $(abspath .tmp.dlcache) .dlcache || : ) 2>/dev/null
	@: # Prepare source tree
	$Q tar xf $(TARBALL) -C $(OOTBUILD) --strip-components=1
	@: # Build binaries with different INSNs in parallel, delete tag on error
	$Q nice $(MAKE) -C $(OOTBUILD) -j`nproc` -l`nproc`		\
		insn-build-sse						\
		insn-build-fma
	@: # Build release packages, INSN=sse is full build, delete tag on error
	$Q nice $(MAKE) -C $(OOTBUILD) -j`nproc` -l`nproc`	\
		INSN=sse builddir=out-sse				\
		anklang-deb						\
		appimage						\
		versioned-manuals
	$Q echo									>  $(RELEASE_SSEDIR)/build-assets
	$Q echo	$(RELEASE_SSEDIR)/anklang_$(version_short)_amd64.deb		>> $(RELEASE_SSEDIR)/build-assets
	$Q echo	$(RELEASE_SSEDIR)/anklang-$(version_short)-x64.AppImage		>> $(RELEASE_SSEDIR)/build-assets
	$Q echo	$(RELEASE_SSEDIR)/ChangeLog-$(version_short).txt		>> $(RELEASE_SSEDIR)/build-assets
	$Q echo $(RELEASE_SSEDIR)/anklang-manual-$(version_short).pdf		>> $(RELEASE_SSEDIR)/build-assets
	$Q echo $(RELEASE_SSEDIR)/anklang-internals-$(version_short).pdf	>> $(RELEASE_SSEDIR)/build-assets
	@: # Check build
	$Q ls -l -h --color=auto `cat $(RELEASE_SSEDIR)/build-assets`
	$Q test ! -r /dev/fuse || time $(RELEASE_SSEDIR)/anklang-$(version_short)-x64.AppImage --quitstartup
	@: # Record build version and artifacts
	$Q echo "$(version_short)"	> $(RELEASE_SSEDIR)/build-version

# == build-release ==
build-release:
	$(QGEN)
	@: # Setup release variables (note, eval preceeds all shell commands)
	@ $(eval RELEASE_TAG       != ./misc/version.sh --news-tag1)
	@: # Check release tag
	$Q NEWS_TAG=`./misc/version.sh --news-tag1` && test "$$NEWS_TAG" == "$(RELEASE_TAG)"
	$Q test -z "`git tag -l '$(RELEASE_TAG)'`" || \
		{ echo '$@: error: release tag from NEWS.md already exists: $(RELEASE_TAG)' >&2 ; false ; }
	@: # Check against origin/trunk
	$Q VERSIONHASH=`git rev-parse HEAD` && \
		git merge-base --is-ancestor "$$VERSIONHASH" origin/trunk || \
		$(RELEASE_TEST) || \
		{ echo "$@: ERROR: Release ($$VERSIONHASH) must be built from origin/trunk" ; false ; }
	@: # Tag release with annotation
	$Q git tag -a '$(RELEASE_TAG)' -m "`git log -1 --pretty=%s`"
	@: # Build versioned release assets
	$Q export CHECKOUT_HEAD='$(RELEASE_TAG)' \
		  WITH_LATEX=`xelatex -version | grep -o 3.14159265` \
		  VERSION_SH_RELEASE=true \
	   && $(MAKE) build-assets

# == build-nightly ==
build-nightly:
	$(QGEN)
	@: # Check against origin/trunk
	$Q VERSIONHASH=`git rev-parse HEAD` && \
		git merge-base --is-ancestor "$$VERSIONHASH" origin/trunk || \
		$(RELEASE_TEST) || \
		{ echo "$@: ERROR: Nightly release ($$VERSIONHASH) must be built from origin/trunk" ; false ; }
	@: # Tag Nightly, force to move any old version
	$Q git tag -f Nightly HEAD
	@: # Update NEWS.md with nightly changes
	$Q NIGHTLY_VERSION=`misc/version.sh --make-nightly` \
		&& echo "$$NIGHTLY_VERSION" | grep 'nightly' \
		|| { echo "$@: missing nightly tag: $$NIGHTLY_VERSION" ; exit 1 ; } \
		&& LOG_RANGE=`git describe --match v'[0-9]*.[0-9]*.[0-9]*' --abbrev=0 --first-parent` \
		&& LOG_RANGE="$$LOG_RANGE..HEAD" \
		&& echo -e "## Anklang $$NIGHTLY_VERSION\n"		>  ./NEWS.build \
		&& echo '```````````````````````````````````````````'	>> ./NEWS.build \
		&& git log --pretty='%s    # %cd %an %h%n%w(0,4,4)%b' \
			--first-parent --date=short "$$LOG_RANGE"	>> ./NEWS.build \
		&& sed -e '/^\s*Signed-off-by:.*<.*@.*>/d'		-i ./NEWS.build \
		&& sed '/^\s*$$/{ N; /^\s*\n\s*$$/D }'			-i ./NEWS.build \
		&& echo '```````````````````````````````````````````'	>> ./NEWS.build \
		&& echo 						>> ./NEWS.build \
		&& cat ./NEWS.md					>> ./NEWS.build
	@: # Build versioned release assets
	$Q export CHECKOUT_HEAD=Nightly \
		  WITH_LATEX=`xelatex -version | grep -o 3.14159265` \
		  VERSION_SH_NIGHTLY=true \
	   && $(MAKE) build-assets

# == upload-nightly ==
upload-nightly:
	$(QGEN)
	@: # Check release attachments
	$Q cat $(RELEASE_SSEDIR)/build-version
	$Q du -hs `cat $(RELEASE_SSEDIR)/build-assets`
	@: # Create Github release and upload assets
	$Q echo 'Nightly'					>  $(RELEASE_SSEDIR)/release-message
	$Q echo							>> $(RELEASE_SSEDIR)/release-message
	$Q echo 'Anklang' \
		`cut '-d ' -f1 $(RELEASE_SSEDIR)/build-version`	>> $(RELEASE_SSEDIR)/release-message
	-$Q hub release delete 'Nightly'
	-$Q git push origin ':Nightly'
	$Q git push origin 'Nightly'					\
		&& ASSETS=( `cat $(RELEASE_SSEDIR)/build-assets` )	\
		&& hub release create --prerelease			\
		     -F '$(RELEASE_SSEDIR)/release-message'		\
		     "$${ASSETS[@]/#/-a}"				\
		     'Nightly'

# == upload-release ==
upload-release:
	$(QGEN)
	@: # Determine version, check release attachments
	@ $(eval RELEASE_TAG != misc/version.sh --release-tag)
	@ $(eval DETAILED_VERSION = $(RELEASE_TAG:v%=%))
	$Q NEWS_TAG=`./misc/version.sh --news-tag1` && test "$$NEWS_TAG" == "$(RELEASE_TAG)"
	$Q du -hs $(RELEASE_CHANGELOG) $(RELEASE_DEB) $(RELEASE_APPIMAGE)
	@: # Create Github release and upload assets
	$Q echo 'Anklang $(DETAILED_VERSION)'		>  $(RELEASE_SSEDIR)/release-message
	$Q echo						>> $(RELEASE_SSEDIR)/release-message
	$Q sed '0,/^## /b; /^## /Q; ' NEWS.md		>> $(RELEASE_SSEDIR)/release-message
	$Q ASSETS=( $(RELEASE_SSEDIR)/*"$(DETAILED_VERSION)"* )		\
		&& hub release create --draft				\
		-F '$(RELEASE_SSEDIR)/release-message'			\
		"$${ASSETS[@]/#/-a}"					\
		"$(RELEASE_TAG)"
	$Q echo 'Run:'
	$Q echo '  git push origin "$(RELEASE_TAG)"'
