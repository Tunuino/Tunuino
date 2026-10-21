# Tunuino

Firmware próprio para centralinas Proteus e uaEFI e Tunuino Console para Windows.

O projeto está no início do desenvolvimento. Ainda não produz firmware para gravar numa ECU.

## Implementado

- Codificação e validação de pacotes com tamanho e CRC32, em C e Java.
- Testes de integridade, limites e interoperabilidade entre as duas implementações.
- Base da Console em Java 21 e JavaFX, com visualização de datalogs CSV offline.

## Por implementar e validar

- Comunicação USB física, deteção das centralinas e recuperação de ligação.
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
