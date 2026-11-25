#!/bin/bash
# Görselleştiriciyi tek komutta açar (macOS): sahte A2 kaynağıyla (W3-viz kapsam md. 7) ya da
# `--sim` ile gerçek simülasyonla (W5-bagla-a2 kapsam 7).
#
#   viz/calistir.sh [--kademe sade|tam] [--a2-port <n>] [--cozunurluk <G>x<Y>] [--derle]
#                   [--yalniz-sahte] [-- <görselleştiriciye ek seçenekler>]
#   viz/calistir.sh --sim <senaryo.toml> [--hiz <çarpan>] [--kok <dizin>] [--a2-port <n>]
#                   [--cozunurluk <G>x<Y>] [--derle] [-- <görselleştiriciye ek seçenekler>]
#
# --sim kipi: `ida_sim --senaryo <senaryo> --a2-port <n> --sil-hiz <çarpan>` başlatılır (SIL;
# çarpan varsayılanı 1 = gerçek zaman), A2 dinleyicisinin açıldığı sim'in günlüğünden görülünce
# Godot aynı porta bağlanır. Pencerede "SAHTE VERİ" etiketi görünmez (veri fizik çekirdeğindendir).
# Senaryo yolu depo köküne göre görelidir (`--kok` ile başka bir kök verilebilir); senaryonun
# varlık kümesinin (`varliklar.kume`) ikili dosyaları bulunmalıdır (`assets/ornek` kümesinin .glb
# dosyaları yoktur: görselleştirici onu yükleyemez). Görünüm kademesi senaryonun
# `kalite.gorunum` alanındandır (`--kademe` bu kipte kullanılmaz). Pencere kapanınca sim düzgün
# kapatılır (SIGINT) ve kapanış özeti yazdırılır; senaryo süresi dolarsa sim kendiliğinden biter
# ve pencere son sahneyle açık kalır.
#
# Ortam değişkenleri:
#   GODOT            Godot 4.7.2 ikilisi. Verilmezse ${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/
#                    altındaki resmî ikili denenir (MP6 ölçümünün kurulum yeri).
#   IDA_VIZ_DERLEME  derleme dizini (varsayılan <depo>/build/viz; IDA_VIZ=ON, Release)
#   IDA_SIM          --sim: `ida_sim` ikilisi. Verilmezse <depo>/build/host-release, sonra
#                    host-debug altındaki denenir (derleme: sim/sunucu/BENIOKU.md).
#   IDA_SIM_SECENEKLER  --sim: ida_sim'e ek seçenekler (boşlukla ayrılır; ör. "--sonar" ya da
#                    "--a3-port 47100"; scenarios/sonar-havuz.toml `--sonar` ister)
#
# Eklenti ve sahte kaynak derlenmemişse (ya da --derle verilirse) önce derlenir (--sim kipinde
# yalnız eklenti); Godot projesi hiç içe aktarılmamışsa (.godot/ yok) bir kez başsız içe
# aktarılır. Pencere kapanınca sahte kaynak da kapatılır. Windows eşdeğeri sonraki dilimde.
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
SIM_SENARYO=""
SIM_HIZ=1
SIM_KOK=$KOK
while [ $# -gt 0 ]; do
  case "$1" in
    --sim) SIM_SENARYO=$2; shift 2 ;;
    --hiz) SIM_HIZ=$2; shift 2 ;;
    --kok) SIM_KOK=$2; shift 2 ;;
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

if [ -n "$SIM_SENARYO" ]; then
  # --sim: yalnız eklenti gerekir (sahte kaynak kullanılmaz).
  if [ "$YALNIZ_SAHTE" = 1 ]; then
    echo "calistir.sh: --yalniz-sahte --sim ile birlikte kullanılamaz" >&2
    exit 2
  fi
  if [ "$DERLE" = 1 ] || [ ! -f "$PROJE/bin/libida_viz.dylib" ]; then
    echo "calistir.sh: eklenti derleniyor ($DERLEME)"
    cmake -S "$KOK" -B "$DERLEME" -G Ninja -DIDA_HEDEF=host -DCMAKE_BUILD_TYPE=Release -DIDA_VIZ=ON > /dev/null
    cmake --build "$DERLEME" --target ida_viz_eklenti
  fi
