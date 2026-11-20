#!/bin/bash
# İDA C/C++ kaynaklarının statik analizi: clang-tidy ve cppcheck (W2-depo §4; 09 §2.8).
#
# Kullanım:
#   tools/denetim/statik-analiz.sh [rapor-dosyası]
#
#   rapor-dosyası  Araç sürümleri, süreler, taranan dizinler ve bulgu sayıları buraya da
#                  yazılır (varsayılan: yalnız standart çıktı).
#
# Ortam değişkenleri:
#   CLANG_TIDY     clang-tidy yolu (varsayılan: PATH'teki clang-tidy)
#   CPPCHECK       cppcheck yolu (varsayılan: PATH'teki cppcheck)
#   IDA_URETILEN   üretilmiş MAVLink başlıklarının kökü (`mavlink/ida/mavlink.h` bunun
#                  altında); varsayılan build/host-debug/protocol/uretilen. Yalnız core/
#                  dışındaki bir dizin taranırken gerekir; yoksa çıkış 70 (önce host-debug
#                  derlenmeli: başlıklar derleme sırasında üretilir).
#
# Taranan dizinler: TARAMA_DIZINLERI listesi (core/, protocol/ida_link/). Var olmayan dizin
# atlanır ve bu günlüğe yazılır. Her dizinde kaynaklar <dizin>/src/ altındaki (alt dizinler
# dahil) .c ve .cpp dosyalarıdır; src/ yoksa dizinin kendisindekiler (test/ ve tests/
# hariç). Kaynak listesi git'ten alınır (izlenen + izlenmeyip yok sayılmayan dosyalar).
# Include yolu: her taranan dizinin include/ ve src/ alt dizinleri; core/ dışında bir dizin
# taranıyorsa ayrıca üretilmiş MAVLink başlıkları (clang-tidy'ye -isystem; cppcheck'e -I ve
# o yoldaki bulgular bastırılır: üretilen kod politikanın dışındadır).
# Dil: .c -> ISO C17, .cpp -> C++20. Sözleşme başlıkları (port.h, algilama_kaydi.h) core/
# kaynakları üzerinden çözümlenir.
#
# Politika: clang-tidy denetimleri ve kapatılan tek denetim kökteki .clang-tidy dosyasındadır
# (bugprone-*, cert-*, clang-analyzer-*; uyarı hatadır). cppcheck: warning, style,
# performance, portability; hedefin <float.h> değerleri tools/denetim/cppcheck-hedef.h ile
# --include olarak verilir (gerekçesi o dosyada). "MISRA uyumlu" iddiası yoktur (09 §2.8).
# Firmware'in GCC -fanalyzer'ı fw-debug ön ayarında, MSVC /analyze IDA_MSVC_ANALYZE
# seçeneğiyle derlemenin içindedir.
# Çıkış: 0 bulgu yok, 1 bulgu var, 70 araç ya da girdi yok, araç çalışmadı.

set -u
export LC_ALL=C

TARAMA_DIZINLERI=(core protocol/ida_link)

kok=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd) || exit 70
cd "$kok" || exit 70
rapor=${1-}
gunluk() { printf 'statik-analiz: %s\n' "$*" >&2; }

clang_tidy=${CLANG_TIDY:-clang-tidy}
cppcheck=${CPPCHECK:-cppcheck}
for arac in "$clang_tidy" "$cppcheck"; do
    if [ ! -f "$arac" ] && ! command -v "$arac" >/dev/null 2>&1; then
        gunluk "araç bulunamadı: $arac"
        exit 70
    fi
done

# Bir dizinin kaynaklarını git'ten listeler (izlenen + izlenmeyip yok sayılmayan).
kaynak_listele() {
    local d=$1 kalip
    if [ -d "$d/src" ]; then kalip="$d/src/**/"; else kalip="$d/"; fi
    git ls-files --cached --others --exclude-standard -- \
        ":(glob)${kalip}*.c" ":(glob)${kalip}*.cpp" |
        grep -v -E "^$d/(test|tests)/" | sort -u
}

c_kaynaklar=()
cxx_kaynaklar=()
include_args=()
taranan=()
uretilen_gerekli=0
for d in "${TARAMA_DIZINLERI[@]}"; do
    if [ ! -d "$d" ]; then
        gunluk "$d/ yok, atlandı"
        continue
    fi
    taranan+=("$d")
    [ "$d" = core ] || uretilen_gerekli=1
    [ -d "$d/include" ] && include_args+=("-I$d/include")
    [ -d "$d/src" ] && include_args+=("-I$d/src")
    n=0
    while IFS= read -r k; do
        [ -n "$k" ] && [ -f "$k" ] || continue
        case "$k" in
            *.c) c_kaynaklar+=("$k") ;;
            *.cpp) cxx_kaynaklar+=("$k") ;;
        esac
        n=$((n + 1))
    done < <(kaynak_listele "$d")
    gunluk "$d/: $n kaynak dosya"
