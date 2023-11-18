# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

devices/4ase.ccfiles += $(strip		\
	devices/liquidsfz/liquidsfz.cc	\
)
$>/devices/liquidsfz/liquidsfz.o: EXTRA_FLAGS ::= -Iexternal/liquidsfz/lib
