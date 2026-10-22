# Protótipo de descodificação de cambota

`crank.h` implementa um núcleo C original para uma roda uniforme com uma única região contígua de dentes em falta. Recebe um timestamp por dente físico e calcula sincronização relativa à falha e RPM. Não recebe sinais diretamente de um pino nem configura hardware.

Este módulo é experimental e só foi testado com sequências sintéticas. Não está ligado à identidade, ao datalog de sensores reais ou a qualquer saída de injeção/ignição.

## Configuração e relógio

`tunuino_crank_init` exige configuração explícita: número de posições nominais, dentes em falta, RPM mínimo/máximo e prazo de perda de sinal. Não existe roda predefinida nem configuração de motor padrão.

As posições nominais admitidas são 12–360, com 1–4 dentes em falta. Os limites RPM têm de ser finitos, com mínimo ≥1, máximo superior ao mínimo e máximo ≤30000. O menor intervalo nominal tem de ser pelo menos 10 µs, para excluir configurações incompatíveis com a resolução de entrada escolhida. Estes são limites do protótipo, não especificações de funcionamento de uma ECU.

O prazo deve ser positivo, no máximo 60 milhões de microssegundos, e suficientemente longo para o maior intervalo de falha no RPM mínimo, com margem de 20%. É recusado se for inferior a `60e6 / (slots × minimum_rpm) × (missing + 1) × 1.2`.

A integração fornece timestamps monotónicos estendidos de 64 bits em microssegundos, de uma única polaridade de aresta selecionada por dente. Deve serializar arestas, polling e leituras, incluindo os produtores em interrupção. Uma aresta capturada não pode ser entregue com timestamp anterior ao último polling. O contador STM32 de 32 bits deve ser estendido antes de fornecer os timestamps.

## Aquisição de sincronização

1. A primeira aresta fixa a origem temporal. Intervalos normais posteriores estabelecem uma estimativa do período de uma posição nominal.
2. Um intervalo normal tem de respeitar os limites RPM e ficar entre 80% e 120% da estimativa anterior. A estimativa é atualizada por `0.75 × anterior + 0.25 × intervalo`.
3. Depois de pelo menos quatro intervalos normais, um intervalo longo pode ser candidato a falha. A sua razão face ao período nominal tem de estar a ±0,25 de `missing + 1`, com período normalizado dentro dos limites RPM.
4. A primeira falha marca apenas um candidato e o dente seguinte à falha como índice zero. Ainda não são expostas RPM válidas.
5. A segunda falha só confirma sincronização se tiver ocorrido exatamente o número esperado de dentes físicos entre as duas falhas: `slots − missing`. Um número errado impede ou retira sincronização.

A RPM é calculada por `60e6 / (slots × período_nominal_us)`. Nos intervalos normais usa-se a estimativa filtrada. Na falha usa-se o intervalo dividido por `missing + 1`, evitando tratar a falha como redução artificial de RPM.

O índice vai de zero a `slots − missing − 1`. Indica posição relativa à falha ao longo de uma volta. Não identifica PMS, fase de 720°, cilindro, sentido de rotação ou momento de atuação. Faltam o offset angular, sinal de árvore de cames, programação de eventos e integração com o estado do motor.

## Perda e leitura

`tunuino_crank_read` verifica o prazo antes de expor os dados. Só em `SYNCED` devolve `valid=1`, índice e RPM. Nos restantes estados limpa esses campos, impedindo reutilizar uma leitura antiga como válida.

Um intervalo nulo, demasiado curto/longo, uma falha inesperada, falta de falha no ponto previsto ou timeout retiram a sincronização. A aquisição pode recomeçar com evidência nova. O polling deve continuar mesmo sem arestas para detetar perda de sinal; no instante exato do prazo, a leitura passa a inválida.

Um recuo do relógio produz `CLOCK`, retido até nova inicialização. Argumentos/configurações inválidos são recusados. O módulo não tenta inferir automaticamente a roda, corrigir ruído do condicionador elétrico ou comandar um motor durante uma falha.

## Verificação e trabalho pendente

Os testes cobrem rodas sintéticas 12−1, 36−1, 60−2 e 360−4 a cinco velocidades, índices, RPM na falha, confirmação por duas falhas, jitter de 1%, aresta espúria, falha prematura/ausente, contagem incorreta, comprimento de falha vizinho, prazo sem sinais, recuperação e relógio perto do limite de 64 bits. São executados por `tools/check_core.py`; as verificações de memória/undefined behavior também passaram neste ambiente.

Falta validar ondas reais durante arranque, compressão, aceleração e ruído, medir o custo computacional no STM32 escolhido e ligar captura temporizada e condicionamento dos sinais. As tolerâncias fixas atuais podem perder sincronização com variação real de velocidade; não foram aprovadas para funcionamento no motor. A configuração Proteus continua dependente da confirmação da variante MCU e revisão física descrita em `hardware/README.md`.
