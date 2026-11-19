#!/bin/sh
# W0-MP3a: kart koşusu sarmalayıcısı. `caffeinate -dims` altında kendini yeniden başlatır; seri portun ve ST-LINK'in
# başka süreçte açık olmadığını denetler; ortamı (OS, makine, güç, ST-LINK USB kaydı, kaynak durumu, imaj SHA-256 ve
# kimliği) kaydeder; kartın KIMLIK yanıtındaki derleme kimliği yerel derlemeyle aynı değilse koşmaz.
# Ham veri: $IDA_CACHE/mp3/kos/<damga>-<ad>/ (yeni dizin; var olan koşu ezilmez).
# Kullanım:
#   tools/olcum/mp3/kos.sh kimlik
#   tools/olcum/mp3/kos.sh tara [--baudlar ...] [--akis-sn S] [--echo-sn S]
#   tools/olcum/mp3/kos.sh kos <ad> --mod akis|echo --baud B --yuk N --sure S [--hiz H] [--en-az-sn S] [--kampanya]
# Ortam: MP3_PORT (varsayılan: ilk /dev/cu.usbmodem*)
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${MP3_CAFFEINATE:-}" ]; then
    MP3_CAFFEINATE=1 exec caffeinate -dims sh "$KOK/kos.sh" "$@"
fi
. "$KOK/ortam.sh"
ARAC="$KOK/derleme/pc/mp3_vcp"
KOM="${1:?komut: kimlik|tara|kos}"; shift
PORT="${MP3_PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
[ -n "$PORT" ] || { echo "kos.sh: seri port yok" >&2; exit 4; }
[ -x "$ARAC" ] && [ -f "$KOK/derleme/fw/kimlik.txt" ] || { echo "kos.sh: önce derle.sh" >&2; exit 4; }
if lsof "$PORT" > /dev/null 2>&1; then echo "kos.sh: $PORT başka süreçte açık:" >&2; lsof "$PORT" >&2; exit 4; fi
if pgrep -fl "STM32_Programmer_CLI|mp3_vcp |mp4_seri|ST-LINK_gdbserver|openocd" > /dev/null 2>&1; then
    echo "kos.sh: ST-LINK/seri port kullanan başka süreç var:" >&2
    pgrep -fl "STM32_Programmer_CLI|mp3_vcp |mp4_seri|ST-LINK_gdbserver|openocd" >&2; exit 4
fi
if [ "$KOM" = kimlik ]; then exec "$ARAC" kimlik "$PORT"; fi
case "$KOM" in
    tara) AD=tara ;;
    kos) AD="${1:?koşu adı}"; shift ;;
    *) echo "kos.sh: bilinmeyen komut $KOM" >&2; exit 4 ;;
esac
mkdir -p "$MP3_VERI/kos"
OUT="$MP3_VERI/kos/$(date +%Y%m%d-%H%M%S)-$AD"
mkdir "$OUT"
YEREL_KIMLIK=$(cat "$KOK/derleme/fw/kimlik.txt")
KART=$("$ARAC" kimlik "$PORT" 2>&1) || { echo "kos.sh: kart KIMLIK yanıtı yok: $KART" | tee "$OUT/HATA" >&2; exit 2; }
KART_KIMLIK=$(echo "$KART" | tr ' ' '\n' | sed -n 's/^kimlik=//p')
{
  echo "komut: tools/olcum/mp3/kos.sh $KOM $AD $*"
  echo "başlangıç: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "makine: $(sysctl -n machdep.cpu.brand_string), $(sw_vers -productName) $(sw_vers -productVersion) ($(sw_vers -buildVersion)), $(uname -m)"
  echo "port: $PORT"
  echo "ST-LINK USB: $(ioreg -p IOUSB -l -w0 2>/dev/null | grep -E '"(USB Product Name|USB Serial Number|bcdDevice|Device Speed)"' | grep -A3 -i stlink | tr -s ' \t' ' ' | tr '\n' ';' | cut -c1-400)"
  echo "güç: $(pmset -g batt | tr '\n' ' ')"
  echo "uyku engeli: $(pmset -g assertions | grep -c caffeinate || true) caffeinate kaydı"
  echo "kaynak: $(git -C "$KOK" rev-parse HEAD), işlenmemiş: $(git -C "$KOK" status --porcelain -- . | wc -l | tr -d ' ') dosya"
  echo "imaj: derleme/fw/mp3.elf SHA-256 $(shasum -a 256 "$KOK/derleme/fw/mp3.elf" | cut -d' ' -f1), kimlik $YEREL_KIMLIK"
  echo "PC aracı: derleme/pc/mp3_vcp SHA-256 $(shasum -a 256 "$ARAC" | cut -d' ' -f1)"
  echo "kart KIMLIK: $KART"
} > "$OUT/ortam.txt"
if [ "$KART_KIMLIK" != "$YEREL_KIMLIK" ]; then
    echo "kos.sh: kartın imajı yerel derleme değil ($KART_KIMLIK != $YEREL_KIMLIK); koşu yapılmadı" | tee "$OUT/HATA" >&2
    exit 4
fi
cat "$OUT/ortam.txt"
# Araç çıkış kodu doğrudan alınır (boru hattı yok; `tee` çıkış kodunu örtmez). tara: 0 hepsi geçti, 1 tarama
# tamamlandı ama en az bir aday geçmedi/başlamadı, 2 en az bir koşu geçersiz, 4 kullanım/ortam hatası.
case "$KOM" in
    tara) if "$ARAC" tara "$PORT" --dizin "$OUT" "$@" > "$OUT/cikti.txt" 2>&1; then rc=0; else rc=$?; fi ;;
    kos)  if "$ARAC" kos "$PORT" --kayit "$OUT/$AD.kayit" "$@" > "$OUT/cikti.txt" 2>&1; then rc=0; else rc=$?; fi ;;
esac
cat "$OUT/cikti.txt"
{
  echo "bitiş: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "çıkış kodu: $rc"
  echo "imaj sonda: SHA-256 $(shasum -a 256 "$KOK/derleme/fw/mp3.elf" | cut -d' ' -f1)"
  echo "kart KIMLIK sonda: $("$ARAC" kimlik "$PORT" 2>&1 | tr '\n' ' ')"
  echo "güç sonda: $(pmset -g batt | tr '\n' ' ')"
} >> "$OUT/ortam.txt"
tail -n 5 "$OUT/ortam.txt"
echo "kos.sh: ham veri $OUT"
exit "$rc"
