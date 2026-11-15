#!/usr/bin/env bash
# W0-MP7a ölçüm programını derler. Embree 4.4.x yolu:
#   EMBREE_ONEK (ör. ${IDA_CACHE:-$HOME/.cache/ida}/embree-4.4.1) verilmişse o,
#   yoksa Homebrew öneki (brew --prefix).
set -euo pipefail
kok="$(cd "$(dirname "$0")" && pwd)"
onek="${EMBREE_ONEK:-$(brew --prefix 2>/dev/null || true)}"
cmake -S "$kok" -B "$kok/derleme" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$onek"
cmake --build "$kok/derleme"
