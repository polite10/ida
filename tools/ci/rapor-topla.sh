#!/bin/bash
# CI: firmware harita, boyut ve yığın raporlarını artefakt dizinine toplar.
# Kullanım: tools/ci/rapor-topla.sh <hedef-dizin>
# Bulunmayan ön ayar atlanır (önceki adım başarısız olmuş olabilir); betik başarısız olmaz.

set -u
hedef=${1:?hedef dizin verilmeli}
kok=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd) || exit 0
mkdir -p "$hedef"
for on_ayar in fw-debug fw-release fw-measure; do
    d="$kok/build/$on_ayar"
    [ -d "$d" ] || continue
    h="$hedef/$on_ayar"
    mkdir -p "$h"
    for f in "$d"/firmware/ida_fw.elf "$d"/firmware/ida_fw.map "$d"/firmware/ida_fw.boyut.txt \
             "$d"/yigin-kullanimi.txt "$d"/core/test/ida_core_test_iskelet.boyut.txt; do
        [ -f "$f" ] && cp "$f" "$h/"
    done
done
exit 0
