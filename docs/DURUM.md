# Durum

Son güncelleme: 2026-11-13

Bu belge projenin şu an nerede olduğunu söyler. Anlamlı bir iş parçası bittiğinde güncellenir. Kararlar ve düzeltmeler için [research/00-duzeltmeler-ve-kararlar.md](research/00-duzeltmeler-ve-kararlar.md) belgesine bakılır.

## Şu an

- **Araştırma aşaması kapandı.** 13 rapor yazıldı, 13'ü de bağımsız doğrulandı; düzeltmeler 00 numaralı belgede (01–13).
- **Mimari iskelet sürüm 0.3** (`docs/mimari/00-iskelet.md`, taslak, kullanıcı onayı bekliyor): 0.2'de fizik adımı 1,25 ms oldu; 0.3'te bölümler arası kararlar eklendi (ortak kabuller, tek seferlik olaylar, zaman çerçevesi ve iki ölçütlü tazelik, tekrar oynatma kaydı gereği, torpidonun kütle muhasebesi ve fırlatma modeli kademesi).
- **Dört bölümün ilk taslağı yazıldı:** `01-kart-yazilimi.md`, `02-ortak-cekirdek.md`, `04-simulator-cekirdegi.md`, `06-protokol-ve-hil.md`.
- **İncelemeler:** 02 ve 04 sol tarafından incelendi (`docs/mimari/inceleme/02-sol.md`, `04-sol.md`); ikisi de bölümü bu haliyle kabul etmedi, bulgular açık. 01 ve 06 incelemesi ile 05 bölümünün yazımı sürüyor.
- **Agent yapısı kuruldu:** tanımlar `.claude/agents/` altında.
- **Kod yok.** Kod dizinleri mimari belge onaylanınca eklenecek.
- **Tek odak:** simülasyon ortamını kurup STM32U5A9J-DK kartında çalıştırmak (00 belgesi, K8).

## Sıradaki adım

1. 02 ve 04 düzeltme turu (sol bulguları).
2. 01 ve 06 sol incelemesi ve düzeltmeleri.
3. 03, 05, 07, 08, 09 bölümlerinin yazımı.
4. Bütün bölümlerin tutarlılık denetimi, ardından iskeletin ve bölümlerin kullanıcı onayı.
5. Kodlama iş akışı (görev belgesi, dal ve pull request, yazan/inceleyen farklı aile, Mac ve Windows'ta otomatik derleme ve test, ölçüm kaydı). Kullanıcıya önerildi; onayı bekleniyor.

## Kodlamanın ilk işleri: ölçümler

Web araştırmasıyla kapanmayan belirsizlikler. Sonuçları çıkmadan ilgili tasarım kararı kesinleşmez (00 belgesi, Bölüm 7).

| Ölçüm | 00 belgesindeki karşılığı |
|---|---|
| Kartta yüksek hızlı USB: çalışıyor mu, gecikme ve kayıp | M1, M2 |
| Godot'da pencere görünmezken kamera görüntüsü üretimi (Mac ve Windows) | M13, M28 |
| Sonar hesap süresi | M11 |
| Kartta 400 Hz döngünün yükü ve en kötü durum süresi | M5 |
| Windows hedef bilgisayarının kaldırdığı kalite kademesi | M12, M20, M21, M30 |

## Açık konular

- Windows hedef bilgisayarının işlemci, bellek ve ekran kartı bilgisi bilinmiyor; kalite kademelerinin tabanı buna göre sabitlenecek (00 belgesi, Ö11).
- 00 belgesindeki açık sorular (Bölüm 9). Yarışmaya ve ardıl karta bağlı olanlar kapsam dışıdır (K7, K8).

## Kayıt

Yeni kayıt en üste eklenir; eski kayıtlar silinmez.

### 2026-11-13 · mimari taslakları ve ilk incelemeler

- İskelet 0.3'e çıktı (fizik adımı 1,25 ms; bölümler arası kararlar eklendi).
- 01, 02, 04, 06 bölüm taslakları yazıldı; hepsi taslak, kullanıcı onayı yok.
- 02 ve 04 sol incelemesinden geçti, ikisi de kabul edilmedi; bulgular açık, düzeltme turu yapılacak.
- 01 ve 06 incelemesi ile 05 bölümü sürüyor (bu kayda dahil değil).

### 2026-11-12 · agent yapısı ve mimari iskelet

- `.claude/agents/` altında altı agent tanımı eklendi (kritik uygulayıcı, uygulayıcı, hafif uygulayıcı, test yazarı, inceleyici, kayıt).
- Karar günlüğüne K11–K14 (mimar kod yazmaz, commit'i kayıt agent'ı atar, sol kodlayabilir ve yazan/inceleyen farklı aile, effort seviyeleri) ve Ö12 (en fazla iki agent) işlendi; `AGENTS.md` Çalışma yöntemi güncellendi.
- Mimari iskelet `docs/mimari/00-iskelet.md` yazıldı (taslak); bölümler yazılıyor.

### 2026-11-12 · kurallar ve kararlar

- Çalışma kurallarına kayıt ve süreklilik, dış içerik ve gerekçeli görüş kuralları eklendi (`AGENTS.md`).
- Karar günlüğüne K8 (tek odak), K9 (paneller web), K10 (araç parametreleri mimarda) ve Ö11 (kalite kademeleri) işlendi.
- Bu belge oluşturuldu.

### 2026-11-11 · araştırmanın kapanışı

- 12 (Godot entegrasyonu) ve 13 (donanım yol haritası) raporları yazıldı ve doğrulandı; 00 belgesi 01–13 kapsamına güncellendi.
- Proje araştırma ödevi olarak kayda geçti; yarışma şartnamesi araştırması iptal edildi (K7).
- Depo GitHub'a gönderildi.
