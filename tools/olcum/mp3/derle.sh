#!/bin/sh
# W0-MP3a: temiz dizinden aygıt yazılımı, PC aracı ve masaüstü testleri; testler koşturulur. Benzersiz derleme
# kimliği; SHA-256, bölüm boyutları, kaynak durumu (git) ve araç sürümleri kaydedilir.
# Çıktı: $IDA_CACHE/mp3/derle-<damga>/ (öncekiler korunur). Herhangi bir derleme hatası, derleyici uyarısı
# (newlib'in "is not implemented" bağlayıcı uyarıları hariç; MP4 ile aynı) ya da başarısız test SIFIR DIŞI çıkış verir.
# Kullanım: tools/olcum/mp3/derle.sh
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
cd "$KOK"
DAMGA=$(date +%Y%m%d-%H%M%S)
OUT="$MP3_VERI/derle-$DAMGA"
mkdir -p "$MP3_VERI"
mkdir "$OUT"
rm -rf derleme/fw derleme/pc
KIMLIK="$DAMGA-mp3-$(od -An -N4 -tx4 /dev/urandom | tr -d ' ')"
cmake --preset mp3-fw -DMP3_KIMLIK="$KIMLIK" > "$OUT/yapilandir-fw.log" 2>&1
cmake --build --preset mp3-fw > "$OUT/derle-fw.log" 2>&1 || { cat "$OUT/derle-fw.log"; exit 1; }
if grep -E "warning:|error:" "$OUT/derle-fw.log" | grep -v "is not implemented and will always fail"; then
    echo "derle.sh: aygıt yazılımında uyarı var" >&2; exit 1
fi
LC_ALL=C grep -c -a "$KIMLIK" derleme/fw/mp3.elf > /dev/null || { echo "derle.sh: kimlik ELF'te yok" >&2; exit 1; }
echo "$KIMLIK" > derleme/fw/kimlik.txt
cmake --preset mp3-pc > "$OUT/yapilandir-pc.log" 2>&1
cmake --build --preset mp3-pc > "$OUT/derle-pc.log" 2>&1 || { cat "$OUT/derle-pc.log"; echo "derle.sh: PC derlemesi başarısız" >&2; exit 1; }
if grep -E "warning:|error:" "$OUT/derle-pc.log"; then echo "derle.sh: PC kodunda uyarı var" >&2; exit 1; fi
if derleme/pc/mp3_test > "$OUT/test.log" 2>&1; then test_sonuc="geçti ($(tail -n 1 "$OUT/test.log"))"; else
    tail -n 30 "$OUT/test.log"; echo "derle.sh: masaüstü testleri başarısız ($OUT/test.log)" >&2; exit 1; fi
{
  echo "kimlik: $KIMLIK"
  echo "kaynak: $(git -C "$KOK" rev-parse HEAD) dal $(git -C "$KOK" rev-parse --abbrev-ref HEAD)"
  echo "kaynak durumu (tools/olcum/mp3, işlenmemiş değişiklikler):"
  git -C "$KOK" status --porcelain -- . | sed 's/^/  /'
  echo "kaynak özeti (izlenen + izlenmeyen dosyaların SHA-256'sı, derleme/ hariç):"
  find . -path ./derleme -prune -o -type f -print | LC_ALL=C sort | xargs shasum -a 256 | shasum -a 256 | sed 's/^/  /'
  echo "derleyici: $("$ARM_GCC_BIN/arm-none-eabi-gcc" --version | head -n 1)"
  echo "host: $(c++ --version | head -n 1); cmake $(cmake --version | head -n 1 | cut -d' ' -f3)"
  echo "imaj:"
  shasum -a 256 derleme/fw/mp3.elf derleme/fw/mp3.bin | sed 's|  derleme/|  derleme/|'
  "$ARM_GCC_BIN/arm-none-eabi-size" derleme/fw/mp3.elf
  grep -E "^ +(RAM|FLASH):" "$OUT/derle-fw.log" || true
  echo "PC: mp3_vcp SHA-256 $(shasum -a 256 derleme/pc/mp3_vcp | cut -d' ' -f1)"
  echo "masaüstü testleri: $test_sonuc"
} > "$OUT/derle-ozet.txt"
cp derleme/fw/mp3.elf derleme/fw/mp3.bin derleme/fw/kimlik.txt "$OUT/"
cat "$OUT/derle-ozet.txt"
echo "derle.sh: çıktı $OUT"
