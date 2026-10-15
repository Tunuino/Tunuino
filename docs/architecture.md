# Arquitetura Tunuino

Projeto original de ECU para STM32F407ZGT6 (LQFP144).

## Módulos previstos

- `firmware/core`: estados, agendamento e controlo do motor
- `firmware/drivers`: periféricos STM32 e interfaces de hardware
- `firmware/sensors`: aquisição e validação de sensores
- `firmware/injection`: cálculo e agendamento da injeção
- `firmware/ignition`: cálculo e agendamento da ignição
- `firmware/can`: comunicação CAN e receção lambda externa
- `firmware/tunerstudio`: protocolo de configuração e telemetria
- `hardware`: documentação e futura PCB
- `tests`: testes automatizados

## Regras

Código original Tunuino nas versões atuais sob a TUNUINO PROPRIETARY SOFTWARE LICENSE v1.0 (ver LICENSE). Licenças anteriormente concedidas para versões antigas não são revogadas por esta alteração. Dependências externas exigem revisão de licença e avisos próprios. Não copiar código de outros firmwares sem autorização compatível.

A ECU só poderá controlar um motor após testes de compilação, eletrónica, proteção de saídas, sincronização e bancada.
