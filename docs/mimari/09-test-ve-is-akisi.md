# 09 — Test ve iş akışı

Sürüm 0.2 (2026-11-14). Durum: taslak; kullanıcı onayı yok.

0.2'de dalga sırası değişti (ölçüm prototipleri W0, sözleşmeler W1); bu mimar önerisidir, kullanıcı onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) (sürüm 0.7) Bölüm 9, satır 09'un kapsamını yazar: kodlama aşamasının nasıl yürüyeceği ve neyin ne zaman "bitti" sayılacağı. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir (`K` kesin karar, `Ö` öneri, `M` ölçülecek konu, `Ç` çözülmüş çelişki); ham rapor değerleri önce oradaki düzeltmeden geçirilmiştir. Diğer bölümlerin "09'da kesinleşir" dediği eşiklerin **rakamlarını** bu bölüm vermez; rakamlar sözleşme ve görev belgelerinde, kabul koşusundan önce dondurulur (7.3). Test kimlikleri 00'daki `K`/`Ö`/`M` kimlikleriyle çakışmasın diye raporlarda `B<bölüm>/<kimlik>` yazılır (ör. `B01/K7`, `B07/P5`).

---

## 1. Amaç ve sorumluluklar

1. Kodlama aşamasının sırasını, görev döngüsünü ve kimin neyi yazıp incelediğini sabitlemek (K11–K14, Ö12).
2. Bir işin bitmiş sayılma koşullarını ve kabul sınırlarının dondurulma yöntemini belirlemek.
3. Ölçüm işlerini tanımlamak; sonucu çıkmayan ölçüme bağlı kararı kesinleşmiş saymamak (`AGENTS.md` md.5).
4. Kart ve PC'nin aynı depoda, iki işletim sisteminde, sabit sürümlerle derlenmesini ve her pull request'te denetlenmesini sağlamak.

**Yazmadıkları:** parçaların iç tasarımı (01–08); eşik rakamları (ilgili bölüm önerir, sözleşme işi dondurur); ardıl kart, gerçek sensör tedariki, yarışma kuralları (K7, K8).

**Çalıştırma yerleri.** Agent'lar yalnız bu Mac'te (Apple Silicon) çalışır; kart bu Mac'e bağlanır.

| Yer | Ne koşar | Ne koşamaz |
|---|---|---|
| Mac (agent) | Derleme, bütün CPU testleri, kartlı testler, Godot'nun GPU testleri, ölçüm paketlerinin Mac yarısı | Windows davranışı |
| Derleme sunucusu: macOS arm64 ve Windows x64, ekran kartı yok | Her PR'da derleme, birim, sözleşme, SIL, statik analiz; gece uzun koşular | Kart, GPU testleri, düşük donanımlı Windows hızı |
| Kullanıcının düşük donanımlı Windows bilgisayarı | 3B ve kamera, kalite kademesi tabanı, Windows'ta kart | Otomatik değil; kullanıcı ölçüm betiğini çalıştırıp çıktıyı verir |

Kullanıcının gerektiği üç yer: mimari onayı, kartın bağlanması, Windows denemesi. Gerisi kullanıcıya sorulmadan yürür.

---

## 2. İç yapı

### 2.1 Sıra

```
mimari onayı (kullanıcı) -> açık bulgular kaydı (hazır) -> W0 ölçüm prototipleri
  -> W1 sözleşmeler (A1-A8; yaz, incele, dondur) -> W2 temel -> W3 çekirdek -> W4 bağlantı -> W5 görünür kısım -> W6 bütünleme
```

Gerekçe: A1'in taşıması, biçimi ve kontrol hızı M1–M3, M5, M9 sonucuna; A2 ve A8'in kamera yolu M13/M28'e; sonar ayrıntısı M11'e bağlıdır. Sözleşme ölçümden önce yazılırsa A1 iki aşamada donmak zorunda kalır ve ölçüm sonucu sözleşmeyi yeniden açar; ayrıca kurulumun bu kartta ve bu bilgisayarlarda çalışıp çalışmadığı en ucuz yoldan, en başta öğrenilir. Açık bulguların en kalabalık kümesi (A1) ölçülmüş veriyle kapatılır.

**Sözleşmeler dondurulmadan paralel kodlama başlamaz.** W0 ölçüm araçları atılabilir prototiptir: kendi dalında (`olcum/mp<n>-<ad>`) yaşar, sözleşmeye dayanmaz, ürün koduna taşınmaz; ürüne girecek her şey W2 ve sonrasında görev belgesiyle yeniden yazılır. A1 tek aşamada, W1'de, ölçüm sonuçlarıyla donar (eski iki aşamalı çözüm, 8.1 md.1, gereksiz kaldı). Kart bağlanana kadar kartsız ölçümler (MP6, MP7) yürür; kart uzun süre bağlanamazsa karta bağlı olmayan sözleşmeler (A3, A5, A6, A7) W1'den öne alınabilir, A1 kart ölçümlerini bekler. Açık bulgular kaydındaki iskelet önerileri W1'in başında mimarca karara bağlanır.

### 2.2 Açık bulgular kaydı

