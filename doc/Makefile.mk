# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/doc/*.d)
ALL_TARGETS       += doc/all
doc/all:

# == Chapters for mkdocs ==
doc/mkdocs-chapters := $(strip		\
	$(wildcard doc/*.md)		\
	ase/gen/class-tree.g.md		\
	ui/ch-component.md		\
	$>/gen/b/cliplist.md		\
)

# == doc/ files ==
doc/install.files ::= $(strip		\
	$>/doc/anklang.1		\
	$>/doc/NEWS.md			\
	$>/doc/NEWS.html		\
	$>/doc/README.md		\
	$>/doc/README.html		\
	doc/copyright			\
)

# == Copy doc/install.files ==
$(filter %.md, $(doc/install.files)): $>/doc/%.md: %.md doc/Makefile.mk			| $>/doc/
	$(QECHO) COPY $<
	$Q $(CP) $< $@

# == doc/jsdocs.md ==
doc/jsdocs_js := $(wildcard ui/*.js ui/b/*.js)
doc/jsdocs_md := $(doc/jsdocs_js:ui/%.js=$>/doc/jsdocsmd/%.md)
$(doc/jsdocs_md): doc/jsdoc2md.js
$>/doc/jsdocsmd/%.md: ui/%.js		| node_modules/.npm.done $>/doc/jsdocsmd/b/
	$(QGEN)
	$Q node doc/jsdoc2md.js -d 2 $< > $@.tmp
	$Q grep -q '[^[:space:]]' $@.tmp && mv $@.tmp $@ || { rm -f $@.tmp && touch $@ ; }
$>/doc/jsdocs.md: $(doc/jsdocs_md) doc/Makefile.mk
	$(QGEN)
	$Q echo -e '\n# UI Component Reference\n'		>  $@.tmp
	$Q for f in $(sort $(doc/jsdocs_md)) ; do \
		(echo && cat $$f && echo ) >> $@.tmp \
	|| exit 1 ; done
	$Q # Use pandoc to convert markdown *without* raw_html to regular markdown with escaped angle brackets
	$Q pandoc -p -f markdown+compact_definition_lists+autolink_bare_uris+emoji+lists_without_preceding_blankline-smart-raw_html-raw_tex \
		-t markdown+autolink_bare_uris+emoji+lists_without_preceding_blankline-smart		$@.tmp > $@.tmp2
	$Q mv $@.tmp2 $@
doc/mkdocs-chapters += $>/doc/jsdocs.md

# == gen/piano-roll.md ==
$>/gen/the-piano-roll.md: $>/gen/b/piano-ctrl.md $>/gen/b/pianoroll.md
	$(QGEN)
	$Q echo				>  $@.tmp
	$Q cat $>/gen/b/pianoroll.md	>> $@.tmp
	$Q echo				>> $@.tmp
	$Q cat $>/gen/b/piano-ctrl.md	>> $@.tmp
	$Q mv $@.tmp $@
doc/mkdocs-chapters += $>/gen/the-piano-roll.md

# == gen/scripting-docs.md ==
doc/mkdocs-chapters := $(filter-out doc/ch-scripting.md, $(doc/mkdocs-chapters)) $>/gen/scripting-docs.md
$>/gen/scripting-docs.md: ui/host.js doc/ch-scripting.md $(doc/jsdoc.deps) doc/Makefile.mk node_modules/.npm.done	| $>/doc/
	$(QGEN)
	$Q cat doc/ch-scripting.md				>  $@.tmp
	$Q echo -e '\n## Reference for $<'			>> $@.tmp
	$Q node doc/jsdoc2md.js -d 2 -e 'Host' $<		>> $@.tmp
	$Q mv $@.tmp $@
doc/jsdoc.deps ::= doc/jsdocrc.json doc/jsdoc-slashes.js doc/jsdoc2md.js

# == pandoc ==
doc/markdown-flavour	::= -f markdown+autolink_bare_uris+emoji+lists_without_preceding_blankline-smart
doc/html_flags		::= --highlight-style doc/highlights.theme --html-q-tags --section-divs --email-obfuscation=references
doc/html-style		::= 'body { max-width: 52em; margin: auto; }'

# == man build rules ==
$>/doc/%.1: doc/%.1.md doc/Makefile.mk					| $>/doc/
	$(QECHO) MD2MAN $@
	$Q $(PANDOC) $(doc/markdown-flavour) -s -p \
		-M date="$(version_date)" \
		-M footer="anklang-$(version_short)" \
		-t man $< -o $@.tmp
	$Q mv $@.tmp $@

# == html from markdown ==
$>/doc/%.html: %.md doc/Makefile.mk					| $>/doc/
	$(QECHO) MD2HTML $@
	$Q $(PANDOC) $(doc/markdown-flavour) -s -p $(doc/html_flags) -t html5 \
		--metadata pagetitle="$(notdir $(@:%.md=%))" \
		$< -o $@.tmp
	$Q sed -re '0,/<\/style>/s|(\s*</style>)|'$(doc/html-style)'\n\1|' -i $@.tmp
	$Q mv $@.tmp $@

# == template.html ==
$>/doc/template.html: doc/template.diff doc/style/onload.html doc/Makefile.mk		| $>/doc/
	$(QGEN)
	$Q $(PANDOC) -D html > $>/doc/template.html \
	  && sed $$'/^<\/body>/{r doc/style/onload.html\nN}' -i $>/doc/template.html \
	  && cd $>/doc/ && patch < $(abspath doc/template.diff)

# == doc/cursors/ ==
$>/doc/cursors/cursors.css: $>/gen/cursors/cursors.css			| $>/doc/
	$(QGEN)
	$Q $(RM) -r -f $>/doc/cursors
	$Q $(CP) -r -p $>/gen/cursors/ $>/doc/cursors/

# == installation ==
pkgdocdir ::= $(pkgdir)/doc
doc/install: $(doc/install.files) install--doc/style/install.files
	@$(QECHO) INSTALL '$(DESTDIR)$(pkgdocdir)/.'
	$Q rm -f '$(DESTDIR)$(pkgdocdir)'/* 2>/dev/null ; true
	$Q $(INSTALL)      -d $(DESTDIR)$(pkgdocdir)/ $(DESTDIR)$(mandir)/man1/
	$Q $(CP) $(doc/install.files) $(DESTDIR)$(pkgdocdir)/
	$Q $(INSTALL) -d $(DESTDIR)$(mandir)/man1/ && ln -fs -r $(DESTDIR)$(pkgdir)/doc/anklang.1 $(DESTDIR)$(mandir)/man1/anklang.1
	$Q $(INSTALL) -d '$(DESTDIR)$(docdir)/'
	$Q rm -f '$(DESTDIR)$(docdir)/anklang'
	$Q ln -s -r '$(DESTDIR)$(pkgdir)/doc' '$(DESTDIR)$(docdir)/anklang'
.PHONY: doc/install
install: doc/install
doc/uninstall: FORCE uninstall--doc/style/install.files
	@$(QECHO) REMOVE '$(DESTDIR)$(pkgdocdir)/.'
	$Q rm -f -r '$(DESTDIR)$(pkgdocdir)'
	$Q rm -f '$(DESTDIR)$(mandir)/man1/anklang.1'
	$Q rm -f '$(DESTDIR)$(docdir)/anklang'
.PHONY: doc/uninstall
uninstall: doc/uninstall

doc/all: $(doc/install.files)

# == doxygen/ ==
DOC_DOXYGEN_DEPS := doc/doxygen.sh doc/style/doxyextra.css doc/doxyheader.htm doc/doxyfooter.htm doc/Makefile.mk
$>/doxygen/.ase: $(DOC_DOXYGEN_DEPS) $(wildcard ase/*[hcd] ase/*/*[hcd])
	$(QGEN)
	$Q mkdir -p $>/doxygen/ && rm -rf $>/doxygen/*/ # clear subdirs
	$Q doc/doxygen.sh --ase $(if $(findstring 1, $(V)),, --quiet)
	$Q @touch $@
$>/doxygen/.done: $>/doxygen/.ase
	$Q @touch $@
doxygen: $>/doxygen/.done
	ls -l $>/doxygen/
clean-doxygen:
	rm -fr $>/doxygen/
.PHONY: doxygen clean-doxygen
MAKE_HELP += ' doxygen         - Build (large) C++ documentation in $>/doxygen/*/\n'
MAKE_HELP += ' clean-doxygen   - Remove $>/doxygen/\n'

# == mkdocs/ ==
doc/mkdocs.symlinks := $(doc/mkdocs-chapters)
doc/mkdocs.symlinks += doc/javascript doc/style $>/doc/jsdocsmd/
$>/mkdocs/.prepared: $(doc/mkdocs.symlinks) doc/Makefile.mk	| $>/mkdocs/
	$(QGEN)
	$Q rm -rf $>/mkdocs/* && mkdir -p $>/mkdocs/doc
	$Q ln -s $(abspath doc/mkdocs.yml) $>/mkdocs/
	$Q ln -s $(abspath $(doc/mkdocs.symlinks)) $>/mkdocs/doc/
	$Q ln -s $(abspath ui/cursors) $>/mkdocs/doc/
	$Q cd $>/mkdocs/ && uv venv --python 3.12 \
	&& UV_LINK_MODE=copy uv pip install \
		mkdocs mkdocs-material mkdocs-file-filter-plugin mkdocs-literate-nav \
		git+https://github.com/tim-janik/mkdocs-live-edit-plugin
	$Q @touch $@
doc/mkdocs/anklang.stamp := $>/mkdocs/anklang/search/search_index.js
$(doc/mkdocs/anklang.stamp): $>/mkdocs/.prepared
	$(QECHO) BUILD $>/mkdocs/anklang/
	$Q test -r $>/doxygen/.done && ln -sf ../../doxygen $>/mkdocs/doc/ || rm -f $>/mkdocs/doc/doxygen
	$Q cd $>/mkdocs/ && uv run mkdocs build
$(doc/mkdocs/anklang.stamp): $(filter doxygen $>/doxygen/.done, $(MAKECMDGOALS))	# let mkdocs pickup doxygen/ if both are built
$(doc/mkdocs/anklang.stamp): $(wildcard $>/doxygen/.done)				# rebuild doxygen first, if it exists
ALL_TARGETS += $(doc/mkdocs/anklang.stamp)
mkdocs-serve: $>/mkdocs/.prepared
	$(QECHO) SERVE mkdocs at localhost:1778
	$Q test -r $>/doxygen/.done && ln -sf ../../doxygen $>/mkdocs/doc/ || rm -f $>/mkdocs/doc/doxygen
	$Q cd $>/mkdocs/ && uv run mkdocs serve --livereload # -a localhost:1778
clean-mkdocs:
	rm -rf $>/mkdocs/
mkdocs-site: $(doc/mkdocs/anklang.stamp)
.PHONY: mkdocs-serve mkdocs-site clean-mkdocs
MAKE_HELP += ' mkdocs-serve    - Serve documentation at localhost:1778 with hot reload\n'
MAKE_HELP += ' mkdocs-site     - Build documentation in $>/mkdocs/anklang/\n'
MAKE_HELP += '                   Reuses $>/doxygen/ iff it exists\n'
MAKE_HELP += ' clean-mkdocs    - Remove $>/mkdocs/anklang/\n'
