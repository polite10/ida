#!/bin/sh
# firmware/araclar/arac.sh — araç zincirinden bir ikiliyi çalıştırır (ortam.sh'in yollarıyla).
# Kullanım: firmware/araclar/arac.sh arm-none-eabi-nm <arg...>
#           firmware/araclar/arac.sh STM32_Programmer_CLI <arg...>
#           firmware/araclar/arac.sh cmake --preset fw-release   (ARM_GCC_BIN ortamda)
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
A="${1:?araç adı}"
shift
case "$A" in
    arm-none-eabi-*) exec "$ARM_GCC_BIN/$A" "$@" ;;
    STM32_Programmer_CLI) exec "$CLI" "$@" ;;
    *) exec "$A" "$@" ;;
esac
