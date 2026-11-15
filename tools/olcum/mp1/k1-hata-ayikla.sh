#!/bin/sh
# W0-MP1a K1: ST-LINK_gdbserver + arm-none-eabi-gdb toplu kipte kesme noktası, yığın izi ve IWDG dondurma.
# Kart çalışan mp1_smps imajıyla bağlıdır (k1-derle-yukle.sh). gdbserver çalışan hedefe bağlanır (-g).
# Başarı: gdb çıkış kodu 0, yığın izi main'e ulaşır, "IWDG_SONUC SIFIRLAMA_YOK", seri kayıt başarılı ve karşı
# denemede iwdgrst=1 ile yeni açılış görülür. Aksi halde çıkış kodu 1.
# Kullanım: tools/olcum/mp1/k1-hata-ayikla.sh [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
[ -n "$PORT" ] || { echo "k1-hata-ayikla: seri port yok" >&2; exit 2; }
OUT="$MP1_VERI/k1"
mkdir -p "$OUT"

"$KOK/derleme/pc/mp1_seri" kayit "$PORT" 75 "$OUT/gdb-seri.csv" > "$OUT/gdb-seri.txt" 2>&1 &
KP=$!
"$GDBSRV" -p 61234 -l 1 -d -g -cp "$CUBEPROG_BIN" > "$OUT/gdbserver.log" 2>&1 &
GP=$!
sleep 3
if "$GDB" -batch -x "$KOK/gdb/k1-iwdg.gdb" "$KOK/derleme/fw/mp1_smps.elf" > "$OUT/gdb.log" 2>&1; then GRC=0; else GRC=$?; fi
cat "$OUT/gdb.log"
sleep 2
kill "$GP" 2>/dev/null || true
if wait "$KP"; then KRC=0; else KRC=$?; fi
echo "--- seri (T dışı satırlar)"
cat "$OUT/gdb-seri.txt"

HATA=0
[ "$GRC" -eq 0 ] || { echo "k1-hata-ayikla: gdb çıkış kodu $GRC" >&2; HATA=1; }
[ "$KRC" -eq 0 ] || { echo "k1-hata-ayikla: seri kayıt çıkış kodu $KRC" >&2; HATA=1; }
grep -q "IWDG_SONUC SIFIRLAMA_YOK" "$OUT/gdb.log" || { echo "k1-hata-ayikla: SIFIRLAMA_YOK görülmedi" >&2; HATA=1; }
grep -q "in main ()" "$OUT/gdb.log" || { echo "k1-hata-ayikla: yığın izi main'e ulaşmadı" >&2; HATA=1; }
LC_ALL=C grep -q "B MP1a.*iwdgrst=1" "$OUT/gdb-seri.csv" || { echo "k1-hata-ayikla: karşı denemede IWDG sıfırlaması görülmedi" >&2; HATA=1; }
echo "k1-hata-ayikla çıkış kodu: $HATA (gdb=$GRC kayıt=$KRC)"
exit "$HATA"
