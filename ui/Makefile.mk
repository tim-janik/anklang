# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ui/*.d)

VITE_DEPS :=	# (intermediate) targets required by vite

# This Makefile creates needed inputs unde $>/gen and calls vite
# to bundle assets under $>/ui/.
# * make run - Build UI, start electron app
# * make serve - Run build server for ui/.
# * DevTools can be activated with Shft+Ctrl+I when run from the devleopment tree.

# == $>/gen/testcalls.g.ts ==
UI_TESTS_FILES := $(wildcard ui/tests/*ts)
$>/gen/testcalls.g.ts: ui/gen-testcalls.ts ui/Makefile.mk $(UI_TESTS_FILES)		| $>/gen/
	$(QGEN)
	$Q $(RUNTS) $< --out $@ $(UI_TESTS_FILES)
VITE_DEPS += $>/gen/testcalls.g.ts

# == $>/gen/ui/assets/testcalls-list.txt ==
$>/gen/ui/assets/testcalls-list.txt: ui/gen-testcalls.ts ui/Makefile.mk $(UI_TESTS_FILES)	| $>/gen/ui/assets/
	$(QGEN)
	$Q $(RUNTS) $< --list $(UI_TESTS_FILES) > $@.tmp
	$Q mv $@.tmp $@
VITE_DEPS += $>/gen/ui/assets/testcalls-list.txt

# == ui/tests/TestList.g.mk ==
ui/tests/TestList.g.mk: ui/gen-testcalls.ts ui/Makefile.mk $(UI_TESTS_FILES)
	$(QGEN)
	$Q echo 'UI_TEST_LIST := '\\			> $@.tmp
	$Q $(RUNTS) $< --list $(UI_TESTS_FILES) | sed 's/^/  /; s/$$/ \\/' >> $@.tmp
	$Q mv $@.tmp $@
ifneq (,$(shell find $(UI_TESTS_FILES) -newer ui/tests/TestList.g.mk))
.PHONY: ui/tests/TestList.g.mk		# update generated file without forcing rebuild of INPUTS
endif
include ui/tests/TestList.g.mk		# UI_TEST_LIST

# == check-ui-tests ==
.PHONY: check-ui-tests
define UI_TEST_CHECK
check-$1: $$>/gen/.vite.done $$(lib/AnklangSynthEngine)
	$$(QECHO) TEST '$1'
	$$Q $$(lib/AnklangSynthEngine) --norc --no-devices --ui-test '$1'
.PHONY: check-$1
check-ui-tests: check-$1
endef
$(foreach T, $(UI_TEST_LIST), $(eval $(call UI_TEST_CHECK,$T)))
CHECK_TARGETS += check-ui-tests

# == ui/assets/AnklangIcons.css ==
$>/gen/assets/AnklangIcons.css: ui/Makefile.mk $(EXTERNAL_BLOBS4ANKLANG_STAMPS)	| $>/gen/assets/
	$(QGEN)
	$Q rm -fr $>/gen/anklangicons/ && tar xf external/blobs4anklang/icons/anklangicons-201123.1.tgz -C $>/gen/
	$Q cd $>/gen/anklangicons/ && $(CP) AnklangIcons.woff2 ../assets/ && $(CP) AnklangIcons.css ../assets/AnklangIcons.css.tmp
	$Q sed -e 's|@font-face *{|@font-face { font-display: block; |' -i $>/gen/assets/AnklangIcons.css.tmp
	$Q rm -r $>/gen/anklangicons/ && mv $@.tmp $@
VITE_DEPS += $>/gen/assets/AnklangIcons.css

# == /gen/assets/ fonts ==
$>/gen/.ui.fonts: $(EXTERNAL_BLOBS4ANKLANG_STAMPS)
	$(QGEN)
	$Q $(CP) $(external/font_files) $>/gen/assets/
	$Q touch $@
VITE_DEPS += $>/gen/.ui.fonts

# == /gen/cursors/cursors.css ==
$>/gen/cursors/cursors.css: $(wildcard ui/cursors/*) Makefile.mk		| $>/gen/cursors/
	$(QECHO) COPY $<
	$Q for SVG in `sed -n "/url.'.gen.cursors\//{ s/.*('.gen.//; s/').*//; p }" ui/cursors/cursors.css` ; do \
		$(CP) ui/"$$SVG" $>/gen/cursors/ || break ; done
	$Q $(CP) ui/cursors/cursors.css $@
