# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/images/knobs/*.d)
CLEANDIRS               += $(wildcard $>/images/knobs/)

images/knobs/cknob.color ::= '\#00ddff'
images/knobs/cknob.args  ::= -w 40 -s 14 -g 6 -n 193

# == .deps ==
$>/images/knobs/.deps: images/knobs/Makefile.mk		| $>/images/knobs/
	$(QGEN)
	$Q rsvg-convert --version | egrep 'rsvg-convert\b.* [2-9][0-9]*\.[0-9]' >/dev/null || \
	   { echo "$@: missing rsvg-convert version >= 2" ; false ; }
	$Q convert --version | egrep 'ImageMagick\b.* [2-9][0-9]*\.[0-9]' >/dev/null || \
	   { echo "$@: missing ImageMagick version >= 6" ; false ; }
	$Q touch $@
CLEANDIRS += $>/images/

# == cknob193u.png ==
$>/images/knobs/cknob193u.png: images/knobs/cknob.svg images/knobs/mksprite.py $>/images/knobs/.deps
	$(QGEN)
	$Q $(PYTHON3) images/knobs/mksprite.py -q --sprite -u $(images/knobs/cknob.color) $(images/knobs/cknob.args) $< $(@:.png=.aux)
	$Q $(RM) $(@:.png=.aux).tmp-*
	$Q mv $(@:.png=.aux).png $@
ALL_TARGETS += $>/images/knobs/cknob193u.png

# == cknob193b.png ==
$>/images/knobs/cknob193b.png: images/knobs/cknob.svg images/knobs/mksprite.py $>/images/knobs/.deps
	$(QGEN)
	$Q $(PYTHON3) images/knobs/mksprite.py -q --sprite -b $(images/knobs/cknob.color) $(images/knobs/cknob.args) $< $(@:.png=.aux)
	$Q $(RM) $(@:.png=.aux).tmp-*
	$Q mv $(@:.png=.aux).png $@
ALL_TARGETS += $>/images/knobs/cknob193b.png
