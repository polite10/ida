#!/bin/sh
# W0-MP1a: ilk yüklemeden önce dahili flash'ın tamamını (2 × 2 MB, 0x08000000) okur.
# Yalnız okuma (-u / upload); Hot Plug kipinde, çekirdek sıfırlanmaz. Harici OSPI flash'a dokunmaz.
# İki kez okunur. Her okumanın çıkış kodu, "Data read successfully" satırı ve tam boyut ayrı ayrı denetlenir;
# biri tutmazsa dosyalar ".gecersiz" adıyla kenara alınır, "TAMAM" işareti yazılmaz, çıkış kodu 1.
# Çıktı: $IDA_CACHE/yedek/  (geçerlilik: yedek-dogrula.sh)
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
OUT="$IDA_CACHE/yedek"
mkdir -p "$OUT"
renksiz() { sed 's/\x1b\[[0-9;]*m//g'; }
BOYUT=0x400000
BOYUT_B=4194304
A="$OUT/u5a9j-dk-dahili-flash-fabrika.bin"
B="$OUT/u5a9j-dk-dahili-flash-fabrika-ikinci-okuma.bin"

if [ -e "$A" ] || [ -e "$B" ]; then
    echo "yedek-al.sh: yedek dosyası zaten var; üzerine yazılmaz (denetim: yedek-dogrula.sh)." >&2
    exit 2
fi

gecersiz() {
    echo "yedek-al.sh: $1" >&2
    for f in "$A" "$B"; do [ -e "$f" ] && mv "$f" "$f.gecersiz"; done
    exit 1
}

oku() { # $1 dosya, $2 günlük
    if "$CLI" -c port=SWD mode=HOTPLUG -u 0x08000000 "$BOYUT" "$1" > "$2.ham" 2>&1; then rc=0; else rc=$?; fi
    renksiz < "$2.ham" > "$2"; rm -f "$2.ham"
    [ "$rc" -eq 0 ] || gecersiz "okuma çıkış kodu $rc ($2)"
    grep -q "Data read successfully" "$2" || gecersiz "okuma başarı satırı yok ($2)"
    [ -f "$1" ] || gecersiz "dosya oluşmadı: $1"
    n=$(wc -c < "$1" | tr -d ' ')
    [ "$n" -eq "$BOYUT_B" ] || gecersiz "$1 boyutu $n, beklenen $BOYUT_B"
}

oku "$A" "$OUT/okuma-1.log"
oku "$B" "$OUT/okuma-2.log"
shasum -a 256 "$A" "$B" > "$OUT/SHA256SUMS"
cat "$OUT/SHA256SUMS"
H1=$(shasum -a 256 "$A" | cut -d' ' -f1)
H2=$(shasum -a 256 "$B" | cut -d' ' -f1)
[ "$H1" = "$H2" ] || gecersiz "iki okuma farklı"
chmod a-w "$A" "$B"
date > "$OUT/TAMAM"
echo "yedek tamam: $A"
