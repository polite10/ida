#!/bin/sh
# W0-MP1a K1: temiz dizinden derleme + yükleme, iki kez üst üste. Her turda ikililerin SHA-256'sı ve
# yüklemeden sonraki seri satırlar kaydedilir. Ölçüt: iki turda aynı özetler, ikisinde de başarılı yükleme
# ve yeni imajın açılış satırı. Herhangi bir adım başarısızsa durur (çıkış kodu ≠ 0).
# Kullanım: tools/olcum/mp1/k1-derle-yukle.sh [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
[ -n "$PORT" ] || { echo "k1: seri port yok" >&2; exit 2; }
OUT="$MP1_VERI/k1"
mkdir -p "$OUT"
for tur in 1 2; do
    echo "=== tur $tur"
    if ! sh "$KOK/derle.sh" > "$OUT/derle-$tur.log" 2>&1; then echo "k1: derleme başarısız (tur $tur)" >&2; exit 1; fi
    grep -A4 -- '--- SHA-256' "$OUT/derle-$tur.log" | tail -n 4 > "$OUT/ozet-$tur.txt"
    cat "$OUT/ozet-$tur.txt"
    # Seri kaydı yüklemeden önce başlar; böylece sıfırlamadan sonraki açılış satırı kaçmaz.
    "$KOK/derleme/pc/mp1_seri" kayit "$PORT" 12 "$OUT/seri-$tur.csv" > "$OUT/seri-$tur.txt" 2>&1 &
    KP=$!
    sleep 1
    if ! sh "$KOK/yukle.sh" smps "$OUT/yukle-$tur.log"; then
        kill "$KP" 2>/dev/null || true
        echo "k1: yükleme başarısız (tur $tur)" >&2; exit 1
    fi
    if ! wait "$KP"; then echo "k1: seri kayıt başarısız (tur $tur)" >&2; exit 1; fi
    cat "$OUT/seri-$tur.txt"
    if ! LC_ALL=C grep -q "B MP1a reg_istenen=SMPS" "$OUT/seri-$tur.csv"; then
        echo "k1: yeni imajın açılış satırı görülmedi (tur $tur)" >&2; exit 1
    fi
done
if cmp -s "$OUT/ozet-1.txt" "$OUT/ozet-2.txt"; then echo "K1 özet: iki turun SHA-256'ları aynı"; else echo "K1 özet: SHA-256 FARKLI"; exit 1; fi
