#!/bin/sh
# W0-MP1a: temiz dizinden aygıt yazılımı ve PC aracı derlemesi. Çıktı dosyalarının SHA-256'sını yazar.
# Kullanım: tools/olcum/mp1/derle.sh
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
cd "$KOK"
rm -rf derleme
cmake --preset mp1-fw >/dev/null
cmake --build --preset mp1-fw
cmake --preset mp1-pc >/dev/null
cmake --build --preset mp1-pc
echo "--- SHA-256"
shasum -a 256 derleme/fw/mp1_smps.elf derleme/fw/mp1_smps.bin derleme/fw/mp1_ldo.elf derleme/fw/mp1_ldo.bin
