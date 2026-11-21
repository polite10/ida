#!/bin/bash
# statik-analiz.sh'nin kendi testi (sol S2): çıkış kodlarını bilinen girdilerle sınar.
#
# Kullanım: tools/denetim/test/statik_analiz_test.sh
#   CLANG_TIDY ve CPPCHECK statik-analiz.sh'deki gibi verilir (gerçek araçlar gerekir).
#
# Durumlar (beklenen çıkış):
#   temiz girdi                                         -> 0
#   gerçek bulgu içeren girdi                           -> 1
#   sözdizimi hatalı girdi                              -> 70 (araç çalışmadı)
#   önişleme hatalı (#error) girdi                      -> 70 (araç çalışmadı)
#   karşı örnek yerine yalnız #error içeren dosya       -> 70 (beklenen kimlikler yok)
# Çıkış: 0 hepsi geçti, 1 en az biri kaldı.

set -u
export LC_ALL=C

kok=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd) || exit 1
cd "$kok" || exit 1
betik=tools/denetim/statik-analiz.sh
d=tools/denetim/deneme/statik
kalan=0

durum() {
    local ad=$1 beklenen=$2 kaynak=$3 karsi=${4-} rc cikti
    cikti=$(IDA_STATIK_KAYNAKLAR="$kaynak" IDA_STATIK_KARSI_ORNEK="$karsi" \
        bash "$betik" 2>&1)
    rc=$?
    if [ "$rc" -eq "$beklenen" ]; then
        printf 'GEÇTİ: %s (çıkış %s)\n' "$ad" "$rc"
    else
        printf 'KALDI: %s (beklenen %s, gerçek %s)\n' "$ad" "$beklenen" "$rc"
        printf '%s\n' "$cikti" | sed 's/^/    /'
        kalan=1
    fi
}

durum "temiz girdi" 0 "$d/temiz.c"
durum "gerçek bulgu" 1 "$d/bulgulu.c"
durum "sözdizimi hatası araç çalışmadı sayılır" 70 "$d/sozdizimi_hatali.c"
durum "önişleme hatası araç çalışmadı sayılır" 70 "$d/onisleme_hatali.c"
durum "yalnız #error içeren karşı örnek kabul edilmez" 70 "$d/temiz.c" "$d/onisleme_hatali.c"

exit "$kalan"
