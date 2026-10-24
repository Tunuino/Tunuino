Integração de desenvolvimento: diagnóstico/empacotamento de programadores em install/ e validação de definições em actions/.
Executar: `python3 misc/install/check_environment.py`; o workflow Windows usa `install/package_programmer_tools.py` para preparar o Bundle offline.
O diagnóstico não instala programas nem altera o equipamento; `actions/verify_generated.py` valida o INI e a assinatura.
