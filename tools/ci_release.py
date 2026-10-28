#!/usr/bin/env python3
"""Prepare release assets from this run's validated, synchronized bench kit."""
import hashlib
import json
import os
from pathlib import Path
import shutil
from zipfile import ZIP_DEFLATED, ZipFile
from ci_package_kit import validate_tunerstudio
from ci_build_installer import SETUP_NAME


def validate_installer(directory, commit, run_id):
    metadata = json.loads((directory / "installer-manifest.json").read_text(encoding="utf-8-sig"))
    if metadata["source_commit"] != commit or str(metadata["run_id"]) != str(run_id):
        raise ValueError("Installer belongs to another commit or kit run")
    if metadata["file"] != SETUP_NAME:
        raise ValueError("Unexpected installer filename")
    for check in ("install_upgrade_uninstall_verified", "user_data_preserved", "installed_runtime_verified"):
        if metadata.get(check) is not True:
            raise ValueError("Installer did not pass " + check)
    package = directory / SETUP_NAME
    if hashlib.sha256(package.read_bytes()).hexdigest() != metadata["sha256"]:
        raise ValueError("Installer checksum mismatch")
    return package


def bundle(source, output, prefix=""):
    files = sorted(p for p in source.rglob("*") if p.is_file())
    if not files:
        raise ValueError(f"Empty release bundle: {source}")
    with ZipFile(output, "w", ZIP_DEFLATED) as archive:
        for path in files:
            if path.is_symlink():
                raise ValueError(f"Symbolic link in release: {path}")
            archive.write(path, (Path(prefix) / path.relative_to(source)).as_posix())


