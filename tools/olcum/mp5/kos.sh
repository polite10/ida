#!/bin/sh
# W0-MP5a: bir koşu. `caffeinate -dims` altında kendini yeniden başlatır; seri portun ve ST-LINK'in başka
# süreçte açık olmadığını denetler; seri kaydı yüklemeden ÖNCE başlatır (açılış satırları kaybolmasın),
# ölçüm yazılımını yükler (yukle.sh: önce yedek doğrulaması) ve kartın kampanya sonu satırına ("T") dek
# kaydeder (azami süre yalnız güvenlik sınırıdır; T gelmezse koşu EKSİK sayılır). Ham kayıt ve imajın
# kimliği/SHA-256'sı $MP5_VERI/kos/ altına yazılır; ardından ozet.py ham kaydı doğrular.
# Kullanım: tools/olcum/mp5/kos.sh <derleme> <azami_s> [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${MP5_CAFFEINATE:-}" ]; then
    MP5_CAFFEINATE=1 exec caffeinate -dims sh "$KOK/kos.sh" "$@"
fi
. "$KOK/ortam.sh"
V="${1:?derleme adı}"
AZAMI="${2:?azami_s}"
PORT="${3:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
[ -n "$PORT" ] || { echo "kos: seri port yok" >&2; exit 2; }
D="$KOK/derleme/$V"
[ -x "$KOK/derleme/pc/mp5_kayit" ] && [ -f "$D/mp5.elf" ] && [ -f "$D/kimlik.txt" ] || { echo "kos: önce derle.sh" >&2; exit 2; }
KIMLIK=$(cat "$D/kimlik.txt")
if lsof "$PORT" > /dev/null 2>&1; then
    echo "kos: $PORT başka süreçte açık; koşu yapılmadı:" >&2; lsof "$PORT" >&2; exit 2
fi
if pgrep -fl "STM32_Programmer_CLI|mp5_kayit|mp4_seri kayit|ST-LINK_gdbserver|openocd" > /dev/null 2>&1; then
    echo "kos: ST-LINK/seri port kullanan başka süreç var; koşu yapılmadı:" >&2
    pgrep -fl "STM32_Programmer_CLI|mp5_kayit|mp4_seri kayit|ST-LINK_gdbserver|openocd" >&2; exit 2
fi
OUT="$MP5_VERI/kos"
mkdir -p "$OUT"
DAMGA=$(date +%Y%m%d-%H%M%S)
AD="$V-$DAMGA"
sha() { shasum -a 256 "$D/mp5.elf" | cut -d' ' -f1; }
SHA_BAS=$(sha)
cp "$KOK/derleme/uretilen/beklenen.json" "$OUT/$AD-beklenen.json"
{
  echo "== $V $DAMGA azami=${AZAMI}s port=$PORT"
  echo "başlangıç: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "imaj: derleme/$V/mp5.elf SHA-256 $SHA_BAS, kimlik $KIMLIK"
  echo "güç önce: $(pmset -g batt | tr '\n' ' ')"
  echo "yük önce: $(uptime)"
} > "$OUT/$AD-kos.txt"
"$KOK/derleme/pc/mp5_kayit" "$PORT" "$AZAMI" "$OUT/$AD.txt" > "$OUT/$AD-seri.txt" 2>&1 &
KP=$!
sleep 1
if ! sh "$KOK/yukle.sh" "$V" "$OUT/$AD-yukle.log" >> "$OUT/$AD-kos.txt" 2>&1; then
    kill "$KP" 2>/dev/null || true
    echo "kos: yükleme başarısız; koşu yapılmadı" | tee -a "$OUT/$AD-kos.txt" >&2
    cat "$OUT/$AD-kos.txt" >&2
    exit 1
fi
echo "yükleme bitti: $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT/$AD-kos.txt"
if wait "$KP"; then krc=0; else krc=$?; fi
SHA_SON=$(sha)
{
  echo "bitiş: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  tail -n 1 "$OUT/$AD-seri.txt"
  echo "seri kayıt çıkış kodu: $krc (0 = T satırı geldi, 4 = azami sürede gelmedi, 5 = X satırı)"
  echo "imaj sonda: SHA-256 $SHA_SON $( [ "$SHA_BAS" = "$SHA_SON" ] && echo "(başla aynı)" || echo "(DEĞİŞTİ)")"
  echo "yük sonra: $(uptime)"
} >> "$OUT/$AD-kos.txt"
rc=0
[ "$krc" -eq 0 ] || rc=1
[ "$SHA_BAS" = "$SHA_SON" ] || rc=1
if python3 -I "$KOK/ozet.py" denetle "$OUT/$AD.txt" --kimlik "$KIMLIK" --beklenen "$OUT/$AD-beklenen.json" >> "$OUT/$AD-kos.txt" 2>&1; then :; else rc=1; fi
echo "kos $V sonuç çıkış kodu: $rc; ham kayıt $OUT/$AD.txt" >> "$OUT/$AD-kos.txt"
cat "$OUT/$AD-kos.txt"
exit "$rc"
