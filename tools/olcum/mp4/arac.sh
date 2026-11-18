#!/bin/sh
# W0-MP4a: araç zincirindeki bir ikiliyi çalıştırır. Kullanım: tools/olcum/mp4/arac.sh arm-none-eabi-nm <arg...>
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
A="${1:?araç adı}"; shift
exec "$ARM_GCC_BIN/$A" "$@"
