# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ui/*.d)
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
	ui/*.scss			\
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
$>/ui/lit.js.map.gz: $>/ui/lit.js ;
$>/ui/lit.js: ui/Makefile.mk node_modules/.npm.done					| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/lit.js* $@
	$Q for mod in $(ui/lit.modules) ; do echo "export * from '$$mod';" ; done	> $>/ui/lit-all.js
	$Q cd $>/ui/ && ../../node_modules/.bin/rollup -p @rollup/plugin-node-resolve lit-all.js -o lit.js --sourcemapFile lit.js.map -m
	$Q gzip -f -9 $@.map
	$Q $(RM) $>/ui/lit-all.js
$>/.ui-build-stamp: $>/ui/lit.js
ui/lit.modules = $(strip	\
	lit lit/directives/live.js lit/directives/repeat.js lit/directives/ref.js \
	lit/directives/async-append.js lit/directives/async-replace.js lit/directives/cache.js lit/directives/choose.js lit/directives/class-map.js \
	lit/directives/guard.js lit/directives/if-defined.js lit/directives/join.js lit/directives/keyed.js lit/directives/map.js lit/directives/range.js \
	lit/directives/style-map.js lit/directives/template-content.js lit/directives/unsafe-html.js lit/directives/unsafe-svg.js lit/directives/until.js \
	lit/directives/when.js \
)

# == ui/signal-polyfill.js ==
$>/ui/signal-polyfill.js.map.gz: $>/ui/signal-polyfill.js ;
$>/ui/signal-polyfill.js: ui/Makefile.mk node_modules/.npm.done					| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/signal-polyfill.js* $@
	$Q for mod in signal-polyfill ; do echo "export * from '$$mod';" ; done	> $>/ui/signal-all.js
	$Q cd $>/ui/ && ../../node_modules/.bin/rollup -p @rollup/plugin-node-resolve signal-all.js -o $(@F) --sourcemapFile $(@F).map -m
	$Q gzip -f -9 $@.map
	$Q $(RM) $>/ui/signal-all.js
$>/.ui-build-stamp: $>/ui/signal-polyfill.js

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

# == ui/b/*.js ==
ui/js.internal := $(strip \
	ui/colors.js ui/sfc-compile.js ui/jsextract.js ui/stylelintrc.cjs ui/eslintrc.js \
	ui/tailwind.config.mjs ui/xbcomments.js \
)
ui/js.sources = $(filter-out $(ui/js.internal), $(filter ui/%.js, $(LS_TREE_LST)))
ui/js.targets = $(ui/js.sources:%=$>/%)
$>/ui/%.js: ui/%.js								| $>/ui/
	$(QECHO) COPY $@
	$Q cp $< $@
$>/ui/index.html: $(ui/js.targets)

# == ui/index.html ==
$>/ui/index.html: ui/index.html $>/ui/global.css node_modules/.npm.done		| $>/ui/
	@ $(eval ui/csshash != sha256sum < $>/ui/global.css | sed -r 's/^([0-9a-f]{12}).*/\1/i')
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
	$Q ASAN_OPTIONS=detect_leaks=0 \
	$(lib/AnklangSynthEngine) --norc  -P null -M null --js-api				>> $@.tmp
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

# == ui/material-icons.css ==
$>/ui/material-icons.css: ui/Makefile.mk			| node_modules/.npm.done $>/ui/
	$(QGEN)
	$Q grep -q '/material-icons.woff2' node_modules/material-icons/iconfont/filled.css || \
		{ echo "$<: failed to find font in node_modules/material-icons/iconfont/" >&2 ; false ; }
	$Q cp node_modules/material-icons/iconfont/material-icons.woff2 $>/ui/material-icons.woff2
	$Q sed -re 's|\boptimizeLegibility\b|optimizelegibility|g' \
		node_modules/material-icons/iconfont/filled.css > $@.tmp
	$Q mv $@.tmp $@
$>/.ui-reload-stamp: $>/ui/material-icons.css

