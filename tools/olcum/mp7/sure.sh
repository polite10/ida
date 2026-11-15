#!/usr/bin/env bash
# W0-MP7a süre taraması, düzeltme turu (daraltılmış ızgara). caffeinate altında:
#   NS3=<S2'nin N'si> caffeinate -dims tools/olcum/mp7/sure.sh
# Çıktı: veri/sure2.csv, veri/sure2_s3.csv, veri/ham2/, veri/sistem2.txt
set -euo pipefail
kok="$(cd "$(dirname "$0")" && pwd)"
bin="$kok/derleme/mp7"
veri="$kok/veri"
mkdir -p "$veri/ham2"
NL="${NL:-1000,2000,5000,10000,20000,40000,80000,160000}"
PING="${PING:-200}"
NS3="${NS3:?S3 icin NS3 verilmeli: S2 sonucundaki N}"

sistem() {
  {
    echo "=== $1 $(date '+%Y-%m-%d %H:%M:%S')"
    sysctl -n vm.loadavg
    pmset -g batt | sed -n '1,2p'
    pgrep -fl Godot.app || echo "Godot yok"
    top -l 2 -n 6 -o cpu -s 1 | grep -E '^(CPU usage|Load Avg)|^ *[0-9]+ ' | tail -8
  } >>"$veri/sistem2.txt"
}

: >"$veri/sistem2.txt"
sistem baslangic
# Orta sahne: tam ızgara (N x D x iş parçacığı), iki menzil
for menzil in 7 50; do
  sistem "tam 1e5 ${menzil}m"
  "$bin" sure --sahne 1e5 --menzil "$menzil" --kademe tam --N "$NL" --D 0,1,2 --is 1,2,4,tumu \
    --ping "$PING" --isinma 10 --cikti "$veri/sure2.csv" --ham "$veri/ham2"
done
# Sahne boyutu etkisi: yalnız N = 20 000 ve 80 000, tek iş parçacığı
for sahne in 1e4 1e6; do
  for menzil in 7 50; do
    sistem "tam $sahne ${menzil}m"
    "$bin" sure --sahne "$sahne" --menzil "$menzil" --kademe tam --N 20000,80000 --D 0,1,2 --is 1 \
      --ping "$PING" --isinma 10 --cikti "$veri/sure2.csv" --ham "$veri/ham2"
  done
done
# Sade kademe (orta sahne)
for menzil in 7 50; do
  sistem "sade 1e5 ${menzil}m"
  "$bin" sure --sahne 1e5 --menzil "$menzil" --kademe sade --N "$NL" --D 0 --is 1,tumu \
    --ping "$PING" --isinma 10 --cikti "$veri/sure2.csv" --ham "$veri/ham2"
done
# S3 hücresi uzun koşu: 1e5, 7 m, D = 2, tek iş parçacığı, 2000 ping
sistem "S3 uzun"
"$bin" sure --sahne 1e5 --menzil 7 --kademe tam --N "$NS3" --D 2 --is 1 --ping 2000 --isinma 10 \
  --cikti "$veri/sure2_s3.csv" --ham "$veri/ham2"
sistem bitis
