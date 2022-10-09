# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

# variable for subdir sources
devices/4ase.ccfiles ::=

# subdir Makefiles add to devices/4ase.ccfiles
include devices/blepsynth/Makefile.mk
include devices/freeverb/Makefile.mk
include devices/liquidsfz/Makefile.mk

# local sources
devices/4ase.ccfiles += $(strip		\
        devices/colorednoise.cc		\
)

# derive object files
devices/4ase.objects ::= $(call BUILDDIR_O, $(devices/4ase.ccfiles))

# create object directories via explicit object dependency
$(devices/4ase.objects):	| $(sort $(dir $(devices/4ase.objects)))

# include object dependencies
include $(wildcard $(devices/4ase.objects:.o=.o.d))

# clean build directory
CLEANDIRS += $>/devices/

# == devices/lint ==
devices/lint: tscheck eslint stylelint
	-$Q { TCOLOR=--color=always ; tty -s <&1 || TCOLOR=; } \
	&& grep $$TCOLOR -nE '(/[*/]+[*/ ]*)?(FI[X]ME).*' -r devices/
.PHONY: devices/lint
lint: devices/lint
