#!/bin/sh
# W0-MP3a: yüklemeden ÖNCE kartta duran imajı tanımlar (salt okuma, HOTPLUG; çekirdek sıfırlanmaz).
# Dahili flash'ın ilk 256 KB'si okunur; SHA-256 ve içindeki derleme kimliği dizgeleri (MP4/MP3 biçimi) kaydedilir.
# Çıktı: $IDA_CACHE/mp3/onceki-imaj-<damga>/ ; Kullanım: tools/olcum/mp3/onceki-imaj.sh
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
OUT="$MP3_VERI/onceki-imaj-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$MP3_VERI"; mkdir "$OUT"
if "$CLI" -c port=SWD mode=HOTPLUG -u 0x08000000 0x40000 "$OUT/flash-256k.bin" > "$OUT/okuma.ham" 2>&1; then rc=0; else rc=$?; fi
sed 's/\x1b\[[0-9;]*m//g' < "$OUT/okuma.ham" > "$OUT/okuma.log"; rm -f "$OUT/okuma.ham"
[ "$rc" -eq 0 ] && grep -q "Data read successfully" "$OUT/okuma.log" || { echo "onceki-imaj.sh: okuma başarısız ($rc)" >&2; exit 1; }
{
  echo "okuma: 0x08000000 + 0x40000, $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "SHA-256 (256 KB): $(shasum -a 256 "$OUT/flash-256k.bin" | cut -d' ' -f1)"
  echo "kimlik dizgeleri:"
  LC_ALL=C strings -n 12 "$OUT/flash-256k.bin" | grep -E '^[0-9]{8}-[0-9]{6}-[a-z0-9-]+-[0-9a-f]{8}$' | sed 's/^/  /' || echo "  (yok)"
} | tee "$OUT/ozet.txt"
