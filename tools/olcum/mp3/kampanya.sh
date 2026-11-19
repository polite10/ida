#!/bin/sh
# W0-MP3a: dondurulmuş kampanya (görev belgesi "Dondurulmuş kampanya tanımı"). İLK TESLİMDE ÇALIŞTIRILMADI.
#  1) Ön taramayı geçen her baud için: iki yönde bağımsız sayaçlı 256 B akış, yön başına 8N1 kapasitesinin %80'i
#     (araçta sabit üst sınır; --oran VERİLMEZ), ≥ 600 s (--sure 600 --en-az-sn 600 --kampanya: profil zorunlu).
#  2) Geçen en yüksek baud'da: echo 64 B ve 256 B, ayrı ayrı 200 ve 400 istek/s, her profil 600 s. Kapasiteyi aşan
#     profil koşturulmaz; araç hesapla GECMEDI kaydeder.
# Her koşu kos.sh ile (uyku engeli, ortam kaydı, kimlik denetimi). Kullanım:
#   tools/olcum/mp3/kampanya.sh <en_yüksek_baud> <baud> [<baud> ...]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
[ $# -ge 2 ] || { echo "kullanım: kampanya.sh <en_yüksek_baud> <baud>..." >&2; exit 4; }
EN=$1; shift
ozet=""
for B in "$@"; do
    if sh "$KOK/kos.sh" kos "akis256-$B" --mod akis --baud "$B" --yuk 256 --sure 600 --en-az-sn 600 --kampanya; then r=0; else r=$?; fi
    ozet="$ozet akis256-$B=$r"
done
for Y in 64 256; do
    for H in 200 400; do
        if sh "$KOK/kos.sh" kos "echo$Y-$H-$EN" --mod echo --baud "$EN" --yuk "$Y" --hiz "$H" --sure 600 --en-az-sn 600 --kampanya; then r=0; else r=$?; fi
        ozet="$ozet echo$Y-$H-$EN=$r"
    done
done
echo "kampanya özeti (0 GECTI, 1 GECMEDI, 2 GECERSIZ, 3 BASLAMADI, 4 ortam):$ozet"
