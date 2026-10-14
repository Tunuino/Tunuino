# Protocolo ECU ↔ Tunuino Console

O protocolo de comunicação será definido e versionado antes de desenvolver a interface gráfica.

Requisitos: identificação da ECU, versão do protocolo, leitura de telemetria, configuração com verificação de integridade, gestão de erros e limites de segurança.

Transportes previstos: USB CDC/serial primeiro; CAN posteriormente. A compatibilidade com TunerStudio será tratada separadamente para evitar dependências entre os dois clientes.

Nenhuma mensagem binária ou formato de memória está ainda definido.
