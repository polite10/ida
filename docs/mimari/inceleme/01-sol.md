# sol incelemesi: 01-kart-yazilimi

## Hüküm

Bölüm bu haliyle kabul edilmemeli. Sekiz başlıklı yapı, sorumluluk sınırları, IMU tetiklemeli ana döngü tercihi ve araştırma düzeltmelerinin çoğu uygundur. Ancak iskelet 0.3’ün kimlikli olay, süreli komut, iki ölçütlü tazelik ve zaman çerçevesi gerekleri tamamlanmamış; A4’ün duvar çağrısı, gözetim koşulları ve yeniden gönderim davranışı emniyet açısından açıklar içeriyor. Zaman bütçesi de yazılan en kötü gecikmeyi kanıtlamıyor. Kritik maddeler giderilip 02 ve 06 ile tek bir arayüz sözleşmesi oluşturulduktan sonra yeniden incelenmeli; donanım yeterliliği M1–M5, M17 ve M19 ölçümlerine bağlı kalmalı.

## Düzeltilmesi gerekenler

Satır numaraları, aksi belirtilmedikçe `docs/mimari/01-kart-yazilimi.md` dosyasına aittir.

| # | Yer (başlık ya da satır) | Sorun | Doğrusu / önerilen düzeltme | Önem (kritik / önemli / küçük) |
|---|---|---|---|---|
| 1 | §2.4, 147; §2.7, 199; §3.1; §8.2.2 | Atış hâlâ `fire[2]` alanlarıyla taşınıyor. Tekrar gönderimde sıfırlamak, iskelet §3.2’nin benzersiz olay kimliği ve alıcıda tekilleştirme şartını karşılamıyor. İlk gönderim kaybolursa olay da kaybolabilir. Sürekli itici komutunun kullanım süresi tanımlanmamış. | A1/A4’te sürekli komut ile olay ayrılmalı. Olay kimliği yeniden gönderimde korunmalı; alıcı aynı olayı yalnız bir kez uygulamalı. Sürekli komutun geçerlilik süresi ve eski önbelleğin iptal koşulları tanımlanmalı. | kritik |
| 2 | §2.7, 199; §3.1; `02-ortak-cekirdek.md` §2.2 | Platform maskesi yalnız `armed && fire[i]` denetliyor. 02, atış modülünü kip komutlarını uygulamadan önce çağırıyor. Aynı adımdaki izin daraltan kip değişikliği, `armed` doğru kalırken atışa geç yansıyabilir. | İskelet §3.2 uyarınca disarm ve izin daraltan komutlar atış modülünden önce uygulanmalı. Son çıktı güncel kip, arıza ve atış izniyle doğrulanmalı; yalnız arm biti yeterli değil. | kritik |
| 3 | §2.4, 102, 147–148; §2.5 | Kesilme ölçütü “son IMU varışı”. Yinelenen, eski veya geçersiz IMU’nun bu süreyi yenileyip yenilemediği tanımsız. Alım zamanı yalnız tanılamada tutuluyor; A4 örneğinde bulunmuyor. Düzenli gelen bayat akış canlı görünebilir. | Her örnekte ölçüm zamanı ve yerel alım zamanı ayrı korunmalı. Canlılık yalnız kabul edilmiş, sıra ve ölçüm zamanı ilerleyen örnekle yenilenmeli. İskelet §4’ün iki ölçütlü tazelik denetimi uygulanmalı. | kritik |
| 4 | §2.5; §3.1–3.4 | İskelet 0.3’ün IMU’dan bağımsız zaman çerçevesi hiç ele alınmıyor. `tick_wall` yalnız `now_w` aldığı için IMU yokken gelen disarm, bağlantı olayı veya sensör girdisi çekirdeğe aktarılamıyor. | 02/06 ile zaman çerçevesinin kartta karşılanması ve IMU gerektirmeyen sağlık/olay girdisi tanımlanmalı. Kartta çağrıyı donanım zamanı sürmeli; IMU’suz çıktının kimliği ve bariyerle ilişkisi açık olmalı. | önemli |
| 5 | §2.4, 102; §2.5, 157; §4 | `T_gap=10 ms` aşılınca 100 Hz çağrı deniyor; ilk çağrının hemen mi, sonraki 10 ms sınırında mı yapıldığı belirtilmemiş. Dolayısıyla güvenli çıktıya geçiş üst sınırı belirsiz. “4 periyot” açıklaması 200 Hz yedeğinde yanlış. | Mutlak son tarihlerle ilk çağrı ve devam aralığı tanımlanmalı. Algılamadan çıktı teslimine kadar üst sınır verilmeli. 400/200 Hz için eşik sabit süre mi, periyot sayısı mı olacak açıkça seçilmeli. | önemli |
| 6 | §2.7, 186–193; §8.2.1 | Besleme için `alive_ok=true` zorunlu; fakat 02 §4, IMU yokluğunda bu bayrağı `false` yapıyor. Böylece “kablo çıkınca reset olmaz” davranışı mevcut A4 ile sağlanamıyor. Öneri yazılması uyuşmazlığı çözmüyor. | İç yazılım sağlığı ile dış veri kullanılabilirliği ayrı alanlar olmalı. 01 ve 02 aynı tanımı kullanmalı; veri yokluğu güvenli çıktı üretirken sağlıklı duvar çağrıları gözetimi sürdürebilmeli. | kritik |
| 7 | §2.2, 62; §2.7, 204 | Platform güvenli beklemesinde çekirdek çağrılmıyor; gözetim ise son pencerede tamamlanmış çekirdek çağrısı istiyor. IWDG başladıktan sonra girilen güvenli bekleme yeniden reset döngüsüne dönüşebilir. | Başlatma, normal çalışma ve güvenli bekleme için ayrı canlılık koşulları tanımlanmalı. Güvenli beklemenin de ilerleyen servis/sağlık kanıtı olmalı. | önemli |
| 8 | §2.7, 183–191; §7 K8 | Son 50 ms’de bir kez ilerlemiş olmak, koşul durduktan sonra yaklaşık 50 ms daha beslemeye izin verir. Bu tür arızada reset süresi yalnız 100 ms değildir. “Servis çağrıldı ve döndü” de iç kuyruğun ilerlediğini tek başına kanıtlamaz. | Görev ilerleme sayaçları ve bekleyen iş varken ilerlememe sınırları tanımlanmalı. Arıza başlangıcından reset sonuna toplam bütçe, sağlık penceresi ve LSI toleransıyla hesaplanmalı. | önemli |
| 9 | §2.4, 99–139 | 1,57 ms gecikme hesabı kesme payını, parametre kaydını ve sağlık değerlendirmesini dışarıda bırakıyor. Ayrıca döngü servisleri arasında yeniden alım kontrolü yoksa bekleme yalnız en uzun tek dilimle sınırlanamaz. | Her dilimden sonra kritik iş kontrolü veya eşdeğer öncelikli yürütme tanımlanmalı. Kesme yığılması, maskeleme ve bütün platform işleriyle yanıt süresi yeniden hesaplanmalı. | önemli |
| 10 | §2.4, 100, 150; §3.2 | `tud_task()` için 150 µs sınırı yalnız tahmin. FreeRTOS yedeğinde aynı çağrı bekleyebilir: 0.21.0 başlığı `tud_task_ext(UINT32_MAX,false)` çağırıyor. RTOS’suz kuyruk davranışıyla RTOS davranışı aynı değil. | Servis için beklemesiz çağrı, olay sayısı ve callback süre sınırları açıkça belirtilmeli. RTOS yedeğinde USB bekleyişi duvar çağrılarını durdurmamalı. [TinyUSB API](https://github.com/hathach/tinyusb/blob/0.21.0/src/device/usbd.h), [servis uygulaması](https://github.com/hathach/tinyusb/blob/0.21.0/src/device/usbd.c). | önemli |
| 11 | §2.4, 124; §3.2, 265 | `write()` kısmi yazabiliyor; çerçevenin kalanının nasıl tutulduğu tanımlı değil. Aktüatörü “önce yazmak”, daha önce FIFO’ya konmuş telemetriyi geçmesini sağlamaz. Yazılamayan atış/aktüatör çıktısında yalnız sayaç artırılıyor. | Çerçeve bütünlüğü, kısmi yazım devamı, ayrılmış kritik kapasite ve kuyruk sınırları tanımlanmalı. Olay teslimi kimlikli yeniden denemeyle; sürekli komut düşürme ise açık geçerlilik politikasıyla ele alınmalı. | kritik |
| 12 | §2.4, 147; §2.7; §4 bağlantı kopması | Duvar arızasından sonra yinelenen IMU’ya eski motor çıktısı gönderilebilir. Kopmada TX temizleniyor, fakat RX, biriktirici, komutlar ve önceki çıktı önbelleğinin temizlenmesi belirtilmiyor. Eski arm komutu yeniden bağlanmada işlenebilir. | Oturum değişimi ve güvenli duruma girişte hangi tamponların/izinlerin iptal edildiği tanımlanmalı. Eski çıktı güncel emniyet durumunu aşamamalı; reset sonrası yeni arm şartı taşıma kuyruğundaki eski komutlarla karşılanamamalı. | kritik |
| 13 | §2.4, 143–148; §3.3 | Örnek taşması tanımlı, `cmds[]` taşması tanımsız. 06 §4 taşmada yeni örneği atıyor; 01 en eskiyi atıyor. IMU kuyruğu ve normal örnek kapasitesinin ilişkisi de açık değil. | Sensör, IMU ve komut kuyruklarının ayrı kapasiteleri ve tek taşma politikası yazılmalı. Disarm gibi izin daraltan komutların sıradan kuyruk doluluğunda kaybolmaması sağlanmalı. | önemli |
| 14 | §2.4, 105, 144; §3.1 | Sonar işaretçisi 02’ye göre yalnız çağrı boyunca geçerli; algılama ise arka planda sürüyor. Çift tamponun yeniden kullanımı sırasında devam eden işin veriyi nasıl koruduğu yok. Sürekli RX yükünde arka plan işi süresiz ertelenebilir. | Profil sahipliği/kopyalama, tampon devri, iptal ve azami iş gecikmesi tanımlanmalı. FreeRTOS yedeğinde paylaşım düzeni ve sonuçların hangi adımda tüketileceği tamamlanmalı. | önemli |
| 15 | §2.6; §3.5 | Flash işi “durum makinesi” olduğu için kendiliğinden bloklamaz sayılıyor. Başlamış işlemin güvenli durumda nasıl “iptal” edildiği yok. Çift banka desteği de gereksiz biçimde tamamen belirsiz bırakılmış. | Donanım işlemi başlatma/tamamlama ve hata zaman aşımı tanımlanmalı; iptal yeni işlemleri durdurmalı, sürmekte olan işlemin sonucunu yönetmeli. DS13543 RWW desteğini belgeliyor; yerleşim koşulları ayrıca doğrulanmalı. HAL’ın `_IT` silme işlevinde bile başlangıç beklemesi bulunuyor. [DS13543](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf), [HAL flash kaynağı](https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/2552682ee5a0d6f826a6435750fc9267be65868b/Src/stm32u5xx_hal_flash_ex.c). | önemli |
| 16 | §3.5 | A/B deposunda kalıcı bayt düzeni, başlıkla birlikte azami boyut, tamamlanma işareti, sayaç sarılması ve kayıt sonucunun çekirdeğe bildirimi yok. Kayıt sürerken yeni arm isteği de tanımlanmamış. | Sürümlü kalıcı biçim, CRC kapsamı, son yazılan commit işareti, kapasite denetimi ve başarı/hata bildirimi tanımlanmalı. Kayıt boyunca arm politikası açık olmalı. | önemli |
| 17 | §2.2, 53–67 | Başlatma sırası HAL zaman tabanının güç/saat beklemelerinden önce kurulmasını söylemiyor. IWDG güç ve PLL işlemlerinden sonra başlıyor. PLL yedeğinde yalnız M/N/R değişimi anlatılıyor; giriş aralığı ayarı da değişmeli. | HAL başlangıcı ve zaman aşımı kaynağı sıraya eklenmeli; IWDG öncesi hata yolları sınırlı olmalı. Her PLL varyantı tam yapılandırmayla tanımlanmalı. ST örneği `HAL_Init`, `PLLRGE` ve flash gecikmesini açıkça kuruyor. [ST başlangıç örneği](https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/Core/Src/main.c). | önemli |
| 18 | §2.5, 154 | Üst–alt–üst okuması tek başına yeterli değil: alt sayaç taşmış, taşma ISR’si henüz üst yarıyı artırmamış olabilir. İki üst okuma eşitken eski üst/yeni alt birleşebilir. | Bekleyen taşma bayrağını da kapsayan monoton okuma algoritması tanımlanmalı; taşma sınırı ve kesme maskeliyken okuma sınanmalı. | önemli |
| 19 | §2.4, 115–122 | MemManage, BusFault ve UsageFault sabit öncelikli gösterilmiş. `BASEPRI`nin “yalnız ilgili kesmeyi” maskelediği de yanlış; eşik ve daha düşük önceliklerin tümünü maskeler. | Sabit ve yapılandırılabilir exception öncelikleri ayrılmalı; öncelik gruplaması ve maskeleme kapsamı yazılmalı. MCU başlığı 4 öncelik biti belgeliyor. [Arm BASEPRI açıklaması](https://arm-software.github.io/CMSIS_6/main/Core/group__Core__Register__gr.html), [ST cihaz başlığı](https://github.com/STMicroelectronics/cmsis-device-u5/blob/main/Include/stm32u5a9xx.h). | önemli |
| 20 | §2.6, 179; §4, 310 | Yığın taşmasında yığına itilmiş PC/LR/xPSR’nin mutlaka okunabilir olduğu varsayılıyor. “60 s içinde üç reset” için resetler arasında süreyi koruyan saat tanımlanmamış. | Hata kaydında stacking hatası ve geçersiz SP kontrolü, bağımsız acil yığın veya asgari kayıtsız reset yolu belirtilmeli. Reset döngüsü için kalıcı zaman dayanağı ya da farklı, uygulanabilir sayaç politikası seçilmeli. | önemli |
| 21 | §2.6, 175–177; §2.4 FreeRTOS yedeği | Standart ayırıcı denetimi düzeltilmiş; fakat RTOS yedeğinde görev, kuyruk ve yığınların statik oluşturulması zorunlu kılınmamış. Libc sembol taraması özel havuz ayırıcılarını yakalamaz. | RTOS ve ara katmanlar için statik oluşturma/yapılandırma şartı eklenmeli; özel ayırıcılar kod incelemesinde denetlenmeli. LVGL istisnası mevcut tasarımda etkinmiş gibi uygulanmamalı. | önemli |
| 22 | §3.1–3.3; §8.2.3 | 06’nın `protocol/ida_link` oturum/parti arayüzü ile 01’in `conv/acc` arayüzünün sahipliği eşlenmemiş. 06’nın beklediği `us_from_thr` dönüşümü de burada tanımlı değil. `BAD_INPUT` sonrasında çıkış kullanımı belirtilmiyor. | Tek dönüştürücü/oturum sözleşmesi seçilmeli. Normalleştirilmiş komuttan A1 darbe genişliğine dönüşüm, hata dönüşleri ve `plat` alanları 02/06 ile eşlenmeli. | önemli |
| 23 | §6; §7 K5–K8 | Bariyer kabulü `yankı p99.9 + çekirdek ≤2,5 ms` ile değerlendiriliyor; PC hesap süresi ve çekirdek dışı kart gecikmesi eksik. Bu eşik 06’nın bütçesiyle uyuşmuyor. | Ölçülen yolun başlangıç/bitiş noktaları belirlenmeli; tam yol bütçesi kullanılmalı. Bayat/tekrarlı IMU, IMU’suz disarm, reset sonrası eski arm, güvenli bekleme, TX doluluğu ve sonar tampon ömrü kabul testlerine eklenmeli. | önemli |
| 24 | §8.1.4; son paragraf | Geciken adım politikası bütünüyle açık gösteriliyor; oysa iskelet son komutla ilerleme, sayma ve çalıştırmayı geçersiz işaretleme kararını vermiş. “İskeletle çelişki bulunmadı” hükmü de 0.3 için yanlış. | Açık kalan protokol ayrıntıları bağlayıcı karardan ayrılmalı; bölüm iskelet 0.3’e göre yeniden değerlendirilerek sonuç paragrafı düzeltilmeli. | küçük |

## Yeniden hesaplananlar

Aşağıdaki sonuçlar bağımsız aritmetik kontrollerdir; çalışma süresi ölçümü değildir.

| Konu | Yeniden hesap | Bölümdeki değerle karşılaştırma |
|---|---|---|
| Fizik/kontrol oranı | `1/800=1,25 ms`; `1/400=2,5 ms`; `1/200=5 ms`. Kontrol periyodu sırasıyla 2 ve 4 fizik adımıdır. | Kontrol periyotları doğru; fizik adımıyla ilişki ve zaman çerçevesi bölüme eklenmeli. |
| PLL birincil | `16 MHz / 4 × 80 / 2 = 160 MHz`; PLL giriş frekansı 4 MHz. | Doğru. |
| PLL yedek | `16 MHz / 1 × 20 / 2 = 160 MHz`; PLL giriş frekansı 16 MHz. | Çıkış doğru; iki varyant aynı giriş aralığı ayarıyla kullanılamaz. |
| Çevrim bütçesi | 160 MHz’te çevrim `6,25 ns`; 1 ms `160.000`, 2,5 ms `400.000` çevrim. | Süre hedeflerinin çevrim karşılığı budur. |
| DWT taşması | `2³² / 160.000.000 = 26,8435456 s`. | Yaklaşık 26,8 s doğru. Bir saatlik ölçümde taşmalar biriktirilmeli. |
| Duvar sayacı | 1 MHz’te çözünürlük `1 µs=1000 ns`; 32 bit alt sayaç `4294,967296 s`, yaklaşık 71,58 dakikada taşar. | Çözünürlük doğru; uzun çalışma ve taşma sınırı testi gerekli. |
| Kritik kart işleri | `150+20+1000+100=1270 µs`. | Çekirdek dışındaki kritik işler toplam 270 µs daha gerektiriyor. |
| Yazılan yanıt hesabı | `300+150+20+1000+100=1570 µs`. | Aritmetik doğru; kapsam eksik. |
| Yazılan tur hesabı | `150+20+1000+100+300+200+100=1870 µs`. | Yaklaşık 1,9 ms doğru; parametre/sağlık işleri ve kesmeler hariç. |
| Kesme payı | `%5 × 2500=125 µs`. Bu pay her kontrol penceresinde sınırlandırılmış kabul edilirse yanıt `1695 µs`, tur `1995 µs` olur. | 1,57/1,9 ms toplamlarına dahil edilmemiş. Ortalama %5, kesme yığılması için üst sınır değildir. |
| Dilimler arası bekleme | Yeniden alım kontrolü yapılmazsa tanılama + arka plan + ekran `100+300+200=600 µs` sürebilir; parametre/sağlık eklenmemiştir. | “En uzun tek dilim kadar” bekleme mevcut sıradan çıkmıyor. |
| Bariyer bütçesi | 06’nın varsayımlarıyla `2,50−0,40−1,00=1,10 ms`. | Bu kalan pay taşıma ve kalan yazılım gecikmelerinindir; yalnız yankı ile çekirdek toplamını 2,5 ms’ye karşılaştırmak yeterli değil. |

**Seri yol ve tamponlar:**

- 8N1 kapasitesi `baud/10` olduğundan 921600 baud’da **92.160 B/s/yön**, 2 Mbaud’da **200.000 B/s/yön** elde edilir. Bölüm doğru hesaplıyor.
- Örnek yük `128×400 + 1200×30 = 51.200+36.000 = 87.200 B/s`. Yaklaşık 87 kB/s doğru; 921600 baud kapasitesinin **%94,62**’sidir. Salt bu yükün gereken bit hızı **872.000 baud** olur. Dolayısıyla “≥2 Mbaud gerekir” ancak ek yük, kapasite payı veya gecikme şartıyla gerekçelendirilebilir.
- 06’nın parçalama düzeninde 1200 örnek için `⌈1200/251⌉=5` DATA çerçevesi gerekir. Son çerçeve kısaltıldığında toplam `44+4×267+212=1324 B/profil` olur. Aynı örnekte aşağı yön yükü **90.920 B/s**, yani 921600 baud’un **%98,65**’idir; diğer trafik henüz eklenmemiştir.
- 128 B’nin 921600 baud’da yalnız iletimi **1,3889 ms** sürer. Ortalama bant genişliğine sığmak, 2,5 ms yanıt bütçesine sığmayı kanıtlamaz.
- 4 KiB TX FIFO, 2 Mbaud’da **20,48 ms** veri tutabilir. Kritik çıktı önceliği FIFO’ya girişten önce uygulanmalı; FIFO doluluğu ayrıca sınırlandırılmalıdır.

**Bellek ve ekran:**

| Konu | Yeniden hesap | Sonuç |
|---|---|---|
| Flash | `2×2 MiB=4.194.304 B`. Son iki 8 KiB sayfanın başlangıçları `0x083FC000` ve `0x083FE000`. | Boyut/adres hesabı tutarlı. Parametre başlığı ve içerik birlikte tek sayfaya sığmalı. |
| Ana SRAM toplamı | `768+64+832+832=2496 KiB=2.555.904 B`; bitiş adresi hariç `0x20270000`. | Toplama ve bitişiklik doğru. |
| Diğer SRAM | SRAM4 16 KiB, BKPSRAM 2 KiB; genel toplam 2514 KiB. SRAM3 ECC seçimi erişilebilir kapasiteyi değiştirir. | 2496 KiB bütün RAM’in toplamı olarak sunulmamalı; kullanılmayan bölgeler ve ECC yapılandırması belirtilmeli. |
| Tek ekran tamponu | `480×480×2=460.800 B=450 KiB`. | Doğru; 832 KiB SRAM5’e sığar. |
| İki ekran tamponu | `2×460.800=921.600 B > 851.968 B`. | İki tam RGB565 tampon SRAM5’e birlikte sığmaz. |
| LTDC okuma yükü | `460.800×60=27.648.000 B/s=27,648 MB/s`. | Yaklaşık 27,6 MB/s doğru; yalnız etkin piksel okuma hesabıdır. |
| Yığın kabulü | `16 KiB×%75=12 KiB=12.288 B`. | K12’nin sayısal sınırı budur; kesme ve hata bağlamları da hesaba katılmalı. |
| Ekran yenilemesi | 2–5 Hz, 500–200 ms güncelleme aralığıdır. | 60 Hz panel taramasıyla çelişmez; çizim ve tarama hızları farklıdır. |

Ana SRAM adresleri [ST cihaz başlığında](https://github.com/STMicroelectronics/cmsis-device-u5/blob/main/Include/stm32u5a9xx.h), SRAM4/BKPSRAM ve ECC kapasitesi [DS13543 §3.5’te](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf) belgeleniyor.

**Gözetim ve diğer süreler:**

- `t=(reload+1)×bölücü/f_LSI` ile nominal 32 kHz’te alt sınır `1×4/32000 = 125 µs`, üst sınır `4096×1024/32000 = 131,072 s`. Bölümdeki düzeltilmiş aralık doğru. [ST IWDG sürücüsü](https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/2552682ee5a0d6f826a6435750fc9267be65868b/Src/stm32u5xx_hal_iwdg.c).
- Nominal 100 ms için örneğin bölücü 4, reload 799; 2 s için bölücü 16, reload 3999 kullanılabilir. Gerçek süre seçilen LSI yolu ve toleransına bağlıdır.
- 100 ms, 400 Hz’te **40**, 200 Hz’te **20** periyottur. 10 ms ise sırasıyla **4** ve **2** periyottur. 100 Hz duvar çağrısının aralığı **10 ms**’dir.
- Ana döngü sürerken bir alt işin ilerlemesi durursa, 50 ms sağlık penceresi ve 100 ms IWDG yaklaşık **150 ms’ye kadar** toplam gecikme oluşturabilir. Ana döngünün tamamen kilitlenmesinde ise besleme hemen kesilir.
- BOR0’ın düşen eşikleri 1,58–1,69 V; yükselen eşikleri 1,60–1,71 V’tur. 00 Tablo 11.19’a göre 1,8 V beslemeyle seçilebilir seviyeler arasında BOR0 uyumludur; BOR1’in en düşük düşen eşiği 1,90 V bile beslemenin üzerindedir. Bu hesap, gerçek BOR güvenlik payı ölçümü değildir.
- K3: `100×2×2=400` numaralandırma denemesi. K4: `256×1000×1800=460.800.000 B/yön`. K7: bir saatte **1.440.000 kontrol adımı**. p99,9 eşiği tek başına bu adımların tamamının son tarihe yetişmesini kanıtlamaz; kaçırma sayısının ayrıca sıfır olması anlamlıdır.

Ölçülmemiş süre, yük ve kapasite hedefleri genel olarak **tahmin/hedef** diye işaretlenmiş. Sorun ağırlıkla etikette değil, bu girdilerden çıkarılan yeterlilik ve en kötü durum sonuçlarının kapsamındadır.

## İskelete ya da diğer bölümlere notlar

**Kapsam ve yapı:** Sekiz ana başlık mevcut. A1 bağlantı, A4 çekirdek portu olarak doğru numaralandırılmış; A2/A3/A5/A6/A7’nin kartta uygulanmaması kapsam eksikliği değildir. Kestirim, kontrol ve atış algoritmalarının içi ilgili bölümlere bırakılmış. Fiziksel çıkışların mevcut kapsamda bulunmaması ve reset güvenliğinin mesaj/PC tarafıyla birlikte ele alınması doğru.

**İskelet 0.3 uyumu:** §3.2’nin olay kimliği ve süreli sürekli komut şartı karşılanmıyor. §3.1/§4’ün alım zamanı ve iki ölçütlü tazelik şartı tamamlanmamış; bağımsız zaman çerçevesi yok. Ortak SI birimleri, NED/FRD, kuaterniyon düzeni ve dönüşüm sahipliği A4 eşlemesinde açıkça korunmalı. 02’nin ham Ping360 alanlarının birim istisnaları da sözleşmede çözümlenmeli.

**Komşu bölümler:**

| Belge | Birlikte çözülmesi gereken konu |
|---|---|
| `docs/mimari/02-ortak-cekirdek.md` | `alive_ok`, IMU’suz olay girdisi, alım zamanı, güncel atış izni, olay kimliği, profil sahipliği ve `BAD_INPUT` davranışı. |
| `docs/mimari/06-protokol-ve-hil.md` | 06’nın seviye/yükselen kenar atış modeli iskelet §3.2’ye uymuyor. Zaman çerçevesi ve alım zamanı eksikliği burada da var. Ayrıca dönüştürücü sahipliği, taşma politikası, oturum temizliği, PWM dönüşümü ve bariyer bütçesi eşlenmeli. |
| `docs/mimari/04-simulator-cekirdegi.md` | `ActuatorFrame` kimlikli olay ve süreli komutu desteklemeli. Dosyada eski 1 ms ifadeleri ve sonlu süreli fırlatma modeli bulunuyor; iskelet 0.3’e güncellenmeli. Bunlar 01’in sorumluluğu değil, bütünleme engelidir. |
| `docs/mimari/inceleme/02-sol.md` | Önceki incelemenin duvar çağrısı, tazelik, güncel izin, tetik tekrarı ve tampon ömrü bulguları burada da devam ediyor. Yalnız tetik sıfırlama önerisi bunları kapatmıyor. |
| `docs/mimari/inceleme/04-sol.md` | Uygulanan fizik adımı, zaman aşımı ve düşürülen veri kararlarının kaydı gereği korunmalı; yalnız A1 mesaj içeriği tekrar üretim için yeterli sayılmamalı. |

**00 düzeltmelerinin madde bazında durumu:**

| Düzeltme | 01’e aktarım durumu |
|---|---|
| 01.1 | Doğru: SMPS zorunlu sayılmıyor; LDO/Range 1/160 MHz koşulu korunmuş. |
| 01.2 | Kısmen: kapasitenin boyut/hıza bağlı olduğu doğru; örnekten zorunlu ≥2 Mbaud sonucu çıkmıyor. |
| 01.3 | Doğru: 15 MHz VCP, yaklaşık Mbaud olarak ayrılmış; DK başarımı ölçüme bırakılmış. |
| 01.4 | Eski yanlış kapasite/pin sayıları tekrarlanmıyor. Gerçek sensör pin planı mevcut kapsam dışında. |
| 01.5 | Eski “sink=device-only” çıkarımı tekrarlanmıyor. |
| 01.6 | Eski genel giriş toleransı yasağı tekrarlanmıyor; ölçüm GPIO’sunun 1,8 V çıkış seviyesi doğru kullanılmış. |
| 01.7 | Rust çözümü taşınmamış; PHY zorunluluğu kanıtlanmış sayılmamış. |
| 01.8 | Doğru: 160 MHz açık yapılandırılıyor. Varsayılan 4 MHz, [ST sistem başlangıç kaynağıyla](https://github.com/STMicroelectronics/cmsis-device-u5/blob/main/Source/Templates/system_stm32u5xx.c) da uyumlu. |
| 08.1 | Yanlış “yalnız iki cihaz örneği” iddiası tekrarlanmıyor. |
| 08.2 | Doğru: referans saat seçimi ile üç tuning işlevi ayrılmış; +%5,9/+%15 zorunlu sayılmamış. |
| 08.3 | Açıklama doğru; hazırlıksız imajın `TZEN=1` durumunda kendi güvenli beklemesine ulaşacağı garanti edilmemeli. Salt okunur yükleme öncesi kontrol esas olmalı. |
| 08.4 | Doğru: ST çatalının kapsamı genellenmemiş; upstream MIT tercih olarak sunulmuş. |
| 08.5 | Zephyr’in destek yokluğu iddiası tekrarlanmıyor. |
| 08.6 | Doğru: `CMakePresets.json`, dosya sahipliği ve CubeMX tercihi ayrılmış. |
| 08.7 | Doğru: libc açık seçilip sabitleniyor; Picolibc varsayılanı korunmuş. |
| 08.8 | Konu bu bölümün kapsamında kullanılmıyor; eski backend iddiası tekrarlanmıyor. |
| 08.9 | Doğru: 1,8 V LDO yolu gereksiz belirsizlik olarak bırakılmamış. |
| 08.10 | Doğru: MAVLink özel dialect dışlanmıyor; mesaj ayrıntıları 06’ya bırakılıyor. |
| 08.11 | Tel üzerinde ham struct önerilmiyor. Kalıcı parametre biçimi ayrıca tamamlanmalı. |
| 08.12 | Kısmen: gerçek USB ISR sorumluluğu ve ana döngü bağlamı doğru; yanıt bütçesi, taşma ve bayatlık davranışları eksik. |
| 08.13 | Kısmen: tanımsız wrapper yöntemi doğru; RTOS ve özel havuz yolları tamamlanmalı. |
| 08.14 | Doğru: FP politikası eşitlik garantisi sayılmıyor. |
| 08.15 | Doğru: küçük BSP işi veya %2–4 yük kapasite kanıtı olarak kullanılmıyor. |
| 13.9 | Kısmen: sürücü sınırı korunmuş; “yalnız BSP değişir” ifadesi garanti yerine tasarım hedefi olmalı. |
| 13.17 | Doğru: mesaj HIL’i fiziksel PWM/GPIO testiyle eşitlenmiyor. |
| 13.18 | Eski bütün girişleri kapsayan seviye genellemesi tekrarlanmıyor. |
| 13.20 | UART kapasite hesabı doğru; diğer sensör/ESC hesapları bu bölümün kapsamında kullanılmıyor. |

Ç1, Ç3, Ç4, Ç7 ve Ç8’in temel uyarıları korunmuş. M1–M5, M6, M9, M10, M17, M19 ve M21’e başvurular genel olarak uygun; ancak ölçüm gereği, tanımsız bir emniyet davranışının yerine geçmemeli. Öncelikle iskeletin değiştirilmesi gerekmiyor; 01, 02 ve 06 mevcut bağlayıcı kurallara uydurulmalı.

Hiçbir dosya değiştirilmedi, hiçbir şey kurulmadı; derleme veya donanım ölçümü yapılmadı.