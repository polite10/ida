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
# Derleyici: macOS'ta Apple Clang, Windows'ta MSVC (geliştirici ortamı etkin, CC=cl).
# Yapılandırma ve derleme çıktısı günlük dosyasına yazılır; başarısızlıkta son satırları
# basılır.

set -euo pipefail
export LC_ALL=C

hedef=${1:?hedef dizin verilmeli}
surum=2.22.0
ozet=d74945deb2d50393430e07596b766f8a779512c7f60dac2a30ea64e059ece57b
gunluk() { printf 'cppcheck-kur: %s\n' "$*" >&2; }

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

if command -v cygpath >/dev/null 2>&1; then
    hedef=$(cygpath -m "$hedef")
fi
kurulum="$hedef/cppcheck-$surum"

exe_bul() {
    local a
    for a in "$kurulum/bin/cppcheck" "$kurulum/bin/cppcheck.exe"; do
        if [ -f "$a" ]; then printf '%s\n' "$a"; return 0; fi
    done
    return 1
}

# Komutu günlük dosyasına çalıştırır; başarısızsa son 60 satırı basar.
kaydet() {
    local ad=$1; shift
    gunluk "$ad"
    if ! "$@" >"$is/$ad.log" 2>&1; then
        gunluk "$ad başarısız; günlüğün sonu:"
        tail -n 60 "$is/$ad.log" >&2 || true
        exit 1
    fi
}

if ! exe=$(exe_bul); then
    is="$hedef/cppcheck-$surum-is"
    rm -rf "$is" "$kurulum"
    mkdir -p "$is"
    gunluk "indiriliyor: cppcheck $surum"
    curl -fsSL --retry 3 -o "$is/kaynak.tar.gz" \
        "https://github.com/danmar/cppcheck/archive/refs/tags/$surum.tar.gz"
    gercek=$(sha256 "$is/kaynak.tar.gz")
    if [ "$gercek" != "$ozet" ]; then
        gunluk "SHA-256 uyuşmuyor: cppcheck $surum ($gercek)"
        exit 1
    fi
    # Göreli arşiv adı: GNU tar (Git Bash) 'D:/...' biçimli adı uzak makine sanar.
    (cd "$is" && tar -xzf kaynak.tar.gz)
    [ -f "$is/cppcheck-$surum/CMakeLists.txt" ] || { gunluk "açılan kaynakta CMakeLists.txt yok"; ls -la "$is" >&2; exit 1; }
    kaydet yapilandirma cmake -S "$is/cppcheck-$surum" -B "$is/derleme" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$kurulum" \
        -DBUILD_GUI=OFF -DBUILD_TESTING=OFF -DHAVE_RULES=OFF -DUSE_MATCHCOMPILER=Off
    kaydet derleme cmake --build "$is/derleme"
    kaydet kurulum cmake --install "$is/derleme"
    rm -rf "$is"
    exe=$(exe_bul) || { gunluk "kurulumdan sonra cppcheck yok: $kurulum/bin"; ls -la "$kurulum" "$kurulum/bin" >&2 || true; exit 1; }
fi

gercek_surum=$("$exe" --version | tr -d '\r')
gunluk "$gercek_surum ($exe)"
[ "$gercek_surum" = "Cppcheck $surum" ] || { gunluk "beklenen Cppcheck $surum"; exit 1; }

printf '%s\n' "$exe"
if [ -n "${GITHUB_ENV-}" ]; then
    printf 'CPPCHECK=%s\n' "$exe" >>"$GITHUB_ENV"
fi
