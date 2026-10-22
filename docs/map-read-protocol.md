# Leitura de mapas Tunuino

Protocolo original de leitura; não escreve calibração, não ativa valores no motor e não declara compatibilidade TunerStudio. Requer a capacidade `0x02` na identidade. A integração STM32 ainda não existe e a identidade padrão dos testes não anuncia esta capacidade.

Cada pedido/resposta usa o enquadramento de `frame.h`. Inteiros multibyte são big-endian. O prefixo dos pedidos é ASCII `TUNUINO:M` (9 bytes). Tipo de mapa: `1` VE, `2` ignição. O ficheiro transferido segue `calibration-format.md`.

## Metadados

Pedido: 11 bytes.

| Posição | Conteúdo |
|---|---|
| 0–8 | Prefixo |
| 9 | Operação `0` |
| 10 | Tipo de mapa |

Resposta válida: 14 bytes.

| Posição | Conteúdo |
|---|---|
| 0 | Estado `0` |
| 1 | Versão `1` |
| 2 | Operação `0` |
| 3 | Tipo |
| 4–7 | Geração uint32 não nula |
| 8–9 | Comprimento uint16 do ficheiro, 48–4368 bytes |
| 10–13 | CRC32 do ficheiro completo, incluindo o seu CRC interno |

## Blocos

Pedido: 19 bytes.

| Posição | Conteúdo |
|---|---|
| 0–8 | Prefixo |
| 9 | Operação `1` |
| 10 | Tipo |
| 11–14 | Geração recebida nos metadados |
| 15–16 | Posição uint16 no ficheiro |
| 17–18 | Quantidade uint16, 1–256 bytes |

Resposta válida: 12 bytes de cabeçalho e a quantidade pedida.

| Posição | Conteúdo |
|---|---|
| 0 | Estado `0` |
| 1 | Versão `1` |
| 2 | Operação `1` |
| 3 | Tipo |
| 4–7 | Geração |
| 8–9 | Posição |
| 10–11 | Quantidade |
| 12… | Bytes do ficheiro |

O último bloco pode ser menor que 256 bytes. Posição ou quantidade fora do ficheiro são pedidos inválidos. O handler C devolve erro ao chamador e não produz uma resposta nesses casos, nem para frames ou comandos inválidos.

Em ambas as operações, uma resposta de um só byte `1` significa mapa indisponível. Uma resposta de um só byte `2` significa que a geração pedida já não é atual. Estes estados não contêm dados parciais.

## Consistência e integração

`tunuino_map_snapshot_prepare` valida e codifica um mapa fornecido pelo chamador em armazenamento próprio. Uma falha invalida o snapshot. O integrador tem de serializar preparação e resposta, manter o snapshot estável durante cada resposta e usar uma geração não nula diferente em cada substituição. Não pode reutilizar gerações enquanto houver uma ligação ativa; antes do esgotamento do contador, deve encerrar as ligações. Pedido e resposta não alteram o mapa ativo.

O handler serve um snapshot de um tipo. O futuro dispatcher da placa deve selecionar o snapshot correspondente ao tipo pedido. Só deve anunciar a capacidade quando o dispatcher, os snapshots reais e o transporte estiverem integrados. Não existem valores de motor predefinidos neste módulo.

`MapReadClient` verifica a capacidade antes de comunicar. Usa um prazo total para metadados e todos os blocos, aloca no máximo 4368 bytes e valida geração, posição, quantidade, CRC de transferência, formato `.tnmap`, CRC interno e tipo final. Nenhum mapa é entregue parcialmente. Uma falha encerra a sessão; não há repetição automática. A sessão serializa cada pedido; snapshots não dependem de um cursor global e pedidos intercalados não alteram a posição de leitura.

Na Console, a leitura decorre na thread USB e bloqueia as outras ações USB. O mapa validado abre no editor como cópia offline, que pode ser guardada num ficheiro novo. Editar essa cópia não envia dados à ECU.

Os testes transferem mapas matemáticos sintéticos de 32×32 em ambos os tipos, incluindo ficheiros maiores que um frame, respostas C consumidas por Java, verificação independente em Python, fragmentação de transporte, alterações de geração, limites, interrupções, corrupção e prazo total. Não constituem validação em hardware.