`docs/mimari/acik-bulgular.md`: `docs/mimari/inceleme/` altındaki sol incelemelerinde kapanmamış bulguların tek belgesi. Toplamayı hafif uygulayıcı yapar, etiketlemeyi mimar. Her kayıt: `AB-nnn` (kalıcı kimlik), kaynak (dosya ve madde), önem (2.4), **kapatacak iş** (tek bir A#, görev kimliği ya da MP#), durum (açık / kapandı + PR / mimarın gerekçeli "kabul edilen sınırı"). Bir iş kendisine atanan bulguları kapatmadan bitmiş sayılmaz; atanmamış önemli bulgu sonraki dalga başında atanır.

### 2.3 Görev döngüsü

| # | Adım | Kim | Çıktı |
|---|---|---|---|
| 1 | Görev belgesi | Mimar | `docs/gorevler/<kimlik>.md` (3.1) |
| 2 | Kabul testleri (yalnız kritik işte) | `ida-test-yazari`; uygulayıcıdan bağımsız: yalnız sözleşmeyi ve görev belgesini okur | Dondurulmuş testler ve sınırlar (7.3) |
| 3 | Uygulama | Belgede adı geçen uygulayıcı, kendi dalında | Kod, kendi birim testleri, PR |
| 4 | İnceleme | Karşı model ailesi: Claude'un yazdığını sol, sol'un yazdığını `ida-inceleyici`; çağrıyı mimar başlatır | Kritik / önemli / küçük bulgu listesi |
| 5 | Düzeltme | Aynı uygulayıcı; en fazla iki tur | Güncel PR |
| 6 | Otomatik derleme ve test | CI (2.8) + gereken kartlı ya da GPU testi | Yeşil CI, test raporu |
| 7 | Denetim | Mimar, görev belgesine göre | Kabul / ret |
| 8 | Kayıt | `ida-kayit`: birleştirme, commit, `docs/DURUM.md` | Kayıt |

Kritik iş: A1–A8 sözleşme uygunluğu, fizik, kestirim ve kontrol, arıza güvenliği, atış kontrolün emniyet zinciri, adım bariyeri ve HIL zamanlaması. Test yazarı uygulayıcıdan **önce** çalışır, testler dondurulur; uygulayıcı kabul testlerine dokunmaz, hatalı bulursa mimara bildirir. Böylece "kritik iş + test yazarı" iki Claude kotasını aynı anda tutmaz (Ö12). Atış kontrolün kapsamı emniyet zinciri ve arayüzlerle sınırlıdır (03; iskelet 0.7); görev belgeleri bunu tekrar eder.

### 2.4 İnceleme döngüsünün sınırı

Gerekçe: mimari bölümlerinde sınırsız döngü yakınsamadı ve her tur yüz binlerce token tüketti (mimar kararı).

- **Kritik:** emniyet davranışı, yanlış sayısal sonuç, veri bozulması, sözleşme ihlali, bellek güvensizliği, kabul testini geçersiz kılan hata. Kritik bulgu kalan iş kabul edilmez.
- **Önemli:** çalışmayı bozmayan ama giderilmesi gereken tutarsızlık. İki düzeltme turundan sonra kalanlar `AB-nnn` olarak kayda girer, iş kabul edilebilir.
- **Küçük:** biçim, ad, açıklama; uygulayıcı isterse giderir.
- İkinci turdan sonra kritik bulgu sürerse iş mimara döner: kapsam daraltılır ya da görev belgesi düzeltilir; aynı kapsamla yeniden gönderilmez.

### 2.5 Kota ve süreklilik

Aynı anda en fazla iki Claude agent'ı (Ö12); sol ayrı kotadan çalışır. Kullanım sınırına takılan agent kaldığı yerden devam ettirilir; her tamamlanan iş hemen commit'lenir. Sol'un yazdığı iş sol sınırına takılırsa Claude'a devredilmez (K13: aynı iş iki modele yazdırılmaz); beklenir ya da mimar kapsamı daraltır.

### 2.6 Araç zinciri ve sürümler

Hareketli etiket (`latest`, `@v4` türü) yasaktır; her bağımlılık açık sürüm ya da commit ile sabitlenir. Aşağıdaki sürümler ham raporlardan ve 00 düzeltmelerinden alınmıştır; **bu bölüm yazılırken kurulum ya da indirme yapılmadı**: ilk W2 işi hepsini çözümleyip kilit dosyalarına yazar, ilk kullanan iş sürümü doğrular.

| Kalem | Sabit | Dayanak / not |
|---|---|---|
| Kart derleyicisi | GNU Tools for STM32 14.3.rel1 (arm-none-eabi), iki OS'ta aynı sürüm | R08 §2.1; Homebrew'deki 16.2.0 kullanılmaz (kayma). CubeIDE 2.2.0 içindeki arm64 sürümü bu Mac'te var; CI'da kurulabilirliği ve Arm GNU 14.3.rel1 ile farkı W2'de doğrulanır |
| Yükleme, hata ayıklama | `STM32_Programmer_CLI` (bu Mac'te 2.23.0; CubeCLT 1.21.0'da 2.22.0), `ST-LINK_gdbserver` + `arm-none-eabi-gdb` (CubeCLT 1.21.0'da CubeProgrammer `bin` yolu verilir); OpenOCD 0.12.0 yalnız yedek, U5A flash desteği doğrulanmadı | R08 §2.1; M21 |
| Derleme sistemi | CMake ≥ 3.20 + Ninja; yapılandırma dosyası `CMakePresets.json` (00 08.6: `.cmake` adı hatalı). Ön ayarlar: `host-debug`, `host-asan`, `host-release`, `fw-debug`, `fw-release`, `fw-measure` | R08, R09 |
| libc | Picolibc (CubeMX 6.18.0 varsayılanı); derleyici, libc ve linker ayarı birlikte sabit | 00 08.7; 01 §6 |
| CubeMX ve üretilen dosyalar | Yalnız pin ve saat referansı: `.ioc` `firmware/cubemx/` altında, **derlemeye girmez**. Üretilen `cmake/stm32cubemx/CMakeLists.txt` her üretimde ezilir; depoya alınmaz, elle düzenlenmez. Başlangıç, linker, sistem ve `hal_conf` dosyaları bir kez kopyalanıp `firmware/bsp/` altında sahiplenilir | 00 08.6; 01 §8.2 md.7 |
| ST kütüphaneleri | HAL, CMSIS device, CMSIS 6, CMSIS-DSP, TinyUSB 0.21.0: alt modül, `firmware/third_party/`, commit sabit. Yedek USB yığını upstream USBX (MIT) | 00 08.4; 01 §8.2 md.7–8 |
| Diller | Çekirdek ISO C17 (`-std=c17 -pedantic-errors`, uzantı yok); platform kodu `gnu17`, uzantı yalnız `firmware/`. PC: C++20, `CXX_EXTENSIONS OFF`, modül ve coroutine yok, kararsız `<chrono>` saat dilimi API'si ortak tabana girmez | 00 Ç10, 09.3; 01 §8.2 md.6 |
| PC derleyicileri | macOS: Apple clang, Xcode 26+ tabanı; Windows: MSVC (sürüm CI görüntüsünden kaydedilir), clang-cl isteğe bağlı. Minimum macOS hedefiyle derleme, bağlama ve çalışma testi M10 kapsamında | 00 09.1, 09.13 |
| PC bağımlılıkları | vcpkg manifest (`builtin-baseline` + `overrides`, tek `vcpkg.json`); küçük başlık-yalnız bağımlılıklar CPM ile commit/`URL_HASH` sabit. Beklenen: Eigen, Embree 4.4.1, Jolt v5.6.0, MCAP, zstd, HTTP/WebSocket kitaplığı (B08), c_library_v2 (commit) | Ö3, Ö4, Ö7, Ö9; 00 09.13 |
| Godot | 4.7.2 (resmî ikili, SHA-256 kayıtlı), godot-cpp 10.0.0 alt modül, `CMAKE_MSVC_RUNTIME_LIBRARY` (tekil ad); 4.6.x'e dönüş otomatik çözüm sayılmaz | 00 12.9, 12.10; M29, M33 |
| CI | Açık runner etiketi (`macos-26` arm64; Windows için açık Server görüntüsü, `windows-latest` değil); eylemler commit SHA ile; Apple Silicon testi için Intel runner yok; Xcode ve MSVC sürümü günlüğe yazılır. Ön yüz paketleri (MapLibre GL JS, uPlot) W5'te kilit dosyasıyla | 00 09.13; K9 |

**Tek depo, iki kök.** `core/` kendi CMake hedefidir (`ida_core`, statik, saf C17, işletim sistemi bağımlılığı yok); aynı `core/CMakeLists.txt` iki kökten alınır: PC kökü (`host-*`) ve firmware kökü (`fw-*`, `CMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-cm33.cmake`). Çekirdek hedefinin include yolunda `stm32*`, CMSIS ve RTOS başlığı bulunmaz; bunu CMake zorlar (R08 §2.6). Dönüştürücü `protocol/conv/` altındadır, çekirdeğin dışında (01 §8.2 md.3).

**Kayan nokta (M10 ile doğrulanana kadar başlangıç politikası).** Çekirdek (kart ve SIL aynı): `-ffp-contract=off -fno-fast-math -Wconversion -Wdouble-promotion -Wfloat-conversion`; MSVC'de `/fp:precise`, `/fp:fast` yasak. Sim fiziği `double` (Ö3), aynı daralma bayrağıyla. Eigen'de `EIGEN_FAST_MATH=0`; `EIGEN_DONT_VECTORIZE` yalnız referans yapılandırmada ve M10'da karşılaştırılır (00 09.7). Bu bayraklar bit eşitliği **garantilemez** (00 08.14, 09.5); amaç SIL'in üretimdeki firmware politikasını temsil etmesidir. Kartın FMA politikasını M10 sonucuyla mimar kesinleştirir.

### 2.7 Dal ve birleştirme

| Dal | Kullanım |
|---|---|
| `main` | Varsayılan; PR ile, ya da yalnız belge kaydı olarak doğrudan değişir |
| `is/<dalga>-<ad>` | Uygulama (ör. `is/w3-fizik`); `sozlesme/a<n>-v<m>`: sözleşme; `olcum/mp<n>-<ad>`: ölçüm aracı ve raporu; `belge/<ad>`: belge |
| `wip/<ad>` | Yarım iş; PR açılmaz, iş bitince `is/` dalına alınır |

Dal adları ASCII'dir. **Birleştirme koşulları:** (1) CI iki OS'ta yeşil, (2) karşı ailenin incelemesinde kritik bulgu yok, (3) PR şablonu eksiksiz, (4) gereken kartlı/GPU test raporu PR'da, (5) mimar denetimi. Birleştirmeyi denetimden sonra `ida-kayit` yapar (mimar önerisi, kullanıcı kararı değil). **Belge ve kod ayrı PR'dır**; istisna görev belgesinin istediği küçük sürüm ya da bağlantı güncellemesi. Yalnız belge içeren PR derleme işlerini atlar. Ham raporlar ve doğrulamalar değişmez (düzeltme 00'a eklenir); `docs/mimari/` yalnız mimar görev belgesiyle değişir; `docs/DURUM.md` ve karar günlüğü kayıtları doğrudan `main`'e girebilir. Derlenmeyen hâl `main`'e ya da PR'a girmez; sınıra takılan agent'ın yarım dosyaları `wip/` dalına alınır (05 düzeltmesinde dosyanın yarı düzenli kalması bunun gerekçesi, `docs/DURUM.md`).

### 2.8 Otomatik derleme ve test

Derleme sunucusu macOS arm64 ve Windows x64'tür. Her pull request'te:

| İş | macOS | Windows | İçerik |
|---|---|---|---|
| Derleme (host) | evet | evet | `core`, `sim`, `protocol`, araçlar; uyarı-hata (`-Wall -Wextra -Wconversion -Wdouble-promotion`; `/W4 /permissive-`) |
| Birim, sözleşme, duman SIL | evet | evet | T1, T2, T3 kısa küme, T5 ve T6 kısa küme, T7 SIL (CTest) |
| Kart derlemesi (kartsız) | evet | evet | `fw-debug`, `fw-release`; harita, `-fstack-usage`, boyut raporu artefakt; yükleme yok. M21'in derleme yarısını korur |
| Statik analiz | evet | evet | GCC `-fanalyzer` (firmware derlemesinde), clang-tidy (`bugprone-*`, `cert-*`, `clang-analyzer-*`), cppcheck (`core/`, `firmware/`), MSVC `/analyze`. Politika: MISRA'dan seçilmiş yaklaşık 20–30 kural (dinamik bellek, özyineleme, ayrıştırılmamış union yok); "MISRA uyumlu" iddiası yazılmaz (R08 §2.6) |
| Bellek ayırma yasağı | evet | evet | `ida_core` ve firmware nesnelerinde `nm` ile `malloc/calloc/realloc/free/_malloc_r` başvurusu yok; firmware bağlamasında `--wrap` ile bu simgeler tanımsız bırakılır, başvuru bağlantı hatası verir (boş tanım **değil**, 00 08.13); `-fstack-usage` yığın tavanı. İstisna (LVGL havuzu, 01 §8.1 md.2) yalnız mimar onayıyla |
| Sanitizer | ASan+UBSan | ASan (MSVC'de UBSan/TSan yok) | T1, T2, T10 sunucu testleri; `host-asan` |
| Godot (GPU'suz) | evet | evet | `--headless` ile uzantı yükleme, serileştirme, A7, halka yuva (B07 P1 ve sürekli testler). `--headless` GPU sensör yolu sayılmaz (Ö5-k) |
| Belge ve depo denetimi | evet | — | Ham rapor ve doğrulamada diff yok; kişisel makine yolu yok; göreli bağlantılar çözülür; Rust ve ardıl kart bağımlılığı yok |

**Gece:** uzun SIL ve Monte Carlo (T3, T7), 4 saatlik Allan kaydı ve 20 tohum (T6), 600 s enerji (T5), iki OS'ta M10 izleri ve karşılaştırması (T8; iki işin artefaktı indirilip karşılaştırılır), tam kademe sonar SIL (CPU), sürüm kayması denetimi. **CI'da koşmayanlar:** kartlı testler (kart Mac'te; self-hosted runner kurulmaz); kamera, görünmeyen pencere, su altı optiği, kare süresi (GPU yok; Mac'te agent, Windows'ta kullanıcı); düşük donanımlı Windows hızı (MP8). Bunların sonucu PR gövdesinde ve `docs/olcumler/` altında raporlanır; yoksa PR'ın "Doğrulanmadı" bölümünde yazılır. CI dakika kotası ve macOS iş maliyeti **doğrulanmadı** (8.2 md.4); kotada darlıkta önce gece işleri, sonra Windows sanitizer işi seyreltilir, PR'daki derleme ve birim kapısı seyreltilmez.

### 2.9 Uygulama dalgaları

Kısaltmalar: Fable = `ida-kritik-uygulayici`, Opus = `ida-uygulayici`, Sonnet = `ida-hafif-uygulayici`, TY = `ida-test-yazari`, İnc = `ida-inceleyici` (yalnız sol'un yazdığını inceler, dosya değiştirmez). Claude'un yazdığını sol, sol'un yazdığını İnc inceler (K13).

| Dalga | Görev | Yazan | TY | İnceleyen |
|---|---|---|---|---|
| W0 | Açık bulgular kaydı (toplama); **yapıldı (2026-11-14)** | Sonnet | — | Mimar |
| W0 | **sol denemesi:** `tools/` altında bellek ayırma denetim betiği, geçen ve geçmeyen örnek nesneler (ölçümlerden bağımsız; ilk iş olarak koşabilir) | sol | — | İnc |
| W0 | MP1–MP4 test aygıt yazılımı, PC ölçüm araçları, ölçümler | Opus | — | sol |
| W0 | MP5 aday düzeneği (deneme geçerse sol, geçmezse Opus; nihai dialect yerine temsili mesaj kümesiyle ölçer: IMU örneği, aktüatör komutu, Ping360 profil parçası) | sol | — | İnc |
| W0 | MP6, MP7 prototip ve ölçüm; MP8 betiği | Opus | — | sol |
| W0 sonu | Ölçüm sonuçlarının iskelet 8. başlığa işlenmesi | Mimar | — | — |
| W1 | A1 (anlam + taşıma ve tel biçimi, tek aşama) + A4 + zaman çerçevesi + olay kuralları + A8 | Fable | — (uygunluk testleri W2'de) | sol |
| W1 | A2 + A7 | Fable | — | sol |
| W1 | A3 + A5 + A6 | Fable | — | sol |
| W2 | Depo iskeleti, CMake ön ayarları, vcpkg, CI, belge denetimi | Sonnet | — | sol |
| W2 | Protokol kütüphanesi (şemadan C ve C++ kodlayıcı/çözücü, altın baytlar) | sol | TY | İnc |
| W2 | Çekirdek iskeleti: tipler, `port.h`, `mat.h`, zaman tipleri, test düzeneği | Fable | TY | sol |
| W3 | Fizik çekirdeği (04); kestirim, güdüm, kontrol (02) (iki ayrı iş) | Fable | TY | sol |
| W3 | Kamera dışı sensör modelleri (05) | sol | — | İnc |
| W3 | Kart yazılımı (01) | Opus | — | sol |
| W4 | HIL köprüsü, bariyer, bağlantı emülatörü (06) | Opus | TY | sol |
| W4 | Kartsız koşucu (SIL; 02, 06) | Fable | TY | sol |
| W4 | Sonar (05) | sol | — | İnc |
| W4 | Atış kontrol: emniyet zinciri ve arayüzler (03; F1–F10) | Opus | TY | sol |
| W5 | Godot sahnesi ve GDExtension (07) | Opus | — | sol |
| W5 | Web panelleri (08) | Sonnet | — | sol |
| W5 | Kayıt (MCAP), tekrar oynatma, A3 sunucusu, A5 doğrulayıcı | sol | — | İnc |
| W5 | Hata enjeksiyonu (04, 05, 06 arayüzleriyle) | Opus | TY | sol |
| W6 | Kabul koşuları, M10 tam turu, MP4 yinelemesi, uçtan uca HIL, kalan `AB-nnn`; Windows denemesi kullanıcıda | TY + Opus | TY | sol |

**sol'un payı:** 24 görev satırından 6'sı (bir satır birden çok iş içerebilir) sol'dadır, yani kabaca dörtte bir; hedef üçte birdir. Sayı görev sayısıdır, iş büyüklüğü ölçülmedi; W6 sonrasındaki düzeltme işleri payı artırabilir. **sol'un yazma kipi bu depoda henüz denenmemiştir:** W0'daki küçük görev denemedir. Denemede bakılanlar: görev belgesine ve dosya sahipliğine uyum, commit atmaması, ASCII dal adı ve göreli yol, testi gerçekten koşturup sonucu doğru raporlaması, İnc'in sol çıktısını inceleyebilmesi. Deneme geçmezse sol'a ayrılan yazım işleri Opus'a verilir, sol yalnız inceler. **Paralellik:** her dalgada aynı anda en fazla iki Claude agent'ı; aynı dosyaya dokunan görevler aynı anda koşmaz.

---

## 3. Arayüzler

### 3.1 Görev belgesi

Mimar yazar; uygulayıcı yalnız bunu ve `AGENTS.md` kuralını talimat sayar.

| Başlık | İçerik |
|---|---|
| Kimlik, dalga, uygulayıcı, inceleyici | `W3-fizik-01`, Fable, sol |
| Amaç ve okunacaklar | Hangi bölümün hangi başlıkları ve sürümü |
| Sözleşmeler | Dokunulan A# ve dondurulmuş sürümleri; sözleşme değişikliği bu işin kapsamında **değil** |
| Dosya sahipliği | Yazılacak ve dokunulmayacak yollar |
| Atanan açık bulgular | `AB-nnn` listesi; her biri için kapanış kanıtı beklenir |
| Kabul testleri | Test kimlikleri, sınır dosyası etiketi (7.3), koşacağı yerler ve kademeler |
| Yasaklar | Kapsam dışı (K7, K8; Rust yok), ölçülmemiş rakamı gerçek yazmama, başka agent başlatmama, commit atmama |
| Rapor ve bitiş | Yapılan / doğrulanan / doğrulanmayan ayrı; bitiş koşulu 7.2; tur sınırı iki |

### 3.2 Pull request gövdesi

Boş başlık bırakılmaz, yoksa "yok" yazılır: (1) **Ne değişti** ve neden. (2) **Nasıl doğrulandı:** komut, işletim sistemi, derleyici sürümü, kademe, tohum, CI bağlantısı; kartlı ya da GPU testinde makine ve tarih. (3) **Doğrulanmadı:** koşulmayan testler, ölçülmemiş (tahmin etiketli) rakamlar, tek işletim sisteminde denenenler. (4) **Kapanan açık bulgular:** `AB-nnn` ve kanıt. (5) **Yeni açık bulgular.** (6) **Sözleşme etkisi:** dondurulmuş sözleşmeye dokunan PR yalnız `sozlesme/` dalından gelir. (7) **İnceleme:** inceleyen, tur sayısı, kalan bulgu.

### 3.3 Kayıt biçimleri

| Kayıt | Biçim | Sahibi |
|---|---|---|
| Commit | Türkçe, kısa: `<alan>: ne değişti (görev kimliği)`; bir PR bir commit (squash) | `ida-kayit` |
| `docs/DURUM.md` | Her birleşen anlamlı iş parçasından sonra; yalnız kendi kaydı yeniden yazılır | `ida-kayit` |
| Ölçüm raporu | `docs/olcumler/MP<n>-<ad>.md`: makine, OS, derleyici, kart revizyonu (`REV_ID`), komut, ham veri yolu ve özeti, sonuç, geçti/geçmedi. Büyük ham veri depoya girmez (`.gitignore`) | Ölçümü yapan; denetim mimar |
| Karar günlüğü | Ölçüm sonucu ve mimarın seçimi 00'da yeni `Ö` satırı ("ölçümle kesinleşti", rapor bağlantısı); kullanıcı kararı `K` satırı. İskelet 8. başlık tablosunu yalnız mimar günceller (iskelet sürümü artar) | `ida-kayit`; mimar |
| Sözleşme | `docs/sozlesmeler/A<n>-<ad>.md` + makine tarafından okunan şema (`protocol/`, `scenarios/`); sürümlü; dondurma etiketi `sozlesme-A<n>-v<m>`; dondurulmuşta değişiklik ayrı sözleşme PR'ı, tüketici listesi ve etkilenen testlerle, mimar onayıyla | Yazan; dondurma mimar |

---

## 4. Hata durumları ve davranışı

İş akışının kendi arızaları.

| Durum | Davranış |
|---|---|
| Agent kullanım sınırına takıldı | Kaldığı yerden devam ettirilir; yarım dosyalar `wip/` dalında kalır, görev belgesi değişmez; devam eden agent `git status` ve belgeyle başlar. Yarım ya da derlenmeyen dosya `main`'e girmez |
| Çakışan değişiklik | Dosya sahipliği görev belgesindedir; çakışırsa sonra gelen iş yeniden temellenir, sözleşme dosyasında tek sahip vardır; çözülmezse mimar işleri sıralar |
| **Güvenlik nedeniyle duran agent** | Çıktısı güvenlik sınıflandırıcısı tarafından durdurulan iş **aynı kapsamla yeniden gönderilmez**, başka agent'a da aynı kapsamla yazdırılmaz (03 düzeltmesi, `docs/DURUM.md`). Mimar kapsamı daraltır (atış kontrolde yalnız emniyet zinciri ve arayüz), durumu `docs/DURUM.md`'ye yazar ve kullanıcıya bildirir; kullanıcı onayı beklenir |
| sol sınırı ya da bozuk sol çıktısı | Sol'un işi Claude'a devredilmez; beklenir ya da kapsam daraltılır. Claude'un yazdığı iş sol incelemesi gelmeden kabul edilmez; yerine Claude incelemesi konmaz |
| İnceleme yakınsamıyor | İki turdan sonra mimar karar verir (2.4); kritik bulguda kabul yok |
| Kararsız (flaky) test | Kabul testiyse kök neden bulunana kadar iş kabul edilmez; kabul dışıysa `AB-nnn` ile karantina |
| CI kotası ya da altyapı arızası | Yerel Mac koşusu kayıtlı komutla yapılır, PR'da "CI yok" yazılır; birleştirme CI dönene kadar durur |
| Kart ya da Windows denemesi yok | Kart gerektiren işler beklemeye alınır, kullanıcıdan istenir; SIL işleri sürer; kartlı sonuç olmadan T4 kabulü verilmez. Windows gelmezse MP8 ve T9'un Windows yarısı "doğrulanmadı" kalır, kademe tabanı açık nokta olarak taşınır |
| Araç sürümü kayboldu ya da değişti | Kilit özeti hata verir; sürüm sessizce değiştirilmez, mimar kararı ve karar günlüğü kaydı gerekir |
| Ölçüm birincili çürütür | Rapor yazılır, yedeğe geçilir, iskelet 8. başlık ve ilgili bölüm güncellenir; planlı yoldur, hata değildir |

---

## 5. Kalite kademelerinin etkisi

Kademe bir deney parametresidir; test sonucu kademesiyle kaydedilir. Fizik, kamera dışı sensörler, otopilot ve HIL her kademede tam çalıştığı için emniyet kabulü kademeden bağımsızdır; kameraya bağlı davranış yalnız kamera açıkken sınanır. CI "Kapalı" kademede (3B ve kamera yok) ve sade sonarla koşar.

| Test | Kapalı | Sade | Tam |
|---|---|---|---|
| T1, T2, T5, T6 (kamera dışı) | Her PR | Aynı sonuç beklenir | Aynı |
| T3 duman, T7 SIL | Her PR | Her PR; gece uzun küme | Gece: çok ışınlı sonar, CPU |
| T4 HIL | Bariyerli ölçümler görselleştiricisiz | Kabul kademesi MP8'e göre | Mac'te raporlanır; kabul HIL'i değil |
| Kameraya bağlı (B03 F5, B07 P4–P8) | B07 P13: görselleştiricisiz B04–B06 testleri | P4, P5 | P4, P5 tekrarlanır |
| Ping360 istatistiği (T6) | — | Kutu yoğunluğu üstel; HIL'de koşar | Rayleigh (KS); SIL'de; HIL'de ancak MP7 uygunsa |
| Kayıt (T10) | "Yalnızca olaylar": tekrar oynatma güvencesi yok | Telemetri: fizik yeniden koşturma | Girdi tekrarı (sonar, kamera) |

Düşük donanımlı Windows'ta kabul edilen kademe MP8'in sonucudur; öncesinde Sade'nin taban olduğu **tahmindir** (Ö11).

---

## 6. Ölçüme bağlı kararlar

### 6.1 Ölçüm işleri

Ölçüm işleri sözleşmelerden ve kodlamadan önce gelir (MP9 hariç; kod gerektirir); sonucu çıkmayan karar kesinleşmiş sayılmaz. Her ölçüm kendi dalında (`olcum/mp<n>-<ad>`) aracını ve raporunu yazar (3.3). **Geçme ölçütleri ilgili bölümlerin başlangıç önerisidir (kaynaklı eşik değil, ölçülmemiştir);** ölçümden önce dondurulur (7.3); sözleşme henüz olmadığından ölçütü ölçümün görev belgesi dondurur. Yedeğe geçiş ölçütleri B01 §6, B05 §6, B06 §6, B07 §6'dadır.

| MP | M | Amaç ve ölçülen büyüklük | Yöntem | Geçme ölçütü | Kesinleştirdiği karar | Kim, nerede |
|---|---|---|---|---|---|---|
| MP1 | M6, M4, M21 (Mac) | Fabrika durumu; 160 MHz ve regülatör; derle-yükle-ayıkla. `TZEN`, `RDP`, `BOR_LEV`, `REV_ID`; `SYSCLK`; DWT/PC saat oranı | B01 K0–K2: salt okunur seçenek baytı (yazma yok); SMPS ve LDO ayrı 1 saat, USB HS yükü altında | K0 yazma yok ve kayıtlı; K1 kesme noktası ve yığın izi, durdurulunca IWDG sıfırlamaz; K2 160 MHz ± HSE toleransı, hatasız | Regülatör, PLL dizisi, Mac araç zinciri | Opus, Mac + kart; kartı kullanıcı bağlar |
| MP2 | M2, M1 | USB HS numaralandırma, kayıp, gecikme, jitter; CDC ve vendor-bulk; PHY varyantı | B01 K3 (100 tekrar × 2 PHY × OS), K4 (256 B × 1 kHz iki yönlü, ≥ 30 dk, sıra + CRC), K5 (yankı turu, 200/400/1000 Hz, 64/256 B, ≥ 30 dk, p50/p99/p99,9/maks, host duraklaması ve kablo çekme dahil) | K3 %100 ve HS; K4 kayıp 0; K5: 400 Hz'te gidiş-dönüş p99,9 + kart adım süresi ≤ 2,5 ms ise 400 Hz bariyer uygun | A1 taşıması (TinyUSB / USBX), PHY, VBUS, CDC / bulk, `L_act`, kontrol hızı | Mac: Opus; Windows yarısı: kullanıcı (MP8) |
| MP3 | M3 | ST-LINK VCP gerçek hız ve gecikme (yedek taşıma); 15 Mbaud garanti sayılmaz | K6: USART1 DMA, 921600 ve 2/3/6/12 Mbaud, sayaç + CRC, ≥ 10 dk | Kayıpsız en yüksek baud ve gecikme raporlanır | Yedek taşıma kullanılabilirliği (06: ≥ 3 Mbaud, 200 Hz, `L_act = 3h`) | Opus, Mac + kart; MP2 birincil geçse de yedeğin kanıtı için |
| MP4 | M5, M19, M17 (kart) | 400 Hz döngü yükü ve en kötü süre; gözetim; bariyer bütçesi | K7: DWT histogramı, `fw-measure`, ≥ 1 saat; boşta, tam sensör, Ping360 en yüksek hız, 03 arka planı, ekran açık/kapalı. K8: sonsuz döngü enjeksiyonu, LSI ölçümü. **İki aşamalı:** W0'da yer tutucu yük, W3 sonu ve W6'da gerçek çekirdek | Çekirdek adımı en kötü ≤ 1,0 ms; varıştan çıktıya p99,9 ≤ 1,5 ms; bariyerli 1 saatte kaçırılan adım 0; her iç kilitlenme ölçülen LSI'ye göre sıfırlar | Kontrol hızı (400/200 Hz), RTOS'suz ya da FreeRTOS, IWDG süresi, ekran | Opus, Mac + kart |
| MP5 | M9 | MAVLink 2 + dialect ile açık serileştirmeli özel biçimin maliyeti (aynı semantik) | İki aday: kartta ayrıştırma/kodlama çevrimi (DWT), RAM ve flash, Ping360 1200 örnek parçalama, uçtan uca gecikme, bakım yükü (B06 §6) | Sayısal eşik 06'da yok; ölçümden önce yazılır: kart adım bütçesinin payı, parçalama sorunsuzluğu | A1 biçimi | Düzenek sol (W0; geçmezse Opus); koşu Mac + kart. Temsili mesaj kümesiyle ölçer (nihai dialect henüz yok); sonuç W1'de A1'e girer |
| MP6 | M33, M13, M28, M12, M30, M29 | Godot'da pencere görünmezken kamera görüntüsü; kamera hattı; uzantı derlemesi; uzun koşu | B07 P1, P2, P4, P5 (macOS: tamamen örtülü, gizli, simge durumu; ekran koruyucu, kapalı ekran), P6, P11; `force_draw(false, dt)`; her durumda ≥ 30 dk | P5: her durumda kare üretimi, ortalama mutlak piksel farkı ≤ 2/255, hedef ≤ 1 piksel (mimari varsayımı); P4: düşen kare < %1; P11: 2 saat, "timeout waiting for fence" yok | Kamera birincil / yedek (A23), Godot sürümü, CMake / SCons, geri okuma yolu | Opus, Mac (GPU); Windows durumları kullanıcı (MP8) |
| MP7 | M11 | Sonar hesap süresi ve ışın yakınsaması | Ping360 coherent + multipath, sabit ışın/saçıcı/yol/darbe; Embree CPU süresi; topluluk yakınsama (B05 §6); platform eşdeğerliği; alt sahne paylaşımının belge doğrulaması | Işın ×2'de kutu ortalama enerjisi, varyansı ve ilintisi değişimi %5 güven aralığında; p99 hesap süresi ping aralığı ve dünya iş parçacığı bütçesi içinde (bütçe 04/05'ten, ölçümden önce yazılır) | Sonar ayrıntısı (HIL'de tam / yalnız SIL), ışın sayısı, Embree sahne yapısı | Opus, Mac; Windows CPU süresi CI'dan (hızı temsil etmez) ve MP8 |
| MP8 | M12, M20, M21 (Win), M30 | Windows hedef bilgisayarının kaldırdığı kademe; ön yüz ve kart zinciri | `tools/olcum/windows/` betiği donanımı kaydeder (CPU, bellek, GPU, sürücü, OS) ve tarar: SIL hızı, HIL 1x'te kaçırılan adım, kamera hattı (MP6 Windows yarısı), MapLibre + uPlot 60 Hz, MP2 Windows yarısı ve ST-LINK sürücü akışı | Seçilen kademede HIL 1x 30 dk kaçırılan adım 0 (Ö11); en eski hedef tarayıcıda panel kabulü (B08 §7 md.10); yoksa bir alt kademe | Kademe tabanı (iskelet §8); Windows'ta kamera | Betiği agent hazırlar, **kullanıcı çalıştırır**; Windows özellikleri bilinmiyor |
| MP9 | M10, M15 | Kart–SIL sayısal fark; fizik yakınsama | **Kod gerektirir** (W3 sonu, W6): açık çevrim tekrar oynatma (B02 §7); macOS-arm64 ve Windows-x64 farkı; h = 1,25 → 0,625 → 0,3125 ms (B04 §7 md.15) | Dondurulmuş sınırlar (7.3); yakınsama mertebesi raporlanır | libm/FMA politikası, libc, `h` ve alt adım | TY + Opus; Mac + kart; Windows izleri CI'dan |

"Geçme ölçütü" sütunu hedeftir; hiçbiri sonuç gibi okunamaz. Kart gerektiren ölçümler kart bağlanana kadar başlamaz; bu sürede kartsız ölçümler (MP6, MP7) ve 2.1'deki istisna (karta bağlı olmayan sözleşmelerin öne alınması) yürür.

### 6.2 Bu bölümün kendi seçimleri

| Karar | Birincil | Yedek | Ölçüm / dayanak |
|---|---|---|---|
| Bağımlılık yönetimi | vcpkg manifest + CPM | Conan 2 | W2'de ilk iki derleme; R09 §2.2 |
| CI ikili önbelleği | vcpkg + NuGet/GitHub Packages | `actions/cache` | 00 09.13; CI kotası |
| Gece işleri | CI'da uzun SIL | Mac'te gece betiği | CI kotası (8.2 md.4) |
| Statik analiz yığını | 2.8'deki dört araç | Yalnız uyarı-hata + cppcheck | W2'de gürültü ve süre kaydı |

---

## 7. Test yaklaşımı ve kabul ölçütleri

### 7.1 Test katmanları

Ayrıntılı test koşulları 01–08 §7'dedir; bu tablo katmanı, yerini ve zamanını sabitler (`T<n>`).

| # | Katman | Neyi sınar | Nerede | Ne zaman |
|---|---|---|---|---|
| T1 | Çekirdek birim (Unity, C) | `core/` işlevleri, kartla aynı sayı tipleriyle: kip tablosu, tahsis, süzgeç, durum makineleri, CFAR, Munkres (B02, B03 §7). Aynı testler kartta da koşar (B01 §7). PC uygulamaları için GoogleTest (R08 §2.6) | PC: Mac, CI; kart: Mac | Her PR; kartta W3 sonrası ve her kabul |
| T2 | Arayüz uygunluğu | A1 altın baytlar, kodla-çöz gidiş-dönüşü, şema özeti kuralları (B06 §7); A2/A3/A5 şema gidiş-dönüşü (C++ ve TypeScript aynı altın dosya); A4 imzaları; A7 manifest özeti; A8 kayıt biçimi | Mac, CI | Her PR (sözleşmeye ya da tüketicisine dokunan) |
| T3 | Kartsız senaryo (SIL) | Hızlandırılmış kapalı çevrim; donmuş sınırlar; aynı ikilide bit eşitliği | Mac, CI | PR duman kümesi; gece Monte Carlo |
| T4 | Kartlı (HIL) | Bariyerli tekrarlanabilir koşular: USB, bariyer, gözetim, adım süresi (B01 K1–K12, B06 bariyer ve oturum) | Yalnız Mac + kart; Windows yarısı kullanıcıda | Ölçüm paketlerinde; W4 sonrası her kabul; sürüm adayı |
| T5 | Fizik doğrulama | Analitik durumlar, enerji, hidrostatik denge, roll/heave periyodu, uç hız, darbe modeli, yakınsama, yayımlanmış referanslar (B04 §7) | Mac, CI; uzun koşu gece | Fizik PR'ı; gece |
| T6 | Sensör istatistik | Allan eğrisi, GM süreci, ≥ 20 tohum ve %95 güven aralığı, açık veri kümesi karşılaştırması (B05 §7) | Mac, CI kısa; gece uzun | Sensör PR'ı; gece |
| T7 | Emniyet ve arıza enjeksiyonu | B03 F1–F10, B02 kritik yol 1–9, B06 hata enjeksiyonu, B01 K8–K9; "sıfır yanlış atış" | SIL: Mac, CI; HIL: Mac + kart | Her PR (SIL); HIL'de her dalga sonunda en az bir kez (B03 §7) |
| T8 | Kart–SIL farkı (M10) | Aynı ikilide bit eşitliği; macOS-arm64 ve Windows-x64 farkı; kart ve SIL açık çevrim tekrar oynatma farkı, ilk ayrışma adımı | Kart: Mac; Windows: CI artefaktı | Gece; kartla W3 sonu ve W6 |
| T9 | Görsel hat | B07 P1–P13: poz akışı, kamera, görünmeyen pencere, iki zamanlı çizim, dalga tutarlılığı, A7, uzun koşu | GPU: Mac (agent), Windows: kullanıcı; CI yalnız GPU'suz kısım | W0 (MP6), W5, W6 |
| T10 | Laboratuvar | Yavaş istemci, erişim, emniyet girdisi eylemi, doğrulayıcı, hata enjeksiyonu, kayıt ve tekrar oynatma (B08 §7); ön yüz taban kabulü en eski hedef tarayıcıda (M20) | Mac, CI | Sunucu: her PR; ön yüz: W5 |

Aşağı katman yeşil değilse yukarısı kabul için koşulmaz (T1, T2 → T3, T5, T6 → T4, T7, T8).

### 7.2 Bir işin bitmiş sayılması

Hepsi sağlanır: (1) Görev belgesindeki kabul testleri iki OS'ta (CI) ve gerekiyorsa kartta ya da GPU'lu makinede geçer; geçmeyen ya da koşulmayan her şey PR'da yazılıdır. (2) Atanan her `AB-nnn` kapanmış ya da mimar tarafından gerekçeli "kabul edilen sınır" yapılmıştır. (3) Karşı ailenin incelemesinde kritik bulgu yok, en fazla iki tur. (4) Statik analiz, bellek ayırma denetimi ve sanitizer temiz; yeni uyarı bastırması gerekçeli. (5) Sözleşme sürümleri değişmemiş; değiştiyse ayrı sözleşme PR'ı bitmiş. (6) Kabul sınırları ilk kabul koşusundan önce dondurulmuş ve değişmemiş. (7) **Ölçülmemiş performans, gecikme ya da yük rakamı kabul gerekçesi olamaz:** "hedef" ya da "tahmin" etiketli rakam ölçüm raporu çıkana kadar hedef kalır; rapor yoksa iş o rakama dayanmaz ve ilgili kısım "doğrulanmadı" ile teslim edilir. Rakamın aşılması ölçüme bağlı kararı yedeğe geçirir (iskelet §8). (8) `docs/DURUM.md` işlenmiştir.

### 7.3 Ayar ile kabulün ayrılması ve sınırların dondurulması

B02, B03, B04, B05, B07 ve B08'de "09'da kesinleşir" denen her eşik için geçerlidir.

| Kural | İçerik |
|---|---|
| Ayrık veri | Ayar senaryoları ve tohumları ile kabul senaryoları ve tohumları ayrı dosyada ve ayrı sayı aralığındadır; ayar yapan kod kabul veri kümesini okumaz. Kalibrasyon verisi ile kabul verisi ayrıdır (Hasan 2024 senaryolarının yarısı kalibrasyon, yarısı kabul; B05 §7) |
| Sınır dosyası | `tests/kabul/<kimlik>/sinirlar.*`; test yazarı yazar, mimar onaylar. B08'in A5'i ayar ve kabul tohum aralıklarını ayrı alan tutar |
| Dondurma anı | İlk kabul koşusundan **önce** ayrı commit ve `kabul-sinir/<kimlik>` etiketiyle; görev belgesi etiketin özetini taşır. Ölçüme bağlı sınırlar (ör. M10 açık çevrim: 60 s'de konum ≤ 1 mm, tutum ≤ 1e-4 rad, itici komutu ≤ 1e-3; B02 §7, tahmin) ilk ölçümden önce dondurulur |
| Gevşetme | Sonuç görüldükten sonra gevşetme mimar kararı ve kayıt gerektirir (eski sınır, yeni sınır, gerekçe); önceki kabul sonuçlarını geçersiz kılar. Sıkılaştırma serbest |
| Karantina | Kabul testi karantinaya alınamaz; kabul dışı kararsız test `AB-nnn` ile alınabilir |
| Monte Carlo | Koşu sayısı koşudan önce görev belgesinde sabittir. "Sıfır yanlış atış" (B03 §7 md.1) için: N koşuda sıfır olay %95 güvenle yaklaşık 3/N üst sınırı verir (üç kuralı); sayı bu kurala göre seçilir, rapora üst sınır yazılır, "sıfır kanıtlandı" yazılmaz |

**Bu bölümün kabulü:** W2'nin CI'ı bir PR'da 2.8'deki işleri iki OS'ta yeşil koşturur; belge denetimi bilerek değiştirilmiş bir ham raporu reddeder; bellek denetimi `malloc` çağıran deneme nesnesini yakalar; sınır dosyası dondurma etiketiyle uyuşmazsa CI hata verir; bir sözleşme, bir ölçüm ve bir uygulama işi şablonla baştan sona döngüden geçer; sol denemesi 2.9'daki ölçütlerle değerlendirilip kayda geçer. Hiçbiri hız rakamına dayanmaz.

---

## 8. Açık noktalar ve iskelete öneriler

### 8.1 İskelete ve komşu bölümlere öneriler (mimar karar verir)

1. **A1 iki aşamalı dondurma** (2.1): iskelet §3 sözleşmeleri kodlamadan önce dondurur, §8 ise A1 taşımasını ve biçimini ölçüme bağlar. Öneri: §3'e "A1 anlamı W0'da, taşıma ve tel biçimi eki W1 sonucundan sonra donar". **0.2: sıra değişikliğiyle gereksiz kaldı (2.1).**
2. **Depo düzeni (§7):** `docs/sozlesmeler/`, `docs/olcumler/`, `docs/gorevler/`, `docs/mimari/acik-bulgular.md`, `cmake/`, `CMakePresets.json`, `vcpkg.json`, `.github/workflows/`, `tests/kabul/`; 01'in önerileri (`firmware/third_party/`, `firmware/cubemx/`, `firmware/bsp/`, `protocol/conv/`) ile birlikte.
3. **Kalite kademesi:** iskelet §6'ya: "emniyet kabulü kademeden bağımsızdır; kameraya bağlı davranış yalnız kamera açıkken sınanır" (5. başlık).
4. **Test kimlikleri çakışıyor:** B01 `K0–K12`, B07 `P1–P13`, B03 `F1–F10` kimlikleri 00'ın `K`/`Ö` kimlikleriyle karışıyor; raporlarda `B<bölüm>/<kimlik>` yazılsın (bu bölümde uygulandı).
5. **"09'da kesinleşir" eşikleri** (B03 Monte Carlo sayısı, B04 eşikleri, B05, B07 ve B08 yük rakamları): bu bölüm rakam üretmez, yöntemi verir (7.3). 01–08'deki ifade "sözleşme işinde dondurulur" olarak yansıtılmalı; her sözleşme işine sınır dosyası sahibi (test yazarı) atanmalı.
6. **B01 §7 "her kart testi macOS ve Windows'ta tekrarlanır"**: agent'lar yalnız Mac'te çalıştığından Windows yarısı kullanıcı makinesinde, MP8 kapsamında koşar; B01'e not gerekir.
7. **Süre farkı:** B06 §7 bariyerli 400 Hz koşusu 30 dk, B01 K7 bir saat ister; iki ayrı ölçümdür (M17 ve M5), birbirinin yerine kullanılmaz; B06'ya not önerilir.
8. **B06 §7** kablo çıkarma için yazılımla kesilebilen USB hub'ı anıyor, aracı seçilmemiş; MP2 ve T7'nin HIL kısmı için kullanıcıdan donanım gerektirebilir.

### 8.2 Açık noktalar

1. **Mimari onayı** (kullanıcı) olmadan W0 başlamaz; 01–08 taslaktır.
2. **A16** (yapı zamanı Python/Node, 00): yanıt gelene kadar kritik yolda (derleme, kabul testi, altın dosya üretimi) kullanılmaz; ön yüz paketi hariç yardımcı betikler POSIX kabuğu ve CMake ile yazılır. B02 §7'deki referans üreticisi (MSS/PVS) bu yanıta bağlı.
3. **A23, A24** (kamera görünürlük kabulü, çözünürlük ve hız): MP6 kapsamı ve MP8'in kamera satırı bunlara bağlı.
4. **CI olanakları doğrulanmadı:** özel depoda dal koruma kuralı, dakika kotası, macOS ve Windows iş süresi maliyeti, ST ve Arm araç zincirinin CI'da kurulabilirliği. W2'nin ilk işi bunları kayda geçirir; sonuca göre 2.7 ve 2.8 ayarlanır. Dal koruma kullanılamazsa mimar denetimi tek kapıdır.
5. **QGC hedef mi prova mı** (00 A6) yanıtsız: B06 M18 ve B08'in UDP uç noktası için bu bölümde test işi tanımlanmadı.
6. **Windows hedef bilgisayarının özellikleri bilinmiyor** (Ö11): MP8 öncesi kademe tabanı varsayımdır.
7. **Kod gerektiren ölçümler** (M10, M15, MP4'ün gerçek çekirdekli turu) W3 sonrasına kalır; W0 yalnız yer tutucu yükle ön bilgi verir.
8. **Linux CI işi:** R09 uyarı çeşitliliği için ek bir Linux işi önerir; K4 yalnız macOS ve Windows'u kapsadığı için eklenmesi mimar kararıdır, eklenmedi.
9. **Self-hosted kartlı runner** kurulmadı; kartlı testler Mac'te betikle koşar. İleride istenirse ayrı karar.
