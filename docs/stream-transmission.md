# Transmissão incremental de respostas

`stream_tx.h` mantém uma resposta binária completa num buffer C fixo e acompanha a conclusão do envio por partes. Não chama o controlador USB nem espera ativamente. O chamador fornece um relógio monotónico estendido de 64 bits em milissegundos e confirma o que o driver efetivamente transmitiu.

## Utilização

1. Inicializar `tunuino_stream_tx` com um prazo de 1 a 60000 ms e o instante atual.
2. Obter um frame de resposta válido do dispatcher e entregá-lo a `tunuino_tx_begin`. A função valida comprimento e CRC e copia o frame; o buffer original pode ser reutilizado.
3. Usar `tunuino_tx_view` para obter os bytes ainda pendentes. Entregar ao driver apenas a quantidade que ele pode tratar, mantendo no máximo uma transferência assíncrona em curso.
4. Quando o driver confirmar a conclusão dessa transferência, chamar `tunuino_tx_advance` com a quantidade concluída, uma única vez. Bytes apenas aceites numa fila USB ainda não são bytes concluídos.
5. Repetir enquanto o estado for `PENDING`. Chamar `tunuino_tx_poll` periodicamente, incluindo quando o driver está ocupado e não há progresso.
6. Em `DONE`, o frame foi confirmado por inteiro. O ciclo de pedido/resposta pode libertar o recetor e começar o próximo pedido.

O buffer admite até 4096 bytes de corpo mais 6 de enquadramento. Não há alocação dinâmica. Um `begin` durante envio pendente devolve `BUSY` e preserva a resposta atual. Não existe fila de respostas nem concatenação automática.

## Prazo e falhas

O prazo total começa em `begin`; os avanços parciais, incluindo avanços de zero bytes, não o prolongam. No instante exato de expiração, o módulo recusa continuar. Um objeto inativo ou um envio já concluído não expira. Um recuo do relógio é erro. O cálculo não soma timestamps e funciona perto do limite de 64 bits; o chamador tem de estender contadores STM32 de 32 bits.

Timeout, recuo do relógio, contagem de conclusão superior ao restante e aborto deixam o emissor bloqueado. Não disponibiliza mais bytes nem aceita uma resposta nova até ser reinicializado. Uma desconexão deve chamar `tunuino_tx_abort` e abortar também o recetor. O módulo não cancela operações USB por si.

Antes de reinicializar ou reutilizar o armazenamento, a integração tem de cancelar ou concluir qualquer operação do driver e descartar callbacks e bytes pendentes da ligação anterior. Não pode sobrescrever o buffer enquanto um periférico/DMA ainda o usa. Depois de uma falha a meio de uma resposta, a ligação deve ser encerrada ou explicitamente limpa; uma resposta nova não pode ser anexada à anterior.

Um frame inválido é recusado antes de o envio começar e mantém o estado inativo/concluído. `BUSY`, argumentos inválidos e avanços sem envio pendente não representam novos estados de transmissão. Uma inicialização com prazo inválido deixa o objeto indisponível até ser inicializado corretamente. O objeto deve ser inicializado antes do uso, as chamadas devem ser serializadas e os argumentos não podem sobrepor o objeto.

## Integração e testes

O caminho portátil de receção, dispatcher e transmissão está coberto por teste: um pedido de identidade recebido byte a byte produz uma resposta enviada em partes e novamente validada pelo codec. Os testes incluem frames mínimos/máximos, diversas quantidades por transferência, preservação da cópia, resposta ocupada, CRC, frames truncados, prazo sem progresso, desconexão, contagens incorretas e relógio.

Faltam o driver USB STM32, filas, ligação aos callbacks de conclusão, sincronização com interrupções e ensaio físico em Proteus/uaEFI. O sucesso dos testes portáteis não valida uma ligação USB real nem produz uma imagem gravável.
