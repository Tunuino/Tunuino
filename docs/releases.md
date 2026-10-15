# Tunuino versions and releases

GitHub **Releases** are published from version tags on the existing `master`
branch. No release branch is required. The release workflow is defined in
`.github/workflows/firmware-package.yml`.

## Versioning

Use tags of the form `vMAJOR.MINOR.PATCH` (for example `v0.1.0`).
The Git tag is the authoritative release version. The default branch is
ongoing development, not a stable release. No version tag is created by this
file alone.

## Publishing

1. Ensure the firmware build and native tests pass on `master`.
2. Create and push an annotated version tag pointing to the verified commit.
3. GitHub Actions builds and checks the standalone STM32F429 image, uploads
   a workflow artifact and, **only on tag pushes**, publishes a GitHub
   pre-release with the binary and SHA256 checksum.
4. Confirm the Release page and downloaded checksum before distribution.

Manual workflow dispatch produces a build artifact only; it does **not**
publish a release. All firmware releases are marked experimental because
physical CAN, USB, bootloader recovery, engine outputs and real-hardware
validation are incomplete. The release is not an ECU ready for an engine.
