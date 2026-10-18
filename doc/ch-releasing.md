# Releasing

Releases of the Anklang project are hosted on GitHub under [Anklang Releases](https://github.com/tim-janik/anklang/releases/).
`make dist` writes the version into `.version` of the tarball.
GitHub "Source code" downloads get it from `git archive` (`export-subst`).

## Versioning

The version is `git describe` without the leading `v`, for example `0.1.2` or `0.1.2-345-gabc`.
It uses annotated tags only. Lightweight tags, e.g. nightlies, never show up in it.
Git tags store release versions. In a checkout, make reads `git describe`. In a tarball, it reads `.version`.
Run `make version` to print the version and commit date.

## Release Assets

The scripts `misc/mkdeb.sh` and `misc/mkAppImage.sh` can be used to build binary packages after a successful build.
Ideally, these packages should be built from a distribution tarball, created with `make dist`.
Building the project from a distribution tarball must work without any Git or Jujutsu dependency.
Producing a distribution tarball depends on Git however, for versioning, ChangeLog and archiving.

`.github/workflows/gh-release.sh` builds the assets with `make distcheck DIST_VERSION=<tag>` and creates the GitHub release.
For lightweight tags (nightlies, RCs), DIST_VERSION is `<tag>-g<hash>`, because these tags may vanish.
The tarball, packages and `SHA256SUMS` end up in `artifacts/`.
