#!/bin/bash
# W3-viz kabul md. 4 (Godot ile): sahte kaynak çalışırken görselleştirici bağlanır; kaynak önce
# habersiz öldürülür (SIGKILL), sonra yeniden başlatılır; ardından olağan kapanır (SIGTERM →
# veda(sim_kapaniyor)) ve yine başlatılır. Görselleştirici her seferinde çökmeden yeniden
# bağlanmalı ve sahneyi yeniden kurmalıdır.
#
#   viz/test/yeniden_baglanma_godot.sh [derleme_dizini]
# Ortam: GODOT (yoksa ${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/...); başsız (--headless) koşar.
set -u
BURADA=$(cd "$(dirname "$0")" && pwd)
KOK=$(cd "$BURADA/../.." && pwd)
DERLEME=${1:-$KOK/build/viz}
GODOT=${GODOT:-${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/app/Godot.app/Contents/MacOS/Godot}
PORT=$((47400 + RANDOM % 400))
GUNLUK=$(mktemp -d)
SAHTE="$DERLEME/viz/ida_sahte_a2"
MANIFEST="$KOK/assets/acik_su/manifest.json"

sahte_baslat() { "$SAHTE" --manifest "$MANIFEST" --a2-port "$PORT" --sessiz >> "$GUNLUK/sahte.log" 2>&1 & SP=$!; }

sahte_baslat
"$GODOT" --headless --path "$KOK/viz/proje" -- --a2-port "$PORT" > "$GUNLUK/godot.log" 2>&1 &
GP=$!
sleep 6
kill -9 "$SP"                  # 1) habersiz ölüm (çökme benzeri)
sleep 4
kill -0 "$GP" 2>/dev/null || { echo "BAŞARISIZ: görselleştirici kaynak ölünce kapandı"; exit 1; }
sahte_baslat                   # 2) yeniden başlat
sleep 6
kill -TERM "$SP"; wait "$SP" 2>/dev/null   # 3) olağan kapanış: veda(sim_kapaniyor)
sleep 3
sahte_baslat                   # 4) yeniden başlat
sleep 6
canli=0; kill -0 "$GP" 2>/dev/null && canli=1
kill "$GP" 2>/dev/null; kill "$SP" 2>/dev/null
sleep 1; kill -9 "$GP" 2>/dev/null
kurulum=$(grep -c 'sahne kuruldu: durum 0' "$GUNLUK/godot.log")
bagli=$(grep -c 'bağlantı: bağlı' "$GUNLUK/godot.log")
cokme=$(grep -c 'handle_crash\|Program crashed' "$GUNLUK/godot.log")
echo "görselleştirici canlı: $canli, sahne kurulumu: $kurulum, bağlanma: $bagli, çökme izi: $cokme (günlük: $GUNLUK)"
if [ "$canli" = 1 ] && [ "$kurulum" -ge 3 ] && [ "$bagli" -ge 3 ] && [ "$cokme" = 0 ]; then
  echo "GEÇTİ"
  exit 0
fi
echo "BAŞARISIZ"
exit 1
