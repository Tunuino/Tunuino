# Ciclo de comunicação do firmware

`tunuino_link` reúne o recetor incremental, o dispatcher e o emissor num ciclo C sem alocação dinâmica. Trata um pedido e uma resposta de cada vez. É uma camada portátil para integrar no transporte da placa, ainda sem driver USB STM32.

## Estados e chamadas

| Estado | Comportamento |
|---|---|
| `RECEIVING` | Aceita fragmentos de um pedido e verifica o prazo de receção |
| `RESPONDING` | Disponibiliza a resposta e acompanha as conclusões parciais; não consome outro pedido |
| Erro RX, dispatcher ou TX | Bloqueia ambas as direções; não fornece mais dados |
| `CLOSED` | Ligação abortada; exige nova inicialização com transporte limpo |

Inicializar com `tunuino_link_init`, um contexto de fornecedores válido, prazos RX/TX separados entre 1 e 60000 ms e relógio monotónico de 64 bits em milissegundos. O contexto é copiado, mas os objetos referenciados continuam a pertencer à placa e têm de permanecer válidos. A inicialização recusa capacidades sem implementação ou sem fornecedor.

`tunuino_link_feed` reúne no máximo um pedido. Quando está completo, chama o dispatcher e copia a resposta validada para o emissor. `consumed` indica os bytes usados. O chamador conserva quaisquer bytes restantes do bloco recebido; enquanto o estado for `RESPONDING`, a contagem consumida é zero. Este mecanismo não cria filas nem autoriza descartar bytes que chegaram juntos.

Usar `tunuino_link_tx_view` para obter os bytes pendentes e `tunuino_link_tx_advance` apenas depois de o driver confirmar a conclusão real da parte transmitida. Deve existir no máximo uma transferência assíncrona pendente. Enfileirar bytes no driver não equivale a transmiti-los. Só após a confirmação de toda a resposta o módulo liberta o recetor e regressa a `RECEIVING`.

Chamar `tunuino_link_poll` periodicamente, mesmo sem tráfego ou callbacks, para detetar prazos expirados. O prazo RX começa no primeiro byte; o prazo TX começa ao preparar a resposta. Não há repetição automática nem reinício de prazos por progresso parcial. As regras de relógio, buffers e conclusão são as de `stream-reception.md` e `stream-transmission.md`.

## Falhas e armazenamento

Uma falha RX, TX ou de dispatcher aborta ambos os módulos e permanece retida. Nenhuma resposta parcial nova é exposta. `state`, `rx_status`, `dispatch_status` e `tx_status` são diagnósticos de leitura; não devem ser modificados pelo integrador. Os três últimos indicam os resultados mais recentes de cada componente, e o estado principal identifica qual causou a falha.

Um argumento inválido numa chamada devolve `ARGUMENT` sem substituir um ciclo ativo válido. Um erro de inicialização deixa o objeto indisponível. Um comando desconhecido, desativado ou inválido é uma falha do dispatcher e encerra o ciclo lógico, sem inventar uma resposta de erro no fio.

`tunuino_link_abort` trata a desconexão lógica. O integrador continua responsável por parar ou cancelar o driver, descartar filas e callbacks antigos e garantir que nenhum DMA/periférico usa os buffers antes de reinicializar. A reinicialização não é uma tentativa de recuperar bytes de uma ligação antiga.

O objeto contém os buffers fixos RX e TX. Durante o despacho há um buffer temporário de `TUNUINO_DISPATCH_RESPONSE_MAX` bytes (atualmente 274), suficiente para as respostas implementadas. Este limite tem de ser atualizado se forem acrescentados handlers maiores. Não são colocadas cópias de mapas completos na stack. O módulo não é reentrante: chamadas e atualizações dos fornecedores devem ser sincronizadas, incluindo interrupções.

## Verificação e trabalho pendente

Os testes exercitam identidade Proteus/uaEFI, dados válidos/indisponíveis, transferência integral dos mapas VE e ignição de 32×32, pedidos juntos, receção byte a byte, envio em partes, alterações de fornecedor após preparar uma resposta, CRC inválido, prazos, recuo do relógio, contagem TX incorreta, configuração inválida e desconexão em ambas as fases.

Faltam o driver USB, filas limitadas, callbacks físicos, sincronização STM32 e ensaio nas placas. O ciclo portátil não adquire sensores, não define pinos e não produz firmware gravável.
