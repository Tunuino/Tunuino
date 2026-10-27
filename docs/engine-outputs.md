# Contrato do controlo sequencial e persistência

Alvo: Proteus v0.7 STM32F427. [Origem dos pinos](../hardware/review/proteus-v07-outputs.json).
LS1–LS16 comandam drivers VNLD5160; IGN1–IGN12 são sinais lógicos para etapas externas de ignição. Latches são colocados a zero antes de configurar GPIO.

## Perfil persistente

O bloco IO esquema 2 conserva 1408 bytes: página 1 (256), VE (576), ignição (576). Extensão retrocompatível nos bytes anteriormente reservados da página 1:

- 102: política U8, 0 ativação manual supervisionada por USB, 1 ativação autónoma ao ligar alimentação.
- 103–127: zero.
- 128–255: perfil formato 1, 128 bytes. Todos zero representam motor não configurado.

O perfil é gravado no mesmo registo CRC/commit-marker dos sensores e mapas, sem alterar o tamanho dos registos nem perder configurações esquema 2 anteriores. `.tncfg` inclui agora também o motor; os mapas continuam em `.tnmap`. Configurações antigas sem perfil continuam sem saídas automáticas. Atualizar Console, firmware e INI em conjunto.

Perfil relativo a 128: formato U8 em 0; cilindros U8 em 1; U16 ReqFuel em 2, dwell em 4, tempo morto em 6, pulso máximo em 8 (unidades internas de temporizador); U16 escorva em ms em 10; S16 temperaturas liga/desliga em centi°C em 12/14 e fim de injeção em centigraus em 16; U32 dente zero em centigraus em 18; U8 bomba/ventoinha/relé em 22/23/24 (0 desligado, 1–16 LS); U8 janela de fase primeiro/último/volta em 25/26/27; zero em 28–31. Doze descritores de 8 bytes em 32: injetor U8 e IGN U8 zero-based, PMS U32 centigraus, duas reservas zero. Descritores inativos zero. As interfaces convertem os tempos de injeção, dwell e tempo morto para **ms**, precisão de 0,001 ms.

Autónomo exige perfil válido, captura, MAP, temperatura e todas as células VE não nulas. Esta validação estrutural não certifica a afinação nem o ponto mecânico. Escritas e Burn são recusados enquanto armado; não existe afinação em tempo real nesta revisão. Guardar um perfil recarrega-o desarmado. Só o arranque elétrico pode ativá-lo automaticamente. STOP/falhas não rearmam por consultas, reconexão USB ou recuperação do sinal.

## Execução

Injeção/ignição sequenciais quatro tempos, 1–12 cilindros, referência de fase 720°. O cálculo é:

`pulso = ReqFuel × VE_percent / 100 × MAP_kPa / 100 + tempo_morto`.

ReqFuel corresponde a VE 100% / MAP 100 kPa. O agendador TIM2/GPT5 continua limitado a dwell 5 ms e injeção 20 ms. Pulso superior ao máximo ou 80% do ciclo produz corte. Referência atrasada, falha de sincronismo/sensores, mudança de geração e ausência de serviço por 20 ms cancelam saídas. O motor não depende de consultas USB no modo autónomo; o modo manual mantém supervisão de 2 segundos. Pedidos de runtime TunerStudio também renovam a supervisão manual.

A recuperação USB é uma máquina de estados sem pausa bloqueante de 250 ms no serviço do motor. Leituras/escritas CDC são não bloqueantes. No modo autónomo, recuperação USB não reinicia o descodificador nem para as saídas. O serviço continua no ciclo principal; o prazo de 20 ms é supervisionado por GPT em interrupção, não por watchdog externo. Não garante proteção contra todos os bloqueios de CPU/interrupções.

Bomba: escorva limitada e funcionamento com rotação; ventoinha: histerese, liga se temperatura inválida; relé principal ligado enquanto armado. Perda de sincronismo após funcionamento mantém corte até intervenção/reinício. Ainda não existe recuperação automática de um motor que parou por falha de sincronismo.

## Comandos

Corpo CRC `TUNUINO:X` + opcode; inteiros big-endian. Recusa 0x84.

| Opcode | Pedido | Efeito |
|---|---|---|
| 0 | vazio | Estado 24 bytes, renova supervisão manual |
| 1 | perfil 128 bytes | Compatibilidade: perfil apenas RAM; desativa política autónoma na sessão |
| 2 | `ARM!` + geração U32 | Ativação explícita da geração atual |
| 3 | vazio | STOP, cancela tudo e bloqueia reativação automática nesta sessão |
| 4 | canal U8 + duração U16 + `TEST` | Pulso individual, unidade interna do temporizador |
| 5 | canal U8 + duração U16 ms + `AUX!` | Relé atribuído, 1–800 ms, motor parado |
| 6 | `RUN!` | Ativação explícita do perfil persistente, usado pelo TunerStudio |

Estado: 0 status; 1 formato=1; 2 modo (0 parado, 1 espera fase, 2 sequencial, 3 corte, 4 teste); 3 falhas (1 atraso, 2 prazo, 4 sincronismo, 8 calibração); 4 máscara ativa U32; 8 máscara pendente U32; 12 perfil válido; 13 cilindros; 14 timer; 15 flags (1 perfil persistente, 2 autónomo); 16 geração U32; 20 pulso U32 em unidades internas do temporizador.

Runtime IO permanece com 100 bytes. Byte 31 antes reservado: modo nos bits 0–2, perfil persistente no bit 4, autónomo no bit 5, falha no bit 7. As restantes posições não mudam. `TUNUINO:V` acrescenta erros 256 perfil, 512 política autónoma.

## Limites atuais

Não há controlo de ralenti, turbo, VVT, knock, flex fuel, launch control, Lua, compensação IAT/barométrica, deadtime por tensão, enriquecimentos de arranque/aquecimento/aceleração nem lambda em malha fechada. Também não há calibração OEM específica ou suporte a rodas compostas fora do descodificador missing-tooth. Não apresentar estas funções como prontas.

Testes de registos/flash simulados exercitam comandos, temporizadores, corte, persistência e perda de USB. Precisão física, polaridades, alimentação, carga, PMS e funcionamento real continuam por validar. [Utilização e estado](https://github.com/Tunuino/Tunuino_documents/blob/main/configuracao-persistente-mapas-base.md).
