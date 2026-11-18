#!/bin/sh
# W0-MP4a: bir koşu. `caffeinate -dims` altında kendini yeniden başlatır; seri portun ve ST-LINK'in başka
# süreçte açık olmadığını denetler; seri kaydı yüklemeden ÖNCE başlatır (açılış satırları kaybolmasın),
# aygıt yazılımını yükler (yukle.sh: önce yedek doğrulaması), kartın kampanya sonu satırına ("T") dek kaydeder
# (azami süre yalnız güvenlik sınırıdır; T gelmezse koşu EKSİK sayılır) ve çözümler. Başta ve sonda saat,
# güç durumu ve imajın SHA-256'sı kaydedilir; çözümleyici açılış satırındaki derleme kimliğini denetler.
# Kullanım: tools/olcum/mp4/kos.sh o2|og|o2-200|og-200|kisa <azami_s> [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${MP4_CAFFEINATE:-}" ]; then
    MP4_CAFFEINATE=1 exec caffeinate -dims sh "$KOK/kos.sh" "$@"
fi
. "$KOK/ortam.sh"
V="${1:?derleme: o2|og|o2-200|og-200|kisa}"
AZAMI="${2:?azami_s}"
PORT="${3:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
[ -n "$PORT" ] || { echo "kos: seri port yok" >&2; exit 2; }
D="$KOK/derleme/$V"
[ -x "$KOK/derleme/pc/mp4_seri" ] && [ -f "$D/mp4.elf" ] && [ -f "$D/kimlik.txt" ] || { echo "kos: önce derle.sh" >&2; exit 2; }
KIMLIK=$(cat "$D/kimlik.txt")
# Beklenen kampanya (derleme ön ayarlarıyla aynı; çözümleyici açılış satırını bunlara karşı doğrular, imajın
# bildirdiği maske zorunlu kampanyayı daraltamaz). Süre ölçütü muhafazakâr alt sınırla denetlenir.
case "$V" in
    o2|og)  BEK="--beklenen-hz 400 --beklenen-fazlar 7 --beklenen-faz-adim 241000 --en-az-adim 240000 --en-az-sn 600" ;;
    *-200)  BEK="--beklenen-hz 200 --beklenen-fazlar 2 --beklenen-faz-adim 121000 --en-az-adim 120000 --en-az-sn 600" ;;
    kisa)   BEK="--beklenen-hz 400 --beklenen-fazlar 7 --beklenen-faz-adim 4000" ;;
    *) echo "kos: bilinmeyen derleme: $V" >&2; exit 2 ;;
esac
# Yığın üst sınırı: ölçülen imajın bellek düzeninden (ELF sembolleri), kayıttan bağımsız. Firmware yığını
# _ebss + 256 B'den _estack'e kadar boyar; ölçülebilecek en büyük yığın bu bölgedir.
YIGIN_AZAMI=$("$ARM_GCC_BIN/arm-none-eabi-nm" "$D/mp4.elf" | awk '
  $3=="_ebss" {e=$1} $3=="_estack" {s=$1}
  function h(x,  i,v){v=0; for(i=1;i<=length(x);i++) v=v*16+index("0123456789abcdef",substr(tolower(x),i,1))-1; return v}
  END { if (e=="" || s=="") exit 1; printf "%d\n", h(s) - (h(e) + 256) }') || { echo "kos: ELF'te _ebss/_estack yok" >&2; exit 2; }
[ "$YIGIN_AZAMI" -gt 0 ] || { echo "kos: yığın üst sınırı hesaplanamadı" >&2; exit 2; }
BEK="$BEK --yigin-azami $YIGIN_AZAMI"
# Seri port ya da ST-LINK başka süreçte açık mı?
if lsof "$PORT" > /dev/null 2>&1; then
    echo "kos: $PORT başka süreçte açık; koşu yapılmadı:" >&2; lsof "$PORT" >&2; exit 2
fi
if pgrep -fl "STM32_Programmer_CLI|mp4_seri kayit|ST-LINK_gdbserver|openocd" > /dev/null 2>&1; then
    echo "kos: ST-LINK/seri port kullanan başka süreç var; koşu yapılmadı:" >&2
    pgrep -fl "STM32_Programmer_CLI|mp4_seri kayit|ST-LINK_gdbserver|openocd" >&2; exit 2
fi
OUT="$MP4_VERI/kos"
mkdir -p "$OUT"
DAMGA=$(date +%Y%m%d-%H%M%S)
AN="$OUT/$V-$DAMGA-analiz.txt"
sha() { shasum -a 256 "$D/mp4.elf" | cut -d' ' -f1; }
SHA_BAS=$(sha)
{
  echo "== $V $DAMGA azami=${AZAMI}s port=$PORT"
  echo "başlangıç: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "imaj: derleme/$V/mp4.elf SHA-256 $SHA_BAS, kimlik $KIMLIK"
  echo "güç önce: $(pmset -g batt | tr '\n' ' ')"
  echo "caffeinate: $(pmset -g assertions | grep -i caffeinate | tr -s ' \t' ' ' | tr '\n' ';' || true)"
} > "$AN"
"$KOK/derleme/pc/mp4_seri" kayit "$PORT" "$AZAMI" "$OUT/$V-$DAMGA.csv" > "$OUT/$V-$DAMGA-seri.txt" 2>&1 &
KP=$!
sleep 1
if ! sh "$KOK/yukle.sh" "$V" "$OUT/$V-$DAMGA-yukle.log" >> "$AN" 2>&1; then
    kill "$KP" 2>/dev/null || true
    echo "kos: yükleme başarısız; koşu yapılmadı" | tee -a "$AN" >&2
    exit 1
fi
echo "yükleme bitti: $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$AN"
if wait "$KP"; then krc=0; else krc=$?; fi
SHA_SON=$(sha)
{
  echo "bitiş: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  tail -n 1 "$OUT/$V-$DAMGA-seri.txt"
  echo "seri kayıt çıkış kodu: $krc (0 = T satırı geldi, 4 = azami sürede gelmedi)"
  echo "imaj sonda: SHA-256 $SHA_SON $( [ "$SHA_BAS" = "$SHA_SON" ] && echo "(başla aynı)" || echo "(DEĞİŞTİ)")"
  echo "güç sonra: $(pmset -g batt | tr '\n' ' ')"
} >> "$AN"
# shellcheck disable=SC2086
if "$KOK/derleme/pc/mp4_seri" analiz "$OUT/$V-$DAMGA.csv" --kimlik "$KIMLIK" --hist "$OUT/$V-$DAMGA-hist" $BEK > "$AN.tmp" 2>&1; then rc=0; else rc=$?; fi
cat "$AN.tmp" >> "$AN"; rm -f "$AN.tmp"
[ "$krc" -eq 0 ] || rc=1
[ "$SHA_BAS" = "$SHA_SON" ] || rc=1
echo "kos $V sonuç çıkış kodu: $rc" >> "$AN"
cat "$AN"
exit "$rc"
