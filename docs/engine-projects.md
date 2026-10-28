# Projeto completo Proteus v0.7 F427

`.tnproj` contém as quatro páginas de configuração, com as duas tabelas nativas 16×16 completas. Não contém firmware, drivers, valores de sensores ao vivo nem chaves de ativação. Abrir um ficheiro não envia comandos à ECU. Não altera a disposição de flash nem o protocolo; usa os pedidos de página/Burn já implementados.

## Formato 1

Inteiros big-endian. Ficheiro de exatamente 2868 bytes:

| Posição | Conteúdo |
|---|---|
| 0–7 | ASCII `TUNUINOP` |
| 8 | Formato 1 |
| 9/10/11 | Alvo: Proteus 1, revisão 7, família MCU F4 1 |
| 12–267 | Página principal, 256 bytes |
| 268–523 | Auxiliares/funções, 256 bytes |
| 524/526 | Comprimentos U16 dos ficheiros VE e ignição, ambos 1168 |
| 528–1695 | Mapa VE completo no formato `.tnmap`, com o seu CRC |
| 1696–2863 | Mapa de ignição completo `.tnmap`, com o seu CRC |
| 2864–2867 | CRC32 de todos os bytes anteriores |

O leitor limita o tamanho antes de analisar. Recusa truncamento, dados extra, alvo/formato desconhecido, CRC errado, tabelas trocadas, dimensões diferentes de 16×16, valores sem representação nativa exata e dependências inválidas entre sensores, válvulas, perfil e proteções. O UID da ECU de origem não integra o ficheiro para permitir utilizar um projeto noutro equipamento do mesmo alvo; a operação de restauração fica vinculada à identidade/UID da sessão e à leitura atual.

## Leitura e restauração

A leitura exige ausência de alterações pendentes. Lê perfil/sensores, auxiliares, os dois mapas e estado de saídas; confirma identidade, validação e geração coerentes. Nunca descarta alterações pendentes de outro editor para obter uma leitura.

Restaurar compara o snapshot completo atual com o esperado e exige saídas desarmadas. Cria uma nova cópia `.tnproj` do estado anterior com CREATE_NEW antes de preparar qualquer página. Falha de cópia impede a alteração da ECU, incluindo quando o destino já existe. Mesmo quando os dados já coincidem, guarda a cópia anterior pedida sem gerar uma gravação desnecessária.

As quatro páginas são preparadas por blocos de 64 bytes, validadas em conjunto e relidas pelo protocolo de páginas, que nesta fase representa os valores pendentes. Só depois é emitido um único Burn. A configuração ativa muda uma única geração e usa o mecanismo de persistência atómica existente. Uma nova leitura completa tem de coincidir com o projeto e indicar persistência confirmada, sem pendentes nem saídas armadas.

Uma falha anterior ao pedido de Burn tenta descartar apenas a preparação própria, dado que alterações pendentes anteriores foram recusadas no início. Depois de pedir Burn, não descarta nem repete automaticamente: indica a fase e exige nova leitura. A cópia anterior permanece disponível. A política autónoma faz parte do projeto e é mostrada na confirmação; só tem efeito de ativação automática no próximo arranque elétrico, como nas restantes formas de guardar.

Os testes integram os clientes Java, dispatcher C, adaptador de saídas e persistência de teste: as quatro páginas mudam uma só geração; cópia existente, snapshot antigo, valores pendentes e saídas armadas são recusados; falha de gravação preserva estado ativo e cópia e não provoca repetição automática. Não representam ensaio físico numa ECU.
