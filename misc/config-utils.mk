# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

# == Escapes ==
,::=,

# == $Q $V ==
Q       ::= $(if $(findstring 1, $(V)),, @)
QSKIP   ::= $(if $(findstring s,$(MAKEFLAGS)),: )
QSTDOUT ::= $(if $(findstring 1, $(V)),, 1>/dev/null)
QSTDERR ::= $(if $(findstring 1, $(V)),, 2>/dev/null)
QGEN	  = @$(QSKIP)echo '  GEN     ' $@
QCHECK	  = @$(QSKIP)echo '  CHECK   ' $@
QECHO	  = @QECHO() { Q1="$$1"; shift; QR="$$*"; QOUT=$$(printf '  %-8s ' "$$Q1" ; echo "$$QR") && $(QSKIP) echo "$$QOUT"; }; QECHO
QDIE	  = bash -c 'echo "  ERROR    $@: $$@" >&2 ; exit 127' _

# == GIT / JJ ==
# Dependencies that are updated with a new git / jj commit
GITCOMMITDEPS  := $(wildcard .git/logs/HEAD)
REPOCOMMITDEPS := $(strip $(GITCOMMITDEPS) $(wildcard .jj/working_copy/checkout))

# == MULTIOUTPUT ==
# Macro to call for the output targets of a recipe with multiple side-effect outputs.
# I.e. it is used to define a recipe that yields multiple outputs, works with parallel
# MAKE invocations and still works regardless of which output may be removed or added
# later on.
# Much of the problem space is described here in
# [Automake - Many Outputs](https://www.gnu.org/software/automake/manual/html_node/Multiple-Outputs.html).
# A partial solution exists via [Pattern Rules](https://www.gnu.org/software/make/manual/make.html#Pattern-Examples).
# A good solution can be implemented via an .INTERMEDIATE pseudo target
# [SO - Correctly update multiple targets](https://stackoverflow.com/questions/19822435/multiple-targets-from-one-recipe-and-parallel-execution/47951465#47951465).
# Except that parallel execution may skip regeneration every other time, as described here:
# ["No recipe for 'a' and no prerequisites actually changed"](https://stackoverflow.com/questions/19822435/multiple-targets-from-one-recipe-and-parallel-execution/54025879#54025879).
# To properly deal with the recipe side-effect that MAKE is not aware of, we need to be
# [Using Empty Recipes](https://www.gnu.org/software/make/manual/make.html#Empty-Recipes).
# And if one target is missing, the recipe will still run without MAKE being aware that
# the other targets are also remade, which updates their timestamps. So a later MAKE
# invocation might rebuild subsequent targets. To avoid this, the pseudo target must
# be .PHONY in case any output targets are missing.
# Also, we use /.../*.INTERMEDIATE as pseudo target, to make conflicts with existing files
# very unlikely.
# Usage Example: $(call MULTIOUTPUT, a b c [, PseudoTargetName]): prerequisites... ; touch a b c
MULTIOUTPUT = $(foreach PseudoTarget,				\
  $(if $2, $2,							\
    /.../·$(call MULTIOUTPUTSANITIZE, $1)·.INTERMEDIATE),	\
  $(eval .INTERMEDIATE: $(PseudoTarget))			\
  $(foreach OutputTarget, $1,					\
    $(eval $(OutputTarget): $(PseudoTarget) ; ) 		\
    $(if $(wildcard $(OutputTarget)),,				\
      $(eval .PHONY: $(PseudoTarget))) )			\
  $(PseudoTarget)						\
)
#MULTIOUTPUT = $(foreach PseudoTarget,				# Local variable 'PseudoTarget' is assigned
#  $(if $2, $2, 						#   a sanitized /.../fake.INTERMEDIATE filename.
#    /.../·$(call MULTIOUTPUTSANITIZE, $1)·.INTERMEDIATE),	#
#  $(eval .INTERMEDIATE: $(PseudoTarget))			# Avoid always running the recipe just because PseudoTarget does not exist.
#  $(foreach OutputTarget, $1,					# For each side-effect OutputTarget; we introduce a dependency
#    $(eval $(OutputTarget): $(PseudoTarget) ; )		#   to serialize the generation of OutputTarget without MAKE missing a recipe.
#    $(if $(wildcard $(OutputTarget)),, 			# Special case; if any OutputTarget is missing
#      $(eval .PHONY: $(PseudoTarget))) )			#   make the PseudoTarget .PHONY; so MAKE knows *all* outputs are outdated.
#  $(PseudoTarget)						# This expands the actual (pseudo) target name used by the recipe.
#)								# Use 'foreach' as variable scope.
BLANK ::=
SPACE ::= $(BLANK) $(BLANK)
MULTIOUTPUTSANITIZE = $(subst /,∕,$(subst $(SPACE),·,$(strip $1)))

# == MATCH ==
# $(call MATCH, REGEX, wordlist)
MATCH = $(shell set -o pipefail; echo ' $2 ' | tr ' ' '\n' | grep -E '$1' | tr '\n' ' ')

# == $(call pairwise, func, list) ==
# Given a function and a list, $(call func,word1,word2) with two words at
# a time; recurse on the rest list after splitting off the first pair.
pairwise = $(if $2, $(call $1,$(word 1,$2),$(word 2,$2))$(call pairwise,$1,$(wordlist 3,$(words $2),$2)),$3)

# == $(call triplewise, func, list) ==
# Given a function and a list, $(call func,word1,word2,word3) with 3 words
# at a time; recurse on the rest list after splitting off the first triple.
triplewise = $(if $2, $(call $1,$(word 1,$2),$(word 2,$2),$(word 3,$2))$(call triplewise,$1,$(wordlist 4,$(words $2),$2)),$3)

# == Recursive Wildcard ==
# Add a slash to $1 if it is not empty and does not end in a slash
addslash = $(foreach w,$1,$(if $(filter %/,$w),$w,$w/))
# Apply $(wildcard) recursively to directory $1 and filter results against pattern list $2
# Usage for CWD:    $(call rwildcard, , *.cc *.hh)
# Usage for subdir: $(call rwildcard, subdir/, *.cc *.hh)
rwildcard = $(strip $(foreach d,$(sort $(wildcard $(call addslash,$1)*)),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d)))
