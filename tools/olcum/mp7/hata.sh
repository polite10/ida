#!/usr/bin/env bash
# W0-MP7a 2. tur: "azaltılmış ışın sayısının bilinen hatası" tablosu (betimleyici, ölçüt değil)
# ve sade kademenin (05 beneğiyle, ana kod yolu) N = 5 000 süresi.
# Çıktı: veri/hata_*.csv|txt, veri/sure3_sade.csv, veri/sistem3.txt
set -euo pipefail
kok="$(cd "$(dirname "$0")" && pwd)"
bin="$kok/derleme/mp7"
veri="$kok/veri"
mkdir -p "$veri"
IS="${IS:-4}"
{
  echo "=== baslangic $(date '+%Y-%m-%d %H:%M:%S')"
  pmset -g batt | sed -n '1,2p'
  pgrep -fl Godot.app || echo "Godot yok"
  top -l 2 -n 0 -s 1 | grep 'CPU usage' | tail -1
} >"$veri/sistem3.txt"
# Sade kademe süresi (S2'nin N'si), tek iş parçacığı, orta sahne
for menzil in 7 50; do
  "$bin" sure --sahne 1e5 --menzil "$menzil" --kademe sade --N 5000 --D 0 --is 1 --ping 200 --isinma 10 \
    --cikti "$veri/sure3_sade.csv"
done
# Hata tablosu: N = 20 000 ... 160 000 ve eşli referans N = 2 560 000 (M = 32, 8 açı)
for menzil in 7 50; do
  "$bin" yakinsama --sahne 1e5 --menzil "$menzil" --kademe tam --D 2 --N 20000,40000,80000,160000,2560000 \
    --Nref 2560000 --M 32 --acilar 8 --is "$IS" --hata_tablosu 1 --cikti "$veri/hata_${menzil}m.csv" \
    2>"$veri/hata_${menzil}m.err" | tee "$veri/hata_${menzil}m.txt"
done
echo "=== bitis $(date '+%Y-%m-%d %H:%M:%S')" >>"$veri/sistem3.txt"
