#!/bin/sh
# W0-MP1a: aygıt yazılımını dahili flash'a yükler, doğrular ve sıfırlar.
# Yalnız yazılan sayfalar silinir (toplu silme yok); seçenek baytlarına ve harici flash'a dokunulmaz.
# Kullanım: tools/olcum/mp1/yukle.sh smps|ldo
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
V="${1:?varyant: smps|ldo}"
ELF="$KOK/derleme/fw/mp1_$V.elf"
[ -f "$ELF" ] || { echo "yok: $ELF (önce derle.sh)" >&2; exit 2; }
"$CLI" -c port=SWD -w "$ELF" -v -rst 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -E "Error|error|Download verified|File download complete|Erasing|Memory Programming|Time elapsed|Start operation|MCU Reset|Software reset" || true
"$CLI" -c port=SWD mode=HOTPLUG -r32 0x08000000 8 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -E '^0x' || true
