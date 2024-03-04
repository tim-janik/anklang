# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ui/*.d)
ui/cleandirs ::= $(wildcard $>/ui/ $>/dist/)
CLEANDIRS         += $(ui/cleandirs)
ALL_TARGETS       += $>/.ui-build-stamp $>/.ui-reload-stamp
$>/.ui-build-stamp:	# essential targets without live reload
$>/.ui-reload-stamp:	# live reload targets

# This Makefile creates the web UI in $>/ui/.
# * make run - Build UI, start electron app
# * make serve - Run build server for ui/.
# * DevTools can be activated with Shft+Ctrl+I when run from the devleopment tree.
# make && (cd out/ui/dist/ && python -m SimpleHTTPServer 3333)

# == Copies ==
ui/jscopy.wildcards ::= $(wildcard	\
	ui/*.js				\
	ui/*.mjs			\
	ui/b/*.js			\
	ui/b/*.mjs			\
)
ui/cjs.wildcards ::= $(wildcard		\
)
ui/nocopy.wildcards ::= $(wildcard	\
	ui/*css				\
	ui/postcss.js			\
	ui/sfc-compile.js		\
	ui/slashcomment.js		\
)
ui/copy.files ::= $(filter-out $(ui/nocopy.wildcards) $(ui/cjs.wildcards), $(ui/jscopy.wildcards))
ui/vue.wildcards ::= $(wildcard ui/b/*.vue)
ui/public.wildcards ::= $(wildcard	\
	ui/assets/*.svg			\
	ui/favicon.ico			\
)

# == ui/lit.js ==
$>/ui/lit.js.map: $>/ui/lit.js ;
$>/ui/lit.js: ui/Makefile.mk node_modules/.npm.done					| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/lit.js* $@
	$Q for mod in $(ui/lit.modules) ; do echo "export * from '$$mod';" ; done	> $>/ui/lit-all.js
	$Q cd $>/ui/ && ../../node_modules/.bin/rollup -p @rollup/plugin-node-resolve lit-all.js -o lit.js --sourcemapFile lit.js.map -m
	$Q $(RM) $>/ui/lit-all.js
$>/.ui-build-stamp: $>/ui/lit.js
ui/lit.modules = $(strip	\
	lit lit/directives/live.js lit/directives/repeat.js lit/directives/ref.js \
	lit/directives/async-append.js lit/directives/async-replace.js lit/directives/cache.js lit/directives/choose.js lit/directives/class-map.js \
	lit/directives/guard.js lit/directives/if-defined.js lit/directives/join.js lit/directives/keyed.js lit/directives/map.js lit/directives/range.js \
	lit/directives/style-map.js lit/directives/template-content.js lit/directives/unsafe-html.js lit/directives/unsafe-svg.js lit/directives/until.js \
	lit/directives/when.js \
)

# == ui/vue.js ==
$>/ui/vue.js:	node_modules/.npm.done				| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/vue.js
	$Q $(CP) node_modules/vue/dist/$(UI/VUE-VARIANT.js) $>/ui/vue.js
	$Q sed -i $>/ui/vue.js \
		-e 's/^\s*\(console\.info(.You are running a development build of Vue\)/if(0) \1/' \
		-e 's/\b\(warn(`[^`]* was accessed during render\)/if(0) \1/'
$>/.ui-build-stamp: $>/ui/vue.js
UI/VUE-VARIANT.js ::= $(if $(__UIDEBUG__),vue.esm-browser.js,vue.esm-browser.prod.js)

# == ui/zcam-js.mjs ==
$>/ui/zcam-js.mjs: node_modules/.npm.done				| $>/ui/
	$(QGEN)
	$Q $(CP) node_modules/zcam-js/zcam-js.mjs $@
$>/.ui-build-stamp: $>/ui/zcam-js.mjs
$>/ui/colors.js: $>/ui/zcam-js.mjs

# == ui/index.html ==
$>/ui/index.html: ui/index.html $>/ui/global.css $>/ui/vue-styles.css node_modules/.npm.done		| $>/ui/
	@ $(eval ui/csshash != cat $>/ui/global.css $>/ui/vue-styles.css | sha256sum | sed 's/ *-//')
	$(QGEN)
	$Q rm -f $>/ui/doc && ln -s ../doc $>/ui/doc # do here, b/c MAKE is flaky in tracking symlink timestamps
	$Q echo '    { "config": { $(strip $(PACKAGE_VERSIONS)),'				> $>/ui/config.json
	$Q sed -nr '/^ *"version":/{s/.*: *"(.*)",/    "lit_version": "\1" /;p}' \
		node_modules/lit/package.json						>>$>/ui/config.json
	$Q echo '    } }'									>>$>/ui/config.json
	$Q sed -r \
		-e "/<script type='application\/json' id='--EMBEDD-config_json'>/ r $>/ui/config.json" \
		-e "s/@--CSSHASH--@/$(ui/csshash)/g" \
		< $<	> $@.tmp
	$Q rm $>/ui/config.json
	$Q sed -r \
		-e "/^<\?xml .*\?>\s*$$/d" \
		-i $@.tmp
	$Q mv $@.tmp $@
$>/.ui-reload-stamp: $>/ui/index.html
# remove ::KEEPIF="__DEV__" directives
ui/sed-keepif ::= $(if __DEV__, -e '/<[^<>]*::KEEPIF="__DEV__"/s/::KEEPIF="__DEV__"//')
# delete unmatched ::KEEPIF="" tags
ui/sed-keepif  += -e 's/<[^<>]*::KEEPIF="[^"]*"[^<>]*>//'

# == knob sprites ==
$>/ui/assets/%: $>/images/knobs/%			| $>/ui/assets/
	$(QGEN)
	$Q $(CP) $< $@
$>/.ui-build-stamp: $>/ui/assets/cknob193u.png $>/ui/assets/cknob193b.png

# == ui/.aseignore ==
$>/ui/.aseignore:					| $>/ui/
	$(QGEN)
	$Q rm -f $@.tmp
	$Q echo '.*/[.].*'			>> $@.tmp
	$Q mv $@.tmp $@
$>/.ui-build-stamp: $>/ui/.aseignore

# == ui/aseapi.js ==
$>/ui/aseapi.js: jsonipc/jsonipc.js ase/api.hh $(lib/AnklangSynthEngine) ui/Makefile.mk	| $>/ui/
	$(QGEN)
	$Q $(CP) $< $@.tmp
	$Q ASAN_OPTIONS=detect_leaks=0 $(lib/AnklangSynthEngine) --js-api			>> $@.tmp
	$Q echo '/**@type{ServerImpl}*/'							>> $@.tmp
	$Q echo 'export let server = Jsonipc.setup_promise_type (Server, s => server = s);'	>> $@.tmp
	$Q mv $@.tmp $@
$>/.ui-build-stamp: $>/ui/aseapi.js

# == ui/b/vuejs.targets ==
ui/b/vuejs.targets ::= $(ui/vue.wildcards:%.vue=$>/%.js)
$(ui/b/vuejs.targets): ui/sfc-compile.js
$(ui/b/vuejs.targets): $>/%.js: %.vue			| $>/ui/b/ node_modules/.npm.done
	$(QGEN)
	$Q node ui/sfc-compile.js --debug -I $>/ui/ $< -O $(@D)
$>/.ui-reload-stamp: $(ui/b/vuejs.targets)

# == vue-styles.css ==
ui/b/vuecss.targets ::= $(ui/vue.wildcards:%.vue=$>/%.vuecss)
$(ui/b/vuecss.targets): $(ui/b/vuejs.targets) ;
$>/ui/vue-styles.css: $(ui/b/vuecss.targets) ui/stylelintrc.cjs ui/Makefile.mk
	$(QGEN)
	$Q echo '@charset "UTF-8";'					>  $@.vuecss
	$Q echo "@import 'mixins.scss';"				>> $@.vuecss
	$Q for f in $(ui/b/vuecss.targets:$>/ui/b/%=%) ; do		\
		echo "@import 'b/$${f}';"				>> $@.vuecss \
		|| exit 1 ; done
	$Q node ui/postcss.js --test $V # CHECK transformations
	$Q cd $>/ui/ && node $(abspath ui/postcss.js) --map -Dthemename_scss=dark.scss -I ../../ui/ -i $(@F).vuecss $(@F).tmp
	-$Q cd $>/ && $(abspath node_modules/.bin/stylelint) $${INSIDE_EMACS:+-f unix} -c $(abspath ui/stylelintrc.cjs) ui/b/*.vuecss \
	|& sed -r 's|/[^ :]*/(ui/b/[^ /:]+)\.vuecss:|\1.vue:|'
	$Q $(RM) $@.vuecss && mv $@.tmp $@
$>/.ui-reload-stamp: $>/ui/vue-styles.css

# == UI/GLOBALSCSS_IMPORTS ==
UI/GLOBALSCSS_IMPORTS =
# Material-Icons
$>/ui/material-icons.css: ui/Makefile.mk		| $>/ui/ node_modules/.npm.done
	$(QGEN)
	$Q grep -q '/material-icons.woff2' node_modules/material-icons/iconfont/filled.css || \
		{ echo "$<: failed to find font in node_modules/material-icons/iconfont/" >&2 ; false ; }
	$Q cp node_modules/material-icons/iconfont/material-icons.woff2 $>/ui/material-icons.woff2
	$Q sed -re 's|\boptimizeLegibility\b|optimizelegibility|g' \
		node_modules/material-icons/iconfont/filled.css > $@.tmp
	$Q mv $@.tmp $@
UI/GLOBALSCSS_IMPORTS += $>/ui/material-icons.css
# AnklangIcons
$>/ui/assets/AnklangIcons.css: ui/Makefile.mk			| $>/ui/assets/
	$(QGEN)
	$Q rm -fr $>/ui/anklangicons/ && tar xf external/blobs4anklang/icons/anklangicons-201123.1.tgz -C $>/ui/
	$Q cd $>/ui/anklangicons/ && $(CP) AnklangIcons.woff2 ../assets/ && $(CP) AnklangIcons.css ../assets/AnklangIcons.css.tmp
	$Q sed -e 's|@font-face *{|@font-face { font-display: block; |' -i $>/ui/assets/AnklangIcons.css.tmp
	$Q rm -r $>/ui/anklangicons/ && mv $@.tmp $@
UI/GLOBALSCSS_IMPORTS += $>/ui/assets/AnklangIcons.css
# Fork-Awesome
$>/ui/assets/fork-awesome.css: ui/Makefile.mk		| node_modules/.npm.done $>/ui/assets/
	$(QGEN)
	$Q $(CP) node_modules/fork-awesome/fonts/forkawesome-webfont.woff2 $>/ui/assets/
	$Q sed  -e "/^ *src: *url/s,src: *url(.*);,src: url('forkawesome-webfont.woff2');," \
		-e 's|@font-face *{|@font-face { font-display: block; |' \
		node_modules/fork-awesome/css/fork-awesome.css > $@.tmp
	$Q mv $@.tmp $@
UI/GLOBALSCSS_IMPORTS += $>/ui/assets/fork-awesome.css
# ui/cursors/
$>/ui/cursors/cursors.css: $(wildcard ui/cursors/*) Makefile.mk		| $>/ui/cursors/
	$(QECHO) COPY $<
	$Q for SVG in `sed -n "/url.'cursors\//{ s/.*('//; s/').*//; p }" ui/cursors/cursors.css` ; do \
		$(CP) ui/"$$SVG" $>/ui/cursors/ || break ; done
	$Q $(CP) ui/cursors/cursors.css $@
UI/GLOBALSCSS_IMPORTS += $>/ui/cursors/cursors.css
# ui/spinner.svg
$>/ui/spinner.scss: ui/assets/spinner.svg
	$(QGEN)
	$Q sed -rn '/@keyframe/,$${ p; /^\s*}\s*$$/q; }' $< > $@
UI/GLOBALSCSS_IMPORTS += $>/ui/spinner.scss

# == ui/global.css ==
ui/b/js.files := $(wildcard ui/b/*.js)
ui/tailwind.inputs := $(wildcard ui/*.html ui/*.css ui/*.scss ui/*.js ui/b/*.js ui/b/*.vue $(ui/b/js.files))
$>/ui/global.css: ui/global.scss $(ui/tailwind.inputs) ui/jsextract.js ui/stylelintrc.cjs $(UI/GLOBALSCSS_IMPORTS)
	$(QGEN)
	$Q $(CP) $< $>/ui/global.scss
	$Q node ui/jsextract.js -O $>/ui/b/ $(ui/b/js.files)		# do node ui/jsextract.js $$f -O "$>/$${f%/*}"
	$Q for f in $(ui/b/js.files:$>/ui/%=%) ; do \
		echo "@import '../$$f""css';" >> $>/ui/global.scss || exit 1 ; done
	$Q node ui/postcss.js --test $V # CHECK transformations
	$Q node ui/postcss.js --map -Dthemename_scss=dark.scss -I ui/ $>/ui/global.scss $@.tmp
	-$Q node_modules/.bin/stylelint $${INSIDE_EMACS:+-f unix} -c ui/stylelintrc.cjs $(wildcard ui/*.*css)
	-$Q cd $>/ && $(abspath node_modules/.bin/stylelint) $${INSIDE_EMACS:+-f unix} -c $(abspath ui/stylelintrc.cjs) ui/b/*.jscss \
	|& sed -r 's|/[^ :]*/(ui/b/[^ /:]+)\.jscss:|\1.js:|'
	$Q rm -f $>/ui/*.jscss $>/ui/b/*.jscss $>/ui/global.scss
	$Q mv $@.tmp $@
$>/.ui-reload-stamp: $>/ui/global.css

# == all-components.js ==
$>/ui/all-components.js: ui/Makefile.mk $(ui/b/vuejs.targets) $(wildcard ui/b/*)	| $>/ui/
	$(QGEN)
	$Q echo '/* Generated by: make $@ */'			>  $@.tmp
	$Q for fjs in $$(cd ui/ && echo b/*.js) ; do			\
		echo "import './$$fjs';" ||				\
		exit 1 ; done					>> $@.tmp
	$Q echo "export default {"				>  $@.tmp2
	$Q for fvue in $$(cd ui/ && echo b/*.vue) ; do			\
		base=$${fvue%.*} && id=$${base//[^a-z0-9A-Z]/_} &&	\
		echo "import $$id""_vue from './$$base.js';" &&		\
		echo "  '$${base//\//-}': $$id""_vue,"	>> $@.tmp2 ||	\
		exit 1 ; done					>> $@.tmp
	$Q echo "};"						>> $@.tmp2
	$Q cat $@.tmp2 >> $@.tmp && $(RM) $@.tmp2
	$Q mv $@.tmp $@
$>/.ui-reload-stamp: $>/ui/all-components.js

# == File Copies ==
ui/copy.targets ::= $(ui/copy.files:%=$>/%)
$(ui/copy.targets): $>/ui/%: ui/%	| $>/ui/b/
	$(QECHO) COPY $@
	$Q $(CP) $< --parents $>/
$>/.ui-reload-stamp: $(ui/copy.targets)

# == Copies to ui/ ==
ui/public.targets ::= $(ui/public.wildcards:ui/%=$>/ui/%)
$(ui/public.targets): $>/ui/%: ui/%			| $>/ui/assets/
	$(QECHO) COPY $<
	$Q cd ui/ && $(CP) $(<:ui/%=%) --parents $(abspath $>/)/ui/
$>/.ui-reload-stamp: $(ui/public.targets)

# == CJS Files ==
ui/cjs.targets ::= $(ui/cjs.wildcards:%.js=$>/%.cjs)
$(ui/cjs.targets): $>/ui/%.cjs: ui/%.js	| $>/ui/b/
	$(QECHO) COPY $@
	$Q $(CP) $< $@
$>/.ui-reload-stamp: $(ui/cjs.targets)

# == Inter Typeface ==
$>/ui/InterVariable.woff2: external/blobs4anklang/fonts/InterVariable.woff2	| $>/ui/
	$(QGEN)
	$Q $(CP) $< $@
$>/.ui-build-stamp: $>/ui/InterVariable.woff2

# == $>/ui/browserified.js ==
$>/ui/browserified.js: node_modules/.npm.done	| ui/Makefile.mk $>/ui/
	$(QGEN)
	$Q: # bundle and re-export module for the browser
	$Q mkdir -p $>/ui/tmp-browserify/
	$Q echo "const modules = {"								>  $>/ui/tmp-browserify/requires.js
	$Q for mod in \
		markdown-it \
		; do \
		echo "  '$${mod}': require ('$$mod')," ; done					>> $>/ui/tmp-browserify/requires.js
	$Q echo "};"										>> $>/ui/tmp-browserify/requires.js
	$Q echo "const browserify_require = m => modules[m] || console.error ('Unknown module:', m);"	>> $>/ui/tmp-browserify/requires.js
	$Q echo "Object.defineProperty (window, 'require', { value: browserify_require });"		>> $>/ui/tmp-browserify/requires.js
	$Q echo "window.require.modules = modules;"						>> $>/ui/tmp-browserify/requires.js
	$Q node_modules/.bin/browserify --debug -o $>/ui/tmp-browserify/browserified.long.js $>/ui/tmp-browserify/requires.js
	$Q node_modules/.bin/terser --source-map content=inline --comments false $>/ui/tmp-browserify/browserified.long.js -o $>/ui/tmp-browserify/browserified.min.js
	$Q mv $>/ui/tmp-browserify/browserified.min.js.map $@.map
	$Q mv $>/ui/tmp-browserify/browserified.min.js $@
	$Q rm -r $>/ui/tmp-browserify/
$>/.ui-build-stamp: $>/ui/browserified.js

# == $>/ui/favicon.ico ==
$>/ui/favicon.ico: ui/assets/favicon.svg node_modules/.npm.done ui/Makefile.mk	| $>/ui/
	$(QGEN)
	$Q mkdir -p $>/ui/tmp-icongen/
	$Q node_modules/.bin/icon-gen -i $< -o $>/ui/tmp-icongen/ --favicon --favicon-png-sizes 128 --favicon-ico-sizes 128 # -r
	$Q cd $>/ui/tmp-icongen/ && mv favicon-128.png ../anklang.png && mv favicon.ico ../favicon.ico.tmp
	$Q rm -r $>/ui/tmp-icongen/ && mv $@.tmp $@
$>/ui/anklang.png: $>/ui/favicon.ico
$>/.ui-build-stamp: $>/ui/favicon.ico $>/ui/anklang.png

# == eslint ==
ui/eslint.files ::= $(wildcard ui/*.html ui/*.js ui/b/*.js ui/b/*.vue)
$>/.eslint.done: ui/eslintrc.cjs $(ui/eslint.files) ui/Makefile.mk node_modules/.npm.done	| $>/ui/
	$(QECHO) RUN eslint
	$Q node_modules/.bin/eslint --no-eslintrc -c ui/eslintrc.cjs -f unix --cache --cache-location $>/.eslintcache \
		$(abspath $(ui/eslint.files)) |& ./misc/colorize.sh
	$Q touch $@
$>/.ui-reload-stamp: $>/.eslint.done
eslint: node_modules/.npm.done
	$Q rm -f $>/.eslint.done
	$Q $(MAKE) $>/.eslint.done
.PHONY: eslint
CLEANFILES += .eslintcache

# == tscheck ==
ui/tscheck.deps ::= $(wildcard ui/*.js ui/*/*.js) $(wildcard $>/ui/*.js $>/ui/*/*.js)
tscheck $>/.tscheck.done: ui/types.d.ts ui/tsconfig.json $(ui/tscheck.deps) node_modules/.npm.done | $>/.ui-build-stamp $>/tscheck/
	$(QECHO) RUN tscheck
	@ # tsc *.js needs to find node_modules/ in the directory hierarchy ("moduleResolution": "node")
	$Q cp ui/tsconfig.json ui/types.d.ts $>/
	-$Q (cd $>/ && ../node_modules/.bin/tsc -p tsconfig.json $${INSIDE_EMACS:+--pretty false}) \
	&& touch $>/.tscheck.done
$>/.ui-reload-stamp: $>/.tscheck.done
.PHONY: tscheck
CLEANDIRS += $>/tscheck/

# == ui/lint ==
ui/lint: 									| node_modules/.npm.done
	$(QGEN)
	$(MAKE) --no-print-directory NPMBLOCK=y -j1 \
		tscheck eslint $>/ui/global.css
	-$Q node_modules/.bin/stylelint $${INSIDE_EMACS:+-f unix} -c ui/stylelintrc.cjs $(wildcard ui/*.*css ui/b/*.*css)
	-$Q { TCOLOR=--color=always ; tty -s <&1 || TCOLOR=; } \
	&& grep $$TCOLOR -nE '(/[*/]+[*/ ]*)?(FI[X]ME).*' -r ui/ --exclude '*.js'
	$Q misc/synsmell.py --separate-body=0 ui/*js ui/b/*.js
.PHONY: ui/lint
lint: ui/lint

# == $>/doc/b/*.md ==
$>/doc/b/.doc-stamp: $(wildcard ui/b/*.js) ui/xbcomments.js ui/Makefile.mk node_modules/.npm.done	| $>/doc/b/
	$(QGEN)
	$Q node ui/xbcomments.js $(wildcard ui/b/*.js) -O $>/doc/b/
	$Q touch $@
$>/.ui-reload-stamp: $>/doc/b/.doc-stamp $>/doc/anklang-manual.html $>/doc/anklang-internals.html

# == serve ==
serve: all $>/.ui-build-stamp
	$Q cd $>/ui/ && npm run serve
.PHONY: serve

# == ui/rebuild ==
ui/rebuild:
	@: # incremental rebuild targets (without npm) that must succeed
	$(MAKE) --no-print-directory $>/.ui-build-stamp NPMBLOCK=y -j`nproc`
	@: # rebuild live reload targets
	$(MAKE) $>/.ui-reload-stamp NPMBLOCK=y -j`nproc` |& tee $>/ui-build.log || ( : \
		&& echo '<html><head><title>anklang/ui: make error</title></head><body><pre>' \
		&& cat out/ui-build.log && echo '</pre>' \
		&& echo '<script>setTimeout(_=>window.location.reload(), 3000)</script></body></html>' \
		) > $>/ui/index.html
	@: # close open sockets, only works if *same* executable still runs
	-killall -s USR2 -u $(USER) -- $(abspath $(lib/AnklangSynthEngine))
.PHONY: ui/rebuild

# == installation ==
ui/installdir ::= $(DESTDIR)$(pkgdir)/ui
ui/install.pattern ::= $(strip	\
	$>/ui/.aseignore	\
	$>/ui/*.woff2		\
	$>/ui/*.css		\
	$>/ui/*.html		\
	$>/ui/*.ico		\
	$>/ui/*.js		\
	$>/ui/*.mjs		\
	$>/ui/*.png		\
)
ui/b/install.pattern ::= $(strip \
	$>/ui/b/*.js		\
)
ui/install: $>/.ui-build-stamp $>/.ui-reload-stamp
	@$(QECHO) INSTALL '$(ui/installdir)/.'
	$Q rm -f -r '$(ui/installdir)'
	$Q $(INSTALL)      -d $(ui/installdir)/ $(ui/installdir)/assets/ $(ui/installdir)/b/
	$Q $(INSTALL_DATA) -p $(ui/install.pattern) $(ui/installdir)/
	$Q $(INSTALL_DATA) -p $>/ui/assets/* $(ui/installdir)/assets/
	$Q $(INSTALL_DATA) -p $(ui/b/install.pattern) $(ui/installdir)/b/
	$Q ln -s ../doc $(ui/installdir)/doc
.PHONY: ui/install
install: ui/install
ui/uninstall: FORCE
	@$(QECHO) REMOVE '$(ui/installdir)/.'
	$Q rm -f -r '$(ui/installdir)'
.PHONY: ui/uninstall
uninstall: ui/uninstall
