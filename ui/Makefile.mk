# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ui/*.d)
ui/cleandirs ::= $(wildcard $>/ui/ $>/dist/)
CLEANDIRS         += $(ui/cleandirs)
ALL_TARGETS       += $>/ui/.build1-stamp $>/ui/.build2-stamp
$>/ui/.build1-stamp:	# essential build targets for the UI
$>/ui/.build2-stamp:	# extra targets, deferred during incremental rebuilds

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
$>/ui/lit.js:	$>/node_modules/.npm.done ui/Makefile.mk		| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/lit.js* $@
	$Q for mod in \
		lit lit/directives/async-append.js lit/directives/async-replace.js \
		lit/directives/cache.js lit/directives/choose.js lit/directives/class-map.js lit/directives/guard.js \
		lit/directives/if-defined.js lit/directives/join.js lit/directives/keyed.js lit/directives/live.js \
		lit/directives/map.js lit/directives/range.js lit/directives/ref.js lit/directives/repeat.js \
		lit/directives/style-map.js lit/directives/template-content.js lit/directives/unsafe-html.js \
		lit/directives/unsafe-svg.js lit/directives/until.js lit/directives/when.js \
		  ; do \
		echo "export * from '$$mod';" ; \
	   done							> $>/ui/lit-all.js
	$Q cd $>/ui/ && ../node_modules/.bin/rollup \
		-p @rollup/plugin-node-resolve \
		lit-all.js -o lit.js --sourcemapFile lit.js.map -m
	$Q $(RM) $>/ui/lit-all.js
$>/ui/.build1-stamp: $>/ui/lit.js

# == ui/vue.js ==
$>/ui/vue.js:	$>/node_modules/.npm.done				| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/vue.js $>/ui/.eslintcache
	$Q $(CP) $>/node_modules/vue/dist/vue.esm-browser.js $>/ui/vue.js
	$Q sed -i $>/ui/vue.js \
		-e 's/^\s*\(console\.info(.You are running a development build of Vue\)/if(0) \1/' \
		-e 's/\b\(warn(`[^`]* was accessed during render\)/if(0) \1/'
$>/ui/.build1-stamp: $>/ui/vue.js

# == ui/zcam-js.mjs ==
$>/ui/zcam-js.mjs: $>/node_modules/.npm.done				| $>/ui/
	$(QGEN)
	$Q $(CP) $>/node_modules/zcam-js/zcam-js.mjs $@
$>/ui/.build1-stamp: $>/ui/zcam-js.mjs
$>/ui/colors.js: $>/ui/zcam-js.mjs

# == ui/csstree-validator.esm.js ==
$>/ui/csstree-validator.esm.js: $>/node_modules/.npm.done				| $>/ui/
	$(QGEN)
	$Q $(CP) $>/node_modules/csstree-validator/dist/csstree-validator.esm.js $@
$>/ui/.build1-stamp: $>/ui/csstree-validator.esm.js

# == ui/index.html ==
$>/ui/index.html: ui/index.html ui/Makefile.mk			| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/doc && ln -s ../doc $>/ui/doc # MAKE is flaky in tracking symlink timestamps
	$Q : $(file > $>/ui/config.json,  { "config": { $(strip $(PACKAGE_VERSIONS)) } })
	$Q sed -r \
		-e "/<script type='application\/json' id='--EMBEDD-config_json'>/ r $>/ui/config.json" \
		< $<	> $@.tmp
	$Q rm $>/ui/config.json
	$Q sed -r \
		-e "/^<\?xml .*\?>\s*$$/d" \
		-i $@.tmp
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/index.html
# remove ::KEEPIF="__DEV__" directives
ui/sed-keepif ::= $(if __DEV__, -e '/<[^<>]*::KEEPIF="__DEV__"/s/::KEEPIF="__DEV__"//')
# delete unmatched ::KEEPIF="" tags
ui/sed-keepif  += -e 's/<[^<>]*::KEEPIF="[^"]*"[^<>]*>//'

# == knob sprites ==
$>/ui/assets/%: $>/images/knobs/%			| $>/ui/assets/
	$(QGEN)
	$Q $(CP) $< $@
