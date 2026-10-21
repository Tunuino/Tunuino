# Dados em tempo real, versão 1

A Console só envia o pedido se a identidade recebida anunciar o bit 0 de capacidades. O corpo do pedido contém os nove bytes ASCII `TUNUINO:L`, enquadrados com comprimento e CRC32.

Uma resposta com um único byte de valor 1 significa dados indisponíveis. Não se grava uma linha CSV nem se substituem os sinais em falta por valores supostos. Uma amostra válida contém exatamente 28 bytes:

| Offset | Tamanho | Campo |
| --- | --- | --- |
| 0 | 1 | Estado: 0 |
| 1 | 1 | Versão: 1 |
| 2 | 1 | Validade: bits 0 RPM, 1 MAP, 2 lambda, 3 temperatura; todos necessários |
| 3 | 1 | Reservado: 0 |
| 4 | 8 | Milissegundos desde o início do relógio da ECU, de 0 a 2^63 − 1 |
| 12 | 4 | RPM × 1000, inteiro sem sinal |
| 16 | 4 | MAP em kPa × 1000, inteiro sem sinal |
| 20 | 4 | Lambda × 1 000 000, inteiro sem sinal |
| 24 | 4 | Temperatura do líquido em °C × 1000, inteiro com sinal |

Os campos de vários bytes usam ordem de rede. A representação da temperatura usa complemento de dois. Não se transmitem estruturas C diretamente nem números de vírgula flutuante dependentes do compilador.

O handler C requer um snapshot estável fornecido pela camada da placa. Ainda não existe aquisição física de sensores neste projeto. Os valores dos testes são sintéticos. O bit de capacidade de dados só deve ser anunciado numa compilação que integre este handler e o respetivo fornecedor de snapshots; a identidade sintética dos testes de identificação continua a anunciar zero capacidades.

O gravador Java faz pedidos sequenciais, com prazo de 500 ms por pedido e pausa configurável entre pedidos. Uma paragem normal termina o pedido em curso e fecha o CSV, mantendo a ligação aberta. Dados inválidos ou erros de comunicação encerram a sessão. Erros de ficheiro e recuo do relógio interrompem a gravação; as linhas já gravadas são preservadas.

O CSV usa o mesmo cabeçalho e unidades do leitor offline. O ficheiro é criado novo e cada amostra completa é descarregada do buffer Java para o sistema operativo. Isto não constitui garantia contra perda de energia. Um ficheiro sem amostras contém apenas o cabeçalho e não pode ser analisado como datalog pelo leitor offline.
