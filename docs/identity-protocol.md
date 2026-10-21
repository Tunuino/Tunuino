# Identificação Tunuino, versão 1

A identificação é um pedido de leitura explícito. A abertura de uma porta não envia este pedido automaticamente. Este protocolo requer firmware Tunuino que implemente o respetivo handler; não identifica firmware diferente apenas por estar instalado numa Proteus ou uaEFI.

O corpo do pedido contém os oito bytes ASCII `TUNUINO?`, enquadrados pelo codec binário comum. O corpo da resposta tem exatamente 32 bytes, também enquadrados com comprimento e CRC32.

| Offset | Tamanho | Campo |
| --- | --- | --- |
| 0 | 1 | Estado: zero para identificação válida |
| 1 | 7 | ASCII `TUNUINO` |
| 8 | 1 | Versão do protocolo: 1 |
| 9 | 1 | Família: 1 Proteus, 2 uaEFI |
| 10 | 1 | Série MCU: 1 STM32F4, 2 STM32F7, 3 STM32H7 |
| 11 | 2 | Identificador da revisão de hardware, de 1 a 65535 |
| 13 | 3 | Versão de firmware: major, minor, patch |
| 16 | 4 | Capacidades do firmware |
| 20 | 12 | UID do microcontrolador |

Os campos de vários bytes usam ordem de rede. UID todo a zero ou todo a `FF`, revisão zero, família/série desconhecida e bits de capacidades reservados são inválidos. O handler não produz uma resposta normal quando a configuração da placa está incompleta. As identidades sintéticas dos testes não são configurações de hardware de produção.

| Bit de capacidade | Significado reservado |
| --- | --- |
| 0 | Leitura de dados em tempo real |
| 1 | Leitura de calibração |
| 2 | Escrita de calibração |
| 3 | Atualização de firmware |
| 4 | Autotune |

Estes bits só podem ser anunciados quando os handlers correspondentes estiverem implementados. A implementação atual de teste anuncia zero capacidades. Os restantes bits ficam reservados.

A camada específica da placa deve fornecer um snapshot estável da identidade: revisão auditada, série MCU efetiva, UID lido do dispositivo e versão da compilação. A série MCU não identifica o modelo exato do chip. A combinação família/série não autoriza gravação de uma imagem nem valida um mapa de pinos.

A Console compara a família recebida com a seleção do utilizador. Respostas incompatíveis ou malformadas encerram a sessão. A identidade é declarada pelo firmware e não constitui autenticação criptográfica nem validação independente dos circuitos da placa.
