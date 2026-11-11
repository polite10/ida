#!/usr/bin/env bash
# Bir araştırma raporunun kritik iddialarını Codex ile bağımsız doğrular.
#
# Kullanım:
#   tools/codex-dogrula.sh <rapor-no> [ek-kontrol]
#
#   <rapor-no>    docs/research/ altındaki raporun iki haneli numarası (ör. 08)
#   [ek-kontrol]  İsteğe bağlı: özellikle kontrol edilmesi istenenler; düz metin
#                 ya da bu metni içeren bir dosyanın yolu
#
# Çıktı: docs/research/dogrulama/<rapor-no>-codex.md
#
# Ortam değişkenleri:
#   CODEX_MODEL   Kullanılacak model (varsayılan: gpt-6.1-sol)
#   DRY_RUN=1     Codex'i çalıştırmadan istemi ekrana yazar
#
# Codex yalnızca okur ve web'de arar (read-only sandbox); dosya değiştirmez.
# Proje bağlamını depo kökündeki AGENTS.md dosyasından alır.
set -euo pipefail

if [[ $# -lt 1 ]]; then
  sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
  exit 64
fi

KOK="$(cd "$(dirname "$0")/.." && pwd)"
ARASTIRMA="docs/research"
CIKTI="$ARASTIRMA/dogrulama"
LOG="$KOK/.codex-log"
MODEL="${CODEX_MODEL:-gpt-6.1-sol}"
NO="$1"

cd "$KOK"
RAPOR="$(ls "$ARASTIRMA/$NO"-*.md 2>/dev/null | head -1 || true)"
if [[ -z "$RAPOR" ]]; then
  echo "hata: $ARASTIRMA/$NO-*.md bulunamadı" >&2
  exit 66
fi
AD="$(basename "$RAPOR" .md)"

EK=""
if [[ $# -ge 2 ]]; then
  if [[ -f "$2" ]]; then EK="$(cat "$2")"; else EK="$2"; fi
fi

ISTEM="Bağımsız bir doğrulayıcısın. Bu depodaki bir araştırma raporunu başka bir yapay zeka modeli yazdı; görevin iddialarını sınamak, rapora güvenmemek.

Proje bağlamı ve verilmiş kararlar için depo kökündeki AGENTS.md dosyasını ve $ARASTIRMA/00-duzeltmeler-ve-kararlar.md belgesini oku.

Doğrulanacak rapor: $RAPOR
Aynı dizinde diğer raporlar, $CIKTI/ altında önceki doğrulamalar var; gerektiğinde oku.

Yapılacaklar:
1. Raporu baştan sona oku.
2. 'Bağımsız doğrulanması gereken kritik iddialar' bölümündeki her maddeyi canlı web aramasıyla birincil kaynaktan kontrol et. Hafızadan yanıtlama; kaynağı aç ve oku.
3. Raporun geri kalanında mimari kararı etkileyecek, şüpheli ya da kaynaksız gördüğün diğer iddiaları da kontrol et. Sayısal hesapları kendin yeniden yap.
${EK:+4. Ayrıca özellikle kontrol et: $EK}

Kurallar:
- Hiçbir dosyayı değiştirme, hiçbir şey kurma. Yalnızca oku ve web'de ara.
- Bir kaynağa erişemediysen bunu açıkça yaz; erişemediğin kaynağı doğrulanmış sayma.
- Dosya yollarını depo köküne göre göreli yaz.
- Yanıtın Türkçe olsun.

Son yanıtın tam olarak şu yapıda bir Markdown belgesi olsun (başka hiçbir şey ekleme):

# Codex doğrulaması: $AD

## Hüküm tablosu
| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
Hüküm değerleri: DOĞRU / KISMEN DOĞRU / YANLIŞ / DOĞRULANAMADI

## Yanlış ya da yanıltıcı bulunanlar
(her biri için: raporda ne yazıyor, doğrusu ne, kaynak)

## Rapor dışı ek bulgular
(raporun atladığı ama mimariyi etkileyen noktalar)

## Raporlar arası çelişki değerlendirmesi
(başka raporlarla ya da 00 numaralı belgeyle çelişki varsa)

## Mimari için net sonuçlar
(en fazla 8 madde)"

if [[ "${DRY_RUN:-0}" == "1" ]]; then
  printf '%s\n' "$ISTEM"
  exit 0
fi

mkdir -p "$CIKTI" "$LOG"
echo "[$NO] başladı: $(date +%H:%M:%S) model=$MODEL"
printf '%s\n' "$ISTEM" | codex exec -m "$MODEL" -s read-only --skip-git-repo-check --ephemeral \
  -c 'web_search="live"' -C "$KOK" -o "$CIKTI/$NO-codex.md" - > "$LOG/$NO.log" 2>&1
echo "[$NO] bitti: $(date +%H:%M:%S) -> $CIKTI/$NO-codex.md"
