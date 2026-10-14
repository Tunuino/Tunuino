#!/usr/bin/env bash
set -euo pipefail
# Standalone build bridge while the reference firmware build system is migrated.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
make -C "$repo_root/firmware/platform/stm32f429" "$@"
