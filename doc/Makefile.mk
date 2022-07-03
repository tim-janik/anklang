# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/doc/*.d)
doc/cleandirs ::= $(wildcard $>/doc/)
CLEANDIRS         += $(doc/cleandirs)
ALL_TARGETS       += doc/all
doc/all:

# == doc/ files ==
doc/manual-chapters ::= $(strip		\
	doc/ch-intro.md			\
	$>/doc/ch-man-pages.md		\
	$>/ui/scripting-docs.md		\
)
doc/internals-chapters ::= $(strip	\
	doc/ch-development.md		\
	$>/ui/vue-docs.md		\
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

# == PDF with Latex dependency ==
ifneq ($(WITH_LATEX),)
doc/install.files += $>/doc/anklang-manual.pdf $>/doc/anklang-internals.pdf
endif

# == Copy files ==
$(filter %.md, $(doc/install.files)): $>/doc/%.md: %.md doc/Makefile.mk			| $>/doc/
	$(QECHO) COPY $<
	$Q $(CP) $< $@

# == doc/copyright ==
$>/doc/copyright: misc/mkcopyright.py doc/copyright.ini $>/misc/git-ls-tree.lst		| $>/doc/
	$(QGEN)
	$Q misc/mkcopyright.py -c doc/copyright.ini $$(cat $>/misc/git-ls-tree.lst) > $@.tmp
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

# == anklang-manual.html ==
$>/doc/anklang-manual.html: $>/doc/template.html $(doc/manual-chapters) $(doc/style/install.files)	| $>/doc/
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
$>/doc/anklang-manual.pdf: doc/pandoc-pdf.tex $(doc/manual-chapters)					| $>/doc/
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
