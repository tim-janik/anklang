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

# == appimage tools ==
$>/misc/appaux/appimagetool/AppRun:					| $>/misc/appaux/
	$(QGEN) # Fetch and extract AppImage tools
	$Q cd $>/misc/appaux/ && \
		curl -sfSOL https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage && \
		curl -sfSOL https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage && \
		chmod +x linuxdeploy-x86_64.AppImage appimagetool-x86_64.AppImage && \
		rm -rf squashfs-root linuxdeploy appimagetool && \
		./linuxdeploy-x86_64.AppImage  --appimage-extract && mv -v squashfs-root/ ./linuxdeploy && \
		./appimagetool-x86_64.AppImage --appimage-extract && mv -v squashfs-root/ ./appimagetool && \
		rm linuxdeploy-x86_64.AppImage appimagetool-x86_64.AppImage

# == appimage ==
APPINST = $>/appinst/
APPBASE = $>/appbase/
appimage: all $>/misc/appaux/appimagetool/AppRun				| $>/misc/bin/
	$(QGEN)
	$Q echo "  CHECK   " "for AppImage build with prefix=/usr"
	$Q test '$(prefix)' = '/usr' || { echo "$@: assertion failed: prefix=$(prefix)" >&2 ; false ; }
	@: # Installation Step
	@echo '  INSTALL ' AppImage files
	$Q rm -fr $(APPINST) $(APPBASE) && make install DESTDIR=$(APPINST)
	@: # Populate appinst/, linuxdeploy expects libraries under usr/lib, binaries under usr/bin, etc
	@: # We achieve that by treating the anklang-$MAJOR-$MINOR/ installation directory as /usr/.
	@: # Also, we hand-pick extra libs for Anklang to keep the AppImage small.
	$Q mkdir $(APPBASE)
	$Q cp -a $(APPINST)/usr/lib/anklang-* $(APPBASE)/usr
	$Q rm -f Anklang-x86_64.AppImage
	@echo '  RUN     ' linuxdeploy ...
	$Q if test -e /usr/lib64/libc_nonshared.a ; \
	   then LIB64=/usr/lib64/ ; \
	   else LIB64=/usr/lib/x86_64-linux-gnu/ ; fi \
	   && LD_LIBRARY_PATH=$(APPBASE)/usr/lib/ \
	     $>/misc/appaux/linuxdeploy/AppRun \
		$(if $(findstring 1, $(V)), -v1, -v2) \
		--appdir=$(APPBASE) \
		-l $$LIB64/libXss.so.1 \
		-l $$LIB64/libXtst.so.6 \
		-i $(APPBASE)/usr/ui/anklang.png \
		-e $(APPBASE)/usr/bin/anklang \
		--custom-apprun=misc/AppRun
	@: # 'linuxdeploy -e usr/bin/anklang' turns this symlink into an executable copy, which electron does not support
	$Q rm $(APPBASE)/usr/bin/anklang && cp -auv $(APPINST)/usr/lib/anklang-*/bin/* $(APPBASE)/usr/bin/	# restore bin/* links
	@: # linuxdeploy collects too many libs for electron/anklang, remove duplictaes of electron/lib*.so
	$Q cd $(APPBASE)/usr/lib/ && rm -f $(notdir $(wildcard $(APPBASE)/usr/electron/lib*.so*))
	@: # Create AppImage executable
	@echo '  RUN     ' appimagetool ...
	$Q ARCH=x86_64 $>/misc/appaux/appimagetool/AppRun -n $(if $(findstring 1, $(V)), -v) $(APPBASE) # XZ_OPT=-9e --comp=xz
	$Q mv Anklang-x86_64.AppImage $>/anklang-$(version_short)-x64.AppImage
	$Q ls -l -h --color=auto $>/anklang-*-x64.AppImage
.PHONY: appimage

# == installation ==
misc/desktop/installdir ::= $(DESTDIR)$(pkgsharedir)/applications
misc/install: misc/anklang.desktop
	@$(QECHO) INSTALL '$(misc/desktop/installdir)/...'
	$Q rm -f -r '$(misc/desktop/installdir)'
	$Q $(INSTALL)      -d $(misc/desktop/installdir)/
	$Q $(INSTALL_DATA) -p misc/anklang.desktop $(misc/desktop/installdir)/
.PHONY: misc/install
install: misc/install
misc/uninstall: FORCE
	@$(QECHO) REMOVE '$(misc/desktop/installdir)/...'
	$Q rm -f -r '$(misc/desktop/installdir)'
.PHONY: misc/uninstall
uninstall: misc/uninstall
