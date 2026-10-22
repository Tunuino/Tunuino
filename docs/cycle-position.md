# Protótipo de identificação do ciclo de 720°

`position.h` reúne o descodificador de cambota com um modelo de árvore de cames de um impulso selecionado por ciclo de duas voltas. O módulo é C portátil e experimental, testado apenas com timestamps sintéticos. Não configura GPIO/timers, não adquire sensores e não programa injeção ou ignição.

## Configuração explícita

`tunuino_position_config` contém a configuração `tunuino_crank_config`, os índices inicial/final da janela do impulso de cames e a fase de volta desse impulso (`0` ou `1`). Não existe seleção automática de roda, polaridade ou padrão de cames.

A janela é inclusiva, não atravessa o índice zero e tem de caber nos dentes físicos: `0 ≤ first ≤ last < slots − missing`. O índice usado é o do último dente de cambota aceite, sem interpolação do instante entre dentes. A janela deve ser determinada e validada para o padrão físico; uma janela larga reduz a precisão da deteção de eventos fora de posição.

`cam_revolution_phase` atribui ao impulso confirmado o rótulo configurado da volta. Os rótulos 0/1 não identificam por si admissão, compressão, PMS ou cilindro. Faltam a referência angular, o offset do motor e a relação com a ordem de ignição.

## Confirmação e alternância

Depois de a cambota confirmar duas falhas com contagem correta, o estado passa a `CRANK_ONLY`. RPM e índice podem estar válidos; o ciclo de 720° continua inválido.

O primeiro impulso de cames dentro da janela marca apenas um candidato. O segundo confirma o ciclo se chegar dentro da janela depois de exatamente duas passagens pela falha da cambota. Ainda é preciso observar esse segundo impulso; a passagem do tempo ou polling repetido não confirma a fase.

Após confirmação, cada passagem pela falha alterna `revolution_phase`. Um impulso válido ao fim de duas voltas renova a confirmação. O contador de voltas é limitado: quando o prazo angular para o impulso termina, o candidato é descartado, sem acumular um contador global de voltas.

## Validade e perdas

`tunuino_position_read` verifica o estado da cambota e o prazo de perda de sinal antes de devolver dados. O chamador tem de respeitar os dois indicadores:

| Indicador | Significado |
|---|---|
| `crank_valid` | RPM e índice da cambota válidos segundo o protótipo |
| `cycle_valid` | Impulsos de cames confirmaram a distinção entre as duas voltas |

Se apenas os cames perderem validade, RPM/índice podem continuar disponíveis e a fase de volta é limpa. Isso não constitui uma estratégia de funcionamento do motor sem cames; é apenas o estado dos dados.

Um impulso fora da janela, duplicado ou depois de uma só volta invalida o ciclo e produz `CAM_LOST`. Se o impulso esperado não chegar, a fase é retirada ao passar o fim da janela da segunda volta. Se a janela terminar no último dente físico, a perda é detetada na passagem pela falha seguinte. Recuperar exige novamente dois impulsos coerentes.

Perda de sincronização ou timeout da cambota limpam também a fase de cames, impedindo reutilizá-la depois de a cambota recuperar. Recuo do relógio produz `CLOCK`, retido até nova inicialização. Leituras sem cambota válida têm todos os campos a zero.

## Integração e limites

As chamadas de cambota, cames, polling e leitura devem ser serializadas e usar o mesmo relógio monotónico estendido de 64 bits em microssegundos. Uma captura mais antiga que o último instante processado é recusada. Em timestamps iguais, a integração deve processar a aresta de cambota antes da de cames para que a janela use o dente acabado de receber. O polling deve continuar sem sinais.

O objeto contém o seu descodificador de cambota. As arestas devem passar por `tunuino_position_crank_edge`; alterar o objeto interno ou alimentar um descodificador paralelo não atualiza a contagem de voltas deste módulo. O armazenamento é do chamador, sem alocação dinâmica.

Os testes verificam 36−1/60−2, ambos os rótulos de fase, dez ciclos consecutivos por combinação, perdas de cames, ruído/duplicação, impulso por volta, janela errada, perda e recuperação da cambota, timeout, relógio, janelas nos extremos e polling repetido no índice zero. Fazem parte de `tools/check_core.py`; as verificações de memória e undefined behavior também passaram.

Continuam pendentes ondas reais de arranque/aceleração, condicionamento elétrico, padrões de cames adicionais, tolerâncias validadas, custo computacional no MCU escolhido e ligação à captura STM32. A identidade e o protocolo de dados em tempo real não foram alterados para anunciar aquisição física ou atuação.
