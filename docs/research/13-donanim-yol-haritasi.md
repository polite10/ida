# 13 - Donanım Yol Haritası (ardıl kart, gerçek sensör/aktüatör arayüzleri, soyutlama sınırı, kademeli tedarik)

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Doğrulaması: [dogrulama/13-codex.md](dogrulama/13-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Tarih: 2026-11-11. Kapsam: STM32U5A9J-DK geçici kart olduğu için, "gerçek sensörler geldiğinde yalnızca sürücü katmanı değişir" ilkesinin tutması amacıyla ardıl kart ve gerçek arayüzlerin şimdiden belirlenmesi. Bu belge `docs/research/00-duzeltmeler-ve-kararlar.md` ile çelişirse o belge geçerlidir. Rust ya da Rust tabanlı araç önerilmemiştir.

Yöntem notu: Web araması (arama özeti + sayfa çekimi) kullanıldı. ST'nin kendi sayfalarının (st.com) çoğu bu oturumda zaman aşımına uğradı; bu yüzden ST iddiaları Zephyr kart belgeleri, dağıtıcı sayfaları ve arama özetleri üzerinden dolaylı doğrulandı ve işaretlendi. Fiyatlar "dağıtıcı anlık görüntüsü"dür, tarihsiz olanlar bayat sayılmalıdır.

---

## 1. Özet

1. **STM32U5G9J-DK1 gerçek bir ardıl değildir; yalnızca yedek/aynı sınıf karttır.** Aynı kullanım kılavuzu (UM2967, "STM32U5x9NJ" kitleri) iki kartı kapsıyor, aynı iki 2,54 mm genişleme konnektörü, USB-C HS (sink), ekran odaklı kit; Ethernet ve CAN alıcı-vericisi yok. ST veri özeti (DB4620) kartın VDD'sinin yalnızca 1,8 V tasarlandığını söylüyor (hangi karta ait olduğu arama özetinde net değil; DK1 için ayrıca doğrulanmadı). Ayrıca Cortex-M33 tek duyarlıklı FPU. Mevcut BSP'nin neredeyse aynen çalışacağı bir yedek olarak değerli, ama sınırlamaları çözmez.
2. **Bench ardılı için en iyi aday NUCLEO-H753ZI** (NUCLEO-H743ZI/ZI2'nin ST tarafından önerilen yerine geçeni). STM32H7: Cortex-M7, **çift duyarlıklı FPU**, 480 MHz, Ethernet MAC + RJ45, 2 FDCAN (alıcı-verici harici), 4 USART + 4 UART, 6 SPI, 4 I2C. Zio/morpho konnektörleri, ST HAL/CubeMX tam desteği. Dağıtıcı fiyatı kabaca £21-€42 (anlık görüntü, doğrulanmadı güncel). H743ZI2 üretimden kalktı/NRND.
3. **NUCLEO-H723ZG (Active) ikinci aday**: 550 MHz, FPU, Ethernet, 2 FDCAN; ancak dağıtıcıda uzun teslim süresi (DigiKey ABD "99 hafta" standart) bildiriliyor; tedarik riski var.
4. **Araç içi (su altı) donanım için dev kit değil, Pixhawk sınıfı kart ya da özel taşıyıcı kart önerilir.** Pixhawk 6C/6X (STM32H743/H753): yerleşik IMU/barometre/manyetometre, 16 PWM, 2 CAN, 3-4 seri port, güç girişleri, standart JST-GH konnektörler; 6X ayrıca Ethernet ve 3 IMU. PX4 dokümanı bu kartları üretici desteğiyle listeliyor; **kartın üzerinde bizim C firmware'imizi koşturmak için resmî bir yol yok** (ArduPilot hwdef ve PX4 kart dosyalarından pin haritası türetilir; bu iş BSP yazımıdır).
5. **Sürücü soyutlama sınırı:** ArduPilot (`AP_HAL::Device`: transfer/periyodik geri çağrı/semafor), PX4 (uORB ile yalnızca yayın sözleşmesi) ve Zephyr (devicetree + sensör API) aynı fikre varıyor: **çip sürücüsü kart bağımsız, yalnızca veri yolu (SPI/I2C/UART/GPIO/PWM/CAN/zaman) ve pin/gerilim/saat konfigürasyonu kart bağımlı**. Öneri: çip sürücüleri ortak çekirdekte kalsın, BSP yalnızca bus HAL + kart konfigürasyonu olsun, HIL enjeksiyonu "sensör arayüzü" düzeyinde yapılsın.
6. **1,8 V karşılaştırmalı kritik gerçek:** ICM-42688-P (VDD ve VDDIO 1,71-3,6 V) ve BMI088 (VDDIO 1,2-3,6 V; VDD 2,4-3,6 V) 1,8 V I/O ile doğrudan konuşabilir; **Bar30 (3,3 V I2C), Ping sonar (3,3 V TTL), DVL A50 (3,3 V UART), SOS sızıntı sensörü (VCC'ye çeker), Blue Robotics ESC PWM (3,3-5 V) 1,8 V karta doğrudan bağlanmamalı**; seviye çevirici gerekir. Bu sorun H7 Nucleo (3,3 V) ve Pixhawk (3,3 V) ile büyük ölçüde ortadan kalkar (kart I/O gerilimi H7 Nucleo için doğrulanmadı; VDD aralığı 1,62-3,6 V).
7. **ESC:** Blue Robotics Basic ESC / BlueESC için aktüatör arayüzü **servo PWM (1100-1900 µs, 1500 µs nötr)**; BlueESC ek olarak I2C (5 V) sunuyor. DShot ArduPilot'un ESC'lerinde mevcut ama Blue Robotics su altı ESC'leri için resmî doğrulama bulunamadı. DShot/CAN yolu "ileri aşama seçeneği" olarak tutulmalı.
8. **Yardımcı bilgisayar:** Raspberry Pi 5 (~3 W boşta, yük altında ~9-12 W, üçüncü taraf ölçümler) ve Jetson Orin Nano Super (7/15/25 W modları, MAXN SUPER; lansman $249). Mikrodenetleyiciyle bağlantı için UART+MAVLink veya Ethernet (H7 sınıfı kartta) uygundur. Sızdırmaz muhafazada ısı en büyük risk: Blue Robotics'in 4" muhafaza + Pi 4 konfigürasyonu 0-30 °C ortam sınırı veriyor.
9. **HIL sensör emülasyonu:** 1,8 V UART için FTDI FT232R tabanlı 1,8 V kablo (TTL-232RG-VREG1V8-WE) mevcut; FT232H 3,3 V G/Ç'dir. I2C için PCA9306 (VREF1 1,2-3,3 V, VREF2 1,8-5,5 V), SPI için TXB-sınıfı itme-çekme çeviriciler uygun (TXB0108 I2C için kullanılmaz). Sensör taklidi için RP2040 (IOVDD 1,8-3,3 V) aday; **PIO ile I2C/SPI slave örneği doğrulanmadı**. W5500 gibi bir SPI-Ethernet çipi ile Oculus/DVL TCP taklidi mümkündür (00 belgesi 05.7).
10. **Önerilen kademe:** Aşama 0 (yalnızca DK, tam simülasyon, ~0) -> Aşama 1 (masa üstü: ICM-42688-P kartı + Bar30 + seviye çevirici + RP2040 emülatör; yaklaşık $150-300, çoğu fiyat doğrulanmadı) -> Aşama 2 (NUCLEO-H753ZI; ~€40-60) -> Aşama 3 (Pixhawk 6X ya da özel taşıyıcı + T200/ESC/batarya ile havuz testi). Yatırım sırası yazılım doğrulamasıyla eşleşmeli (Bölüm 3.6).

---

## 2. Bulgular

### 2.1 Ardıl otopilot kartı adayları

#### 2.1.1 Karşılaştırma tablosu

| Ölçüt | STM32U5A9J-DK (eldeki) | STM32U5G9J-DK1 | NUCLEO-H753ZI | NUCLEO-H723ZG | Pixhawk 6C | Pixhawk 6X | Özel taşıyıcı kart (H7 çekirdek modülü) |
|---|---|---|---|---|---|---|---|
| MCU | U5A9NJ, Cortex-M33 160 MHz | U5G9NJ, Cortex-M33 160 MHz, 3 MB SRAM / 4 MB flash | STM32H753ZI, Cortex-M7, 480 MHz | STM32H723ZG, Cortex-M7, 550 MHz | STM32H743, M7 480 MHz, 2 MB flash/1 MB RAM; IO: STM32F103 | STM32H753, M7 480 MHz, 2 MB/1 MB; IO: STM32F100 | Seçime bağlı (H7) |
| FPU | Tek duyarlıklı | Tek duyarlıklı | **Çift duyarlıklı** (H743 sayfası; H753 aynı çekirdek, ayrıca doğrulanmadı) | FPU var; duyarlık bu oturumda doğrulanmadı (H7 ailesi için DP beklenir) | Sayfada belirtilmemiş; H743 çekirdeği DP (ST) | Sayfada belirtilmemiş; H753 çekirdeği, DP beklenir (doğrulanmadı) | Seçime bağlı |
| Ethernet | Yok | Yok (yerleşik MAC/PHY/RJ45 bulunmuyor; ST kart sayfası kontrol edilemedi, doğrulanmadı) | Evet (RJ45) | Evet (RJ45) | Sayfada yok | **100 Mbps Ethernet portu** | Tasarıma bağlı |
| FDCAN | 1 (alıcı-verici yok) | 1 (aynı kılavuz sınıfı; doğrulanmadı) | 2 FDCAN (alıcı-verici harici) | 2 FDCAN (alıcı-verici harici) | 2 CAN | 2 CAN | Tasarıma bağlı |
| UART | 2 etiketli (alternatiflerle daha fazla, 00 belgesi) | benzer | 4 USART + 4 UART | 4 USART + 4 UART | 3 genel + 2 GPS | 4 genel + 2 GPS | tasarıma bağlı |
| SPI/I2C | 2 SPI, 3 I2C (etiketli) | benzer | 6 SPI, 4 I2C | 6 SPI, 4 I2C | 1 I2C portu; dahili sensör veri yolları | 1 I2C, 1 SPI (2 CS) | tasarıma bağlı |
| PWM | ~4-6 belgeli | benzer | Zio/morpho üzerinden timer pinleri (kart başına atanır) | aynı | **16** (8 IO + 8 FMU) | **16** | tasarıma bağlı |
| G/Ç gerilimi | **1,8 V** | 1,8 V (DB4620: VDD 1,8 V; DK1 için doğrulanmadı) | 3,3 V tipik (MCU VDD 1,62-3,6 V; kart ayarı doğrulanmadı) | aynı | 3,3 V mantık tipik (sayfada belirtilmemiş; doğrulanmadı) | aynı | Seçilir |
| Yerleşik sensör | Sıcaklık/ToF; IMU yok | ToF/ivme-jest + sıcaklık sensörü (STTS22H); IMU yok | Yok | Yok | ICM-42688-P + BMI055, IST8310, MS5611 | 3x ICM-45686 (Rev 8), ICP20100 + BMP388, BMM150 | Seçilir |
| ST HAL/CubeMX | Var | Var (Zephyr desteği de var) | Var (tam) | Var (tam) | Kart için hazır CubeMX projesi yok; HAL'e göre BSP yazılır | aynı | HAL/CubeMX seçilebilir |
| Konnektör | Genişleme 2x2,54 mm | aynı | Zio + morpho (Arduino uyumlu) | aynı | JST-GH (standart Pixhawk) | JST-GH | Tasarım |
| Durum/tedarik | Üretimden kalktı | **Active** (ST kart sayfası, arama özeti); fiyat bulunamadı | **Active**; DigiKey UK ~£21,60 / ~740 adet; Belçika €34,68; TME €42 (anlık görüntü, tarihsiz) | **Active**; DigiKey ABD standart teslim "99 hafta" | Holybro/bayiler; £165-225, $225 (plastik kasa) | Holybro $309,99; bayiler $269-$467 | Tasarım/üretim süresi |
| Su altına uygunluk | Hayır | Hayır | Hayır (çıplak dev kit) | Hayır | Orta: araç içi elektronik muhafazaya girer; ısı kontrollü IMU | İyi: 3 IMU, titreşim yalıtımı, yedekli barometre | Tasarıma bağlı |

Kaynak notu: Pixhawk 6C/6X özellikleri PX4 belgeleri: https://docs.px4.io/main/en/flight_controller/pixhawk6c.html , https://docs.px4.io/main/en/flight_controller/pixhawk6x.html . Nucleo-H743ZI "obsolete, yerine NUCLEO-H753ZI" ve Ethernet/FDCAN/ARM özellikleri: https://docs.zephyrproject.org/latest/boards/st/nucleo_h743zi/doc/index.html . H723ZG özellikleri: https://docs.zephyrproject.org/latest/boards/st/nucleo_h723zg/doc/index.html . U5G9J-DK1 donanımı: https://docs.zephyrproject.org/latest/boards/st/stm32u5g9j_dk1/doc/index.html . H743 çift duyarlıklı FPU: https://www.st.com/en/product/stm32h743zi . H743ZI2 NRND/obsolete: https://community.st.com/stm32-mcus-boards-and-hardware-tools-26/nucleo-h743zi2-nrnd-and-use-nucleo-h753zi-134211 .

#### 2.1.2 Adaylar hakkında notlar

**STM32U5G9J-DK1.** MCU STM32U5G9NJH6Q (Cortex-M33, TrustZone, 4 MB flash, 3 MB SRAM), 2,47" yuvarlak 480x480 DSI ekran, ST-LINK-V3E, USB-C HS sink-only, iki genişleme konnektörü (USART, LPUART, iki SPI, SAI, üç I2C, SDMMC, ADC, timer). Zephyr varsayılan eşlemesi: USART1 (VCP PA9/PA10), USART3 (PB10/PB11), LPUART1 (PG7/PG8), I2C1/2/6, SPI2/3 (U5A9J-DK ile birebir benzer). Kaynak: https://docs.zephyrproject.org/latest/boards/st/stm32u5g9j_dk1/doc/index.html ; Mouser sayfası: https://www.mouser.in/en/new/stmicroelectronics/st-stm32u5g9j-dk1-discovery-kit/ . ST kullanım kılavuzu (UM2967 "Discovery kits with STM32U5x9NJ MCUs") iki kartı kapsıyor: https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf (PDF bu oturumda çekilemedi; kapsam bilgisi arama özetinden). **Sonuç:** DK1, mevcut BSP'nin yedek donanımı olarak mantıklı; mimari kısıtlamaları (1,8 V, Ethernet yok, tek FDCAN, tek duyarlıklı FPU) aynen taşır. Kart yedek planı (00 belgesi A3) için "aynı BSP + küçük pin farkı" düzeyinde iş beklenir (doğrulanmadı).

**STM32H7 Nucleo-144 (H753ZI / H723ZG).** Ethernet RJ45 + MAC, FDCAN yalnızca MCU tarafı (PD0/PD1; harici CAN alıcı-verici gerekir), Zio konnektörü Arduino Uno V3'ü genişletir. ST-LINK sürümü: H743'te V2-1, H753'te V3 (Zephyr belgesi). Bu kartlar HIL ve gerçek zamanlı kontrol için yeterli CPU/FPU sağlar; fakat su altı aracında çıplak halde kullanılmaz (konnektör, titreşim, ısı, güç kaynağı).

**Pixhawk standardı kartlar.**
- Pixhawk 6C: H743, 16 PWM (8 IO + 8 FMU), 3 genel seri + 2 GPS, 1 I2C, 2 CAN, 2 analog güç girişi, ICM-42688-P + BMI055, IST8310, MS5611; POWER1/2 normal 4,9-5,5 V, servo rayı 0-36 V (sayfa 42 V mutlak azami de veriyor). Ethernet yok.
- Pixhawk 6X: H753, 100 Mbps Ethernet, 2 CAN, 16 PWM, 4 genel seri, 2 GPS, 1 SPI (2 CS), 3 IMU + 2 barometre ayrı veri yollarında, yalıtılı sensör güç alanları; "standart, mini, Jetson ve RPi CM4 taban kartları" için pinout dokümanı.
- PX4 dokümanı: "PX4 bu (veya herhangi bir) otopilotu üretmez"; donanım sorunları üreticiye (Holybro) gider.
- Su altı için kanıtlanmış yol **ArduSub**'dır: BlueOS belgesi önerilen kartlar olarak Blue Robotics Navigator (Raspberry Pi 4B gerektirir), Pixhawk 1 ve Holybro Pixhawk 4 / Cube'u sayıyor (https://blueos.cloud/docs/hardware/required/flight-controller/ , arama özeti). Bizim C firmware'imiz için ArduSub gerekmez ama "donanım çoğunlukla kanıtlanmış" argümanı Pixhawk için geçerlidir.
- Yazılım tarafı: ArduPilot kartı `libraries/AP_HAL_ChibiOS/hwdef/<kart>/hwdef.dat` ile tanımlıyor (https://ardupilot.org/dev/docs/porting.html); bu dosyalar pin haritalarını kaynak koda açık biçimde taşıdığı için kendi C BSP'miz için **pin haritası kaynağı** olarak kullanılabilir (Pixhawk6C/6X hwdef'inin varlığı bu oturumda doğrulanmadı).
- CUAV (V5+/X7) ve Cube serisi bu oturumda incelenmedi; "doğrulanmadı".
- Fiyatlar (tarihsiz bayi anlık görüntüleri): 6X Holybro $309,99, bayiler $269-$468; 6X Pro $619-$873; 6C £165-225 / $224,99 (plastik kasa), 6C Mini $177. Kaynak: https://holybro.com/products/pixhawk-6x ve bayi sayfaları (arama özeti; doğrulanmadı).

**Özel taşıyıcı kart.** Pixhawk 6X'in taban kartları ve "Jetson/RPi CM4" taban kartı tasarımı, bir FMU çekirdeği + özel taban modelinin ticari olarak da yapıldığını gösterir. Bizim için: H7 çekirdek modülü (ya da doğrudan H753 + kendi PCB) + su altına özel konnektörler (Blue Robotics WetLink penetratörleri, bkz. 2.6). Maliyet/süre yüksek; Aşama 3'ten sonra düşünülmelidir.

### 2.2 Gerçek sensör ve aktüatör arayüzleri

| Aygıt | Arayüz | Gerilim | Veri hızı / zaman | Tipik sürücü yapısı | Kaynak |
|---|---|---|---|---|---|
| IMU ICM-42688-P | SPI (24 MHz), I2C 1 MHz, I3C 12,5 MHz | VDD 1,71-3,6 V; **VDDIO 1,71-3,6 V** | 2 kB FIFO, 2 programlanabilir kesme; ODR 12,5 Hz-32 kHz (05 belgesi) | SPI cihaz + data-ready/FIFO kesmesi; kayıt sağlama toplamı | https://invensense.tdk.com/?p=9240 ; TDK sayfası (arama özeti) |
| IMU BMI088 | SPI veya I2C; ivmeölçer ve jiroskop **ayrı CS** (CSB1 ivme, CSB2 jiro) | VDD 2,4-3,6 V; VDDIO 1,2-3,6 V | SPI hız üst sınırı bu oturumda doğrulanmadı | İki ayrı SPI cihaz; ivmeölçer okumalarında kukla bayt (ikincil kaynak, doğrulanmadı) | https://www.bosch-sensortec.com/products/motion-sensors/imus/bmi088/ |
| Manyetometre (IIS2MDC / LIS3MDL / RM3100 vb.) | I2C (SPI de mümkün) | LIS3MDL Vdd_IO 1,71 V min; LIS3MDL ST'de obsolete (LIS2MDL önerilir); IIS2MDC 1,71-3,6 V | tipik 10-100 Hz | I2C cihaz; sürücü hatalı örnek eler | ST sayfaları (arama özeti), https://www.st.com/en/product/lis3mdl |
| Basınç/derinlik Bar30 (MS5837-30BA) | **Yalnızca I2C**, adres 0x76 (sabit; veri yolunda tek sensör) | Bar30 kartı: besleme 2,5-5,5 V, **I2C mantığı 3,3 V**; çip 1,5-3,6 V | OSR 8192: tipik 16,44 ms, **azami 18,08 ms** dönüşüm; basınç + sıcaklık için iki dönüşüm | I2C sıralı durum makinesi (reset, PROM oku, D1/D2 dönüştür, CRC4); 18,08 ms'den kısa bekleme olmaz | https://bluerobotics.com/?p=227172 ; datasheet kopyası https://www.farnell.com/datasheets/2917217.pdf |
| Ping sonar (Ping1D) | UART (TTL), 115200 (varsayılan) veya 9600; Ping Protocol | Besleme 4,5-5,5 V; **mantık 3,3 V (5 V toleranslı)** | Güncelleme hızı sayfada yok; akım 100 mA tipik/900 mA tepe; 300 m | UART + çerçeve ayrıştırıcı (B,R, u16 uzunluk, u16 mesaj kimliği, u16 toplam sağlama; küçük-endian); master-slave | https://bluerobotics.com/store/sonars/echosounders/ping-sonar-r2-rp/ ; https://docs.bluerobotics.com/ping-protocol/ |
| Ping360 | USB, **RS485**, Ethernet (UDP); iç konektör taşınarak arayüz değişir | 11-25 V (sayfada iki değer: 11-25 / 11-18 V), azami 5 W | RS485 baud sayfada yok | Ping Protocol; RS485'te yarı çift yönlü yeniden deneme gerekebilir | https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/ ; https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/ ; "Changing Communications Interface on the Ping360" (https://bluerobotics.com/?p=120410) |
| DVL Water Linked A50 | UART **3,3 V (5 V toleranslı)**, 115200 8N1, hız kontrolü yok; Ethernet; açılışta UART hatlarında <10 µs ~5 V sıçrama uyarısı | Besleme 10-30 V; tüketim kaynaklara göre ortalama 3-4 W, tepe 15-35 W (çelişkili) | 2-15 Hz (05 belgesi) | UART ya da TCP + JSON/CRC-8; `time_of_validity` ayrı | https://waterlinked.github.io/dvl/interfaces (arama özeti); https://www.blueyerobotics.com/specsheet/WL-DVL-A50/pdf |
| GNSS (u-blox MAX-M10S / NEO-M9N) | UART (UBX/NMEA), I2C | MAX-M10S UART varsayılanı 38400 baud (SparkFun); modülün V_IO/VCC aralığı bu oturumda doğrulanmadı | 1-10 Hz navigasyon; UBX | UART + UBX ayrıştırıcı | https://docs.sparkfun.com/SparkFun_u-blox_MAX-M10S/hardware_overview/ ; https://content.u-blox.com/sites/default/files/MAX-M10S_DataSheet_UBX-20035208.pdf |
| ESC (Blue Robotics Basic ESC / R3) | Servo PWM; 1500 µs nötr, 1900 tam ileri, 1100 tam geri (BlueOS belgesi); BLHeli_S yazılımı; OneShot125 yetenekli | Giriş 7-26 V (2-6S), sürekli 30 A (soğutmaya bağlı); sinyal mantığı 3,3 V olarak resmî doğrulanmadı | 50-400 Hz PWM (BlueESC için) | Timer PWM çıkışı; zaman aşımı/başarısızlık güvenli değeri | https://blueos.cloud/docs/1.2/integrations/hardware/required/esc/ ; https://docs.bluerobotics.com/bluesc/ ; arama özeti (BESC R3) |
| BlueESC | PWM 3,3-5 V, 50-400 Hz; I2C 5 V adres 0x29 (0x38'e kadar) | 6-22 V, 35 A suda/25 A havada | | PWM veya I2C | https://docs.bluerobotics.com/bluesc/ |
| DShot (ESC seçeneği) | Tek tel, 16 bit çerçeve (11 bit gaz, 1 bit telemetri isteği, 4 bit CRC), DShot150/300/600, çift yönlü telemetri | 3,3 V mantık (ArduPilot sayfası seviyeyi söylemiyor, doğrulanmadı) | 600 kbit/s ~37,5 kHz çerçeve | Timer + DMA; tam uyum ESC yazılımına bağlı | https://ardupilot.org/copter/docs/common-dshot-escs.html (arama özeti) |
| Sızıntı sensörü (Blue Robotics SOS) | GPIO (dijital; sızıntıda sinyal VCC'ye çekilir) | Besleme 3,3-5 V, maks. 20 mA; Pixhawk AUX OUT 6'ya bağlanır | Kesme/seviye yoklama | GPIO + EXTI; debounce; kalıcı (latch) bayrak | https://bluerobotics.com/?p=11612 ; https://claude.wp.bluerobotics.com/?p=113143 |
| Güç ölçüm modülü | Analog (V, I) ve/veya I2C SMBus (Pixhawk'ta "2 güç girişi SMBus ile") | Pixhawk POWER1/2 normal 4,9-5,5 V | 10-100 Hz | ADC + bölücü kalibrasyonu; ya da I2C | PX4 6C/6X sayfaları |
| Acil durdurma anahtarı | Devre düzeyi: güç hattını (batarya/ESC) mekanik/kontaktörle keser; yazılıma yalnızca durum bilgisi (GPIO) taşır | Hat gerilimi | - | **Yazılım kaynaklı olmamalı**: ESC güç rayını bağımsız keser | Bu belgenin önerisi (kaynak yok) |
| Torpido servosu | Servo PWM, 50 Hz tipik | 5-6 V servo rayı; 3,3 V sinyal | - | Timer PWM | Servo standardı (genel bilgi) |
| Solenoid | MOSFET sürücü + flyback diyotu | Solenoid gerilimi | Darbe süresi ms düzeyi | GPIO -> MOSFET kapısı (mantık seviyeli) | Genel elektronik pratiği (kaynak yok) |
| Aydınlatma | PWM (dimmer) ile sürücü | Batarya gerilimi | 1-20 kHz PWM | Timer PWM + güç MOSFET | Genel pratik; Blue Robotics Lumen için bu oturumda doğrulama yapılmadı |

**Notlar.**
- Ping Protocol belgesi çerçeveyi veriyor ama fiziksel katmanı ve baud'ları vermiyor: https://docs.bluerobotics.com/ping-protocol/ . Ping360 RS485 baud değeri **doğrulanmadı**; 05 belgesindeki açık soru 9 hâlâ geçerli.
- Bar30'un 3,3 V I2C mantığı Blue Robotics'in kendi uyarısıdır: 5 V mikrodenetleyici seviye çevirici gerektirir. 1,8 V karttan Bar30'a bağlantı da seviye çevirici gerektirir; PCA9306 bu aralığı kapsar (VREF1 1,2-3,3 V, VREF2 1,8-5,5 V): https://www.ti.com/product/pca9306 .
- DVL A50'nin UART'ı 3,3 V; 1,8 V kartın girişine doğrudan bağlanmaz; ayrıca açılışta <10 µs, ~5 V sıçrama bildirilmiş: bu yüzden seviye çevirici ve koruma önerilir.
- Pixhawk'ta sızıntı sensörü AUX OUT 6'ya bağlı olarak "kuruyken düşük" mantığıyla yapılandırılıyor (QGroundControl): https://claude.wp.bluerobotics.com/?p=113143 .
- ICM-42688-P'nin 1,8 V uyumu seviye çevirici gerektirmeyen en kolay sensör olmasını sağlar. 00 belgesindeki "1,8 V giriş toleransı pin ve yön bazında" hükmü geçerli kalır.

### 2.3 Donanım soyutlama katmanı için dersler

**ArduPilot AP_HAL.** `AP_HAL.h` saf sanal arayüzleri tek başlıkta topluyor: UARTDriver, AnalogIn, Storage, GPIO, RCInput, RCOutput, Scheduler, Semaphores, Util, OpticalFlow, Flash, DSP, CANIface (başlık: https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/AP_HAL/AP_HAL.h ; `I2CDevice`/`SPIDevice` ayrı başlıklarda tanımlı, bu çekimde görünmedi). `AP_HAL::Device` çip sürücüsünü veri yolundan ayırır: `BusType` (I2C, SPI, UAVCAN, SITL, MSP, SERIAL, WSPI), 32 bit `bus_id`, `transfer()`, `set_speed()` (HIGH/LOW), `get_semaphore()` (yalnızca başlatma), `register_periodic_callback(period_usec, cb)` (veri yolu iş parçacığında kilit tutulu), `register_completion_callback()` (DMA), `setup_checked_registers()` (kayıt bozulma denetimi), `set_chip_select()`/`set_read_flag()` (SPI özgülükleri) (https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/AP_HAL/Device.h). Kart tanımı ayrı: ChibiOS kartları için `hwdef.dat` (+ bootloader için `hwdef-bl.dat`) pin, veri yolu, seri sırası ve parametre varsayılanlarını taşır (https://ardupilot.org/dev/docs/porting.html). Linux kartları için ayrı HAL (`AP_HAL_Linux`) ve SITL için `BUS_TYPE_SITL` var: **aynı sürücü kodu farklı HAL'lerde koşar.**

**PX4.** Sürücüler `src/drivers` altında; "kural" olarak yalnızca uORB mesajı yayınlamaları gerekir. 24 bit cihaz kimliği (veri yolu türü, örnek, adres, cihaz türü) kalibrasyon parametrelerini ve günlük kayıtlarını eşler (https://docs.px4.io/main/en/middleware/drivers.html). Kart yapılandırması kconfig ile ayrı. **Ders:** sürücü sınırı "yayınlanan ölçüm mesajı"dır; tüketici (EKF) çip sürücüsünü görmez.

**Zephyr.** Devicetree donanımı tanımlar; kart dosyaları pin eşlemesi, etkin çevre birimleri, varsayılan Kconfig'i taşır; uygulama `aliases`/`chosen` düğümleri ve board overlay üzerinden kart bağımsız kalır (https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html). Örnek: ICM-42688 için `invensense,icm42688` bağlayıcısı, SPI ve isteğe bağlı RTIO ile FIFO akışı ve sensör emülatörü (`icm42688_emul.c`) mevcut (https://docs.zephyrproject.org/latest/build/dts/api/bindings/sensor/invensense,icm42688.html). **Ders:** emülatör aynı sensör API arkasına bağlanabiliyor: tam "çip sürücüsü + emüle bus" testi mümkün.

**C'de BSP ve bus soyutlama deseni (öneri, kaynaklardaki desenlerden türetildi).**
- `hal_spi_dev_t`, `hal_i2c_dev_t`, `hal_uart_t`, `hal_gpio_t`, `hal_pwm_t`, `hal_can_t`, `hal_time_*` gibi opak tutamaç + fonksiyon işaretçisi tabloları (`transfer`, `set_speed`, `lock/unlock`, `register_periodic`).
- Sensör arayüzü (ör. `imu_if_t { start; read_sample; status; }`, `baro_if_t`, `mag_if_t`, `range_if_t`, `dvl_if_t`, `gnss_if_t`) çekirdeğin gördüğü tek sınır olsun. Çip sürücüleri (ICM-42688, MS5837, Ping, A50, UBX) bu arayüzü bus HAL üzerinden uygular.
- HIL'de aynı arayüzün ikinci bir uygulaması (`imu_if_sim`) USB/UART mesajlarından beslenir.
- Kart bağımlı: yalnızca `bsp_<kart>.c` (pin tabloları, saat, VDDIO/seviye notları, DMA/kesme eşleme) + bus HAL'lerin ST HAL tabanlı uygulamaları + USB/UART taşıma.

**Sınır nereden geçmeli?** ArduPilot/PX4/Zephyr'den ortak sonuç: **bus HAL ile kart konfigürasyonu aşağıda, çip sürücüleri ve sensör arayüzleri yukarıda, çekirdek hiçbirini görmez.** Kart değişiminde yeniden yazılan: BSP + bus HAL uygulaması + saat/güç yapılandırması. Çip sürücüleri, çekirdek, protokol ayrıştırıcıları, kontrol, EKF değişmez. İstisna: pin gerilim seviyesi ve seviye çevirici varlığı BSP ve donanım şemasının işidir.

### 2.4 Yardımcı bilgisayar seçenekleri (kamera ile hedef algılama)

| Aday | Güç | Arayüz (MCU'ya) | Not |
|---|---|---|---|
| Raspberry Pi 5 | Boşta ~3-4 W, tam yükte ~9-12 W (üçüncü taraf ölçüm/tahmin); 27 W/5 V-5 A PSU önerilir | UART (MAVLink), USB, Gigabit Ethernet (H7 kartta); kamera CSI | Fiyat lansmanda 8 GB için $80-90 civarı raporlanmış, güncel doğrulanmadı |
| Jetson Orin Nano Super Developer Kit | 7 / 15 / 25 W modları + MAXN SUPER | USB, Ethernet, UART | Lansman fiyatı $249; yazılım güncellemesiyle Super moda geçilir; JetPack 6.2 |
| Pi 4 / CM4 | Daha düşük | Blue Robotics Navigator + ArduSub ekosistemi | BlueOS "Pi 4B" öneriyor |
| Pixhawk 6X Jetson / RPi CM4 taban kartı | Taşıyıcı karta bağlı | FMU ile yerleşik bağlantı | PX4 sayfasına göre "Jetson and RPi CM4 baseboards" için pinout belgesi var |

Kaynaklar: https://jetsonhacks.com/2024/12/17/jetson-orin-nano-super-developer-kit/ , https://www.cnx-software.com/2024/12/18/249-nvidia-jetson-orin-nano-super-developer-kit-targets-generative-ai-applications-at-the-edge/ , https://raspberry.tips/en/faq/raspberry-pi-power-consumption-update-2026-all-models-compared , https://www.tomshardware.com/reviews/raspberry-pi-5 .

**Isı.** Blue Robotics'in Pi 4B içeren 4" muhafaza konfigürasyonu 0-30 °C çalışma aralığı veriyor; kapalı muhafazada Pi CM4 ~80 °C'de kısılıyor (Element14 blogu). Jetson'un 15-25 W'ı çok daha zor: alüminyum uç kapağa ısı iletimli montaj, ortam suyu sıcaklığı ve güç modu sınırlama (7-15 W) ölçülerek seçilmelidir. Yayımlanmış bağımsız bir Blue Robotics muhafaza ısıl testi bulunamadı (https://discuss.bluerobotics.com/t/heat-from-electronic-parts-inside-watertight-enclosure/168 , https://bluerobotics.com/?p=15815 , arama özeti).

**MCU bağlantısı.** MAVLink/UART (115200-921600) hedef algılama çıktısı için yeterlidir (küçük mesajlar). Görüntü akışı MCU'dan geçmez; bu yüzden U5A9J-DK'da kamera sorunu 00 belgesindeki 07.1 hükmüyle yardımcı bilgisayar tarafında çözülmelidir. H7 Nucleo/Pixhawk 6X'te Ethernet ile bağlantı da mümkündür.

### 2.5 Kablo seviyesinde HIL için sensör emülasyon donanımı

| Gereç | Kullanım | Not/Kaynak |
|---|---|---|
| FTDI TTL-232RG-VREG1V8-WE (FT232R tabanlı, 1,8 V) | PC <-> kart UART (Ping, A50, GNSS, MAVLink taklidi) | FT232R VCCIO 1,8-5,25 V; **FT232H 3,3 V G/Ç**, 1,8 V için seviye çevirici gerekir. https://uk.rs-online.com/web/p/products/7158525 (arama özeti) |
| PCA9306 (I2C) | Bar30 <-> 1,8 V kart | VREF1 1,2-3,3 V / VREF2 1,8-5,5 V. https://www.ti.com/product/pca9306 |
| TXB-sınıfı itme-çekme çevirici | SPI (ICM-42688-P'yi 3,3 V karta bağlarken vb.) | TXB0108 I2C için uygun değil (embeddedrelated özeti); veri hızı/yük sınırlarını TI datasheet'inden kontrol et |
| RP2040 (Pico) | I2C/SPI **slave** olarak ICM-42688/MS5837 kayıt haritası taklidi | IOVDD 1,8-3,3 V (RP2040 datasheet özeti). **PIO ile slave doğrulanmadı;** donanım I2C bloğu slave olabilir (SparkFun forum, doğrulanmadı). SPI slave için donanım SPI bloğu kullanılabilir (doğrulanmadı). |
| W5500 vb. SPI-Ethernet | Oculus/DVL TCP taklidi (kartta Ethernet olmayan durum) | 00 belgesi 05.7 |
| Mantık analizörü / osiloskop | Zamanlama ve kayıt doğrulaması | genel |

**Hangi sensör hangi yolla:**
- **Kablo seviyesinde sınanabilenler:** UART tabanlı olanlar (Ping1D, Ping360-RS485, A50 UART, GNSS UBX/NMEA); I2C sensörler (MS5837, manyetometre) emülatör MCU ile; SPI IMU emülatörle mümkün ama zordur (örnekleme zamanı, FIFO, kesme).
- **Yalnızca mesaj enjeksiyonuyla sınanabilenler (kartta gerçek arayüz yoksa):** Ping360 Ethernet/UDP, A50 Ethernet/TCP, Oculus; ESC PWM ve aktüatör çıkışları (yalnızca ölçülür/okunur); sızıntı GPIO'su (GPIO ile mümkün); acil durdurma hattı (fiziksel).
- **Kablo seviyesi neyi kanıtlar:** gerçek SPI/I2C edinimi, DMA zamanlaması, sürücü hataları, CRC/zaman aşımı yolları; mesaj enjeksiyonu yalnızca sensör sürücüsünden sonraki yapıları sınar (00 belgesi "HIL enjeksiyon sınırı").

### 2.6 Deniz aracı elektrik entegrasyonunun temelleri

- **Güç dağıtımı.** Batarya -> sigorta/devre kesici -> güç dağıtım kartı -> ESC'ler (yüksek akım), buck dönüştürücüler (5 V/3,3 V mantık), güç ölçüm modülü. BlueROV2 örneğinde batarya kutusundan ana elektronik kutusuna 11 AWG eşdeğer, WetLink penetratörlü ve 5,5 mm bullet konektörlü kablo seti kullanılıyor (https://bluerobotics.com/?p=142189 , arama özeti; bullet boyutu kaynaklar arasında 5 mm / 5,5 mm diye çelişkili). Sigorta değerleri bu oturumda doğrulanmadı.
- **Su geçirmez geçişler.** WetLink penetratör 950 m dereceli, 3,7-9,8 mm kablo aralığı; T200 itici penetratörle gelir (https://www.unmannedsystemstechnology.com/2021/06/new-line-of-cable-penetrators-for-underwater-robotics-introduced/ ve https://www.marinetechnologynews.com/amp/news/robotics-expands-wetlink-615566 , arama özeti).
- **İticilerin manyetometreye etkisi.** Forum deneyimi: T100 çalışırken 3-4 inç uzaktaki LSM303'ü bozmuş, 5 inç uzakta düzelmiş; yazar kaynağın motordan çok yüksek akımlı kablo/batarya olduğundan şüpheleniyor; gürültü pusulayı gaz oranına bağlı kaydırıyor. Resmî bir asgari mesafe bulunamadı (https://discuss.bluerobotics.com/t/frustrated-any-suggestions/305 , https://discuss.bluerobotics.com/t/deviation-in-compass/12174). **Sonuç:** manyetometreyi yüksek akımlı kablolardan ve iticilerden uzak/harici tut; EKF'de gaz-ölçekli sapma kompanzasyonu (ArduPilot'ta "motor compensation" benzeri) düşün; 00 belgesindeki ESC/itici gerçek davranışı M22/M23 ölçümlerine bağlı.
- **Topraklama ve gürültü.** Güç ve mantık topraklarını tek noktada birleştir; ESC PWM kablolarını kısa/bükülü tut; ADC referansını ve sensör beslemesini LDO ile ayır; 1,8 V kartın VDDA/VREF sınırları için 00 belgesi 01.x hükümlerine bak. Bu bölüm genel mühendislik pratiğidir, kaynak verilmemiştir.

---

## 3. Mimari için öneriler

### 3.1 Ardıl kart kararı

1. **Aşama 2 bench ardılı: NUCLEO-H753ZI.** Gerekçe: aktif ürün, ST'nin resmî H743 ardılı; çift duyarlıklı FPU ile 06 raporundaki f32/f64 tartışmasını (00 belgesi 06.6) kaldırır; Ethernet ile Oculus/DVL/Ping360-UDP sınıfı sensörlerin kablo seviyesi emülasyonu ve PC ile yüksek bant genişlikli HIL bağlantısı mümkün olur; 2 FDCAN ile CAN gereksinimi (A4) test edilebilir; ST HAL/CubeMX/CubeIDE akışı U5 ile aynı (C kararına uygun); fiyat düşük. Zayıf yön: çıplak dev kit, 3,3 V kart ayarı doğrulanmadı, CAN alıcı-verici ve seviye hâlâ ek donanım.
2. **Yedek:** NUCLEO-H723ZG (550 MHz; ancak teslim süresi riski). U5G9J-DK1 yalnızca mevcut BSP'nin **birebir yedeği** olarak alınabilir; ardıl değildir.
3. **Aşama 3 araç kartı: Pixhawk 6X (öncelik) veya özel taşıyıcı.** 6X: H753 (aynı MCU ailesi, Nucleo ile kod taşınabilirliği yüksek), 3 IMU, Ethernet, 2 CAN, 16 PWM, standart konnektörler, ısı kontrollü/yalıtılı sensör yapısı. Karşılığında: kendi C firmware'imiz için BSP yazımı (hwdef/PX4 kart dosyalarından pin haritası), bootloader/yükleme akışı (resmî destek PX4/ArduPilot için) ve donanım sürümü (Rev) değişkenliği; 6X'te sensör çipleri sürümle değişiyor (Rev 8: ICM-45686). Çip sürücüleri bu yüzden ICM-42688-P'ye değil "imu_if" arayüzüne bağlanmalı.
4. **Pixhawk 6C**, bütçe alternatifi (Ethernet yok).

### 3.2 Sürücü soyutlama sınırı (somut)

```
çekirdek (C17, kart/HIL bağımsız)  <- yalnızca sensör/aktüatör arayüzleri (imu_if, baro_if, mag_if, range_if, dvl_if, gnss_if, esc_if, gpio_if)
çip sürücüleri (ortak C)           <- bus HAL (spi/i2c/uart/gpio/pwm/can/time) üzerinden
kart HAL + BSP (kart başına)       <- bsp_u5a9j_dk.c, bsp_nucleo_h753.c, bsp_pixhawk6x.c; pin, saat, DMA, seviye notları
HIL taşıma (USB/UART/Ethernet)     <- ayrı modül; "sim" sürücüleri aynı arayüzü uygular
```

- Sınır testi: kart değişiminde yalnızca `bsp_*` ve bus HAL dosyaları değişmeli; değişiyorsa çekirdek kart bilgisi taşıyordur.
- Her çip sürücüsü yalnızca `bus_id`/CS ve hız bilgisini BSP'den alır (ArduPilot `bus_id`, PX4 cihaz kimliği desenleri).
- Her ölçüm paketi `t_sample` (zaman damgası), `valid`, `sequence`, `fault_flags` taşısın (00 belgesi HIL enjeksiyon sınırı).
- Üç HIL modu aynı arayüz altında: `sim_inject` (mesaj), `bus_emulated` (gerçek bus, emülatör MCU), `real` (gerçek sensör).
- Seviye/gerilim bilgisi koda değil BSP belgesine ve donanım envanterine yazılır.

### 3.3 Sensör seçimi (sürücü soyutlamasını sağlamlaştıran)

- IMU: ICM-42688-P (05 belgesinde zaten modelli). Pixhawk 6X'e geçildiğinde ICM-45686'ya geçiş için `imu_if` yeterli olmalı; ayrı sürücü gerekir.
- Basınç: Bar30 (MS5837, I2C 0x76, tek sensör/veri yolu).
- Sonar: Ping1D (UART TTL) en basit; Ping360 RS485 iç konnektör taşıması gerektirir, Ethernet UDP seçenek.
- Pusula: harici, iticilerden uzak; I2C.

### 3.4 ESC/aktüatör

- Başlangıç: servo PWM (1100-1900 µs). `esc_if` arayüzü komutu [-1,1] olarak alır, PWM hesabını sürücü yapar.
- DShot ve CAN (DroneCAN) ileride eklenecek ayrı `esc_if` uygulamalarıdır; BLHeli_S ESC'lerde DShot desteği ürün bazında doğrulanmalıdır.
- Acil durdurma yazılımdan bağımsız güç kesmesi olmalı; yazılım yalnızca durumunu okur.

### 3.5 Yardımcı bilgisayar

- Başlangıç: Pi 5 (düşük güç, kolay). Jetson ancak ölçülen model gereksinimi bunu gerektiriyorsa.
- Bağlantı: UART+MAVLink (düşük hız), H7 kartlarda Ethernet.
- Isı: alüminyum uç kapağa ısı iletimli montaj, güç sınırlama; havuz testinde SoC sıcaklığı kaydedilmeli.

### 3.6 Kademeli tedarik yol haritası

| Aşama | Alınacaklar | Yaklaşık maliyet | Yazılımda doğrulanan |
|---|---|---|---|
| **0** Yalnızca DK, tam simülasyon | Yok (isteğe bağlı: 1,8 V FTDI kablo, yedek DK1) | ~0; FTDI 1,8 V kablo ve DK1 fiyatı bu oturumda doğrulanmadı | Çekirdek algoritma, USB HIL taşıma, sensör/aktüatör arayüzleri, hata enjeksiyonu, zaman/lockstep, BSP/HAL sınırı |
| **1** Masa üstü sensör | ICM-42688-P (ya da BMI088) breakout; Bar30 ($80-90, Blue Robotics sayfası arama özeti); PCA9306/TXB seviye çeviriciler; RP2040 Pico emülatör; 1,8 V FTDI kablo; mantık analizörü | Bar30 $80-90 doğrulandı (arama özeti); diğerleri tahmini ~$50-150, **doğrulanmadı** | Gerçek SPI/I2C edinimi, DMA, FIFO/kesme, MS5837 durum makinesi + 18,08 ms bekleme, CRC, seviye çevirme; bus-emüle HIL |
| **2** Ardıl kart | NUCLEO-H753ZI (+ CAN alıcı-verici modülü, Ethernet kablosu); isteğe bağlı W5500 modülü | Kart ~£21-€42 (anlık görüntü); alıcı-verici ~$5-15 **doğrulanmadı** | Çift duyarlıklı FPU, Ethernet HIL (Oculus/DVL/Ping360-UDP), CAN, BSP değişimi sonrası "yalnızca BSP değişir" kanıtı |
| **3** Su testi | Pixhawk 6X (veya özel taşıyıcı), T200 + Basic ESC, batarya + BMS, güç dağıtımı + sigorta, WetLink penetratörleri, Ping1D ($430, Blue Robotics sayfası), Bar30, acil durdurma, SOS sızıntı sensörü, harici pusula; isteğe bağlı Pi 5 / Jetson | 6X $270-$310+ (bayi anlık); Ping1D $344-430; Ping360 $2200-$2750; DVL A50 fiyatı **doğrulanmadı**; itici/ESC/batarya/penetratör toplamı **doğrulanmadı** | Gerçek ESC/itici modeli (M22/M23), güç çökmesi/BOR davranışı, ısıl davranış, manyetometre kirliliği, sızıntı ve acil durdurma, tam kontrol döngüsü |

Not: Aşama 1'de sensör emülatörü ile gerçek sensör **aynı** `imu_if/baro_if` arayüzünden geçmeli; böylece Aşama 2/3'te çekirdek test seti yeniden kullanılır.

---

## 4. Riskler ve açık sorular

1. **DK1'in 1,8 V ve Ethernet durumu doğrulanmadı.** ST kart sayfası ve DB4620 bu oturumda çekilemedi; "DK1 de 1,8 V" iddiası arama özetine ve aynı kılavuza dayanıyor.
2. **H753/H723 Nucleo üzerinde G/Ç gerilimi, SB (lehim köprüsü) ve pin tahsisi doğrulanmadı;** Ethernet pinleri ile CAN/UART/SPI pinlerinin çakışması olabilir (Zephyr H743 sayfası Ethernet pinlerini listeler).
3. **Pixhawk 6X/6C üzerinde özel C firmware** için resmî destek yok; bootloader, kilit bitleri, IO işlemcisi (F103/F100) rolü, güç yönetimi, sensör besleme sıralaması belgelenmiş olabilir ama bu oturumda incelenmedi. Risk: BSP yazımı beklenenden uzun sürer.
4. **Pixhawk PWM/UART sinyal seviyesi** sayfalarda belirtilmedi; 3,3 V varsayımı doğrulanmadı.
5. **Fiyat/stok verileri** tarihsiz ve bölgesel; H723ZG teslim süresi 99 hafta, H753ZI bir dağıtıcıda 0 stok. Karar vermeden önce güncel doğrulama.
6. **Blue Robotics ESC'lerinde DShot ve 3,3 V sinyal seviyesi** resmî olarak doğrulanamadı.
7. **Ping360 RS485 baud/zamanlama** ve Ping1D güncelleme hızı bilinmiyor.
8. **BMI088 SPI hız sınırı** doğrulanmadı; ICM-42688-P birincil kaynak TDK DS-000347 v1.8 (2023-07-27) ile kontrol edilmeli (05 belgesi rev 1.6 kullanmıştı).
9. **RP2040 ile I2C/SPI slave emülasyonu** pratikte hız ve zamanlama bakımından yeterli mi? Doğrulanmadı.
10. **Isı:** Jetson veya Pi 5'in kapalı muhafazadaki davranışı için yayımlanmış veri yok; ölçüm gerekli.
11. **Elektrik güvenliği:** sigorta/kesici seçimi, kısa devre akımı, bataryanın (Li-ion) güvenliği ve acil durdurma devresi için bu oturumda kaynak bulunamadı. Gerçek güç eklemeden önce ayrı araştırma (ABYC/ilgili standartlar) gerekli.
12. **Açık soru (kullanıcıya):** Gerçek CAN gereksinimi var mı (A4)? Araç içi ana bilgisayar Pi mi Jetson mu? Bütçe aralığı nedir?

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **STM32U5G9J-DK1 ve STM32U5A9J-DK aynı kılavuzu (UM2967) paylaşır, DK1 aktif ve G/Ç gerilimi 1,8 V'tur; Ethernet yoktur.** Kaynak: https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf , https://www.st.com/en/evaluation-tools/stm32u5g9j-dk1.html .
2. **NUCLEO-H743ZI obsolete, H743ZI2 NRND/obsolete; NUCLEO-H753ZI ve NUCLEO-H723ZG aktif.** Kaynak: https://www.st.com/en/evaluation-tools/nucleo-h743zi.html , https://www.st.com/en/evaluation-tools/nucleo-h723zg.html , https://docs.zephyrproject.org/latest/boards/st/nucleo_h743zi/doc/index.html .
3. **STM32H753/H723 çift duyarlıklı FPU içerir;** H7 Nucleo 144'te 2 FDCAN (alıcı-verici harici), Ethernet RJ45 var. Kaynak: https://www.st.com/en/product/stm32h743zi , https://docs.zephyrproject.org/latest/boards/st/nucleo_h723zg/doc/index.html .
4. **Pixhawk 6X: STM32H753, 3x ICM-45686 (Rev 8), 100 Mbps Ethernet, 2 CAN, 16 PWM; 6C: STM32H743, 2 CAN, 16 PWM, Ethernet yok.** Kaynak: https://docs.px4.io/main/en/flight_controller/pixhawk6x.html , https://docs.px4.io/main/en/flight_controller/pixhawk6c.html .
5. **ICM-42688-P VDD ve VDDIO 1,71-3,6 V; SPI 24 MHz.** Kaynak: TDK DS-000347 (https://invensense.tdk.com/?p=9240). **BMI088: VDD 2,4-3,6 V, VDDIO 1,2-3,6 V, ayrı CS.** Kaynak: https://www.bosch-sensortec.com/products/motion-sensors/imus/bmi088/ .
6. **Bar30: yalnızca I2C, adres 0x76, I2C mantığı 3,3 V, besleme 2,5-5,5 V; MS5837 OSR 8192 azami dönüşüm 18,08 ms.** Kaynak: https://bluerobotics.com/?p=227172 , https://www.farnell.com/datasheets/2917217.pdf .
7. **Ping sonar: UART TTL 3,3 V (5 V toleranslı), 4,5-5,5 V besleme, 115200/9600; Ping360: 11-25 V, 5 W, USB/RS485/Ethernet UDP.** Kaynak: https://bluerobotics.com/store/sonars/echosounders/ping-sonar-r2-rp/ , https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/ .
8. **DVL A50: 3,3 V UART (5 V toleranslı) 115200 8N1; 10-30 V; açılışta UART'ta <10 µs ~5 V sıçrama.** Kaynak: https://waterlinked.github.io/dvl/interfaces , https://www.blueyerobotics.com/specsheet/WL-DVL-A50/pdf .
9. **Blue Robotics ESC: 1100/1500/1900 µs PWM, BlueESC PWM sinyali 3,3-5 V, 6-22 V.** Kaynak: https://blueos.cloud/docs/1.2/integrations/hardware/required/esc/ , https://docs.bluerobotics.com/bluesc/ .
10. **FTDI FT232R VCCIO 1,8-5,25 V (1,8 V kablo mevcut); FT232H 3,3 V G/Ç; PCA9306 VREF1 1,2-3,3 V / VREF2 1,8-5,5 V; TXB0108 I2C için uygun değil.** Kaynak: https://www.ftdichip.com (FT232R/FT232H datasheet), https://www.ti.com/product/pca9306 , https://www.ti.com/product/txb0102 .

---

## 6. Kaynaklar

ST / kart:
- https://www.st.com/en/evaluation-tools/stm32u5g9j-dk1.html (çekilemedi; arama özetleri)
- https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf (çekilemedi)
- https://docs.zephyrproject.org/latest/boards/st/stm32u5g9j_dk1/doc/index.html
- https://www.mouser.in/en/new/stmicroelectronics/st-stm32u5g9j-dk1-discovery-kit/ (çekilemedi; arama özeti)
- https://docs.zephyrproject.org/latest/boards/st/nucleo_h743zi/doc/index.html
- https://docs.zephyrproject.org/latest/boards/st/nucleo_h723zg/doc/index.html
- https://www.st.com/en/evaluation-tools/nucleo-h723zg.html (arama özeti)
- https://community.st.com/stm32-mcus-boards-and-hardware-tools-26/nucleo-h743zi2-nrnd-and-use-nucleo-h753zi-134211
- https://www.digikey.co.uk/en/products/detail/stmicroelectronics/NUCLEO-H753ZI/21348937 ve bayi sayfaları (fiyat/stok anlık görüntüleri, tarihsiz)
- https://www.st.com/en/product/stm32h743zi
- https://community.st.com/stm32-mcus-boards-and-hardware-tools-26/is-it-possible-to-configure-gpio-on-the-stm32u5a9j-dk-for-3-3v-output-126333

Pixhawk / ArduPilot / PX4 / Zephyr:
- https://docs.px4.io/main/en/flight_controller/pixhawk6c.html
- https://docs.px4.io/main/en/flight_controller/pixhawk6x.html
- https://holybro.com/products/pixhawk-6x (fiyat; arama özeti)
- https://ardupilot.org/dev/docs/porting.html
- https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/AP_HAL/AP_HAL.h
- https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/AP_HAL/Device.h
- https://docs.px4.io/main/en/middleware/drivers.html
- https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html
- https://docs.zephyrproject.org/latest/build/dts/api/bindings/sensor/invensense,icm42688.html
- https://blueos.cloud/docs/hardware/required/flight-controller/ (arama özeti)

Sensörler/aktüatörler:
- https://invensense.tdk.com/?p=9240 (ICM-42688-P)
- https://www.bosch-sensortec.com/products/motion-sensors/imus/bmi088/
- https://bluerobotics.com/?p=227172 (Bar30/Bar02)
- https://www.farnell.com/datasheets/2917217.pdf (MS5837-30BA)
- https://docs.bluerobotics.com/ping-protocol/ ; https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/
- https://bluerobotics.com/store/sonars/echosounders/ping-sonar-r2-rp/
- https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/
- https://bluerobotics.com/?p=120410 (Ping360 arayüz değişimi; arama özeti)
- https://waterlinked.github.io/dvl/interfaces ; https://www.blueyerobotics.com/specsheet/WL-DVL-A50/pdf
- https://docs.sparkfun.com/SparkFun_u-blox_MAX-M10S/hardware_overview/ ; https://content.u-blox.com/sites/default/files/MAX-M10S_DataSheet_UBX-20035208.pdf
- https://blueos.cloud/docs/1.2/integrations/hardware/required/esc/ ; https://docs.bluerobotics.com/bluesc/
- https://ardupilot.org/copter/docs/common-dshot-escs.html (arama özeti)
- https://bluerobotics.com/?p=11612 ; https://claude.wp.bluerobotics.com/?p=113143 (SOS sızıntı)

Yardımcı bilgisayar / HIL / elektrik:
- https://jetsonhacks.com/2024/12/17/jetson-orin-nano-super-developer-kit/
- https://www.cnx-software.com/2024/12/18/249-nvidia-jetson-orin-nano-super-developer-kit-targets-generative-ai-applications-at-the-edge/
- https://raspberry.tips/en/faq/raspberry-pi-power-consumption-update-2026-all-models-compared
- https://www.tomshardware.com/reviews/raspberry-pi-5
- https://discuss.bluerobotics.com/t/heat-from-electronic-parts-inside-watertight-enclosure/168 ; https://bluerobotics.com/?p=15815
- https://uk.rs-online.com/web/p/products/7158525 (FTDI 1,8 V kablo)
- https://www.ti.com/product/pca9306 ; https://www.ti.com/product/txb0102
- https://discuss.bluerobotics.com/t/frustrated-any-suggestions/305 ; https://discuss.bluerobotics.com/t/deviation-in-compass/12174
- https://bluerobotics.com/?p=142189 ; https://www.marinetechnologynews.com/amp/news/robotics-expands-wetlink-615566
- RP2040 datasheet özeti: https://www.radiolocman.com/datasheet/pdf.html?di=173443 (arama özeti)

Depo içi: `AGENTS.md`, `docs/research/00-duzeltmeler-ve-kararlar.md`, `docs/research/01-kart-ve-arac-zinciri.md`, `docs/research/dogrulama/01-codex.md`, `docs/research/05-sensor-modelleme.md`, `docs/research/08-kart-c-yigini.md`, `docs/research/11-haberlesme-ve-guc.md`.
