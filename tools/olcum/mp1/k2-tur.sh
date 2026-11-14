#!/bin/sh
# W0-MP1a K2 (kısa tur): verilen regülatör varyantını yükler, açılıştan 3 s sonra seri kaydı başlatır,
# SURE saniye kaydeder ve DWT/PC oranını çözümler. Kayıt sırasında B (açılış) ya da E (hata) satırı
# görülürse tur hatalıdır.
# Kullanım: tools/olcum/mp1/k2-tur.sh smps|ldo [süre_s=630] [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
V="${1:?varyant: smps|ldo}"
SURE="${2:-630}"
PORT="${3:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
OUT="$MP1_VERI/k2"
mkdir -p "$OUT"
cmake --build --preset mp1-pc >/dev/null 2>&1 || (cd "$KOK" && cmake --preset mp1-pc >/dev/null && cmake --build --preset mp1-pc >/dev/null)
DAMGA=$(date +%Y%m%d-%H%M%S)
echo "== $V $DAMGA süre=${SURE}s port=$PORT" | tee "$OUT/$V-$DAMGA-analiz.txt"
shasum -a 256 "$KOK/derleme/fw/mp1_$V.elf" | tee -a "$OUT/$V-$DAMGA-analiz.txt"
sh "$KOK/yukle.sh" "$V" >> "$OUT/$V-$DAMGA-yukle.log" 2>&1
sleep 3
# Bağlam için: sistem saatinin NTP sunucusuna göre farkı (yalnız okuma; saat ayarlanmaz). Belirsizliği
# ±0,1 s mertebesindedir; ppm düzeyinde kalibrasyon için kullanılmaz.
echo "sntp önce: $(date +%T) $(sntp time.apple.com 2>/dev/null | tail -n 1)" >> "$OUT/$V-$DAMGA-analiz.txt" || true
"$KOK/derleme/pc/mp1_seri" kayit "$PORT" "$SURE" "$OUT/$V-$DAMGA.csv" > "$OUT/$V-$DAMGA-seri.txt" 2>&1
echo "sntp sonra: $(date +%T) $(sntp time.apple.com 2>/dev/null | tail -n 1)" >> "$OUT/$V-$DAMGA-analiz.txt" || true
"$KOK/derleme/pc/mp1_seri" analiz "$OUT/$V-$DAMGA.csv" | tee -a "$OUT/$V-$DAMGA-analiz.txt"
echo "--- K ve P satırları (ilk ve son)" | tee -a "$OUT/$V-$DAMGA-analiz.txt"
grep -E ',(K|P) ' "$OUT/$V-$DAMGA.csv" | sed -n '1,2p;$p' | cut -d, -f3 | tee -a "$OUT/$V-$DAMGA-analiz.txt"
