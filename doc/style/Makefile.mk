# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/doc/style/*.d)
doc/style/cleandirs ::= $(wildcard $>/doc/style/)
CLEANDIRS         += $(doc/style/cleandirs)
ALL_TARGETS       += doc/style/all
doc/style/all:

doc/style/install.files ::=
doc/style/faketex.css   ::= $>/doc/style/faketex.css

# == doc/style/mathjax ==
$>/doc/style/mathjax-tex-svg.js: doc/style/mathjax-config.js $>/node_modules/.npm.done	| $>/doc/style/
	$(QGEN)
	$Q cat $< $>/node_modules/mathjax/es5/tex-svg-full.js		> $@.tmp
	$Q mv $@.tmp $@
doc/style/install.files += $>/doc/style/mathjax-tex-svg.js

# == doc/style/faketex.css ==
$(doc/style/faketex.css): doc/style/faketex.scss doc/style/features.scss $>/node_modules/.npm.done  | $>/doc/style/
	$(QGEN)
	$Q $>/node_modules/.bin/sass --embed-source-map $< $@.tmp
	$Q mv $@.tmp $@
doc/style/install.files += $(doc/style/faketex.css)

DOC/STYLE/SEDFONTS ::= -e "s/, *url('[^()']*\.woff') *format('woff')//" -e "s|url('\./files|url('./|"

# == doc/style/charis-sil.css ==
$>/doc/style/charis-sil.css: $>/node_modules/.npm.done		| $>/doc/style/
	$(QGEN)
	$Q $(CP) $>/node_modules/@fontsource/charis-sil/files/*.woff2 $>/doc/style/
	$Q sed $>/node_modules/@fontsource/charis-sil/all.css $(DOC/STYLE/SEDFONTS) > $@.tmp
	$Q mv $@.tmp $@
doc/style/install.files += $>/doc/style/charis-sil.css

# == doc/style/inconsolata.css ==
$>/doc/style/inconsolata.css: $>/node_modules/.npm.done		| $>/doc/style/
	$(QGEN)
	$Q $(CP) $>/node_modules/@fontsource/inconsolata/files/inconsolata-latin-ext-*-normal.woff2 $>/doc/style/
	$Q sed $>/node_modules/@fontsource/inconsolata/latin-ext.css $(DOC/STYLE/SEDFONTS) > $@.tmp
	$Q mv $@.tmp $@
doc/style/install.files += $>/doc/style/inconsolata.css

# == doc/style/ installation ==
doc/style/all: $(doc/style/install.files)
doc/style/installdir ::= $(DESTDIR)$(pkglibdir)/doc/style
$(call INSTALL_DIR_RULE, doc/style/install.files, $(doc/style/installdir), $(wildcard $>/doc/style/*))
