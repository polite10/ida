#!/bin/bash
# CI: koşucunun hazır vcpkg kopyasını (VCPKG_INSTALLATION_ROOT) vcpkg.json'daki
# builtin-baseline ile kullanılabilir yapar ve VCPKG_ROOT'u GITHUB_ENV'e yazar.
#
# Paket sürümleri vcpkg.json'daki baseline + overrides ile sabittir; koşucunun vcpkg
# commit'i yalnız aracı (vcpkg ikilisi) belirler ve günlüğe yazılır (tools/ci/surumler.sh).
# Baseline commit'i koşucunun kopyasında yoksa yalnız o commit getirilir.

set -euo pipefail
export LC_ALL=C

kok=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
vcpkg_kok=${VCPKG_INSTALLATION_ROOT:?VCPKG_INSTALLATION_ROOT yok (koşucu görüntüsü)}
if command -v cygpath >/dev/null 2>&1; then
    vcpkg_kok=$(cygpath -m "$vcpkg_kok")
fi

baseline=$(sed -n 's/.*"builtin-baseline": *"\([0-9a-f]\{40\}\)".*/\1/p' "$kok/vcpkg.json")
[ -n "$baseline" ] || { echo "vcpkg.json'da builtin-baseline yok" >&2; exit 1; }

if ! git -C "$vcpkg_kok" cat-file -e "$baseline^{commit}" 2>/dev/null; then
    echo "baseline $baseline koşucuda yok; getiriliyor" >&2
    git -C "$vcpkg_kok" fetch --quiet origin "$baseline"
fi
git -C "$vcpkg_kok" cat-file -e "$baseline^{commit}"
echo "vcpkg kökü: $vcpkg_kok (HEAD $(git -C "$vcpkg_kok" rev-parse --short HEAD)); baseline $baseline" >&2

if [ -n "${GITHUB_ENV-}" ]; then
    printf 'VCPKG_ROOT=%s\n' "$vcpkg_kok" >>"$GITHUB_ENV"
fi
