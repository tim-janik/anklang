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
ui/copy.wildcards ::= $(wildcard	\
	ui/*.js				\
	ui/b/*.js			\
)
ui/nocopy.wildcards ::= $(wildcard	\
	ui/sfc-compile.js		\
	ui/slashcomment.js		\
)
ui/copy.files ::= $(filter-out $(ui/nocopy.wildcards), $(ui/copy.wildcards))
ui/vue.wildcards ::= $(wildcard		\
	ui/b/*.vue			\
)
ui/public.wildcards ::= $(wildcard	\
	ui/assets/*.svg			\
	ui/favicon.ico			\
)

# == ui/vue.js ==
$>/ui/vue.js:	$>/node_modules/.npm.done				| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/vue.js $>/ui/.eslintcache
	$Q $(CP) $>/node_modules/vue/dist/vue.esm-browser.js $>/ui/vue.js
	$Q sed -i $>/ui/vue.js \
		-e 's/^\s*\(console\.info(.You are running a development build of Vue\)/if(0) \1/' \
		-e 's/\b\(warn(`[^`]* was accessed during render\)/if(0) \1/'
$>/ui/.build1-stamp: $>/ui/vue.js

# == ui/index.html ==
$>/ui/index.html: ui/index.html ui/assets/eknob.svg ui/Makefile.mk	| $>/ui/
	$(QGEN)
	$Q rm -f $>/ui/doc && ln -s ../doc $>/ui/doc # MAKE is flaky in tracking symlink timestamps
	$Q : $(file > $>/ui/config.json,  { "config": { $(strip $(PACKAGE_VERSIONS)) } })
	$Q sed -r \
		-e "/<script type='application\/json' id='--EMBEDD-config_json'>/ r $>/ui/config.json" \
		-e "/<!--EMBEDD='eknob\.svg'-->/r ui/assets/eknob.svg" \
		< $<	> $@.tmp
	$Q rm $>/ui/config.json
	$Q sed -r \
		-e "/^<\?xml .*\?>\s*$$/d" \
		-i $@.tmp
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/index.html

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
	$Q $(lib/AnklangSynthEngine) --js-api		>> $@.tmp
	$Q echo 'export let server = s => { if (s instanceof Server) server = s; };'	>> $@.tmp
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/aseapi.js

# == ui/b/vue.targets ==
ui/b/vue.targets ::= $(ui/vue.wildcards:%.vue=$>/%.js)
$(ui/b/vue.targets): ui/sfc-compile.js 			| $>/ui/fonts/AnklangIcons.css
$(ui/b/vue.targets): $>/%.js: %.vue			| $>/ui/b/ $>/node_modules/.npm.done
	$(QGEN)
	$Q node ui/sfc-compile.js --debug -I $>/ui/ $< -O $(@D)
$(ui/b/vue.targets): $(wildcard ui/*.scss ui/b/*.scss)	# includes of the sfc generated CSS outputs
$>/ui/.build1-stamp: $(ui/b/vue.targets)

# == all-styles.css ==
$>/ui/all-styles.css: ui/Makefile.mk $(ui/b/vue.targets)
	$(QGEN)
	$Q echo 					>  $@.tmp
	$Q for f in $$(cd $>/ui/b/ && ls *.css) ; do				\
		echo "@import './b/$${f}';"		>> $@.tmp		\
		|| exit 1 ; done
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/all-styles.css

# == all-components.js ==
$>/ui/all-components.js: ui/Makefile.mk $(ui/b/vue.targets) $(wildcard ui/b/*)		| $>/ui/
	$(QGEN)
	$Q echo 					>  $@.tmp
	$Q EX='' && for f in $$(cd ui/b/ && ls *.vue) ; do			\
		b=$${f%.*} && id=$${b//-/_} &&					\
		EX="$$EX  'b-$$b': _$$id,"$$'\n'				\
		&& echo "import _$$id from './b/$${f%.vue}.js';" >> $@.tmp	\
		|| exit 1 ; done						\
	   && echo "export default {"			>> $@.tmp		\
	   && echo "$$EX};"				>> $@.tmp
	$Q mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/all-components.js

# == File Copies ==
ui/copy.targets ::= $(ui/copy.files:%=$>/%)
$(ui/copy.targets): $>/ui/%: ui/%	| $>/ui/b/
	$(QECHO) COPY $<
	$Q cp -a $< --parents $>/
$>/ui/.build1-stamp: $(ui/copy.targets)

# == Copies to ui/ ==
ui/public.targets ::= $(ui/public.wildcards:ui/%=$>/ui/%)
$(ui/public.targets): $>/ui/%: ui/%			| $>/ui/
	$(QECHO) COPY $<
	$Q cd ui/ && cp -a $(<:ui/%=%) --parents $(abspath $>/)/ui/
$>/ui/.build1-stamp: $(ui/public.targets)

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
	$Q cd $>/ui/anklangicons/ && cp AnklangCursors.scss AnklangIcons.woff2 ../fonts/
	$Q cd $>/ui/anklangicons/ && cp AnklangIcons.css ../fonts/AnklangIcons.css.tmp
	$Q rm -r $>/ui/anklangicons/ && mv $@.tmp $@
$>/ui/.build1-stamp: $>/ui/fonts/AnklangIcons.css

# == Material-Icons ==
$>/ui/fonts/material-icons.css: ui/Makefile.mk		| $>/ui/fonts/
	$(QECHO) FETCH Material-Icons
	@ $(eval T := material-icons-200821-1-h0fccaba10.tar.xz)
	$Q cd $(@D) $(call foreachpair, AND_DOWNLOAD_SHAURL, \
		11653afda6690b5230a64d67a0057634bc5a45b9f2c9fc95916de839ba72e12f \
		  https://github.com/tim-janik/assets/releases/download/material-icons/$T)
	$Q cd $(@D) \
		&& rm -fr material-icons/ && tar -xf $T \
		&& mv material-icons/material-icons.woff2 ./ \
		&& cp material-icons/material-icons.css material-icons.css.tmp \
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
	$Q echo "const module_list = {"								>  $>/ui/tmp-browserify/browserified.js
	$Q for mod in markdown-it chroma-js ; do \
		echo "  '$${mod}': require ('$$mod')," ; done					>> $>/ui/tmp-browserify/browserified.js
	$Q echo "};"										>> $>/ui/tmp-browserify/browserified.js
	$Q echo "Object.defineProperty (window, 'require', { value: m => module_list[m] });"	>> $>/ui/tmp-browserify/browserified.js
	$Q echo "if (__DEV__) window.require.module_list = module_list;"			>> $>/ui/tmp-browserify/browserified.js
	$Q $>/node_modules/.bin/browserify -o $>/ui/tmp-browserify/out.browserified.js $>/ui/tmp-browserify/browserified.js
	$Q mv $>/ui/tmp-browserify/out.browserified.js $@ && rm -r $>/ui/tmp-browserify/
$>/ui/.build1-stamp: $>/ui/browserified.js

# == $>/ui/favicon.ico ==
$>/ui/favicon.ico: ui/assets/favicon.svg $>/node_modules/.npm.done ui/Makefile.mk	| $>/ui/
	$(QGEN)
	$Q mkdir -p $>/ui/tmp-icongen/
	$Q $>/node_modules/.bin/icon-gen -i $< -o $>/ui/tmp-icongen/ # -r
	$Q cd $>/ui/tmp-icongen/ && mv favicon-128.png ../anklang.png && mv favicon.ico ../favicon.ico.tmp
	$Q rm -r $>/ui/tmp-icongen/ && mv $@.tmp $@
$>/ui/anklang.png: $>/ui/favicon.ico
$>/ui/.build1-stamp: $>/ui/favicon.ico $>/ui/anklang.png

# == $>/ui/eslint.files ==
ui/eslint.files ::= $(wildcard ui/*.html ui/*.js ui/b/*.js ui/b/*.vue)
$>/ui/.eslint.files: ui/eslintrc.js $(ui/eslint.files)			| $>/ui/
	$(QGEN)
	$Q cp $< $(@D)/.eslintrc.js
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

# == ui/*js.md ==
ui/jsdoc.deps     ::= ui/jsdocrc.json ui/slashcomment.js doc/jsdoc2md.js
$>/ui/envuejs.md: ui/b/envue.js $(ui/jsdoc.deps) ui/Makefile.mk $>/node_modules/.npm.done	| $>/ui/
	$(QGEN)
	$Q $>/node_modules/.bin/jsdoc -c ui/jsdocrc.json -X ui/b/envue.js	>  $(@:.md=.jsdoc)
	$Q echo -e '\n## Reference for envue.js'				>  $@.tmp
	$Q node doc/jsdoc2md.js -d 2 -e 'Envue' $(@:.md=.jsdoc)			>> $@.tmp
	$Q mv $@.tmp $@
$>/ui/utiljs.md: ui/util.js $(ui/jsdoc.deps) ui/Makefile.mk $>/node_modules/.npm.done	| $>/ui/
	$(QGEN)
	$Q $>/node_modules/.bin/jsdoc -c ui/jsdocrc.json -X ui/util.js		>  $(@:.md=.jsdoc)
	$Q echo -e '\n## Reference for util.js'					>  $@.tmp
	$Q node doc/jsdoc2md.js -d 2 -e 'Util' $(@:.md=.jsdoc)			>> $@.tmp
	$Q mv $@.tmp $@

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
	$>/ui/*.css		\
	$>/ui/*.html		\
	$>/ui/*.ico		\
	$>/ui/*.js		\
	$>/ui/*.png		\
)
ui/install: $>/ui/.build1-stamp $>/ui/.build2-stamp
	@$(QECHO) INSTALL '$(ui/installdir)/...'
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
	@$(QECHO) REMOVE '$(ui/installdir)/...'
	$Q rm -f -r '$(ui/installdir)'
.PHONY: ui/uninstall
uninstall: ui/uninstall
