# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/devices/*.d)
CLEANDIRS += $(wildcard $>/devices/)

# == devices/ definitions ==
# collect .cc sources and object names to include in ASE
devices/4ase.ccfiles ::= $(wildcard devices/*.cc)
devices/4ase.objects ::= $(call BUILDDIR_O, $(devices/4ase.ccfiles))

# == Sub Directories ==
include devices/blepsynth/Makefile.mk

# == devices/ dependencies ==
# create object directories via explicit object dependency
$(devices/4ase.objects): | $(sort $(dir $(devices/4ase.objects)))

# == .nostray - catch non-git files ==
$(devices/4ase.objects): | $>/devices/.nostray
$>/devices/.nostray: $(devices/4ase.ccfiles)
	$(QECHO) CHECK devices/: no stray sources
	$Q test -z "$(DOTGIT)" || { \
	     git status -s -u -- $^ | { grep '^?? .*' && \
		{ echo 'devices/: error: untracked source files present'; exit 3 ; } || :; \
	   }; }
	$Q echo '$^' > $@
ifneq ($(devices/4ase.ccfiles),$(shell cat $>/devices/.nostray 2>/dev/null))
.PHONY: $>/devices/.nostray
endif
