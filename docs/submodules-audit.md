# Tunuino submodule audit

This inventory reflects the `.gitmodules` file and gitlink entries on `master` as checked on 2026-10-16. It is not a license clearance or a complete build validation.

## Inventory

| Submodule path | Source repository | Pinned commit |
| --- | --- | --- |
| firmware/ChibiOS | rusefi/ChibiOS | 48ec4391378e |
| firmware/ChibiOS-Contrib | rusefi/ChibiOS-Contrib | 7f6678f2c6a7 |
| unit_tests/googletest | google/googletest | c5a47cc5bd62 |
| hardware/rusefi_lib | rusefi/kicad-libraries | 6f95125ede0c |
| hardware/InteractiveHtmlBom | openscopeproject/InteractiveHtmlBom | c444135ba2fc |
| firmware/controllers/can/wideband_firmware | rusefi/wideband | 27e115b7faa1 |
| firmware/ext/uzlib | pfalcon/uzlib | 27e4f4c15ba3 |
| firmware/ext/lua | rusefi/lua | 037aa8bd0741 |
| firmware/controllers/lua/luaaa | rusefi/luaaa | 75762ca0127c |
| firmware/ext/openblt | rusefi/openblt | f887c6f0fdb5 |
| misc/hex2dfu | rusefi/hex2dfu | 02535ca53a95 |
| java_console/luaformatter | rusefi/luaformatter | b61e80566016 |
| firmware/libfirmware | rusefi/libfirmware | dffa24266200 |
| java_console/peak-can-basic | rusefi/peak-can-basic | 7053f7d3ea3b |
| .github/workflows/hw-ci/gethla | rusefi/gethla | 27a03eff2d5d |
| firmware/ext/rusefi-gha | rusefi/rusefi-gha | 55184e40b177 |
| firmware/ext/magic_enum | Neargye/magic_enum | 7d87efb4a3dd |

## Migration rules

1. Keep external upstream URLs unless a Tunuino-specific modification is required.
2. Before redirecting a submodule, create or identify a repository containing the exact pinned commit, not merely a similar source tree.
3. Review upstream and bundled dependency licenses, preserving required notices and attribution. Renaming a dependency does not transfer copyright.
4. Change the URL in `.gitmodules` only after the destination and commit have been verified.
5. Run recursive submodule initialization and relevant firmware, console and hardware CI jobs before considering migration complete.

## Verification performed

All 17 pinned commits were individually confirmed through GitHub's git commit API at their configured upstream repositories on 2026-10-16. All 17 local gitlinks were present. This establishes commit availability at the remote API, not recursive checkout or successful compilation. At the time of the initial audit, no submodule URLs or pinned commits were changed. See the direct upstream migration section below.

## Ownership and upstream provenance (2026-10-16)

The GitHub URL identifies the **host of the configured repository**, not the original author or copyright holder. GitHub repository metadata identifies the following configured `rusefi/*` repositories as forks:

| Configured repository | GitHub upstream (parent) |
| --- | --- |
| rusefi/ChibiOS | ChibiOS/ChibiOS |
| rusefi/ChibiOS-Contrib | ChibiOS/ChibiOS-Contrib |
| rusefi/lua | lua/lua |
| rusefi/openblt | feaser/openblt |
| rusefi/wideband | mck1117/wideband |
| rusefi/luaaa | gengyong/luaaa |
| rusefi/hex2dfu | encedo/hex2dfu |
| rusefi/luaformatter | neoedmund/luaformatter |
| rusefi/gethla | a-v-s/gethla |

The GitHub API reports `fork: false` for `rusefi/libfirmware`, `rusefi/peak-can-basic` and `rusefi/rusefi-gha`; this **does not prove original authorship**. Provenance of `rusefi/kicad-libraries` was not established from the metadata returned and requires manual inspection. No conclusion about copyright ownership can be drawn from fork status alone.

**Migration caution:** The pinned commit in a modified fork may not exist in its upstream parent. Before switching URLs, check that the exact pinned commit is available in the proposed target, or create a suitable Tunuino fork retaining the required history and notices. Do not replace a modified fork with its upstream without checking compatibility, branch differences, licensing, and CI.

## Direct upstream migration (2026-10-16)

The nine fork-hosted submodules identified above now use their original upstream GitHub URLs in `.gitmodules`, instead of `rusefi/*` URLs. Each original upstream was verified via GitHub's commit API to contain the exact existing gitlink commit before migration. All gitlink commit hashes remain unchanged.

Fork-specific `branch` settings for these nine submodules were removed because they may not exist at the original upstream. The other eight URLs remain unchanged. After pulling this change, run `git submodule sync --recursive` and `git submodule update --init --recursive`. A full recursive checkout and build are still required to validate the migration end-to-end.

## Remaining rusefi-hosted submodules (2026-10-16)

Four configured top-level submodules still point to `rusefi/*`:

