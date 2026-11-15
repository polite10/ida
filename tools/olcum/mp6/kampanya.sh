#!/bin/bash
# MP6a tam ölçüm kampanyası (Mac). Sıralı koşar; her koşu kendi süreçlerini kapatır.
# Kullanım: kampanya.sh <kok_dizin> [SURE=300] [OKUMA_SURE=120]
# Ham veri <kok_dizin> altına yazılır; depoya girmez.
set -u
KOK=$1
SURE=${SURE:-300}
OKUMA_SURE=${OKUMA_SURE:-120}
BURADA=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$KOK"

{
  echo "tarih $(date '+%Y-%m-%dT%H:%M:%S%z')"
  sw_vers
  sysctl -n machdep.cpu.brand_string
  sysctl -n hw.memsize
  pmset -g | grep -Ei "lowpowermode|powermode|displaysleep|^ sleep" || true
  pmset -g batt | head -2
} > "$KOK/ortam.txt" 2>&1

kos() {
  # kos <ad> <yol> <durum> <sure> [ek arg...]
  local ad=$1 y=$2 d=$3 s=$4; shift 4
  echo "$(date '+%H:%M:%S') basla $ad" >> "$KOK/kampanya.log"
  "$BURADA/kos.sh" "$y" "$d" "$s" "$KOK/$ad" "$@"
  echo "$(date '+%H:%M:%S') bitti $ad" >> "$KOK/kampanya.log"
  sleep 3
}

# 1) Pencere durumu x yol (eşzamanlı get_image, 640x480, her 30. kare dökülür).
for Y in a b; do
  for D in gorunur ortulu gizli simge tamekran; do
    kos "${Y}_${D}" "$Y" "$D" "$SURE"
  done
done

# 2) Geri okuma maliyeti (görünür pencere, yol b; ikinci okuma GPU boştayken aktarımı ayırır).
for C in 640x480 1280x720; do
  for G in esz rdesz async; do
    kos "okuma_${C}_${G}" b gorunur "$OKUMA_SURE" --cozunurluk="$C" --geri="$G" --cift=1 --ornek=0
  done
done

# 3) Gizli pencerede asenkron geri okuma kuyruğu force_draw ile ilerliyor mu.
kos "okuma_640x480_async_gizli" b gizli "$OKUMA_SURE" --geri=async --ornek=0

# 4) Başsız kip.
ISINMA=3 kos a_basliksiz a basliksiz 20
ISINMA=3 kos b_basliksiz b basliksiz 20

echo "$(date '+%H:%M:%S') kampanya bitti" >> "$KOK/kampanya.log"
