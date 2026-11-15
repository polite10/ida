#!/bin/sh
# W0-MP1a K2: SMPS ve LDO turlarını sırayla koşar. Mac'in uykuya geçmemesi için caffeinate altında çağrılır:
#   caffeinate -dims sh tools/olcum/mp1/k2-iki-tur.sh [süre_s=640]
# İlk (SMPS) tur başarısızsa LDO turuna geçilmez; çıkış kodu 1.
KOK="$(cd "$(dirname "$0")" && pwd)"
SURE="${1:-640}"
if ! sh "$KOK/k2-tur.sh" smps "$SURE"; then
    echo "K2: SMPS turu başarısız; LDO turu koşulmadı" >&2
    exit 1
fi
if ! sh "$KOK/k2-tur.sh" ldo "$SURE"; then
    echo "K2: LDO turu başarısız" >&2
    exit 1
fi
echo "K2: iki tur da geçerli"
