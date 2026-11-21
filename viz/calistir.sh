#!/bin/bash
# Görselleştiriciyi sahte A2 kaynağıyla tek komutta açar (W3-viz kapsam md. 7; macOS).
#
#   viz/calistir.sh [--kademe sade|tam] [--a2-port <n>] [--cozunurluk <G>x<Y>] [--derle]
#                   [--yalniz-sahte] [-- <görselleştiriciye ek seçenekler>]
#
# Ortam değişkenleri:
#   GODOT            Godot 4.7.2 ikilisi. Verilmezse ${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/
#                    altındaki resmî ikili denenir (MP6 ölçümünün kurulum yeri).
#   IDA_VIZ_DERLEME  derleme dizini (varsayılan <depo>/build/viz; IDA_VIZ=ON, Release)
#
# Eklenti ve sahte kaynak derlenmemişse (ya da --derle verilirse) önce derlenir; Godot
# projesi hiç içe aktarılmamışsa (.godot/ yok) bir kez başsız içe aktarılır. Pencere
# kapanınca sahte kaynak da kapatılır. Windows eşdeğeri sonraki dilimde.
set -euo pipefail

BURADA=$(cd "$(dirname "$0")" && pwd)
KOK=$(cd "$BURADA/.." && pwd)
PROJE="$BURADA/proje"
DERLEME=${IDA_VIZ_DERLEME:-$KOK/build/viz}
GODOT=${GODOT:-${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/app/Godot.app/Contents/MacOS/Godot}

KADEME=tam
PORT=47210
COZ=1920x1080
DERLE=0
YALNIZ_SAHTE=0
while [ $# -gt 0 ]; do
  case "$1" in
    --kademe) KADEME=$2; shift 2 ;;
    --a2-port) PORT=$2; shift 2 ;;
    --cozunurluk) COZ=$2; shift 2 ;;
    --derle) DERLE=1; shift ;;
    --yalniz-sahte) YALNIZ_SAHTE=1; shift ;;
    --) shift; break ;;
    *) echo "calistir.sh: bilinmeyen seçenek: $1" >&2; exit 2 ;;
  esac
done

if [ ! -x "$GODOT" ]; then
  echo "calistir.sh: Godot 4.7.2 ikilisi bulunamadı: $GODOT" >&2
  echo "  GODOT ortam değişkeniyle yolu verin." >&2
  exit 1
fi

if [ "$DERLE" = 1 ] || [ ! -f "$PROJE/bin/libida_viz.dylib" ] || [ ! -x "$DERLEME/viz/ida_sahte_a2" ]; then
  echo "calistir.sh: eklenti ve sahte kaynak derleniyor ($DERLEME)"
  cmake -S "$KOK" -B "$DERLEME" -G Ninja -DIDA_HEDEF=host -DCMAKE_BUILD_TYPE=Release -DIDA_VIZ=ON > /dev/null
  cmake --build "$DERLEME" --target ida_viz_eklenti ida_sahte_a2
fi

if [ ! -f "$PROJE/.godot/extension_list.cfg" ]; then
  echo "calistir.sh: Godot projesi ilk kez içe aktarılıyor"
  "$GODOT" --headless --path "$PROJE" --import > /dev/null 2>&1 || true
fi

SAHTE_PID=""
temizle() {
  if [ -n "$SAHTE_PID" ] && kill -0 "$SAHTE_PID" 2>/dev/null; then kill "$SAHTE_PID" 2>/dev/null || true; fi
}
trap temizle EXIT INT TERM

"$DERLEME/viz/ida_sahte_a2" --manifest "$KOK/assets/acik_su/manifest.json" --kademe "$KADEME" --a2-port "$PORT" &
SAHTE_PID=$!
if [ "$YALNIZ_SAHTE" = 1 ]; then
  wait "$SAHTE_PID"
  exit $?
fi
"$GODOT" --path "$PROJE" --resolution "$COZ" -- --a2-port "$PORT" --sahte "$@"
