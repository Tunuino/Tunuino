# Montagem de telemetria com validade e frescura

`telemetry.h` constrói um `tunuino_live_sample` a partir das RPM do processador de capturas e de snapshots de MAP, lambda e temperatura. Não adquire ADC, não calibra sensores e não configura periféricos.

## Fontes e escalas

As RPM vêm exclusivamente de `tunuino_capture_processor_read`. O processador tem de estar sem trabalho pendente/falhas e o descodificador tem de fornecer `crank_valid`. O montador converte RPM para mili-RPM, arredondando ao inteiro mais próximo, com verificação de finitude e de limite uint32. Uma falha de cambota, fila pendente, captura futura, timeout ou erro da fila impede declarar RPM válidas.

O ciclo de 720° não é exigido para registar RPM. `cycle_valid` continua separado e não é convertido em autorização para controlar o motor.

`tunuino_telemetry_sensors` contém valores já calibrados e verificados pela camada fornecedora, timestamps individuais e uma máscara que só pode declarar MAP, lambda e temperatura. Os campos têm as mesmas escalas do protocolo existente:

| Sinal | Campo | Escala |
|---|---|---|
| MAP | `map_milli_kpa` | kPa × 1000, uint32 |
| Lambda | `lambda_million` | lambda × 1000000, uint32 positivo |
| Líquido | `coolant_milli_c` | °C × 1000, int32 |

A máscara dos sensores não pode declarar RPM nem bits desconhecidos. Lambda zero é recusada como sinal válido. Os limites elétricos/físicos, calibração e deteção de falha dos sensores continuam a pertencer à camada fornecedora; o módulo não presume curvas, tensões ou pinos.

## Frescura e resultado

`tunuino_telemetry_policy` exige idades máximas explícitas, individuais, entre 1 e 60000000 µs. Não existem valores de motor predefinidos. Um sinal é fresco se o timestamp não for futuro e `now_us − timestamp < idade_máxima`. No instante exato do limite deixa de ser válido.

Todos os timestamps usam o domínio de captura estendido de 64 bits em microssegundos. O tempo da amostra é `now_us / 1000`, conforme o protocolo. A frescura das RPM usa a configuração do processador de capturas.

A saída é inicialmente limpa. Só são copiados campos válidos/frescos e ativados os seus bits. O retorno é `READY` apenas com os quatro sinais válidos; um conjunto parcial devolve `NOT_READY`. Argumentos, política ou máscara inválidos devolvem `ARGUMENT` com a amostra limpa. A máscara parcial permite diagnóstico no núcleo, mas não altera o formato da resposta USB.

`tunuino_live_reply` continua a emitir o estado de um byte `NOT_READY` para amostras incompletas. Não envia os campos limpos como se fossem medições reais. O cliente Java omite esse estado e o gravador CSV não cria uma linha. O formato e a versão do protocolo não mudaram.

## Integração

A função pertence ao consumidor do processador de capturas e deve ser serializada com drenagem/leitura e atualizações dos snapshots de sensores. A sequência é processar as capturas, obter fontes estáveis, montar a telemetria e publicar uma cópia estável para o dispatcher. O módulo não sincroniza por si uma tarefa USB com uma tarefa de aquisição, nem garante atomicidade de estruturas partilhadas.

Os buffers/argumentos não podem sobrepor o processador ou os dados das fontes. A camada STM32 ainda tem de adquirir e validar os sinais e implementar essa publicação. Este módulo não anuncia capacidades na identidade nem cria firmware gravável.

## Verificação

O teste C liga fila, descodificador, processador e montador. Verifica RPM calculadas numa roda sintética 36−1, arredondamento, escalas, timestamps futuros, expiração individual exata, bits de validade, ausência de fase de cames, trabalho pendente, perda de sinal, overflow, argumentos e resposta `NOT_READY` do protocolo real. Faz parte de `tools/check_core.py`; AddressSanitizer/UndefinedBehaviorSanitizer também passaram neste ambiente.

São dados sintéticos. Faltam os sensores físicos, tarefa STM32, validação das idades sob carga e ensaio no Windows/placas com o transporte real.
