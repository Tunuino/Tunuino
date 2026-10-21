# Arquitetura Tunuino

## Firmware

O núcleo C portátil contém contratos e funções verificáveis no computador. A camada STM32 deve fornecer relógios, temporizadores, ADC, DMA, armazenamento e USB. A seleção de hardware tem de distinguir Proteus e uaEFI, a revisão de PCB e o microcontrolador efetivamente instalado.

Nenhum pino de injeção, ignição, acelerador ou relé será definido por suposição. A ativação de saídas depende de um mapa de hardware validado e de um estado de funcionamento explícito. O núcleo de comunicação atual não configura periféricos nem comanda saídas.

## Comunicação

O transporte entrega um corpo binário com 1 a 4096 bytes. O pacote contém comprimento de 16 bits em ordem de rede, corpo e CRC32 de 32 bits em ordem de rede. O CRC cobre apenas o corpo. O algoritmo usa polinómio refletido 0xEDB88320, valor inicial 0xFFFFFFFF e complemento final.

O codec constitui apenas o enquadramento binário. Os comandos, códigos de resposta, páginas de calibração, assinatura de firmware e definições INI precisam de implementação e testes próprios antes de declarar compatibilidade TunerStudio.

## Console

Java 21 e JavaFX, destinada ao Windows. O processamento de ficheiros e o protocolo são independentes da interface gráfica. A interface abre datalogs CSV offline. O transporte USB e a gravação devem executar fora da thread JavaFX, com timeout, cancelamento e verificação da identidade de hardware.

`SerialChannel` separa o acesso à porta da lógica binária. `PacketSession` usa um prazo total por pedido, incluindo espera por exclusividade da sessão. Trata transferências parciais e fecha a ligação quando um pedido iniciado falha, impedindo a reutilização de bytes de uma resposta atrasada. Um pedido que expire enquanto espera por outro não encerra a transação já em curso.

`UsbSerialPorts` usa jSerialComm para enumerar e abrir portas. A lista não classifica uma porta como ECU apenas pelo nome, VID/PID ou seleção de família. As operações nativas de descoberta, abertura e fecho decorrem numa thread dedicada.

O botão de identificação usa `IdentityClient` na thread USB e valida uma resposta Tunuino de versão conhecida e família correspondente. `DeviceIdentity` contém dados declarados pelo firmware. O núcleo C usa `tunuino_identity_reply` sobre o pacote completo com CRC e exige configuração explícita fornecida pela camada da placa. O protocolo está em `identity-protocol.md`. Calibração e gravação continuam por implementar; a identidade recebida não autoriza essas operações.

## Hardware

Os projetos KiCad de Proteus e uaEFI são fontes de verdade para as ligações elétricas. O futuro gerador deve relacionar referência do conector, contacto, rede, circuito intermédio e pino MCU. Cada mapa precisa da revisão de hardware e do estado de validação. A seleção de uma família na interface não confirma compatibilidade de uma imagem de firmware.