elif [ "$DERLE" = 1 ] || [ ! -f "$PROJE/bin/libida_viz.dylib" ] || [ ! -x "$DERLEME/viz/ida_sahte_a2" ]; then
  echo "calistir.sh: eklenti ve sahte kaynak derleniyor ($DERLEME)"
  cmake -S "$KOK" -B "$DERLEME" -G Ninja -DIDA_HEDEF=host -DCMAKE_BUILD_TYPE=Release -DIDA_VIZ=ON > /dev/null
  cmake --build "$DERLEME" --target ida_viz_eklenti ida_sahte_a2
fi

if [ ! -f "$PROJE/.godot/extension_list.cfg" ]; then
  echo "calistir.sh: Godot projesi ilk kez içe aktarılıyor"
  "$GODOT" --headless --path "$PROJE" --import > /dev/null 2>&1 || true
fi

if [ -n "$SIM_SENARYO" ]; then
  # ------------------------------------------------------------------ gerçek sim (W5-bagla-a2)
  SIM=${IDA_SIM:-}
  if [ -z "$SIM" ]; then
    for d in host-release host-debug; do
      if [ -x "$KOK/build/$d/sim/sunucu/ida_sim" ]; then SIM="$KOK/build/$d/sim/sunucu/ida_sim"; break; fi
    done
  fi
  if [ -z "$SIM" ] || [ ! -x "$SIM" ]; then
    echo "calistir.sh: ida_sim ikilisi bulunamadı (build/host-release ya da build/host-debug)." >&2
    echo "  Derleyin (cmake --preset host-release && cmake --build --preset host-release --target ida_sim)" >&2
    echo "  ya da IDA_SIM ortam değişkeniyle yolu verin." >&2
    exit 1
  fi
  GUNLUK=$(mktemp "${TMPDIR:-/tmp}/ida_sim_a2.XXXXXX")
  SIM_PID=""
  sim_kapat() {
    if [ -n "$SIM_PID" ] && kill -0 "$SIM_PID" 2>/dev/null; then
      kill -INT "$SIM_PID" 2>/dev/null || true   # düzgün kapanış (kapanış çerçevesi ve özet satırı)
      wait "$SIM_PID" 2>/dev/null || true
    fi
    SIM_PID=""
  }
  trap sim_kapat EXIT INT TERM
  # shellcheck disable=SC2086  # IDA_SIM_SECENEKLER bilerek sözcüklere ayrılır
  "$SIM" --senaryo "$SIM_SENARYO" --kok "$SIM_KOK" --a2-port "$PORT" --sil-hiz "$SIM_HIZ" --gunluk 1 \
    ${IDA_SIM_SECENEKLER:-} 2> "$GUNLUK" &
  SIM_PID=$!
  # Hazır olma işareti: sim'in günlüğündeki "A2 127.0.0.1:<port> adresinde dinliyor" satırı (sabit
  # uyku yok; en çok 60 s yoklanır). A2 açılamadıysa ya da sim bittiyse nedeni günlüktedir.
  I=0
  until grep -q "A2 127.0.0.1:$PORT adresinde dinliyor" "$GUNLUK" 2>/dev/null; do
    if ! kill -0 "$SIM_PID" 2>/dev/null || grep -q "A2'siz çalışılıyor\|dinleyici açılmadı" "$GUNLUK" 2>/dev/null \
       || [ "$I" -ge 600 ]; then
      echo "calistir.sh: ida_sim A2 dinleyicisini açmadı; sim günlüğü ($GUNLUK):" >&2
      cat "$GUNLUK" >&2
      exit 1
    fi
    sleep 0.1
    I=$((I + 1))
  done
  echo "calistir.sh: ida_sim çalışıyor (senaryo $SIM_SENARYO, hız $SIM_HIZ, A2 127.0.0.1:$PORT; günlük $GUNLUK)"
  KOD=0
  "$GODOT" --path "$PROJE" --resolution "$COZ" -- --a2-port "$PORT" "$@" || KOD=$?
  sim_kapat
  echo "calistir.sh: sim günlüğü ($GUNLUK):"
  cat "$GUNLUK"
  exit "$KOD"
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
