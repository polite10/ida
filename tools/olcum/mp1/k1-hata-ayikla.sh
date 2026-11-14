#!/bin/sh
# W0-MP1a K1: ST-LINK_gdbserver + arm-none-eabi-gdb toplu kipte kesme noktası, yığın izi ve IWDG dondurma.
# Kart çalışan mp1_smps imajıyla bağlıdır (k1-derle-yukle.sh). gdbserver çalışan hedefe bağlanır (-g).
# Kullanım: tools/olcum/mp1/k1-hata-ayikla.sh [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
OUT="$MP1_VERI/k1"
mkdir -p "$OUT"

"$KOK/derleme/pc/mp1_seri" kayit "$PORT" 75 "$OUT/gdb-seri.csv" > "$OUT/gdb-seri.txt" 2>&1 &
KP=$!
"$GDBSRV" -p 61234 -l 1 -d -g -cp "$CUBEPROG_BIN" > "$OUT/gdbserver.log" 2>&1 &
GP=$!
sleep 3
"$GDB" -batch -x "$KOK/gdb/k1-iwdg.gdb" "$KOK/derleme/fw/mp1_smps.elf" 2>&1 | tee "$OUT/gdb.log"
sleep 2
kill "$GP" 2>/dev/null || true
wait "$KP" || true
echo "--- seri (T dışı satırlar)"
cat "$OUT/gdb-seri.txt"
echo "--- seri T satırları, gdb oturumu boyunca"
grep ',T ' "$OUT/gdb-seri.csv" | awk -F, 'NR==1{t0=$1} {printf "%8.3f s  %s\n", ($1-t0)/1e9, $3}'
