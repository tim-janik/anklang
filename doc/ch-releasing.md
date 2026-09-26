# Releasing

Releases of the Anklang project are hosted on GitHub under [Anklang Releases](https://github.com/tim-janik/anklang/releases/).
A release tarball has the version baked into `.version` by `git archive` (`export-subst`).

## Versioning

The version is `git describe` without the leading `v`, for example `0.1.2` or `0.1.2-345-gabc`.
Git tags store release versions. In a checkout, make reads `git describe`. In a tarball, it reads `.version`.
Run `make version` to print the version and commit date.

## Release Assets

The scripts `misc/mkdeb.sh` and `misc/mkAppImage.sh` can be used to build binary packages after a successful build.
Ideally, these packages should be built from a distribution tarball, created with `make dist`.
Building the project from a distribution tarball must work without any Git or Jujutsu dependency.
Producing a distribution tarball depends on Git however, for versioning, ChangeLog and archiving.
