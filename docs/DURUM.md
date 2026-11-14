# Durum

Son güncelleme: 2026-11-14

Bu belge projenin şu an nerede olduğunu söyler. Anlamlı bir iş parçası bittiğinde güncellenir. Kararlar ve düzeltmeler için [research/00-duzeltmeler-ve-kararlar.md](research/00-duzeltmeler-ve-kararlar.md) belgesine bakılır.

## Şu an

- **Araştırma aşaması kapandı.** 13 rapor yazıldı, 13'ü de bağımsız doğrulandı; düzeltmeler 00 numaralı belgede (01–13).
- **Mimari iskelet sürüm 0.7** (`docs/mimari/00-iskelet.md`, taslak, kullanıcı onayı bekliyor). 0.4: gövde referans noktası sabit geometrik nokta; aralık boyunca toplanan ölçümlerin zaman damgası. 0.5: sanal emniyet girdileri ve fırlatıcı durum geri okuması; olay kimliğinin kapsamı; sonar tarama satırının zaman damgası. 0.6: kamera algılama bileşeninin bağlantısı A8; malzeme değerlerinin yeri; türetilmiş ağların depoya girmemesi. **0.7: mimar, 0.5'teki "torpido itkilidir (yaklaşık 3 m/s, 10 m menzil)" kararını geri aldı; 0.5'teki itkili torpido ifadesi artık geçerli değil.** Torpido yeniden itkisiz, kısa menzilli basit bir simülasyon nesnesidir; itkili torpido ve hareketli hedefe önleme hesabı proje kapsamı dışındadır; sonarın göremediği yakın bölgeyle çakışma kabul edilen bir sınırdır. 03 numaralı bölümün kapsamı emniyet zinciri ve arayüzlerle sınırlandı.
- **Dokuz mimari bölümün hepsi yazıldı.** 08 (laboratuvar uygulaması) ve 09 (test ve iş akışı) kısa biçimde yazıldı; sol incelemesinden geçmedi. 01 (sürüm 0.2) ve 05 için düzeltme turu tamamlandı.
- **Süreç kararı (mimar, 2026-11-14):** inceleme ve düzeltme döngüsü yakınsamadığı ve her tur yüz binlerce token tükettiği için döngü kesildi; başka düzeltme turu başlatılmayacak. Kapanmamış bulgular tek bir açık bulgular kaydında (`docs/mimari/acik-bulgular.md`, henüz yazılmadı) toplanacak; her bulgu kodlamadan önceki arayüz sözleşmesinde ya da ilgili uygulama işinde kapatılacak. Mimari "taslak ve bilinen açık bulgular" durumunda kullanıcı onayına sunulacak.
- **Süren işler (bu kayda dahil değil):** 01 ve 05 için sol ikinci incelemesi.
- **03 durumu:** düzeltme yapılmadı; ilk taslak duruyor (kapsam emniyet zinciri ve arayüzlerle sınırlı, iskelet 0.7).
- **Çalışma kuralı (mimarın önerisi, karar değil):** inceleme-düzeltme döngüsü kesildi; kalan bulgular açık bulgular kaydında toplanır ve sözleşme ya da uygulama aşamasında kapatılır.
- **Agent yapısı kuruldu:** tanımlar `.claude/agents/` altında.
- **Kod yok.** Kod dizinleri mimari belge onaylanınca eklenecek.
- **Tek odak:** simülasyon ortamını kurup STM32U5A9J-DK kartında çalıştırmak (00 belgesi, K8).

### Bölüm durumu

| Bölüm | Yazım | İlk inceleme | Düzeltme | İkinci inceleme |
|---|---|---|---|---|
| 01 kart yazılımı | var | var (kabul yok) | yapıldı (0.2) | ikinci inceleme sürüyor |
| 02 ortak çekirdek | var | var | yapıldı | var: 4 kritik açık, kabul yok |
| 03 atış kontrol | var | var: 12 kritik açık, kabul yok | yapılmadı | - |
| 04 simülatör çekirdeği | var | var | yapıldı | var: kritik yok, 5 önemli açık |
| 05 sensörler ve sonar | var | var (kabul yok) | yapıldı | ikinci inceleme sürüyor |
| 06 protokol ve HIL | var | var (kabul yok) | yapıldı (0.2) | var: 5 kritik açık, kabul yok |
| 07 görselleştirme | var | var: 8 kritik açık, kabul yok | yapılmadı | - |
| 08 laboratuvar uygulaması | var | - | - | - |
| 09 test ve iş akışı | var | - | - | - |

## Sıradaki adım

1. Açık bulgular kaydının derlenmesi (`docs/mimari/acik-bulgular.md`).
2. Mimarinin kullanıcı onayı ("taslak ve bilinen açık bulgular").
3. Arayüz sözleşmeleri ve ölçüm işleri; açık bulgular bu aşamada kapatılır.
4. Kodlama iş akışı (görev belgesi, dal ve pull request, yazan/inceleyen farklı aile, Mac ve Windows'ta otomatik derleme ve test, ölçüm kaydı). Kullanıcıya önerildi; onayı bekleniyor.

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
