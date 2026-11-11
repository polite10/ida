# Codex doğrulaması: 08-kart-c-yigini

## Hüküm tablosu

| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
|---|---|---|---|---|
| 1 | §5.1: CubeU5 v1.9.0’da CDC-ACM cihaz örneği yalnız iki kartta; DK’da üç host örneği var | YANLIŞ | [v1.9.0 tam kaynak ağacı](https://api.github.com/repos/STMicroelectronics/STM32CubeU5/git/trees/v1.9.0?recursive=1) | Cihaz örneği **dört** kartta var: NUCLEO-U545RE-Q, NUCLEO-U575ZI-Q, NUCLEO-U5A5ZJ-Q, STM32U5G9J-DK2. DK hakkındaki bölüm doğru. |
| 2 | §5.2: Nucleo örneği HS, gömülü PHY ve kapalı VBUS algılama kullanıyor; eşik ayarlarını çağırmıyor | DOĞRU | [main.c](https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/NUCLEO-U5A5ZJ-Q/Applications/USBX/Ux_Device_CDC_ACM/Core/Src/main.c), [MSP](https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/NUCLEO-U5A5ZJ-Q/Applications/USBX/Ux_Device_CDC_ACM/Core/Src/stm32u5xx_hal_msp.c) | Üç tuning işlevi çağrılmıyor. Ancak `HAL_SYSCFG_SetOTGPHYReferenceClockSelection` çağrılıyor; “SetOTGPHY* hiç çağrılmıyor” biçiminde okunmamalı. |
| 3 | §5.3: HAL’da PHY eşik/preemphasis işlevleri ve +%5,9 / +%15 önerileri var | DOĞRU | [Cube v1.9.0’ın sabitlediği HAL başlığı](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx_hal_driver/2552682ee5a0d6f826a6435750fc9267be65868b/Inc/stm32u5xx_hal.h), [uygulaması](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx_hal_driver/2552682ee5a0d6f826a6435750fc9267be65868b/Src/stm32u5xx_hal.c) | Değerler gerçekten “recommended” olarak işaretlenmiş. Bunun DK’da zorunluluğu veya kararlılık etkisi kanıtlanmış değil. |
| 4 | §5.4: Eclipse ThreadX MIT; ST çatalı Azure RTOS lisanslı ve listelenen donanımla sınırlı | DOĞRU | [Eclipse lisansı](https://github.com/eclipse-threadx/threadx/blob/master/LICENSE.txt), [ST lisansı](https://github.com/STMicroelectronics/stm32-mw-threadx/blob/main/LICENSE.md), [donanım listesi](https://github.com/STMicroelectronics/stm32-mw-threadx/blob/main/LICENSED-HARDWARE.txt) | STM32U5 listede. Her kart değişiminin lisansı geçersiz kılacağı sonucu yanlış. |
| 5 | §5.5: DK YAML listesindeki can/pwm/usb eksik; Nucleo listesinde can/usb_device var | DOĞRU | [DK YAML](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/stm32u5a9j_dk.yaml), [Nucleo YAML](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/nucleo_u5a5zj_q/nucleo_u5a5zj_q.yaml) | Dosya içeriği doğru aktarılmış; bu liste destek yokluğunun yeterli kanıtı değil. |
| 6 | §5.6: TinyUSB 0.21.0’da U5A9J-DK BSP’si yok; U5A5 BSP’sinde belirtilen PHY/SMPS/VBUS kodu var | DOĞRU | [Sürüme sabitlenmiş BSP listesi](https://api.github.com/repos/hathach/tinyusb/contents/hw/bsp/stm32u5/boards?ref=0.21.0), [board.h](https://github.com/hathach/tinyusb/blob/0.21.0/hw/bsp/stm32u5/boards/stm32u5a5nucleo/board.h), [family.c](https://github.com/hathach/tinyusb/blob/0.21.0/hw/bsp/stm32u5/family.c) | Kaynak kod desteği var; DK’da çalıştığına ilişkin donanım doğrulaması yok. |
| 7 | §5.7: CubeMX CMake alt proje sorunu, yeniden üretimde ezilme ve preset değişikliği | KISMEN DOĞRU | [6.14 sorunu ve ST yanıtı](https://community.st.com/stm32cubemx-mcus-29/cubemx-6-14-xx-breaks-cmake-workflows-151130), [6.15 yanıtı](https://community.st.com/stm32cubemx-mcus-29/cmake-project-generated-by-stm32cubemx-6-15-build-fails-says-compiler-broken-154417), [ST CMake belgesi](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/1.0.0/en/docs/markup/basic_concepts/cmake.html) | Tarihsel sorun gerçek. Preset dosyası **CMakePresets.json**; üst düzey CMakeLists korunuyor. |
| 8 | §5.8: DK ve Nucleo örneklerinin PLL, Scale 1 ve SMPS ayarları | DOĞRU | [DK main.c](https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/Core/Src/main.c), [Nucleo main.c](https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/NUCLEO-U5A5ZJ-Q/Applications/USBX/Ux_Device_CDC_ACM/Core/Src/main.c) | İki PLL kombinasyonu da belirtilen 16 MHz kaynakla 160 MHz verir. |
| 9 | §5.9: SymForce yalnız C++ ve Python belgeliyor; C çıktısı yok | KISMEN DOĞRU | [Resmî backend listesi](https://symforce.org/api/symforce.codegen.html) | Hazır C backend’i listelenmiyor. Ancak CUDA, PyTorch, Rust ve TypeScript de belgelenmiş; “yalnız iki dil” yanlış. |
| 10 | §5.10: TZEN varsayılan 0; açma ve kapama RDP regresyonu gerektiriyor | KISMEN DOĞRU | [AN5347](https://www.st.com/content/st_com/en/technical-documents/an5347-introduction-to-arm-trustzone-features-on-stm32l5-stm32u5-and-stm32u3-mcus-stmicroelectronics_asset/chunk-004-topic-an5347-security-tz-enabled.html) | Varsayılan doğru. Regresyon gerekliliği **devre dışı bırakma** için; etkinleştirme TZEN ayarlanarak yapılıyor. |
| 11 | 160 MHz için SMPS zorunlu değil | DOĞRU | [DS13543, Tablo 38–39](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf) | LDO ile 160 MHz belgelenmiş; 1,8 V konusundaki R11 belirsizliği de gideriliyor. |
| 12 | Zephyr’de DK için CAN/PWM/USB desteği yokmuş gibi değerlendirme yapılabilir | YANLIŞ | [Güncel kart belgesi](https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html), [kart DTS’si](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/stm32u5a9j_dk.dts) | PWM ve USB/PHY etkin. CAN sürücü/düğümü mevcut fakat varsayılan CAN yapılandırması ayrıca tamamlanmalı. |
| 13 | TinyUSB güncel sürümü 0.21.0; U5 için dwc2 ve gömülü HS PHY desteği mevcut | DOĞRU | [Güncel sürüm](https://api.github.com/repos/hathach/tinyusb/releases/latest), [0.21.0 dwc2 kaynağı](https://github.com/hathach/tinyusb/blob/0.21.0/src/portable/synopsys/dwc2/dwc2_stm32.h) | 11 Kasım 2026 kontrolünde sürüm doğru; kaynak U59/U5A/U5F/U5G gömülü HS PHY ayrımını açıkça içeriyor. |
| 14 | RTOS’suz, timer ISR’sinde 400 Hz kontrol teknik olarak yeterli/deterministik | KISMEN DOĞRU | [TinyUSB eşzamanlılık modeli](https://docs.tinyusb.org/en/latest/reference/concurrency.html), [HAL kesme uyarısı](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx_hal_driver/2552682ee5a0d6f826a6435750fc9267be65868b/Src/stm32u5xx_hal.c) | Uygulanabilir; frekans tek başına yeterlilik kanıtı değil. Kesme, veri paylaşımı ve USB servis bütçeleri eksik. |
| 15 | USB ISR’si yalnız FIFO boşaltıp bayrak kaldırmalı | KISMEN DOĞRU | [TinyUSB dwc2 ISR](https://github.com/hathach/tinyusb/blob/0.21.0/src/portable/synopsys/dwc2/dcd_dwc2.c) | Ağır uygulama işi ertelenebilir; sürücü ISR’si reset, enumerasyon, endpoint ve aktarım olaylarını da işler. |
| 16 | DVL/sonar eksikliği nedeniyle MAVLink HIL için önerilmemeli | YANLIŞ | [Common mesajları](https://mavlink.io/en/messages/common.html), [özel mesaj tanımlama](https://mavlink.io/en/guide/define_xml_element.html) | Standart mesajların kapsamı dar gösterilmiş; özel dialect seçeneği dışlanamaz. |
| 17 | Ortak proto.h içindeki sabit struct + COBS + CRC taşınabilir ve deterministik protokol sağlar | KISMEN DOĞRU | [WG14 N1570, §6.2.6.1 ve §6.7.2.1](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) | COBS/CRC; padding, hizalama, endian ve alan gösterimini çözmez. Açık serileştirme gerekiyor. |
| 18 | IWDG ve timer kapılaması önerisi görev öğesi çıkışının emniyeti için yeterince tanımlı | KISMEN DOĞRU | [IWDG sürücüsü](https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/main/Src/stm32u5xx_hal_iwdg.c), [prescaler/reload tanımları](https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/main/Inc/stm32u5xx_hal_iwdg.h) | Yön doğru; süre bütçesi, besleme koşulları ve reset boyunca elektriksel güvenli durum eksik. |
| 19 | `--wrap=malloc` veya boş malloc tanımı dinamik belleği bağlantı hatasıyla yasaklar | KISMEN DOĞRU | [GNU ld --wrap](https://sourceware.org/binutils/docs/ld/Options.html) | Wrap yönlendirme yapar. Boş işlev tanımı kendiliğinden bağlantı hatası üretmez; bütün tahsis yollarını da kapsamaz. |
| 20 | C17 ortak çekirdek ve toleranslı sayısal karşılaştırma uygulanabilir | DOĞRU | [GCC 14.3 C standardı](https://gcc.gnu.org/onlinedocs/gcc-14.3.0/gcc/C-Dialect-Options.html), [MSVC standardı](https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170), [FP contraction](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html) | Derleyiciye özgü bayraklar ayrılmalı. FMA kapatılması bit düzeyinde eşitlik garantisi sağlamaz. |
| 21 | SymPy C99 üretimi ve CMSIS-DSP matris işlemleri mevcut | DOĞRU | [SymPy C yazıcıları](https://docs.sympy.org/latest/modules/printing.html), [CMSIS-DSP matrisleri](https://arm-software.github.io/CMSIS-DSP/latest/group__groupMatrix.html) | Önerilen C yolu mevcut. `arm_mat_init_f32` yapıyı başka bölgeye taşımaz; alanlarını başlatır. |
| 22 | Küçük BSP çalışması ve yaklaşık %2–4 kestirim yüküyle sistem kapasitesi yeterli olacak | DOĞRULANAMADI | [U5A5 BSP](https://github.com/hathach/tinyusb/blob/0.21.0/hw/bsp/stm32u5/boards/stm32u5a5nucleo/board.h) | Kod benzerliği iş miktarı veya kart performansı ölçümü değildir. Raporda da donanım denemesi yapılmamış. |

İlk on satır, Bölüm 5’in on maddesine karşılık gelir. Raporun tamamı ve ilgili önceki doğrulamalar okundu; hükümler açılıp incelenen canlı birincil kaynaklara dayanır. Donanım ölçümü yapılmadı.

## Yanlış ya da yanıltıcı bulunanlar

**1. USBX cihaz örneklerinin sayısı yanlış.**

Raporda yalnız NUCLEO-U5A5ZJ-Q ve STM32U5G9J-DK2 sayılıyor. Etiketli kaynak ağacında NUCLEO-U545RE-Q ve NUCLEO-U575ZI-Q için de aynı adlı örnek bulunuyor. Bununla birlikte **STM32U5x9J-DK altında yalnız üç host örneği bulunduğu doğru**. Eklenen iki kartın örneğinin varlığı, bunların U5A9 gömülü HS PHY’sine doğrudan referans olduğu anlamına gelmez. [Tam v1.9.0 ağacı](https://api.github.com/repos/STMicroelectronics/STM32CubeU5/git/trees/v1.9.0?recursive=1).

**2. Zephyr’in kısa YAML listesi destek matrisi olarak kullanılmış.**

Güncel kart belgesi CAN, PWM, USB OTGHS ve HS PHY’yi listeliyor. Kart DTS’sinde `pwm1`, `pwm2`, `usbotg_hs` ve `otghs_phy` etkinleştirilmiş. CAN ise ortak SoC tanımında mevcut fakat `disabled`; DK için pin/saat/overlay ve haricî transceiver işi gerekiyor.

Dolayısıyla “CAN/PWM/USB yok” gerekçesiyle Zephyr’i elemek yanıltıcıdır. Zephyr’in karmaşıklığı ayrı bir mühendislik değerlendirmesi olabilir. [Kart DTS’si](https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/stm32u5a9j_dk.dts), [CAN düğümü](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/dts/arm/st/u5/stm32u5.dtsi).

**3. CubeMX hakkında tarihsel sorun, genel zorunluluğa dönüştürülmüş.**

6.14 için `CMAKE_SOURCE_DIR` sorunu ve ST’nin açtığı biletler doğrulanıyor. Ancak bundan “CubeMX’i derleme yolundan çıkarmak gerekir” sonucu zorunlu olarak çıkmaz. ST, üst düzey `CMakeLists.txt` dosyasının kullanıcı değişikliklerini koruduğunu açıkça belgeliyor; kendi çekirdeğini bu dosyadan eklemek mümkündür. Üretilen `cmake/stm32cubemx/CMakeLists.txt` ayrı değerlendirilmelidir. [6.14 kaydı](https://community.st.com/stm32cubemx-mcus-29/cubemx-6-14-xx-breaks-cmake-workflows-151130), [ST dosya sahipliği açıklaması](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/1.0.0/en/docs/markup/basic_concepts/cmake.html).

Ayrıca doğru ad **`CMakePresets.json`**. `CMakePresets.cmake` ifadesi, ST moderatörünün yanıtındaki adlandırma hatasının tekrarıdır. 6.15 değişikliği, toolchain seçiminin üst dosyadan kaldırılmasıdır; toolchain’i komut satırından belirtmek de mümkündür. Güncel CubeMX’te alt proje sorununun giderilip giderilmediği bu incelemede doğrulanmadı. [6.15 açıklaması](https://community.st.com/stm32cubemx-mcus-29/cmake-project-generated-by-stm32cubemx-6-15-build-fails-says-compiler-broken-154417), [CMake preset şartnamesi](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html).

**4. TrustZone açma ve kapama aynı işlem gibi anlatılmış.**

Bölüm 5’teki “açma/kapama RDP regresyonu gerektirir” ifadesi yanlış genellemedir. AN5347, etkinleştirmeyi TZEN’in ayarlanması; devre dışı bırakmayı RDP regresyonuyla birlikte gerçekleştirilen işlem olarak ayırıyor. [AN5347](https://www.st.com/content/st_com/en/technical-documents/an5347-introduction-to-arm-trustzone-features-on-stm32l5-stm32u5-and-stm32u3-mcus-stmicroelectronics_asset/chunk-004-topic-an5347-security-tz-enabled.html).

“TZEN=1 iken non-secure proje çalıştırılamaz” da fazla kesin. **Tek başına hazırlıksız non-secure imajla başlatma** sorundur; uygun secure başlangıç ve kaynak bölümlemesiyle non-secure uygulama çalıştırılabilir. [Başlangıç güvenlik durumu](https://www.st.com/content/st_com/en/technical-documents/an5347-introduction-to-arm-trustzone-features-on-stm32l5-stm32u5-and-stm32u3-mcus-stmicroelectronics_asset/chunk-002-topic-an5347-stm32l5-series-security-configurations.html).

**5. ThreadX lisans ayrımı doğru, kart değişimi sonucu aşırı geniş.**

ST lisansı mevcut U5 kullanımına izin veriyor; listede başka birçok STM32 serisi de bulunuyor. Kart değişimi ancak yeni donanım ilgili hakların dışında kalırsa bu sorunu doğurur. Ayrıca lisans, iç geliştirme/test/değerlendirmeyi üretim ve dağıtımdan ayrı düzenliyor. Upstream MIT tercihinin taşınabilirlik gerekçesi sağlamdır; “kart değişince ST lisansı geçersiz” ifadesi düzeltilmelidir. [ST lisansının §1–2’si](https://github.com/STMicroelectronics/stm32-mw-threadx/blob/main/LICENSE.md), [liste](https://github.com/STMicroelectronics/stm32-mw-threadx/blob/main/LICENSED-HARDWARE.txt).

**6. MAVLink’i eleme gerekçesi yeterli değil.**

Raporda özel HIL protokolü, DVL/sonar eksikliği ve ArduPilot’un tarihsel HIL tercihiyle gerekçelendiriliyor. Doğru ayrım şöyle:

- `ODOMETRY`, DVL’den türetilmiş hız/çerçeve/kovaryansın bir bölümünü taşıyabilir; ışın verilerini ve bütün DVL durumlarını standartlaştırmaz.
- `DISTANCE_SENSOR` ultrasonik menzili kapsar; ham sonar görüntüsüyle eşdeğer değildir.
- `BATTERY_STATUS` mevcut; sızıntı için durum biti de bulunur. Bunların ham HIL girdisi olarak işlenmesi alıcı firmware’in sorumluluğudur.
- `HIL_SENSOR.fields_updated`, farklı sensör güncelleme hızlarını destekler. [Common tanımları](https://mavlink.io/en/messages/common.html).

Eksik alanlar **özel MAVLink dialect’iyle** tanımlanabilir. Üretilen C başlıklarını depoda tutmak mümkündür; firmware derlemesinde sürekli kod üretmek gerekmez. Kütüphanenin kanal durumları ve alıcı tamponları statik olarak tutulabilir. Dolayısıyla sıfır dinamik bellek yalnız özel protokolün üstünlüğü değildir. [Dialect tanımlama](https://mavlink.io/en/guide/define_xml_element.html), [C uygulaması](https://raw.githubusercontent.com/mavlink/c_library_v2/master/mavlink_helpers.h).

Özel protokolün sağlam gerekçeleri; tam sensör semantiği, büyük paket ihtiyacı ve ölçülmüş maliyet olabilir. Klasik MAVLink 2 çerçevesinin imzasız ek yükü 12 bayt, yük sınırı 255 bayttır. Aynı semantiği taşıyan iki adayın gecikmesi, CPU/RAM maliyeti ve bakım yükü karşılaştırılmalıdır. [Tel biçimi](https://mavlink.io/en/guide/serialization.html).

**7. Ortak struct, ortak tel biçimi sağlamaz.**

Tek `proto.h`; padding, hizalama, enum/bool boyutu, endian veya kayan nokta gösterimini tanımlamaz. COBS çerçeve sınırını, CRC bütünlük denetimini sağlar; bu sorunları çözmez. Raporda açık bir serileştirme sözleşmesi eksiktir.

Sabit genişlikli alanlar, bayt sırası, IEEE-754 koşulu, uzunluk sınırları, geçersiz değerler ve sürüm uyuşmazlığı davranışı tanımlanmalı; alanlar açıkça kodlanıp çözülmelidir. `packed` tek başına taşınabilirlik çözümü değildir. [C nesne gösterimi ve yapı yerleşimi](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).

**8. RTOS’suz 400 Hz önerisi savunulabilir, fakat tasarım henüz tamamlanmış değil.**

Rapor ölçüm gereğini doğru belirtiyor. Ancak 2,5 ms periyot ve ortalama CPU tahmini, en kötü durum yanıt süresini kanıtlamaz. Tam kontrol adımı ISR’de kalacaksa bloklayan çağrı içermemeli; üst öncelikli kesmeler, kesme maskeleri ve ana döngünün USB servis süresi birlikte bütçelenmelidir. HAL, düşük öncelikli tick’e bağımlı `HAL_Delay` kullanımının ISR’yi kilitleyebileceğini açıkça uyarıyor. [HAL kaynağı](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx_hal_driver/2552682ee5a0d6f826a6435750fc9267be65868b/Src/stm32u5xx_hal.c).

TinyUSB’nin gerçek USB kesme işleyicisi korunmalı; uygulama ayrıştırması ve kontrol hesabı uygun bağlama ayrılmalıdır. “Yalnız FIFO ve bayrak” sürücünün gerçek sorumluluklarını eksik anlatıyor. [Eşzamanlılık belgesi](https://docs.tinyusb.org/en/latest/reference/concurrency.html), [dwc2 ISR](https://github.com/hathach/tinyusb/blob/0.21.0/src/portable/synopsys/dwc2/dcd_dwc2.c).

**Mühendislik değerlendirmem:** Kısa timer ISR’siyle zaman damgası/iş salımı ve bütçeli öncelikli yürütme daha kolay doğrulanır. Tam hesabın ISR’de kalması da ölçümle savunulabilir. Her iki seçenekte tutarlı sensör görüntüsü, kuyruk taşması, kaçırılmış adım ve bayat veri davranışı tanımlanmalıdır.

**9. Bellek yasağının uygulama tarifi eksik.**

`--wrap=malloc`, çözülmemiş başvuruları `__wrap_malloc` sembolüne yönlendirir. Tanımsız wrapper bağlantı hatası oluşturabilir; “boş malloc tanımı” oluşturmaz. `calloc`, `realloc`, reentrant libc girişleri ve özel havuzlar ayrıca denetlenmelidir. [GNU ld açıklaması](https://sourceware.org/binutils/docs/ld/Options.html).

## Rapor dışı ek bulgular

- **IWDG süre aralığı raporda verilmemiş.** U5 HAL’ında prescaler 4–1024, reload 0–4095’tir. `t=(reload+1)×prescaler/fLSI` hesabıyla, **32 kHz nominal LSI’de 125 µs–131,072 s** elde edilir; HAL açıklaması üst sınırı yaklaşık 131,04 s yazar. Gerçek süre LSI toleransına bağlıdır. `LSIPREDIV` ile 250 Hz seçimi süreleri ayrıca değiştirir. [HAL başlığı](https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/main/Inc/stm32u5xx_hal_iwdg.h), [HAL açıklaması](https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/main/Src/stm32u5xx_hal_iwdg.c), [hesabı uygulayan sürücü](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/drivers/watchdog/wdt_iwdg_stm32.c), [DS13543 LSI tablosu](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf).

- **Watchdog canlılık denetimi yapmalı.** Bu benim mühendislik çıkarımımdır: yalnız 400 Hz ISR’den watchdog beslemek, USB/ana döngü kilitlense bile sistemi canlı gösterebilir. Besleme; kontrol, haberleşme ve veri tazeliği denetimlerinin birleşik sonucuna bağlanmalı. Zararsız görev öğesi çıkışında reset boyunca güvenli seviye, dış kapılama ve pasif dirençlerle elektriksel olarak sağlanmalı; IWDG’nin varlığından kendiliğinden çıkmaz.

- **USB HS, 400 Hz uçtan uca gecikme garantisi değildir.** Bulk transfer için ayrılmış bant genişliği yoktur; yoğunlukta bekleyebilir. CDC veya vendor-bulk seçimi bu temel durumu değiştirmez. Host duraklaması, kablo kopması ve geciken paketler kabul testine eklenmelidir. [Microsoft bulk aktarım açıklaması](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-bulk-and-interrupt-transfer).

- **Gerçekçilik için HIL’in enjeksiyon sınırı belirlenmeli.** Mühendislik çıkarımı: sensör sürücüsünden sonraki yapılara veri vermek; gerçek SPI/I²C edinimini, DMA zamanlamasını ve sensör sürücüsü hatalarını sınamaz. PC modeli örnekleme zamanı, gecikme, kayıp, doygunluk ve arıza bayraklarını üretmeli; aktüatör komutunun uygulanma zamanı da modellenmelidir. Bunlar RTOS seçiminden bağımsızdır.

- **TinyUSB portunun “küçük” olması garanti değil.** U5A5 BSP’sindeki ADC tabanlı VBUS algılama pinleri, bölücü/referans değerleri ve Type-C davranışı DK şemasıyla eşleştirilmelidir. MCU sürücüsünün hazır olması kart bağlantılarının hazır olduğu anlamına gelmez. [BSP uygulaması](https://github.com/hathach/tinyusb/blob/0.21.0/hw/bsp/stm32u5/boards/stm32u5a5nucleo/board.h).

- **Güncel CubeMX davranışı ayrıca sabitlenmeli.** 6.18.0 resmî FAQ’sı CMake projelerinde Picolibc varsayılanını belgeliyor. Rapordaki Newlib-nano varsayımı bütün güncel üretim akışlarına genellenemez; derleyici, libc ve linker ayarları birlikte sabitlenmelidir. [CubeMX FAQ](https://dev.st.com/stm32cube-docs/stm32cubemx/6.18.0/en/docs/markup/CubeMX_FAQ.html).

- **Erişim sınırları:** RM0456 PDF’si araçta boyut sınırına takıldı; HTML adresi de açılamadı. Dolayısıyla §15.2.3’ün metnini doğrulanmış saymıyorum; PHY işlevleri ve önerilen değerler erişilen HAL kaynaklarından doğrulandı. ST CMake 1.0.1 sayfasının devamına erişim zaman aşımına uğradı; gerekli içerik açılabilen resmî 1.0.0 belgesinden kontrol edildi. Bazı commit adresleri GitHub bağlayıcısında okunamadı; aynı kaynaklar web aracındaki ham URL’lerden açıldı. DK’da HS kararlılığı, Windows/macOS gecikmesi ve gerçek CPU yükü doğrulanmış değildir.

## Raporlar arası çelişki değerlendirmesi

**SMPS — 01 doğrulamasıyla esaslı çelişki yok.** 08 raporu zaten SMPS’in zorunlu olmadığını kabul ediyor. Ancak 1,8 V LDO yolunu yeniden açık soru yapması gereksiz: DS13543 Tablo 38, LDO/Range 1/160 MHz’i 1,71–3,6 V kapsamında belgeliyor. ST örneğinin SMPS seçmesi çalışma zorunluluğu değil, örnek tercihidir. [DS13543](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf).

**MAVLink — 06 doğrulamasıyla çelişki devam ediyor.** 08 raporu, önceki doğrulamanın düzelttiği DVL/sonar gerekçesini yeniden kullanıyor. Sensör mesajının standardında bulunması, bütün sensör anlamlarını taşıması ve firmware tarafından HIL girdisi kabul edilmesi ayrı konulardır. Özel dialect bunların uygulanabileceği geçerli bir seçenektir. [Common mesajları](https://mavlink.io/en/messages/common.html), [dialect tanımı](https://mavlink.io/en/guide/define_xml_element.html).

**Zaman modeli — kısmi ilerleme var.** 08, simülasyon ve gerçek zamanı ayırmayı benimsemiş. Fakat serbest çalışan 400 Hz timer ile PC’den adım bekleyen HIL’in uzlaştırılması hâlâ tanımlı değil. Geciken adımda bekleme, eski veriyle ilerleme veya güvenli duruma geçme kararı açık olmalı; iki saat alanını taşımak tek başına bu kararı vermez.

## Mimari için net sonuçlar

1. **C17 çekirdek + C kart katmanı + C++ native PC kararı korunabilir.** Bu incelemede dil kararını değiştiren bulgu yok.
2. **TinyUSB güçlü adaydır; DK’da kanıtlanmış çözüm değildir.** USBX’i karşılaştırmalı başlatma referansı olarak kullanın.
3. **160 MHz için SMPS zorunlu sayılmamalı.** Saat, voltaj ölçeği ve regülatör seçimini ayrı doğrulayın.
4. **400 Hz RTOS’suz yapı koşullu olarak savunulabilir.** Kesme ve USB servis bütçelerini, veri paylaşımını ve hata davranışını tamamlayın.
5. **MAVLink özel dialect’ini elemeden karşılaştırın.** Özel protokol seçilirse ham struct yerine açık serileştirme kullanın.
6. **HIL zaman sözleşmesini kesinleştirin:** örnek zamanı, varış zamanı, adım kimliği, gecikme ve duraklama davranışı.
7. **Görev öğesi çıkışını elektriksel olarak güvenli tasarlayın.** Watchdog beslemesi bütün kritik işlerin canlılığını izlemeli.
8. **CubeMX ve Zephyr kararlarını düzeltilmiş gerekçelerle verin.** Dosya sahipliğini ve gerçek çevre birimi yapılandırmasını esas alın.