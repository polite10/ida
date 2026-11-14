# Durum

Son güncelleme: 2026-11-14

Bu belge projenin şu an nerede olduğunu söyler. Anlamlı bir iş parçası bittiğinde güncellenir. Kararlar ve düzeltmeler için [research/00-duzeltmeler-ve-kararlar.md](research/00-duzeltmeler-ve-kararlar.md) belgesine bakılır.

## Şu an

- **Araştırma aşaması kapandı.** 13 rapor yazıldı, 13'ü de bağımsız doğrulandı; düzeltmeler 00 numaralı belgede (01–13).
- **Mimari çalışması taslak olarak tamamlandı, kullanıcı onayı bekliyor:** iskelet 0.7 (`docs/mimari/00-iskelet.md`), dokuz bölüm taslağı, incelemeler ve açık bulgular kaydı (`docs/mimari/acik-bulgular.md`). İskelet 0.7'de mimar itkili torpido kararını geri aldı: torpido itkisiz, kısa menzilli basit simülasyon nesnesidir; 03 kapsamı emniyet zinciri ve arayüzlerle sınırlıdır.
- **Açık bulgular kaydı:** 233 bulgu (AB-001…AB-233); 224'ü açık (33 kritik, 94 önemli, 8 küçük, 89 önem belirtilmemiş), 9'u kapsam dışı. En çok bulgu A1 sözleşmesinde (47), A4 sözleşmesinde (26) ve atış kontrol uygulamasında (23). Belge ayrıca 29 satırlık bölümler arası uyumsuzluk tablosu ve 26 maddelik iskelet önerisi listesi içerir.
- **sol ikinci incelemeleri:** 01 (`inceleme/01-sol-2.md`): koşulsuz kabul yok; açık bulgularla sözleşme aşamasına taşınabilir; 4 kritik, 12 önemli, 1 küçük yeni bulgu. 05 (`inceleme/05-sol-2.md`): koşulsuz kabul yok; 1 kritik, 15 önemli yeni bulgu; yeni belge düzeltme turu önerilmiyor.
- **Süreç kararı (mimar, 2026-11-14):** inceleme-düzeltme döngüsü kesildi; kapanmamış bulgular sözleşme ya da uygulama aşamasında kapatılır.
- **Agent yapısı kuruldu:** tanımlar `.claude/agents/` altında.
- **Kod yok.** Kod dizinleri mimari belge onaylanınca eklenecek.
- **Tek odak:** simülasyon ortamını kurup STM32U5A9J-DK kartında çalıştırmak (00 belgesi, K8).

### Bölüm durumu

| Bölüm | Yazım | İlk inceleme | Düzeltme | İkinci inceleme |
|---|---|---|---|---|
| 01 kart yazılımı | var | var (kabul yok) | yapıldı (0.2) | var: koşulsuz kabul yok; 4 kritik, 12 önemli, 1 küçük yeni bulgu |
| 02 ortak çekirdek | var | var | yapıldı | var: 4 kritik açık, kabul yok |
| 03 atış kontrol | var | var: 12 kritik açık, kabul yok | yapılmadı | - |
| 04 simülatör çekirdeği | var | var | yapıldı | var: kritik yok, 5 önemli açık |
| 05 sensörler ve sonar | var | var (kabul yok) | yapıldı | var: koşulsuz kabul yok; 1 kritik, 15 önemli yeni bulgu |
| 06 protokol ve HIL | var | var (kabul yok) | yapıldı (0.2) | var: 5 kritik açık, kabul yok |
| 07 görselleştirme | var | var: 8 kritik açık, kabul yok | yapılmadı | - |
| 08 laboratuvar uygulaması | var | - | - | - |
| 09 test ve iş akışı | var | - | - | - |

## Sıradaki adım

1. Mimarinin kullanıcı onayı ("taslak ve bilinen açık bulgular").
2. Arayüz sözleşmeleri (A1–A8); açık bulgular bu aşamada kapatılır.
3. Ölçüm işleri (`docs/mimari/09-test-ve-is-akisi.md`).
4. Uygulama dalgaları (kodlama iş akışı; kullanıcı onayı bekleniyor).

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

### 2026-11-14 · açık bulgular kaydı, mimari onaya hazır

- `docs/mimari/acik-bulgular.md` yazıldı: 233 bulgu, 224'ü açık (33 kritik, 94 önemli, 8 küçük, 89 önem belirtilmemiş), 9'u kapsam dışı; 29 satırlık uyumsuzluk tablosu ve 26 maddelik iskelet önerisi listesi.
- sol ikinci incelemeleri kayda geçti: `inceleme/01-sol-2.md` ve `inceleme/05-sol-2.md`.
- Mimari çalışması taslak olarak tamamlandı; kullanıcı onayı bekleniyor. Sıradaki: onay, arayüz sözleşmeleri, ölçüm işleri.

### 2026-11-14 · dokuz bölüm tamam, döngü kesildi

- 08 ve 09 yazıldı (kısa biçim, incelenmedi); dokuz bölümün hepsi taslak olarak mevcut.
- 01 (0.2) ve 05 düzeltme turları yapıldı (`inceleme/01-yanit.md`, `inceleme/05-yanit.md`).
- Mimarın süreç kararı: inceleme-düzeltme döngüsü yakınsamadığı ve çok token tükettiği için kesildi; kalan bulgular `acik-bulgular.md` kaydında toplanacak, sözleşme ya da uygulama aşamasında kapatılacak.
- Süren (bu kayda dahil değil): 01 ve 05 sol ikinci incelemesi.

### 2026-11-14 · iskelet 0.7, bölüm 06 düzeltmesi, 03 durdurması

- İskelet 0.7: itkili torpido kararı geri alındı (itkisiz, kısa menzilli basit nesne; itkili torpido ve önleme hesabı kapsam dışı); 03 kapsamı emniyet zinciri ve arayüzlerle sınırlandı.
- 06 düzeltme turu yapıldı (sürüm 0.2, `inceleme/06-yanit.md`). İkinci sol incelemesi (`inceleme/06-sol-2.md`): kabul yok, 5 kritik ve 15 önemli yeni bulgu.
- 07 ilk sol incelemesi (`inceleme/07-sol.md`): kabul yok, 8 kritik ve 15 önemli bulgu.
- 03 düzeltmesi durdu: agent çıktısı güvenlik sınıflandırıcısı tarafından durdurulduğu için dosyaya dokunulmadı; kapsam daraltma için kullanıcı onayı bekleniyor.
- Süren (bu kayda dahil değil): 05 ve 01 düzeltme turları. Bekleyen: 08 ve 09 yazımı.

### 2026-11-13 · bölüm 03, 05, 07 ve düzeltme turları

- İskelet 0.4–0.6 (gövde referans noktası, itkili torpido, sanal emniyet girdileri, kamera bileşeni bağlantısı A8, malzeme değerlerinin yeri).
- 03, 05, 07 taslakları yazıldı; hepsi taslak, kullanıcı onayı yok.
- 02 ve 04 düzeltme turu yapıldı (`inceleme/02-yanit.md`, `04-yanit.md`). İkinci incelemeler: 02 kabul edilmedi (4 kritik yeni bulgu); 04'te kritik bulgu kalmadı, 5 önemli bulgu açık.
- 01, 03, 05, 06 ilk sol incelemesi: dördü de kabul edilmedi.
- Süren (bu kayda dahil değil): 06 ve 03 düzeltme turu, 07 incelemesi. Bekleyen: 01 ve 05 düzeltmesi, 08 ve 09 yazımı.

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
