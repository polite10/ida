#!/bin/sh
# ida_sim'e gerçek SIGINT/SIGTERM (W4-sunucu-kucuk kabul 1): koşu sürerken sinyal, süreç sınırlı sürede
# 5 koduyla biter, standart hatada "kapanış: sinyal" özeti ve "sinyalle kesildi" görülür, CSV eksiksiz
# kapanır (son satır tam: "\n" ile biter). Kullanım: sinyal_surec.sh <ida_sim> <ida_sil> <geçici dizin> <INT|TERM>
SIM="$1"
SIL="$2"
T="$3"
SINYAL="$4"
rm -rf "$T"
mkdir -p "$T" || exit 2
# Uzun koşu: 1 500 sim saniyesi (SIL, hızlı); bu betik oturum kurulunca keser (sabit uyku yok).
"$SIM" --kip sil --karsi tcp --a1-port 0 --sil "$SIL" --sure 1500 --gunluk 2 --csv "$T/k.csv" >"$T/cikti.txt" 2>"$T/hata.txt" &
PID=$!
# Hazır olma işareti: "oturum kuruldu" günlük satırı (en çok 20 s; asılma sınırı, ölçüt değil).
I=0
while ! grep -q "oturum kuruldu" "$T/hata.txt" 2>/dev/null && kill -0 "$PID" 2>/dev/null && [ "$I" -lt 200 ]; do
  sleep 0.1
  I=$((I + 1))
done
if ! kill -0 "$PID" 2>/dev/null; then
  echo "KALDI: sinyal verilmeden süreç bitti (koşu oturum kurulunca henüz sürmüyordu)"; cat "$T/hata.txt"; exit 1
fi
grep -q "oturum kuruldu" "$T/hata.txt" || { kill -9 "$PID"; echo "KALDI: oturum 20 s içinde kurulmadı"; cat "$T/hata.txt"; exit 1; }
kill -"$SINYAL" "$PID"
# En çok 20 s bekle (asılma sınırı; ölçüt değil).
I=0
while kill -0 "$PID" 2>/dev/null && [ "$I" -lt 200 ]; do
  sleep 0.1
  I=$((I + 1))
done
if kill -0 "$PID" 2>/dev/null; then
  kill -9 "$PID"
  echo "KALDI: sinyalden sonra süreç 20 s içinde bitmedi"; exit 1
fi
wait "$PID"
KOD=$?
echo "çıkış kodu $KOD (SIG$SINYAL'den sonra ≤ $((I / 10)),$((I % 10)) s)"
[ "$KOD" -eq 5 ] || { echo "KALDI: çıkış kodu 5 değil"; cat "$T/hata.txt"; exit 1; }
grep -q "kapanış: sinyal;" "$T/hata.txt" || { echo "KALDI: özet satırı yok"; cat "$T/hata.txt"; exit 1; }
grep -q "son hata: sinyalle kesildi" "$T/hata.txt" || { echo "KALDI: son hata alanı"; cat "$T/hata.txt"; exit 1; }
grep -q "kapanış yazımı eksik 0" "$T/hata.txt" || { echo "KALDI: kapanış yazımı eksik"; cat "$T/hata.txt"; exit 1; }
[ -s "$T/k.csv" ] || { echo "KALDI: CSV yok ya da boş"; exit 1; }
[ "$(tail -c 1 "$T/k.csv" | od -An -c | tr -d ' ')" = '\n' ] || { echo "KALDI: CSV'nin son satırı tam değil"; exit 1; }
echo "GECTI"
