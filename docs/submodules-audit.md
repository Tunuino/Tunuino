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
