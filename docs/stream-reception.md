# Receção incremental de pacotes

`stream_rx.h` fornece um coletor C de bytes para o enquadramento Tunuino. Não depende de um controlador USB, de alocação dinâmica ou de pinos STM32. A integração da placa tem de fornecer os bytes recebidos e um relógio monotónico de 64 bits em milissegundos.

## Sequência de utilização

1. Com o transporte limpo, inicializar `tunuino_stream_rx` com `tunuino_rx_init`, um prazo de 1 a 60000 ms e o instante atual.
2. Entregar cada bloco recebido a `tunuino_rx_feed`. `consumed` indica quantos bytes desse bloco foram usados.
3. Enquanto o estado for `MORE`, continuar a receção e chamar periodicamente `tunuino_rx_poll`, mesmo quando não chegam bytes.
4. Em `READY`, obter uma vista do pacote completo com `tunuino_rx_view` e chamar o dispatcher com um buffer separado para a resposta.
5. Depois de tratar o pedido e a resposta, chamar `tunuino_rx_release`. Os bytes restantes de um bloco pertencem ao chamador e podem ser entregues para o próximo pacote.
6. Numa desconexão, chamar `tunuino_rx_abort` e descartar também os bytes pendentes no transporte. Uma nova ligação começa com uma inicialização nova e filas limpas.

O coletor consome no máximo um pacote por chamada. Se dois pacotes chegarem juntos, para no fim do primeiro; não perde os bytes do segundo. Enquanto um pacote estiver `READY`, não aceita mais bytes nem altera a vista. A vista só é válida até à libertação, aborto ou reinicialização e não pode ser usada como buffer de resposta modificável.

## Limites e falhas

O buffer fixo admite 4096 bytes de corpo e os 6 bytes de enquadramento. O comprimento é validado assim que chegam os dois bytes iniciais; zero ou mais de 4096 é erro. Só um pacote completo e com CRC válido chega a `READY`.

O prazo total começa no primeiro byte de um pacote. Novos fragmentos não reiniciam o prazo. No instante exato de expiração, bytes novos são recusados. Um recetor vazio pode permanecer inativo sem expirar. Um pacote já completo permanece disponível, pois o prazo deste módulo cobre apenas a receção; o envio da resposta precisa de um prazo próprio.

O relógio tem de ser não decrescente, incluindo nas chamadas de polling e libertação. A integração deve estender um contador STM32 de 32 bits para 64 bits antes de o fornecer ao módulo. Um recuo ou uma volta a zero é erro; o cálculo do prazo não soma timestamps e evita overflow perto de `UINT64_MAX`.

Erros de comprimento, CRC, prazo e relógio ficam retidos: chamadas seguintes não consomem bytes nem disponibilizam um pacote. `release` não recupera uma falha. O módulo não procura um novo cabeçalho em bytes residuais. A integração tem de terminar ou limpar explicitamente o transporte antes de reinicializar, para evitar interpretar o fim de um pacote antigo como pedido novo.

Argumentos inválidos devolvem `ARGUMENT`, com contagem consumida a zero quando esta existe. O objeto tem de ser inicializado antes de ser usado e as suas operações devem ser serializadas. Buffers e parâmetros de saída não podem sobrepor o objeto do coletor. Uma inicialização com prazo inválido deixa o objeto indisponível até ser inicializado corretamente.

## Estado da integração

Os testes exercitam todos os pontos de divisão para os comprimentos selecionados, incluindo o limite de 4096 bytes, receção byte a byte, pacotes juntos, pausa sem bytes, prazo total, CRC, cabeçalhos inválidos, desconexão, relógio e encaminhamento de um pedido fragmentado ao dispatcher real.

O coletor ainda precisa de ser ligado ao USB STM32. O módulo portátil de transmissão parcial com prazo está em `stream-transmission.md`. Faltam o driver, filas de receção, callbacks reais de conclusão de envio, sincronização com interrupções e ensaio físico em Proteus/uaEFI. Esta implementação não configura periféricos nem controla saídas do motor.
