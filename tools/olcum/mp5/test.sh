#!/bin/sh
# W0-MP5a iyileştirme turu: davranış eşitliği. Ürünün birim testleri ve bağımsız kabul testleri,
# ürün kaynağı ve kopya/cesitler.txt'deki her tel_iyi.c çeşidi bağlanarak host'ta derlenir ve koşar.
# Ürün kökü MP5_URUN_KOK (yoksa bu çalışma kopyası); kökün host-debug derlemesi önceden yapılmış olmalı.
# Kullanım: tools/olcum/mp5/test.sh            Çıktı: $MP5_VERI/test-<damga>.txt
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
URUN="${MP5_URUN_KOK:-$(cd "$KOK/../../.." && pwd)}"
CES="urun=$URUN/protocol/ida_link/src/tel.c|"
while read -r ad tanim; do
    case "$ad" in ''|'#'*) continue ;; esac
    CES="$CES;$ad=kopya/tel_iyi.c|$tanim"
done < "$KOK/kopya/cesitler.txt"
D="$KOK/derleme/test"
mkdir -p "$MP5_VERI"
OUT="$MP5_VERI/test-$(date +%Y%m%d-%H%M%S).txt"
cmake -S "$KOK" -B "$D" -G Ninja -DMP5_HEDEF=test -DCMAKE_BUILD_TYPE=Debug -DIDA_KOK="$URUN" \
      "-DMP5_TEST_CESITLERI=$CES" > "$OUT.yap" 2>&1 || { cat "$OUT.yap"; exit 1; }
cmake --build "$D" > "$OUT.der" 2>&1 || { tail -40 "$OUT.der"; exit 1; }
{
  echo "ürün kökü: $URUN ($(git -C "$URUN" rev-parse --short HEAD 2>/dev/null || echo '?'))"
  ( cd "$D" && ctest --output-on-failure ) || true
} > "$OUT" 2>&1
grep -E "tests passed|tests failed|\(Failed\)|Failed  " "$OUT" || true
echo "test.sh: ayrıntı $OUT"
