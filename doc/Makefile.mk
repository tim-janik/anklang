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

# == pandoc ==
doc/pandoc-nosmart	::= $(if $(HAVE_PANDOC1),,-smart)
doc/markdown-flavour	::= -f markdown+autolink_bare_uris+emoji+lists_without_preceding_blankline$(doc/pandoc-nosmart)
doc/version_month	::= ./version.sh -d | sed -r -e 's/^([2-9][0-9][0-9][0-9])-([0-9][0-9])-.*/m\2 \1/' \
			-e 's/m01/January/ ; s/m02/February/ ; s/m03/March/ ; s/m04/April/ ; s/m05/May/ ; s/m06/June/' \
			-e 's/m07/July/ ; s/m08/August/ ; s/m09/September/ ; s/m10/October/ ; s/m11/November/ ; s/m12/December/'

# == template.html ==
$>/doc/template.html: doc/template.diff					| $>/doc/
	$(QGEN)
	$Q $(PANDOC) -D html > $>/doc/template.html \
	  && cd $>/doc/ && patch < $(abspath doc/template.diff)

# == anklang-manual.html ==
$>/doc/anklang-manual.html: $>/doc/template.html $(doc/manual-chapters) $(doc/style/install.files)	| $>/doc/
	$(QGEN)
	$Q DATE=$$( $(doc/version_month) ) \
	  && $(PANDOC) $(doc/markdown-flavour) \
		--toc --number-sections \
		--variable=subparagraph \
		--variable date="$$DATE" \
		--template=$< \
		-s -c style/$(notdir $(doc/style/faketex.css)) \
		--mathjax='style/mathjax-tex-svg.js' \
		$(doc/manual-chapters) -o $@
doc/all: $>/doc/anklang-manual.html

# == anklang-manual.pdf ==
# REQUIRES: python3-pandocfilters texlive-xetex pandoc2
$>/doc/anklang-manual.pdf: doc/pandoc-pdf.tex $(doc/manual-chapters)					| $>/doc/
	$(QGEN)
	$Q xelatex --version 2>&1 | grep -q '^XeTeX 3.14159265' \
	   || { echo '$@: missing xelatex, required version: XeTeX >= 3.14159265' >&2 ; false ; }
	$Q DATE=$$( $(doc/version_month) ) \
	  && $(PANDOC) $(doc/markdown-flavour) \
		--toc --number-sections \
		--variable=subparagraph \
		-V date="$$DATE" \
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
