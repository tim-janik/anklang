# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
include $(wildcard $>/devices/blepsynth/*.d)
CLEANDIRS += $(wildcard $>/devices/blepsynth/)

# == devices/blepsynth/ definitions ==
# collect .cc sources and object names to include in ASE
devices/blepsynth/4ase.ccfiles ::= $(wildcard devices/blepsynth/*.cc)
devices/blepsynth/4ase.objects ::= $(call BUILDDIR_O, $(devices/blepsynth/4ase.ccfiles))

# == devices/blepsynth/ dependencies ==
# create object directories via explicit object dependency
$>/devices/blepsynth/.nostray $(devices/blepsynth/4ase.objects): | $(sort $(dir $(devices/blepsynth/4ase.objects)))

# == .nostray - catch non-git files ==
$(devices/blepsynth/4ase.objects): | $>/devices/blepsynth/.nostray
$>/devices/blepsynth/.nostray: $(devices/blepsynth/4ase.ccfiles)
	$(QECHO) CHECK devices/blepsynth/: no stray sources
	$Q test -z "$(DOTGIT)" || { \
	     git status -s -u -- $^ | { grep '^?? .*' && \
		{ echo 'devices/blepsynth/: error: untracked source files present'; exit 3 ; } || :; \
	   }; }
	$Q echo '$^' > $@
ifneq ($(devices/blepsynth/4ase.ccfiles),$(shell cat $>/devices/blepsynth/.nostray 2>/dev/null))
.PHONY: $>/devices/blepsynth/.nostray
endif

# == Link into libase ==
devices/4ase.objects += $(devices/blepsynth/4ase.objects)