$>/ui/.build1-stamp: $>/ui/assets/cknob193u.png $>/ui/assets/cknob193b.png

# == ui/spinner.svg ==
$>/ui/spinner.scss: ui/assets/spinner.svg
	$(QGEN)
	$Q sed -rn '/@keyframe/,$${ p; /^\s*}\s*$$/q; }' $< > $@
$>/ui/all-styles.css: $>/ui/spinner.scss

# == ui/.aseignore ==
$>/ui/.aseignore:					| $>/ui/
	$(QGEN)
	$Q rm -f $@.tmp
	$Q echo '.*/[.].*'			>> $@.tmp
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/.aseignore

# == ui/aseapi.js ==
$>/ui/aseapi.js: jsonipc/jsonipc.js ase/api.hh $(lib/AnklangSynthEngine) ui/Makefile.mk	| $>/ui/
	$(QGEN)
	$Q $(CP) $< $@.tmp
	$Q ASAN_OPTIONS=detect_leaks=0 $(lib/AnklangSynthEngine) --js-api			>> $@.tmp
	$Q echo 'export let server = Jsonipc.setup_promise_type (Server, s => server = s);'	>> $@.tmp
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/aseapi.js

# == ui/b/vuejs.targets ==
ui/b/vuejs.targets ::= $(ui/vue.wildcards:%.vue=$>/%.js)
ui/b/vuecss.targets ::= $(ui/vue.wildcards:%.vue=$>/%.css)
$(ui/b/vuecss.targets): $(ui/b/vuejs.targets) ;
$(ui/b/vuejs.targets): ui/sfc-compile.js 			| $>/ui/fonts/AnklangIcons.css
$(ui/b/vuejs.targets): $>/%.js: %.vue			| $>/ui/b/ $>/node_modules/.npm.done
	$(QGEN)
	$Q node ui/sfc-compile.js --debug -I $>/ui/ $< -O $(@D)
$(ui/b/vuejs.targets): $(wildcard ui/*.scss ui/b/*.scss)	# includes of the sfc-compile.js generated CSS outputs
$>/ui/.build1-stamp: $(ui/b/vuejs.targets)

# == ui/all-styles.css ==
ui/csscopy.sources ::= ui/styles.scss ui/theme.scss ui/mixins.scss ui/shadow.scss
$>/ui/all-styles.css: $>/ui/postcss.js ui/Makefile.mk $(ui/csscopy.sources) $(ui/b/vuecss.targets)	| $>/ui/
	$(QGEN)
	$Q $(CP) $(ui/csscopy.sources) $>/ui/
	$Q echo '@charset "UTF-8";'					>  $>/ui/imports.scss
	$Q echo "@import 'styles.scss';"				>> $>/ui/imports.scss
	$Q for f in $$(cd $>/ui/b/ && echo *.css) ; do			\
		echo "@import 'b/$${f}';"				>> $>/ui/imports.scss \
		|| exit 1 ; done
	$Q cd $>/ui/ && node ./postcss.js imports.scss $(@F).tmp
	$Q mv $@.tmp $@
$>/ui/postcss.js: ui/postcss.js ui/Makefile.mk $>/ui/colors.js $>/node_modules/.npm.done
	$(QGEN)
	$Q $(CP) $< $@.tst.js
	$Q cd $>/ui/ && node ./$(@F).tst.js --test $V # CHECK transformations
	$Q mv $@.tst.js $@
$>/ui/.build1-stamp: $>/ui/all-styles.css $>/ui/postcss.js

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
$>/ui/.build1-stamp: $>/ui/all-components.js

# == File Copies ==
ui/copy.targets ::= $(ui/copy.files:%=$>/%)
$(ui/copy.targets): $>/ui/%: ui/%	| $>/ui/b/
	$(QECHO) COPY $@
	$Q $(CP) $< --parents $>/
$>/ui/.build1-stamp: $(ui/copy.targets)

# == Copies to ui/ ==
ui/public.targets ::= $(ui/public.wildcards:ui/%=$>/ui/%)
$(ui/public.targets): $>/ui/%: ui/%			| $>/ui/assets/
	$(QECHO) COPY $<
	$Q cd ui/ && $(CP) $(<:ui/%=%) --parents $(abspath $>/)/ui/
$>/ui/.build1-stamp: $(ui/public.targets)

# == ui/cursors/ ==
$>/ui/cursors/cursors.css: $(wildcard ui/cursors/*) Makefile.mk		| $>/ui/cursors/
	$(QECHO) COPY $<
	$Q for SVG in `sed -n '/url.cursors\//{ s/.*(//; s/).*//; p }' ui/cursors/cursors.css` ; do \
		$(CP) ui/"$$SVG" $>/ui/cursors/ || break ; done
	$Q $(CP) ui/cursors/cursors.css $@
