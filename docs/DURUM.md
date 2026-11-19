# Durum

Son güncelleme: 2026-11-19

Bu belge projenin şu an nerede olduğunu söyler. Anlamlı bir iş parçası bittiğinde güncellenir. Kararlar ve düzeltmeler için [research/00-duzeltmeler-ve-kararlar.md](research/00-duzeltmeler-ve-kararlar.md) belgesine bakılır.

## Şu an

- **MP3 sürüyor (2026-11-19; ölçüm prototipi, ürün kodu değil):** dal `olcum/mp3-vcp` (main'e birleşmedi, henüz commit'lenmedi; çalışma kopyası `.claude/worktrees/mp3-vcp`). Kod, masaüstü testleri (219/219) ve iki düzeltme turu tamam; sol kapanış incelemesi (`MP3-sol-3.md`): bir önemli bulgu (A1: kampanya profilinde 64 bekleyen sınırı kayıttan değiştirilebiliyor) açık; mimar (o tarihte Codex) kabul kapsamını dondurulmuş kampanya ayarlarına daralttı (`docs/gorevler/W0-MP3a-kapsam-karari.md`, dalda). Uzun kampanya 2026-11-19 sabahı başladı; 600 s çift yönlü 256 B akış koşuları: 921600 baud GEÇTİ (159 920 çerçeve), 2 000 000 baud GEÇTİ (347 827 çerçeve, iki yönde eşit); 3 000 000 baud koşusu 09:40'ta kart bağlantısı koptuğu için GEÇERSİZ (ölçüt ihlali değil, kopma; yinelenecek). Kalan: 3 ve 6 Mbaud akış koşuları, geçen en yüksek hızda dört echo (gecikme) koşusu, rapor, sol'un son veri incelemesi, PR. Kampanya 2026-11-19 akşamı kaldığı yerden sürdürülüyor.
- **Mimarlık (K19, 2026-11-19):** mimar oturumunu yeniden Fable yürütüyor; K18'in görev dağılımı sürüyor.
- **MP4a tamamlandı (2026-11-18, daraltılmış kapsamla; ölçüm prototipi, ürün kodu değil):** dal `olcum/mp4-dongu-yuku`. D1/D2 geçmedi, D3/D4 geçti. O2 ağır yük 400 Hz: maksimum 3,32238125 ms, aşım 241000/241000; O2 200 Hz: 121000 adımda aşım 0. Ölçülen yer tutucu çekirdek adımıdır ve taşımayı içermez; gerçek çekirdek ve taşıma hızı kesinleşmedi, mevcut kararlar korunur. Mimar, taşmalı histogramların kapsam dışı bırakılıp reddedilmesine karar verdi (Y5). Çözümleyici 103 denetimi geçti; sol kapanış incelemesi kritik/önemli bulgu görmedi (`docs/olcumler/inceleme/MP4-sol-kapanis.md`; küçük bulgu Y8 giderildi). Rapor: `docs/olcumler/MP4-dongu-yuku.md`. Main'e PR #5 ile squash birleşti (b2b9346).
- **Tek kablo kararı (K17, 2026-11-16):** sistem yalnız kartın ST-LINK portuyla çalışır; yükleme, hata ayıklama ve kart–simülatör bağlantısı (A1) sanal seri porttan (USART1) geçer; kullanıcı USB girişi (USB HS) kullanılmaz; MP2 ve kart tarafı USB yığını plandan çıktı; kontrol hızı ve bariyer bütçesi MP3'e göre belirlenir.
- **Araştırma aşaması kapandı.** 13 rapor yazıldı, 13'ü de bağımsız doğrulandı; düzeltmeler 00 numaralı belgede (01–13).
- **Mimari çalışması tamamlandı; kullanıcı 2026-11-14'te bilinen açık bulgularla onayladı (K15).** Belgeler taslak sürüm numaralarını korur; açık bulgular sözleşme ve uygulama işlerinde kapatılır: iskelet 0.7 (`docs/mimari/00-iskelet.md`), dokuz bölüm taslağı, incelemeler ve açık bulgular kaydı (`docs/mimari/acik-bulgular.md`). İskelet 0.7'de mimar itkili torpido kararını geri aldı: torpido itkisiz, kısa menzilli basit simülasyon nesnesidir; 03 kapsamı emniyet zinciri ve arayüzlerle sınırlıdır.
- **Açık bulgular kaydı:** 233 bulgu (AB-001…AB-233); 224'ü açık (33 kritik, 94 önemli, 8 küçük, 89 önem belirtilmemiş), 9'u kapsam dışı. En çok bulgu A1 sözleşmesinde (47), A4 sözleşmesinde (26) ve atış kontrol uygulamasında (23). Belge ayrıca 29 satırlık bölümler arası uyumsuzluk tablosu ve 26 maddelik iskelet önerisi listesi içerir.
- **sol ikinci incelemeleri:** 01 (`inceleme/01-sol-2.md`): koşulsuz kabul yok; açık bulgularla sözleşme aşamasına taşınabilir; 4 kritik, 12 önemli, 1 küçük yeni bulgu. 05 (`inceleme/05-sol-2.md`): koşulsuz kabul yok; 1 kritik, 15 önemli yeni bulgu; yeni belge düzeltme turu önerilmiyor.
- **Süreç kararı (mimar, 2026-11-14):** inceleme-düzeltme döngüsü kesildi; kapanmamış bulgular sözleşme ya da uygulama aşamasında kapatılır.
- **Dalga sırası (kullanıcı onayladı, K15):** ölçüm prototipleri W0, arayüz sözleşmeleri W1; A1 ölçüm sonuçlarıyla tek aşamada donar. `docs/mimari/09-test-ve-is-akisi.md` sürüm 0.2.
- **`tools/sol-incele.sh`:** Claude'un yazdığı belge ya da kodun sol incelemesini başlatan betik (ilk ve ikinci inceleme kipi). **sol yazma denemesi geçti (W0-SOL1, PR #3, 2026-11-15):** sol görev belgesine ve dosya sahipliğine uydu, commit atmadı, çalıştıramadığı ELF testlerini "atlandı" diye dürüstçe raporladı; `ida-inceleyici` 8 bulgu verdi (1 kritik: ELF arşivi denetlenemiyordu), sol bir düzeltme turunda hepsini giderdi; mimar bağımsız koşuyla doğruladı (58 denetim geçti). Sonuç: 09 §2.9'daki sol yazım işleri sol'da kalır; sol'a yazım görevi verilirken gereken araç zinciri ortamına (`PATH`) eklenir. Ürün: `tools/denetim/bellek-ayirma.sh`.
- **Agent yapısı kuruldu:** tanımlar `.claude/agents/` altında.
- **W0 başladı:** ilk iki iş W0-MP1a ve W0-MP6a (`docs/gorevler/`).
- **Kart Mac'e bağlı** (ST-LINK portu, USB hub üzerinden); tek kablo yeterli (K17).
- **Ürün kodu yok;** W0 ölçüm prototipleri `olcum/` dallarında, `tools/olcum/` altında yazılıyor.
- **W0 durumu (2026-11-15):**
  - **W0-MP1a tamamlandı ve main'e birleşti** (PR #1). Rapor `docs/olcumler/MP1-arac-zinciri.md`, kod `tools/olcum/mp1/` (atılabilir prototip). K0, yedek, K1, K2 düzeltilmiş ölçütlere göre geçti; sol iki tur inceledi, kritik bulgu kalmadı. Mimari için bulgular (açık bulgular kaydına işlenecek): STM32U59x/5Ax'te 160 MHz'e geçişte geçici AHB ÷2 ve en az 5 µs bekleme gerekli; Picolibc CubeIDE 14.3.rel1 macOS paketinde yok; ST kart örneklerinin çoğu PLL'i MSIS'ten besliyor; kart revizyonu `REV_ID` 0x3000 ES0553 Rev 6 listesinde yok.
  - **W0-MP6a tamamlandı ve main'e birleşti** (PR #2). Rapor `docs/olcumler/MP6-godot-kamera.md`, kod `tools/olcum/mp6/`. G1–G4 geçti: `force_draw(false, dt)` + `SubViewport` `UPDATE_ONCE` + eşzamanlı geri okuma yolu görünür, örtülü, gizli ve simge durumlarında 300 s boyunca 9000/9000 kare üretti, görünür referansla bit düzeyinde aynı; 640 × 480 geri okuma p99 5,69 ms; olağan çizim döngüsü görünmeyen pencerede kare üretmedi (elendi). Doğrulanmadı: tam ekran durumu, Windows, 5 dakikadan uzun koşular, GDExtension. sol iki tur inceledi, kritik bulgu kalmadı. Mimari için bulgular (açık bulgular kaydına işlenecek): kamera `force_draw` ile ana iş parçacığından çizdirilmeli; `get_image()` RGB8 döndürüyor; asenkron geri okuma gizli pencerede ≈ 70 ms.
  - **W0-MP7a tamamlandı ve main'e birleşti** (PR #4). Sonuç: sade kademe (yalnız doğrudan yol, kutu başına benek) 5 000 ışınla dondurulmuş yakınsama ölçütünü sağlıyor ve tek iş parçacığında p99 0,93 ms (7 m) / 0,79 ms (50 m); tam kademe (eşevreli hücreler, çok yollu yayılım) ölçütü taranan en büyük ışın sayısına (≈ 2,56 milyon) kadar sağlamadı; `N` = 1 280 000'de tek iş parçacığı p99 504,4 ms (bütçe 9,33–25,17 ms); bütçeye giren ışın sayısı tek iş parçacığında 20 000–40 000, 11 iş parçacığında 80 000; azaltılmış ışın sayısının bilinen hatası raporda (7 m: enerji farkı 20 000'de 0,146, 160 000'de 0,057). Tekrar üretilebilirlik geçti. Mimari için: bölüm 05 §2.4.3 saçıcı hücresi kuralı hatalıydı; yeni kural ve yakınsama tanımı `docs/gorevler/W0-MP7a.md` içinde; iskelet §8 sonar kararı (kartlı çalışmada sade kademe varsayılan, tam ayrıntı yalnız kartsız koşuda) W0 sonunda mimarca işlenecek. sol iki tur inceledi; kapanış mimar kararıyla. Rapor `docs/olcumler/MP7-sonar-suresi.md`.
  - **W0-MP4a (kartta 400 Hz döngü yükü, yer tutucu çekirdek adımı):** başladı (2026-11-16), sürüyor; görev belgesi `docs/gorevler/W0-MP4a.md`.
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

