#!/bin/sh
# W0-MP5a: fabrika yedeğinin geçerliliğini denetler (kart erişimi yok, yalnız dosyalar).
# MP1a'nın tools/olcum/mp1/yedek-dogrula.sh betiğinin kopyası (main'de; bu dalın tabanında yok).
# Geçerli yedek: iki okuma dosyası var, ikisi de tam 4.194.304 bayt, SHA-256'ları eşit ve SHA256SUMS ile
# tutarlı, yedek-al.sh'in yazdığı "tamam" işareti ya da başarılı okuma günlükleri var. Çıkış kodu 0 = geçerli.
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
if [ ! -f "$D/TAMAM" ]; then
    for g in "$D/okuma-1.log" "$D/okuma-2.log"; do
        [ -f "$g" ] || hata "yok: $g"
        grep -q "Data read successfully" "$g" || hata "$g başarılı okuma göstermiyor"
        if grep -qi "error" "$g"; then hata "$g hata içeriyor"; fi
    done
fi
echo "yedek geçerli: $A ($BEKLENEN B, SHA-256 $HA)"
