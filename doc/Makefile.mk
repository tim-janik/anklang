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
	doc/ch-development.md		\
	$>/ui/vue-docs.md		\
	doc/ch-appendix.md		\
)
doc/install.files ::= $(strip		\
	$>/doc/anklang-manual.html	\
	$>/doc/anklang.1		\
	$>/doc/NEWS.md			\
	$>/doc/NEWS.html		\
	$>/doc/README.md		\
	$>/doc/README.html		\
)
doc/all: $(doc/install.files)

# == Copy *.md ==
$(filter %.md, $(doc/install.files)): $>/doc/%.md: %.md doc/Makefile.mk			| $>/doc/
	$(QECHO) COPY $<
	$Q cp $< $@

# == pandoc ==
doc/markdown-flavour	::= -f markdown+autolink_bare_uris+emoji+lists_without_preceding_blankline-smart
doc/html_flags		::= --html-q-tags --section-divs --email-obfuscation=references # --toc --toc-depth=6
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
$>/doc/template.html: doc/template.diff					| $>/doc/
	$(QGEN)
	$Q $(PANDOC) -D html > $>/doc/template.html \
	  && cd $>/doc/ && patch < $(abspath doc/template.diff)

# == anklang-manual.html ==
$>/doc/anklang-manual.html: $>/doc/template.html $(doc/manual-chapters) $(doc/style/install.files)	| $>/doc/
	$(QGEN)
	$Q $(PANDOC) $(doc/markdown-flavour) \
		--toc --number-sections \
		--variable=subparagraph \
		--variable date="$(version_to_month)" \
		--template=$< \
		-s -c style/$(notdir $(doc/style/faketex.css)) \
		--mathjax='style/mathjax-tex-svg.js' \
		$(doc/manual-chapters) -o $@

# == anklang-manual.pdf ==
# REQUIRES: python3-pandocfilters texlive-xetex pandoc2
$>/doc/anklang-manual.pdf: doc/pandoc-pdf.tex $(doc/manual-chapters)					| $>/doc/
	$(QGEN)
	$Q xelatex --version 2>&1 | grep -q '^XeTeX 3.14159265' \
	   || { echo '$@: missing xelatex, required version: XeTeX >= 3.14159265' >&2 ; false ; }
	$Q $(PANDOC) $(doc/markdown-flavour) \
		--toc --number-sections \
		--variable=subparagraph \
		-V date="$(version_to_month)" \
		--variable=lot \
		-H $< \
		--pdf-engine=xelatex -V mainfont='Charis SIL' -V mathfont=Asana-Math -V monofont=inconsolata \
		-V fontsize=11pt -V papersize:a4 -V geometry:margin=2cm \
		$(doc/manual-chapters) -o $@

# == viewdocs ==
viewdocs: $>/doc/anklang-manual.html $>/doc/anklang-manual.pdf
	$Q for B in firefox google-chrome ; do \
	     command -v $$B && exec $$B $^ ; done ; \
	   for U in $^ ; do xdg-open "$$U" & done

# == installation ==
doc/installdir ::= $(pkgdir)/doc
doc/install: $(doc/install.files)
	@$(QECHO) INSTALL '$(DESTDIR)$(doc/installdir)/...'
	$Q rm -f '$(DESTDIR)$(doc/installdir)'/* 2>/dev/null ; true
	$Q $(INSTALL)      -d $(DESTDIR)$(doc/installdir)/ $(DESTDIR)$(mandir)/man1/
	$Q $(INSTALL_DATA) -p $(doc/install.files) $(DESTDIR)$(doc/installdir)/
	$Q $(INSTALL) -d $(DESTDIR)$(mandir)/man1/ && ln -s -r $(DESTDIR)$(pkgdir)/doc/anklang.1 $(DESTDIR)$(mandir)/man1/anklang.1
.PHONY: doc/install
install: doc/install
doc/uninstall: FORCE
	@$(QECHO) REMOVE '$(DESTDIR)$(doc/installdir)/...'
	$Q rm -f -r '$(DESTDIR)$(doc/installdir)'
	$Q rm -f '$(DESTDIR)$(mandir)/man1/anklang.1'
.PHONY: doc/uninstall
uninstall: doc/uninstall
