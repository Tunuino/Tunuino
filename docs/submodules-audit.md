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

All 17 pinned commits were individually confirmed through GitHub's git commit API at their configured upstream repositories on 2026-10-16. All 17 local gitlinks were present. This establishes commit availability at the remote API, not recursive checkout or successful compilation. No submodule URLs or pinned commits were changed during this audit.

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
