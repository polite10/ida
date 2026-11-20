#!/bin/bash
# CI: Arm GNU Toolchain 14.3.rel1 (arm-none-eabi) indirir, SHA-256'sını doğrular, açar.
#
# Kullanım: tools/ci/arm-gnu-kur.sh <hedef-dizin>
#   Araç zinciri <hedef-dizin>/arm-gnu-14.3.rel1/ altına açılır. Arşivin iç düzenine
#   güvenilmez: açtıktan sonra bin/arm-none-eabi-gcc(.exe) aranır ve tek aday beklenir
#   (Windows zip'inde üst dizin yoktur, macOS tar.xz'inde vardır; 2026-11-20'de zip'in
#   merkezi dizini HTTP aralık isteğiyle okunarak doğrulandı). Bulunan bin dizini standart
#   çıktıya, GITHUB_ENV varsa ARM_GCC_BIN olarak oraya da yazılır. Hedef zaten kuruluysa
#   (önbellek) indirme yapılmaz, yalnız sürüm doğrulanır.
#
# Kaynak: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads (14.3.rel1).
# Özetler Arm'ın yayımladığı .sha256asc dosyalarındandır (W2-depo kayıt tarihi 2026-11-20).
# Geliştirme Mac'indeki derleyici aynı üst akış sürümünün ST yapısıdır (GNU Tools for
# STM32 14.3.rel1); ikisi de `-dumpversion` için 14.3.1 verir (cmake/toolchains denetler).
# Lisans: GPL-3.0 ve paket içindeki öteki lisanslar; depoya girmez, yalnız CI önbelleğinde.

set -euo pipefail
export LC_ALL=C

hedef=${1:?hedef dizin verilmeli}
taban_url=https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel
gunluk() { printf 'arm-gnu-kur: %s\n' "$*" >&2; }

son=""
case "$(uname -s)" in
    Darwin)
        [ "$(uname -m)" = arm64 ] || { gunluk "yalnız macOS arm64 desteklenir"; exit 1; }
        dosya=arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi.tar.xz
        ozet=30f4d08b219190a37cded6aa796f4549504902c53cfc3c7e044a8490b6eba1f7 ;;
    MINGW*|MSYS*|CYGWIN*)
        son=.exe
        dosya=arm-gnu-toolchain-14.3.rel1-mingw-w64-x86_64-arm-none-eabi.zip
        ozet=864c0c8815857d68a1bbba2e5e2782255bb922845c71c97636004a3d74f60986 ;;
    *)
        gunluk "desteklenmeyen işletim sistemi: $(uname -s)"; exit 1 ;;
esac

if command -v cygpath >/dev/null 2>&1; then
    hedef=$(cygpath -m "$hedef")
fi
kurulum="$hedef/arm-gnu-14.3.rel1"

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

# Kurulum dizininde (kendisi ya da tek alt düzey) bin/arm-none-eabi-gcc(.exe) arar; tam bir
# aday bulursa bin dizinini yazar. Kabuk kalıbı kullanılır: Windows'ta PATH'te
# System32'deki find.exe GNU find'ın önüne geçebilir.
bin_bul() {
    local adaylar=() a
    [ -d "$kurulum" ] || return 1
    shopt -s nullglob
    for a in "$kurulum/bin/arm-none-eabi-gcc$son" "$kurulum"/*/bin/arm-none-eabi-gcc"$son"; do
        if [ -f "$a" ]; then adaylar+=("$a"); fi
    done
    shopt -u nullglob
    if [ "${#adaylar[@]}" -ne 1 ]; then
        gunluk "bin/arm-none-eabi-gcc$son aday sayısı ${#adaylar[@]} (1 bekleniyordu)"
        if [ "${#adaylar[@]}" -gt 0 ]; then printf '  %s\n' "${adaylar[@]}" >&2; fi
        return 1
    fi
    dirname "${adaylar[0]}"
}

dizin_dok() {
    local d e n
    gunluk "açılan dizinin ilk iki düzeyi ($kurulum):"
    shopt -s nullglob dotglob
    for d in "$kurulum"/*; do
        printf '  %s\n' "${d#"$kurulum"/}" >&2
        if [ -d "$d" ]; then
            n=0
            for e in "$d"/*; do
                n=$((n + 1))
                if [ "$n" -le 15 ]; then printf '    %s\n' "${e#"$kurulum"/}" >&2; fi
            done
            if [ "$n" -gt 15 ]; then printf '    ... (%d girdi)\n' "$n" >&2; fi
        fi
    done
    shopt -u nullglob dotglob
}

if ! bin=$(bin_bul 2>/dev/null); then
    rm -rf "$kurulum"
    mkdir -p "$kurulum"
    arsiv="$hedef/$dosya"
    gunluk "indiriliyor: $taban_url/$dosya"
    curl -fsSL --retry 3 -o "$arsiv" "$taban_url/$dosya"
    gercek=$(sha256 "$arsiv")
    if [ "$gercek" != "$ozet" ]; then
        gunluk "SHA-256 uyuşmuyor: $dosya ($gercek)"
        rm -f "$arsiv"
        exit 1
    fi
    gunluk "SHA-256 doğrulandı; açılıyor"
    case "$dosya" in
        *.tar.xz)
            # Göreli arşiv adı: GNU tar 'X:' önekli yolu uzak makine sanabilir.
            (cd "$hedef" && tar -xJf "$dosya" -C "$kurulum") ;;
        *.zip)
            if command -v unzip >/dev/null 2>&1; then
                gunluk "araç: unzip"
                unzip -q "$arsiv" -d "$kurulum"
            elif command -v 7z >/dev/null 2>&1; then
                gunluk "araç: 7z"
                7z x -y -bd "-o$kurulum" "$arsiv" >/dev/null
            else
                gunluk "araç: PowerShell Expand-Archive"
                powershell.exe -NoProfile -NonInteractive -Command \
                    "Expand-Archive -LiteralPath '$arsiv' -DestinationPath '$kurulum' -Force"
            fi ;;
    esac
    rm -f "$arsiv"
    if ! bin=$(bin_bul); then
        dizin_dok
        exit 1
    fi
    # Belgeler derlemede kullanılmaz; önbellek boyutunu küçültür.
    rm -rf "$(dirname "$bin")/share/doc"
fi

gcc="$bin/arm-none-eabi-gcc$son"
if [ ! -x "$gcc" ]; then
    gunluk "çalıştırılabilir değil: $gcc"
    dizin_dok
    exit 1
fi
surum_satiri=$("$gcc" --version | tr -d '\r' | sed -n 1p)
gunluk "$surum_satiri"
dump=$("$gcc" -dumpversion | tr -d '\r')
[ "$dump" = "14.3.1" ] || { gunluk "sürüm '$dump', beklenen 14.3.1"; exit 1; }

printf '%s\n' "$bin"
if [ -n "${GITHUB_ENV-}" ]; then
    printf 'ARM_GCC_BIN=%s\n' "$bin" >>"$GITHUB_ENV"
fi
