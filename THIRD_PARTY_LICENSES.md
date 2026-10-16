# Third-party components and licenses

This repository includes third-party Git submodules. **The Tunuino Proprietary
Software License v1.0 does not replace or restrict their licenses.**

The following inventory is generated from the repository's `.gitmodules` file.
It identifies upstream sources, **not verified license classifications**.
Before distributing firmware, hardware files, or binaries, inspect each pinned
submodule revision and its license, notices, and attribution requirements.

| Submodule path | Upstream repository | License status |
| --- | --- | --- |
| `firmware/ChibiOS` | [Upstream](https://github.com/ChibiOS/ChibiOS.git) | Review pinned revision |
| `firmware/ChibiOS-Contrib` | [Upstream](https://github.com/ChibiOS/ChibiOS-Contrib.git) | Review pinned revision |
| `unit_tests/googletest` | [Upstream](https://github.com/google/googletest.git) | Review pinned revision |
| `hardware/rusefi_lib` | [Upstream](https://github.com/rusefi/kicad-libraries) | Review pinned revision |
| `hardware/InteractiveHtmlBom` | [Upstream](https://github.com/openscopeproject/InteractiveHtmlBom.git) | Review pinned revision |
| `firmware/controllers/can/wideband_firmware` | [Upstream](https://github.com/mck1117/wideband.git) | Review pinned revision |
| `firmware/ext/uzlib` | [Upstream](https://github.com/pfalcon/uzlib) | Review pinned revision |
| `firmware/ext/lua` | [Upstream](https://github.com/lua/lua.git) | Review pinned revision |
| `firmware/controllers/lua/luaaa` | [Upstream](https://github.com/gengyong/luaaa.git) | Review pinned revision |
| `firmware/ext/openblt` | [Upstream](https://github.com/feaser/openblt.git) | Review pinned revision |
| `misc/hex2dfu` | [Upstream](https://github.com/encedo/hex2dfu.git) | Review pinned revision |
| `java_console/luaformatter` | [Upstream](https://github.com/neoedmund/luaformatter.git) | Review pinned revision |
| `firmware/libfirmware` | [Upstream](https://github.com/rusefi/libfirmware.git) | Review pinned revision |
| `java_console/peak-can-basic` | [Upstream](https://github.com/rusefi/peak-can-basic) | Review pinned revision |
| `.github/workflows/hw-ci/gethla` | [Upstream](https://github.com/a-v-s/gethla.git) | Review pinned revision |
| `firmware/ext/rusefi-gha` | [Upstream](https://github.com/rusefi/rusefi-gha) | Review pinned revision |
| `firmware/ext/magic_enum` | [Upstream](https://github.com/Neargye/magic_enum) | Review pinned revision |

## Compliance notes

1. Retain original copyright and license notices in third-party files.
2. Verify license compatibility for every combined or linked distribution.
3. Comply with source-disclosure obligations where applicable, including GPL
   components if they are incorporated into a distributed combined work.
4. Do not assume that public source availability grants permission to relicense
   third-party material under Tunuino's proprietary terms.
5. The submodule list is not an exhaustive inventory of external code, build
   tools, downloaded dependencies, or contributed source files.

The proprietary notice in [LICENSE](LICENSE) applies only where Tunuino owns
or controls the rights needed to impose those terms. Earlier grants of rights
remain unaffected for versions previously distributed under those terms.
