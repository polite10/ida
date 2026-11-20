#!/bin/sh
# W0-MP5a: birden çok derlemeyi sırayla koşturur (her biri kos.sh ile; kart tek, koşular ardışık).
# Kullanım: tools/olcum/mp5/kampanya.sh <derleme...>      (ör. kopya-o2 kopya-os tablo-o2 o2)
set -u
KOK="$(cd "$(dirname "$0")" && pwd)"
rc=0
for v in "$@"; do
    if sh "$KOK/kos.sh" "$v" 3600; then echo "kampanya: $v geçti"; else echo "kampanya: $v BAŞARISIZ"; rc=1; fi
done
exit "$rc"
