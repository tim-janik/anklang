# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/ase/tests/*.d)
CLEANDIRS      += $(wildcard $>/ase/tests/)

# == ase/tests/ file sets ==
ase/tests/ccsources ::= $(wildcard ase/tests/*.cc)
ase/tests/objects   ::= $(call BUILDDIR_O, $(ase/tests/ccsources))

# == ase/tests/ dependencies ==
$(ase/tests/objects): | $>/ase/tests/