VITE_DEPS += $>/gen/cursors/cursors.css

# == /gen/assets/spinner.css ==
# Extract keyframes for rotating spinner.svg into CSS
$>/gen/assets/spinner.css: ui/assets/spinner.svg				| $>/ext/ui/assets/
	$(QGEN)
	$Q sed -rn '/@keyframe/,$${ p; /^\s*}\s*$$/q; }' $< > $@
VITE_DEPS += $>/gen/assets/spinner.css

# == gen/api-jsonipc.g.ts ==
$>/gen/api-jsonipc.g.ts: ase/gen/api-jsonipc.g.ts
	$(QECHO) COPY $@
	$Q $(CP) $< $@
VITE_DEPS += $>/gen/api-jsonipc.g.ts

# == /gen/assets/ knobs ==
$>/gen/assets/%: $>/images/knobs/%			| $>/gen/assets/
	$(QGEN)
	$Q $(CP) $< $@
VITE_DEPS += $>/gen/assets/cknob193u.png $>/gen/assets/cknob193b.png

# == /gen/assets/ images ==
ui/assets/images := $(wildcard ui/assets/*.svg)
ui/gen/targets   := $(ui/assets/images:ui/%=$>/gen/%)
$(ui/gen/targets): $>/gen/%: ui/%			| $>/gen/assets/
	$(QECHO) COPY $<
	$Q cd ui/ && $(CP) $(<:ui/%=%) --parents $(abspath $>/)/gen/
VITE_DEPS += $(ui/gen/targets)

# == $>/gen/ui/anklang.png ==
$>/gen/ui/anklang.png: ui/assets/favicon.svg ui/Makefile.mk	| $>/gen/ui/
	$(QGEN)
	$Q mkdir -p $>/gen/tmpanklangpng/
	$Q mogrify -density 600 -background transparent -resize 128x128 -format png -path $>/gen/tmpanklangpng/ $<
	$Q cp $>/gen/tmpanklangpng/favicon.png $>/gen/ui/favicon.ico
	$Q mv $>/gen/tmpanklangpng/favicon.png $@.tmp && rm -r $>/gen/tmpanklangpng/ && mv $@.tmp $@
VITE_DEPS += $>/gen/ui/anklang.png

# == $>/gen/ui/assets/favicon.svg ==
# Used by binary packages: $prefix/anklang-*/ui/assets/favicon.svg
$>/gen/ui/assets/favicon.svg: ui/assets/favicon.svg ui/Makefile.mk	| $>/gen/ui/assets/
	$(QGEN)
	$Q cp $< $@
VITE_DEPS += $>/gen/ui/assets/favicon.svg

# == ui/synsmell ==
ui/synsmell.files: $(filter ui/%. ui/b/%, $(WILDCARD_FILES)))
$>/.uisynsmell.done: misc/synsmell.ts $(ui/synsmell.files)				| node_modules/.npm.done
	$(QECHO) CHECK 'synsmell (ui/)'
	$Q $(RUNTS) $< --separate-body=0 $(ui/synsmell.files)
	$Q touch $@
$>/.uisynsmell.done: $(if $(filter check,$(MAKECMDGOALS)), FORCE) # force on 'make check'
check: $>/.uisynsmell.done

