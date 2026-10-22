#!/usr/bin/env python3
"""Prepare release assets from this run's validated, synchronized bench kit."""
import hashlib
import json
import os
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


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
    commit = os.environ["GITHUB_SHA"]
    run_id = os.environ["GITHUB_RUN_ID"]
    if manifest["source_commit"] != commit or str(manifest["run_id"]) != run_id:
        raise ValueError("Release kit belongs to another commit or run")
    if manifest["physical_ecu_validation"] is not False:
        raise ValueError("This publisher is only for experimental bench builds")
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
    repo = os.environ["GITHUB_REPOSITORY"]
    notes = f"""## Tunuino — versão experimental de bancada

Console Windows x64 e firmware **Proteus v0.7 / STM32F427**, produzidos a partir do mesmo commit.

USB FS configurado para a Proteus F427 sem deteção externa de VBUS em PA9. A Console mostra o build no título e distingue ID inválido de erro da ferramenta ST, registando o código de saída. A gravação/enumeração física nesta ECU continua a exigir confirmação.

| Download | Conteúdo |
|---|---|
| `tunuino_bundle_proteus_v07_f427_usb_bench.zip` | Kit completo: Console, firmware e instruções |

Um único ZIP inclui `console/`, `firmware/`, `kit-manifest.json`, `START-HERE.txt` e `SHA256SUMS.txt`. Java está incluído na Console portátil.

**Firmware apenas para testar USB em bancada. Não controla um motor. A gravação começa em 0x08000000 e substitui o bootloader instalado. A validação física na ECU está pendente.**

Extrair o ZIP completo antes de abrir `console/TunuinoConsole.exe`. Manter `.bin.properties` junto do `.bin`.

O pacote prepara a configuração das ferramentas de gravação. Não inclui automaticamente STM32CubeProgrammer, BootCommander ou drivers de terceiros; consultar `console/tools/PROGRAMMERS.txt` no kit. A Console permite selecionar uma instalação existente.

[Instruções de bancada](https://github.com/Tunuino/Tunuino_documents/blob/main/proteus-v07-bringup.md) · [USB e DFU](https://github.com/Tunuino/Tunuino_documents/blob/main/console-firmware-usb.md)

Commit: [{commit}](https://github.com/{repo}/commit/{commit})

Compilação e testes automáticos: [execução {run_id}](https://github.com/{repo}/actions/runs/{run_id}). Incluem simulação de USB e verificação do runtime Windows; não constituem teste numa ECU física.

Não há imagem uaEFI nem F767 neste pacote.
"""
    Path("build/release-notes.md").write_text(notes, encoding="utf-8")
    print(f"Prepared {len(list(output.iterdir()))} assets for {commit}")


if __name__ == "__main__":
    main()
