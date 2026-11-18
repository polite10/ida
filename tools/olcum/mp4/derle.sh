#!/bin/sh
# W0-MP4a: temiz dizinden aygıt yazılımı derlemeleri ve PC araçları; ardından ek gözlemler:
#   - her derlemeye benzersiz kimlik (MP4_KIMLIK; açılış satırında basılır, koşuyu imaja bağlar)
#   - SHA-256, bölüm boyutları, adım kodunun (libmp4_adim.a) boyutu
#   - D3: yt_adim'den erişilen işlevlerde çift duyarlıklı yazılım kayan nokta çağrısı (mp4_cagri)
#   - adım kodunun statik yığın kullanımı (-fstack-usage, .su dosyaları)
# Derlemeler: o2, og (400 Hz, A+B+C; zorunlu), o2-200, og-200 (200 Hz, yalnız B; ek), kisa (düzenek denemesi).
# Çıktılar $IDA_CACHE/mp4/derle-<damga>/ altına yazılır (önceki derleme kayıtları korunur).
# Kullanım: tools/olcum/mp4/derle.sh
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
cd "$KOK"
DAMGA=$(date +%Y%m%d-%H%M%S)
OUT="$MP4_VERI/derle-$DAMGA"
mkdir -p "$OUT"
rm -rf derleme
cmake --preset mp4-pc > /dev/null
cmake --build --preset mp4-pc > "$OUT/derle-pc.log" 2>&1 || { cat "$OUT/derle-pc.log"; exit 1; }
if grep -E "warning:|error:" "$OUT/derle-pc.log"; then echo "PC araçlarında uyarı var" >&2; exit 1; fi
rc_d3=0
trap 'cat "$OUT/derle-ozet.txt"' EXIT
for v in o2 og o2-200 og-200 kisa; do
    KIMLIK="$DAMGA-$v-$(od -An -N4 -tx4 /dev/urandom | tr -d ' ')"
    cmake --preset "mp4-$v" -DMP4_KIMLIK="$KIMLIK" > /dev/null
    cmake --build --preset "mp4-$v" > "$OUT/derle-$v.log" 2>&1 || { echo "derleme başarısız: $v"; cat "$OUT/derle-$v.log"; exit 1; }
    if grep -E "warning:|error:" "$OUT/derle-$v.log" | grep -v "is not implemented and will always fail" > "$OUT/uyari-$v.txt"; then
        echo "--- $v: derleyici uyarıları (derleme durduruldu)"; cat "$OUT/uyari-$v.txt"; exit 1
    fi
    D="derleme/$v"
    echo "$KIMLIK" > "$D/kimlik.txt"
    LC_ALL=C grep -c -a "$KIMLIK" "$D/mp4.elf" > /dev/null || { echo "kimlik ELF'te yok: $v" >&2; exit 1; }
    echo "=== $v (kimlik $KIMLIK)"
    shasum -a 256 "$D/mp4.elf" "$D/mp4.bin" | sed 's|  .*/derleme/|  derleme/|'
    "$ARM_GCC_BIN/arm-none-eabi-size" "$D/mp4.elf"
    grep -E "^ +(RAM|FLASH):" "$OUT/derle-$v.log" || true
    case "$v" in o2|og) ;; *) continue ;; esac
    echo "--- adım kodu (libmp4_adim.a, bağlanmadan önce)"
    "$ARM_GCC_BIN/arm-none-eabi-size" -t "$D/libmp4_adim.a"
    echo "--- adım kodu (bağlanmış ELF'te, adım kitaplığında tanımlı sembollerin boyutu; libm hariç)"
    "$ARM_GCC_BIN/arm-none-eabi-nm" --defined-only "$D/libmp4_adim.a" | awk 'NF==3 {print $3}' | sort -u > "$OUT/adim-sembol-$v.txt"
    "$ARM_GCC_BIN/arm-none-eabi-nm" -S "$D/mp4.elf" | awk 'NF==4' | grep -wFf "$OUT/adim-sembol-$v.txt" |
      awk '{ t=$3; sz=0; h=$2; for (i=1;i<=length(h);i++) sz = sz*16 + index("0123456789abcdef", substr(h,i,1)) - 1;
             if (t ~ /[Tt]/) k="kod(text)"; else if (t ~ /[Rr]/) k="sabit(rodata)"; else if (t ~ /[Bb]/) k="bss"; else if (t ~ /[Dd]/) k="data"; else k=t;
             top[k]+=sz } END { for (k in top) printf "  %-14s %8d B\n", k, top[k] }'
    echo "--- yt_adim'den erişilen libm/libgcc dahil bütün işlevlerin kod boyutu"
    "$ARM_GCC_BIN/arm-none-eabi-objdump" -d "$D/mp4.elf" > "$OUT/objdump-$v.txt"
    if derleme/pc/mp4_cagri "$OUT/objdump-$v.txt" yt_adim > "$OUT/d3-$v.txt"; then d3=0; else d3=$?; fi
    sed -n 's/^  ÇİFT  //p; s/^        //p' "$OUT/d3-$v.txt" | awk '{print $1}' > "$OUT/erisilen-$v.txt"
    "$ARM_GCC_BIN/arm-none-eabi-nm" -S "$D/mp4.elf" | awk 'NF==4 && ($3=="T"||$3=="t")' | grep -wFf "$OUT/erisilen-$v.txt" |
      awk '{ sz=0; h=$2; for (i=1;i<=length(h);i++) sz = sz*16 + index("0123456789abcdef", substr(h,i,1)) - 1; top+=sz }
           END { printf "  %d işlev, %d B kod\n", NR, top }'
    echo "--- D3: yt_adim'den erişilen işlevler (mp4_cagri çıkış kodu $d3)"
    tail -n 3 "$OUT/d3-$v.txt"
    [ "$d3" -eq 0 ] || rc_d3=1
    echo "--- ELF'in tamamında __aeabi_d* / __*df* sembolü (adım yolu dışı dahil): $("$ARM_GCC_BIN/arm-none-eabi-nm" "$D/mp4.elf" | grep -cE ' (__aeabi_d|__[a-z]*df[0-9a-z]*$)' || true)"
    echo "--- adım nesnelerinde çözülmemiş çift duyarlıklı başvuru: $("$ARM_GCC_BIN/arm-none-eabi-nm" -u "$D/libmp4_adim.a" | grep -cE '__aeabi_d|df[0-9]' || true)"
    echo "--- adım kodu statik yığın (-fstack-usage, en büyük 8)"
    cat "$D"/CMakeFiles/mp4_adim.dir/adim/*.su | sed "s|.*/tools/olcum/mp4/|tools/olcum/mp4/|" | sort -t"$(printf '\t')" -k2 -n -r | head -n 8
done > "$OUT/derle-ozet.txt" 2>&1
echo "D3 çıkış kodu: $rc_d3 (özet: $OUT/derle-ozet.txt)" >> "$OUT/derle-ozet.txt"
exit "$rc_d3"
