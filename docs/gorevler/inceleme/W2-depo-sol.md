# sol incelemesi: .

## Hüküm

birleştirilemez

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `cmake/IdaCekirdekSiniri.cmake:63` | Çekirdek sınırı yalnız hedefin `SOURCES` listesindeki dosyaları tarıyor; içerilen başlıkları takip etmiyor. Örneğin mevcut `iskelet.c` değiştirilmeden, `iskelet.h` içine aynı dizindeki `stm32u5xx.h` başlığının eklenmesi denetimden geçer. Include dizinlerinin adında yasak sözcük bulunması da gerekmez. Mevcut karşı örnekler yalnız doğrudan kaynak ve include yolu eklemeyi sınadığı için bu kaçışı yakalamaz. | Derlemenin gerçek başlık bağımlılıklarını denetle; dolaylı başlık içermeyi de kapsayan karşı örnek ekle. Yapılandırma sırasında henüz bulunmayan veya generator expression ile verilen kaynakların sessizce atlanması da aynı denetim kapsamında kapatılmalı. | kritik |
| 2 | `tools/denetim/statik-analiz.sh:193` | Statik analiz karşı örneği herhangi bir `error` satırını başarılı algılama sayıyor; araçların çıkış durumları alınmıyor. Karşı örnek yalnız `#error denetim_calismadi` içerdiğinde derleme/önişleme hatası beklenen analiz bulgusunun yerine geçebilir. Olağan kaynakların analizi temizse betik yine 0 dönebilir. Ana clang-tidy çağrısında da `clang-diagnostic-error`, “araç çalışmadı” yerine sıradan bulgu sayılıyor. LLVM bu tanıyı analizi engelleyen derleme hatası olarak tanımlar. [LLVM belgesi](https://clang.llvm.org/extra/clang-tidy/#using-clang-tidy). | Her çağrının çıkış durumunu ayrı kaydet; derleme, önişleme ve araç hatalarını reddet. Karşı örnekte beklenen gerçek denetim kimliklerini doğrula; ayrıca sözdizimi/önişleme hatasının 70 ile sonuçlandığını sınayan test ekle. | kritik |
| 3 | `tools/denetim/belge-denetimi.py:66` | Ardıl kart deseni bütün STM32U5 ailesini serbest bırakıyor. Örneğin bir CMake dosyasındaki `set(IDA_KART STM32U5G9J-DK1)` satırı yakalanmaz. K8’in istisnası STM32U5 ailesi değil, yalnız STM32U5A9J-DK’dır. Mevcut test yalnız başka STM32 ailesini sınar. | Ortak ST araçlarını ve aile başlıklarını koruyarak, başka U5 kartı/MCU’sunun hedef seçilmesini de denetle. Aynı aileden ardıl kart için karşı örnek ekle. | önemli |
| 4 | `.github/workflows/derleme.yml:46` | Yeni gönderim sürmekte olan eski macOS işini iptal etmiyor. Etiketle başlayan iş `derleme-macos-<PR>-etiket` grubunda; `synchronize` olayında macOS işi atlanıyor. Windows işi farklı grupta olduğundan eski macOS koşusu devam edip eski commit için yeşil sonuç üretebilir. Bu, görev §5’teki “aynı dalda yeni gönderim eski koşuyu iptal eder” şartını karşılamıyor. [GitHub concurrency kuralı](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency). | Etiket olayının Windows işini iptal etmemesini korurken, yeni gönderimin önceki macOS koşusunu da iptal etmesini sağlayan bir düzen kur. Elle ve etiketle başlatılan aynı dal koşularının ilişkisini de açıkça belirle. | önemli |

Karşı örnekler dosyalara yazılmadı veya çalıştırılmadı; yukarıdaki sonuçlar kod akışından çıkarıldı. İncelemede dosya değiştirilmedi, kurulum veya derleme yapılmadı.

## Yeniden hesaplananlar

| Kalem | Yeniden hesap | Sonuç |
|---|---|---|
| Bağlayıcıdaki FLASH | `4096 × 1024 = 4.194.304 B` | 4 MiB |
| SRAM1 ve başlangıç yığını | `768 × 1024 = 786.432 B`; `0x20000000 + 0xC0000 = 0x200C0000` | `_estack` hesabı doğru. SRAM1’in 768 KiB olması ST belgesiyle uyumlu. [DS13543 §3.5](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf). |
| Ayrılmış yığın payı | `0x2000 = 8192 B` | CI’daki 8200 B RAM kullanımının 8192 B’si ayrılmış paydır; bu sayı ölçülmüş çalışma zamanı yığın tüketimi değildir. |
| Eigen duman matrisi | `det(A)=18`; `A⁻¹ = [[5,-2,1],[-2,8,-4],[1,-4,11]] / 18` | Matris terslenebilir; tam aritmetikte `A·A⁻¹=I`. |
| Başlık birleşimleri | C17: Clang, MSVC, Arm GCC; C++20: Clang, MSVC | Kapsam **beş** birleşim tanımlıyor. Kabul md.3’teki “dört” ifadesi görev belgesi tutarsızlığıdır; kod beşini kuruyor. |
| Dakika ücretleri | macOS/Windows: `0,062/0,010=6,2`; macOS/Linux: `0,062/0,006≈10,33` | Yorumlardaki “altı katı” ve “onda biri” yaklaşık anlatımlardır. Ücretler güncel birincil kaynakla uyuşuyor. [GitHub fiyatları](https://docs.github.com/en/billing/reference/actions-runner-pricing). |
| Kaydedilmiş soğuk koşu | Windows 7 dk 3 s → 8 ücretli dakika; Linux 11 s → 1 dakika | `8×0,010 + 1×0,006 = 0,086 USD/gönderim`. |
| Kaydedilmiş sıcak koşu | Windows 2 dakika; Linux 1 ücretli dakika | `0,026 USD/gönderim`. Yalnız bu iki iş ve tüm 12 USD tahsis edilirse en çok `⌊12/0,026⌋=461` gönderim; soğuk durumda 139. Bunlar aylık PR kapasitesi değildir. |
| macOS dahil kapasite | Sıcak gönderime ek maliyet: `0,062 × ⌈macOS süresi/dakika⌉` | macOS koşusu olmadığı için bütün bir PR’ın ölçülmüş maliyeti ve aylık kapasitesi henüz hesaplanamaz. GitHub işleri ayrı ayrı yukarı yuvarlar. [Faturalama kuralı](https://docs.github.com/en/billing/reference/actions-runner-pricing). |

12 USD, GitHub Free’ın genel aylık dahil kullanım tutarıyla uyumludur; hesabın planı ve kalan kullanılabilir tutarı bu incelemede hesap ekranından doğrulanmadı. [Plan tablosu](https://docs.github.com/en/billing/reference/product-usage-included).

**Sürüm ve özet doğrulamaları:**

| Kalem | Sonuç |
|---|---|
| Eylem SHA’ları | Belirtilen tam SHA’lardaki birincil dosyalar sırasıyla checkout **7.0.1**, setup-python **7.1.0**, cache **6.1.0**, upload-artifact **7.0.2** bildiriyor. [checkout](https://raw.githubusercontent.com/actions/checkout/3d3c42e5aac5ba805825da76410c181273ba90b1/package.json), [setup-python](https://raw.githubusercontent.com/actions/setup-python/6d4fd22b20465ccc0f7354476588e8df7c9ca0e0/package.json), [cache](https://raw.githubusercontent.com/actions/cache/55cc8345863c7cc4c66a329aec7e433d2d1c52a9/package.json), [upload-artifact](https://raw.githubusercontent.com/actions/upload-artifact/cf430e030ddbb5b0abf93d22962f4752f3646cd9/package.json). |
| CPM 0.43.2 | `49a3bef9…a2f232aa` özeti yayımlanan varlık özetiyle aynı. [Sürüm varlıkları](https://github.com/cpm-cmake/CPM.cmake/releases/expanded_assets/v0.43.2). |
| clang-tidy 21.1.6 | macOS arm64 ve Windows x64 wheel SHA-256 değerleri PyPI ile aynı. [PyPI](https://pypi.org/project/clang-tidy/21.1.6/). |
| pymavlink 2.4.50 | Kaynak arşiv adresi ve `8d65a8b0…e7f10062` özeti PyPI ile aynı. [PyPI](https://pypi.org/project/pymavlink/2.4.50/). |
| Eigen 5.0.1 | Sabitlenen vcpkg baseline içindeki port sürümüyle aynı. [Baseline’daki port](https://raw.githubusercontent.com/microsoft/vcpkg/9eec002df1cd109f8f104cd966a763daf9751489/ports/eigen3/vcpkg.json). |
| Unity ve cppcheck | Unity 2.7.0’ın `b6763fb…`, cppcheck 2.22.0’ın `a436ca3…` commit ilişkileri doğrulandı. Otomatik kaynak arşivlerinin SHA-256 değerleri bağımsız yeniden hesaplanmadı. [Unity](https://github.com/ThrowTheSwitch/Unity/releases/tag/v2.7.0), [cppcheck](https://github.com/danmar/cppcheck/releases/tag/2.22.0). |
| Arm GNU 14.3.rel1 | Resmî indirme sayfası robot doğrulamasına yönlendi; iki `.sha256asc` dosyası web aracında desteklenmeyen içerik türü verdi. **Arm arşiv özetleri bağımsız doğrulanmadı.** |

## İskelete ya da diğer bölümlere notlar

İncelenen fark `3e82a085..f9469f46`, dal `is/w2-depo`dur. Güncel sözleşme başvuruları istendiği gibi `origin/sozlesme/w1-butunlesme` üzerinden okundu. Belirtilen sözleşme dosyalarında bu farkın yaptığı içerik değişikliği yok.

**Kapsam maddeleri:**

| Görev kapsamı | Durum | Kanıt / sınır |
|---|---|---|
| §1 dizin iskeleti ve README | karşılandı | İskelet §7’nin üst dizinleri ve amaçlarını açıklayan `BENIOKU.md` dosyaları var; W0 prototipleri derlemeye bağlanmamış. |
| §2 altı ön ayar, CMake alt sınırı, Ninja | karşılandı | Altı configure/build ön ayarı mevcut; CMake 3.25 ve şema 6 uyumlu. [CMake belgesi](https://cmake.org/cmake/help/v3.25/manual/cmake-presets.7.html). |
| §2 ortak `ida_core`, ISO C17 ve sınır | kısmen | Aynı statik hedef iki kökten kullanılıyor; standart ve uyarı ayarları mevcut. Sınır zorlamasında bulgu #1 var. |
| §2 firmware araç zinciri ve yer tutucu | karşılandı | Cortex-M33, hard-float, newlib-nano, bağlayıcı betiği, başlangıç ve boş uygulama kurulmuş. ST kütüphaneleri alınmamış; alt modül yöntemi belgelenmiş. |
| §2 uyarı ve kayan nokta politikası | karşılandı | İstenen GCC/Clang ve MSVC seçenekleri, C++20 ve uzantı ayarları uygulanıyor. |
| §2 başlıklar ve MAVLink birleşimi | kısmen | Nesne hedefi varsayılan derlemeye giriyor; MAVLink üretimi bağlı. Windows ve Arm GCC kanıtı mevcut; Apple Clang için bağımsız CI kanıtı yok. |
| §3 vcpkg, CPM ve gerçek bağımlılık | kısmen | Baseline, override, CPM özetleri ve Eigen duman testi var; Windows’ta çözülüp çalışmış. İkinci işletim sisteminin CI sonucu eksik. |
| §3 Unity ve aynı testin firmware derlemesi | karşılandı | Host testi koşmuş; aynı test kaynağı iki firmware yapılandırmasında ELF olarak bağlanmış. |
| §4 belge/depo denetimi ve kendi testleri | kısmen | Linux günlüğünde 19 test geçmiş; 378 dosyada 0 bulgu, 57 uyarı var. Ardıl kart denetiminde bulgu #3 mevcut. |
| §4 bellek ayırma yasağı | karşılandı | Windows üzerindeki her firmware ön ayarında altı test geçmiş; nesne, bağlama ve imaj karşı örnekleri bağlı. MSVC host eksikliği bilinen ayrı iştir. |
| §4 statik analiz, gürültü ve süre | kısmen | Araçlar çağrılmış; boş çekirdekte iki araç da 0 bulgu bildirmiş. Karşı örnek doğrulaması bulgu #2 nedeniyle güvenilir değil. Günlükteki 0 s, saniye çözünürlüğündeki ölçümdür. |
| §5 host, sanitizer, firmware ve artefakt işleri | kısmen | Windows adımları geçmiş; rapor artefaktı yayımlanmış. macOS işi atlanmış. |
| §5 tetikleyici ve iş koşulları | karşılandı | Kod içeren PR gönderimlerinde Windows; yalnız `ci-mac` eklenmesinde veya elle macOS; belge PR’larında Linux belge denetimi kurulmuş. |
| §5 eski koşunun iptali | kısmen | Windows ve belge işleri için var; eski macOS işi için bulgu #4 mevcut. |
| §5 açık koşucu sürümleri, SHA ve izinler | karşılandı | `*-latest` yok; eylemler tam SHA; `contents: read`; checkout kimlik bilgilerini saklamıyor. |
| §5 kota ölçümü ve sonuç raporu | kısmen | Genel ücretler doğrulandı; Windows süreleri kayıtlı. İncelenen farkta ve PR açıklamasında istenen bütün kabul sonuçlarını, sürüm tablosunu ve tam PR kota hesabını birleştiren rapor yok. |
| Kapsam dışı ürün ve atış kontrol kodu | karşılandı | İncelenen değişiklikte hedef seçimi, hedefleme çözümü veya atış ölçütü uygulaması görülmedi. |

**Kabul maddeleri:**

| Kabul | Durum | Kanıt |
|---|---|---|
| 1 — temiz host derlemesi ve örnek test, iki OS CI | kısmen | Güncel HEAD’de Windows host-debug 7/7 test geçmiş; macOS CI sonucu yok. |
| 2 — firmware debug/release, rapor ve ayırma denetimi | karşılandı | İki yapı bağlanmış; her birinde 6/6 denetim geçmiş; harita/boyut üretimi ve rapor artefaktı mevcut. |
| 3 — başlıkların bütün derleyici/dil birleşimleri | kısmen | MSVC C17/C++20 ve Arm GCC C17 geçmiş. Apple Clang sonucu yerel kayıt beyanına dayanıyor. |
| 4 — bozuk belge ve çekirdek sınırı karşı örnekleri | kısmen | Mevcut belge ve doğrudan include karşı örnekleri geçmiş; dolaylı başlık kaçışı kapanmamış. |
| 5 — aynı PR’da iki OS yeşil ve ölçülmüş kota hesabı | karşılanmadı | Windows yeşil; macOS `skipped`. Tam PR maliyeti ve kapasite raporu eksik. |
| 6 — kişisel yol, hareketli sürüm etiketi ve Rust bağımlılığı yok | karşılandı | Mevcut farkta bunlara rastlanmadı; Linux belge işi de yeşil. Denetimin gelecekteki ardıl kart girdilerini kaçırması ayrıca #3’tedir. |

CI kanıtı: güncel HEAD’in [derleme koşusu](https://github.com/polite10/ida/actions/runs/44704213972) ve [belge koşusu](https://github.com/polite10/ida/actions/runs/44704213935). Windows günlüğü MSVC **19.51.36260**, araç takımı **14.51.36231**, Visual Studio **18.11.3** bildiriyor; kullanıcı tarafından verilen Windows bilgisiyle çelişki yok.

İş akışlarında PR başlığı, dal adı veya etiket metninin `run:` kabuk gövdesine doğrudan yerleştirildiği bir yol görülmedi. İndirmeler sabit özetlerle denetleniyor; CPM önbellekteki kendi dosyasını da yeniden doğruluyor. Önbellek anahtarları işletim sistemi/mimariyi ayırıyor; vcpkg ikili anahtarı derleyici sürümünü içermese de bunun tek başına yanlış ikili kullanımına yol açtığı gösterilmedi.

Bilinen macOS CI eksikliği, yerel ASan kilitlenmesi, iki kapalı clang-tidy denetimi, değiştirilmeyen bellek betiğinin MSVC sınırı ve inceleme bağlantılarının uyarı olması yeni bulgu sayılmadı. Hesabın gerçek planı/kalan kotası, Arm arşiv özetleri, Unity/cppcheck arşiv özetlerinin yeniden hesabı ve yerel Mac testlerinin bağımsız tekrarı doğrulanmadı.