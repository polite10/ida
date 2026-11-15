#!/bin/sh
# W0-MP1a K2 (kısa tur): verilen regülatör varyantını yükler, seri kaydı yüklemeden ÖNCE başlatır
# (açılış satırları kaybolmasın; ilk tur bulgusu 6), SURE saniye kaydeder ve çözümler.
# Çözümleyici; ölçüm penceresinde sıfırlama, hata, sıra boşluğu, istenenden farklı REGS, UART hatası ya da
# 600 s'den kısa pencere görürse eğim hesaplamaz ve başarısız döner. Bu betik de o durumda başarısız döner.
# Kullanım: tools/olcum/mp1/k2-tur.sh smps|ldo [süre_s=640] [port]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
V="${1:?varyant: smps|ldo}"
SURE="${2:-640}"
PORT="${3:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
[ -n "$PORT" ] || { echo "k2: seri port yok" >&2; exit 2; }
case "$V" in smps) REG=SMPS ;; ldo) REG=LDO ;; *) echo "k2: varyant smps|ldo" >&2; exit 2 ;; esac
OUT="$MP1_VERI/k2"
mkdir -p "$OUT"
if ! (cd "$KOK" && cmake --build --preset mp1-pc > /dev/null 2>&1); then
    (cd "$KOK" && cmake --preset mp1-pc > /dev/null && cmake --build --preset mp1-pc > /dev/null)
fi
DAMGA=$(date +%Y%m%d-%H%M%S)
AN="$OUT/$V-$DAMGA-analiz.txt"
echo "== $V $DAMGA süre=${SURE}s port=$PORT istenen_reg=$REG" | tee "$AN"
shasum -a 256 "$KOK/derleme/fw/mp1_$V.elf" | sed 's|  .*/|  |' | tee -a "$AN"
# Bağlam için: sistem saatinin NTP sunucusuna göre farkı (yalnız okuma; saat ayarlanmaz; belirsizliği ±0,1 s
# mertebesinde, ppm kalibrasyonu için kullanılmaz).
echo "sntp önce: $(date +%T) $(sntp time.apple.com 2>/dev/null | tail -n 1 || true)" >> "$AN"

"$KOK/derleme/pc/mp1_seri" kayit "$PORT" "$SURE" "$OUT/$V-$DAMGA.csv" > "$OUT/$V-$DAMGA-seri.txt" 2>&1 &
KP=$!
sleep 1
if ! sh "$KOK/yukle.sh" "$V" "$OUT/$V-$DAMGA-yukle.log" >> "$AN" 2>&1; then
    kill "$KP" 2>/dev/null || true
    echo "k2: yükleme başarısız; tur yapılmadı" | tee -a "$AN" >&2
    exit 1
fi
if ! wait "$KP"; then echo "k2: seri kayıt başarısız" | tee -a "$AN" >&2; exit 1; fi
echo "sntp sonra: $(date +%T) $(sntp time.apple.com 2>/dev/null | tail -n 1 || true)" >> "$AN"

if "$KOK/derleme/pc/mp1_seri" analiz "$OUT/$V-$DAMGA.csv" 160000000 "$REG" > "$AN.tmp" 2>&1; then rc=0; else rc=$?; fi
cat "$AN.tmp" >> "$AN"; cat "$AN.tmp"; rm -f "$AN.tmp"
echo "--- B, K ve P satırları (açılış, ilk ve son durum)" >> "$AN"
LC_ALL=C grep -E 'B MP1a|,(K|P) ' "$OUT/$V-$DAMGA.csv" | sed -n '1,3p;$p' | cut -d, -f3- >> "$AN"
echo "k2 $V çözümleme çıkış kodu: $rc" | tee -a "$AN"
exit "$rc"
