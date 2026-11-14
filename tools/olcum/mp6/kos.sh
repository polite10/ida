#!/bin/bash
# MP6a tek koşu: bir yol x bir pencere durumu.
# Kullanım: kos.sh <yol a|b> <durum gorunur|ortulu|gizli|simge|tamekran|basliksiz> <sure_s> <cikti_dizini> [ek kullanıcı arg...]
# Ek argümanlar Godot'ya "--" sonrasında aktarılır (ör. --cozunurluk=1280x720 --geri=async --ornek=0).
# Sistem ayarına dokunmaz; açtığı bütün süreçleri çıkışta kapatır.
set -u

YOL=$1
DURUM=$2
SURE=$3
CIKTI=$4
shift 4
case "$CIKTI" in
  /*) ;;
  *) echo "HATA: cikti dizini mutlak yol olmali: $CIKTI" >&2; exit 2 ;;
esac

BURADA=$(cd "$(dirname "$0")" && pwd)
PROJE="$BURADA/proje"
GODOT=${GODOT:-${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/app/Godot.app/Contents/MacOS/Godot}
ISINMA=${ISINMA:-8}

mkdir -p "$CIKTI"
OLCUM_PID=""
ORTU_PID=""

temizle() {
  if [ -n "$ORTU_PID" ] && kill -0 "$ORTU_PID" 2>/dev/null; then kill "$ORTU_PID" 2>/dev/null; fi
  if [ -n "$OLCUM_PID" ] && kill -0 "$OLCUM_PID" 2>/dev/null; then kill "$OLCUM_PID" 2>/dev/null; fi
  sleep 1
  if [ -n "$ORTU_PID" ] && kill -0 "$ORTU_PID" 2>/dev/null; then kill -9 "$ORTU_PID" 2>/dev/null; fi
  if [ -n "$OLCUM_PID" ] && kill -0 "$OLCUM_PID" 2>/dev/null; then kill -9 "$OLCUM_PID" 2>/dev/null; fi
}
trap temizle EXIT INT TERM

PENCERE_ARG=(--position 160,160 --resolution 480x360)
if [ "$DURUM" = "gorunur" ]; then PENCERE_ARG+=(--always-on-top); fi
if [ "$DURUM" = "basliksiz" ]; then PENCERE_ARG=(--headless); fi

echo "baslangic $(date '+%Y-%m-%dT%H:%M:%S%z') yol=$YOL durum=$DURUM sure=$SURE" > "$CIKTI/kos.log"
"$GODOT" --path "$PROJE" "${PENCERE_ARG[@]}" -- --yol="$YOL" --durum="$DURUM" --sure="$SURE" \
  --isinma="$ISINMA" --cikti="$CIKTI" "$@" > "$CIKTI/godot.log" 2>&1 &
OLCUM_PID=$!
echo "olcum_pid $OLCUM_PID" >> "$CIKTI/kos.log"

# Isınmanın bitmesini bekle (shader derlemesi dahil; en çok 180 s).
for _ in $(seq 1 360); do
  [ -f "$CIKTI/hazir" ] && break
  kill -0 "$OLCUM_PID" 2>/dev/null || break
  sleep 0.5
done
if [ ! -f "$CIKTI/hazir" ]; then
  echo "HATA: hazir dosyasi gelmedi" >> "$CIKTI/kos.log"
  exit 1
fi
echo "hazir $(date '+%H:%M:%S')" >> "$CIKTI/kos.log"

case "$DURUM" in
  ortulu)
    # Başka bir süreçteki her zaman üstte pencere ölçüm penceresini tamamen örter.
    "$GODOT" --path "$PROJE" --always-on-top --position 40,40 --resolution 1100x900 -- \
      --rol=ortu --sure=$((SURE + 120)) > "$CIKTI/ortu.log" 2>&1 &
    ORTU_PID=$!
    ;;
  tamekran)
    "$GODOT" --path "$PROJE" --fullscreen -- --rol=ortu --sure=$((SURE + 120)) > "$CIKTI/ortu.log" 2>&1 &
    ORTU_PID=$!
    ;;
  gizli)
    osascript -l JavaScript "$BURADA/pencere.js" gizle "$OLCUM_PID" >> "$CIKTI/kos.log" 2>&1
    ;;
esac
echo "ortu_pid ${ORTU_PID:-yok}" >> "$CIKTI/kos.log"

# İzleme: her 15 s'de pencere durumu ve süreç önceliği.
SON=$(( $(date +%s) + SURE + 120 ))
while kill -0 "$OLCUM_PID" 2>/dev/null; do
  if [ "$DURUM" != "basliksiz" ]; then
    osascript -l JavaScript "$BURADA/pencere.js" durum "$OLCUM_PID" >> "$CIKTI/pencere.jsonl" 2>>"$CIKTI/kos.log"
  fi
  ps -o pid=,pri=,nice=,%cpu=,stat= -p "$OLCUM_PID" | sed "s/^/$(date +%s) /" >> "$CIKTI/ps.log"
  if [ "$(date +%s)" -gt "$SON" ]; then
    echo "HATA: sure asildi, surec kapatiliyor" >> "$CIKTI/kos.log"
    break
  fi
  for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    kill -0 "$OLCUM_PID" 2>/dev/null || break
    sleep 1
  done
done
wait "$OLCUM_PID" 2>/dev/null
echo "cikis_kodu $? $(date '+%H:%M:%S')" >> "$CIKTI/kos.log"
