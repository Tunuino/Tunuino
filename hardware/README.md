# Integração do hardware

A ferramenta original Tunuino `tools/import_kicad_netlist.py` extrai a pertença dos contactos selecionados às redes de uma netlist XML KiCad D/E. Os dados de cada relatório provêm da fonte identificada no próprio ficheiro. O relatório não é uma definição de pinos para firmware.

## Fontes consultadas

| Família | Repositório e commit | Conteúdo observado | Estado |
|---|---|---|---|
| Proteus | [Tunuino/proteus](https://github.com/Tunuino/proteus/tree/383558bf7ca1c53ad8bbd8fdc9dd3a4187013c46) | Projeto `proteus.kicad_pro`, esquema principal, folhas hierárquicas e PCB; esquema principal declara `v0.7` | Não foi encontrada netlist XML exportada na árvore deste commit; importação pendente |
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
