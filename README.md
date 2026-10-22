# Tunuino

Firmware próprio para centralinas Proteus e uaEFI e Tunuino Console para Windows.

O projeto está no início do desenvolvimento. Ainda não produz firmware para gravar numa ECU.

## Implementado

- Fila C11 de 64 capturas cambota/cames, com ordem temporal, falhas retidas e testes de produtor/consumidor concorrentes.
- Protótipo C de identificação do ciclo de 720° por cames, com confirmação em duas voltas e invalidação independente da fase.
- Protótipo C de descodificação de cambota com dentes em falta, confirmação de sincronização, RPM e perda de sinal, testado com sequências sintéticas.
- Codificação e validação de pacotes com tamanho e CRC32, em C e Java.
- Testes de integridade, limites e interoperabilidade entre as duas implementações.
- Comparação de componentes PCB/esquema Proteus, com deteção da ambiguidade STM32F427/STM32F767 e de instâncias adicionais.
- Importador PCB KiCad legado, com relatório USB Proteus rastreado ao ficheiro e sem revisão elétrica.
- Importador de netlists XML KiCad, com rastreio da fonte e relatório USB da uaEFI F3 ainda não revisto.
- Teste entre processos Java/C com clientes reais, ciclo do firmware, mapas completos, CSV e falhas injetadas.
- Base da Console em Java 21 e JavaFX, com visualização de datalogs CSV offline.
- Enumeração de portas série e abertura/fecho da ligação USB na Console, através de jSerialComm.
- Sessão de pedidos/respostas com limite de tempo total, leituras e escritas parciais, cancelamento e invalidação após erro.
- Protocolo de identificação Tunuino em C e Java, com família, série MCU, revisão, versão de firmware e UID.
- Pedido de identificação explícito na Console e rejeição de família diferente da selecionada.
- Protocolo de dados em tempo real em C/Java e gravação CSV com início/paragem na Console.
- Gráfico de RPM durante a gravação, limitado a 600 pontos; os dados válidos continuam a ser gravados no ficheiro.
- Mapas VE e ignição até 32×32, com validação, CRC e interpolação bilinear em C/Java.
- Consulta offline de ficheiros `.tnmap` na Console, com tabela RPM/MAP e cálculo de um ponto.
- Ciclo C de pedido/resposta que liga receção, dispatcher e envio, bloqueando pedidos novos até terminar a resposta.
- Transmissão incremental em C, com cópia validada da resposta, conclusões parciais, prazo total e bloqueio após falha.
- Receção incremental em C, com buffer limitado, CRC, prazo total e tratamento de pacotes fragmentados ou juntos.
- Dispatcher C para identificação, datalog e mapas, com validação de capacidades e fornecedores configurados.
- Leitura de mapas por blocos em C/Java, com geração do snapshot, CRC e prazo total; botão na Console condicionado à capacidade do firmware.
- Edição offline das células VE/ignição, validação decimal exata, desfazer/refazer e gravação de uma nova cópia.

## Por implementar e validar

- Validação USB física e da identificação no Windows com firmware Tunuino nas centralinas, e recuperação de ligação.
- Firmware STM32, aquisição/validação física de cambota e cames, padrões adicionais e controlo do motor.
- Drivers, bootloader e gravação específicos de cada variante de hardware.
- Aquisição física e validação do datalogging nas placas, definições INI para TunerStudio e Autotune online/offline.
- Exportação da netlist Proteus e comparação com o PCB, rastreio dos circuitos intermédios, validação elétrica de conectores/pinos STM32 e geração de definições de hardware.

## Verificar o núcleo

Requer Python 3, um compilador C11 com atomics unsigned sempre lock-free e JDK 17 ou superior para os testes independentes da interface:

```sh
python3 tools/check_core.py
```

O teste integrado inicia um peer C com dados sintéticos e comunica com os clientes Java por pipes binários. Não exige uma ECU nem valida USB físico. Os cenários e limites estão em [process-integration-test.md](docs/process-integration-test.md).

## Executar a Console no Windows

Requer JDK 21 e Maven 3.9 ou superior. A primeira resolução das dependências necessita de rede; depois podem ser usadas as dependências em cache.

```powershell
cd console
mvn javafx:run
```

Os datalogs CSV usam cabeçalho `time_ms,rpm,map_kpa,lambda,coolant_c` e ponto decimal. A Console abre ficheiros locais sem ligação à ECU.

## Ligação USB

Seleciona a família da centralina, usa **Atualizar portas**, seleciona a porta correspondente e escolhe **Ligar USB**. A abertura da porta não confirma a identidade da ECU.

Usa **Identificar ECU** para enviar o pedido de leitura ao firmware Tunuino. Uma resposta válida apresenta família, série STM32, revisão, versão do firmware e UID. Uma resposta inválida, uma família diferente da selecionada ou a ausência de resposta encerra a sessão. A identificação precisa de firmware Tunuino que implemente o handler; ainda não existe uma imagem STM32 gravável neste projeto.

