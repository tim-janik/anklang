# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/doc/*.d)
doc/cleandirs ::= $(wildcard $>/doc/)
CLEANDIRS         += $(doc/cleandirs)
ALL_TARGETS       += doc/all
doc/all:

# == doc/ files ==
doc/manual-chapters ::= $(strip		\
	doc/ch-intro.md			\
	doc/ch-install.md		\
	doc/ch-editing.md		\
	$>/doc/b/cliplist.md		\
	$>/doc/b/pianoroll.md		\
	$>/doc/b/piano-ctrl.md		\
	$>/doc/ch-man-pages.md		\
	$>/doc/scripting-docs.md	\
)
doc/internals-chapters ::= $(strip	\
	doc/ch-development.md		\
	$>/doc/class-tree.md		\
	ui/ch-component.md		\
	$>/doc/jsdocs.md		\
	doc/ch-releasing.md		\
	doc/ch-appendix.md		\
)
doc/install.files ::= $(strip		\
	$>/doc/anklang-manual.html	\
	$>/doc/anklang-internals.html	\
	$>/doc/anklang.1		\
	$>/doc/NEWS.md			\
	$>/doc/NEWS.html		\
	$>/doc/README.md		\
	$>/doc/README.html		\
	$>/doc/copyright		\
)
doc/pdf.files := $>/doc/anklang-manual.pdf $>/doc/anklang-internals.pdf

# == PDF rule (Latex dependency) ==
pdf: $(doc/pdf.files)
assets/pdf: pdf
	mkdir -p assets/
	$(CP) $>/doc/anklang-manual.pdf assets/anklang-manual-$(version_short).pdf
	$(CP) $>/doc/anklang-internals.pdf assets/anklang-internals-$(version_short).pdf
.PHONY: pdf assets/pdf

# == Copy files ==
$(filter %.md, $(doc/install.files)): $>/doc/%.md: %.md doc/Makefile.mk			| $>/doc/
	$(QECHO) COPY $<
	$Q $(CP) $< $@

# == doc/copyright ==
$>/doc/copyright: misc/mkcopyright.py doc/copyright.ini $>/ls-tree.lst	| $>/doc/
	$(QGEN)
	$Q if test -r .git ; then				\
	     misc/mkcopyright.py -c doc/copyright.ini		\
		$$(cat $>/ls-tree.lst) > $@.tmp ;		\
	   else							\
	     $(CP) doc/copyright $@.tmp ;			\
	   fi
	$Q mv $@.tmp $@

# == doc/jsdocs.md ==
doc/jsdocs_js := $(wildcard ui/*.js ui/b/*.js)
doc/jsdocs_md := $(doc/jsdocs_js:ui/%.js=$>/doc/jsdocsmd/%.md)
$(doc/jsdocs_md): doc/jsdoc2md.js
$>/doc/jsdocsmd/%.md: ui/%.js		| $>/node_modules/.npm.done $>/doc/jsdocsmd/b/
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

# == doc/scripting-docs.md ==
$>/doc/scripting-docs.md: ui/host.js doc/ch-scripting.md $(doc/jsdoc.deps) doc/Makefile.mk $>/node_modules/.npm.done	| $>/doc/
	$(QGEN)
	$Q cat doc/ch-scripting.md				>  $@.tmp
	$Q echo -e '\n## Reference for $<'			>> $@.tmp
	$Q node doc/jsdoc2md.js -d 2 -e 'Host' $<		>> $@.tmp
	$Q mv $@.tmp $@
doc/jsdoc.deps ::= doc/jsdocrc.json doc/jsdoc-slashes.js doc/jsdoc2md.js

# == doc/class-tree.md ==
$>/doc/class-tree.md: $(lib/AnklangSynthEngine) doc/Makefile.mk
	$(QGEN)
	$Q echo '## Ase Class Inheritance Tree'			>  $@.tmp
	$Q echo ''						>> $@.tmp
	$Q echo '```'						>> $@.tmp
	$Q ASAN_OPTIONS=detect_leaks=0 \
	   $(lib/AnklangSynthEngine) --class-tree		>> $@.tmp
	$Q echo '```'						>> $@.tmp
	$Q mv $@.tmp $@

# == pandoc ==
doc/markdown-flavour	::= -f markdown+autolink_bare_uris+emoji+lists_without_preceding_blankline-smart
doc/pdf_flags		::= --highlight-style doc/highlights.theme
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

# == ch-man-pages.md ==
doc/manual-man-pages ::= doc/anklang.1.md
$>/doc/ch-man-pages.md: $(doc/manual-man-pages) doc/filt-man.py doc/Makefile.mk	| $>/doc/
	$(QGEN)
	$Q echo '# Manual Pages'			>  $@.tmp
	$Q echo ''					>> $@.tmp
	$Q for m in $(doc/manual-man-pages) ; do		\
		n="$${m%%.md}" ; u="$${n^^}" ; n="$${u##*/}" ;	\
		echo "## $${n/\./(})" ; echo ;			\
		$(PANDOC) -t markdown -F doc/filt-man.py "$$m"	\
		|| exit $$? ; echo ; done		>> $@.tmp
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
$>/doc/cursors/cursors.css:			| $>/doc/
	$(QGEN)
	$Q $(RM) $>/doc/cursors
	$Q ln -s ../ui/cursors $>/doc/