$>/ui/.build1-stamp: $>/ui/cursors/cursors.css

# == CJS Files ==
ui/cjs.targets ::= $(ui/cjs.wildcards:%.js=$>/%.cjs)
$(ui/cjs.targets): $>/ui/%.cjs: ui/%.js	| $>/ui/b/
	$(QECHO) COPY $@
	$Q $(CP) $< $@
$>/ui/.build1-stamp: $(ui/cjs.targets)

# == Inter Typeface ==
$>/ui/fonts/Inter-Medium.woff2: ui/Makefile.mk		| $>/ui/fonts/
	$(QECHO) FETCH Inter Typeface
	$Q cd $(@D) $(call foreachpair, AND_DOWNLOAD_SHAURL, \
		9cd56084faa8cc5ee75bf6f3d01446892df88928731ee9321e544a682aec55ef https://github.com/rsms/inter/raw/v3.10/docs/font-files/Inter-Medium.woff2 )
	$Q touch $@
$>/ui/.build1-stamp: $>/ui/fonts/Inter-Medium.woff2

# == AnklangIcons ==
$>/ui/fonts/AnklangIcons.css: ui/Makefile.mk		| $>/ui/fonts/
	$(QECHO) FETCH AnklangIcons
	@ $(eval S := ae0daeee324a1be1051f722e5393cdef445b5209119b97330ab92f9052b7206a https://github.com/tim-janik/anklang/releases/download/buildassets-v0/anklangicons-201123.1.tgz)
	@ $(eval H := $(firstword $(S))) $(eval U := $(lastword $(S))) $(eval T := $(notdir $(U)))
	$Q if test -e images/$T ; then \
		$(CP) images/$T $>/ui/ && exit $$? ; \
	   else \
		cd $>/ui/ $(call AND_DOWNLOAD_SHAURL, $H, $U) ; \
	   fi
	$Q rm -fr $>/ui/anklangicons/ && tar -xf $>/ui/$T -C $>/ui/ && rm $>/ui/$T
	$Q cd $>/ui/anklangicons/ && $(CP) AnklangIcons.woff2 ../fonts/
	$Q cd $>/ui/anklangicons/ && $(CP) AnklangIcons.css ../fonts/AnklangIcons.css.tmp
	$Q rm -r $>/ui/anklangicons/ && mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/fonts/AnklangIcons.css

# == Material-Icons ==
$>/ui/fonts/material-icons.css: ui/Makefile.mk		| $>/ui/fonts/
	$(QECHO) FETCH Material-Icons
	@ $(eval T := material-icons-220403-1-h964709088.tar.xz)
	$Q cd $(@D) $(call foreachpair, AND_DOWNLOAD_SHAURL, \
		5b51584613a9f84ea935b785123d4fe088fa8cb7660f886e66420c81fee89659 \
		  https://github.com/tim-janik/assets/releases/download/material-icons/$T)
	$Q cd $(@D) \
		&& rm -fr material-icons/ && tar -xf $T \
		&& mv material-icons/material-icons.woff2 ./ \
		&& $(CP) material-icons/material-icons.css material-icons.css.tmp \
		&& rm -fr material-icons/ $T
	$Q mv $@.tmp $@ && touch $@
$>/ui/.build1-stamp: $>/ui/fonts/material-icons.css

# == Fork-Awesome ==
$>/ui/fonts/forkawesome-webfont.css: ui/Makefile.mk	| $>/ui/fonts/
	$(QECHO) FETCH Fork-Awesome
	$Q cd $(@D) $(call foreachpair, AND_DOWNLOAD_SHAURL, $(ui/fork-awesome-downloads))
	$Q sed "/^ *src: *url/s,src: *url(.*);,src: url('forkawesome-webfont.woff2');," -i $>/ui/fonts/fork-awesome.css
	$Q mv $>/ui/fonts/fork-awesome.css $@
ui/fork-awesome-downloads ::= \
  844517a2bc5430242cb857e56b6dccf002f469c4c1b295ed8d0b7211fb452f50 \
    https://raw.githubusercontent.com/ForkAwesome/Fork-Awesome/b0605a81632452818bf19c8fa97469da1206b52b/fonts/forkawesome-webfont.woff2 \
  630b0e84fa43579f7e97a26fd47d4b70cb5516ca7e6e73393597d12ca249a8ee \
    https://raw.githubusercontent.com/ForkAwesome/Fork-Awesome/b0605a81632452818bf19c8fa97469da1206b52b/css/fork-awesome.css
$>/ui/.build1-stamp: $>/ui/fonts/forkawesome-webfont.css

# == $>/ui/browserified.js ==
$>/ui/browserified.js: $>/node_modules/.npm.done	| ui/Makefile.mk $>/ui/
	$(QGEN)
	$Q: # re-export and bundle postcss modules
	$Q mkdir -p $>/ui/tmp-browserify/
	$Q echo "const modules = {"								>  $>/ui/tmp-browserify/requires.js
	$Q for mod in \
		markdown-it \
		postcss \
		postcss-discard-comments \
		postcss-discard-duplicates \
		css-color-converter \
		postcss-scss \
		postcss-advanced-variables \
		postcss-functions \
		postcss-nested \
		postcss-color-mod-function postcss-color-hwb postcss-lab-function \
		; do \
		echo "  '$${mod}': require ('$$mod')," ; done					>> $>/ui/tmp-browserify/requires.js
	$Q echo "};"										>> $>/ui/tmp-browserify/requires.js
	$Q echo "const browserify_require = m => modules[m] || console.error ('Unknown module:', m);"	>> $>/ui/tmp-browserify/requires.js
	$Q echo "Object.defineProperty (window, 'require', { value: browserify_require });"		>> $>/ui/tmp-browserify/requires.js
	$Q echo "window.require.modules = modules;"						>> $>/ui/tmp-browserify/requires.js
	$Q $>/node_modules/.bin/browserify --debug -o $>/ui/tmp-browserify/browserified.long.js $>/ui/tmp-browserify/requires.js
	$Q $>/node_modules/.bin/terser --source-map content=inline --comments false $>/ui/tmp-browserify/browserified.long.js -o $>/ui/tmp-browserify/browserified.min.js
	$Q mv $>/ui/tmp-browserify/browserified.min.js.map $@.map
	$Q mv $>/ui/tmp-browserify/browserified.min.js $@
	$Q rm -r $>/ui/tmp-browserify/
$>/ui/.build1-stamp: $>/ui/browserified.js

# == $>/ui/favicon.ico ==
$>/ui/favicon.ico: ui/assets/favicon.svg $>/node_modules/.npm.done ui/Makefile.mk	| $>/ui/
	$(QGEN)
	$Q mkdir -p $>/ui/tmp-icongen/
	$Q $>/node_modules/.bin/icon-gen -i $< -o $>/ui/tmp-icongen/ --favicon --favicon-png-sizes 128 --favicon-ico-sizes 128 # -r
	$Q cd $>/ui/tmp-icongen/ && mv favicon-128.png ../anklang.png && mv favicon.ico ../favicon.ico.tmp
	$Q rm -r $>/ui/tmp-icongen/ && mv $@.tmp $@
$>/ui/anklang.png: $>/ui/favicon.ico
$>/ui/.build1-stamp: $>/ui/favicon.ico $>/ui/anklang.png

# == $>/ui/eslint.files ==
ui/eslint.files ::= $(wildcard ui/*.html ui/*.js ui/b/*.js ui/b/*.vue)
$>/ui/.eslint.files: ui/eslintrc.js $(ui/eslint.files)			| $>/ui/
	$(QGEN)
	$Q $(CP) $< $(@D)/.eslintrc.cjs
	$Q echo '$(abspath $(ui/eslint.files))' | tr ' ' '\n' > $@
$>/ui/.build1-stamp: $>/ui/.eslint.files

# == eslint.done ==
$>/ui/.eslint.done: $>/ui/.eslint.files $>/node_modules/.npm.done
	$(QGEN)
	$Q cd $>/ui/ && npm run eslint |& ../../misc/colorize.sh
	$Q touch $@
$>/ui/.build2-stamp: $>/ui/.eslint.done	# deferred during rebuilds
eslint: $>/ui/.eslint.files $>/node_modules/.npm.done
	$Q cd $>/ui/ && npm run $@
.PHONY: eslint

# == cssvalid.done ==
$>/ui/.cssvalid.done: $>/ui/all-styles.css ui/Makefile.mk
	$(QGEN)
	$Q $>/node_modules/.bin/csstree-validator -r gnu $< > $>/ui/.cssvalid.err 2>&1 ; :
	$Q test ! -s $>/ui/.cssvalid.err || sed -r $$'s/^"([^"]+)":/\e[31;1m\\1\e[0m:/' $>/ui/.cssvalid.err
	$Q # test -s $>/ui/.cssvalid.err && exit 1
	$Q touch $@
$>/ui/.build2-stamp: $>/ui/.cssvalid.done	# deferred during rebuilds

# == ui/scripting-docs.md ==
$>/ui/scripting-docs.md: ui/host.js ui/ch-scripting.md $(ui/jsdoc.deps) ui/Makefile.mk $>/node_modules/.npm.done	| $>/ui/
	$(QGEN)
	$Q cat ui/ch-scripting.md 						>  $@.tmp
	$Q $>/node_modules/.bin/jsdoc -c ui/jsdocrc.json -X $<			>  $>/ui/host.jsdoc
	$Q echo -e '\n## Reference for $<'					>> $@.tmp
	$Q node doc/jsdoc2md.js -d 2 -e 'Host' $>/ui/host.jsdoc			>> $@.tmp
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/scripting-docs.md

# == ui/*js.md ==
ui/jsdoc.deps     ::= ui/jsdocrc.json ui/slashcomment.js doc/jsdoc2md.js
$>/ui/envuejs.md: ui/b/envue.js $(ui/jsdoc.deps) ui/Makefile.mk $>/node_modules/.npm.done	| $>/ui/
	$(QGEN)
	$Q $>/node_modules/.bin/jsdoc -c ui/jsdocrc.json -X $<			>  $(@:.md=.jsdoc)
	$Q echo -e '\n## Reference for $<'					>  $@.tmp
	$Q node doc/jsdoc2md.js -d 2 -e 'Envue' $(@:.md=.jsdoc)			>> $@.tmp
	$Q mv $@.tmp $@
$>/ui/utiljs.md: ui/util.js ui/script.js $(ui/jsdoc.deps) ui/Makefile.mk $>/node_modules/.npm.done	| $>/ui/
	$(QGEN)
	$Q echo -e '\n## Reference for $<'					>  $@.tmp
	$Q $>/node_modules/.bin/jsdoc -c ui/jsdocrc.json -X $<			>  $>/ui/util.jsdoc
	$Q node doc/jsdoc2md.js -d 2 -e 'Util' $>/ui/util.jsdoc			>> $@.tmp
	$Q echo -e '\n## Reference for ui/script.js'				>> $@.tmp
	$Q $>/node_modules/.bin/jsdoc -c ui/jsdocrc.json -X ui/script.js	>  $>/ui/script.jsdoc
	$Q node doc/jsdoc2md.js -d 2 -e 'Script' $>/ui/script.jsdoc		>> $@.tmp
	$Q mv $@.tmp $@

# == $>/doc/b/*.md ==
$>/doc/b/.doc-stamp: $(wildcard ui/b/*.js) ui/xbcomments.js ui/Makefile.mk $>/node_modules/.npm.done	| $>/doc/b/
	$(QGEN)
	$Q node ui/xbcomments.js $(wildcard ui/b/*.js) -O $>/doc/b/
	$Q touch $@
$>/ui/.build2-stamp: $>/doc/b/.doc-stamp # deferred during rebuilds

# == ui/vue-doc ==
$>/ui/vue-docs.md: ui/b/ch-vue.md $>/ui/envuejs.md $>/ui/utiljs.md $(ui/vue.wildcards) ui/Makefile.mk doc/filt-docs2.py		| $>/ui/
	$(QGEN)
	$Q echo -e "<!-- Vue Components -->\n\n"					>  $@.tmp
	@: # extract <docs/> blocks from vue files and filter headings through pandoc
	$Q for f in $(sort $(ui/vue.wildcards)) ; do \
		echo ""									>> $@.tmp ; \
		sed -n '/^<docs>\s*$$/{ :loop n; /^<\/docs>/q; p;  b loop }' <"$$f"	>> $@.tmp \
		|| exit $$? ; \
	done
	$Q sed -r 's/^  // ; s/^#/\n#/; ' -i $@.tmp # unindent, spread heading_without_preceding_blankline
	$Q $(PANDOC) -t markdown -F doc/filt-docs2.py -f markdown+compact_definition_lists $@.tmp -o $@.tmp2
	$Q cat $< $@.tmp2 $>/ui/envuejs.md $>/ui/utiljs.md > $@.tmp && rm -f $@.tmp2
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/vue-docs.md
$>/ui/.build2-stamp: $>/doc/anklang-manual.html # deferred during rebuilds

# == serve ==
serve: all $>/ui/.build1-stamp
	$Q cd $>/ui/ && npm run serve
.PHONY: serve

# == ui/rebuild ==
ui/rebuild:
	@: # incremental rebuild of source files without npm.done
	$(MAKE) $>/ui/.build1-stamp NPMBLOCK=y -j
	@: # close open sockets, only works if *same* executable still runs
	killall -s USR2 -u $(USER) -- $(abspath $(lib/AnklangSynthEngine))
	@: # perform non-essential rebuilds that may fail
	$(MAKE) $>/ui/.build2-stamp NPMBLOCK=y -j --no-print-directory &
.PHONY: ui/rebuild

# == installation ==
ui/installdir ::= $(DESTDIR)$(pkgdir)/ui
ui/install.pattern ::= $(strip	\
	$>/ui/.aseignore	\
	$>/ui/*.scss		\
	$>/ui/*.css		\
	$>/ui/*.html		\
	$>/ui/*.ico		\
	$>/ui/*.js		\
	$>/ui/*.mjs		\
	$>/ui/*.png		\
)
ui/install: $>/ui/.build1-stamp $>/ui/.build2-stamp
	@$(QECHO) INSTALL '$(ui/installdir)/.'
	$Q rm -f -r '$(ui/installdir)'
	$Q $(INSTALL)      -d $(ui/installdir)/ $(ui/installdir)/assets/ $(ui/installdir)/b/ $(ui/installdir)/fonts/
	$Q $(INSTALL_DATA) -p $(ui/install.pattern) $(ui/installdir)/
	$Q $(INSTALL_DATA) -p $>/ui/assets/* $(ui/installdir)/assets/
	$Q $(INSTALL_DATA) -p $>/ui/b/* $(ui/installdir)/b/
	$Q $(INSTALL_DATA) -p $>/ui/fonts/* $(ui/installdir)/fonts/
	$Q ln -s ../doc $(ui/installdir)/doc
.PHONY: ui/install
install: ui/install
ui/uninstall: FORCE
	@$(QECHO) REMOVE '$(ui/installdir)/.'
	$Q rm -f -r '$(ui/installdir)'
.PHONY: ui/uninstall
uninstall: ui/uninstall
