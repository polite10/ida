#!/bin/sh
# W0-MP5a: derlemelerin bağlanmış imajlarındaki tel katmanı simgelerinin boyu (nm -S; tür, boy, ad).
# Simge listesi, derlemenin bağladığı kitaplık arşivinden alınır (kopya çeşitlerinde
# derleme/<ad>/libmp5_ida_link.a; ürün derlemesinde ürün kökünün libida_link.a'sı; ürün kökü
# MP5_URUN_KOK ya da bu çalışma kopyası). Yerel (static) simgeler dahildir.
# Kullanım: tools/olcum/mp5/boyut.sh [-o] <derleme...>     (-o: yalnız toplam satırı)
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
URUN="${MP5_URUN_KOK:-$(cd "$KOK/../../.." && pwd)}"
OZ=0
[ "${1:-}" = "-o" ] && { OZ=1; shift; }
for v in "$@"; do
    A="$KOK/derleme/$v/libmp5_ida_link.a"
    [ -f "$A" ] || A="$URUN/build/fw-release/protocol/ida_link/libida_link.a"
    "$ARM_GCC_BIN/arm-none-eabi-nm" --defined-only "$A" | awk 'NF==3 && $3 !~ /^\$/ {print $3}' | sort -u > "$KOK/derleme/.simge-$v"
    "$ARM_GCC_BIN/arm-none-eabi-nm" -S --size-sort "$KOK/derleme/$v/mp5.elf" | awk 'NF==4' |
      grep -wFf "$KOK/derleme/.simge-$v" |
      awk -v ad="$v" -v oz="$OZ" '{ sz=0; h=$2; for (i=1;i<=length(h);i++) sz = sz*16 + index("0123456789abcdef", substr(h,i,1)) - 1;
             if (!oz) printf "  %-2s %5d B  %s\n", $3, sz, $4;
             if ($3 ~ /[Tt]/) kod+=sz; else if ($3 ~ /[Rr]/) ro+=sz; else rw+=sz }
           END { printf "%s: kod %d B, salt okunur veri %d B, yazılabilir veri %d B\n", ad, kod, ro, rw }'
    rm -f "$KOK/derleme/.simge-$v"
done