# == anklang-manual.html ==
$(doc/manual-chapters): $>/doc/b/.doc-stamp
$>/doc/anklang-manual.html: $>/doc/template.html $(doc/manual-chapters) $>/doc/cursors/cursors.css $(doc/style/install.files)	| $>/doc/
	$(QGEN)
	$Q $(PANDOC) $(doc/markdown-flavour) \
		-s $(doc/html_flags) \
		--toc --number-sections \
		--variable=subparagraph \
		--variable date="$(version_to_month)" \
		--template=$< \
		-c style/$(notdir $(doc/style/faketex.css)) \
		--mathjax='style/mathjax-tex-svg.js' \
		$(doc/manual-chapters) -o $@
# == anklang-internals.html ==
$>/doc/anklang-internals.html: $>/doc/template.html $(doc/internals-chapters) $(doc/style/install.files)	| $>/doc/
	$(QGEN)
	$Q $(PANDOC) $(doc/markdown-flavour) \
		-s $(doc/html_flags) \
		--toc --number-sections \
		--variable=subparagraph \
		--variable date="$(version_to_month)" \
		--template=$< \
		-c style/$(notdir $(doc/style/faketex.css)) \
		--mathjax='style/mathjax-tex-svg.js' \
		$(doc/internals-chapters) -o $@

# == anklang-manual.pdf ==
# REQUIRES: python3-pandocfilters texlive-xetex pandoc2
$>/doc/anklang-manual.pdf: doc/pandoc-pdf.tex $(doc/manual-chapters) $>/doc/cursors/cursors.css					| $>/doc/
	$(QGEN)
	$Q xelatex --version 2>&1 | grep -q '^XeTeX 3.14159265' \
	   || { echo '$@: missing xelatex, required version: XeTeX >= 3.14159265' >&2 ; false ; }
	$Q $(PANDOC) $(doc/markdown-flavour) \
		--resource-path $>/doc/ --resource-path . \
		$(doc/pdf_flags) \
		--toc --number-sections \
		--variable=subparagraph \
		-V date="$(version_to_month)" \
		--variable=lot \
		-H $< \
		--pdf-engine=xelatex -V mainfont='Charis SIL' -V mathfont=Asana-Math -V monofont=inconsolata \
		-V fontsize=11pt -V papersize:a4 -V geometry:margin=2cm \
		$(doc/manual-chapters) -o $@
# == anklang-internals.pdf ==
# REQUIRES: python3-pandocfilters texlive-xetex pandoc2
$>/doc/anklang-internals.pdf: doc/pandoc-pdf.tex $(doc/internals-chapters)					| $>/doc/
	$(QGEN)
	$Q xelatex --version 2>&1 | grep -q '^XeTeX 3.14159265' \
	   || { echo '$@: missing xelatex, required version: XeTeX >= 3.14159265' >&2 ; false ; }
	$Q $(PANDOC) $(doc/markdown-flavour) \
		$(doc/pdf_flags) \
		--toc --number-sections \
		--variable=subparagraph \
		-V date="$(version_to_month)" \
		--variable=lot \
		-H $< \
		--pdf-engine=xelatex -V mainfont='Charis SIL' -V mathfont=Asana-Math -V monofont=inconsolata \
		-V fontsize=11pt -V papersize:a4 -V geometry:margin=2cm \
		$(doc/internals-chapters) -o $@

# == viewdocs ==
viewdocs: $>/doc/anklang-manual.html $>/doc/anklang-internals.html $>/doc/anklang-manual.pdf $>/doc/anklang-internals.pdf
	$Q for B in firefox google-chrome ; do \
	     command -v $$B && exec $$B $^ ; done ; \
	   for U in $^ ; do xdg-open "$$U" & done

# == installation ==
pkgdocdir ::= $(pkgdir)/doc
doc/install: $(doc/install.files) install--doc/style/install.files
	@ $(eval doc/install.pdf.files := $(wildcard $(doc/pdf.files)))
	@$(QECHO) INSTALL '$(DESTDIR)$(pkgdocdir)/.'
	$Q rm -f '$(DESTDIR)$(pkgdocdir)'/* 2>/dev/null ; true
	$Q $(INSTALL)      -d $(DESTDIR)$(pkgdocdir)/ $(DESTDIR)$(mandir)/man1/
	$Q $(CP) $(doc/install.files) $(DESTDIR)$(pkgdocdir)/
	$Q test -z "$(doc/install.pdf.files)" || $(CP) $(doc/install.pdf.files) $(DESTDIR)$(pkgdocdir)/
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
