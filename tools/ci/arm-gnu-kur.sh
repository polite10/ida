#!/bin/bash
# CI: Arm GNU Toolchain 14.3.rel1 (arm-none-eabi) indirir, SHA-256'sını doğrular, açar.
#
# Kullanım: tools/ci/arm-gnu-kur.sh <hedef-dizin>
#   <hedef-dizin>/<paket-adı>/bin altında araç zinciri durur; bin dizini standart çıktıya
#   yazılır, GITHUB_ENV varsa ARM_GCC_BIN oraya da yazılır. Hedef zaten kuruluysa (önbellek)
#   yalnız doğrulanır.
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

case "$(uname -s)" in
    Darwin)
        [ "$(uname -m)" = arm64 ] || { echo "yalnız macOS arm64" >&2; exit 1; }
        paket=arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi
        dosya=$paket.tar.xz
        ozet=30f4d08b219190a37cded6aa796f4549504902c53cfc3c7e044a8490b6eba1f7 ;;
    MINGW*|MSYS*|CYGWIN*)
        paket=arm-gnu-toolchain-14.3.rel1-mingw-w64-x86_64-arm-none-eabi
        dosya=$paket.zip
        ozet=864c0c8815857d68a1bbba2e5e2782255bb922845c71c97636004a3d74f60986 ;;
    *)
        echo "desteklenmeyen işletim sistemi: $(uname -s)" >&2; exit 1 ;;
esac

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

bin="$hedef/$paket/bin"
if [ ! -x "$bin/arm-none-eabi-gcc" ] && [ ! -x "$bin/arm-none-eabi-gcc.exe" ]; then
    mkdir -p "$hedef"
    arsiv="$hedef/$dosya"
    curl -fsSL --retry 3 -o "$arsiv" "$taban_url/$dosya"
    gercek=$(sha256 "$arsiv")
    if [ "$gercek" != "$ozet" ]; then
        echo "SHA-256 uyuşmuyor: $dosya ($gercek)" >&2
        rm -f "$arsiv"
        exit 1
    fi
    case "$dosya" in
        *.tar.xz) tar -xJf "$arsiv" -C "$hedef" ;;
        *.zip)
            if command -v unzip >/dev/null 2>&1; then unzip -q "$arsiv" -d "$hedef"
            else 7z x -y -bd "-o$hedef" "$arsiv" >/dev/null; fi ;;
    esac
    rm -f "$arsiv"
    # Belgeler derlemede kullanılmaz; önbellek boyutunu küçültür.
    rm -rf "$hedef/$paket/share/doc"
fi

"$bin/arm-none-eabi-gcc" --version | head -1 >&2
[ "$("$bin/arm-none-eabi-gcc" -dumpversion)" = "14.3.1" ] || { echo "sürüm 14.3.1 değil" >&2; exit 1; }

if command -v cygpath >/dev/null 2>&1; then
    bin=$(cygpath -m "$bin")
fi
printf '%s\n' "$bin"
if [ -n "${GITHUB_ENV-}" ]; then
    printf 'ARM_GCC_BIN=%s\n' "$bin" >>"$GITHUB_ENV"
fi
