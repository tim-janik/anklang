# Releasing

Releases of the Anklang project are hosted on GitHub under [Anklang Releases](https://github.com/tim-janik/anklang/releases/).
A release encompasses a distribution tarball that has the release version number baked into the misc/version.sh script.

## Versioning

The Anklang project uses **`MAJOR.MINOR.MICRO[.DEVEL][-SUFFIX]`** version numbers with the folloing uses:
- **`MAJOR`** - The major number is currently 0, so all bets are off. It is planned to signify major changes to users.
- **`MINOR`** - The minor number indicates significant changes, often these are user visible improvements.
- **`MICRO`** - The micro number increases with every release.
- **`DEVEL`** - The devel part is optional and increases with every new commit, it numbers builds between official releases.
  The presence of the `[.DEVEL]` part indicates a version ordered *after* its corresponding `MAJOR.MINOR.MICRO` release.
- **`SUFFIX`** - An optional suffix is sometimes used for e.g. release candidates.
  The presence of the `[-SUFFIX]` part indicates a version ordered *before* its corresponding `MAJOR.MINOR.MICRO` release.

Git tags are used to store release versions, development versions are derived from those tags similar to how `git describe` works.
The current version can always be obtained by invoking `misc/version.sh`.

## Release Assets

The script `misc/mkassets.sh` can be used to create and clean up a release build directory and it triggers the necessary rules to
create a distribution tarball and to build the release assets. All assets are built from the distribution tarball without any
Git dependency. Producing a distribution tarball depends on Git however.
