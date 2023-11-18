# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

devices/4ase.ccfiles += $(strip			\
	devices/saturation/saturation.cc	\
)
$>/devices/saturation/saturation.o: EXTRA_FLAGS ::= -Iexternal/pandaresampler/lib
