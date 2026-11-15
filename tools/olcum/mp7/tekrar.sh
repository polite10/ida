#!/usr/bin/env bash
# W0-MP7a tekrar üretilebilirlik (05 §2.6): aynı tohumla iki ayrı süreç,
# tek iş parçacığı ve tüm iş parçacıkları; profil özetleri (FNV-1a) karşılaştırılır.
set -euo pipefail
kok="$(cd "$(dirname "$0")" && pwd)"
bin="$kok/derleme/mp7"
mkdir -p "$kok/veri"
cikti="$kok/veri/tekrar2.txt"
: >"$cikti"
for menzil in 7 50; do
  for is in 1 tumu; do
    for kosu in 1 2; do
      satir="$("$bin" tekrar --sahne 1e5 --menzil "$menzil" --N "${NT:-80000}" --D 2 --is "$is" --ping 200)"
      echo "kosu=$kosu $satir" | tee -a "$cikti"
    done
  done
done
echo "--- ayrı özet sayısı (menzil başına 1 beklenir: 1 ve tüm iş parçacığı aynı) ---" | tee -a "$cikti"
for menzil in 7 50; do
  n="$(grep "menzil=$menzil " "$cikti" | sed -E 's/.*(ozet_I=[0-9a-f]+ ozet_u8=[0-9a-f]+).*/\1/' | sort -u | wc -l | tr -d ' ')"
  echo "menzil=$menzil ayri_ozet=$n" | tee -a "$cikti"
done
