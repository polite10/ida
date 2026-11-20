#!/bin/sh
# W0-MP5a: derleme.
#   1. Ürün kökünde ürün kitaplığı: fw-release ön ayarı (W2-depo/W2-protokol derleme düzeni; ürün bayrakları).
#      Ürün kökü MP5_URUN_KOK ortam değişkeniyle verilebilir (ör. ürünün güncel dalının geçici çalışma
#      ağacı); yoksa bu çalışma kopyasıdır.
#   2. PC araçları (mp5_uret, mp5_kayit) ve girdi akışlarının üretimi (derleme/uretilen/); referans,
#      ürün kökündeki tel.c'nin host derlemesidir.
#   3. Ölçüm yazılımı derlemeleri (her birine benzersiz kimlik; açılış satırında basılır). Derleme adları:
#      CMakePresets.json'daki mp5-<ad> ön ayarları; "urun2" = ürün kitaplığı, yalnız ICACHE açık kipler;
#      "iyi-<çeşit>" = kopya/tel_iyi.c, tanımları kopya/cesitler.txt'den, yalnız ICACHE açık kipler.
#      MP5_YINELEME_EZ verilirse seri başına yineleme onunla ezilir (yalnız keşif koşuları için).
#   4. Boyut ve yığın raporu: kitaplık nesnesi, bağlanmış imajdaki kitaplık simgeleri, -fstack-usage.
# Kayıtlar $MP5_VERI/derle-<damga>/ altına yazılır (git'in yok saydığı dizin).
# Kullanım: tools/olcum/mp5/derle.sh [derleme...]     (varsayılan: o2 kisa)
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
URUN="${MP5_URUN_KOK:-$(cd "$KOK/../../.." && pwd)}"
export MP5_URUN_KOK="$URUN"
DAMGA=$(date +%Y%m%d-%H%M%S)
OUT="$MP5_VERI/derle-$DAMGA"
mkdir -p "$OUT"
VARS="${*:-o2 kisa}"
EZ=""
[ -z "${MP5_YINELEME_EZ:-}" ] || EZ="-DMP5_YINELEME=$MP5_YINELEME_EZ"

# 1. Ürün kitaplığı
( cd "$URUN" && cmake --preset fw-release > "$OUT/urun-yapilandir.log" 2>&1 &&
  cmake --build --preset fw-release --target ida_link ida_link_uret > "$OUT/urun-derle.log" 2>&1 ) ||
  { cat "$OUT/urun-yapilandir.log" "$OUT/urun-derle.log" 2>/dev/null; exit 1; }
echo "ürün kökü: $(git -C "$URUN" rev-parse HEAD 2>/dev/null || echo '?')" > "$OUT/urun-surumu.txt"
FW="$URUN/build/fw-release"
LIB="$FW/protocol/ida_link/libida_link.a"
python3 -I - "$FW/compile_commands.json" > "$OUT/tel-derleme-komutu.txt" <<'EOF'
import json, sys
for e in json.load(open(sys.argv[1])):
    if e['file'].endswith('protocol/ida_link/src/tel.c'):
        c = e['command']
        # kişisel yolları gizle: yalnız bayraklar
        print(' '.join(t for t in c.split() if t.startswith('-') and not t.startswith(('-I', '-o')) and t != '-isystem'))
EOF

# 2. PC araçları ve akışlar
cd "$KOK"
rm -rf derleme/pc derleme/uretilen
cmake --preset mp5-pc -DIDA_KOK="$URUN" > "$OUT/pc-yapilandir.log" 2>&1 || { cat "$OUT/pc-yapilandir.log"; exit 1; }
cmake --build --preset mp5-pc > "$OUT/pc-derle.log" 2>&1 || { cat "$OUT/pc-derle.log"; exit 1; }
mkdir -p derleme/uretilen
derleme/pc/mp5_uret derleme/uretilen | tee "$OUT/uret.log"
cp derleme/uretilen/beklenen.json derleme/uretilen/akis-dokum.md "$OUT/"

