# Contrato do controlo experimental de saídas

Alvo: Proteus v0.7 STM32F427. [Pinos e origem do PCB](../hardware/review/proteus-v07-outputs.json). O nível inativo dos 28 sinais MCU é zero. LS1–LS16 entram nos drivers VNLD5160; IGN1–IGN12 entram nos buffers não inversores MIC4427 e exigem uma etapa de potência externa apropriada. Não são saídas de potência para ligar o primário de uma bobina diretamente.

O arranque configura os latches a zero antes de selecionar GPIO de saída. A configuração de aquisição e os mapas conservam o protocolo IO 0.2.0 e o esquema de flash 2. O perfil de motor é separado, volátil e não pertence ao backup `.tncfg`. Não há armação automática no arranque, na identificação, no carregamento do perfil ou na recuperação USB.

## Perfil e cálculo

O perfil explicita 1–12 cilindros, saída LS e IGN distintas para cada cilindro, ângulo PMS de cada cilindro no ciclo 720°, ângulo do dente zero, janela e volta de referência do sensor de fase, ReqFuel de referência, dwell, tempo morto, pulso máximo, fim de injeção e canais opcionais de bomba, ventoinha e relé. As auxiliares não podem partilhar uma saída com um injetor ou entre si. Este primeiro perfil suporta injeção sequencial e ignição sequencial com carga a nível alto. Não implementa bobinas com entrada de carga invertida, injetores peak-and-hold, injeção em bancada ou wasted-spark.

`pulso_us = round(ReqFuel_us × VE_centi × MAP_milli_kPa / 1 000 000 000) + tempo_morto_us`.

ReqFuel refere-se a VE 100% e MAP 100 kPa. O perfil exige calibração real; não calcula automaticamente ReqFuel a partir de cilindrada, caudal ou combustível. Não há compensação IAT/barométrica, curva de tempo morto por tensão, enriquecimento de arranque/aquecimento, aceleração, lambda em malha fechada, knock, controlo de boost ou borboleta eletrónica. O uso da primeira coluna RPM abaixo do eixo permite cálculo a baixa rotação; não se extrapola acima do eixo RPM nem fora dos eixos MAP. Um pulso acima do máximo configurado ou 80% do ciclo provoca corte, em vez de truncar silenciosamente a quantidade de combustível.

## Temporização e bloqueios

O contador TIM2 a 1 MHz fornece a referência dos dentes. O consumidor de captura só expõe o timestamp com posição quando a fila está drenada, íntegra e fresca. O agendador usa a posição 720°, ângulos PMS e RPM medidos para preparar pares de flancos com até 3 ms de antecedência. Cada canal/ciclo só pode ser agendado uma vez. O GPT5 a 1 MHz executa os flancos e o prazo de supervisão em interrupção, separado da leitura USB. Dwell máximo 5 ms; injeção máxima 20 ms. O cálculo do modelo não mede a precisão dos flancos físicos sob carga e aceleração.

Perda de fase depois de entrar em funcionamento, sinais inválidos, referência incoerente, RPM no limite, alteração de configuração, pulsewidth inválido, flanco atrasado mais de 100 µs ou ausência de serviço do motor por 20 ms desativam todos os canais e cancelam pulsos pendentes. O limite de idade do último dente considera a duração da falha da roda. A gravação da flash é recusada enquanto o agendador está armado. Uma falha não permite retomar automaticamente: exige desarmar e armar explicitamente.

A Console consulta o estado das saídas a cada 750 ms quando armado. A ausência destas consultas durante 2 segundos provoca corte. Neste perfil experimental, a monitorização USB/CSV separada fica bloqueada durante a armação para manter disponível a consulta de estado e a ordem de paragem no mesmo proprietário da porta. O perfil não constitui ainda funcionamento autónomo da ECU sem Console.

O prazo de 20 ms é supervisionado por interrupção de temporizador; **não é um watchdog independente de bloqueios de CPU/interrupções nem substitui uma proteção externa de dwell**. Antes de qualquer ligação a um motor faltam ensaios de arranque/reset, níveis elétricos, polaridade, sincronismo/PMS, precisão/jitter sob carga, aceleração e falhas de alimentação/interrupções.

## Protocolo binário nativo

Pedidos CRC habituais, corpo `TUNUINO:X` seguido do opcode. Inteiros em big-endian. Recusa `0x84`; firmware sem suporte devolve `0x83` e a Console conserva a ligação.

| Opcode | Pedido após opcode | Resposta |
|---|---|---|
| 0 | vazio | 24 bytes de estado; renova o prazo de comunicação |
| 1 | perfil de 128 bytes | status; exige saídas desarmadas e ausência de alterações de calibração pendentes |
| 2 | `ARM!` + geração U32 | status; exige perfil atual, timer disponível, aquisição ativa e calibração persistida |
| 3 | vazio | status; cancela todos os pulsos e níveis auxiliares |
| 4 | canal U8, duração U16, `TEST` | status; um pulso de bancada com motor desarmado e nenhuma rotação válida |

Estado: status U8, formato 1 U8, modo U8 (0 desarmado, 1 espera fase, 2 sequencial, 3 corte, 4 pulso), falhas U8 (1 atraso, 2 prazo, 4 sincronismo/sinal, 8 calibração), máscara ativa U32, máscara agendada U32, perfil válido U8, cilindros U8, temporizador válido U8, reservado U8, geração U32, último pulso calculado U32 em µs. Canais 0–15 são LS1–LS16; canais 16–27 são IGN1–IGN12. Uma máscara não mede corrente ou realimentação do driver.

Perfil: formato U8 em 0, cilindros U8 em 1; U16 ReqFuel em 2, dwell em 4, tempo morto em 6, pulso máximo em 8, escorva ms em 10; S16 ventoinha liga/desliga em centi°C em 12/14, fim de injeção relativo ao PMS em centigraus em 16; U32 ângulo do dente zero em centigraus em 18; U8 bomba/ventoinha/relé em 22/23/24 (0 não atribuído, 1–16 LS); U8 primeiro/último dente da janela e volta de fase em 25/26/27; reservas zero 28–31. De 32 a 127, doze descritores de 8 bytes: injetor U8 zero-based, ignição U8 zero-based, PMS U32 em centigraus e duas reservas zero. Descritores não utilizados devem ser zero.