| Path | Current URL | GitHub metadata / next action |
| --- | --- | --- |
| `hardware/rusefi_lib` | `rusefi/kicad-libraries` | Repository API reports a permanent move; resolve the canonical destination and inspect hardware library contents before any migration |
| `firmware/libfirmware` | `rusefi/libfirmware` | GitHub reports a non-fork shared firmware repository; evaluate a Tunuino-owned compatible fork only if needed |
| `java_console/peak-can-basic` | `rusefi/peak-can-basic` | GitHub reports non-fork; identify vendor provenance, licenses and integration before migration |
| `firmware/ext/rusefi-gha` | `rusefi/rusefi-gha` | GitHub reports non-fork; inspect build tooling usage before migration |

GitHub's `fork: false` is not evidence of original copyright ownership. This table does not include nested submodules, which may have separate remotes and notices. None of these four URLs was changed in this step.

## KiCad submodule URL repair (2026-10-16)

The old `hardware/rusefi_lib` URL (`rusefi/kicad-libraries`) redirects. The pinned gitlink commit `6f95125ede0cd79ac943646ba923cc5db0515922` was verified as available at `rusefi/kicad-legacy-libraries`, but not at `rusefi/kicad6-libraries`. The URL was changed to `https://github.com/rusefi/kicad-legacy-libraries.git` without changing the gitlink commit, and `THIRD_PARTY_LICENSES.md` was updated to match. This is a URL repair, **not** removal of the rusEFI dependency. Recursive checkout and CI remain to be verified.

## libfirmware migration assessment (2026-10-16)

Current gitlink: `firmware/libfirmware` -> `rusefi/libfirmware` at `dffa2426620039f6711def5a794c41c5dd03194f`.

The pinned upstream tree contains 130 Git tree entries, including board IDs, CAN shared interfaces, PT2001 controller headers and project assets, build/test scripts, and a nested `ext/googletest` submodule pinned at `bea621c3c39d8a7f71f07bd543c3a58bfa684f92`. Paths include `pt2001/include/rusefi/` and `pt2001/project/rusefi/`. Those paths must not be renamed without updating all corresponding include/build/project references.

No new replacement repository or verified equivalent gitlink revision is available. Preserve the current submodule pin until a migration repository, file-level provenance/license review, reference updates, and CI validation are ready. Hosting a copy does not transfer third-party copyright.

## libfirmware migration integration (2026-10-17)

The `firmware/libfirmware` submodule now points to `https://github.com/Tunuino/Tunuino-LibFirmware.git` at `d2e5b907763f8cc972a8ebb17bd9f8066b4f8843`. Its Linux/macOS and Windows unit-test workflows passed at this revision. The nested `ext/googletest` gitlink is restored at `bea621c3c39d8a7f71f07bd543c3a58bfa684f92`. Full main ECU firmware compilation and file-level third-party license audit remain separate validation tasks.

## peak-can-basic import (2026-10-17)

The `java_console/peak-can-basic` gitlink is updated to `Tunuino/peak-can-basic` at `12c1bba2a759ad5464218c5c559ab91e7a27a51d`. The imported tree has the same 37 paths as upstream pinned `7053f7d3ea3bd08fb47f9a8df4e107917dd3310c`. The only content change identified is line 72 of `src/main/java/peak/can/basic/TPCANMsg.java`: a comment changes the original upstream issue URL to `https://github.com/tunuino/tunuino/issues/4370`, which is not verified as an equivalent issue. Code behavior is otherwise blob-identical across the compared paths. No standalone license file or repository Actions runs were present. Verify the issue attribution and applicable upstream permissions before relicensing; main Console build and integration checks remain necessary.

## rusefi-gha dependency inspection (2026-10-17)

The `firmware/ext/rusefi-gha` gitlink remains pinned at `55184e40b177deb524f9d8294ef456b1bab06eae`. The pinned tree has just one file, `static-content/silent_st_drivers2.exe`, plus its parent directory. GitHub code search found the submodule path only in `.gitmodules`, `THIRD_PARTY_LICENSES.md`, and this audit document; no indexed references to `silent_st_drivers2.exe` were found. This is not proof that build scripts or workflows never use the binary (search indexing can be incomplete). Do not rebrand or redistribute this Windows executable without verifying its provenance, signature, license, and actual build dependency. Keep the pin unchanged pending that review.

## Tunuino-gha submodule migration (2026-10-17)

The `firmware/ext/rusefi-gha` gitlink now references `https://github.com/Tunuino/Tunuino-gha.git` at `7147e28d00cff9cd5da0700685e2b6a3379daab6`. The only imported file, `static-content/silent_st_drivers2.exe`, is blob-identical to the prior upstream pinned revision `55184e40b177deb524f9d8294ef456b1bab06eae` (blob `01d0779eadd90a981227f4ee3cb238396a58c69f`, 5,848,642 bytes). The folder path retains its previous name to avoid unverified build path changes. This does not establish the binary's authenticity, security, or redistribution license. Review those separately.

## Submodule directory rename (2026-10-17)

The submodule formerly at `firmware/ext/rusefi-gha` has been moved to `firmware/ext/tunuino-gha` without changing its pinned commit or binary. Code search found no references outside `.gitmodules`, this audit, and the third-party inventory. Historical references above are retained as migration history.
