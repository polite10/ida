# Durum

Son güncelleme: 2026-11-15

Bu belge projenin şu an nerede olduğunu söyler. Anlamlı bir iş parçası bittiğinde güncellenir. Kararlar ve düzeltmeler için [research/00-duzeltmeler-ve-kararlar.md](research/00-duzeltmeler-ve-kararlar.md) belgesine bakılır.

## Şu an

- **Araştırma aşaması kapandı.** 13 rapor yazıldı, 13'ü de bağımsız doğrulandı; düzeltmeler 00 numaralı belgede (01–13).
- **Mimari çalışması tamamlandı; kullanıcı 2026-11-14'te bilinen açık bulgularla onayladı (K15).** Belgeler taslak sürüm numaralarını korur; açık bulgular sözleşme ve uygulama işlerinde kapatılır: iskelet 0.7 (`docs/mimari/00-iskelet.md`), dokuz bölüm taslağı, incelemeler ve açık bulgular kaydı (`docs/mimari/acik-bulgular.md`). İskelet 0.7'de mimar itkili torpido kararını geri aldı: torpido itkisiz, kısa menzilli basit simülasyon nesnesidir; 03 kapsamı emniyet zinciri ve arayüzlerle sınırlıdır.
- **Açık bulgular kaydı:** 233 bulgu (AB-001…AB-233); 224'ü açık (33 kritik, 94 önemli, 8 küçük, 89 önem belirtilmemiş), 9'u kapsam dışı. En çok bulgu A1 sözleşmesinde (47), A4 sözleşmesinde (26) ve atış kontrol uygulamasında (23). Belge ayrıca 29 satırlık bölümler arası uyumsuzluk tablosu ve 26 maddelik iskelet önerisi listesi içerir.
- **sol ikinci incelemeleri:** 01 (`inceleme/01-sol-2.md`): koşulsuz kabul yok; açık bulgularla sözleşme aşamasına taşınabilir; 4 kritik, 12 önemli, 1 küçük yeni bulgu. 05 (`inceleme/05-sol-2.md`): koşulsuz kabul yok; 1 kritik, 15 önemli yeni bulgu; yeni belge düzeltme turu önerilmiyor.
- **Süreç kararı (mimar, 2026-11-14):** inceleme-düzeltme döngüsü kesildi; kapanmamış bulgular sözleşme ya da uygulama aşamasında kapatılır.
- **Dalga sırası (kullanıcı onayladı, K15):** ölçüm prototipleri W0, arayüz sözleşmeleri W1; A1 ölçüm sonuçlarıyla tek aşamada donar. `docs/mimari/09-test-ve-is-akisi.md` sürüm 0.2.
- **`tools/sol-incele.sh`:** Claude'un yazdığı belge ya da kodun sol incelemesini başlatan betik (ilk ve ikinci inceleme kipi). sol'un yazma kipi bu depoda henüz denenmedi (ilk deneme W0'da).
- **Agent yapısı kuruldu:** tanımlar `.claude/agents/` altında.
- **W0 başladı:** ilk iki iş W0-MP1a ve W0-MP6a (`docs/gorevler/`).
- **Kart Mac'e bağlı** (ST-LINK portu, USB hub üzerinden); ikinci (kullanıcı) USB bağlantısı MP2'de gerekecek.
- **Ürün kodu yok;** W0 ölçüm prototipleri `olcum/` dallarında, `tools/olcum/` altında yazılıyor.
- **W0 durumu (2026-11-15):**
  - **W0-MP1a tamamlandı ve main'e birleşti** (PR #1). Rapor `docs/olcumler/MP1-arac-zinciri.md`, kod `tools/olcum/mp1/` (atılabilir prototip). K0, yedek, K1, K2 düzeltilmiş ölçütlere göre geçti; sol iki tur inceledi, kritik bulgu kalmadı. Mimari için bulgular (açık bulgular kaydına işlenecek): STM32U59x/5Ax'te 160 MHz'e geçişte geçici AHB ÷2 ve en az 5 µs bekleme gerekli; Picolibc CubeIDE 14.3.rel1 macOS paketinde yok; ST kart örneklerinin çoğu PLL'i MSIS'ten besliyor; kart revizyonu `REV_ID` 0x3000 ES0553 Rev 6 listesinde yok.
  - **W0-MP6a (Godot kamera):** tam kampanya 2026-11-15 gecesi kesintisiz koştu. Sonuç (sol incelemesi öncesi, düzeltme turu sürüyor): `force_draw(false, dt)` + `SubViewport` `UPDATE_ONCE` + eşzamanlı geri okuma yolu pencere örtülü, gizli ve simge durumundayken 5 dakika boyunca 9000/9000 kare üretti, içerik görünür referansla bit düzeyinde aynı; olağan çizim döngüsü bu durumlarda kare üretmedi; 640 × 480 geri okuma p99 ≈ 5,7 ms. sol incelemesi: 2 kritik, 4 önemli bulgu (çoğu raporlama; biri 5 dakikalık görünür koşunun yinelenmesi). Düzeltme turu kapsamı `docs/gorevler/W0-MP6a.md` içinde; dal `olcum/mp6-godot-kamera`.
  - **W0-MP7a (sonar süresi):** ölçüm tamamlandı, rapor dalda (`olcum/mp7-sonar-suresi`, henüz commit'lenmedi), sol incelemesi sürüyor. Ön sonuç: S1, S4 geçti; S3 geçmedi (orta sahne, 7 m, N = 80 000, D = 2, tek iş parçacığı: p99 36,6 ms; bütçe 9,33 ms alt sınır / 25,17 ms model tahmini); S2 biçimsel geçti ama kararlı değil: bölüm 05'in saçıcı hücresi kuralı enerjiyi ışın sayısına bağlı kılıyor (mimar kararı gerekiyor). Sahne boyutunun süreye etkisi küçük (10⁴ → 10⁶ üçgende en çok %10); yansıma derecesi 0 → 2 süreyi 2,6–3,3 katına çıkarıyor.
  - Yerel çalışma kopyaları (`.claude/worktrees/`) ve ham veri önbelleği depoya girmez.
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

0. Devam edildiğinde, sırayla: MP6a düzeltme turu ve birleştirme; MP7a sol incelemesi, düzeltme turu, birleştirme; sol yazma denemesi; MP2 (USB HS; kartın ikinci USB kablosu gerekir), MP3, MP4, MP5; MP8 Windows betiği; W0 sonu: ölçüm sonuçlarının iskelet §8'e ve açık bulgulara işlenmesi.
1. W0 ölçüm prototipleri (`docs/mimari/09-test-ve-is-akisi.md` 2.1 ve 6.1). MP1a bitti; sürüyor: MP6a, MP7a. Sırada: sol yazma denemesi, MP2–MP5, MP8 betiği.
2. W1 arayüz sözleşmeleri (A1–A8); açık bulgular bu aşamada kapatılır; iskelet önerileri W1 başında karara bağlanır.
3. Uygulama dalgaları (W2–W6).

## Kodlamanın ilk işleri: ölçümler

Web araştırmasıyla kapanmayan belirsizlikler. Sonuçları çıkmadan ilgili tasarım kararı kesinleşmez (00 belgesi, Bölüm 7).

| Ölçüm | 00 belgesindeki karşılığı |
|---|---|
| Kartta yüksek hızlı USB: çalışıyor mu, gecikme ve kayıp | M1, M2 |
| Godot'da pencere görünmezken kamera görüntüsü üretimi (Mac ve Windows) | M13, M28 |
| Sonar hesap süresi | M11 |
| Kartta 400 Hz döngünün yükü ve en kötü durum süresi | M5 |
| Windows hedef bilgisayarının kaldırdığı kalite kademesi | M12, M20, M21, M30 |

Kart gerektiren ölçümler için kart Mac'e iki kabloyla (hata ayıklayıcı portu ve kullanıcı USB portu) bağlanır; araç kurulumu için kullanıcı izni gerekir.

## Açık konular

- Windows hedef bilgisayarı: 8 GB bellek, dahili ekran kartı (K16); işlemci bilinmiyor; kalite kademesi tabanı MP8 ile sabitlenecek (Ö11).
- Bölüm 03'ün dar kapsamlı (yalnız emniyet zinciri ve arayüzler) düzeltme turu için kullanıcı yanıtı bekleniyor.
- Kartın kullanıcı USB bağlantısı (ikinci kablo) MP2 için bekleniyor.
- 00 belgesindeki açık sorular (Bölüm 9). Yarışmaya ve ardıl karta bağlı olanlar kapsam dışıdır (K7, K8).

## Kayıt

Yeni kayıt en üste eklenir; eski kayıtlar silinmez.

### 2026-11-15 · W0: MP1a birleşti, MP6a ve MP7a ölçüldü

- MP1a: düzeltme turlarıyla main'e birleşti (PR #1); kritik bulgu kalmadı.
- MP6a: tam kampanya koştu; `force_draw` yolu gizli pencerede 9000/9000 kare, geri okuma p99 ≈ 5,7 ms; sol 2 kritik, 4 önemli bulgu verdi, düzeltme turu sürüyor.
- MP7a: ölçüm bitti; S3 geçmedi (p99 36,6 ms), S2 saçıcı hücresi kuralı için mimar kararı gerekiyor; sol incelemesi sürüyor.
- Sıradaki: MP6a ve MP7a düzeltme ve birleştirme, sol yazma denemesi, MP2–MP5, MP8.

### 2026-11-14 · W0: MP1a incelendi, MP6a yarım, oturum durdu

- MP1a: sol incelemesi 7 bulgu verdi (5 kritik, 2 önemli); en önemlisi 160 MHz geçişinde eksik geçici AHB ÷2 adımı. Düzeltme turu bekliyor.
- MP6a: olağan çizim döngüsü görünmeyen pencerede kare üretmedi; `force_draw` kısa koşularda bit düzeyinde aynı kare üretti; G1–G3 tamamlanamadı, G4 geçti.
- MP7a başlamadan durduruldu; görev belgesi hazır.
- Sıra: MP1a düzeltme turu, MP6a kalan koşular, MP7a; ölçüm bulguları açık bulgular kaydına işlenecek.

### 2026-11-14 · W0: MP1a tamam, oturum durduruldu

- MP1a tamamlandı (dal `olcum/mp1-arac-zinciri` gönderildi, main'e birleştirilmedi); K0, flash yedeği, K1 geçti; K2 turları hatasız, tolerans ölçütü mimarca düzeltilecek; sol incelemesi sonucu işlenmedi.
- MP6a koşuları sürerken durdu (`olcum/mp6-godot-kamera`, commit'siz, rapor yok); MP7a başlamadan durduruldu, görev belgesi hazır.
- MP1a'nın mimari için bulguları (açık bulgulara işlenecek): Picolibc CubeIDE'nin 14.3.rel1 macOS paketinde yok (newlib-nano kullanıldı); ST'nin kart örneklerinin çoğu PLL'i MSIS'ten besliyor, HSE dizisi yalnız USBX örneklerinde; fabrika demosu silindi, yedeği yerel önbellekte.

### 2026-11-14 · mimari onaylandı, W0 başladı

- Kullanıcı mimariyi bilinen açık bulgularla onayladı; sıra W0 → W1 → uygulama (K15). Ölçümler için araç kurulumuna izin verildi.
- Windows hedef bilgisayarı: 8 GB bellek, dahili ekran kartı (K16); işlemci bilinmiyor.
- Kart Mac'e bağlı; STLINK_V3 numaralanıyor (mimarın gözlemi). Mac'te STM32CubeIDE var; cmake, ninja ve Godot kurulu değil.
- İki görev belgesi yazıldı (`docs/gorevler/W0-MP1a.md`, `docs/gorevler/W0-MP6a.md`); işler başlatılıyor.

### 2026-11-14 · dalga sırası önerisi ve sol inceleme betiği

- Mimar önerisi: ölçüm prototipleri W0, arayüz sözleşmeleri W1; A1 ölçüm sonuçlarıyla tek aşamada donar. `09-test-ve-is-akisi.md` sürüm 0.2; `acik-bulgular.md` yalnız dalga etiketleri güncellendi (bulgu içeriği ve sayılar değişmedi). Kullanıcı onayı mimari onayıyla birlikte bekleniyor.
- `tools/sol-incele.sh` eklendi (hafif uygulayıcı yazdı; sol inceledi: 4 bulgu, 1 kritik ve 3 önemli, tek turda giderildi). Düzeltmeden önce gerçek sol çağrısıyla bir kez çalıştı; düzeltmeden sonra yalnız kuru çalıştırma (`DRY_RUN=1`) ve macOS bash 3.2 ile denendi.
- `AGENTS.md` "Belge kuralları"na betiğin kullanımını söyleyen madde eklendi.
- sol'un yazma kipi bu depoda henüz denenmedi (ilk deneme W0'da).

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
