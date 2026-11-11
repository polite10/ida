# 08 - Kartta C Yazılım Yığını (STM32U5A9J-DK, C11/C17, CMake, USB HS, RTOS seçimi, taşınabilir çekirdek)

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Doğrulaması: [dogrulama/08-codex.md](dogrulama/08-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Tarih: 2026-11-11. Araştırma makinesi: macOS 26.5.2 (Apple Silicon). Windows tarafı bu makinede denenmedi; Windows satırları kaynak okumasına dayanır. Kart bağlı değildi; hiçbir donanım ölçümü yapılmadı. Hiçbir şey kurulmadı.

Önceki belgelerle ilişki: `01-kart-ve-arac-zinciri.md` ve `dogrulama/01-codex.md` içindeki kart/pin/güç bulguları (1,8 V, tek FDCAN, PD0/PD1, V3E VCP 15 MHz, errata) yeniden araştırılmadı; çeliştiği yerde Codex doğrulaması esas alındı. Bu rapor yalnızca C yolunun eksiklerini ekler.

Kaynak etiketleri `[K..]` biçiminde; tam URL'ler Bölüm 6'dadır. "Doğrulanmadı" ve "Mühendislik değerlendirmesi" etiketleri bilinçli kullanılmıştır.

---

## 1. Özet

1. **USB HS CDC için ST'nin kendi referansı var, ama bu kart için değil.** Cube U5 v1.9.0'da `Ux_Device_CDC_ACM` yalnızca NUCLEO-U5A5ZJ-Q ve STM32U5G9J-DK2 için var. `Projects/STM32U5x9J-DK/Applications/USBX` altında yalnızca üç host örneği var. Nucleo örneği HS + gömülü PHY (`PCD_SPEED_HIGH`, `USB_OTG_HS_EMBEDDED_PHY`), VBUS algılama kapalı, HSE tabanlı PHY saati ile çalışıyor [K1][K2][K3]. Bu, kart için doğrulanmış bir HS CDC kanıtı değildir.
2. **PHY ayarı (COMPDISTUNE/SQRXTUNE) konusu C tarafında da açık.** ST HAL'ında `HAL_SYSCFG_SetOTGPHYDisconnectThreshold/SquelchThreshold/PreemphasisCurrent` var ve başlıkta "önerilen değer" olarak +%5,9 ve +%15 yazıyor [K4]. Ancak ST'nin Nucleo-U5A5ZJ CDC örneği bu işlevleri çağırmıyor [K2]. Yani ST örneği ayarsız çalışıyor olabilir ya da bu örneğin HS'de kararlılığı kanıtsız. Kartta iki varyantı (ayarlı/ayarsız) denemek kabul testine eklenmeli.
3. **TinyUSB 0.21.0 (2026-06-30) STM32U5 için `dwc2` sürücüsüyle destek veriyor; U5A5 Nucleo BSP'si var, U5A9J-DK BSP'si yok.** Nucleo BSP'si ST'nin örneğiyle aynı saat dizisini kullanıyor (HSE 16 MHz, PLL, `RCC_USBPHYCLKSOURCE_HSE`, `SYSCFG_OTG_HS_PHY_CLK_SELECT_1`, SMPS) [K5]. Kartımız için küçük bir board dosyası yazmak yeterli görünüyor (Mühendislik değerlendirmesi). CherryUSB kodunda U5A9xx tanımı geçiyor [K6]; ancak kart/örnek kanıtı yok.
4. **ThreadX lisansı kritik ayrım içeriyor.** Upstream Eclipse ThreadX MIT. ST'nin `stm32-mw-threadx` çatalı ise hâlâ "Microsoft Azure RTOS" lisansı altında; üretim/dağıtım hakkı yalnızca `LICENSED-HARDWARE.txt` içinde listelenen (STMicroelectronics) donanım için geçerli [K7][K8]. Kart ileride değişecek olduğundan ST paketindeki ThreadX'e bağımlılık lisans riski taşır; upstream MIT kullanılabilir.
5. **Zephyr bu kartı "Maintained" listeliyor ama `supported` listesinde CAN, PWM ve USB yok** (DTS düğümleri var: `st,stm32-fdcan`, `usbotg_hs`, `otghs_phy`). Nucleo-U5A5ZJ-Q'nun listesinde `can` ve `usb_device` var [K9][K10]. Zephyr bu proje için ağır ve sonraki kart değişimi için bağımlılık getirir; birincil yol olarak önerilmiyor.
6. **Önerilen çalışma modeli: RTOS'suz, kesme tetiklemeli deterministik döngü** (400 Hz kontrol adımı bir timer kesmesinde, USB/haberleşme ana döngüde), RTOS isteğe bağlı ve ancak somut ihtiyaç doğarsa FreeRTOS (MIT, `ARM_CM33_NTZ` portu var). Atış emniyeti yazılım zamanlayıcısına değil donanım kapılamasına ve bağımsız watchdog'a dayanmalı. Bu bir mühendislik değerlendirmesidir; kartta ölçülmeli.
7. **Proje düzeni: CubeMX'i üst düzey CMake sahibi yapma.** CubeMX 6.14'te üretilen CMake `CMAKE_SOURCE_DIR`/`CMAKE_PROJECT_NAME` kullanıyor ve alt dizin olarak kullanılamıyor; `cmake/stm32cubemx/CMakeLists.txt` her üretimde ezilir; 6.15'ten beri toolchain yalnızca `CMakePresets.cmake`'te [K11][K12][K13]. Öneri: HAL/CMSIS depolarını (ST'nin ayrı GitHub depoları) sürüm sabitli alt modül olarak alıp kendi CMake'imizle derlemek; CubeMX yalnızca pin/saat doğrulama referansı.
8. **Kart başlangıç saati belgeli:** Nucleo örneği HSE 16 MHz, M=1, N=20, R=2 → 160 MHz; DK host örneği M=4, N=80, R=2 → 160 MHz, VOS Scale 1 ve `PWR_SMPS_SUPPLY` [K2][K14]. TrustZone: ST'ye göre U5'te varsayılan TZEN=0 [K15]; bu kartın fabrika durumu doğrulanmadı.
9. **Taşınabilir çekirdek için sayısal tutarlılık riski var:** M33'te yalnızca tek duyarlıklı FPU; PC'de aynı `float` kodu farklı FMA kaynaştırması ve `libm` farkı verebilir. `-ffp-contract=off`, `float`-only ve tolerans tabanlı SIL/HIL karşılaştırması gerekir (Mühendislik değerlendirmesi).
10. **MISRA iddiasından kaçının, "proje alt kümesi" tanımlayın.** cppcheck'in açık kaynak MISRA eklentisi kural metinlerini içermiyor ve MISRA C:2012'yi tarif ediyor [K16]. Derleyici uyarıları + clang-tidy + cppcheck + GCC `-fanalyzer` ile kendi kural listemizi zorunlu kılmak daha dürüst.

---

## 2. Bulgular

### 2.1 Proje düzeni ve araç zinciri

**Bu makinede salt okunur kontrol (macOS arm64):**

| Araç | Durum |
|---|---|
| arm-none-eabi-gcc, cmake, ninja, openocd, cppcheck, clang-tidy, west | PATH'te yok |
| STM32CubeIDE 2.2.0 | Var; içinde arm64 `gnu-tools-for-stm32.14.3.rel1`, `cmake`, `ninja`, `make`, `stlink-gdb-server`, `openocd` eklentileri (plugins dizininde dosya adları görüldü) |
| STM32CubeProgrammer | Var, `STM32_Programmer_CLI --version` = 2.23.0 |
| Homebrew | Formüller mevcut: arm-none-eabi-gcc 16.2.0, cppcheck 2.22.0, cmake 4.4.5, ninja 1.13.2, open-ocd 0.12.0, llvm 23.1.5; cask `gcc-arm-embedded` 15.3.rel2 (`brew info`, kurulum yapılmadı) |
| Python 3.12.4, git, lldb (Apple) | Var |

**CubeCLT:** 1.21.0 (2026-02) Apple Silicon yerel desteği getirdi; paket CubeProgrammer 2.22.0, ST-LINK GDB server 7.13.0 içeriyor; 1.21.0 bilinen sorunu: `ST-LINK_gdbserver` çalıştırılırken CubeProgrammer `bin` yolu argümanla verilmeli [K17][K18]. Bu raporun araması 1.22.0 sürüm notunun varlığına işaret etti ama içeriği okunamadı (zaman aşımı); bu yüzden 1.22.0 içeriği doğrulanmadı. ST (2026-04-03) "bundle manager CubeCLT'nin yerini alacak" diyor; tarih yok, CubeCLT henüz kullanımdan kalkmadı [K19]. Çıkarım: CLI akışı ilerde değişebilir; araç sürümlerini sabitleyip yerel bir arşivde saklamak gerekir.

**CubeMX CMake çıktısı ve yeniden üretim:**

| Dosya | Yeniden üretimde | Kaynak |
|---|---|---|
| `./CMakeLists.txt` (üst düzey) | Bir kez üretilir, kullanıcı değişikliği korunur | [K12][K20] |
| `cmake/stm32cubemx/CMakeLists.txt` | Her üretimde ezilir | [K12] |
| `CMakePresets.json`/`CMakePresets.cmake`, toolchain `.cmake` | Kullanıcı düzenleyebilir; 6.15'ten beri toolchain yalnızca preset tarafında | [K13][K20] |
| `Core/Src` içindeki `USER CODE` blokları | Korunur; bloklar dışındaki kod ezilir | genel CubeMX davranışı, bu oturumda ayrıca doğrulanmadı |

Bilinen sorunlar: 6.14.xx'te üretilen CMake `CMAKE_SOURCE_DIR` kullandığı için bir üst projenin alt dizini olamıyor (ST iç bilet 206627/209315, çözüm gösterilmedi) [K11]; 6.14.0'da USBX device controller dosyaları CMake'e girmiyordu, ST yanıtı: CubeMX'te "Device Controllers"ın seçilmesi gerekiyor [K21]; 6.15'te toolchain tanımı CMakeLists.txt'ten kalktığı için CLion'da "compiler broken" [K13]. ST'nin önerdiği özelleştirme: `target_compile_definitions/target_include_directories(stm32cubemx INTERFACE ...)` ve üretilen kaynakları `list(REMOVE_ITEM)` ile ayıklayıp kendi sürümlerini koymak [K20].

**Önerilen düzen (Mühendislik değerlendirmesi):**

```
repo/
  CMakePresets.json          # host-debug, host-asan, fw-debug, fw-release (Ninja)
  cmake/toolchains/arm-none-eabi-cm33.cmake
  core/                      # taşınabilir C: estimator, guidance, control, mission, fire_ctl, proto
  platform/
    host/                    # PC sürücüleri (SIL): soket, zaman, log
    stm32u5/                 # HAL ince sarmalayıcı: usb, uart, can, pwm, clock, wdg
  third_party/               # alt modül (sürüm sabit): stm32u5xx_hal_driver, cmsis_device_u5, CMSIS_6, CMSIS-DSP, tinyusb
  board/stm32u5a9j_dk/       # pin tablosu, saat, linker, startup (Cube'tan bir kez kopyalanıp sahiplenilmiş)
  tools/cubemx/              # .ioc: yalnızca pin/saat doğrulama referansı, derlemeye girmez
  tests/  tools/codegen/
```

Bu düzende CubeMX yeniden üretiminin ezeceği bir şey yoktur; bedeli, başlangıç/linker/sistem dosyalarını ve HAL yapılandırmasını (`stm32u5xx_hal_conf.h`) elle sahiplenmektir. TinyUSB'nin kendi STM32U5 BSP'si de aynı yöntemi kullanıyor (ST HAL + CMSIS device alt modülleri, kendi CMake'i) [K5]. Cube U5 v1.9.0 da HAL, CMSIS device ve BSP'yi ayrı GitHub depolarından alt modül olarak çekiyor [K22].

**Derleme, yükleme, hata ayıklama:**

| İş | Komut satırı yolu | Notlar |
|---|---|---|
| Derleme | `cmake --preset fw-release && cmake --build --preset fw-release` (Ninja) | Toolchain: ST'nin GNU Tools for STM32 14.3.rel1 (CubeCLT/CubeIDE içinde) ya da Arm GNU; ilkini sabitlemek makul çünkü ST örnekleri ve CubeCLT onu kullanıyor |
| Yükleme | `STM32_Programmer_CLI -c port=SWD -w build.elf -v -rst` (genel kullanım; bu oturumda kartla denenmedi) | macOS ve Windows'ta aynı araç; Zephyr kart tanımı da `--port=swd --reset-mode=hw` ile CubeProgrammer çalıştırıcısını kullanıyor [K23] |
| Hata ayıklama | `ST-LINK_gdbserver` (CubeCLT/CubeIDE) + `arm-none-eabi-gdb` | 1.21.0'da `-cp <CubeProgrammer/bin>` gerekir [K18] |
| Alternatif | OpenOCD: Zephyr kartı için `stlink-dap`, `dapdirect_swd`, `HW reset`, `CLOCK_FREQ 8000`, `STOP_WATCHDOG 1` ile yapılandırıyor [K23]; OpenOCD v0.12.0 etiketinde `stm32u5x.cfg` var ve master'da flash sürücüsü `STM32U59/U5Axx` kimliğini tanıyor [K24]; v0.12.0'ın U5A flash sürücüsü destek düzeyi doğrulanmadı | Homebrew `open-ocd` 0.12.0 |
| VS Code | CMake Tools + Cortex-Debug (ya da ST'nin "STM32CubeIDE for VS Code") eklentisi; CubeMX'in ürettiği preset'lerle Ninja çökmesi topluluk raporu | VS Code akışının macOS/Windows'ta uçtan uca denemesi yapılmadı |

Windows: ST-LINK sürücü ihtiyacı belgelerde çelişiyor (Codex doğrulaması: ST RN0093 "Windows 10'dan itibaren kurulum gerekmez", probe-rs belgesi ST sürücüsü istiyor); C yolunda CubeProgrammer/gdbserver ST'nin kendi USB erişimini kullanır, bu yüzden risk daha düşük ama Windows'ta denenmedi.

### 2.2 Yüksek hızlı USB cihaz (CDC) yığınları

| Yığın | Lisans / durum | U5A9 HS PHY desteği ve kanıt | Bu kart / kardeş kartlar için çalışan örnek | Not |
|---|---|---|---|---|
| ST HAL PCD + **USBX** (ThreadX'li ya da standalone) | ST paketi: Microsoft Azure RTOS lisansı [K7]; upstream USBX MIT [K25] | Nucleo-U5A5ZJ-Q ve U5G9J-DK2 örnekleri `Ux_Device_CDC_ACM` var [K1]; HS + gömülü PHY [K2] | U5A9J-DK: yalnızca host örnekleri [K1]; forumda DK1 üzerinde standalone USBX MSC ve eski sürücüyle CDC'nin VBUS algılama olmadan çalıştığı bildirilmiş [K26] | ThreadX ucu: örnek `tx_application_define` ve ThreadX yığın notları gerektiriyor; CubeMX USBX'i destekliyor, klasik USB Device ara katmanını U5 için değil [K27] |
| **Klasik STM32 USB Device kütüphanesi** | ST ara katman | U5 için CubeMX klasik ara katmanı sunmuyor (ST moderatör: U5 yalnızca USBX) [K27]; forumda kullanıcı eski sürücüyle CDC çalıştırdığını söylüyor [K26] | Doğrulanmadı | Önerilmez (ST'nin yönü USBX) |
| **TinyUSB** 0.21.0 | MIT, OS'siz/FreeRTOS destekli | `dwc2` + STM32U5 BSP; U5A5 Nucleo BSP'si (HS PHY saati, SMPS, VBUS algılama) [K5] | U5A9J-DK BSP'si yok; BSP listesi: b_u585i_iot2a, u545nucleo, u575eval, u575nucleo, u5a5nucleo [K5] | CI'da donanımda HS testi yapıldığı bu oturumda doğrulanmadı |
| **CherryUSB** v1.6.1 | Açık kaynak; `usb_glue_st.c` içinde STM32U595/U5A5/U599/U5A9/U5F/U5G tanımı geçiyor [K6] | Kod tabanında tanım var | Kart örneği bulunamadı | Dokümantasyon ST'yi dwc2/fsdev IP ile destekli sayıyor; U5 örnek kanıtı yok |

**PHY ayar parametreleri (kaynaklı):**
- HSE 16 MHz ve `RCC_USBPHYCLKSOURCE_HSE`; `HAL_SYSCFG_SetOTGPHYReferenceClockSelection(SYSCFG_OTG_HS_PHY_CLK_SELECT_1)` (16 MHz) [K2][K5].
- `__HAL_RCC_USB_OTG_HS_CLK_ENABLE()`, `__HAL_RCC_USBPHYC_CLK_ENABLE()`, `HAL_PWREx_EnableVddUSB()`, `HAL_PWREx_EnableUSBHSTranceiverSupply()`, `HAL_SYSCFG_EnableOTGPHY(SYSCFG_OTG_HS_PHY_ENABLE)` [K2][K5].
- Ayar kayıtları `SYSCFG->OTGHSPHYTUNER2`: `COMPDISTUNE` (bit 0-2; HAL önerisi +%5,9 = `…_COMPDISTUNE_1`), `SQRXTUNE` (bit 4-6; HAL önerisi +%15 = 0), `TXPREEMPAMPTUNE` (bit 13-14) [K4][K28]. RM0456 §15.2.3 bu ayarları anlatıyor (arama özetinden; sayfanın kendisi okunamadı).
- HAL_PCD: `dev_endpoints=9`, `speed=PCD_SPEED_HIGH`, FIFO ayarı elle: `HAL_PCDEx_SetRxFiFo(…,0x200)` ve TX FIFO'lar `…_HS_MPS/4` [K2]. CubeMX yalnızca `MX_USB_OTG_HS_PCD_Init()` üretir; FIFO, USBX bağlama ve `HAL_PCD_Start()` uygulamaya aittir (ST moderatör) [K29].
- `vbus_sensing_enable` ENABLE iken U5A9J-DK'da kesme tetiklenmedi, DISABLE yapınca tetiklendi (kullanıcı raporu; kök neden kanıtlanmadı; şema VBUS_SENSE'i PG1'e gösteriyor) [K29]. TinyUSB U5A5 BSP'si VBUS için ADC tabanlı özel mantık ve `BVALOEN/BVALOVAL` geçersiz kılma kullanıyor [K5].

**Bilinen hatalar/uyarılar:** `HAL_PCD_Init` içinde `USB_CoreReset` zaman aşımı raporları (U5A5/U595; kullanıcı forum iletileri) [K30]; ST örneklerinde ThreadX kullanımı SysTick'i ThreadX'e verdiği için HAL zaman tabanının ayrı TIM olması zorunlu [K3]; Windows'ta HS bulk için MPS 512 gerekir (01 raporundan; Embassy tarafında karşılaşılan sorun, USB standardı gereği genel).

**Değerlendirme (Mühendislik değerlendirmesi):** Birincil aday TinyUSB (OS'siz, MIT, kart değişiminde BSP yeniden yazımı küçük). ST'nin Nucleo-U5A5ZJ USBX örneği "bilinen iyi PHY bring-up" referansı olarak aynı karta (gerekirse HAL taban katmanıyla) çalıştırılmalı: eğer o dizi bu kartta HS'ye çıkıyorsa, TinyUSB board dosyası aynı diziyi taklit eder. Yedek: HAL PCD + upstream (MIT) USBX standalone.

### 2.3 Gerçek zamanlı yapı

| Seçenek | Durum (2026-11) | Lisans | U5A9J-DK desteği | 400 Hz kontrol için uygunluk | Güçlü yan | Zayıf yan |
|---|---|---|---|---|---|---|
| **RTOS'suz** (timer kesmesi + ana döngü) | - | - | Tamamen kendi kodumuz | Yeterli: 2,5 ms periyot; 06 raporunun tahmini tüm kestirim yükü ~%2-4 CPU (kendi tahmin; ölçülmeli) | En küçük sistem, WCET analizi kolay, çekirdek RTOS bağımsız | Blokaj yapan sürücüler (dosya sistemi/SD) için kooperatif tasarım şart |
| **FreeRTOS** | Kernel V11.3.3 (2026-10-30) [K31]; `portable/GCC/ARM_CM33_NTZ` portu var [K32] | MIT | HAL ile çalışır; CubeMX/CubeIDE paketi üzerinden gelir (Cube U5 `Middlewares/Third_Party`'de FreeRTOS yok, 01 raporu) | Uygun; öncelikli kesmeyle zamanlama sınırı | Yaygın, MIT, kart bağımsız | Kesme önceliği/`configMAX_SYSCALL_INTERRUPT_PRIORITY` hataları klasik tuzak |
| **ThreadX** | Upstream Eclipse ThreadX v6.5.2.202603b (2026-11-01), USBX v6.5.2.202603 (2026-10-02) [K25]; ST çatalı README'sinde v6.4.0 [K8] | Upstream MIT; ST çatalı Microsoft Azure RTOS lisansı, üretim hakkı yalnızca STM32 ("LICENSED-HARDWARE") [K7][K8] | ST örnekleri ThreadX+USBX; DK'da ThreadX uygulama dizini var [K1] | Uygun | ST araç/örnek desteği; USBX ile bütünleşik | ST paket lisansı kart değişiminde risk; ThreadX 100 tick/s varsayılan ve SysTick HAL ile çakışma uyarısı [K3] |
| **Zephyr** | v4.5.0 (2026-10-28) [K33] | Apache-2.0 | Kart "Maintained"; `supported`: gpio, led, button, adc, uart, usart, lpuart, watchdog, spi, i2c, flash, sdmmc, timer, rng, rtc; CAN/PWM/USB yok [K9]; DTS'te FDCAN, OTG HS, OTG HS PHY, PWM, TIM, 160 MHz var [K10][K34]. Nucleo-U5A5ZJ-Q'da `can`, `usb_device` var [K10] | Yapılabilir | Çok kartlı taşınabilirlik, ayrıntılı sürücü modeli | Ağır; devicetree/Kconfig öğrenme yükü; proje çekirdeği zaten taşınabilir C olduğundan kazanç sınırlı |

**Zamanlama garantisi:** Kaynaklarda hiçbir seçeneğin WCET'i kendiliğinden garanti ettiğine dair bulgu yok; yanıt süresi analizi DWT çevrim sayacı ölçümü, bütçe izleme ve donanım watchdog'u ile yapılmalı (06 raporu ile aynı sonuç). Kanıta dayalı olmayan öneriler (Mühendislik değerlendirmesi): kontrol adımı en yüksek-ikinci öncelikli kesmede koşmalı; USB kesmesi bunun altında olmalı; USB ISR'si yalnızca FIFO'ları boşaltıp bayrak kaldırmalı.

**Atış kontrol emniyeti (Mühendislik değerlendirmesi):**
- "ARM" yazılım yetkisi iki aşamalı ve zaman sınırlı olmalı (jeton + süre dolumu); süre dolumu bağımsız bir donanım zamanlayıcısıyla kontrol edilmeli, yazılım döngüsünün çalışmasına bağlı olmamalı.
- Fiziksel çıkış (tetikleme) timer break/BKIN girişi ya da harici donanım kapısı ile ikinci bir engelden geçmeli; IWDG tetiklendiğinde çıkış düşük kalmalı (kart yeniden başlatıldığında varsayılan GPIO durumu güvenli olmalı).
- Atış kontrol durum makinesi çekirdekte olsun (PC'de testlenebilir); tetikleme sürücüsü platform katmanında ve çekirdek tarafından yalnızca "istek" olarak görülsün.
- TrustZone ile atış sürücüsünü güvenli dünyaya almak mümkün ama karmaşıklık ve fabrika TZEN belirsizliği nedeniyle önerilmiyor.

### 2.4 Saat ve güç yapılandırması

- **160 MHz kaynaklı örnekler:** Nucleo-U5A5ZJ-Q USBX örneği: HSE 16 MHz, `PLLM=1, PLLN=20, PLLR=2, PLLRGE_1`, Scale 1, SMPS [K2]. U5x9J-DK host örneği: `PLLM=4, PLLN=80, PLLR=2, PLLRGE_0`, Scale 1, `HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY)` [K14]. TinyUSB U5A5 BSP'si de `PLLN=20`, `PLLR=2`, SMPS [K5].
- **Regülatör:** SMPS 160 MHz için zorunlu değil (Codex doğrulaması). Ancak ST'nin DK örneği SMPS seçiyor [K14]; U585 üzerinde `ConfigSupply(SMPS)` çağrısının PWR saati açık olmadan zaman aşımına uğradığı bildirilmiş [K35]. 1,8 V VDD ile LDO'nun 160 MHz'de belgeli olup olmadığı bu oturumda doğrulanmadı; bu yüzden ST'nin örnek kombinasyonu (Scale 1 + SMPS) varsayılan olsun.
- **Zephyr kartı** için HSE 16 MHz zorunlu notu: "DSI LCD ve USB HS HSE gerektirir" [K36].
- **TrustZone (TZEN):** ST AN5347: U5'te TrustZone varsayılan kapalı, TZEN option byte ile açılır; geri alma RDP regresyonu gerektirir [K15]. Bu karta özel fabrika durumu bulunamadı (forum örneği Nucleo U5A5ZJ'de TZEN işaretli gösteriyor; sevk durumu belirsiz) [K15]. Etkisi: TZEN=1 iken FLASH/SRAM bölümleme (SAU/GTZC) nedeniyle "non-secure" bir proje çalıştırılamaz; seçenek baytları kilitlenebilir. **Önce salt okunur `STM32_Programmer_CLI … -ob displ` ile bakılmalı, yazma yapılmamalı** (komut sözdizimi bu oturumda denenmedi; kart bağlı değildi).

### 2.5 Kartta matematik

- **CMSIS-DSP** `arm_matrix_instance_f32` ve işlevleri: toplama, çıkarma, çarpma, devrik, ters, Cholesky, LDLT, ölçekleme, matris-vektör çarpımı, QR/Householder; boyut denetimi `ARM_MATH_MATRIX_CHECK` ile açılır ve `ARM_MATH_SIZE_MISMATCH` döndürür; `arm_mat_init_f32` yapıyı const bölgeden çıkarır [K37]. Ayrıntılı işlev imzaları ve kapsamlı kısıtlar bu oturumda doğrulanmadı.
- **Sabit boyutlu matris:** CMSIS-DSP'de boyutlar çalışma zamanında yapıdadır. 15x15 gibi küçük sabit boyutlar için C'de makro/`static inline` ile derleme zamanı boyutlu çarpma yazılabilir (Mühendislik değerlendirmesi). Öneri: ince bir `mat.h` soyutlaması; PC ve kart aynı basit C döngülerini kullansın, CMSIS-DSP isteğe bağlı hızlandırma (karşılaştırmalı ölçüm sonra).
- **15 durumlu hata durumlu Kalman filtresi:**

| Yol | Artı | Eksi |
|---|---|---|
| Elle türetme, kare/üst-üçgen P ile | Bağımlılık yok, tam kontrol | Hata riski yüksek; test gerektirir |
| SymPy → C99 (`codegen`/`ccode`, `cse`) | Türetme betikte, üretilen dosya depoda; C çıktısı doğrudan | Bu rapor için SymPy C99 çıktısı doğrulanmadı (genel bilgi); üretilen kod sıfırdan C olarak test edilmeli |
| SymForce | PX4 ekibi tarafından kullanıldığı biliniyor (06 raporu kapsamında değil, doğrulanmadı) | Resmi sayfaya göre çıktı C++ (Eigen şablonları) ve Python; **C çıktısı yok**; bellek ayırmasız C++ üretimi var, Apache-2.0 [K38] |
| CasADi C kod üretimi | C çıktısı verir | Lisans (LGPL) ve üretilen kod bağımlılıkları bu oturumda doğrulanmadı |

Öneri: SymPy ile üretim + elle yazılmış "Joseph formu/ardışık skaler güncelleme" iskeleti; matrisler `static` ya da çağıran sahipli `float[N*N]`. **Bellek ayırmasız tasarım**: `malloc` yasak (linker ile `-Wl,--wrap=malloc` ya da boş `malloc` ile bağlantı hatası), tüm durumlar bir `core_ctx_t` içinde, boyutlar `enum`/`#define`. Newlib-nano ve `printf` kayan nokta bayrağı (`-u _printf_float`) çekirdekte yasak.

### 2.6 Taşınabilir C çekirdek kuralları

**Sınır tasarımı (Mühendislik değerlendirmesi):**
- Çekirdek yalnızca ISO C ve `<stdint.h>`, `<stddef.h>`, `<math.h>` (sınırlı) kullanır; `#include "stm32…"`, CMSIS, RTOS başlıkları yasak. Bunu CMake'te çekirdek hedefinin include yolundan çıkararak zorlayın.
- Çekirdek zamanı kendisi okumaz: her adım `core_step(ctx, in, now_us)` biçiminde monoton `uint64_t` mikro saniye alır. Simülasyon zamanı ile gerçek zaman (watchdog/bağlantı zaman aşımı) ayrı tutulur (Codex 06 doğrulaması bu ayrımı öneriyor).
- Küresel değişken yok; tek `core_ctx_t` (aynı PC sürecinde birden çok örnek çalıştırmak için). Çıkışlar "istek" yapılarıdır.
- Donanım soyutlama: duyarlı veri yapıları (`imu_sample_t` vb., SI birim, çerçeve ve zaman damgası içerir); kart sürücüleri bunları üretir, HIL `simlink` sürücüsü de aynısını üretir.
- Sayısal: `-ffp-contract=off`, `-fno-fast-math`, çekirdekte `double` yok (M33 donanımı tek duyarlıklı; `double` yazılım emülasyonuna düşer, 06 raporu), `-Wdouble-promotion -Wfloat-conversion -Wconversion`. PC ve kart sonuçlarının bit-bit aynı olması **garanti edilemez** (`libm` ve derleyici farkları); SIL/HIL kıyaslaması tolerans ile yapılmalı (Mühendislik değerlendirmesi).

**C standardı:** GCC ARM 14.x varsayılanı gnu17. Öneri: çekirdek `-std=c17 -pedantic-errors` (C17 = C11 hata düzeltmeli sürüm), `_Static_assert` serbest, VLA/`complex`/`<stdatomic.h>`/`<threads.h>` yok. Windows'ta MSVC C11/C17 destekliyor ama bazı opsiyonel özellikler eksik (genel bilgi, bu oturumda doğrulanmadı); alternatif clang-cl ya da LLVM-MinGW. GNU eklentileri yalnızca `platform/` içinde ve makroyla (`PACKED`, `ALIGNED`).

**Statik analiz:**

| Araç | Durum | Kullanım |
|---|---|---|
| GCC uyarıları + `-fanalyzer` (C) | GCC 14'te var | Her derlemede |
| cppcheck 2.22.0 (Homebrew formülü) | MISRA eklentisi açık kaynak ama kural metinleri dağıtılamıyor, kullanıcı `--rule-texts` ile verir; README MISRA C:2012'yi anıyor [K16]; Premium özellikleri doğrulanmadı | Genel hata taraması |
| clang-tidy (LLVM 23.1.5 Homebrew) | `bugprone-*`, `cert-*`, `clang-analyzer-*` | CI |
| Proje "alt kümesi" | MISRA'dan seçilmiş ~20-30 kural (ör. dinamik bellek yok, özyineleme yok, ayrıştırılmamış union yok) | Yazılı politika + araçla zorlama; "MISRA uyumlu" iddia edilmemeli |

**PC'de birim test çatıları:**

| Çatı | Sürüm/durum | Artı | Eksi |
|---|---|---|---|
| Unity | v2.7.0 (2026-07-16) [K39] | Saf C, küçük, kartta da koşabilir | Sahte nesne için CMock (sürüm doğrulanmadı) ve Ruby gerekir |
| CppUTest | sürüm doğrulanmadı | Bellek sızıntı denetimi | C++ derleyicisi gerektirir |
| GoogleTest (C kodu `extern "C"`) | sürüm doğrulanmadı | PC tarafı zaten C++; zengin eşleştiriciler | Kartta koşmaz |

Öneri: çekirdek birim testi Unity (aynı testler ayrıca kartta kabul testi olarak koşabilir), PC uygulamaları ve simülatör için GoogleTest. CMock/Ruby bağımlılığı Windows+macOS'ta ek yük; ilk aşamada elle yazılmış fake'ler yeterli (Mühendislik değerlendirmesi).

**SITL deseni:** ArduPilot dokümanı SITL'i "sıradan bir C++ derleyicisiyle derlenmiş otopilot kodu" olarak tanımlıyor ve PC'de hata ayıklayıcı, statik/dinamik analiz araçlarının kullanılabildiğini söylüyor [K40]. (HAL ayrımı AP_HAL / zamanlayıcı konularını bu sayfa anlatmıyor; 06 raporu bunu ayrıca kaynaklıyor.) Bizdeki karşılık: `platform/host` ve `platform/stm32u5` iki "HAL" gerçeklemesi.

### 2.7 Kartta protokol kütüphanesi

| Bağlantı | Gereksinim | Öneri | Gerekçe |
|---|---|---|---|
| **Simülatör ↔ kart (HIL)** | 400-1000 Hz, DVL/sonar/sızıntı/batarya dahil, deterministik, kare sıra numarası ve iki saat alanı | Elle tanımlı sabit boyutlu `struct` + COBS + CRC32 (ya da CRC16), sürüm ve şema-özet alanı; tek `proto.h` hem C hem C++'tan dahil | Sıfır dinamik bellek, sıfır kod üretimi bağımlılığı, M33'te maliyet ihmal edilebilir; MAVLink HIL mesajlarında DVL/sonar yok (06 raporu) |
| **Kart ↔ yer istasyonu** | QGroundControl uyumu, görev planı | **MAVLink 2** C kitaplığı, ayrı kanal (ST-LINK VCP/UART ya da USB'de ikinci CDC arayüzü) | Başlık-yalnız C kütüphanesi; özel dialect üretimi için `mavgen`; MAVLink 2 dokümanı `mavlink_parse_char`, `MAVLINK_COMM_NUM_BUFFERS` (bellek kısıtlı sistemlerde 1'e düşürülebilir) ve `-Waddress-of-packed-member` uyarısını (Cortex-M0'da hata verebilir) belgeliyor [K41] |
| Ayar/konfigürasyon, olay kaydı (düşük hız) | Şema evrimi | **nanopb** (0.4.9.2, 2026-08-25 [K42]) isteğe bağlı | Statik tahsis modu, `.options` ile `max_size`; HIL'de değil, yalnız yönetim düzleminde |

MAVLink bellek maliyeti: `mavlink_message_t` boyutu ve `status` yapısı için bu oturumda sayı doğrulanmadı; kabul testinde `size`/`-fstack-usage` ile ölçülmeli. MAVLink HIL'de önerilmiyor (ArduPilot klasik HIL'i bırakmıştı, gerekçe doğrulanmadı - 06 raporu).

### 2.8 Ekran (düşük öncelik)

- **TouchGFX:** Designer yalnızca Windows; komut satırı araçları ve simülatör Linux'ta var, macOS belirtilmemiş [K43]. U5A9J-DK için TouchGFX desteği ST ekosistem belgesinde olabilir ama sayfa okunamadı (zaman aşımı); doğrulanmadı.
- **LVGL:** U5G9J-DK2 için `lv_port_stm32u5g9j-dk2` ve NeoChrom çizim birimi var (paralel RGB 800x480); MB1835 yuvarlak DSI paneli için LVGL portu bulunamadı. ST (2024) U5A9J-DK için LVGL planı olmadığını söylemiş (tarihli) [K44].
- **Sonuç:** İlk HIL kapsamı dışında bırakılmalı; durum gösterimi LED + log. Gerekirse Windows'ta TouchGFX ya da Zephyr'in MB1835 kalkanı çalışması başlangıç noktası.

### 2.9 Donanım kabul denemesi planı (C ile)

Genel kural: `REV_ID` ve `DBGMCU->IDCODE` kaydedilsin (ES0553 errata için); her test macOS ve Windows'ta tekrarlansın; sonuçlar CSV ile saklansın; 160 MHz yapılandırması yazılımla doğrulansın (MCO çıkışı ya da DWT çevrim sayacı ile bilinen bir gecikme).

| # | Test | Araç | Ölçüt (öneri; kaynaklı eşik değil) |
|---|---|---|---|
| T0 | TZEN/RDP salt okunur denetim | `STM32_Programmer_CLI` | Durum kaydı; yazma yok |
| T1 | Derleme + yükleme + gdb | CMake/Ninja, Programmer CLI, `ST-LINK_gdbserver`+gdb | İki OS'te tekrarlanabilir; kesme noktası ve yığın izi çalışır |
| T2 | 160 MHz saat, SMPS | MCO + osiloskop/sayıcı, DWT | SYSCLK=160 MHz ±HSE toleransı; 1 saat boyunca sürekli çalışma |
| T3 | USB HS CDC sayaç testi | Kart: TinyUSB; PC: C++ ölçüm aracı | HS numaralandırma (PC'de hız bilgisi); 256 B x 1 kHz, 10 dk: kayıp = 0, kare sıra numarası sürekliliği; ters yön de eşzamanlı |
| T4 | USB gecikme/jitter | Kart zaman damgası (DWT) + PC yüksek çözünürlüklü saat; echo turu | Medyan, p99, p99.9, maksimum; macOS ve Windows ayrı; CDC ve vendor-bulk karşılaştırması |
| T5 | PHY ayar varyantları | A/B yapı: tuner yazmaları var/yok; USB soğuk/sıcak takma, 100 tekrar | Numaralandırma başarı oranı |
| T6 | ST-LINK VCP gerçek hızı | USART1 DMA, 921600/2/3/6/12 Mbaud; sayaç+CRC | Kayıpsız en yüksek hız; yayımlanan değer 15 MHz (UM2525), DK'da ölçülmedi |
| T7 | FDCAN | İç döngü (loopback) sonra harici alıcı-verici ve CAN analizörü; PD0/PD1 | Tx sırası ve filtre kombinasyonları (errata); hata sayaçları |
| T8 | PWM | TIM1/2/3/4 kanalları, mantık analizörü | Frekans/görev doğruluğu; ortak zaman tabanı etkisi; güvenli varsayılan durum (reset sonrası) |
| T9 | UART-DMA | USART3/LPUART1 halka tampon, harici USB-UART (1,8 V seviye) | 2-3 Mbaud'da kayıp = 0; idle-line kesmesi |
| T10 | Zamanlama bütçesi | DWT çevrim sayacı, kontrol adımı için histogram | Adım süresi ve jitter; USB yükü altında kontrol adımı gecikmesi |
| T11 | Watchdog ve atış çıkışı | IWDG, GPIO/timer break, mantık analizörü | Yazılım kilitlendiğinde çıkış güvenli durumda; reset sırasında hiç darbe yok |

---

## 3. Mimari için öneriler

**Önerilen yığın:**

| Katman | Öneri | Gerekçe | Yedek |
|---|---|---|---|
| Dil / standart | Çekirdek ISO C17 (`-std=c17 -pedantic-errors`), platform gnu17 | Taşınabilirlik; GCC ARM 14 ve MSVC/clang ikisinde de derlenir | C11 |
| Derleme | CMake + Ninja + CMake presets; kendi CMake'imiz; CubeMX yalnızca referans | CubeMX CMake alt dizin olamıyor, çekirdek dosyalar ezilir [K11][K12] | CubeMX CMake'i ayrı `board/` hedefi olarak kullanma |
| Toolchain | GNU Tools for STM32 14.3.rel1 (CubeCLT/CubeIDE içindeki) sabit; yerel arşiv | ST örnekleri ve CubeCLT aynı; CubeCLT geleceği belirsiz [K19] | Arm GNU 15.3.rel2 (cask) |
| HAL / sürücü | ST `stm32u5xx_hal_driver` + `cmsis_device_u5` alt modül (sürüm etiketi sabit) | Cube U5 v1.9.0 ile aynı kaynak; TinyUSB aynı yolu kullanıyor [K5][K22] | LL sürücüler |
| Yükleme / hata ayıklama | `STM32_Programmer_CLI`, `ST-LINK_gdbserver` + `arm-none-eabi-gdb` | İki işletim sisteminde ST'nin kendi araçları; OpenOCD yedek | OpenOCD (0.12.0) |
| Zamanlama | RTOS'suz: 400 Hz kontrol adımı timer kesmesinde, USB/haberleşme ana döngüde, IWDG + DWT bütçe izleme | En küçük yüzey; çekirdek RTOS'tan bağımsız | FreeRTOS (MIT) |
| USB HIL | Native HS CDC (CN4), TinyUSB, `dwc2`; ST Nucleo USBX örneği PHY referansı | MIT, OS'siz, kart değişimine dayanıklı | HAL+upstream USBX standalone; ST-LINK VCP (ölçülmüş hızda) |
| HIL çerçevesi | Sabit struct + COBS + CRC | Deterministik, MAVLink DVL/sonar eksikliği | postcard benzeri şema üreteci (C için nanopb) |
| GCS | MAVLink 2 (mavgen C, özel dialect) | QGC uyumu | - |
| Matematik | Kendi `mat.h` (sabit boyut), SymPy → C99 üretimi, CMSIS-DSP isteğe bağlı | PC/kart aynı kod; malloc'suz | CMSIS-DSP matrisleri |
| Test | Unity (çekirdek, hem PC hem kart), GoogleTest (PC uygulamaları) | Çekirdek saf C; PC tarafı C++ | CppUTest |
| Analiz | `-Wall -Wextra -Wconversion -Wdouble-promotion`, `-fanalyzer`, clang-tidy, cppcheck, proje kural alt kümesi | Açık kaynak araçlarla uygulanabilir | Ticari MISRA aracı |
| Atış emniyeti | Çekirdekte durum makinesi + donanım kapılama + IWDG + zaman sınırlı ARM | Yazılım zamanlayıcısına bağımlı olmayan emniyet | - |

Ek öneriler:
1. **Geçiş planı:** `platform/stm32u5` altındaki her sürücü, kart değişiminde yalnızca yeniden yazılacak tek yerdir. Çekirdek testleri kartta koşabilecek şekilde derlenmeli (Unity), böylece yeni kartta hızlı doğrulama sağlanır.
2. **ThreadX'e bağımlılık yaratılacaksa** yalnızca upstream Eclipse (MIT) kullanın; ST paketindeki çatal lisansı kart değişiminde sorun çıkarır [K7][K8].
3. **USB spike öncelik sırası:** (a) ST Nucleo-U5A5ZJ USBX örneğinin PHY dizisini bu kartta çalıştır, (b) TinyUSB board dosyası, (c) PHY ayar yazmaları ile/olmadan karşılaştırma.

---

## 4. Riskler ve açık sorular

| # | Risk / soru | Etki | Durum |
|---|---|---|---|
| R1 | U5A9J-DK'da HS CDC hiçbir yığınla doğrulanmadı | HIL ana yolu | Spike |
| R2 | PHY ayarlarının (COMPDISTUNE/SQRXTUNE) gerekliliği belirsiz; ST örneği çağırmıyor, HAL öneriyor | HS kararlılığı | Spike: iki varyant |
| R3 | VBUS algılama davranışı (PG1/UCPD/TCPP01) kartta; kök neden belirsiz | Bağlantı algılama | Spike |
| R4 | ST'nin ThreadX paketi lisansı kart değişiminde geçersiz | Hukuki / taşınabilirlik | Upstream MIT kullan |
| R5 | CubeCLT'nin bundle manager ile değişimi; araç sürümlerinin kaybolması | Derleme tekrarlanabilirliği | Araçları yerel arşivle |
| R6 | CubeMX sürüm kırılmaları (6.14/6.15 CMake değişiklikleri) | Üretilen kod | CubeMX'i derleme yolundan çıkar |
| R7 | PC–kart sayısal sonuç farkı (FMA, `libm`) | SIL/HIL karşılaştırması | Tolerans ve politika |
| R8 | TZEN/RDP fabrika durumu doğrulanmadı | Karta yükleme | Önce salt okunur denetle |
| R9 | Windows akışı (ST-LINK sürücü, TinyUSB/CDC sürücü) denenmedi | İki OS hedefi | Kabul testi |
| R10 | Silikon errata (ES0553): FDCAN Tx sıralaması, ADC4+ADC1/2 eşzamanlı dönüşüm (Codex) | Kabul testi kapsamı | REV_ID kaydet |
| R11 | 1,8 V VDD ile LDO yolunun 160 MHz'de belgeli olup olmadığı | Regülatör seçimi | Doğrulanmadı |
| R12 | MAVLink C kütüphanesinin RAM/flash maliyeti | Bellek bütçesi | Ölç |

Açık sorular: CubeCLT 1.22.0 içeriği; OpenOCD 0.12.0 U5A flash desteği; RM0456 §15.2.3 metninin birebir okunması; TouchGFX'in U5A9J-DK desteği; CMock güncel sürümü; MAVLink C mesaj yapı boyutları.

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **Cube U5 v1.9.0'da `Ux_Device_CDC_ACM` yalnızca NUCLEO-U5A5ZJ-Q ve STM32U5G9J-DK2 için var; `STM32U5x9J-DK/Applications/USBX` yalnızca Ux_Host_CDC_ACM, Ux_Host_HID, Ux_Host_HUB_HID_MSC içeriyor.** Kaynak: https://github.com/STMicroelectronics/STM32CubeU5/tree/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX ve .../NUCLEO-U5A5ZJ-Q/Applications/USBX
2. **ST'nin Nucleo-U5A5ZJ-Q CDC örneği `PCD_SPEED_HIGH` + `USB_OTG_HS_EMBEDDED_PHY`, `vbus_sensing_enable=DISABLE` kullanıyor ve PHY ayar işlevlerini (`HAL_SYSCFG_SetOTGPHY*`) çağırmıyor.** Kaynak: https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/NUCLEO-U5A5ZJ-Q/Applications/USBX/Ux_Device_CDC_ACM/Core/Src/main.c ve .../Core/Src/stm32u5xx_hal_msp.c
3. **HAL'da `HAL_SYSCFG_SetOTGPHYDisconnectThreshold/SquelchThreshold/PreemphasisCurrent` var; önerilen değerler +%5,9 (disconnect) ve +%15 (squelch).** Kaynak: https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/main/Inc/stm32u5xx_hal.h ve .../Src/stm32u5xx_hal.c
4. **ST'nin `stm32-mw-threadx` deposu Microsoft Azure RTOS lisansı altında ve üretim/dağıtım hakkı yalnızca `LICENSED-HARDWARE.txt` donanımı (STMicroelectronics) için; upstream Eclipse ThreadX MIT.** Kaynak: https://github.com/STMicroelectronics/stm32-mw-threadx/blob/main/LICENSE.md , .../LICENSED-HARDWARE.txt , https://community.st.com/stm32-mcus-embedded-software-32/azure-rtos-licensing-terms-eclipse-threadx-nowadays-138336
5. **Zephyr `stm32u5a9j_dk.yaml` `supported` listesinde can/pwm/usb yok; `nucleo_u5a5zj_q.yaml` listesinde `can` ve `usb_device` var.** Kaynak: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/stm32u5a9j_dk.yaml ve .../nucleo_u5a5zj_q/nucleo_u5a5zj_q.yaml
6. **TinyUSB 0.21.0'ın STM32U5 BSP listesinde U5A9J-DK yok; U5A5 Nucleo BSP'si HSE tabanlı PHY saati, `SYSCFG_OTG_HS_PHY_CLK_SELECT_1`, SMPS ve VBUS geçersiz kılma içeriyor.** Kaynak: https://github.com/hathach/tinyusb/tree/master/hw/bsp/stm32u5/boards , https://github.com/hathach/tinyusb/blob/master/hw/bsp/stm32u5/boards/stm32u5a5nucleo/board.h , .../hw/bsp/stm32u5/family.c
7. **CubeMX 6.14'te üretilen CMake bir üst projenin alt dizini olarak kullanılamıyor; `cmake/stm32cubemx/CMakeLists.txt` her üretimde ezilir; 6.15'ten itibaren toolchain tanımı yalnızca CMakePresets.cmake'te.** Kaynak: https://community.st.com/stm32cubemx-mcus-29/cubemx-6-14-xx-breaks-cmake-workflows-151130 , https://community.st.com/stm32cubemx-mcus-29/how-to-customize-your-stm32cubemx-generated-cmake-project-159569 , https://community.st.com/stm32cubemx-mcus-29/cmake-project-generated-by-stm32cubemx-6-15-build-fails-says-compiler-broken-154417
8. **Cube U5 DK host örneği 160 MHz'e HSE 16 MHz, M=4, N=80, R=2, Scale 1 ve `PWR_SMPS_SUPPLY` ile çıkıyor; Nucleo örneği M=1, N=20, R=2.** Kaynak: https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/Core/Src/main.c ve .../NUCLEO-U5A5ZJ-Q/Applications/USBX/Ux_Device_CDC_ACM/Core/Src/main.c
9. **Resmi SymForce sayfası yalnızca C++ ve Python üretimi belgeliyor (C yok).** Kaynak: https://github.com/symforce-org/symforce
10. **AN5347: U5'te TrustZone varsayılan kapalı (TZEN=0); açma/kapama RDP regresyonu gerektirir.** Kaynak: https://www.st.com/content/st_com/en/technical-documents/an5347-introduction-to-arm-trustzone-features-on-stm32l5-stm32u5-and-stm32u3-mcus-stmicroelectronics_asset/chunk-004-topic-an5347-security-tz-enabled.html

---

## 6. Kaynaklar

- [K1] STM32CubeU5 v1.9.0 `Projects` dizinleri: https://github.com/STMicroelectronics/STM32CubeU5/tree/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX ; https://github.com/STMicroelectronics/STM32CubeU5/tree/v1.9.0/Projects/NUCLEO-U5A5ZJ-Q/Applications/USBX ; https://github.com/STMicroelectronics/STM32CubeU5/tree/v1.9.0/Projects/STM32U5G9J-DK2/Applications/USBX
- [K2] Nucleo-U5A5ZJ-Q Ux_Device_CDC_ACM kaynağı: https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/NUCLEO-U5A5ZJ-Q/Applications/USBX/Ux_Device_CDC_ACM/Core/Src/main.c ; .../Core/Src/stm32u5xx_hal_msp.c
- [K3] Aynı örneğin README'si (ThreadX ipuçları): https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/NUCLEO-U5A5ZJ-Q/Applications/USBX/Ux_Device_CDC_ACM/README.md
- [K4] STM32U5 HAL: https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/main/Inc/stm32u5xx_hal.h ; https://github.com/STMicroelectronics/stm32u5xx_hal_driver/blob/main/Src/stm32u5xx_hal.c
- [K5] TinyUSB: https://github.com/hathach/tinyusb/tree/master/hw/bsp/stm32u5 ; https://github.com/hathach/tinyusb/blob/master/hw/bsp/stm32u5/boards/stm32u5a5nucleo/board.h ; https://github.com/hathach/tinyusb/blob/master/hw/bsp/stm32u5/family.c ; sürüm 0.21.0 (GitHub API, 2026-06-30)
- [K6] CherryUSB: https://github.com/cherry-embedded/CherryUSB/blob/master/port/dwc2/usb_glue_st.c ; sürüm v1.6.1 (GitHub API); belge: https://cherryusb.readthedocs.io/en/latest/quick_start/demo.html
- [K7] ST ThreadX çatalı lisansı: https://github.com/STMicroelectronics/stm32-mw-threadx/blob/main/LICENSE.md ; https://github.com/STMicroelectronics/stm32-mw-threadx/blob/main/LICENSED-HARDWARE.txt
- [K8] ST ThreadX çatalı README (v6.4.0): https://github.com/STMicroelectronics/stm32-mw-threadx ; ST topluluk yanıtı (2024-09-06): https://community.st.com/stm32-mcus-embedded-software-32/azure-rtos-licensing-terms-eclipse-threadx-nowadays-138336
- [K9] Zephyr U5A9J-DK kart tanımı: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/stm32u5a9j_dk.yaml
- [K10] Zephyr Nucleo-U5A5ZJ-Q: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/nucleo_u5a5zj_q/nucleo_u5a5zj_q.yaml
- [K11] CubeMX 6.14 CMake sorunu: https://community.st.com/stm32cubemx-mcus-29/cubemx-6-14-xx-breaks-cmake-workflows-151130
- [K12] CubeMX CMake özelleştirme: https://community.st.com/stm32cubemx-mcus-29/how-to-customize-your-stm32cubemx-generated-cmake-project-159569
- [K13] CubeMX 6.15 toolchain değişikliği: https://community.st.com/stm32cubemx-mcus-29/cmake-project-generated-by-stm32cubemx-6-15-build-fails-says-compiler-broken-154417
- [K14] DK host örneği: https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/Core/Src/main.c
- [K15] AN5347: https://www.st.com/content/st_com/en/technical-documents/an5347-introduction-to-arm-trustzone-features-on-stm32l5-stm32u5-and-stm32u3-mcus-stmicroelectronics_asset/chunk-004-topic-an5347-security-tz-enabled.html ; https://community.st.com/t5/stm32-mcus/how-to-disable-trustzone-on-stm32u5-series/ta-p/49793
- [K16] cppcheck MISRA eklentisi: https://sources.debian.org/src/cppcheck/2.20.0-2/addons/README.md
- [K17] CubeCLT 1.21.0 duyurusu: https://community.st.com/stm32cubeide-mcus-28/stm32cubeclt-1-21-0-released-163146
- [K18] RN0132 v1.21.0: https://www.st.com/resource/en/release_note/rn0132-stm32cube-commandline-toolset-release-v1210-stmicroelectronics.pdf (arama özeti üzerinden okundu; v1.22.0: https://www.st.com/resource/en/release_note/rn0132-stm32cube-commandline-toolset-release-v1220-stmicroelectronics.pdf okunamadı)
- [K19] CubeCLT'nin geleceği: https://community.st.com/t5/stm32cubeide-for-visual-studio/the-future-of-stm32cubeclt/td-p/889860
- [K20] ST VS Code CMake belgesi: https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/1.0.1/en/docs/markup/basic_concepts/cmake.html
- [K21] USBX device controller CMake: https://community.st.com/stm32cubemx-mcus-29/usbx-stm32-device-controllers-files-not-included-in-cmake-build-149379
- [K22] Cube U5 alt modüller: https://raw.githubusercontent.com/STMicroelectronics/STM32CubeU5/v1.9.0/.gitmodules
- [K23] Zephyr U5A9J-DK OpenOCD/CubeProgrammer ayarları: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/support/openocd.cfg ; .../board.cmake
- [K24] OpenOCD: https://github.com/openocd-org/openocd/blob/master/tcl/target/stm32u5x.cfg ; https://github.com/openocd-org/openocd/blob/master/src/flash/nor/stm32l4x.c ; v0.12.0 etiketinde dosya varlığı (HTTP 200)
- [K25] Eclipse ThreadX/USBX sürümleri: https://github.com/eclipse-threadx/threadx/releases ; https://github.com/eclipse-threadx/usbx/releases ; MIT: https://projects.eclipse.org/node/28988
- [K26] DK1 USB device sorusu: https://community.st.com/stm32-mcus-products-25/stm32u5a9j-dk1-usb-device-question-150504
- [K27] CubeMX U5'te USBX: https://community.st.com/stm32-mcus-products-25/otg-hs-irqhandler-isn-t-fired-on-stm32u5a9j-dk-when-usbc-attached-153639
- [K28] CMSIS device başlığı (OTGHSPHYTUNER2 bitleri): https://github.com/STMicroelectronics/cmsis_device_u5/blob/main/Include/stm32u5a9xx.h
- [K29] Aynı iş parçacığı (FIFO/VBUS bulgusu): [K27]
- [K30] USB_CoreReset zaman aşımı: https://community.st.com/t5/stm32-mcus-embedded-software/stm32u5a5ztj-usb-pcd-failing-on-initialization-at-usb-corereset/m-p/632232
- [K31] FreeRTOS-Kernel sürümleri: https://github.com/FreeRTOS/FreeRTOS-Kernel/releases
- [K32] FreeRTOS CM33 portları: https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/main/portable/GCC
- [K33] Zephyr sürümleri: https://github.com/zephyrproject-rtos/zephyr/releases
- [K34] Zephyr U5A9J-DK DTS: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/stm32u5a9j_dk.dts
- [K35] ConfigSupply zaman aşımı: https://community.st.com/stm32-mcus-products-25/stm32u585-cannot-switch-between-smps-and-ldo-power-supplies-10459
- [K36] Zephyr U5A9J-DK belgesi: https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html
- [K37] CMSIS-DSP matrisler: https://arm-software.github.io/CMSIS-DSP/latest/group__groupMatrix.html
- [K38] SymForce: https://github.com/symforce-org/symforce
- [K39] Unity: https://github.com/ThrowTheSwitch/Unity/releases
- [K40] ArduPilot SITL: https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html
- [K41] MAVLink C: https://mavlink.io/en/mavgen_c/
- [K42] nanopb: https://github.com/nanopb/nanopb/releases
- [K43] TouchGFX gereksinimleri: https://support.touchgfx.com/docs/introduction/prerequisites
- [K44] LVGL/ST: https://community.st.com/t5/stm32-mcus-touchgfx-and-gui/are-there-plans-to-port-lvgl-to-stm32h747i-disco-or-stm32u5a9j/td-p/648698 ; https://docs.lvgl.io/master/integration/chip_vendors/stm32/neochrom.html
- Önceki belgeler: 01-kart-ve-arac-zinciri.md ; dogrulama/01-codex.md ; 06-otopilot.md ; dogrulama/06-codex.md