# == $>/gen/**/*.md - for doc/Makefile.mk ==
# ui/xbcomments.js ui/Makefile.mk node_modules/.npm.done	| $>/gen/b/
$>/gen/%.md: ui/%.js						| $>/gen/b/ node_modules/.npm.done
	$(QGEN)
	$Q node ui/xbcomments.js $< -O $(@D)
$>/gen/%.md: ui/%.jsx						| $>/gen/b/ node_modules/.npm.done
	$(QGEN)
	$Q node ui/xbcomments.js $< -O $(@D)
$>/gen/%.md: ui/%.tsx						| $>/gen/b/ node_modules/.npm.done
	$(QGEN)
	$Q node ui/xbcomments.js $< -O $(@D)

# == ui dist build ==
VITE_DEPS += $>/version.json $(wildcard ui/* ui/b/*)
$>/gen/.vite.done: vite.config.ts ui/index.html ui/css-functions.js ui/tests/css-tests.css ui/Makefile.mk $(VITE_DEPS)	| $>/tests/ node_modules/.npm.done
	@$(QECHO) BUILD "Vite Output"
	$Q BUILDDIR='$(abspath $>)' node_modules/.bin/vite -c vite.config.ts build -l warn --emptyOutDir
	$Q ln -fs anklang.png $>/ui/favicon.ico
	$Q gzip -f -9 $>/ui/assets/*.map
	$Q echo '.*/[.].*'		>> $>/ui/.aseignore
	$Q mv $>/ui/assets/css-tests-*.css $>/tests/css-tests.css \
	&& sed '1s/.*/set -Eeuo pipefail/; t; s/\*\/ *//; s/^ *\/\?\* *grep\b\(.*\)/grep \1 $$1/; /^grep/!s/.*//' \
		< $>/tests/css-tests.css > $>/tests/css-tests.sh
	$Q touch $@

# == ui/check-css-tests ==
ui/check-css-tests: $>/tests/css-tests.sh $>/tests/css-tests.css ui/tests/css-tests.css $>/gen/.vite.done
	@$(QECHO) CHECK '$@'
	$Q grep -q 'grep' $>/tests/css-tests.sh || { echo "ui/tests/css-tests.css: error: missing 'grep' assertions" ; false ; }
	$Q bash -x $>/tests/css-tests.sh $>/tests/css-tests.css > $>/tests/css-tests.out 2>&1 \
	|| { cat $>/tests/css-tests.out ; false ; } \
	&& echo '  PASS    ' $@
check: ui/check-css-tests

# == ui/tscheck ==
ui/tscheck: tsconfig.json
	$(QECHO) CHECK $@
	$Q node_modules/.bin/tsc -p $<
check: ui/tscheck

# == ui dist build ==
LATE_EVAL += $$(eval $$>/gen/.ui.done: $${doc/mkdocs/anklang.stamp})	# doc/mkdocs/anklang.stamp is assigned later
$>/gen/.ui.done: $>/gen/.vite.done
	@$(QECHO) BUILD "$>/ui/"
	$Q rm -rf $>/ui/anklang && cp -RL $>/mkdocs/anklang $>/ui/
	$Q touch $@
ALL_TARGETS += $>/gen/.ui.done

# == serve ==
serve: all
	$Q misc/serve.sh
.PHONY: serve

# == installation ==
ui/install: $>/gen/.ui.done
	@$(QECHO) INSTALL '$(DESTDIR)$(pkgdir)/ui'
	$Q rm -rf '$(DESTDIR)$(pkgdir)/ui'
	$Q $(INSTALL) -d $(DESTDIR)$(pkgdir)/ui
	$Q cp -RP $>/ui $(DESTDIR)$(pkgdir)/
.PHONY: ui/install
install: ui/install
ui/uninstall: FORCE
	@$(QECHO) REMOVE '$(DESTDIR)$(pkgdir)/ui'
	$Q rm -rf '$(DESTDIR)$(pkgdir)/ui'
.PHONY: ui/uninstall
uninstall: ui/uninstall
