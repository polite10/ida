#!/bin/sh
# firmware/araclar/yedek-dogrula.sh — fabrika flash yedeğinin geçerliliği (kart erişimi yok).
# MP1'in tools/olcum/mp1/yedek-dogrula.sh kuralı: iki okuma dosyası var, ikisi de tam
# 4 194 304 bayt, SHA-256'ları eşit ve SHA256SUMS ile tutarlı. Çıkış 0 = geçerli.
set -eu
. "$(dirname "$0")/ortam.sh"
D="$IDA_CACHE/yedek"
A="$D/u5a9j-dk-dahili-flash-fabrika.bin"
B="$D/u5a9j-dk-dahili-flash-fabrika-ikinci-okuma.bin"
BEKLENEN=4194304
hata() { echo "yedek-dogrula.sh: GEÇERSİZ: $1" >&2; exit 1; }
[ -f "$A" ] || hata "yok: $A"
[ -f "$B" ] || hata "yok: $B"
[ -f "$D/SHA256SUMS" ] || hata "yok: $D/SHA256SUMS"
for f in "$A" "$B"; do
    n=$(wc -c < "$f" | tr -d ' ')
    [ "$n" -eq "$BEKLENEN" ] || hata "$f boyutu $n, beklenen $BEKLENEN"
done
HA=$(shasum -a 256 "$A" | cut -d' ' -f1)
HB=$(shasum -a 256 "$B" | cut -d' ' -f1)
[ "$HA" = "$HB" ] || hata "iki okumanın SHA-256'sı farklı"
grep -q "^$HA " "$D/SHA256SUMS" || hata "SHA256SUMS kaydı dosyayla tutmuyor"
echo "yedek geçerli ($BEKLENEN B, SHA-256 $(echo "$HA" | cut -c1-16)…)"
