# Arquitetura Tunuino

## Firmware

O núcleo C portátil contém contratos e funções verificáveis no computador. A camada STM32 deve fornecer relógios, temporizadores, ADC, DMA, armazenamento e USB. A seleção de hardware tem de distinguir Proteus e uaEFI, a revisão de PCB e o microcontrolador efetivamente instalado.

Nenhum pino de injeção, ignição, acelerador ou relé será definido por suposição. A ativação de saídas depende de um mapa de hardware validado e de um estado de funcionamento explícito. O núcleo de comunicação atual não configura periféricos nem comanda saídas.

## Cambota

`tunuino_crank` recebe arestas temporizadas de uma roda uniforme com uma região de dentes em falta. Exige duas falhas com contagem correta antes de expor RPM e índice relativo. Uma leitura verifica timeout e limpa dados inválidos. Ruído/contagem incorreta retiram a sincronização e um recuo de relógio exige reinicialização. O protótipo é independente de GPIO, transporte e atuação; os sinais dos testes são sintéticos. Faltam aquisição STM32, validação durante arranque, referência PMS, captura física de cames e controlo de saídas. O contrato está em `crank-decoder.md`.

`tunuino_position` contém o descodificador de cambota e valida um impulso de cames numa janela de dentes configurada. Só expõe a fase do ciclo depois de confirmar dois impulsos separados por duas voltas. Cames inválidas retiram a fase; perda da cambota limpa ambas as referências. O protótipo usa índices do último dente, sem interpolação angular ou relação com cilindros/PMS, e continua sem aquisição física ou atuação. O contrato está em `cycle-position.md`.

## Comunicação

O transporte entrega um corpo binário com 1 a 4096 bytes. O pacote contém comprimento de 16 bits em ordem de rede, corpo e CRC32 de 32 bits em ordem de rede. O CRC cobre apenas o corpo. O algoritmo usa polinómio refletido 0xEDB88320, valor inicial 0xFFFFFFFF e complemento final.

O codec constitui apenas o enquadramento binário. Os comandos, códigos de resposta, páginas de calibração, assinatura de firmware e definições INI precisam de implementação e testes próprios antes de declarar compatibilidade TunerStudio.

`tunuino_dispatch_reply` encaminha pacotes completos para os handlers de identificação, sensores e mapas. A configuração exige identidade válida e fornecedores para as capacidades anunciadas; capacidades ainda sem implementação são recusadas. Mapas VE e ignição são selecionados separadamente. Os estados C de erro não geram respostas transmissíveis. O contrato, a sincronização dos snapshots e o trabalho pendente no transporte estão em `firmware-dispatch.md`.

`tunuino_stream_rx` recebe fragmentos sem alocação dinâmica, valida o comprimento após o cabeçalho e só expõe frames completos com CRC válido. Um prazo total limita a receção desde o primeiro byte; polling deteta transferências paradas. A receção para no primeiro frame e devolve a contagem consumida para preservar bytes do próximo pedido. Falhas ficam retidas até uma reinicialização com transporte limpo. O módulo ainda precisa de ser ligado ao driver USB STM32. O contrato está em `stream-reception.md`.

`tunuino_stream_tx` copia uma resposta válida e acompanha os bytes cuja transferência foi concluída pelo driver. O prazo total começa no início do envio e não é prolongado por progresso parcial. Só uma resposta pode estar pendente; uma falha bloqueia o emissor até limpeza do transporte e reinicialização. O teste integrado percorre receção, dispatcher e emissão parcial. Faltam o driver STM32 e os callbacks USB reais. O contrato está em `stream-transmission.md`.

`tunuino_link` integra recetor, dispatcher e emissor: reúne um pedido, prepara uma resposta e só liberta a receção quando a transmissão termina. Bytes de pedidos seguintes ficam com o chamador enquanto o envio estiver pendente. Uma falha encerra ambas as direções e conserva os diagnósticos até reinicialização. O módulo usa buffers fixos e precisa de um driver com filas/callbacks e cancelamento validados. O contrato está em `communication-link.md`.

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

## Integração no computador

`FirmwareLinkChecks` inicia um peer C de teste e usa os clientes Java reais através de pipes. O peer executa `tunuino_link` e os handlers existentes com dados sintéticos. O teste cobre as duas famílias, leitura integral de mapas, datalog CSV e falhas de geração, CRC e desconexão. O adaptador e os dados do peer não são incluídos nos caminhos da Console de produção. O contrato e os limites estão em `process-integration-test.md`; não substitui o ensaio USB nas placas.

## Hardware

`tools/import_kicad_netlist.py` já importa pertença exata de contactos a redes XML KiCad, com revisão da folha raiz, commit declarado e hashes do ficheiro. Conectores e alvos são explícitos; a ferramenta não atravessa circuitos nem interpreta etiquetas como pinos MCU validados. O relatório USB uaEFI F3 referencia o módulo M6 e permanece sem revisão elétrica. `tools/import_kicad_pcb.py` extrai atribuições de pads do PCB legado Proteus, preservando ausência de revisão, pads repetidos na mesma rede e contagens de endpoints sem referência. Não verifica continuidade de cobre nem equivalência ao esquema. As fontes, limites e netlist XML Proteus pendente estão em `hardware/README.md`.

`tools/compare_kicad_components.py` compara os valores e footprints selecionados com uma folha esquemática diretamente referenciada, verificando o caminho da instância. A comparação Proteus encontra coincidência desses campos, mas assinala o valor de símbolo F427 face ao PN F767 em U1501 e um caminho de instância adicional. O resultado exige revisão e não valida compatibilidade MCU ou pinagem.

Os projetos KiCad de Proteus e uaEFI são fontes de verdade para as ligações elétricas. O futuro gerador deve relacionar referência do conector, contacto, rede, circuito intermédio e pino MCU. Cada mapa precisa da revisão de hardware e do estado de validação. A seleção de uma família na interface não confirma compatibilidade de uma imagem de firmware.
