# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

EXTERNAL_CXX_STAMPS :=

# == fetch-and-check ==
# $(call fetch-and-check, FILENAME, SHA256HASH, URL)
define fetch-and-check
( sha256sum -c --status <<<'$(strip $2)  $(strip $1)' 2>/dev/null || \
    curl -# -fSL '$(strip $3)' -o $(strip $1) ) && \
sha256sum -c <<<'$(strip $2)  $(strip $1)' || \
  { echo '$(strip $1): ERROR: failed to fetch: $(strip $3)' >&2 ; exit 1 ; }
endef

# == external/clean ==
external/clean:
	rm -r -f external/*/
.PHONY: external/clean
clean: external/clean


