#!/bin/bash
# Ortak çekirdeğin statik analizi: clang-tidy ve cppcheck (W2-depo §4; 09 §2.8).
#
# Kullanım:
#   tools/denetim/statik-analiz.sh [rapor-dosyası]
#
#   rapor-dosyası  Araç sürümleri, süreler ve bulgu sayıları buraya da yazılır
#                  (varsayılan: yalnız standart çıktı).
#
# Ortam değişkenleri:
#   CLANG_TIDY  clang-tidy yolu (varsayılan: PATH'teki clang-tidy)
#   CPPCHECK    cppcheck yolu (varsayılan: PATH'teki cppcheck)
#
# Kapsam: core/src/*.c ve core/include, core/src altındaki başlıklar (sözleşme başlıkları
# port.h ve algilama_kaydi.h dahil). Firmware'in GCC -fanalyzer'ı fw-debug ön ayarında,
# MSVC /analyze IDA_MSVC_ANALYZE seçeneğiyle derlemenin içindedir.
#
# Politika: clang-tidy bugprone-*, cert-*, clang-analyzer-* (uyarı hatadır); cppcheck
# warning, style, performance, portability. "MISRA uyumlu" iddiası yoktur (09 §2.8).
# Çıkış: 0 bulgu yok, 1 bulgu var, 70 araç yok ya da çalışmadı.

set -u
export LC_ALL=C

kok=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd) || exit 70
cd "$kok" || exit 70
rapor=${1-}

clang_tidy=${CLANG_TIDY:-clang-tidy}
cppcheck=${CPPCHECK:-cppcheck}
for arac in "$clang_tidy" "$cppcheck"; do
    if ! command -v "$arac" >/dev/null 2>&1; then
        printf 'statik-analiz: araç bulunamadı: %s\n' "$arac" >&2
        exit 70
    fi
done

kaynaklar=()
for k in core/src/*.c; do
    [ -f "$k" ] && kaynaklar+=("$k")
done
if [ "${#kaynaklar[@]}" -eq 0 ]; then
    printf 'statik-analiz: core/src altında kaynak yok\n' >&2
    exit 70
fi

derleyici_args=(-std=c17 -Icore/include -Icore/src)
case "$(uname -s)" in
    Darwin)
        # PyPI'deki clang-tidy macOS SDK'sını kendisi bulmaz.
        sdk=$(xcrun --show-sdk-path 2>/dev/null) && derleyici_args+=(-isysroot "$sdk") ;;
esac

gecici=$(mktemp -d "${TMPDIR:-/tmp}/ida-statik.XXXXXX") || exit 70
trap 'rm -rf "$gecici"' EXIT

zaman() { date +%s; }

# ---- clang-tidy ----------------------------------------------------------------------
ct_surum=$("$clang_tidy" --version 2>&1 | grep -m1 -i 'version' | sed 's/^ *//')
t0=$(zaman)
"$clang_tidy" --quiet \
    --checks='-*,bugprone-*,cert-*,clang-analyzer-*' \
    --warnings-as-errors='*' \
    --header-filter='.*core[/\\](include|src)[/\\].*' \
    "${kaynaklar[@]}" -- "${derleyici_args[@]}" >"$gecici/clang-tidy.txt" 2>&1
ct_rc=$?
t1=$(zaman)
ct_bulgu=$(grep -c -E '(warning|error): ' "$gecici/clang-tidy.txt")

# ---- cppcheck ------------------------------------------------------------------------
cc_surum=$("$cppcheck" --version 2>&1)
t2=$(zaman)
"$cppcheck" --quiet --std=c11 --language=c \
    --enable=warning,style,performance,portability \
    --inline-suppr --suppress=missingIncludeSystem \
    --error-exitcode=1 \
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
    -Icore/include -Icore/src \
    "${kaynaklar[@]}" >"$gecici/cppcheck.txt" 2>&1
cc_rc=$?
t3=$(zaman)
cc_bulgu=$(grep -c -E ': (error|warning|style|performance|portability): ' "$gecici/cppcheck.txt")

ozet=$(printf '%s\n' \
    "# Statik analiz özeti (core/; ${#kaynaklar[@]} kaynak dosya)" \
    "clang-tidy: ${ct_surum}" \
    "  çıkış=${ct_rc} bulgu=${ct_bulgu} süre=$((t1 - t0)) s" \
    "cppcheck: ${cc_surum}" \
    "  çıkış=${cc_rc} bulgu=${cc_bulgu} süre=$((t3 - t2)) s")

printf '%s\n' "$ozet"
printf '%s\n' '---- clang-tidy çıktısı ----'
cat "$gecici/clang-tidy.txt"
printf '%s\n' '---- cppcheck çıktısı ----'
cat "$gecici/cppcheck.txt"
if [ -n "$rapor" ]; then
    {
        printf '%s\n' "$ozet"
        printf '%s\n' '---- clang-tidy ----'
        cat "$gecici/clang-tidy.txt"
        printf '%s\n' '---- cppcheck ----'
        cat "$gecici/cppcheck.txt"
    } >"$rapor"
fi

# ---- Karşı örnek: araçlar bilerek hatalı dosyada bulgu üretmeli ------------------------
deneme=tools/denetim/deneme/statik/hatali.c
"$clang_tidy" --quiet --checks='-*,bugprone-*,cert-*,clang-analyzer-*' \
    "$deneme" -- "${derleyici_args[@]}" >"$gecici/ct-deneme.txt" 2>&1
ct_deneme=$(grep -c -E '(warning|error): ' "$gecici/ct-deneme.txt")
"$cppcheck" --quiet --std=c11 --language=c --enable=warning,style,performance,portability \
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
    "$deneme" >"$gecici/cc-deneme.txt" 2>&1
cc_deneme=$(grep -c -E ': (error|warning|style|performance|portability): ' "$gecici/cc-deneme.txt")
printf 'karşı örnek (%s): clang-tidy %s bulgu, cppcheck %s bulgu\n' "$deneme" "$ct_deneme" "$cc_deneme"
[ -z "$rapor" ] || printf 'karşı örnek (%s): clang-tidy %s bulgu, cppcheck %s bulgu\n' \
    "$deneme" "$ct_deneme" "$cc_deneme" >>"$rapor"
if [ "$ct_deneme" -eq 0 ] || [ "$cc_deneme" -eq 0 ]; then
    printf 'statik-analiz: araç karşı örnekte bulgu üretmedi; denetim çalışmıyor\n' >&2
    cat "$gecici/ct-deneme.txt" "$gecici/cc-deneme.txt" >&2
    exit 70
fi

if [ "$ct_rc" -ne 0 ] || [ "$ct_bulgu" -ne 0 ] || [ "$cc_rc" -ne 0 ] || [ "$cc_bulgu" -ne 0 ]; then
    exit 1
fi
exit 0
