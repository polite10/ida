#!/bin/sh
# W0-MP3a: aygıt yazılımını dahili flash'a yükler, doğrular, sıfırlar (MP1/MP4 yukle.sh yaklaşımı).
# Önkoşul: geçerli fabrika yedeği (tools/olcum/mp1/yedek-dogrula.sh); yoksa yükleme yapılmaz.
# Yalnız yazılan sayfalar silinir; seçenek baytlarına, harici flash'a ve ST-LINK aygıt yazılımına dokunulmaz.
# Kullanım: tools/olcum/mp3/yukle.sh [günlük]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
ELF="$KOK/derleme/fw/mp3.elf"
LOG="${1:-$MP3_VERI/yukle-$(date +%Y%m%d-%H%M%S).log}"
mkdir -p "$(dirname "$LOG")"
[ -f "$ELF" ] || { echo "yukle.sh: yok: $ELF (önce derle.sh)" >&2; exit 2; }
sh "$KOK/../mp1/yedek-dogrula.sh" || { echo "yukle.sh: geçerli yedek yok; yükleme yapılmadı" >&2; exit 3; }
if "$CLI" -c port=SWD -w "$ELF" -v -rst > "$LOG.ham" 2>&1; then rc=0; else rc=$?; fi
sed 's/\x1b\[[0-9;]*m//g' < "$LOG.ham" > "$LOG"; rm -f "$LOG.ham"
grep -E "Error|error|Download verified|File download complete|Erasing|Time elapsed|Software reset" "$LOG" || true
if [ "$rc" -ne 0 ]; then echo "yukle.sh: programlayıcı çıkış kodu $rc" >&2; exit 1; fi
grep -q "Download verified successfully" "$LOG" || { echo "yukle.sh: doğrulama satırı yok" >&2; exit 1; }
echo "yukle.sh: yüklendi ve doğrulandı (ELF SHA-256 $(shasum -a 256 "$ELF" | cut -c1-16)…, kimlik $(cat "$KOK/derleme/fw/kimlik.txt"))"
