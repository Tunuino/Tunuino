<p align="center"><img src="assets/tunuino-logo.png" alt="Tunuino" width="420"></p>

# Tunuino

Firmware de gestão eletrónica de motor e aplicação **Tunuino Console**, em desenvolvimento.

- **ECU:** arquitetura experimental STM32F429ZIT6.
- **Mapas:** combustível VE e ignição 16×16.
- **Console:** telemetria e leitura experimental de mapas.
- **TunerStudio:** integração em desenvolvimento, ainda não validada em hardware.
- **Segurança:** saídas de injeção e ignição não ativadas neste protótipo.

Projeto independente, distribuído sob licença MIT.

> Firmware e comunicação ainda em desenvolvimento. Não utilizar para controlar um motor sem validação completa do hardware e do software.

## Repository build entry points

The root `Makefile` provides a single entry point across the existing modules:
`make test` (native C tests), `make simulator` (native validation harness),
`make firmware` (STM32F429 cross-compile), `make console` (Java Console modules),
and `make tools` (Java tools). `make clean` removes native and firmware build
outputs. The simulator target is a host test harness, **not** a complete engine
simulator. Java and STM32 toolchains must be installed separately.

## Versions and Releases

[Latest GitHub Release](https://github.com/Tunuino/tunuino/releases/latest) ·
[All releases](https://github.com/Tunuino/tunuino/releases) ·
[Versioning and publishing guide](docs/releases.md)

Version tags `vMAJOR.MINOR.PATCH` trigger an **experimental pre-release**
with the STM32F429 binary and checksum after CI packaging succeeds. A normal
commit to `master` does not create a release.
