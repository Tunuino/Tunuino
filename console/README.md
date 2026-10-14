# Tunuino Console

Interface independente, MIT, para monitorização e leitura de calibrações Tunuino.

## Funcionalidades implementadas

- Dashboard RPM, MAP, TPS, CLT, IAT, AFR e tensão, atualizado apenas com telemetria validada.
- Web Serial a 115200 baud com identificação HELLO, enquadramento e CRC16.
- Leitura apenas de mapas VE e ignição 16×16 em 37 blocos de até 32 bytes, com confirmação, timeout e repetição.
- Validação de tamanho, versão, eixos, limites de células e CRC32 do snapshot antes de apresentar dados da ECU.
- Mapas de demonstração editáveis localmente e exportação CSV. Mapas recebidos da ECU são apenas de leitura.
- Desconexão, expiração de telemetria e estados de erro visíveis.

## Executar

Abrir a pasta `console/` num servidor HTTP estático e aceder a `index.html` num navegador com Web Serial (por exemplo, Chrome ou Edge em computador). A API Web Serial requer contexto seguro, como `http://localhost` ou HTTPS. Nem todos os navegadores móveis suportam Web Serial.

Testes: `cd console && npm test` (Node.js 22). GitHub Actions executa os testes automaticamente.

## Limitações de hardware e segurança

A Console está implementada como cliente de protocolo experimental, mas **não está validada numa ECU física**. O firmware STM32F429 ainda mantém USART2 fisicamente desligada e não fornece um snapshot de calibração válido em produção; por isso a leitura real não funcionará até integrar e validar ambos os lados na PCB. Não existe escrita de mapas na ECU, flash de firmware, controlo de injetores/ignição, leitura real de DTC nem suporte CAN direto no navegador.

Os mapas de demonstração são fictícios e não devem ser usados para operar um motor. Compatibilidade TunerStudio não validada.
