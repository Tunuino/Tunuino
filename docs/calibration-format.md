# Mapas de calibração Tunuino, versão 1

O ficheiro `.tnmap` representa uma tabela independente de VE ou ignição. A leitura de um ficheiro não altera uma ECU. O projeto ainda não implementa páginas de calibração, envio USB destes mapas, persistência na ECU ou integração destes valores no controlo físico do motor.

| Offset | Tamanho | Conteúdo |
| --- | --- | --- |
| 0 | 8 | ASCII `TUNUINOM` |
| 8 | 1 | Versão: 1 |
| 9 | 1 | Tipo: 1 VE, 2 ignição |
| 10 | 1 | Número de posições RPM: 2 a 32 |
| 11 | 1 | Número de posições MAP: 2 a 32 |
| 12 | 4 × posições RPM | Eixo RPM, inteiros de 32 bits |
| Seguinte | 4 × posições MAP | Eixo MAP em kPa × 1000, inteiros de 32 bits |
| Seguinte | 4 × posições RPM × posições MAP | Células: linhas MAP, colunas RPM |
| Últimos 4 bytes | 4 | CRC32 sobre todos os bytes anteriores |

Os inteiros usam ordem de rede e complemento de dois para valores com sinal. Os dois eixos são não negativos e estritamente crescentes. As células VE usam percentagem × 1000, de 0 a 300000. As células de ignição usam graus antes do PMS × 1000, de −90000 a 90000. Estes são limites do formato; não constituem valores aprovados para um motor.

A consulta usa interpolação bilinear e devolve percentagem VE ou graus de ignição. Consultas fora dos eixos ficam limitadas à borda mais próxima e devolvem uma indicação explícita de limitação. RPM/MAP negativos, NaN e infinitos são recusados. Eixos repetidos ou fora de ordem, tipo/versão desconhecidos, células inválidas, dimensões incorretas e CRC errado são recusados.

O núcleo C valida o ficheiro completo antes de alterar o mapa de destino. O destino deve usar armazenamento separado da entrada e permanecer estável durante uma consulta. O mapa tem armazenamento fornecido pelo chamador; não existe alocação dinâmica no módulo C. A implementação ainda não foi medida num ciclo de controlo STM32.

O modelo Java é imutável e a gravação em ficheiro cria um destino novo, preservando ficheiros existentes. A leitura do disco limita a quantidade de dados antes de os interpretar. O tamanho máximo é 4368 bytes, pelo que um futuro envio USB terá de usar transferência em blocos; o ficheiro não é um pacote binário nem uma definição INI TunerStudio.

Os mapas dos testes são superfícies matemáticas sintéticas. Não existem mapas padrão de motor neste bloco. A associação a uma configuração de motor, a uma revisão de hardware e a limites de aplicação continua por implementar.
