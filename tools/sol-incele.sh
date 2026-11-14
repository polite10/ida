#!/usr/bin/env bash
# Bir mimari bölümü, sözleşmeyi ya da kodu sol (Codex) ile bağımsız inceler.
#
# Kullanım:
#   tools/sol-incele.sh [-2 <yanit-dosyasi> -1 <ilk-inceleme>] <hedef> <cikti-dosyasi> [ek-talimat]
#
#   <hedef>         İncelenecek dosya ya da dizin (depo köküne göre)
#   <cikti-dosyasi> İncelemenin yazılacağı Markdown dosyası (depo köküne göre)
#   [ek-talimat]    İsteğe bağlı: özellikle kontrol edilmesi istenenler; düz metin
#                   ya da bu metni içeren bir dosyanın yolu
#   -2 <yanit>      İkinci inceleme kipi: yazarın yanıt dosyası; -1 ile birlikte verilir
#   -1 <ilk>        İkinci inceleme kipinde ilk incelemenin dosyası
#
# Ortam değişkenleri:
#   CODEX_MODEL   Kullanılacak model (varsayılan: gpt-6.1-sol)
#   DRY_RUN=1     Codex'i çalıştırmadan istemi ekrana yazar
#
# Seçenekler konumsal argümanlardan önce gelir. Codex effort'u açıkça `high` verilir (karar günlüğü K14).
# Codex yalnızca okur ve web'de arar (read-only sandbox); dosya değiştirmez.
# Proje bağlamını depo kökündeki AGENTS.md dosyasından alır.
# Çıkış kodları: 64 kullanım hatası (çıktı dosyası bir girdiyle aynıysa da), 66 dosya bulunamadı.
# Günlük: .codex-log/<çıktı-yolu>-<zaman damgası>.log
set -euo pipefail

KOK="$(cd "$(dirname "$0")/.." && pwd)"
LOG="$KOK/.codex-log"
MODEL="${CODEX_MODEL:-gpt-6.1-sol}"
YANIT=""
ILK=""

kullanim() {
  sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'
}

while getopts ":1:2:" SEC; do
  case "$SEC" in
    1) ILK="$OPTARG" ;;
    2) YANIT="$OPTARG" ;;
    *) kullanim; exit 64 ;;
  esac
done
shift $((OPTIND - 1))

