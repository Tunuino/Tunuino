#!/bin/sh
# SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
set -eu
cd "$(dirname "$0")/.."
exec make -C simulator test