# == ui/assets/AnklangIcons.css ==
$>/ui/assets/AnklangIcons.css: ui/Makefile.mk			| $>/ui/assets/
	$(QGEN)
	$Q rm -fr $>/ui/anklangicons/ && tar xf external/blobs4anklang/icons/anklangicons-201123.1.tgz -C $>/ui/
	$Q cd $>/ui/anklangicons/ && $(CP) AnklangIcons.woff2 ../assets/ && $(CP) AnklangIcons.css ../assets/AnklangIcons.css.tmp
	$Q sed -e 's|@font-face *{|@font-face { font-display: block; |' -i $>/ui/assets/AnklangIcons.css.tmp
	$Q rm -r $>/ui/anklangicons/ && mv $@.tmp $@
$>/.ui-reload-stamp: $>/ui/assets/AnklangIcons.css

# == ui/assets/fork-awesome.css ==
$>/ui/assets/fork-awesome.css: ui/Makefile.mk			| node_modules/.npm.done $>/ui/assets/
	$(QGEN)
	$Q $(CP) node_modules/fork-awesome/fonts/forkawesome-webfont.woff2 $>/ui/assets/
	$Q sed  -e "/^ *src: *url/s,src: *url(.*);,src: url('forkawesome-webfont.woff2');," \
		-e 's|@font-face *{|@font-face { font-display: block; |' \
		node_modules/fork-awesome/css/fork-awesome.css > $@.tmp
	$Q mv $@.tmp $@
$>/.ui-reload-stamp: $>/ui/assets/fork-awesome.css

# == ui/cursors/cursors.css ==
$>/ui/cursors/cursors.css: $(wildcard ui/cursors/*) Makefile.mk		| $>/ui/cursors/
	$(QECHO) COPY $<
	$Q for SVG in `sed -n "/url.'cursors\//{ s/.*('//; s/').*//; p }" ui/cursors/cursors.css` ; do \
		$(CP) ui/"$$SVG" $>/ui/cursors/ || break ; done
	$Q $(CP) ui/cursors/cursors.css $@
$>/.ui-reload-stamp: $>/ui/cursors/cursors.css

# == ext/spinner.scss ==
$>/ext/spinner.scss: ui/assets/spinner.svg				| $>/ext/ui/assets/
	$(QGEN)
	$Q sed -rn '/@keyframe/,$${ p; /^\s*}\s*$$/q; }' $< > $@
$>/ui/global.css: $>/ext/spinner.scss

