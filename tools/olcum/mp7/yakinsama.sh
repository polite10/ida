#!/usr/bin/env bash
# W0-MP7a yakınsama koşuları, düzeltme turu (mimar tanımı): orta sahne, M = 32, 8 açı,
# kutu kümesi bağımsız referanstan (Nref = en büyük N, tohum 5000..). Gürültü kapalı.
# Çıktı: veri/yakinsama2_*.csv|txt (depoya girmez; özetleri sonuc/ altına kopyalanır).
set -euo pipefail
kok="$(cd "$(dirname "$0")" && pwd)"
bin="$kok/derleme/mp7"
veri="$kok/veri"
mkdir -p "$veri"
L="${L:-625,1250,2500,5000,10000,20000,40000,80000,160000,320000,640000,1280000,2560000}"
IS="${IS:-4}"

kos() {  # ad, sonra mp7 seçenekleri
  local ad="$1"; shift
  echo "== $ad: $*"
  "$bin" yakinsama --sahne 1e5 --N "$L" --M 32 --acilar 8 --is "$IS" --cikti "$veri/yakinsama2_$ad.csv" "$@" \
    2>"$veri/yakinsama2_$ad.err" | tee "$veri/yakinsama2_$ad.txt"
}

secim="${1:-hepsi}"
if [[ "$secim" == "hepsi" || "$secim" == "tam" ]]; then
  kos 7m_tam_D2 --menzil 7 --kademe tam --D 2
  kos 50m_tam_D2 --menzil 50 --kademe tam --D 2
fi
if [[ "$secim" == "hepsi" || "$secim" == "sade" ]]; then
  kos 7m_sade --menzil 7 --kademe sade --D 0 --benek 0
  kos 7m_sade_benek --menzil 7 --kademe sade --D 0 --benek 1
fi
