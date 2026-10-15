#!/usr/bin/env bash
set -euo pipefail
# Board build entry point; the STM32 firmware remains experimental.
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../../../../" && pwd)"
make -C "$repo_root/firmware" BOARD=stm32f429 "$@"