def main():
    root = Path("build/kit")
    manifest = json.loads((root / "kit-manifest.json").read_text())
    commit = os.environ.get("SOURCE_SHA", os.environ["GITHUB_SHA"])
    run_id = os.environ.get("KIT_RUN_ID", os.environ["GITHUB_RUN_ID"])
    if manifest["source_commit"] != commit or str(manifest["run_id"]) != run_id:
        raise ValueError("Release kit belongs to another commit or run")
    if manifest["physical_ecu_validation"] is not False:
        raise ValueError("This publisher is only for experimental bench builds")
    validate_tunerstudio(root, manifest["firmware"])
    for required in ("console/TunuinoConsole.exe", "console/package-manifest.json",
                     "console/tools/tools-manifest.json", "firmware/manifest.json", "START-HERE.txt"):
        if not (root / required).is_file():
            raise ValueError(f"Incomplete release kit: {required}")
    bins = list((root / "firmware").glob("*.bin"))
    if not bins or any(not path.with_name(path.name + ".properties").is_file() for path in bins):
        raise ValueError("Firmware and flashing manifest must stay together")
    output = Path("build/release")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Release destination must be empty")
    checksums = []
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p != root / "SHA256SUMS.txt"):
        with path.open("rb") as source:
            digest = hashlib.file_digest(source, "sha256").hexdigest()
        checksums.append(f"{digest}  {path.relative_to(root).as_posix()}\n")
    (root / "SHA256SUMS.txt").write_text("".join(checksums), encoding="utf-8")
    bundle(root, output / "tunuino_bundle_proteus_v07_f427_usb_bench.zip")
    installer = validate_installer(Path("build/installer"), commit, run_id)
    shutil.copy2(installer, output / SETUP_NAME)
    repo = os.environ["GITHUB_REPOSITORY"]
    notes = f"""## Tunuino — tabelas estáveis, painéis ajustáveis e logótipo original

Console Windows x64, firmware **Proteus v0.7 / STM32F427** e **INI TunerStudio**, produzidos a partir do mesmo commit.

Esta imagem usa o startup, kernel e USB CDC do repositório **Tunuino/ChibiOS**, fixado no commit `fd2e59c34878b1ef93ef5358f11fb5cc830027ff`: inicialização ChibiOS, desconexão durante 250 ms e ligação do USB FS em PA11/PA12, sem deteção externa de VBUS. SYSCLK: 168 MHz; USB: 48 MHz. A Console mostra o build no título. A nova comunicação TunerStudio ainda requer ensaio na ECU.

| Download | Conteúdo |
|---|---|
| **`tunuino_setup_windows_x64.msi`** | **Descarga principal:** instalador Windows x64 com Java, Console, firmware e INI |
| `tunuino_bundle_proteus_v07_f427_usb_bench.zip` | Versão portátil temporária para testes |

**Instalação:** abrir o MSI e seguir o assistente. Inclui atalhos no ambiente de trabalho e menu Iniciar, escolha de pasta e desinstalação nas Definições do Windows. Java está incluído. O firmware, o INI correspondente, o código-fonte e as licenças estão na pasta `firmware` junto da aplicação; o seletor de firmware da Console abre essa pasta. Não grava a ECU nem instala drivers automaticamente.

**Atualizações:** fechar a Console e instalar o MSI mais recente. Mapas, configurações e datalogs são guardados por predefinição em `%LOCALAPPDATA%\\TunuinoConsole` e ficam preservados na atualização e desinstalação. As definições antigas de ferramentas em `%USERPROFILE%\\.tunuino` continuam legíveis. A atualização exige descarregar e executar o novo instalador; ainda não existe atualização automática. O instalador ainda não tem assinatura digital de editor.

O ZIP portátil mantém `console/`, `firmware/`, `kit-manifest.json`, `START-HERE.txt` e `SHA256SUMS.txt`. Só será retirado dos novos lançamentos após confirmação de que os testes passaram. Os dois formatos contêm os componentes do mesmo commit. O firmware inclui `corresponding-source.tar.gz` e as licenças ChibiOS.

**Correção ao guardar na ECU:** o ciclo USB volta a ler o relógio depois de executar um pedido que pode gravar na flash. A transmissão já iniciava o seu prazo após a gravação; o ciclo principal reutilizava o instante anterior e provocava um erro de relógio, seguido de reinício USB. O teste do ciclo real reproduziu o erro antes da correção e passou com persistência simulada de 2, 250, 1500 e 4500 ms, incluindo falha de persistência sem falsa confirmação. A Console identifica a fase da falha e mantém o estado incerto para releitura, sem repetir automaticamente a gravação. **É necessário gravar na Proteus o firmware incluído neste Bundle para aplicar a correção.** [Detalhes e atualização](https://github.com/Tunuino/Tunuino_documents/blob/main/correcao-gravacao-usb.md).

**Tabelas e logótipo:** eliminado o ciclo de cálculo de tamanho entre os desenhos das tabelas e os respetivos contentores. O mapa acompanha a área disponível sem a fazer crescer. Separadores arrastáveis ajustam a largura VE/estado, ignição/3D e a altura das duas zonas. O editor de mapas tem controlos que passam à linha, limites adaptados ao ecrã e redimensionamento nativo. O cabeçalho volta a usar o PNG original Tunuino, com proporções preservadas. A regressão do tamanho preferido falhou antes da correção; os testes verificam também encolher/expandir e preservar as 256 células. [Detalhes](https://github.com/Tunuino/Tunuino_documents/blob/main/console-tabelas-redimensionamento.md).

**Painel simplificado:** retirada a coluna Registo do Painel e das áreas de tabelas VE/ignição. Fica apenas Estado do motor com comunicação à direita; a tabela começa com 80% da largura central, ajustável pelo separador. A página Datalog e os avisos das operações continuam disponíveis. Esta alteração visual aplica-se ao atualizar a Console.

**Console e calibrações:** painel compacto com seis mostradores, duas tabelas 16×16, apenas Estado do motor à direita, sem coluna Registo e superfície 3D; calibrações MAP, lambda, CLT, TPS, IAT e tensão reunidas em Sensores e calibrações. Medir ponto usa o ADC real da ligação escolhida; TPS tem captura fechado/aberto e CLT/IAT têm assistente NTC com R25, Beta e pull-up explícitos. Guardar TPS/IAT/tensão preserva os restantes controlos e modos, com validação e releitura. As pesquisas automáticas COM/DFU deixam de bloquear os controlos; a DFU pausa fora da página ou com a COM ocupada e mantém listas/seleção quando nada mudou. A monitorização lê TPS, IAT, tensão e estado das saídas pelo protocolo nativo, sem alterar o formato CSV existente. [Guia e imagens](https://github.com/Tunuino/Tunuino_documents/blob/main/console-calibracoes-estabilidade.md).

**Ligações rápidas:** listas curtas com Desativada, Recomendada/Proposta e Atual quando diferente. As alternativas completas ficam em Personalizar e a matriz manual fica fechada em Avançado. As recomendações identificam conector e contacto: bomba Preto35/20, ventoinha Preto35/9, relé Preto35/21, MAP Branco35/13, TPS Branco35/24, CLT Branco35/20 e IAT Branco35/30. Configurações anteriores são preservadas; escolher uma recomendação não ocupa uma ligação usada por outra função. [Guia](https://github.com/Tunuino/Tunuino_documents/blob/main/ligacoes-rapidas-proteus.md).

**Funções de entradas e saídas:** novo quadro em Configuração → Funções de I/O, com escolhas pelo nome: MAP, lambda, TPS, temperaturas, injetores, ignição, bomba, ventoinha, relé e válvulas PWM. Cada ligação mostra apenas funções compatíveis; mover uma função retira a atribuição anterior. Alterações das páginas 1 e 4 são validadas e guardadas numa única operação, com releitura e sem armar saídas. As listas das restantes janelas e do INI também usam nomes de funções. [Guia](https://github.com/Tunuino/Tunuino_documents/blob/main/funcoes-entradas-saidas.md).

**Combustível e proteções:** nova página da Console com referências aspiradas/sobrealimentadas, configuração no INI e todas as novas funções desativadas na atualização. Acrescenta compensação IAT, tempo morto/dwell por tensão em ms, alvo lambda por MAP e PI de correção, enriquecimento de aceleração por TPS, DFCO, limitador por corte de combustível, launch com entrada analógica condicionada, flex fuel por controlador analógico, corte retido por CLT/tensão e ajustes individuais por cilindro. As definições TPS/ralenti/turbo são preservadas e o backup `.tnaux` inclui toda a página. Alvo lambda é uma curva de três pontos por MAP, sem aprendizagem VE/Autotune. Flex fuel não lê diretamente sensores de frequência. VVT, detonação e borboleta eletrónica continuam pendentes. Firmware antigo rejeita esta extensão; exportar todas as configurações antes de atualizar ou voltar a versões anteriores. [Guia](https://github.com/Tunuino/Tunuino_documents/blob/main/combustivel-protecoes.md).

**Ralenti e turbo:** nova página TPS/ralenti/turbo na Console e no INI. TPS crescente ou decrescente; ralenti por curva de temperatura ou correção PI de RPM; turbo por duty fixo ou correção PI de MAP absoluto; limite de sobrepressão com corte. PWM executado pelo temporizador de saídas, com recusa de canais usados por injetores/relés. Controlos desativados por predefinição. Exigem configuração das válvulas, afinação e validação elétrica. Configuração guardada na página 4 e backup separado `.tnaux`. Os registos anteriores de flash são lidos e migrados para o novo formato sem apagar o último registo válido antes da confirmação. Voltar a firmware antigo depois de gravar este formato pode recuperar valores antigos do banco legado; exportar perfil, mapas e auxiliares antes de atualizar.

**Arranque e aquecimento:** correções persistentes de combustível durante arranque, enriquecimento após arranque com redução temporal em ms, curva de aquecimento de três pontos e avanço fixo de arranque opcional. São aplicadas pelo firmware antes do tempo morto e dos limites de pulso. Referências editáveis, desativadas por predefinição; exigem afinação e validação física.

**Menus reorganizados:** Configuração → Motor e saídas / Sensores e entradas; Diagnóstico → Testes de atuadores / Estado da ECU. Modelos locais de cilindros e ordem de ignição, rodas 36-1, 60-2, 24-1 e 12-1, curvas lineares ADC e listas de valores. Testes separados de injetores, ignição e relés; os relés atribuídos usam pulsos de 100–800 ms. Nenhuma escolha de modelo ou saída envia comandos automaticamente. Os modelos estruturais exigem a calibração real do motor.

**Protocolo IO 0.2.0 com comando experimental de saídas: injeção/ignição sequenciais 720°, bomba, relé e ventoinha configuráveis. O perfil do motor é guardado na flash com os sensores e mapas. No modo autónomo configurado, ativa o perfil ao ligar a alimentação e funciona sem USB; guardar ou voltar a ligar a USB não arma as saídas. STOP e falhas mantêm o corte até ativação explícita ou reinício. Este novo comportamento ainda exige validação física na Proteus e num motor. A gravação começa em 0x08000000 e substitui o bootloader instalado.**

Extrair o ZIP completo antes de abrir `console/TunuinoConsole.exe`. Manter `.bin.properties` junto do `.bin`.

**TunerStudio:** gravar o firmware deste Bundle; encerrar a ligação COM da Console e criar um projeto no TunerStudio. Em **Other/Browse**, selecionar `firmware/tunuino_proteus_v07_f427_io.ini`, a COM da ECU e 115200 baud, com protocolo **Firmware Default**. O INI apresenta as 16 entradas ADC, sensores calibrados, configuração de roda fónica e mapas VE/ignição 16×16. As entradas calibradas e a captura ficam desativadas até serem configuradas. Configurar as curvas reais do sensor/controlador e usar Burn; as definições são guardadas com CRC em dois setores reservados da flash. A configuração de fábrica mantém mapas a zero e saídas desativadas. A Console inclui mapas base locais 16×16 (aspirado ou referência plana), configuração inicial e estimativa de ReqFuel; exigem afinação para o motor real. Os tempos de injeção, dwell e tempo morto são apresentados em ms. O INI também configura cilindros, saídas, PMS, fase, injeção, ignição, bomba, ventoinha, relé e modo autónomo. Com perfil ativo e sincronismo válido, os mapas são usados no comando das saídas. Usar uma aplicação de cada vez na mesma COM. Na Console, Ver dados em tempo real mostra sensores válidos e Entradas ADC mostra as leituras brutas; os mapas podem ser guardados e verificados na ECU. A validação física deste novo perfil continua pendente. Antes de atualizar, exportar a configuração e os mapas antigos: os registos IO 0.2.0 anteriores são recuperados com ralenti/turbo desativados e migrados na primeira gravação. Formatos anteriores ao IO 0.2.0 não são recuperados automaticamente.

O pacote prepara a configuração das ferramentas de gravação. Não inclui automaticamente STM32CubeProgrammer, BootCommander ou drivers de terceiros; consultar `console/tools/PROGRAMMERS.txt` no kit. A Console permite selecionar uma instalação existente.

[Instruções de bancada](https://github.com/Tunuino/Tunuino_documents/blob/main/proteus-v07-bringup.md) · [USB e DFU](https://github.com/Tunuino/Tunuino_documents/blob/main/console-firmware-usb.md)

Commit: [{commit}](https://github.com/{repo}/commit/{commit})

Compilação e testes automáticos: [execução {run_id}](https://github.com/{repo}/actions/runs/{run_id}). Incluem simulação de USB, instalação limpa e atualização MSI, atalhos, hashes dos componentes instalados, runtime instalado, desinstalação e preservação de ficheiros do utilizador. Não constituem teste numa ECU física.

Não há imagem uaEFI nem F767 neste pacote.
"""
    Path("build/release-notes.md").write_text(notes, encoding="utf-8")
    print(f"Prepared {len(list(output.iterdir()))} assets for {commit}")


if __name__ == "__main__":
    main()
