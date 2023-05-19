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

# == lint-cppcheck ==
CPPCHECK ?= cppcheck
CPPCHECK_CCENABLE := warning,style,performance,portability
lint-cppcheck: $>/ls-tree.lst misc/Makefile.mk		| $>/misc/cppcheck/
	$Q egrep $(CLANGTIDY_GLOB) < $<		> $>/misc/cppcheck/sources.lst
	$Q $(CPPCHECK) --enable=$(CPPCHECK_CCENABLE) $(CPPCHECK_DEFS) \
		$$(cat $>/misc/cppcheck/sources.lst)
CPPCHECK_DEFS := -D__SIZEOF_LONG__=8 -D__SIZEOF_WCHAR_T__=4 -D__linux__ -U_SC_NPROCESSORS_ONLN -U_WIN32 -U__clang__
.PHONY: lint-cppcheck

# == lint-unused ==
lint-unused: $>/ls-tree.lst misc/Makefile.mk		| $>/misc/cppcheck/
	$Q egrep $(CLANGTIDY_GLOB) < $<			> $>/misc/cppcheck/sources.lst
	$Q $(CPPCHECK) --enable=unusedFunction,$(CPPCHECK_CCENABLE) $(CPPCHECK_DEFS) \
		$$(cat $>/misc/cppcheck/sources.lst)	2>&1 | \
	   grep -E '(\bunuse|reach)' | sort | tee $>/misc/cppcheck/lint-unused.log
.PHONY: lint-unused

# == ls-lint.d ==
CLANGTIDY_GLOB	:= "^(ase|devices|jsonipc|ui)/.*\.(cc)$$"
CLANGTIDY_IGNORE	:= "^(ase)/.*\.(cpp)$$"
CLANGTIDY_SRC	:= # added to by ls-lint.d
$>/misc/ls-lint.d: $>/ls-tree.lst misc/Makefile.mk	| $>/misc/
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
check-copyright: misc/mkcopyright.py doc/copyright.ini $>/ls-tree.lst
	$(QGEN)
	$Q misc/mkcopyright.py -b -u -e -c doc/copyright.ini $$(cat $>/ls-tree.lst)
CHECK_TARGETS += $(if $(HAVE_GIT), check-copyright)

# == release-news ==
release-news:
	$Q LAST_TAG=`misc/version.sh --news-tag2` && ( set -x && \
	  git log --first-parent --date=short --pretty='%s    # %cd %an %h%d%n%w(0,4,4)%b' --reverse HEAD "$$LAST_TAG^!" ) | \
		sed -e '/^\s*Signed-off-by:.*<.*@.*>/d' -e '/^\s*$$/d'
.PHONY: release-news

