# Tunuino Console

Aplicação original Tunuino para configuração e diagnóstico de ECU. A organização funcional inspira-se nas ferramentas de gestão de motores, incluindo a rusEFI Console, mas **não contém código copiado da rusEFI**.

## Protótipo funcional

- Dashboard com indicadores RPM, MAP, TPS, CLT, IAT, AFR e bateria (ainda sem dados reais).
- Editor local de mapas VE e avanço de ignição, com validação e exportação CSV.
- Secções de diagnóstico e ligação, ainda sem implementação de comunicação.
- Interface responsiva para computador e telemóvel.

### Executar

Servir a pasta `console/` com um servidor HTTP estático e abrir `index.html` no navegador. A aplicação usa módulos JavaScript nativos. Não são necessárias dependências de produção.

### Testar

Com Node.js 22: `cd console && npm test`. Os testes também são executados no GitHub Actions.

## Limitações

**Modo demonstração exclusivamente.** Sem USB/serial/CAN, sem telemetria, sem escrita de mapas na ECU e sem controlo de saídas. A ECU ainda não implementa o protocolo de comunicação necessário. Os mapas iniciais são valores fictícios, não calibrações seguras para um motor.

## Próximas etapas

1. Definir protocolo Tunuino (identificação, versão, CRC, telemetria, erros e segurança).
2. Implementar transporte serial/USB e telemetria real.
3. Adicionar datalogging, leitura de DTC, calibração e mapas persistentes.
4. Melhorar visualização de mapas, gráficos e ferramentas de configuração.
5. Avaliar compatibilidade com TunerStudio e fluxos equivalentes aos da rusEFI Console.

## Licenciamento

Código próprio da Tunuino: MIT, Copyright (c) 2026 Tunuino. Qualquer integração futura de código de terceiros terá de respeitar as respetivas licenças e avisos de direitos de autor.