done
toplam=$(( ${#c_kaynaklar[@]} + ${#cxx_kaynaklar[@]} ))
if [ "$toplam" -eq 0 ]; then
    gunluk "taranacak kaynak yok (${TARAMA_DIZINLERI[*]})"
    exit 70
fi

ct_sistem=()
cc_uretilen=()
if [ "$uretilen_gerekli" -eq 1 ]; then
    uretilen=${IDA_URETILEN:-build/host-debug/protocol/uretilen}
    if [ ! -f "$uretilen/mavlink/ida/mavlink.h" ]; then
        gunluk "üretilmiş MAVLink başlığı yok: $uretilen/mavlink/ida/mavlink.h (önce host-debug derleyin ya da IDA_URETILEN verin)"
        exit 70
    fi
    gunluk "üretilmiş başlıklar: $uretilen"
    ct_sistem=(-isystem "$uretilen")
    cc_uretilen=("-I$uretilen" "--suppress=*:*protocol/uretilen/*" "--suppress=*:*protocol\\uretilen\\*")
fi

sistem_args=()
case "$(uname -s)" in
    Darwin)
        # PyPI'deki clang-tidy macOS SDK'sını kendisi bulmaz.
        sdk=$(xcrun --show-sdk-path 2>/dev/null) && sistem_args+=(-isysroot "$sdk") ;;
esac

gecici=$(mktemp -d "${TMPDIR:-/tmp}/ida-statik.XXXXXX") || exit 70
trap 'rm -rf "$gecici"' EXIT
: >"$gecici/clang-tidy.txt"

zaman() { date +%s; }
bulgu_say() { grep -c -E "$1" "$2"; }

# ---- clang-tidy (dil başına bir çağrı) --------------------------------------------------
ct_surum=$("$clang_tidy" --version 2>&1 | tr -d '\r' | grep -m1 -i 'version' | sed 's/^ *//')
t0=$(zaman)
ct_rc=0
if [ "${#c_kaynaklar[@]}" -gt 0 ]; then
    "$clang_tidy" --quiet --config-file=.clang-tidy "${c_kaynaklar[@]}" -- \
        -std=c17 ${include_args[@]+"${include_args[@]}"} ${ct_sistem[@]+"${ct_sistem[@]}"} \
        ${sistem_args[@]+"${sistem_args[@]}"} >>"$gecici/clang-tidy.txt" 2>&1 || ct_rc=$?
fi
if [ "${#cxx_kaynaklar[@]}" -gt 0 ]; then
    "$clang_tidy" --quiet --config-file=.clang-tidy "${cxx_kaynaklar[@]}" -- \
        -std=c++20 ${include_args[@]+"${include_args[@]}"} ${ct_sistem[@]+"${ct_sistem[@]}"} \
        ${sistem_args[@]+"${sistem_args[@]}"} >>"$gecici/clang-tidy.txt" 2>&1 || ct_rc=$?
fi
t1=$(zaman)
ct_bulgu=$(bulgu_say '(warning|error): ' "$gecici/clang-tidy.txt")

# ---- cppcheck --------------------------------------------------------------------------
cc_surum=$("$cppcheck" --version 2>&1 | tr -d '\r')
t2=$(zaman)
"$cppcheck" --quiet --std=c11 --std=c++20 \
    --enable=warning,style,performance,portability \
    --inline-suppr --suppress=missingIncludeSystem \
    --include=tools/denetim/cppcheck-hedef.h \
    --error-exitcode=1 \
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
    ${include_args[@]+"${include_args[@]}"} ${cc_uretilen[@]+"${cc_uretilen[@]}"} \
    ${c_kaynaklar[@]+"${c_kaynaklar[@]}"} ${cxx_kaynaklar[@]+"${cxx_kaynaklar[@]}"} \
    >"$gecici/cppcheck.txt" 2>&1
cc_rc=$?
t3=$(zaman)
cc_bulgu=$(bulgu_say ': (error|warning|style|performance|portability): ' "$gecici/cppcheck.txt")

ozet=$(printf '%s\n' \
    "# Statik analiz özeti (taranan: ${taranan[*]}; ${#c_kaynaklar[@]} C, ${#cxx_kaynaklar[@]} C++ kaynak)" \
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
"$clang_tidy" --quiet --config-file=.clang-tidy \
    "$deneme" -- -std=c17 ${sistem_args[@]+"${sistem_args[@]}"} >"$gecici/ct-deneme.txt" 2>&1
ct_deneme=$(bulgu_say '(warning|error): ' "$gecici/ct-deneme.txt")
"$cppcheck" --quiet --std=c11 --language=c --enable=warning,style,performance,portability \
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
    "$deneme" >"$gecici/cc-deneme.txt" 2>&1
cc_deneme=$(bulgu_say ': (error|warning|style|performance|portability): ' "$gecici/cc-deneme.txt")
printf 'karşı örnek (%s): clang-tidy %s bulgu, cppcheck %s bulgu\n' "$deneme" "$ct_deneme" "$cc_deneme"
[ -z "$rapor" ] || printf 'karşı örnek (%s): clang-tidy %s bulgu, cppcheck %s bulgu\n' \
    "$deneme" "$ct_deneme" "$cc_deneme" >>"$rapor"
if [ "$ct_deneme" -eq 0 ] || [ "$cc_deneme" -eq 0 ]; then
    gunluk "araç karşı örnekte bulgu üretmedi; denetim çalışmıyor"
    cat "$gecici/ct-deneme.txt" "$gecici/cc-deneme.txt" >&2
    exit 70
fi

if [ "$ct_rc" -ne 0 ] || [ "$ct_bulgu" -ne 0 ] || [ "$cc_rc" -ne 0 ] || [ "$cc_bulgu" -ne 0 ]; then
    exit 1
fi
exit 0
