#!/bin/sh
# W0-MP5a iyileştirme turu: verilen derlemelerin (derleme/<ad>/kimlik.txt) en son ham kaydını bulur
# ve ozet.py ile tek tabloda özetler. Kullanım: tools/olcum/mp5/adimlar.sh [adimlar|deneme] <derleme...>
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
KOMUT="${1:?adimlar ya da deneme}"; shift
KAYITLAR=""
KIMLIKLER=""
for v in "$@"; do
    K=$(cat "$KOK/derleme/$v/kimlik.txt")
    Y=$(grep -l -a "kimlik=$K " "$MP5_VERI"/kos/"$v"-*[0-9].txt 2>/dev/null | tail -n 1)
    [ -n "$Y" ] || { echo "kayıt yok: $v ($K)" >&2; exit 1; }
    KAYITLAR="$KAYITLAR $Y"
    KIMLIKLER="${KIMLIKLER:+$KIMLIKLER,}$K"
done
# shellcheck disable=SC2086
python3 -I "$KOK/ozet.py" "$KOMUT" $KAYITLAR --kimlik "$KIMLIKLER"
