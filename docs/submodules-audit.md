# Tunuino submodule audit

This document records the currently configured submodules and their integration status. For current upstream license notices and authorship information, see [third-party licenses](../THIRD_PARTY_LICENSES.md) and the source files in each dependency.

## Tunuino-owned dependencies

| Path | Repository | Pinned commit |
| --- | --- | --- |
| `hardware/tunuino-kicad-library` | `Tunuino/Tunuino-KiCad-Library` | `8093ed2da45f49bdcb94a4e2152aebf2736088f4` |
| `firmware/libfirmware` | `Tunuino/Tunuino-LibFirmware` | `d2e5b907763f8cc972a8ebb17bd9f8066b4f8843` |
| `java_console/peak-can-basic` | `Tunuino/peak-can-basic` | `b7d55ed1d101096e475655ae3f49552f3348f6c4` |
| `firmware/ext/tunuino-gha` | `Tunuino/Tunuino-gha` | `7147e28d00cff9cd5da0700685e2b6a3379daab6` |

## Validation

- The KiCad library submodule path was renamed without changing its pinned revision. Validate library loading and symbol/footprint compatibility in KiCad before hardware release.
- The LibFirmware dependency passed its Linux/macOS and Windows unit-test workflows at the pinned revision.
- The PEAK CAN Java library comment was corrected without changing the CAN message constructor behavior. The main Console and Java tools workflow passed on the integration commit.
- The Windows driver asset is a binary-only dependency. Its source authenticity, signature and redistribution permissions remain to be reviewed.
- The main firmware, package, stack usage, integrity and CodeQL workflows passed after the driver submodule rename. The subsequent KiCad path rename passed the repository integrity workflow.
- Physical ECU tests and comprehensive third-party license compliance review are still pending.

This audit describes current configuration and verification; Git history retains earlier migration commits.
