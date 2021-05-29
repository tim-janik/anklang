# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/doc/*.d)
doc/cleandirs ::= $(wildcard $>/doc/)
CLEANDIRS         += $(doc/cleandirs)
ALL_TARGETS       += doc/all
doc/all:

# == doc/ files ==
doc/manual-chapters ::= $(strip		\
	doc/ch-intro.md			\
	doc/ch-development.md		\
	$>/ui/vue-docs.md		\
	doc/ch-appendix.md		\
)
doc/install.files ::= $(strip		\
	$>/doc/anklang-manual.html	\
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
doc/installdir ::= $(DESTDIR)$(pkglibdir)/doc
doc/install: $(doc/install.files)
	@$(QECHO) INSTALL '$(doc/installdir)/...'
	$Q rm -f '$(doc/installdir)'/* 2>/dev/null ; true
	$Q $(INSTALL)      -d $(doc/installdir)/
	$Q $(INSTALL_DATA) -p $(doc/install.files) $(doc/installdir)/
.PHONY: doc/install
install: doc/install
doc/uninstall: FORCE
	@$(QECHO) REMOVE '$(doc/installdir)/...'
	$Q rm -f -r '$(doc/installdir)'
.PHONY: doc/uninstall
uninstall: doc/uninstall
