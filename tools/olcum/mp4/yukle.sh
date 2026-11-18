#!/bin/sh
# W0-MP4a: aygıt yazılımını dahili flash'a yükler, doğrular ve sıfırlar (MP1 yukle.sh'in yaklaşımı).
# Önkoşul: geçerli fabrika yedeği (tools/olcum/mp1/yedek-dogrula.sh); yoksa yükleme yapılmaz.
# Yalnız yazılan sayfalar silinir; seçenek baytlarına ve harici flash'a dokunulmaz.
# Kullanım: tools/olcum/mp4/yukle.sh o2|og [günlük]
set -eu
KOK="$(cd "$(dirname "$0")" && pwd)"
. "$KOK/ortam.sh"
V="${1:?derleme: o2|og}"
ELF="$KOK/derleme/$V/mp4.elf"
LOG="${2:-$MP4_VERI/yukle-son.log}"
mkdir -p "$(dirname "$LOG")"
[ -f "$ELF" ] || { echo "yukle.sh: yok: $ELF (önce derle.sh)" >&2; exit 2; }
sh "$KOK/../mp1/yedek-dogrula.sh" || { echo "yukle.sh: geçerli yedek yok; yükleme yapılmadı" >&2; exit 3; }
if "$CLI" -c port=SWD -w "$ELF" -v -rst > "$LOG.ham" 2>&1; then rc=0; else rc=$?; fi
sed 's/\x1b\[[0-9;]*m//g' < "$LOG.ham" > "$LOG"; rm -f "$LOG.ham"
grep -E "Error|error|Download verified|File download complete|Erasing|Time elapsed|Software reset" "$LOG" || true
if [ "$rc" -ne 0 ]; then echo "yukle.sh: programlayıcı çıkış kodu $rc" >&2; exit 1; fi
if ! grep -q "Download verified successfully" "$LOG"; then echo "yukle.sh: doğrulama satırı yok" >&2; exit 1; fi
echo "yukle.sh: $V yüklendi ve doğrulandı ($(shasum -a 256 "$ELF" | cut -c1-16)…)"
