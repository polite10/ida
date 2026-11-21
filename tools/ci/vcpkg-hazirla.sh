#!/bin/bash
# CI: koşucunun hazır vcpkg kopyasını (VCPKG_INSTALLATION_ROOT) vcpkg.json'daki
# builtin-baseline ile kullanılabilir yapar ve VCPKG_ROOT'u GITHUB_ENV'e yazar.
#
# Paket sürümleri vcpkg.json'daki baseline + overrides ile sabittir; koşucunun vcpkg
# commit'i yalnız aracı (vcpkg ikilisi) belirler ve günlüğe yazılır (tools/ci/surumler.sh).
# Baseline commit'i koşucunun kopyasında yoksa yalnız o commit getirilir.
#
# Koşucu görüntüsündeki vcpkg kopyası başka bir kullanıcıya ait olabilir; git bunu
# "dubious ownership" ile reddeder (vcpkg'nin kendi git çağrıları dahil). Bu yüzden yalnız
# CI'da (GITHUB_ACTIONS=true) kopya global safe.directory listesine eklenir.

set -euo pipefail
export LC_ALL=C
gunluk() { printf 'vcpkg-hazirla: %s\n' "$*" >&2; }

kok=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
vcpkg_kok=${VCPKG_INSTALLATION_ROOT:?VCPKG_INSTALLATION_ROOT yok (koşucu görüntüsü)}
ham_kok=$vcpkg_kok
if command -v cygpath >/dev/null 2>&1; then
    vcpkg_kok=$(cygpath -m "$vcpkg_kok")
fi
gunluk "vcpkg kökü: $vcpkg_kok"

if [ "${GITHUB_ACTIONS-}" = true ]; then
    git config --global --add safe.directory "$vcpkg_kok"
    [ "$ham_kok" = "$vcpkg_kok" ] || git config --global --add safe.directory "$ham_kok"
fi

if [ -x "$vcpkg_kok/vcpkg" ] || [ -x "$vcpkg_kok/vcpkg.exe" ]; then
    :
else
    gunluk "vcpkg ikilisi yok: $vcpkg_kok/vcpkg(.exe)"
    ls -la "$vcpkg_kok" >&2 || true
    exit 1
fi

baseline=$(tr -d '\r' <"$kok/vcpkg.json" |
    sed -n 's/.*"builtin-baseline": *"\([0-9a-f]\{40\}\)".*/\1/p')
[ -n "$baseline" ] || { gunluk "vcpkg.json'da builtin-baseline yok"; exit 1; }

if [ "$(git -C "$vcpkg_kok" rev-parse --is-shallow-repository)" = true ]; then
    gunluk "uyarı: vcpkg kopyası sığ (shallow); vcpkg sürüm çözümü reddedebilir"
fi
if ! git -C "$vcpkg_kok" cat-file -e "$baseline^{commit}" 2>/dev/null; then
    gunluk "baseline $baseline koşucuda yok; getiriliyor"
    git -C "$vcpkg_kok" fetch --quiet origin "$baseline"
fi
git -C "$vcpkg_kok" cat-file -e "$baseline^{commit}"
gunluk "HEAD $(git -C "$vcpkg_kok" rev-parse --short HEAD); baseline $baseline mevcut"

if [ -n "${GITHUB_ENV-}" ]; then
    printf 'VCPKG_ROOT=%s\n' "$vcpkg_kok" >>"$GITHUB_ENV"
fi