# == ext/ui/b/*.js ==
ui/b/js.files     := $(wildcard ui/b/*.js)
ext/ui/b/js.files := $(ui/b/js.files:%=$>/ext/%)
$(ext/ui/b/js.files): $>/ext/ui/b/.jsstamp
$>/ext/ui/b/.jsstamp: $(ui/b/js.files) ui/jsextract.js			| $>/ext/ui/b/
	$(QECHO) EXTRACT 'ext/ui/b/*.js'
	$Q node ui/jsextract.js -O $>/ext/ui/b/ $(ui/b/js.files)
	$Q touch $@
ext/ui/lint: $>/ext/ui/b/.jsstamp
	-$Q cd $>/ext/ \
	&& $(abspath node_modules/.bin/stylelint) -c $(abspath ui/stylelintrc.cjs) \
		$${INSIDE_EMACS:+-f unix} $(ext/ui/b/js.files:$>/ext/%=%) |& \
		sed -r 's|^/[^ :]*/(ui/b/)|\1|'
.PHONY: ext/ui/lint
ui/lint: ext/ui/lint

# == ui/global.css ==
ui/b/vuecss.targets ::= $(ui/vue.wildcards:%.vue=$>/%.vuecss)
$(ui/b/vuecss.targets): $(ui/b/vuejs.targets) ;
ui/tailwind.inputs := $(wildcard ui/*.html ui/*.*css ui/*.*js ui/b/*.*js ui/b/*.vue)
$>/ui/global.css: ui/global.scss $(ui/tailwind.inputs) $(ext/ui/b/js.files) ui/stylelintrc.cjs ui/postcss.config.mjs $(UI/GLOBALSCSS_IMPORTS) $(ui/b/vuecss.targets)	| $>/ui/
	$(QGEN)
	$Q echo '@charset "UTF-8";'				>  $>/ext/imports.scss
	$Q echo "@import 'ui/dark.scss';"			>> $>/ext/imports.scss
	$Q echo "@import 'ui/global.scss';"			>> $>/ext/imports.scss
	$Q for f in $(ui/b/vuecss.targets:$>/ui/b/%=%) ; do		\
	    echo "@import '$>/ui/b/$${f}';" || exit 1 ; done	>> $>/ext/imports.scss
	$Q for f in $(ext/ui/b/js.files); do \
	    echo "@import '$$f';" || exit 1 ; done		>> $>/ext/imports.scss
	$Q POSTCSS_IMPORT_PATH=.:$>/ext \
	node_modules/.bin/postcss --config ui < $>/ext/imports.scss -o $>/ext/ui/global.css
	$Q gzip -f -9 $>/ext/ui/global.css.map
	$Q mv $>/ext/ui/global.css* $(@D)
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

# == $>/ui/markdown-it.mjs ==
$>/ui/markdown-it.mjs: node_modules/.npm.done	| $>/ui/
	$(QGEN)
	$q echo 'import dflt from "markdown-it"; export default dflt;' > $@.js	\
	&& node_modules/.bin/rollup -f es --sourcemap \
		-p @rollup/plugin-node-resolve -p "terser={output:{beautify:false}}" \
		-o $@ $@.js \
	&& gzip -f -9 $@.map \
	&& rm -f $@.js
$>/.ui-build-stamp: $>/ui/markdown-it.mjs

# == $>/ui/anklang.png ==
$>/ui/anklang.png: ui/assets/favicon.svg ui/Makefile.mk	| $>/ui/
	$(QGEN)
	$Q mkdir -p $>/ui-tmpanklpng/
	$Q mogrify -density 600 -background transparent -resize 128x128 -format png -path $>/ui-tmpanklpng/ $<
	$Q mv $>/ui-tmpanklpng/favicon.png $@.tmp && rm -r $>/ui-tmpanklpng/ && mv $@.tmp $@
$>/.ui-build-stamp: $>/ui/anklang.png

# == $>/ui/favicon.ico ==
$>/ui/favicon.ico: $>/ui/anklang.png
	$(QGEN)
	$Q ln -fs $(<F) $@
$>/.ui-build-stamp: $>/ui/favicon.ico

# == eslint ==
x11test.js ::= $(wildcard x11test/*.*js)
ui/eslint.files ::= $(wildcard ui/*.html ui/*.js ui/b/*.js)
$>/.eslint.done: ui/eslintrc.js $(ui/eslint.files) ui/Makefile.mk node_modules/.npm.done	| $>/ui/
	$(QECHO) RUN eslint
	$Q node_modules/.bin/eslint -c ui/eslintrc.js -f unix --cache --cache-location $>/.eslintcache \
		$(abspath $(ui/eslint.files) jsonipc/jsonipc.js $(x11test.js)) \
	|& ./misc/colorize.sh
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
	$(MAKE) --no-print-directory NPMBLOCK=y -j1 eslint tscheck
	-$Q node_modules/.bin/stylelint $${INSIDE_EMACS:+-f unix} -c ui/stylelintrc.cjs $(wildcard ui/*.*css ui/b/*.*css)
	$Q misc/synsmell.py --separate-body=0 ui/*.* ui/b/*.*
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
	$(MAKE) $>/.ui-reload-stamp NPMBLOCK=y -j`nproc` |& tee $>/ui-build.log || { ( : \
		&& echo '<html><head><title>anklang/ui: make error</title></head><body><pre>' \
		&& cat $>/ui-build.log && echo '</pre>' \
		&& echo '<script>setTimeout(_=>window.location.reload(), 3000)</script></body></html>' \
		) > $>/ui/index.html && touch --date=1990-01-01 $>/ui/index.html ; }
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