# == appimage-runtime-zstd ==
$>/misc/appaux/appimage-runtime-zstd:					| $>/misc/appaux/
	$(QECHO) FETCH $(@F), linuxdeploy # fetch AppImage tools
	$Q cd $(@D) $(call foreachpair, AND_DOWNLOAD_SHAURL, \
		0c4c18bb44e011e8416fc74fb067fe37a7de97a8548ee8e5350985ddee1c0164 https://github.com/tim-janik/appimage-runtime/releases/download/21.6.0/appimage-runtime-zstd )
	$Q cd $>/misc/appaux/ && \
		curl -sfSOL https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage && \
		chmod +x linuxdeploy-x86_64.AppImage
misc/mkassets.sh: $>/misc/appaux/appimage-runtime-zstd
CLEANDIRS += $>/mkdeb/

# == build-release ==
build-release: misc/mkassets.sh
	$(QGEN)
	@: # Determine release tag (eval preceeds all shell commands)
	@ $(eval RELEASE_TAG != misc/version.sh --news-tag1)
	@: # Check for new release tag
	$Q test -z "`git tag -l '$(RELEASE_TAG)'`" || \
		{ echo '$@: error: release tag from NEWS.md already exists: $(RELEASE_TAG)' >&2 ; false ; }
	@: # Check against origin/trunk
	$Q VERSIONHASH=`git rev-parse HEAD` && \
		git merge-base --is-ancestor "$$VERSIONHASH" origin/trunk || \
		$(RELEASE_TEST) || \
		{ echo "$@: ERROR: Release ($$VERSIONHASH) must be built from origin/trunk" ; false ; }
	@: # Tag release with annotation
	$Q git tag -a '$(RELEASE_TAG)' -m "`git log -1 --pretty=%s`"
	@: # make dist - create release tarball
	$Q $(MAKE) dist
	@: # mkassets.sh - build binary release assets
	$Q export V=$V							\
	&& misc/dbuild.sh misc/mkassets.sh $>/anklang-$(RELEASE_TAG:v%=%).tar.zst
RELEASE_TEST ?= false

# == build-nightly ==
build-nightly: misc/mkassets.sh
	$(QGEN)
	@: # Check against origin/trunk
	$Q VERSIONHASH=`git rev-parse HEAD` && \
		git merge-base --is-ancestor "$$VERSIONHASH" origin/trunk || \
		$(RELEASE_TEST) || \
		{ echo "$@: ERROR: Nightly release ($$VERSIONHASH) must be built from origin/trunk" ; false ; }
	@: # Tag Nightly, force to move any old version
	$Q git tag -f Nightly HEAD
	@: # make dist - create nightly tarball with updated NEWS.md
	$Q ARCHIVE_VERSION=`misc/version.sh | sed 's/ .*//'`		\
		&& NEWS_TAG=`misc/version.sh --news-tag1`		\
		&& mv ./NEWS.md ./NEWS.saved				\
		&& echo -e "## Anklang $$ARCHIVE_VERSION\n"		>  ./NEWS.md \
		&& echo '```````````````````````````````````````````'	>> ./NEWS.md \
		&& git log --pretty='%s    # %cd %an %h%n%w(0,4,4)%b' \
		       --first-parent --date=short "$$NEWS_TAG..HEAD"	>> ./NEWS.md \
		&& sed -e '/^\s*Signed-off-by:.*<.*@.*>/d'		-i ./NEWS.md \
		&& sed '/^\s*$$/{ N; /^\s*\n\s*$$/D }'			-i ./NEWS.md \
		&& echo '```````````````````````````````````````````'	>> ./NEWS.md \
		&& echo 						>> ./NEWS.md \
		&& git show HEAD:NEWS.md 				>> ./NEWS.md \
		&& $(MAKE) dist						\
		&& mv ./NEWS.saved ./NEWS.md
	@: # mkassets.sh - build binary release assets
	$Q export V=$V								\
	&& ARCHIVE_VERSION=`misc/version.sh | sed 's/ .*//'`			\
	&& misc/dbuild.sh misc/mkassets.sh $>/anklang-$$ARCHIVE_VERSION.tar.zst	\
	&& cp $>/build-version $>/nightly-version

# == upload-release ==
upload-release:
	$(QGEN)
	@: # Determine version (eval preceeds all shell commands)
	@ $(eval RELEASE_VERSION != cut '-d ' -f1 $>/build-version)
	@ $(eval NIGHTLY != cmp -s $>/build-version $>/nightly-version && echo true)
	@: # Check release tag
	$Q $(if $(NIGHTLY), true, false) \
	|| ( NEWS_TAG=`misc/version.sh --news-tag1` && test "$$NEWS_TAG" == "v$(RELEASE_VERSION)" )
	@: # Check release attachments
	$Q du -hs `cat $>/build-assets`
	@: # Create release message
	$Q echo $(if $(NIGHTLY), $(NIGHTLY_DISCLAIMER),		\
				 'Anklang $(RELEASE_VERSION)')	>  $>/release-message
	$Q sed '0,/^## /b; /^## /Q; ' $>/build-news		>> $>/release-message
	$Q echo							>> $>/release-message
	-$Q $(if $(NIGHTLY), gh release delete -y 'Nightly' )
	-$Q $(if $(NIGHTLY), git push origin ':Nightly' )
	$Q  $(if $(NIGHTLY), git push origin 'Nightly' )
	@: # Create Github release and upload assets
	$Q ASSETS=( `cat $>/build-assets` )		\
	&& gh release create				\
		$(if $(NIGHTLY), --prerelease, --draft)	\
		-F '$>/release-message'			\
		-t $(if $(NIGHTLY), 'Nightly Build', "v$(RELEASE_VERSION)" ) \
		$(if $(NIGHTLY), 'Nightly', "v$(RELEASE_VERSION)" ) \
		"$${ASSETS[@]}"
	$Q $(if $(NIGHTLY), true, false) \
	|| ( echo 'Run:' && echo '  git push origin "v$(RELEASE_VERSION)"' )
NIGHTLY_DISCLAIMER = "Development version - may contain bugs or compatibility issues."
