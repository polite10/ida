#!/bin/sh
# W0-MP1a: ilk yüklemeden önce dahili flash'ın tamamını (2 × 2 MB, 0x08000000) okur.
# Yalnız okuma (-u / upload); Hot Plug kipinde, çekirdek sıfırlanmaz. Harici OSPI flash'a dokunmaz.
# İki kez okunur; SHA-256 tutmazsa çıkış kodu 1.
# Çıktı: $IDA_CACHE/yedek/
set -eu
. "$(dirname "$0")/ortam.sh"
OUT="$IDA_CACHE/yedek"
mkdir -p "$OUT"
renksiz() { sed 's/\x1b\[[0-9;]*m//g'; }
BOYUT=0x400000
A="$OUT/u5a9j-dk-dahili-flash-fabrika.bin"
B="$OUT/u5a9j-dk-dahili-flash-fabrika-ikinci-okuma.bin"

if [ -e "$A" ]; then
    echo "yedek-al.sh: $A zaten var; üzerine yazılmaz." >&2
    exit 2
fi
"$CLI" -c port=SWD mode=HOTPLUG -u 0x08000000 "$BOYUT" "$A" 2>&1 | renksiz > "$OUT/okuma-1.log"
"$CLI" -c port=SWD mode=HOTPLUG -u 0x08000000 "$BOYUT" "$B" 2>&1 | renksiz > "$OUT/okuma-2.log"
ls -l "$A" "$B"
shasum -a 256 "$A" "$B" | tee "$OUT/SHA256SUMS"
H1=$(shasum -a 256 "$A" | cut -d' ' -f1)
H2=$(shasum -a 256 "$B" | cut -d' ' -f1)
if [ "$H1" != "$H2" ]; then
    echo "yedek-al.sh: iki okuma farklı" >&2
    exit 1
fi
chmod a-w "$A"
echo "yedek tamam: $A"
