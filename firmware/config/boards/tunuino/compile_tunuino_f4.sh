#!/usr/bin/env bash
set -euo pipefail
# Temporary standalone build entry point until the firmware build is ported.
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../../../../" && pwd)"
make -C "$repo_root/firmware/platform/stm32f429" "$@"
