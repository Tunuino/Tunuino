# Fila de capturas de cambota e cames

`capture_queue.h` contém uma fila C11 fixa de 64 eventos para separar a produção de timestamps do processamento dos sinais. Um evento identifica cambota ou cames e um instante estendido de 64 bits em microssegundos. A fila não configura pinos, interrupções ou timers STM32.

## Concorrência e memória

O contrato exige exatamente um produtor serializado e um consumidor. O produtor pode ser uma rotina de captura e o consumidor uma tarefa de processamento. Duas interrupções que chamem `push` não podem aninhar-se nem executar como produtores independentes: a integração tem de serializá-las. O módulo não fornece essa exclusão entre produtores.

A inicialização deve ocorrer com produtores, consumidor e hardware parados. Também é essa a única condição permitida para reinicializar após falha. Os campos do objeto não devem ser modificados diretamente nem sobrepostos com buffers de entrada/saída.

Os índices e o estado usam `atomic_uint` C11, com publicação `release` e leitura `acquire`. A compilação exige `ATOMIC_INT_LOCK_FREE == 2`, para não introduzir bloqueios de biblioteca no caminho de captura. Isso ainda tem de ser verificado no compilador/MCU efetivamente usado. O armazenamento deve estar em RAM normal acessível pelo CPU; não é um contrato de transferência DMA.

As 64 posições podem ser usadas; não há posição reservada para distinguir cheio de vazio. Os contadores usam aritmética unsigned modular e a capacidade é uma potência de dois. A escrita de um evento termina antes de publicar o índice; a posição só pode ser reutilizada depois de o consumidor concluir a cópia.

## Ordem e falhas

`tunuino_capture_push` não espera por espaço. Uma fila cheia produz `OVERFLOW`. Timestamp anterior ao último aceite ou uma aresta de cambota depois de cames no mesmo timestamp produz `ORDER`. Nos instantes iguais, a ordem é cambota antes de cames, para que o descodificador use o dente correto ao avaliar a janela de cames.

Um tipo de evento inválido produz `ARGUMENT` e retém a falha. A fila não filtra impulsos duplicados do mesmo tipo: esses eventos continuam a ser avaliados pelo descodificador.

Depois de uma falha retida, o produtor não aceita mais eventos e o consumidor não entrega capturas já pendentes. Não são descartados dentes antigos para abrir espaço e continuar uma sequência aparentemente válida. O chamador deve invalidar o estado do descodificador e tratar a falha antes de voltar a adquirir sincronização. A fila não cancela por si saídas ou operações externas.

`tunuino_capture_take` devolve `EMPTY` quando não há evento. Em vazio ou erro limpa o resultado. `tunuino_capture_state` permite consultar a falha retida. `tunuino_capture_count` é apenas um retrato limitado da ocupação, que pode sobrestimar sob concorrência; devolve zero numa falha e, por isso, não substitui a consulta do estado.

## Ligação ao descodificador

O consumidor entrega cada evento, por ordem, a `tunuino_position_crank_edge` ou `tunuino_position_cam_edge`, usando o timestamp capturado. Não deve substituir os timestamps pela hora da tarefa.

A integração ainda tem de resolver a ordenação entre capturas pendentes e polling. Antes de avançar o relógio do descodificador para um instante de polling, deve garantir que nenhuma captura anterior fica pendente ou pode ser publicada depois desse avanço. Isso exige coordenação com o produtor/captura física; observar a fila vazia numa única leitura não fornece essa garantia. Sem essa coordenação, o descodificador recusa posteriormente o timestamp antigo e perde sincronização.

O teste de integração desta etapa entrega sinais sintéticos da fila ao descodificador real e confirma o ciclo de 720°. Não implementa ainda a tarefa de drenagem/polling concorrente nem o driver STM32.

## Verificação

`tools/check_core.py` verifica capacidade completa, volta dos contadores unsigned ao zero, reutilização das posições, retenção de overflow/ordem, timestamps iguais, relógio de 64 bits e o percurso fila→cambota/cames.

Neste ambiente, um teste com threads C11 transferiu 200000 eventos entre produtor e consumidor concorrentes e verificou cada timestamp/tipo. O produtor desse teste espera por espaço; a rotina `push` continua a falhar imediatamente se a fila encher. O teste com threads é omitido no Windows ou quando o compilador declara ausência de threads C11; os restantes testes continuam incluídos. As verificações AddressSanitizer/UndefinedBehaviorSanitizer também passaram neste ambiente.

Faltam captura física, prioridades de interrupção, medição da carga e latência, tratamento integrado de falha e validação no MCU/revisão selecionados. Esta fila não altera a identidade nem anuncia sensores físicos na Console.
