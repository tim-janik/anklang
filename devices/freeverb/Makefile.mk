# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

devices/4ase.ccfiles += $(strip		\
	devices/freeverb/freeverb.cc	\
)
$>/devices/freeverb/freeverb.o: EXTRA_FLAGS ::= -Wno-unused-function
