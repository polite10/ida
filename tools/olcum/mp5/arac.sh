#!/bin/sh
# W0-MP5a: araç zincirindeki bir ikiliyi çalıştırır (MP4a arac.sh'in kopyası).
# Kullanım: tools/olcum/mp5/arac.sh arm-none-eabi-objdump <arg...>
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
A="${1:?araç adı}"; shift
exec "$ARM_GCC_BIN/$A" "$@"
