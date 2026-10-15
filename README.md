<div align="center">

<img src="assets/tunuino-logo.png" alt="Tunuino" width="600" />

<b>Engine control unit development</b>

[![Release](https://img.shields.io/github/v/release/Tunuino/tunuino?style=flat&label=release)](https://github.com/Tunuino/tunuino/releases/latest)
![Last Commit](https://img.shields.io/github/last-commit/Tunuino/tunuino?style=flat)
[![Commits Since Release](https://img.shields.io/github/commits-since/Tunuino/tunuino/latest?color=blueviolet&label=Commits%20Since%20Release)](https://github.com/Tunuino/tunuino/commits/master/)

</div>

# Tunuino

Projeto independente de gestão eletrónica de motor, com firmware STM32F429ZIT6 e aplicação **Tunuino Console**. Em desenvolvimento. O código original Tunuino está sujeito a **direitos reservados**; componentes de terceiros mantêm as respetivas licenças. Consultar [LICENSE](LICENSE) e [licenças de terceiros](THIRD_PARTY_LICENSES.md).

# Download

Consultar as [Releases](https://github.com/Tunuino/tunuino/releases) para versões publicadas. Os ficheiros dos builds de desenvolvimento estão disponíveis nos [GitHub Actions](https://github.com/Tunuino/tunuino/actions). Ainda não existe uma versão validada para utilização num motor.

# Release Notes

Consultar o [guia de versões e publicação](docs/releases.md). As versões do firmware são experimentais até à validação em hardware real.

# O que existe no repositório?

- [Firmware](firmware/) — código de ECU, configuração, arranque e módulos de comunicação.
- [Hardware](hardware/) — recursos de desenvolvimento de hardware e dependências.
- [Tunuino Console](java_console/) — módulos Java da interface de diagnóstico.
- [Ferramentas Java](java_tools/) — utilitários de desenvolvimento.
- [Simulador](simulator/) — testes nativos; ainda não é um simulador completo de motor.
- [Testes](unit_tests/) — dependências e infraestrutura de testes; testes C próprios em [tests/](tests/).
- [Documentação](docs/) — arquitetura, protocolos e progresso da implementação.

# Compilação

```sh
make test       # testes nativos
make simulator  # ambiente de validação nativo
make firmware   # firmware STM32F429 (requer arm-none-eabi)
make console    # módulos Java da Console
make tools      # ferramentas Java
```

Consultar [firmware/README.md](firmware/README.md) para os requisitos e limitações do alvo STM32.

# Clonar o repositório

```sh
git clone https://github.com/Tunuino/tunuino.git
cd tunuino
git submodule update --init --recursive
```

> **Segurança:** firmware e comunicações ainda em desenvolvimento. As saídas de injeção e ignição não estão ativadas neste protótipo. Não ligar a um motor sem validação completa do hardware e software.
