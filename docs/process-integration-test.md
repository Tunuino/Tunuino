# Teste de integração Java/C por processos

`tools/check_core.py` compila `unit_tests/link_peer.c` e inicia-o a partir de `FirmwareLinkChecks`. Os clientes Java reais de identificação, dados em tempo real e leitura de mapas comunicam por pipes binários com o ciclo C real de receção, dispatcher e transmissão. As respostas deste teste são calculadas pelo código C em execução, não por um emulador Java do protocolo.

É exclusivamente um teste no computador. O peer usa identidade, revisão, UID, sensores e mapas sintéticos; não são configurações reais de Proteus/uaEFI. Anuncia capacidades apenas dentro deste processo de teste. Não existe seleção de simulador nem utilização destes dados pela Console de produção.

## Cobertura

- Identificação das famílias Proteus e uaEFI e recusa de uma família diferente da esperada.
- Pedidos escritos em partes de até três bytes e respostas lidas em partes de até sete bytes.
- Leitura e verificação de todos os eixos e células dos mapas VE e ignição de 32×32, maiores que um frame.
- Gravação dos mapas recebidos em `.tnmap` e reabertura pelo leitor offline.
- Gravação de três amostras de cada família num CSV, reabertura e confirmação de tempo crescente.
- Ausência de sinais e mapas sem snapshot disponível.
- Substituição da geração do snapshot entre metadados e primeiro bloco, detetada pelo handler C e recusada pelo cliente Java.
- Corrupção deliberada do CRC no transporte, resposta de identificação truncada e desconexão a meio de um bloco de mapa.
- Encerramento da sessão após falha e preservação da sessão depois de trocas válidas.

Os mapas sintéticos usam uma fórmula determinística para permitir comparar cada célula, incluindo ignição negativa. Não são mapas de motor nem valores para aplicar numa ECU.

## Execução e limites

Executar `python3 tools/check_core.py`. Não requer JavaFX, jSerialComm, rede ou uma placa. O peer C usa stdin/stdout binários, incluindo modo binário explícito no Windows. A documentação da Console continua a exigir JDK 21; este teste portátil pode ser compilado com JDK 17 ou superior.

O adaptador Java de pipes existe apenas em `src/test`: limita as partes entregues, verifica o prazo das leituras e encerra o processo no fecho. A execução deste teste Java tem um limite externo de 30 segundos no runner Python. Os processos normais e os cenários de mapa têm de terminar com código zero; os ficheiros temporários são removidos no final.

O relógio do peer C é sintético e fixo. Os testes específicos de RX/TX/link continuam a verificar prazos e relógios separadamente. Este teste não mede latência real, não compila a interface JavaFX, não carrega o driver série nativo e não valida USB/DMA/callbacks STM32. Ainda é necessário compilar e ensaiar no Windows e nas placas.

## RPM calculadas de capturas simuladas

Os modos exclusivos de teste `crank-live` e `crank-stall` percorrem captura simulada→fila→descodificador de cambota→processador→montador de telemetria→link C→cliente Java→CSV. Usam uma roda sintética 36−1 e fontes sintéticas de MAP/lambda/temperatura; não fornecem dados à Console de produção.

Para cada família, o teste grava e reabre 45 amostras com RPM calculadas dos intervalos entre dentes. A gravação atravessa a falha da roda, verifica a manutenção das RPM e confirma os passos de timestamp de 1/2 ms do cenário.

No cenário de paragem, a primeira amostra é válida; depois deixam de chegar dentes enquanto as outras fontes são renovadas. A expiração das RPM produz `NOT_READY`. O cliente mantém a ligação e o gravador não escreve amostras antigas ou zeros: o ficheiro criado nesta verificação contém apenas o cabeçalho. O leitor offline continua a recusar um datalog sem amostras, conforme o seu contrato.

Os instantes de captura são simulados e avançam por pedido de teste, sem medir tempo real, latência USB ou desempenho de um motor. Estes cenários não substituem a aquisição STM32 e o ensaio nas placas.
