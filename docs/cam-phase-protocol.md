# Aquisição da fase de cames

A Proteus v0.7/F427 mede a posição da aresta selecionada na entrada de cames já adquirida por `board_io.c`. Exige dois impulsos coerentes por 720° do modelo existente. O ângulo usa o índice do último dente e a fração temporal até à aresta, estimada com o período filtrado de cambota. Não extrapola para além do próximo dente físico esperado; admite o intervalo maior da falha da roda.

A origem é o primeiro dente depois da falha, com a paridade configurada de 0/360°. São graus de **cambota**, entre 0 e 719,99°. Não são graus de cames, PMS, avanço VVT ou desvio de um alvo. A resolução de transporte de 0,01° não garante precisão física: a interpolação depende do período estimado, da aceleração e da latência da captura EXTI.

`tunuino_position_cam_read` devolve a última aresta confirmada e o seu timestamp de 64 bits. A leitura não faz o ângulo avançar. Duplicação, janela errada, perda de cames/cambota e recuo do relógio retiram a medida. `tunuino_capture_processor_cam` aplica os mesmos bloqueios de fila pendente, evento futuro, timeout e falha retida da posição; não avança o relógio do descodificador para o instante da consulta.

O firmware publica o snapshot depois da aquisição, limpa-o ao reinicializar captura ou aplicar configuração e serve `TUNUINO:H` no envelope CRC existente. Apenas leitura: não grava flash, altera calibrações ou comanda saídas. Preserva páginas persistentes, os 100 bytes TunerStudio e o CSV existente.

| Offset | Tipo big-endian | Conteúdo |
|---|---|---|
| 0 | U8 | Estado 0 |
| 1 | U8 | Formato 1 |
| 2 | U8 | Bits 0 cambota válida, 1 ciclo confirmado, 2 medida válida |
| 3 | U8 | Reservado 0 |
| 4 | U32 | Ângulo em centésimos de grau de cambota |
| 8 | U64 | Instante da última aresta medida, µs |
| 16 | U64 | Instante do snapshot de aquisição, µs |
| 24 | U32 | Geração da configuração |
| 28 | U16 | Posições da roda, incluindo dentes em falta |
| 30 | U8 | Dentes em falta |
| 31 | U8 | Reservado 0 |

Conjuntos de bits válidos: 0, 1, 3, 7. Sem bit 2, ângulo e timestamp da aresta são zero; a Console apresenta indisponibilidade. Firmware anterior responde `0x83`: a Console deixa de repetir o pedido nessa sessão e mantém a monitorização. Respostas malformadas continuam a ser erros. O painel descarta gerações diferentes do estado do motor e limpa a fase ao terminar a monitorização/desligar.

Testes automáticos com entradas sintéticas: 36-1/60-2, duas paridades, meio dente, intervalo da falha, timestamp além de 32 bits, estabilidade do snapshot, duplicação/perda, bloqueios da fila, CRC/layout independente e validação Java. A aquisição está ligada ao firmware da placa, mas exige ensaio físico. Não há controlo de válvula VVT, calibração de referência nem suporte de múltiplas árvores ou padrões com vários impulsos por ciclo.
