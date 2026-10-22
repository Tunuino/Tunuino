# Integração do hardware

A ferramenta original Tunuino `tools/import_kicad_netlist.py` extrai a pertença dos contactos selecionados às redes de uma netlist XML KiCad D/E. Os dados de cada relatório provêm da fonte identificada no próprio ficheiro. O relatório não é uma definição de pinos para firmware.

## Fontes consultadas

| Família | Repositório e commit | Conteúdo observado | Estado |
|---|---|---|---|
| Proteus | [Tunuino/proteus](https://github.com/Tunuino/proteus/tree/383558bf7ca1c53ad8bbd8fdc9dd3a4187013c46) | Projeto `proteus.kicad_pro`, esquema principal, folhas hierárquicas e PCB; esquema principal declara `v0.7` | Netlist XML pendente; atribuições USB do PCB extraídas, sem revisão declarada no PCB |
| uaEFI | [Tunuino/uaefi](https://github.com/Tunuino/uaefi/tree/fefb25a22f8b1968a828ce377363717c529c712f) | Projeto, esquemas, PCB e `gerber/uaefi.net`; a folha raiz da netlist declara `F3` | Relatório USB extraído, ainda não revisto eletricamente |

Os commits fixam as fontes consultadas. Não identificam por si a revisão de uma placa física. A netlist exportada pode estar desatualizada face ao esquema ou PCB do mesmo repositório; essa correspondência ainda tem de ser verificada. O projeto Proteus tem várias folhas e não pode ser mapeado apenas por nomes de etiquetas no esquema principal.

## Relatório uaEFI F3

`review/uaefi-f3-usb.json` foi produzido a partir da netlist completa, selecionando J8, J9 e o alvo M6. Contém 23 contactos listados, nove redes distintas e os nós dessas redes. A fonte completa tem 170 componentes, 208 redes e 669 nós. Os hashes SHA-256 e Git blob SHA-1 permitem verificar o ficheiro utilizado; o blob foi comparado com a árvore do commit indicado.

M6 é um módulo, não o encapsulamento STM32. A netlist relaciona J8.2 com M6.N2 e J8.3 com M6.N3. As funções dos pinos incluem etiquetas com nomes STM32, mas o importador conserva-as apenas como texto da fonte. A atribuição ao MCU precisa do esquema do módulo, da variante realmente montada e de revisão elétrica. Nenhuma saída é ativada por este relatório.

O relatório mantém `validation: unreviewed` e `firmware_pin_definitions_generated: false`. O estado não pode ser promovido automaticamente pelo importador.

## Reproduzir a extração

Obter a versão exata de `gerber/uaefi.net` do commit acima num diretório de trabalho separado e executar, a partir da raiz Tunuino:

```sh
python3 tools/import_kicad_netlist.py /caminho/uaefi.net \
  --family uaefi --revision F3 --repository Tunuino/uaefi \
  --commit fefb25a22f8b1968a828ce377363717c529c712f \
  --source-path gerber/uaefi.net \
  --connector J8 --connector J9 --target M6 \
  --output uaefi-usb-review.json
```

Usar um nome novo para a saída. Ficheiros existentes são preservados. A revisão é comparada com a folha raiz da netlist; a família, repositório e commit são metadados fornecidos pelo chamador, não verificados remotamente pela ferramenta. Ao extrair de outra revisão, usar os metadados dessa fonte e rever o resultado.

Para Proteus, ainda é necessário exportar uma netlist XML da hierarquia completa e confirmar a revisão, bibliotecas e correspondência com o PCB. Não existe `kicad-cli` no ambiente usado nesta etapa, por isso essa exportação não foi executada.

## Regras e limites

- São aceites até 16 MiB de XML UTF-8, 20000 componentes, 50000 redes e 200000 nós. DTD, entidades declaradas, XML inválido e versões desconhecidas são recusados.
- Referências repetidas, redes duplicadas, nós com componentes desconhecidos e o mesmo contacto físico repetido são erros; não se escolhe silenciosamente uma alternativa.
- Conectores e alvos são selecionados explicitamente. As redes aparecem uma só vez e os contactos apontam para os seus nomes. Não há identificação automática de conectores, MCU ou função elétrica.
- A ferramenta só identifica nós na mesma rede. Não atravessa resistências, filtros, transístores ou drivers para presumir ligações ao MCU.
- Só aparecem contactos listados nas redes exportadas. Um contacto ausente não é declarado livre nem sem ligação. Redes de um só nó e nomes de redes sem ligação são preservados.
- Não são copiados os esquemas, bibliotecas, logótipos ou metadados editoriais para o firmware. Os valores e footprints dos componentes selecionados e as etiquetas dos nós continuam identificados como dados da fonte.

`python3 tools/check_core.py` inclui os testes do importador: pertença exata, recusa de ambiguidades, revisão, metadados, XML inválido e preservação de ficheiros. Os testes sintéticos não substituem revisão dos circuitos reais.

Continuam pendentes o circuito intermédio completo, a validação de variantes MCU/PCB, geração de definições de pinos e integração STM32.

## Extração PCB Proteus

`tools/import_kicad_pcb.py` lê o formato PCB legado `20171130`, observado no ficheiro `proteus.kicad_pcb` do commit Proteus indicado acima. Usa um leitor de S-expressions próprio e limitado, sem executar o conteúdo do ficheiro. Outros formatos são recusados até terem suporte explícito.

`review/proteus-pcb-usb.json` seleciona J1501, J1504 e U1501. Contém dez contactos numerados distintos e cinco redes. O PCB identifica U1501 como `STM32F427ZGTx`, encapsulamento LQFP-144. A atribuição de rede associa `/USB_D-` ao pad 103 e `/USB_D+` ao pad 104 desse componente. São números de pads do PCB; não são uma configuração GPIO validada nem prova do componente montado.

O ficheiro tem 1052 footprints, dos quais 355 elétricos com referência e 694 elétricos sem referência. Os pads numerados são agrupados por contacto dentro de cada footprint. Vários pads físicos com o mesmo número e a mesma rede, como a blindagem J1501.5, mantêm a contagem `pad_instances`. O mesmo número em redes diferentes é recusado. Os endpoints sem referência são contabilizados por rede em `anonymous_endpoint_count`, sem lhes inventar referências de componentes; `named_nodes` contém apenas os nós com referência.

O PCB não declara revisão no bloco de título: o relatório mantém `revision: null` e `revision_status: not_declared_in_pcb`. Não recebe a revisão `v0.7` do esquema. A origem é fixada pelo commit e pelo blob `6ad405f5072b949968824d5d1161cfbc3664f24d`, verificado contra o ficheiro obtido. Ainda é preciso comparar PCB, esquema hierárquico e placa física.

Para reproduzir, obter o PCB exato num diretório separado e usar um nome de saída novo:

```sh
python3 tools/import_kicad_pcb.py /caminho/proteus.kicad_pcb \
  --family proteus --repository Tunuino/proteus \
  --commit 383558bf7ca1c53ad8bbd8fdc9dd3a4187013c46 \
  --source-path proteus.kicad_pcb \
  --connector J1501 --connector J1504 --target U1501 \
  --output proteus-pcb-usb-review.json
```

A ferramenta lê atribuições de redes dos pads, sem calcular continuidade das pistas, vias ou zonas e sem comparar com uma netlist esquemática. Um pad numerado sem rede atribuída aparece com `net: null`; isso não autoriza tratá-lo como pino livre. A netlist XML Proteus continua pendente.

O leitor limita o ficheiro a 16 MiB, 1,5 milhões de tokens e 64 níveis de listas. O importador limita footprints, redes e endpoints, valida referências e correspondência entre código/nome de rede e recusa ambiguidades elétricas. A criação da saída é exclusiva. O resultado permanece `unreviewed` e nunca gera definições de firmware. Os testes sintéticos do PCB fazem parte de `tools/check_core.py`.