0. MP2 (USB HS) K17 ile plandan çıktı. MP4a bitti. Sıra: MP3 (sanal seri port hızı ve kaybı; artık A1'in tek taşıması); MP5 (mesaj biçimi maliyeti; seri port üzerinden); MP8 Windows betiği; W0 sonu: ölçüm sonuçlarının ve K17'nin iskelete (§3, §8) ve açık bulgulara işlenmesi; sonra W1 sözleşmeler.
1. W0 ölçüm prototipleri (`docs/mimari/09-test-ve-is-akisi.md` 2.1 ve 6.1). MP1a, MP6a, MP7a, MP4a bitti. Sırada: MP3, sonra MP5, MP8 betiği, W0 sonu, W1.
2. W1 arayüz sözleşmeleri (A1–A8); açık bulgular bu aşamada kapatılır; iskelet önerileri W1 başında karara bağlanır.
3. Uygulama dalgaları (W2–W6).

## Kodlamanın ilk işleri: ölçümler

Web araştırmasıyla kapanmayan belirsizlikler. Sonuçları çıkmadan ilgili tasarım kararı kesinleşmez (00 belgesi, Bölüm 7).

| Ölçüm | 00 belgesindeki karşılığı |
|---|---|
| Kartta yüksek hızlı USB: çalışıyor mu, gecikme ve kayıp (K17 ile plandan çıktı; yerine sanal seri port hızı ve kaybı: M3) | M1, M2 |
| Godot'da pencere görünmezken kamera görüntüsü üretimi (Mac ve Windows) | M13, M28 |
| Sonar hesap süresi | M11 |
| Kartta 400 Hz döngünün yükü ve en kötü durum süresi | M5 |
| Windows hedef bilgisayarının kaldırdığı kalite kademesi | M12, M20, M21, M30 |

Kart Mac'e tek kabloyla, ST-LINK portundan bağlanır (K17); araç kurulumu için kullanıcı izni gerekir.

## Açık konular

- Windows hedef bilgisayarı: 8 GB bellek, dahili ekran kartı (K16); işlemci bilinmiyor; kalite kademesi tabanı MP8 ile sabitlenecek (Ö11).
- Bölüm 03'ün dar kapsamlı (yalnız emniyet zinciri ve arayüzler) düzeltme turu için kullanıcı yanıtı bekleniyor.
- 00 belgesindeki açık sorular (Bölüm 9). Yarışmaya ve ardıl karta bağlı olanlar kapsam dışıdır (K7, K8).

## Kayıt
### 2026-11-19 · mimarlık Fable'a döndü (K19); MP3 kampanyası sürüyor
- K19: mimar oturumunu yeniden Fable (Claude) yürütür; K18'deki Codex devri sona erdi, görev dağılımı sürer.
- MP3 akış koşuları (600 s, çift yönlü, 256 B): 921600 baud GEÇTİ (159 920 çerçeve), 2 000 000 baud GEÇTİ (347 827 çerçeve); 3 ve 6 Mbaud ile echo koşuları kaldı.
- 3 000 000 baud koşusu kart bağlantısı koptuğu için geçersiz (ölçüt ihlali değil); yinelenecek.

### 2026-11-18 · W0: MP3 ara durumu
- MP3 (dal `olcum/mp3-vcp`) ilk teslimi yapıldı; sol ön incelemesi 8 kritik, 3 önemli bulgu verdi, Opus birinci düzeltme turu sürüyor. Kart yeniden bağlandı; kısa kart ölçümü ve uzun kampanya yapılmadı.
- Sıra: düzeltme turu, kısa kart ölçümü, sol incelemesi, uzun kampanya; sonra MP5, MP8, W0 sonu, W1. MP4a tamamlanmış kalır.

### 2026-11-18 · W0: MP4a ölçüm ve inceleme tamamlandı
- MP4a daraltılmış kapsamla bitti; D1/D2 geçmedi, D3/D4 geçti; O2 ağır 400 Hz maks 3,32238125 ms, aşım 241000/241000; O2 200 Hz 121000 adımda aşım 0. Gerçek çekirdek/taşıma hızı kesinleşmedi.
- Mimar kapsamı daralttı: taşmalı histogramlar reddedilir. 103 test geçti; sol kapanışı kritik/önemli bulgu görmedi.
- PR #5 main'e squash birleşti (b2b9346). Sıra: MP3, sonra MP5, MP8, W0 sonu, W1.

### 2026-11-18 · W0: MP4a devam kaydı
- MP4a (ölçüm prototipi, dal `olcum/mp4-dongu-yuku`) tamamlanmadı ve main'e birleşmedi; O2 A/B/C ölçümü bitti, D1/D2 O2 ağır yükte geçmedi, kontrol hızı kesinleşmedi. Ayrıntı "Şu an" bölümünde.
- sol ilk incelemesi 2 kritik, 3 önemli bulgu verdi; PC düzeltmesi, yeniden analiz ve eksik Og koşusu sürüyor.
- K18 mimar devri korunuyor.

### 2026-11-17 · Mimar oturumunun devri (K18)
- Kullanıcı mimar oturumunu Fable'dan Codex'e devretti; kod yazımı Opus'a, bağımsız inceleme sol'a verildi. Karar `AGENTS.md` ve karar günlüğü Bölüm 2.1'e eklendi.
- Bu oturumda uygulama ya da ölçüm başlatılmadı. MP4a'nın mevcut dal ve çalışma kopyalarındaki gerçek ilerlemesi devam etmeden önce doğrulanacak; önceki W0 sırası korunuyor.

### 2026-11-16 · W0: MP7a birleşti, tek kablo kararı (K17), MP4a başladı
- MP7a main'e birleşti (PR #4); sade kademe bütçeye sığıyor, tam kademe ölçütü sağlamadı; bölüm 05 §2.4.3 hücre kuralı düzeltildi (görev belgesinde).
- Kullanıcı ikinci USB-C kablosu olmadığını bildirdi: K17 tek kablo kararı; MP2 plandan çıktı, A1 yalnız sanal seri port.
- MP4a (kartta 400 Hz döngü yükü) başladı; görev belgesi `docs/gorevler/W0-MP4a.md`.
- İskelet §3, §8 ve bölüm 01/06 güncellemesi W0 sonunda mimarca yapılacak.

### 2026-11-15 · W0: sol yazma denemesi geçti
- W0-SOL1 (PR #3) birleşti: `tools/denetim/bellek-ayirma.sh` ve testleri; yazan sol, inceleyen `ida-inceleyici` (8 bulgu, bir düzeltme turu).
- sol dosya sahipliğine uydu, çalıştıramadığını dürüstçe bildirdi; mimar bağımsız koşuyla 58 denetimi doğruladı.
- Sıradaki: MP7a düzeltme ve birleştirme, MP2–MP5, MP8.

### 2026-11-15 · W0: MP6a birleşti, MP7a düzeltme turunda
- MP6a main'e birleşti (PR #2): G1–G4 geçti, sol iki tur, kritik bulgu kalmadı; mimari bulguları açık bulgular kaydına işlenecek.
- MP7a: sol incelemesi sonrası mimar saçıcı hücresi kuralını ve yakınsama büyüklüklerini görev belgesinde yeniden tanımladı; düzeltme turu sürüyor.
- Sıradaki: MP7a düzeltme, ikinci inceleme, birleştirme; sol yazma denemesi, MP2–MP5, MP8.

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
