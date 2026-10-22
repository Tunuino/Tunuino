# Processamento das capturas

`capture_processor.h` liga a fila de capturas ao descodificador cambota/cames. O produtor entrega eventos e um único consumidor drena a fila e consulta leituras. É um módulo portátil experimental, sem driver STM32 ou atuação no motor.

## Relógio de eventos e frescura

O descodificador avança exclusivamente com os timestamps capturados. O relógio `now_us` das chamadas do consumidor serve para verificar a frescura dos dados; uma leitura de estado não avança o relógio interno do descodificador para esse instante.

Esta separação permite processar uma captura publicada depois de uma leitura, embora o seu timestamp seja anterior à hora dessa leitura. Continua a ter de ser posterior ou igual ao último evento capturado/processado, segundo as regras da fila. Isso evita confundir atraso de publicação com recuo do relógio.

O relógio do consumidor deve ser não decrescente, no mesmo domínio de 64 bits em microssegundos dos eventos. Uma chamada que faça esse relógio recuar produz `CLOCK`, retido até reinicialização. Um evento anterior ao instante de inicialização também falha no descodificador. Os contadores físicos de 32 bits continuam a precisar de extensão e ordenação pela integração STM32.

## API e limites

Inicializar com `tunuino_capture_processor_init`, uma configuração de posição explícita e o instante inicial, com produtor/consumidor e hardware parados.

O produtor serializado usa `tunuino_capture_processor_push`. Continua sujeito ao contrato de um produtor e um consumidor da fila, incluindo a ordem cambota antes de cames em timestamps iguais. O produtor não lê campos do consumidor; os seus retornos descrevem o estado da fila. Uma falha do consumidor não pára por si as interrupções do hardware.

`tunuino_capture_processor_drain` processa entre 1 e 64 eventos por chamada, conforme o orçamento fornecido, e devolve a quantidade efetivamente descodificada. Não altera os timestamps. Há no máximo 64 eventos na fila e um registo adicional para um evento já retirado da fila cujo timestamp ainda seja futuro em relação a `now_us`.

Uma captura ligeiramente futura pode surgir se o produtor captar um evento depois de o consumidor ter amostrado a hora. É conservada sem a descodificar. Chamadas posteriores retomam-na quando o relógio a alcançar. Não é repetida nem reordenada. Um desfasamento persistente deve ser investigado na integração dos relógios.

`tunuino_capture_processor_read` limpa a saída antes de qualquer verificação e só disponibiliza o snapshot quando não há trabalho pendente, falhas ou expiração. Os estados do processador são:

| Estado | Significado |
|---|---|
| `READY` | Snapshot disponível; respeitar ainda `crank_valid` e `cycle_valid` |
| `NO_SIGNAL` | Sem captura de cambota ou prazo de frescura expirado |
| `BACKLOG` | Há eventos por processar; a saída fica limpa |
| `FUTURE` | Um evento espera que o relógio do consumidor o alcance; saída limpa |
| `QUEUE_FAULT` | Falha retida de ordem/capacidade/argumento na fila; saída limpa |
| `CLOCK` | Recuo de relógio ou falha de relógio do descodificador; saída limpa |
| `ARGUMENT` | Configuração ou chamada inválida |

`READY` não equivale a sincronização: durante aquisição, por exemplo, as duas flags continuam inválidas. `state`, `queue_status` e `decoder_status` são diagnósticos de leitura. Os objetos internos não devem ser consultados diretamente para contornar a validade do snapshot.

## Expiração e recuperação

A frescura usa o prazo de perda de sinal da configuração de cambota, contado desde a última captura de cambota processada. Cames isoladas não renovam esse prazo. No instante exato de expiração, a leitura é invalidada e o descodificador é reinicializado no último timestamp de evento, preservando a separação dos relógios.

A recuperação depois de expiração exige novamente a evidência normal de cambota e cames. Uma captura publicada com atraso não recupera uma fase antiga apenas por chegar à fila. Os testes incluem expiração seguida de publicação atrasada e posterior recuperação com uma sequência completa nova.

Falhas da fila e do relógio do processador são retidas. Nenhuma leitura válida é entregue depois de observadas, até reinicialização com ambos os lados e o hardware parados. O integrador tem de parar as capturas, descartar operações pendentes e tratar qualquer estado externo de atuação. Este módulo não cancela saídas nem agenda eventos.

As chamadas de consumidor devem ser serializadas. O objeto/configuração/buffers não podem sobrepor-se. Como em qualquer snapshot concorrente, uma nova captura ou falha pode surgir depois de a leitura terminar; o módulo não fornece uma exclusão de hardware em torno de decisões externas.

## Verificação e integração pendente

Os testes executam o caminho fila→cambota→cames→leitura, incluindo ciclo de 720°, capturas publicadas após leitura de frescura, orçamento parcial, dois eventos futuros sucessivos, expiração, cames isoladas, overflow depois de sincronização, ordem incorreta, relógio e recuperação. Fazem parte de `tools/check_core.py`; AddressSanitizer/UndefinedBehaviorSanitizer também passaram neste ambiente.

Faltam a tarefa STM32 que chama estas APIs, captura por timer/interrupção, prioridades/serialização dos produtores, medição do atraso máximo, dimensionamento da fila sob carga e ensaio com ondas reais. A identidade e a Console continuam sem anunciar aquisição física por causa deste módulo.
