# Tunuino

Firmware próprio para centralinas Proteus e uaEFI e Tunuino Console para Windows.

O projeto está no início do desenvolvimento. Ainda não produz firmware para gravar numa ECU.

## Implementado

- Codificação e validação de pacotes com tamanho e CRC32, em C e Java.
- Testes de integridade, limites e interoperabilidade entre as duas implementações.
- Base da Console em Java 21 e JavaFX, com visualização de datalogs CSV offline.
- Enumeração de portas série e abertura/fecho da ligação USB na Console, através de jSerialComm.
- Sessão de pedidos/respostas com limite de tempo total, leituras e escritas parciais, cancelamento e invalidação após erro.

## Por implementar e validar

- Validação USB física no Windows, identificação das centralinas e recuperação de ligação.
- Firmware STM32, descodificação de cambota/árvore de cames e controlo do motor.
- Drivers, bootloader e gravação específicos de cada variante de hardware.
- Datalogging em tempo real, definições INI para TunerStudio e Autotune online/offline.
- Extração KiCad e validação de conectores, circuitos e pinos STM32.

## Verificar o núcleo

Requer Python 3, um compilador C11 e JDK 17 ou superior para os testes independentes da interface:

```sh
python3 tools/check_core.py
```

## Executar a Console no Windows

Requer JDK 21 e Maven 3.9 ou superior. A primeira resolução das dependências necessita de rede; depois podem ser usadas as dependências em cache.

```powershell
cd console
mvn javafx:run
```

Os datalogs CSV usam cabeçalho `time_ms,rpm,map_kpa,lambda,coolant_c` e ponto decimal. A Console abre ficheiros locais sem ligação à ECU.

## Ligação USB

Usa **Atualizar portas**, seleciona a porta correspondente à centralina e escolhe **Ligar USB**. A abertura da porta não confirma o modelo, a revisão de hardware ou o microcontrolador. A interface apresenta **ECU ainda por identificar** até existir identificação de protocolo implementada e validada.

O transporte binário admite um pedido de cada vez e encerra a sessão após timeout, erro de CRC ou resposta incompleta. Não repete automaticamente comandos. Os testes usam um transporte simulado; a integração nativa com jSerialComm e a interface JavaFX ainda precisam de compilação e ensaio no Windows com as centralinas.
