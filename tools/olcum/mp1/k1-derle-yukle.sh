#!/bin/sh
# W0-MP1a K1: temiz dizinden derleme + yükleme, iki kez üst üste. Her turda ikililerin SHA-256'sı ve
# yüklemeden sonraki ilk seri satırlar kaydedilir. Ölçüt: iki turda aynı özetler ve aynı açılış davranışı.
# Kullanım: tools/olcum/mp1/k1-derle-yukle.sh [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
OUT="$MP1_VERI/k1"
mkdir -p "$OUT"
for tur in 1 2; do
    echo "=== tur $tur"
    sh "$KOK/derle.sh" > "$OUT/derle-$tur.log" 2>&1
    grep -A4 -- '--- SHA-256' "$OUT/derle-$tur.log" | tail -n 4 | tee "$OUT/ozet-$tur.txt"
    # Seri kaydı yüklemeden önce başlar; böylece sıfırlamadan sonraki açılış satırı kaçmaz.
    "$KOK/derleme/pc/mp1_seri" kayit "$PORT" 12 "$OUT/seri-$tur.csv" > "$OUT/seri-$tur.txt" 2>&1 &
    KP=$!
    sleep 1
    sh "$KOK/yukle.sh" smps | tee "$OUT/yukle-$tur.log"
    wait "$KP"
    cat "$OUT/seri-$tur.txt"
    "$KOK/derleme/pc/mp1_seri" analiz "$OUT/seri-$tur.csv" | head -n 2
done
if cmp -s "$OUT/ozet-1.txt" "$OUT/ozet-2.txt"; then echo "K1 özet: iki turun SHA-256'ları aynı"; else echo "K1 özet: SHA-256 FARKLI"; exit 1; fi