O transporte binário admite um pedido de cada vez e encerra a sessão após timeout, erro de CRC ou resposta incompleta. Não repete automaticamente comandos. Os testes usam um transporte simulado; a integração nativa com jSerialComm e a interface JavaFX ainda precisam de compilação e ensaio no Windows com as centralinas.

O núcleo de identificação recusa UID, revisão ou família não configurados. A camada específica de cada placa ainda precisa de fornecer os dados reais. Os dados de identificação dos testes são sintéticos e não validam pinagem ou compatibilidade de gravação. O formato está em [identity-protocol.md](docs/identity-protocol.md). O encaminhamento comum dos pedidos está documentado em [firmware-dispatch.md](docs/firmware-dispatch.md); a receção incremental está em [stream-reception.md](docs/stream-reception.md). O envio incremental está em [stream-transmission.md](docs/stream-transmission.md). O ciclo comum está em [communication-link.md](docs/communication-link.md). A ligação ao driver USB STM32 continua pendente.

## Gravar datalog

Depois de identificar a centralina, **Gravar datalog** fica disponível se o firmware anunciar suporte de dados em tempo real. Escolhe um nome de ficheiro novo; ficheiros existentes são preservados. A Console grava tempo, RPM, MAP, lambda e temperatura do líquido, e mostra os valores recebidos durante a gravação.

Usa **Parar gravação** para terminar o pedido em curso e fechar o ficheiro. A ligação fica aberta numa paragem normal. Não são criadas amostras quando os sinais estiverem indisponíveis. Um erro interrompe a gravação, mantendo as linhas que já foram escritas. Recuos do relógio da ECU também interrompem a gravação.

A pausa entre pedidos é de 100 ms, acrescida do tempo necessário à comunicação. O formato está em [live-data-protocol.md](docs/live-data-protocol.md). Os testes usam amostras sintéticas; o projeto ainda não lê sensores físicos nem produz uma imagem STM32 gravável.

## Editar e consultar mapas

Usa **Abrir mapa** para carregar um ficheiro `.tnmap` válido. A janela mostra o tipo de mapa, os eixos RPM/MAP e as células. Introduz RPM e MAP em kPa e usa **Calcular ponto** para consultar a interpolação. Se o ponto ficar fora dos eixos, a janela assinala que foi limitado à borda.

Faz duplo clique numa célula para editar o valor. **Enter** confirma e **Esc** cancela. Usa ponto decimal e até três casas decimais; valores inválidos são recusados, sem arredondamento silencioso. Os eixos RPM/MAP continuam fixos. **Desfazer** e **Refazer** mantêm até 100 alterações no histórico da janela.

**Guardar cópia** pede um ficheiro novo e preserva os ficheiros existentes. Depois de guardar, a cópia pode ser aberta novamente. O título assinala alterações pendentes com `*`. Ao fechar o mapa ou a Console, podes guardar uma cópia, descartar as alterações ou cancelar o fecho. Confirma ou cancela uma célula ainda em edição antes de guardar ou fechar.

Depois de identificar uma ECU cujo firmware anuncie leitura de mapas, seleciona VE ou ignição e usa **Ler mapa da ECU**. A Console valida a transferência completa antes de abrir uma cópia no editor. A integração do handler no firmware STM32 e o ensaio físico continuam pendentes. O protocolo está em [map-read-protocol.md](docs/map-read-protocol.md).

A edição é offline. Ainda não existe envio de mapas para a ECU ou aplicação a injeção/ignição. Não existem mapas padrão de motor. O formato e os limites estão em [calibration-format.md](docs/calibration-format.md).

## Hardware KiCad

As fontes Proteus/uaEFI e o procedimento de importação estão em [hardware/README.md](hardware/README.md). Os relatórios identificam contactos e redes USB da uaEFI F3 até ao módulo M6 e as atribuições dos pads USB do PCB Proteus. Continuam sem validação elétrica e não geram pinagem de firmware. O PCB Proteus não declara revisão; a comparação com o esquema continua pendente.

A comparação Proteus encontrou uma ambiguidade no MCU: U1501 usa o valor `STM32F427ZGTx`, mas a propriedade `PN` indica `STM32F767ZIT6`. A variante física e a configuração STM32 continuam por confirmar. O detalhe está em [hardware/README.md](hardware/README.md).

## Núcleo de cambota

O protótipo portátil e os limites estão em [crank-decoder.md](docs/crank-decoder.md). Só expõe RPM depois de confirmar a falha e a contagem de dentes. Ainda não lê entradas físicas nem comanda injeção/ignição.

A extensão experimental para um impulso de cames por ciclo está em [cycle-position.md](docs/cycle-position.md). Distingue a validade das RPM da fase de 720°; falta a captura física e a referência PMS/cilindro.

A fila de eventos e as condições de integração com interrupções estão em [capture-queue.md](docs/capture-queue.md). Falta ligá-la à captura física STM32 e coordenar a drenagem com o polling.
