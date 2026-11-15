#!/bin/sh
# W0-MP1a K0: kartın fabrika durumunu SALT OKUNUR kaydeder. Hiçbir yazma komutu içermez.
# Bağlantı Hot Plug kipindedir: çekirdek sıfırlanmaz, durdurulmaz.
# Çıktı: $IDA_CACHE/mp1/k0/
set -eu
. "$(dirname "$0")/ortam.sh"
OUT="$MP1_VERI/k0"
mkdir -p "$OUT"
renksiz() { sed 's/\x1b\[[0-9;]*m//g'; }

"$CLI" --version 2>&1 | renksiz > "$OUT/cli-surum.txt"
"$CLI" -l st 2>&1 | renksiz > "$OUT/probe-listesi.txt"
"$CLI" -c port=SWD mode=HOTPLUG -ob displ 2>&1 | renksiz > "$OUT/ob-displ.txt"

# Ham yazmaçlar (RM0456): DBGMCU_IDCODE, FLASH_OPTR, FLASH_NSBOOTADD0/1, WRP,
# FLASHSIZE_BASE (bellek boyutu), PACKAGE_BASE.
oku() { # $1 adres, $2 bayt sayısı, $3 etiket
    printf '%s %s\n' "## $3" "$1" >> "$OUT/ham-yazmaclar.txt"
    "$CLI" -c port=SWD mode=HOTPLUG -r32 "$1" "$2" 2>&1 | renksiz | grep -E '^0x' >> "$OUT/ham-yazmaclar.txt" || true
}
: > "$OUT/ham-yazmaclar.txt"
oku 0xE0044000 4 DBGMCU_IDCODE
oku 0x40022040 4 FLASH_OPTR
oku 0x40022044 8 FLASH_NSBOOTADD0_1
oku 0x40022058 8 FLASH_WRP1AR_WRP1BR
oku 0x40022068 8 FLASH_WRP2AR_WRP2BR
oku 0x0BFA07A0 4 FLASHSIZE_BASE
oku 0x0BFA0500 4 PACKAGE_BASE
echo "K0 kaydı: $OUT"
