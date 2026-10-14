# Firmware Tunuino

Alvo: STM32F407ZGT6, Cortex-M4F, LQFP144.

## Estado

Estrutura inicial, sem firmware executável. Não há ainda bootloader, HAL, drivers, scheduler ou compilação validada.

## Funcionalidades planeadas

- 8 injetores de alta impedância através de drivers externos
- 8 saídas de ignição para bobinas inteligentes
- Sensores de cambota, árvore de cames, MAP, TPS, IAT e CLT
- Controlador wideband LSU 4.9 externo via CAN
- Mapas de combustível e ignição
- Interface TunerStudio
- Proteções e diagnósticos

As funcionalidades serão adicionadas por etapas com testes.
