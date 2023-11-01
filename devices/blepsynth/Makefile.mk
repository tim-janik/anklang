# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

devices/4ase.ccfiles += $(strip			\
	devices/blepsynth/bleposcdata.cc	\
	devices/blepsynth/blepsynth.cc		\
)
$>/devices/blepsynth/blepsynth.o: EXTRA_FLAGS ::= -Iexternal/pandaresampler/lib
