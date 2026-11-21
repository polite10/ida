#!/bin/sh
# firmware/araclar/yukle.sh — kart imajını dahili flash'a yükler, doğrular ve sıfırlar (W3-kart).
#
# Kullanım: firmware/araclar/yukle.sh <ön ayar> [günlük]      (ör. fw-release)
#   İmaj: build/<ön ayar>/firmware/ida_fw.elf (cmake --build --preset <ön ayar>).
#
# Bağlantı sıfırlama altında kurulur (mode=UR): çalışan imajın 100 ms'lik IWDG'si programlama
# sırasında kartı sıfırlayamaz. Yalnız yazılan sayfalar silinir; seçenek baytlarına ve harici
# flash'a dokunulmaz. Önkoşul (MP1'in kuralı): geçerli fabrika yedeği; IDA_YEDEK_DENETIMI=0 ile
# atlanır (yedeği olmayan makinede).
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
DEPO="$(cd "$KOK/../.." && pwd)"
. "$KOK/ortam.sh"
P="${1:?ön ayar: fw-release | fw-debug | fw-measure}"
ELF="$DEPO/build/$P/firmware/ida_fw.elf"
LOG="${2:-$DEPO/build/$P/yukle-son.log}"
[ -f "$ELF" ] || { echo "yukle.sh: yok: $ELF (önce cmake --build --preset $P)" >&2; exit 2; }
[ -x "$CLI" ] || { echo "yukle.sh: STM32_Programmer_CLI yok: $CLI (CUBEPROG_BIN verin)" >&2; exit 2; }
if [ "${IDA_YEDEK_DENETIMI:-1}" != "0" ]; then
    sh "$KOK/yedek-dogrula.sh" || { echo "yukle.sh: geçerli yedek yok; yükleme yapılmadı" >&2; exit 3; }
fi
if "$CLI" -c port=SWD mode=UR reset=HWrst -w "$ELF" -v -rst > "$LOG.ham" 2>&1; then rc=0; else rc=$?; fi
sed 's/\x1b\[[0-9;]*m//g' < "$LOG.ham" > "$LOG"
rm -f "$LOG.ham"
grep -E "Error|error|Download verified|File download complete|Time elapsed|reset|Device ID" "$LOG" || true
if [ "$rc" -ne 0 ]; then echo "yukle.sh: programlayıcı çıkış kodu $rc (günlük $LOG)" >&2; exit 1; fi
if ! grep -q "Download verified successfully" "$LOG"; then
    echo "yukle.sh: doğrulama satırı yok (günlük $LOG)" >&2
    exit 1
fi
echo "yukle.sh: $P yüklendi ve doğrulandı (SHA-256 $(shasum -a 256 "$ELF" | cut -c1-16)…)"
