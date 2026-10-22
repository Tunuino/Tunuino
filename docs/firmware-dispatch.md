# Dispatcher do núcleo de comunicação

`tunuino_dispatch_reply` recebe um pacote completo e encaminha-o para identificação, dados em tempo real ou leitura de mapas. Usa os formatos existentes, sem acrescentar comandos de escrita ou alterar as respostas. É código C portátil; ainda não é um driver USB nem firmware STM32 gravável.

## Contexto fornecido pela placa

`tunuino_dispatch_context` referencia uma identidade, um snapshot de sensores e os snapshots VE/ignição. O chamador mantém esse armazenamento válido e estável durante cada chamada, sincronizando também os produtores em interrupções ou DMA. Os buffers de resposta não podem sobrepor o contexto nem os dados dos fornecedores. Pedido e resposta podem partilhar um buffer.

`tunuino_dispatch_validate` verifica a identidade através de `tunuino_identity_validate`, sem presumir família, MCU, revisão ou UID. As capacidades têm de ser declaradas explicitamente pela placa:

| Capacidade | Condição exigida |
|---|---|
| `0x01`, dados em tempo real | Ponteiro de snapshot de sensores configurado |
| `0x02`, leitura de mapas | Pelo menos um snapshot de mapa configurado; tipos correspondentes aos campos VE/ignição |
| Escrita, gravação de firmware e Autotune | Recusadas por este dispatcher, porque ainda não têm handlers |

Configurar um ponteiro não ativa uma capacidade. Um snapshot de sensores sem sinais válidos ou um snapshot de mapa com geração zero permite responder `NOT_READY`, sem inventar valores. Um mapa de um tipo não é entregue em resposta a um pedido de outro tipo. Se apenas um dos mapas estiver configurado, os pedidos válidos do outro recebem `NOT_READY`.

Ter um ponteiro configurado não comprova aquisição física nem integração de hardware. A placa só deve anunciar capacidades depois dessa integração e validação.

## Resultado e transporte

O dispatcher valida o frame, incluindo CRC, e depois o contexto antes de encaminhar o pedido. Um retorno `TUNUINO_DISPATCH_OK` disponibiliza exatamente um frame, cujo comprimento está em `written`. Inclui os estados de protocolo `NOT_READY` e `SNAPSHOT_CHANGED` já definidos pelos handlers.

Erros de argumento, enquadramento, configuração, comando desconhecido, capacidade desativada, pedido inválido, fornecedor inválido ou capacidade insuficiente do buffer devolvem um estado C e deixam `written` a zero. Não é criado um novo formato de erro no fio. O transporte não deve transmitir bytes de um buffer cujo resultado tenha sido um erro; deve aplicar a política de ligação definida pela integração.

`tunuino_stream_rx` já reúne bytes até obter um pacote completo e limita tamanho e prazo de receção; o contrato está em `stream-reception.md`. `tunuino_link` coordena receção, chamada do dispatcher e conclusão do envio, conforme `communication-link.md`. A integração USB ainda tem de fornecer os blocos recebidos, o relógio e as conclusões físicas de transmissão, manter os bytes não consumidos e tratar desconexões. Uma leitura USB não equivale necessariamente a um pacote. `tunuino_stream_tx` já acompanha a transmissão parcial com prazo, conforme `stream-transmission.md`. A versão atual não implementa o driver, filas USB, ligação aos callbacks de conclusão, periféricos STM32, persistência, pinos nem controlo do motor.

## Verificação

`tools/check_core.py` compila e executa os testes do dispatcher juntamente com o núcleo C/Java. Os testes comparam as respostas encaminhadas com os handlers existentes, cobrem ambas as famílias de mapas, sensores indisponíveis, gerações antigas, capacidades sem fornecedor, identidade inválida, troca de tipos, sobreposição de buffers, limites de saída, CRC incorreto, comandos desconhecidos e frames truncados ou com bytes extra. Os dados são sintéticos; não validam USB físico nem pinagem.
