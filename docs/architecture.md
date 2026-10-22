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

O botão de identificação usa `IdentityClient` na thread USB e valida uma resposta Tunuino de versão conhecida e família correspondente. `DeviceIdentity` contém dados declarados pelo firmware. O núcleo C usa `tunuino_identity_reply` sobre o pacote completo com CRC e exige configuração explícita fornecida pela camada da placa. O protocolo está em `identity-protocol.md`. A escrita de calibração e a gravação de firmware continuam por implementar; a identidade recebida não autoriza essas operações.

## Datalogging

`tunuino_live_reply` serializa um snapshot de sensores em inteiros com escala explícita, sem depender do layout de estruturas ou do formato de floats do compilador. A camada da placa continua responsável pela aquisição, pelo relógio e pela validade dos sinais. O formato está em `live-data-protocol.md`.

`LiveDataClient` exige a capacidade anunciada na identidade. `LiveDataRecorder` faz pedidos sequenciais na thread USB e escreve as amostras com `CsvLogWriter`, usando o mesmo formato do leitor offline. A paragem normal termina a transação em curso. A interface bloqueia ações USB concorrentes enquanto grava; o cancelamento usa uma flag que não interrompe uma transação válida a meio.

O CSV é criado com exclusividade e recebe cada amostra válida. O gráfico mantém até 600 pontos e as atualizações são agregadas para limitar trabalho pendente na thread JavaFX. Falhas de comunicação invalidam a sessão; erros de ficheiro ou tempo fora de ordem interrompem a gravação. O firmware de teste não anuncia dados em tempo real na identidade padrão: essa capacidade depende da integração do fornecedor de snapshots.

## Calibração

`tunuino_map` contém eixos RPM/MAP e células de VE ou ignição em inteiros com escala explícita. O módulo C valida os mapas e usa interpolação bilinear, sinalizando consultas fora dos eixos. O chamador deve fornecer um mapa estável; a avaliação ainda não foi integrada num ciclo de controlo STM32.

`CalibrationMap` implementa o mesmo formato na Console com armazenamento imutável. A leitura recusa dimensões, valores ou CRC inválidos antes de disponibilizar o mapa. `CalibrationDocument` aplica edições exatas sobre novos snapshots, mantém um histórico limitado e distingue o conteúdo atual do último conteúdo guardado.

`CalibrationViewer` permite editar células e consultar pontos offline. A gravação de cópias corre numa thread própria e bloqueia novas edições durante a escrita. O fecho da janela e da Console passa pelo tratamento de alterações pendentes. Uma falha de gravação mantém o editor aberto e as alterações pendentes. Não há comando de escrita de calibração nem aplicação dos valores ao motor neste bloco. O formato está em `calibration-format.md`.

`tunuino_map_snapshot_prepare` cria um ficheiro validado num snapshot com geração explícita. `tunuino_map_read_reply` serve metadados e blocos de até 256 bytes; uma mudança de geração impede continuar uma leitura antiga. O chamador serializa preparação/resposta e gere as gerações. `MapReadClient` valida a transferência completa dentro de um prazo total antes de expor um mapa imutável. A Console exige a capacidade de leitura e executa o pedido na thread USB; o resultado abre como cópia offline. O protocolo está em `map-read-protocol.md`. Ainda falta integrar estes handlers com mapas reais no STM32.

## Hardware

Os projetos KiCad de Proteus e uaEFI são fontes de verdade para as ligações elétricas. O futuro gerador deve relacionar referência do conector, contacto, rede, circuito intermédio e pino MCU. Cada mapa precisa da revisão de hardware e do estado de validação. A seleção de uma família na interface não confirma compatibilidade de uma imagem de firmware.