# 3. Ölçüm yazılımı
{
cat "$OUT/urun-surumu.txt"
echo "== ürün kitaplığı: $(shasum -a 256 "$LIB" | cut -c1-16)… (ürün kökü fw-release)"
echo "derleme bayrakları (tel.c): $(cat "$OUT/tel-derleme-komutu.txt")"
echo "--- libida_link.a (arm-none-eabi-size -A)"
"$ARM_GCC_BIN/arm-none-eabi-size" -A "$LIB" | grep -vE '^\.(debug|comment|ARM.attributes)|^$'
echo "--- -fstack-usage (tel.c)"
find "$FW/protocol/ida_link" -name 'tel.c.su' -exec cat {} \; | sed 's|.*/protocol/|protocol/|'
for v in $VARS; do
    KIMLIK="$DAMGA-$v-$(od -An -N4 -tx4 /dev/urandom | tr -d ' ')"
    D="derleme/$v"
    rm -rf "$D"
    case "$v" in
        iyi-*)
            c="${v#iyi-}"
            TANIM=$(awk -v c="$c" '$1==c {print $2}' kopya/cesitler.txt)
            [ -n "$TANIM" ] || { echo "çeşit yok: $c (kopya/cesitler.txt)" >&2; exit 1; }
            set -- --preset mp5-iyi -B "$D" "-DMP5_KOPYA_TANIM=$TANIM" "-DMP5_DERLEME=$v" ;;
        urun2)
            set -- --preset mp5-o2 -B "$D" "-DMP5_DERLEME=urun2-O2" "-DMP5_ONBELLEK=3" ;;
        *)
            set -- --preset "mp5-$v" ;;
    esac
    # shellcheck disable=SC2086
    cmake "$@" -DIDA_KOK="$URUN" -DMP5_KIMLIK="$KIMLIK" $EZ > "$OUT/yapilandir-$v.log" 2>&1 || { cat "$OUT/yapilandir-$v.log"; exit 1; }
    cmake --build "$D" > "$OUT/derle-$v.log" 2>&1 || { echo "derleme başarısız: $v"; cat "$OUT/derle-$v.log"; exit 1; }
    if grep -E "warning:|error:" "$OUT/derle-$v.log" | grep -v "is not implemented and will always fail" > "$OUT/uyari-$v.txt"; then
        echo "--- $v: derleyici uyarıları (derleme durduruldu)"; cat "$OUT/uyari-$v.txt"; exit 1
    fi
    echo "$KIMLIK" > "$D/kimlik.txt"
    LC_ALL=C grep -c -a "$KIMLIK" "$D/mp5.elf" > /dev/null || { echo "kimlik ELF'te yok: $v" >&2; exit 1; }
    echo "=== $v (kimlik $KIMLIK)"
    shasum -a 256 "$D/mp5.elf" "$D/mp5.bin" | sed 's|  .*/derleme/|  derleme/|'
    "$ARM_GCC_BIN/arm-none-eabi-size" "$D/mp5.elf"
    grep -E "^ +(RAM|FLASH):" "$OUT/derle-$v.log" || true
    echo "--- kitaplıkta çağrılan memcpy/memset (sözcük kopyası çeşitlerinde ölçülen yolda olmamalı)"
    "$ARM_GCC_BIN/arm-none-eabi-objdump" -d "$D/mp5.elf" | awk '/^[0-9a-f]+ <(ida_link_|govde_isle|cerceve_|crc_|bayt_isle|kopyala|sifirla)/ {f=$2} /^$/ {f=""} f!="" && /bl.*<mem(cpy|set)>/ {print "  " f " -> " $NF}' | sort | uniq -c
done
} > "$OUT/derle-ozet.txt" 2>&1 || { cat "$OUT/derle-ozet.txt"; exit 1; }
cat "$OUT/derle-ozet.txt"
echo "derle.sh: kayıtlar $OUT"
