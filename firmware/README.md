# Firmware Tunuino

Alvo: STM32F429ZIT6, Cortex-M4F, LQFP144.

## Estado

Firmware de arranque e protocolo de leitura compilados para ARM. Comunicação física, saídas de potência e validação em placa ainda pendentes.

## Funcionalidades planeadas

- 8 injetores de alta impedância através de drivers externos
- 8 saídas de ignição para bobinas inteligentes
- Sensores de cambota, árvore de cames, MAP, TPS, IAT e CLT
- Controlador wideband LSU 4.9 externo via CAN
- Mapas de combustível e ignição
- Interface TunerStudio
- Proteções e diagnósticos

As funcionalidades serão adicionadas por etapas com testes.

## Compilação do firmware

A partir da raiz do projeto, executar `make firmware`, ou executar
`make -C firmware` para compilar diretamente o módulo. O ponto de entrada
`firmware/Makefile` seleciona explicitamente `BOARD=stm32f429` e encaminha
para `firmware/platform/stm32f429/Makefile`. Valores de `BOARD` não suportados
produzem um erro em vez de selecionar silenciosamente outra placa.

É necessária uma toolchain `arm-none-eabi-`. Esta organização prepara a
separação por placas, mas não significa compatibilidade com o sistema de
compilação completo da arquitetura de referência. Não ativa periféricos nem
saídas de motor.
