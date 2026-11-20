#!/bin/bash
# CI: cppcheck 2.22.0'ı kaynaktan derler ve <hedef-dizin>'e kurar (sürüm ve özet sabit).
#
# Kullanım: tools/ci/cppcheck-kur.sh <hedef-dizin>
#   Kurulu cppcheck'in yolu standart çıktıya, GITHUB_ENV varsa CPPCHECK olarak oraya da
#   yazılır. Hedef zaten kuruluysa (önbellek) yalnız sürüm doğrulanır.
#
# Kaynak: https://github.com/danmar/cppcheck/releases/tag/2.22.0 (etiket commit'i
# a436ca35ed1887bee789765122b65ed2d7a7e045). Arşiv GitHub'ın etiket arşividir; SHA-256 bu iş
# sırasında indirilen arşivden alındı. GitHub arşivin baytlarını değiştirirse kurulum
# özet uyuşmazlığıyla durur (sessizce başka kaynakla sürmez).
# Neden kaynaktan: macOS'ta sürümü sabitlenebilen bir ikili dağıtım yok (Homebrew en son
# sürümü kurar); iki işletim sisteminde aynı yol izlenir. Lisans: GPL-3.0; depoya girmez.
# Derleyici: macOS'ta Apple Clang, Windows'ta MSVC (geliştirici ortamı etkin olmalı).

set -euo pipefail
export LC_ALL=C

hedef=${1:?hedef dizin verilmeli}
surum=2.22.0
ozet=d74945deb2d50393430e07596b766f8a779512c7f60dac2a30ea64e059ece57b

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

if command -v cygpath >/dev/null 2>&1; then
    hedef=$(cygpath -m "$hedef")
fi
kurulum="$hedef/cppcheck-$surum"
exe="$kurulum/bin/cppcheck"
[ -x "$exe" ] || exe="$exe.exe"

if [ ! -x "$exe" ]; then
    is="$hedef/cppcheck-$surum-is"
    rm -rf "$is"
    mkdir -p "$is"
    curl -fsSL --retry 3 -o "$is/kaynak.tar.gz" \
        "https://github.com/danmar/cppcheck/archive/refs/tags/$surum.tar.gz"
    gercek=$(sha256 "$is/kaynak.tar.gz")
    if [ "$gercek" != "$ozet" ]; then
        echo "SHA-256 uyuşmuyor: cppcheck $surum ($gercek)" >&2
        exit 1
    fi
    tar -xzf "$is/kaynak.tar.gz" -C "$is"
    cmake -S "$is/cppcheck-$surum" -B "$is/derleme" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$kurulum" \
        -DBUILD_GUI=OFF -DBUILD_TESTING=OFF -DHAVE_RULES=OFF -DUSE_MATCHCOMPILER=Off \
        >/dev/null
    cmake --build "$is/derleme" >/dev/null
    cmake --install "$is/derleme" >/dev/null
    rm -rf "$is"
    exe="$kurulum/bin/cppcheck"
    [ -x "$exe" ] || exe="$exe.exe"
fi

gercek_surum=$("$exe" --version)
echo "$gercek_surum" >&2
[ "$gercek_surum" = "Cppcheck $surum" ] || { echo "beklenen Cppcheck $surum" >&2; exit 1; }

printf '%s\n' "$exe"
if [ -n "${GITHUB_ENV-}" ]; then
    printf 'CPPCHECK=%s\n' "$exe" >>"$GITHUB_ENV"
fi
