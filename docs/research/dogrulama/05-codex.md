# Codex doğrulaması: 05-sensor-modelleme

## Hüküm tablosu

Raporun tamamı okundu. Dosya değiştirilmedi, kurulum yapılmadı. 1–10 numaraları raporun Bölüm 5’ine; E numaraları ek kontrollere karşılık gelir. Bir kaynaktaki kodun doğru aktarılması, modelin fiziksel olarak doğru olduğu anlamına gelmez.

| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
|---|---|---|---|---|
| 1a | Ping360: 750 kHz, 2°×25°, 0.75–50 m, %0.08 çözünürlük | DOĞRU | [Üretici özellikleri](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/) | 50 m’de 4.1 cm de belirtiliyor. |
| 1b | Ping360: 50 m’de 360° tarama 33 s; Ethernet ile 1 m’de 3.42 s | DOĞRU | [Üretici tarama tablosu](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/) | 50 m için üç arayüzde de 33 s yazıyor. 1 m’de USB 4.17 s, RS-485 4.26 s. |
| 1c | Ping360 açı ve örnekleme alanları | DOĞRU | [Ping360 mesajları](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) | 400 açısal konum, 0.9° adım; `sample_period` 80–40000 adet 25 ns; 200–1200 örnek. |
| 2 | Ping Protocol çerçevesi ve Ping1D #1211 | DOĞRU | [Çerçeve](https://docs.bluerobotics.com/ping-protocol/), [Ping1D](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping1d/#1211-distance_simple) | `BR`, little-endian alanlar, checksum dışındaki baytların u16 toplamı; #1211 yükü u32 mm ve u8 güven yüzdesi. Ping checksum’u CRC değildir. |
| 3a | A50 güncellemesi 2–15 Hz; belirtilen mod tablosu | DOĞRU | [Water Linked range mode](https://docs.waterlinked.com/dvl/range-mode/) | Beş dip takibi satırı raporla aynı. Örtüşen irtifa aralıkları nedeniyle hız yalnız irtifanın tek değerli fonksiyonu değildir. |
| 3b | `velocity_valid` ve `beam_valid` anlamları | DOĞRU | [TCP JSON API](https://docs.waterlinked.com/dvl/dvl-json-protocol/) | Geçerli hız ve yansıtıcı yüzey kilidi ilişkisi belgelenmiş. Su takibinde referans dip değildir. |
| 3c | A50 seri: 115200 8N1, `wrz`, CRC-8 | DOĞRU | [Seri protokol](https://docs.waterlinked.com/dvl/dvl-serial-protocol/) | Varsayılan hızdır; değiştirilebilir. CRC: poly 0x07, init/xorout 0, yansıtma yok. |
| 3d | TCP JSON gecikmesi ortalama 4 ms, σ=2 ms, maksimum 13 ms | KISMEN DOĞRU | [Üreticinin ölçümü](https://docs.waterlinked.com/dvl/dvl-json-protocol/) | Sayılar doğru; gözlenen maksimumdur, gecikme garantisi değildir. |
| 4 | ICM-42688-P gürültü, ofset ve sıcaklık değerleri; bias kararsızlığının bulunmaması | DOĞRU | [TDK DS-000347 rev 1.6](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000347-icm-42688-p-v1.6.pdf) | Gyro 0.0028 °/s/√Hz, ±0.5 °/s, ±0.005 °/s/°C, ±%1.25; accel 65/70 µg/√Hz, ±20 mg, ±0.15 mg/°C doğrulanıyor. İlgili tablo değerlerinin tipik değer ve koşulları korunmalı. Bias kararsızlığı spesifikasyonu bulunmadı. |
| 5 | OceanSim/HoloOcean FPS, önbellek ve gürültü karşılaştırması | DOĞRU | [OceanSim, Tablo II ve denklemler 3–6](https://arxiv.org/html/2503.01074v2) | OceanSim 31.7/30.3/28.2 FPS; HoloOcean 3.8/3.3/1.5 FPS; önbellek 417.8/317.0/502.1 s. RTX A6000 kullanılmış. |
| 6 | Choi 2021: 512 ışında 0.5/3/10 Hz; karmaşık Gauss ve sinc düzeltmesi | DOĞRU | [Yayıncı PDF’si, Tablo 1–2, §2.3–2.4](https://public-pages-files-2025.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2021.706646/pdf) | 60 m/114 ray: 0.5 Hz; 60 m/11 ray: 3 Hz; 10 m/11 ray: 10 Hz. İşlemci ve GPU da raporla aynı. |
| 7 | Stonefish FLS render, histogram ve gürültü yöntemi | DOĞRU | [Fragment shader](https://raw.githubusercontent.com/patrykcieslak/stonefish/master/Library/shaders/sonarInput.frag), [compute shader](https://raw.githubusercontent.com/patrykcieslak/stonefish/master/Library/shaders/flsOutput.comp) | Menzil ve Lambert katsayısı yazılıyor; dikey örnekler kutulanıyor. Işın başına Gauss çarpan, kutu başına Gauss toplamsal gürültü var. |
| 8a | GNSS sinyali en fazla yaklaşık 3 cm suya nüfuz eder | KISMEN DOĞRU | [Özgün GNSS deneyi, §3.5](https://link.springer.com/article/10.1007/s00190-018-1147-x) | Tatlı su ve belirli jeodezik alıcı/anten için yaklaşık 30–35 mm gözlenmiş. Evrensel kesilme derinliği değildir. |
| 8b | NEO-M9N: cold 24 s, hot 2 s, 2 m CEP | DOĞRU | [u-blox veri sayfası R08, Tablo 1](https://content.u-blox.com/sites/default/files/NEO-M9N-00B_DataSheet_UBX-19014285.pdf) | 24 s dört takımyıldızlı yapılandırmaya ait. Başlangıç süreleri belirli test koşullarında tipiktir. Raporda adı geçen R02 yerine erişilen resmi R08 okundu. |
| 9a | PX4 GPS kodunda 60 sabiti, 120 ms ve 5 Hz bulunması | DOĞRU | [GPS başlık dosyası](https://raw.githubusercontent.com/PX4/PX4-SITL_gazebo-classic/main/include/gazebo_gps_plugin.h) | Gürültü ve random-walk sayıları da doğru aktarılmış. |
| 9b | Bu GPS uygulamasının fiziksel korelasyon zamanı 60 s olan doğru Gauss–Markov modeli olması | YANLIŞ | [GPS güncelleme kodu](https://raw.githubusercontent.com/PX4/PX4-SITL_gazebo-classic/main/src/gazebo_gps_plugin.cpp) | Bias sönümleme teriminde `dt` yok; rastgele terim de ek `dt` ile çarpılıyor. |
| 9c | PX4 IMU varsayılanları ve bias korelasyon süreleri | DOĞRU | [IMU başlık dosyası](https://raw.githubusercontent.com/PX4/PX4-SITL_gazebo-classic/main/include/gazebo_imu_plugin.h) | 8.726646e−4, 0.00637, 1000 s ve 300 s doğrulanıyor. Random walk ve açılış bias sigma varsayılanları sıfır. |
| 10a | Kartta yerleşik Ethernet bulunmaması; USB-C HS/sink düzeni | DOĞRU | [ST ürün sayfası](https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html), [UM2967 §8.2](https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf) | Belgelenen kart düzeni USB device kullanımı içindir. Sink, güç rolünü belirtir. |
| 10b | Ethernet sensörlerini bu kartla kablo seviyesinde emüle etmek imkânsızdır | YANLIŞ | [SPI erişimi: UM2967](https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf), [W5500 üretici belgesi](https://docs.wiznet.io/Product/Chip/Ethernet/W5500) | Ek Ethernet denetleyicisiyle mümkündür; ek donanımsız doğrudan bağlantı yoktur. |
| E1 | 115200 baud hesabı gerçek Ping360 tarama hızlarını açıklar | KISMEN DOĞRU | [Ping Viewer baud listesi](https://raw.githubusercontent.com/bluerobotics/ping-viewer/master/src/sensor/ping360.h) | Aritmetik doğru, genelleme yanıltıcı. Yazılım 2 Mbaud dahil farklı hızları destekliyor. |
| E2 | OceanSim sonucu “512 ışın için yaklaşık 30 FPS” kanıtıdır | DOĞRULANAMADI | [OceanSim makalesi](https://arxiv.org/html/2503.01074v2) | Benchmarkın 512 ışınla yapıldığı doğrulanamadı. Şekil 6 örneği 350 menzil ×220 azimut kutusu kullanıyor. |
| E3 | DVL `time_of_validity` üzerinden teslim gecikmesi 2r/c’dir | YANLIŞ | [Zaman tanımları](https://docs.waterlinked.com/dvl/dvl-json-protocol/) | Damga yansıma merkezine aittir; buradan alıcıya kalan akustik yol yaklaşık r/c’dir. |
| E4 | Kalibr’in 10× önerisi sensör simülasyon parametrelerine doğrudan uygulanmalıdır | KISMEN DOĞRU | [Kalibr açıklaması](https://raw.githubusercontent.com/wiki/ethz-asl/kalibr/IMU-Noise-Model.md) | Kalibr kestirimindeki eksik hata bileşenlerini telafi etme önerisidir; gerçek sensörün bias RW değeri değildir. |
| E5 | MS5837 OSR 8192: 18.08 ms ve 0.2 mbar RMS | DOĞRU | [TE veri sayfası, s.2–3](https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FMS5837-30BA%7FB1%7Fpdf%7FEnglish%7FENG_DS_MS5837-30BA_B1.pdf%7FCAT-BLPS0017) | 18.08 ms maksimum tek dönüşüm süresidir. İki dönüşümün salt süre sınırı yaklaşık 27.7 Hz; 25 Hz temkinli çalışma seçimidir. |
| E6 | HoloOcean 2.0 tüm sonarları octree’den ray casting’e geçirmiştir | KISMEN DOĞRU | [HoloOcean 2.0 önizlemesi, §III-B](https://arxiv.org/html/2510.06160v1) | Kaynakta tek ışınlı deney gerçekleştirilmiş; diğer sonar türlerine geçiş plan olarak açıklanıyor. |
| E7 | USB HS CDC/bulk yolu deterministik teslim sağlar | YANLIŞ | [Microsoft USB bulk belgesi](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-bulk-and-interrupt-transfer) | Bulk aktarım için bant genişliği ve teslim süresi rezervasyonu yoktur. |
| E8 | Ainslie–McColm formülü ve tatlı suda 0.13 dB/m fiziksel doğruluğu | DOĞRULANAMADI | [NPL soğurma sayfası](https://resource.npl.co.uk/acoustics/techguides/seaabsorption/physics.html) | Sayfa açılışları zaman aşımı/502 verdi. Rapordaki formülün aritmetiği yaklaşık 0.131 dB/m üretiyor; bu, formülün veya tatlı su uygulamasının doğrulanması değildir. |
| E9 | Kamera için verilen iki katsayılı RGB-D formülü OceanSim’de uygulanıyor | DOĞRU | [OceanSim §III-A1](https://arxiv.org/html/2503.01074v2) | Uygulama doğrulanıyor; her ortamda yeterli olacağı ve maliyetinin ihmal edilebilirliği ayrıca ölçülmeli. |

## Yanlış ya da yanıltıcı bulunanlar

1. **PX4 GNSS modelinin fiziksel yorumu.** Rapordaki bias denklemi kaynak kodu doğru yansıtıyor; fakat “τ=60 s Gauss–Markov” yorumu doğru değil. Kodun sönümleme katsayısı her fizik adımında `1−1/60`; dolayısıyla saniye cinsinden davranış adım süresine bağlı. Fiziksel bir model için örneğin
   \[
   b_{k+1}=e^{-\Delta t/\tau}b_k+
   \sigma_\infty\sqrt{1-e^{-2\Delta t/\tau}}\,\xi_k
   \]
   kullanılabilir. Burada \(\sigma_\infty\) durağan bias standart sapmasıdır. Bu düzeltme koddan ve denklemlerden yapılan matematiksel değerlendirmedir. [PX4 kaynak kodu](https://raw.githubusercontent.com/PX4/PX4-SITL_gazebo-classic/main/src/gazebo_gps_plugin.cpp)

2. **Ping360 bağlantı hızı ile tarama süresinin ilişkilendirilmesi.** 1200 baytın 115200 baud/8N1 ile yaklaşık 104 ms sürmesi doğru. Ancak 400 böyle profil yalnız iletimde en az **41.7 saniye** gerektirir; paket ek yükü buna dahil değildir. Dolayısıyla bu yapılandırma 33 saniyelik taramayı üretemez. Gerçek zamanlama modeli baud, örnek sayısı, açı adımı, motor hareketi ve ping süresini birlikte kullanmalı. [Baud seçenekleri](https://raw.githubusercontent.com/bluerobotics/ping-viewer/master/src/sensor/ping360.h), [üretici süreleri](https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/)

3. **DVL zaman damgasına çift yönlü uçuş süresinin tekrar eklenmesi.** Raporda `time_of_validity = ping zamanı` denip teslim için 2r/c öneriliyor. Üreticinin tanımı yansıma anıdır. Yaklaşık ilişki, bu damgadan itibaren `r/c + işleme + iletim` olmalıdır. Akustik gönderim anından başlanıyorsa toplam yol 2r/c olur. [Water Linked zaman tanımları](https://docs.waterlinked.com/dvl/dvl-json-protocol/)

4. **GNSS için kesin 2–3 cm kapısı.** Makale tatlı suda 30–35 mm’ye kadar bazı sinyallerin izlendiğini, izleme sınırının alıcı eşiğine ve uydu açısına bağlı olduğunu söylüyor. “En fazla 3 cm” fiziksel bir duvar değildir. Model anten üzerindeki su kalınlığına bağlı bozulma ve kayıp olasılığı içermeli. [Özgün deney](https://link.springer.com/article/10.1007/s00190-018-1147-x)

5. **Hot-start süresinin yüzeye çıkış süresiyle özdeşleştirilmesi.** NEO-M9N’in 2 saniyesi doğrulanıyor; ancak komutla başlatılan, uyduların −130 dBm olduğu oda sıcaklığındaki test içindir. Her dalıştan sonra iki saniyede fix garantisi değildir. Yeniden edinim; alıcı durumu, saklanan veriler ve sinyal koşullarıyla ayrı modellenmeli. [u-blox test koşulları](https://content.u-blox.com/sites/default/files/NEO-M9N-00B_DataSheet_UBX-19014285.pdf)

6. **Performans rakamlarından platform garantisi çıkarılması.** OceanSim ve Choi sayıları doğru; eşdeğer iş yükleriyle yapılmış bir karşılaştırma değiller. OceanSim tablosu ayrıca 512 ışın iddiasını doğrulamıyor. Isaac Sim’in desteklenen işletim sistemleri arasında macOS bulunmuyor; bu sonuç Rust/wgpu uygulamasına doğrudan taşınamaz. [OceanSim](https://arxiv.org/html/2503.01074v2), [NVIDIA sistem gereksinimleri](https://docs.isaacsim.omniverse.nvidia.com/latest/installation/requirements.html)

7. **USB aktarımında determinizm.** Tohumlanmış ölçüm üretimi deterministik olabilir; USB bulk teslimi değildir. Ayrıca 1 kHz veriyi sekiz örnek biriktirerek göndermek, ilk örneğe yaklaşık 7 ms ek bekleme getirir. Zaman damgası bu beklemeyi kapalı kontrol çevriminden kaldırmaz. [USB bulk zamanlaması](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-bulk-and-interrupt-transfer)

8. **Ethernet emülasyonunun imkânsız sayılması.** Kartın üzerinde Ethernet olmaması doğru. Bunun sonucu ek donanım gereksinimidir. SPI üzerinden MAC/PHY ve TCP/IP sağlayan bir denetleyici kullanılabilir; bu seçeneğin bant genişliği ayrıca değerlendirilir. [W5500 üretici belgesi](https://docs.wiznet.io/Product/Chip/Ethernet/W5500)

9. **Kalibr’deki 10× katsayısının kullanım amacı.** Kaynak, basitleştirilmiş kestirim modelinin sıcaklık ve ölçek hatalarını içermediği durumda gürültü parametrelerini büyütmeyi öneriyor. Raporda bu etkiler zaten açıkça modelleniyor. Üstüne otomatik 10× uygulamak aynı hataları iki kez temsil edebilir; ölçülmemiş bias random walk’ı da belirlemez. [Kalibr](https://raw.githubusercontent.com/wiki/ethz-asl/kalibr/IMU-Noise-Model.md)

## Rapor dışı ek bulgular

- **UART bağlantısında gerilim seviyesi eksik.** Kartın VDD/VDD_MCU düzeni 1.8 V’tur. USB-UART adaptörü seçimi ve iki yönlü lojik eşikleri pin bazında doğrulanmalı; sıradan 3.3 V bağlantı varsayılmamalı. USART3 TX/RX, CN9’da 26/24; LPUART1 TX/RX, CN10’da 36/38 numaralı pinlerde belgelenmiş. Pin paylaşımı ve köprü seçenekleri de kontrol edilmeli. [UM2967](https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf)

- **Ping360 emülatörü yalnız #2601/#2300 ile sınırlanmamalı.** v1.1.0 ile gelen `auto_transmit` #2602 ve `auto_device_data` #2301 akışı bulunuyor. Hedef sürücü bu yolu kullanıyorsa eski istek/yanıt çiftini uygulamak yeterli olmaz. [Ping360 protokolü](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/)

- **DVL kodlayıcısında bit düzeyinde ayrıntılar gerekli.** CRC, `*` öncesindeki metin üzerinde hesaplanıyor; iki küçük harfli hex karakterle gönderiliyor. Cihaz yanıtı CRLF ile bitiyor. `wru` içinde ayrı geçerlilik bayrağı yok; çözülemeyen ışında mesafe −1, hız 0 kullanılıyor. [Seri protokol](https://docs.waterlinked.com/dvl/dvl-serial-protocol/)

- **Su takibi farklı bir fiziksel ölçümdür.** A50, yaklaşık 1.5–4.5 m aşağıdaki su kolonuna göre hız ölçer. Akıntı alanı modele girmeli; sığ havuzda dip yansımaları suya göre hızı bozabilir. [Water tracking](https://docs.waterlinked.com/dvl/water-tracking/)

- **IMU gürültüsü ODR kadar filtreye de bağlıdır.** Kalibr’in \(\sigma/\sqrt{\Delta t}\) ölçeklemesi ideal alçak geçiren filtreleme varsayar. Filtrelenmiş sensör çıktısını yalnız daha seyrek örneklemek aynı işlemi sağlamaz. Sensör yapılandırmasına bant genişliği ve filtre gecikmesi eklenmeli. [Kalibr filtre koşulu](https://raw.githubusercontent.com/wiki/ethz-asl/kalibr/IMU-Noise-Model.md)

- **Bar30’un sığ su doğruluğu ve kullanım koşulu daha ayrıntılı.** TE tablosu 0–45 °C’de 0–6 bar için ±50 mbar, tam 0–30 bar aralığı için ±200 mbar veriyor. 2 mm çözünürlük, 2 mm doğruluk değildir. Ayrıca ıslak/kuru çevrim koşulu mevcut. [TE veri sayfası](https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FMS5837-30BA%7FB1%7Fpdf%7FEnglish%7FENG_DS_MS5837-30BA_B1.pdf%7FCAT-BLPS0017)

- **Akustik modelde birim sözleşmesi gerekli.** Rapor, dB/km katsayısı ile `exp(−2αr)` biçimini dönüşümü açıklamadan kullanıyor. \(\alpha\) dB/m ise iki yönlü soğurma çarpanı yoğunlukta \(10^{-2\alpha r/10}\), basınç genliğinde \(10^{-2\alpha r/20}\) olur. Genlik ve yoğunluk hesabı ayrılmalı. Choi modeli de önce karmaşık basınç üzerinden ilerliyor. [Choi §2.3](https://public-pages-files-2025.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2021.706646/pdf)

- **Erişim ve doğrulama sınırı:** NPL’nin ses hızı/soğurma sayfaları açılırken zaman aşımı veya 502 verdi; arama özetlerini doğrulama saymadım. [Akkaynak–Treibitz yayın sayfası ve PDF’si](https://openaccess.thecvf.com/content_cvpr_2018/html/Akkaynak_A_Revised_Underwater_CVPR_2018_paper.html) 403 verdi. Kamera formülünün OceanSim uygulaması okundu; özgün CVPR makalesi bu oturumda doğrulanmadı. Rapordaki 6 mm deniz suyu deri derinliği, evrensel “50 m altında kırılma ihmal edilir” eşiği ve T1’in tek çekirdek performansı da doğrulanmış kabul edilmemeli.

## Raporlar arası çelişki değerlendirmesi

Başka bir raporla karşılaştırma istenmedi; raporlar arası hüküm verilmedi.

Bu raporun kendi içinde iki önemli tutarsızlık var: 115200 baud/1200 örnek hesabı 33 saniyelik tam taramayla aynı yapılandırmada uyuşmuyor; DVL için doğru verilen yansıma zamanı tanımı, sonraki gecikme önerisinde gönderim zamanı gibi kullanılıyor.

## Mimari için net sonuçlar

1. Ölçüm modeli, arıza zinciri ve protokol kodlayıcısının ayrılması uygun; önerilen çekirdek mimari korunabilir.
2. Ping360’ı açı başına zamanlanan sensör olarak uygulayın; tarama süresini sabit 33 saniyeye bağlamayın.
3. Ping checksum’u ile DVL CRC-8’ini ayrı uygulayın; hedef firmware/protokol sürümünü sabitleyin.
4. DVL’de akustik gönderim, yansıma, rapor gönderimi ve teslim zamanlarını ayırın.
5. GNSS kaybını ayarlanabilir geçiş olarak modelleyin; 3 cm ve 2 saniyeyi garanti saymayın.
6. PX4 GPS bias kodunu kopyalamayın; zaman adımından bağımsız Gauss–Markov modeli kullanın.
7. USB HS HIL uygundur; gecikme/jitter bütçesi ölçülmeli ve UART bağlantıları 1.8 V koşullarına göre tasarlanmalı.
8. macOS/Windows sonar performansı için kendi sahnenizde benchmark yapın; OceanSim/Choi rakamlarını kapasite garantisi saymayın.