if [[ $# -lt 2 || $# -gt 3 ]]; then
  kullanim
  exit 64
fi
if [[ -n "$YANIT" && -z "$ILK" ]] || [[ -z "$YANIT" && -n "$ILK" ]]; then
  echo "hata: -2 ve -1 birlikte verilmeli" >&2
  exit 64
fi

HEDEF="$1"
CIKTI="$2"
cd "$KOK"

for DOSYA in "$HEDEF" ${YANIT:+"$YANIT"} ${ILK:+"$ILK"}; do
  if [[ ! -e "$DOSYA" ]]; then
    echo "hata: $DOSYA bulunamadı" >&2
    exit 66
  fi
done

for DOSYA in "$HEDEF" ${YANIT:+"$YANIT"} ${ILK:+"$ILK"}; do
  if [[ "$CIKTI" -ef "$DOSYA" ]]; then
    echo "hata: çıktı dosyası girdiyle aynı: $DOSYA" >&2
    exit 64
  fi
done

AD="$(basename "$HEDEF")"
AD="${AD%.md}"
CIKTI_AD="$(basename "$CIKTI" .md)"
GUNLUK_AD="${CIKTI//\//_}"
GUNLUK_AD="${GUNLUK_AD#"${GUNLUK_AD%%[!_]*}"}"
GUNLUK_AD="${GUNLUK_AD%.md}"

EK=""
if [[ $# -ge 3 ]]; then
  if [[ -f "$3" ]]; then EK="$(cat "$3")"; else EK="$3"; fi
fi

if [[ -n "$YANIT" ]]; then
  GIRIS="Bu bir İKİNCİ incelemedir. İlk inceleme: $ILK. Yazarın yanıtı: $YANIT. Her ilk bulgunun hedefte gerçekten giderilip giderilmediğini hedefin kendisinde denetle; yazarın yanıtına güvenme. Yazar haklıysa gerekçesini sınayarak söyle."
  DUZEN="# sol ikinci incelemesi: $AD

## Hüküm

## İlk incelemedeki maddelerin durumu
| # | Durum | Not |
Durum değerleri: giderildi / kısmen / giderilmedi / yazar haklı

## Yeni bulgular
| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |

## Yeniden hesaplananlar

## İskelete ya da diğer bölümlere notlar"
else
  GIRIS="Bu ilk incelemedir."
  DUZEN="# sol incelemesi: $AD

## Hüküm

## Düzeltilmesi gerekenler
| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |

## Yeniden hesaplananlar

## İskelete ya da diğer bölümlere notlar"
fi

ISTEM="Bağımsız bir inceleyicisin. Bu depodaki hedefi başka bir yapay zeka modeli yazdı; görevin hatayı bulmak, onaylamak değil.

Proje bağlamı ve verilmiş kararlar için depo kökündeki AGENTS.md dosyasını, docs/mimari/00-iskelet.md ve docs/research/00-duzeltmeler-ve-kararlar.md belgelerini oku.

İncelenecek hedef: $HEDEF
$GIRIS

Yapılacaklar:
1. Hedefi baştan sona oku.
2. Hedef bir mimari bölümü ya da sözleşmeyse iskeletle ve ilgili komşu belgelerle tutarlılığına bak. Hedef kodsa görev belgesine ve sözleşmelere uygunluğuna, doğruluğuna ve sınır durumlarına bak.
3. Sayısal hesapları kendin yeniden yap.
4. Dış olguları (sürüm, API, donanım özelliği) canlı web aramasıyla birincil kaynaktan denetle; hafızadan yanıtlama.
${EK:+5. Ayrıca özellikle kontrol et: $EK}

Kapsam sınırı: atış kontrol içeriği emniyet zinciri ve arayüzlerle sınırlıdır; inceleme torpidonun yeteneğini artıran ya da hedefleme çözümünü ayrıntılandıran öneri vermez.

Önem değerleri yalnızca şunlardır (tanımlar docs/mimari/09-test-ve-is-akisi.md 2.4):
- kritik: emniyet davranışı, yanlış sayısal sonuç, veri bozulması, sözleşme ihlali, bellek güvensizliği, kabul testini geçersiz kılan hata.
- önemli: çalışmayı bozmayan ama giderilmesi gereken tutarsızlık.
- küçük: biçim, ad, açıklama.

Kurallar:
- Hiçbir dosyayı değiştirme, hiçbir şey kurma. Yalnızca oku ve web'de ara.
- Bir kaynağa erişemediysen bunu açıkça yaz; erişemediğin kaynağı doğrulanmış sayma.
- Dosya yollarını depo köküne göre göreli yaz.
- Yanıtın Türkçe olsun.

Son yanıtın tam olarak şu yapıda bir Markdown belgesi olsun (başka hiçbir şey ekleme):

$DUZEN"

if [[ "${DRY_RUN:-0}" == "1" ]]; then
  printf '%s\n' "$ISTEM"
  exit 0
fi

mkdir -p "$(dirname "$CIKTI")" "$LOG"
GUNLUK="$LOG/$GUNLUK_AD-$(date +%Y%m%d-%H%M%S).log"
echo "[$CIKTI_AD] başladı: $(date +%H:%M:%S) model=$MODEL"
printf '%s\n' "$ISTEM" | codex exec -m "$MODEL" -s read-only --skip-git-repo-check --ephemeral \
  -c 'web_search="live"' -c 'model_reasoning_effort="high"' -C "$KOK" -o "$CIKTI" - > "$GUNLUK" 2>&1
echo "[$CIKTI_AD] bitti: $(date +%H:%M:%S) -> $CIKTI (günlük: $GUNLUK)"
