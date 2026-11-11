# 01 - Kart ve Arac Zinciri (STM32U5A9J-DK, HIL baglantisi, Rust/C arac zinciri)

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Ayrıca dil kararı C/C++ olmadan önce, Rust varsayımıyla yazıldı; Rust'a özgü öneriler geçersizdir. Doğrulaması: [dogrulama/01-codex.md](dogrulama/01-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Tarih: 2026-11-10. Arastirma makinesi: macOS 26.5.2 (Apple Silicon, arm64). Windows tarafi bu makinede denenmedi; Windows ile ilgili tum satirlar kaynak okumasina dayanir.

Kaynak notu: ST'nin kendi sitesi (st.com) bu oturumdan dogrudan indirilemedi. UM2967 (kart kullanim kilavuzu) icin Rev 2 (Mart 2023) kopyasi `evo.net.ua` uzerinden alindi ve yerel olarak metne cevrildi; guncel revizyon farkli olabilir. Cip verisi icin ST datasheet/RM0456 yerine `stm32-data-generated` (embassy projesinin cip veritabani) ve Zephyr/Embassy/probe-rs kaynak kodu kullanildi. Bunlar ikincil/turetilmis kaynaklardir; ilgili yerlerde belirtildi.

---

## 1. Ozet

1. Kart: STM32U5A9J-DK = STM32U5A9NJH6Q (Cortex-M33, TrustZone, FPU, en fazla 160 MHz, 4 MB flash, 2.5 MB SRAM, TFBGA216). Ekran/dokunmatik/PSRAM/OctoSPI flash/eMMC/ToF/sicaklik sensoru var; IMU, barometre, CAN transceiver, Ethernet yok. [UM2967]
2. En kritik kisit: kart yalnizca 1.8 V VDD ile calisacak sekilde tasarlanmis; tum GPIO ve genisleme konnektorleri 1.8 V mantik seviyesinde. 3.3 V/5 V ESC, servo, CAN transceiver, GNSS vb. icin seviye cevirici gerekir. [UM2967 s.2, s.18, s.22]
3. Otopilot icin kullanilabilir arayuz: konnektorlerde 2 UART (USART3, LPUART1) + VCP icin ayrilmis USART1, 1 adet FDCAN (yalniz FDCAN1; kartta transceiver yok), 3 I2C (+ kart ici I2C), 2 SPI, 7 ADC girisi, 4 adanmis + 2 paylasimli PWM-yetenekli pin, USB 2.0 HS (Type-C, yalniz cihaz/sink). FDCAN1'in varsayilan pinleri (PA11/PA12) USB HS ile cakisir; konnektorde tek makul secenek PD0/PD1 (I2C6 ile paylasimli).
4. HIL baglantisi: ST-LINK sanal seri port 115200 baud'da yalniz ~11.5 KB/s verir; 1 kHz'te 64-256 bayt/kare icin 2-3 Mbaud ve ustu gerekir ve ST-LINK-V3E'nin kesin VCP ust siniri birincil kaynakta dogrulanamadi. Dogru yol: native USB 2.0 HS CDC (CN4); ST-LINK/VCP yedek ve log yolu olarak kalmali.
5. Rust: `embassy-stm32` 0.6.0 icinde `stm32u5a9nj` ozelligi var; `stm32-metapac` cip verisi FDCAN1, USB_OTG_HS, DSIHOST, LTDC, HSPI1, OCTOSPI1/2, TIM1/2/3/4/8/15/16/17, ADC1/2/4, GPDMA1 tanimliyor. probe-rs 0.32.0 `STM32U5A9NJ` hedefini ve dahili flash algoritmasini iceriyor.
6. Rust tarafinin zayif noktasi: Embassy'de U5 icin USB OTG HS, PHY ayari eksik oldugu icin calismiyordu; duzeltme 2026-11-06'da `main`'e girdi (PR #7439), 0.6.0 (2026-03-20) icinde YOK ve PR yazari "bu dal donanimda calistirilmadi" diyor. Bu karta ozel (U5A9J-DK) hicbir halka acik Embassy ornegi bulunamadi.
7. Ekran: Embassy'de DSIHOST suruculeri var (2026-03 guncellemeleri) ama panel orneklerinin hepsi STM32H747/F469 icin; U5A9J-DK'nin MB1835 (J025F1CN0201W, Zephyr belgesine gore Himax HX8379C) paneli icin Rust ornegi yok. LTDC ornegi U5G9J-DK2 (paralel RGB) icin ve U5G9J-DK2 uzerinde bozulma bildirimi (#5568) acik. Ekran otopilot icin gerekli degil; plana dahil edilmemeli.
8. C yolu: STM32CubeU5 v1.9.0 (2026-07-01), U5x9J-DK BSP'si, ThreadX/USBX/FileX uygulamalari, CubeMX 6.18, CubeIDE 2.2.0 mevcut; fakat DK icin Cube ornekleri sinirli (BSP, DMA, DSI, FLASH, GPIO, OSPI, PWR, RAMCFG, RCC, RTC) - FDCAN/TIM/UART/USB cihaz ornegi DK dizininde yok. TouchGFX Designer macOS'ta yerel degil.
9. macOS (Apple Silicon): Bu makinede CubeIDE 2.2.0 (arm64 gcc 14.3.rel1, cmake, ST-LINK_gdbserver) ve CubeProgrammer 2.23.0 (arm64) kurulu; Rust, probe-rs, rustup, ayri cmake/gcc kurulu degil. probe-rs macOS'ta surucu istemez; Windows'ta ST surucusu gerekir.
10. Oneri: firmware cekirdegi Rust (`no_std`, HAL'dan bagimsiz) + Embassy HAL, C yalnizca referans/yedek. FFI karmasi varsayilan olarak onerilmez. Karar, 1-2 gunluk bir "spike" ile (USB HS CDC, FDCAN, PWM, UART-DMA, 160 MHz saat) kapatilmali; basarisiz olursa belgelenmis geri donus yollari var.

---

## 2. Bulgular

### 2.1 STM32U5A9J-DK gercek yetenekleri

**MCU** (UM2967 s.2; Zephyr kart belgesi; cip verisi)
- STM32U5A9NJH6Q, Arm Cortex-M33 + TrustZone, TFBGA216. Ust sinir 160 MHz, FPU (tek hassasiyet), DSP, MPU (ST urun sayfasi ozeti ve distributor listeleri; datasheet dogrudan okunamadi).
- Bellek haritasi (stm32-data-generated + probe-rs ile uyumlu): Flash 2 x 2 MB (BANK_1 0x08000000, BANK_2 0x08200000; silme 8 KB, yazma 16 bayt), SRAM1 768 KB + SRAM2 64 KB + SRAM3 832 KB + SRAM5 832 KB = 2496 KB, 0x20000000-0x20270000 arasi bitisik; 2 KB BKPSRAM.
- Cip ayrica GPU2D (NeoChrom), DMA2D (Chrom-ART), GFXMMU, OCTOSPI1/2, HSPI1, SDMMC1/2, DCMI, PSSI, MDF1/ADF1, SAI1/2, UCPD1, OPAMP1/2, COMP1/2, DAC1, CORDIC, FMAC, AES/SAES/PKA/HASH/RNG tanimliyor.

**Kart ustu bilesenler** (UM2967 s.2, s.27-35)
| Bilesen | Detay | Baglanti |
|---|---|---|
| Ekran | 2.47" yuvarlak TFT 480x480, MIPI DSI 2 hat, kapasitif dokunmatik (MB1835 kizi karti, JHD J025F1CN0201W/N) | DSI; dokunmatik I2C5 (PH4/PH5), reset PD5, backlight PI6, touch INT PE8 |
| Octo-SPI NOR | MX25UM51245G 512 Mbit, 1.8 V, 80 MHz DTR garantili | OCTOSPIM_P1 (PA1, PA2, PF6-PF10, PC0-PC3) |
| Hexadeca-SPI PSRAM | APS512XXN-OBR 512 Mbit, 160 MHz DDR | HSPI1 (PH9-PH15, PI0-PI4, PI8-PI15, PJ0) |
| eMMC | MTFC4GACAJCN 4 GB, eMMC 5.0 | SDMMC1 (PC6-PC12, PD2, PB8, PB9, PH6) |
| ToF sensoru | VL53L5CX (Sinif 1 lazer) | I2C3 PH7/PH8, LPN PE14, INT PB5 |
| Sicaklik sensoru | STTS22H | I2C3 (ToF ile paylasimli), INT PF2 |
| Ses codec | CS42L51 + jak (varsayilan olarak LEHIMLI DEGIL) | I2C4, SAI1 |
| Kullanici | 2 LED (PE0 yesil, PE1 kirmizi), B1 kullanici dugmesi (PC13), reset | - |
| Hata ayiklayici | STLINK-V3E (CN5, Micro-B): SWD, VCP, mass storage | - |
| USB Type-C | CN4: USB 2.0 HS, yalniz sink, TCPP01-M12 koruma | PA11/PA12 (HS D-/D+), UCPD yardimcilari PE9/PE12/PG0/PG1/PF15 |
| Diger konnektorler | MIPI10/STDC14 (CN11), Tag-Connect (CN3), CN1 20 pin ses MEMS (bos), CN9 (30 pin) ve CN10 (38 pin) genisleme | - |

Kartta **yok**: IMU, jiroskop, manyetometre, barometre, GNSS, Ethernet, CAN transceiver, kamera konnektoru. Yani HIL'de sensor verisi tamamen PC'den gelecek; bu planla uyumlu.

**Guc/seviye kisitlari** (UM2967 s.2, s.18, s.22-24)
- "Bu Discovery kit yalnizca VDD ve VDD_MCU = 1.8 V icin tasarlanmis dusuk guc sistemidir." VDDIO2 (PG2-PG15) da 1.8 V. VDDA = 1.8 V, bu yuzden VREFBUF VREF+ icin kullanilamaz. VDDUSB 3.3 V (HS USB icin).
- CN9 IOREF = VDD (1.8 V). Konnektordeki 3V3/5V pinleri yalnizca besleme cikisidir, GPIO seviyesini degistirmez.
- JP2/JP3 kartta calisirken sokulmemeli (uzun vadeli guvenilirlik uyarisi).
- GPIO'larin 5 V toleransi bu seviyede **dogrulanmadi** (datasheet okunamadi). Guvenli varsayim: 1.8 V disinda hicbir sinyal dogrudan baglanmaz.

**Genisleme konnektorlerinde fiilen cikan pinler** (UM2967 Tablo 29, 30, 34; zayif yanlar surada isaretli)

| Islev | Pinler (konnektor pin no) | Not |
|---|---|---|
| UART | USART3: PB10 TX (CN9-26), PB11 RX (CN9-24), PA6 CTS (CN9-30), PB1 RTS (CN9-28), PB0 CK (CN9-22). LPUART1: PG7 TX (CN10-36), PG8 RX (CN10-38) | USART3 sinyallerinin bir kismi lehim koprusuyle DSI_V3 konnektoruna da gidebilir. USART1 (PA9/PA10) ST-LINK VCP'ye ayrilmis. |
| FDCAN | Konnektorde etiketli yok. Cipte FDCAN1 pinleri (AF9): PA11/PA12, PB8/PB9, PD0/PD1, PF7/PF8, PH13/PH14 | PA11/PA12 = USB-C HS; PB8/PB9 = eMMC; PF7/PF8 = OctoSPI flash; PH13/PH14 = HSPI PSRAM. Konnektorda ulasilabilen tek aday PD0 (RX)/PD1 (TX) = CN10-35/37 (varsayilan I2C6). Transceiver harici eklenmeli. (Turetilmis sonuc: UM2967 + stm32-data-generated.) |
| I2C | I2C1 PG14 SCL (CN9-29), PG13 SDA (CN9-27); I2C2 PF1 SCL (CN9-16), PF0 SDA (CN9-14); I2C6 PD1 SCL (CN10-37), PD0 SDA (CN10-35) | CN9-27/29 ADC (PA5/PC4) ile paylasimli, lehim koprusu SB6-SB9 ile secilir; kullanmadan once kart uzerinde kontrol edilmeli. |
| SPI | SPI2: PB13 SCK (CN10-15), PD4 MOSI (CN10-13), PD3 MISO (CN10-19), PB12 NSS (CN10-17); SPI3: PG9 SCK (CN10-10), PG10 MISO (CN10-12), PG11 MOSI (CN10-14), PG15 CS (CN10-16) | SPI2'nin bir kismi SB ile SAI2/DSI ile paylasilir. |
| ADC | ADC1/2: PA0 (CN9-21), PA4 (CN9-25), PA5 (CN9-27), PB2 (CN9-19), PC4 (CN9-29), PC5 (CN9-23); ADC4: PF14 (CN10-29) | ADC4_IN6/IN7 (PF15/PG0) varsayilan bagli degil (USB-C UCPD ile ozel). Zephyr DTS ADC1'i 14 bit, ADC4'u 12 bit kullaniyor. |
| PWM-yetenekli | TIM1_CH2 PE11 (CN10-26), TIM2_CH4 PA3 (CN10-32), TIM3_CH2 PA7 (CN10-18), TIM4_CH4 PD15 (CN10-28) -> 4 adanmis. TIM15_CH2 PG11 (CN10-14), LPTIM1_CH1 PG15 (CN10-16) -> SPI3 ile paylasimli | Daha fazlasi icin cip AF tablosuna gore baska islevlerle yer degistirerek: PA0 (TIM2_CH1), PB10/PB11 (TIM2_CH3/4), PA6/PB0/PB1 (TIM3/TIM16/TIM1N), PB12-PB15 (TIM15/TIM1N). Sonuc: ~6 PWM rahat, ~10 PWM bazi arayuzlerden vazgecerek. (Turetilmis sonuc.) |
| Diger | SDMMC2 4-bit, SAI1/SAI2, serbest GPIO (CN10: PF5, PF12, PF13, PE7), PG2-PG6, PJ1-PJ11, PD13/PD14, PE15 test noktasi olarak | USART6 PJ3/PJ4 gibi test noktalari BGA test pedi, lehim gerektirir. |
| USB | CN4 USB-C HS (PA11/PA12) | Yalnizca cihaz; varsayilan JP4=STLK ile kart CN5'ten beslenir, CN4 veri icin "self-powered" cihaz olur. |

Notlar:
- Tek bir FDCAN var (FDCAN1). Araca CAN gerekiyorsa tek veri yolu; ikinci CAN yok.
- Hata ayiklama: SWD (CN5 uzerinden), SWO varsayilan olarak calismaz (SB33/SB34 degisikligi gerekir, SDMMC2.D2 ile cakisir). Yani `defmt`/RTT (SWD uzerinden) kullanilacak, SWO beklenmemeli.
- Boot: SW1 (BOOT0) kartin alt yuzunde; nSWBOOT0=1 oldugu icin dahili flash'tan acilir.
- TrustZone: fabrika TZEN durumu DK icin **dogrulanmadi**. AN5347'ye gore U5'te TrustZone fabrikada kapali; ST topluluk iletisinde TZEN=1 + RDP 0x55 ile "bricked" DK, ancak hot-plug + BOOT0=1 + RDP/TZEN birlikte regresyonuyla kurtarilmis. Yani SBSFU/TrustZone orneklerini bu kartta denerken dikkat; CubeProgrammer option-byte erisimi (TZEN/RDP) Rust yolunda da gerekli.

### 2.2 HIL baglanti secenekleri

Veri hacmi (hesap, bu rapora ozgu): 1 kHz dongu, kare basina N bayt -> N KB/s. 8N1'de bayt basi 10 bit.
| Kare boyu | Veri hizi (1 kHz) | Gereken minimum baud (kaba) |
|---|---|---|
| 64 B | 64 KB/s | ~0.64 Mbaud |
| 128 B | 128 KB/s | ~1.3 Mbaud |
| 256 B (IMU+baro+GNSS+komut+CRC) | 256 KB/s | ~2.6 Mbaud |
Cift yonlu (aktuator komutlari ters yonde) ayri bir akis; ek yuk kucuk. 115200 baud = 11.5 KB/s; 921600 baud = 92 KB/s.

Secenekler:
1. **ST-LINK VCP (CN5, USART1 PA9/PA10)**: UM2967 s.17 baglantiyi dogruluyor. Ust baud/gecikme sinirini ST dokumani vermiyor: UM2967 bir sayi yazmiyor; V3MINIE icin ST 736 bps-16 Mbps (UM2910) ve urun sayfasinda "VCP 15 Mbps" yaziyor; V3E icin kesin sayi **dogrulanamadi**. Topluluk: 921600'a kadar calisiyor, V3 ilk yazilimlarinda hizli buyuk dizilerde karakter kaybi bildirilmis (firmware V3.J5.M2'de duzeldigi soylenmis), alinan hizin buyuk kismi host USB yiginiyla sinirli. Gecikme icin ST sayisi yok. Sonuc: kontrol-dongusu icin degil, log/komut icin uygun; yuksek hizi **olcmeden** kabul etmeyin.
2. **Native USB 2.0 HS CDC (CN4, OTG_HS gomulu PHY, PA11/PA12)**: 480 Mbit/s fiziksel, bulk 512 bayt paket; veri hacmi icin bol pay. Hem macOS hem Windows yerel CDC-ACM surucusunu kullanir. Dikkat: (a) Embassy'de U5 HS PHY duzeltmesi yalniz `main`'de (bkz. 2.3), (b) Windows'ta HS bulk icin max paket boyutu 512 olmali (Embassy ornegi 2026-07'de bu yuzden duzeltildi; yanlis deger "Code 43"), (c) saat HSE (16 MHz kristal kartta var) gerekli. Host tarafi gecikmesi (macOS/Windows seri yigini) icin kaynakli sayi bulunamadi; olculmeli. Alternatif: CDC yerine vendor-specific bulk + `nusb`/libusb (her iki OS'te surucusuz/WinUSB gerekir) daha dusuk gecikme verebilir - **tahmin, dogrulanmadi**.
3. **Harici USB-UART bagdastirici** (USART3 veya LPUART1, 1.8 V): 3+ Mbaud mumkun ama 1.8 V seviye uyumu sart; yedek olarak.
4. **SWD + RTT (defmt)**: yalniz telemetri/log, kontrol dongusu icin kullanma. probe-rs destekli.
5. **FDCAN + USB-CAN bagdastirici**: yalnizca CAN davranisinin kendisini test etmek icin; transceiver (1.8 V uyumlu veya seviye ceviricili) gerekir.
6. Ethernet, SPI-slave tipi HIL: kartta/uygulamada yok; onerilmez.

100-1000 Hz icin yeterli olan: **USB HS CDC** (ana yol), ST-LINK VCP en az 2-3 Mbaud olculerek dogrulanirsa yedek; 115200-921600 yetmez veya sinirda.

### 2.3 Rust destegi durumu (2026-11-10 itibariyla)

Surumler: `embassy-stm32` 0.6.0 (2026-03-20, crates.io), `embassy-executor` 0.10.0, `stm32-metapac` 21.0.0, `rtic` 2.4.0 (2026-10-21), probe-rs v0.32.0 (2026-07-22). Embassy `main` hizli ilerliyor (son 40 gunde >=100 commit) ve 0.6.0'dan API olarak ayriliyor (ornegin 2026-09 "tek birlesik ADC surucusu", "api guidelines hizalama"). Embassy repo rust-toolchain.toml stable 1.97 sabitliyor, yani nightly gerekmiyor. Hedef `thumbv8m.main-none-eabihf` Rust'ta Tier 2 (host araci yok; `core`/`alloc` hazir derlenir).

**Cip destegi**
- `embassy-stm32/Cargo.toml` icinde `stm32u5a9nj` ozelligi var. Cip JSON'u (STM32U5A9NJ.json) su cevre birimlerini tanimliyor: ADC1, ADC2, ADC4, FDCAN1, USB_OTG_HS, DSIHOST, LTDC, DMA2D, GPU2D, GFXMMU, HSPI1, OCTOSPI1/2, OCTOSPIM, SDMMC1/2, I2C1-6, SPI1-3, USART1/2/3/6, UART4/5, LPUART1, TIM1/2/3/4/5/6/7/8/15/16/17, LPTIM1-4, GPDMA1, LPDMA1, SAI1/2, UCPD1, RNG, RTC, IWDG/WWDG. Hicbiri "eksik" olarak isaretli degil ama "tanimli" olmak "calisiyor" demek degil.
- Embassy suruculer (kaynak agaci): `usart` (DMA'li, buffered, ringbuffered), `can/fd` (FDCAN), `timer` (simple_pwm, complementary_pwm, input_capture, qei), `adc` (v3: U5 ADC1/2; v2: U5 ADC4), `usb/otg.rs`, `dsihost`, `ltdc`, `dma2d`, `gpu2d`, `hspi`, `ospi`, `dma/gpdma`.

**Dogrulama kanitlari (donanimda calistirilmis olan)**
- Embassy CI "HIL" testleri `stm32u585ai` ve `stm32u5a5zj` icin derleniyor (tests/stm32/Cargo.toml); U5A9NJ icin HIL testi **yok**. U5A5ZJ ve U5A9NJ ayni "U5" ailesi ama ayni die degil (metapac'ta U5A9 "STM32U599/5A9" hattinda, DIE481); sonuc bire bir aktarilamaz.
- `examples/stm32u5` ornekleri: adc, can, fd_can (NUCLEO-U545RE-Q'da test edilmis, dahili loopback), usb_hs_serial, usb_serial, spi/spi_dma, i2c, dac, flash, ltdc (U5G9J-DK2), hspi_memory_mapped (U5G9J-DK2, MX66LM1G), tsc, pdm, boot, rng. U5A9J-DK icin ornek **yok**; `Cargo.toml` varsayilani `stm32u5g9zj`.
- probe-rs: `STM32U5_Series.yaml` icinde `STM32U5A9NJ` (paket `STM32U5A9NJHxQ`) mevcut; algoritmalar `stm32u59x_4m_0800/0c00` (dahili flash) ve `mx25lm51245g_stm32u599j-dk` (harici OctoSPI flash). Chip adi: `STM32U5A9NJHxQ`. probe-rs README/doc ST-LINK V3 icin firmware >= V3 3.2 istiyor (Windows kurulum sayfasi).

**Bilinen sorunlar (acik/yeni)**
- USB OTG HS: Embassy #6115 (STM32U5A5AJ, "Non-Functional"), #3815 (U595, Windows "Invalid Configuration Descriptor"). Kok neden: RM0456 15.2.3'e gore SYSCFG_OTGHSPHYTUNER2'de COMPDISTUNE/SQRXTUNE ayari yapilmiyordu. PR #7439 (2026-11-06, `main`, merge 7199a75) ile duzeltildi; #6115 2026-11-09'da kapandi. PR metni: "U5A5 uzerinde ayni yazma uygulamadan yapildiginda USB cihaz modu calisiyor; bu dalin kendisi donanimda calistirilmadi". Yani: 0.6.0 ile U5A9'da USB HS kullanilamaz sayilmali; git `main` gerekir ve ilk is olarak kartta dogrulanmali.
- LTDC: #5568 (U5G9J-DK2 "LTDC example broken", acik; 2026-04'te ek DMA2D/LTDC guncellemeleri soylenmis, sonuc belirsiz).
- DMA'li UART: #3900/#4452 (U5'te `read_until_idle` bozuk veri) 2025-12-05'te kapandi; ring-buffer destegi GPDMA'ya eklendi (#2059). Acik: #6835 (DMA/timer panikleri; LLM ile uretilmis rapor, dogrulanmamis).
- OCTOSPI: #4069 (OCTOSPIM Port 2 hic yapilandirilmiyor; DK flash'i Port 1'de, etkilenmeyebilir, **dogrulanmadi**).
- Dahili flash silme: #3561 (U5 cift banka/sayfa boyutu; yalniz Embassy `flash` API'sini kullanirsaniz; probe-rs ile yukleme etkilenmez).
- Guc/regulator: Embassy `rcc/u5.rs` SMPS/regulator secimini (PWR_CR3) yapmiyor; yalnizca VOS ve booster. Kart dahili SMPS'ten (VDDSMPS/VLXSMPS) 1.8 V ile 160 MHz Range1'de calismali; Embassy'nin varsayilan kosusunun bu kartta 160 MHz'de kararli olup olmadigi **dogrulanmadi** (spike'ta olculmeli).

**Ekran (DSI/LTDC)**
- Embassy `dsihost` modulu (mod, panel trait'i, PHY) 2026-03'te genislemis (PR #5727, #5730, #5754). DSI orneklerinin hepsi stm32h747/f469 icin. U5 DSI'si icin docs.embassy.dev'de `stm32u5g9zj`/`stm32u5f9zj` DsiHost sayfalari var; U5A9NJ'de `DSIHOST` cevre birimi cip verisinde tanimli.
- Panel: UM2967 modul parca numarasini veriyor (J025F1CN0201W/N); denetleyici cipi UM2967'de yazmiyor. Zephyr MB1835 kalkan belgesi bu modulde Himax HX8379C diyor (U5G9J-DK1 icin yazilmis; MB1835 U5x9J-DK ailesi icin ortak, UM2967 ile uyumlu).
- Sonuc: Rust'ta ekran icin panel baslatma dizisi, DSI saat tersleme (UM2967 s.27: DSI CK_N/CK_P yazilimla ters cevrilmeli) ve dokunmatik (I2C5) surucusu sifirdan yazilmali. Otopilot gereksinimi degil.

**RTIC alternatifi**
- RTIC 2.4.0 README: "All Cortex-M devices are fully supported" (yani M33 dahil); `rtic-monotonics` STM32 TIM2-5, TIM15 vb. icin `stm32-metapac` uzerinden monotonic saglar. RTIC kendi basina HAL degildir; U5A9 icin yine `embassy-stm32` (zaman surucusu kapali) veya PAC kullanilir, ki bu da yukaridaki cevre birimi olgunlugunu degistirmez. RTIC ile U5A9 uzerinde calisan bir ornek **bulunamadi**. RTIC'in avantaji: oncelik tavani (SRP) ile statik zamanlama analizi; Embassy'nin avantaji: async + USB/CAN yigini (embassy-usb) ile ayni cati altinda.

**Diger toplulukla ilgili**
- ST resmi Rust destegi yok (ST moderatoru Ekim 2025: yakin gelecekte plan yok; kaynak: ST topluluk iletisi). Rust yolunda ST destek kanali yok; sorun yatagi Embassy GitHub/Matrix.

### 2.4 C alternatifi

- STM32CubeU5 v1.9.0 (2026-07-01): HAL/LL, BSP `STM32U5x9J-DK`, ortak ara katmanlar (ThreadX, FileX, LevelX, NetXDuo, USBX, OpenBootloader, mbed-crypto, mcuboot). `Middlewares/Third_Party` altinda FreeRTOS **gorunmuyor** (yalniz mbed-crypto, mcuboot); FreeRTOS CubeMX/CubeIDE paketi (`com.st.stm32cube.ide.mcu.freertos` yerelde var) uzerinden gelir.
- DK projeleri: `Projects/STM32U5x9J-DK`: Applications (FileX, OpenBootloader, ThreadX, USBX - USBX orneklerinde Ux_Host_CDC_ACM/HID/HUB_HID_MSC host agirlikli), Examples (BSP, DMA, DSI, FLASH, GPIO, OSPI, PWR, RAMCFG, RCC, RTC). FDCAN/TIM/UART/USB-cihaz ornegi bu dizinde yok (Nucleo-U5A5ZJ-Q ornekleri var, ayni dil ve HAL, farkli kart).
- Yerel kurulum (bu makine): STM32CubeMX 6.18, STM32CubeIDE 2.2.0, STM32CubeProgrammer v2.23.0 (arm64), CubeIDE icinde arm64 `arm-none-eabi-gcc` 14.3.1 (GNU Tools for STM32 14.3.rel1), cmake, ST-LINK gdbserver. STM32CubeCLT ayri kurulu degil.
- STM32CubeCLT: v1.21.0 (Subat 2026) Apple Silicon yerel destegi ekledi (ST topluluk duyurusu). ST, "bundle manager"in (`cube` CLI) CubeCLT'yi ileride degistirecegini soyluyor (2026-04-03), tarih yok. Yani CLI C akisi icin Cube araclarinin kendisi de gecis doneminde.
- CubeMX CMake projesi uretir (CMakePresets.json + toolchain dosyasi); bildirilen sorunlar: VS Code ile acilista Ninja cokmesi (topluluk).
- TouchGFX: Designer macOS'ta yerel degil (ST topluluk yanitlari; 2026 yanitlari "calisiliyor" diyor, onaylanmadi). Ekran arayuzu istenirse Windows gerekir.
- Zephyr (ucuncu bir C yolu, bu rapor kapsaminda yalniz not): U5A9J-DK "Maintained"; `supported` listesinde gpio, adc, uart, spi, i2c, flash, sdmmc, timer, rng, rtc, watchdog var; CAN/PWM/USB DTS'te tanimli ama yaml `supported` listesinde yok. OpenOCD/CubeProgrammer ile yukleme.

**Karma yaklasim (C surucu + Rust cekirdek, FFI) pratikligi** - kaynakli birincil veri bulunamadi; asagisi muhendislik degerlendirmesidir:
- Mumkun: Rust `staticlib` + CubeMX C projesi (CMake) ya da Rust ana programi + `cc` ile C HAL. Gerceklik: iki ayri baslangic/vektor tablosu/linker betigi, `cortex-m-rt` ve CMSIS `startup_*.s` cakismasi, `bindgen` icin libclang (Windows'ta ek kurulum), iki ayri hata ayiklama baglami, iki ayri HAL'in ayni cevre birimini kullanma riski.
- Makul tek istisna: yalniz ekran/DSI baslatma gibi tek seferlik, cevre birimi baska yerde kullanilmayan bir kutuphane parcasi (gerekirse).

### 2.5 macOS ve Windows'ta derleme, yukleme, hata ayiklama (komut satiri akisi)

**Rust yolu**
- Araclar: `rustup` (stable, hedef `thumbv8m.main-none-eabihf`), `probe-rs-tools` (`cargo binstall probe-rs-tools` / macOS Homebrew `probe-rs/probe-rs` / resmi kurulum betikleri), `defmt` + `defmt-rtt`, `flip-link` istege bagli. Calistirma: `.cargo/config.toml` icinde `runner = "probe-rs run --chip STM32U5A9NJHxQ"` (Embassy ornegi `STM32U5G9ZJTxQ` kullanir; kendi chip adini `probe-rs chip list` ile dogrula).
- macOS: surucu gerekmez (probe-rs belgesi). Bu makine: rustup/cargo/probe-rs kurulu degil (kurulmadi, kontrol edildi).
- Windows: ST-LINK icin ST resmi surucusu (probe-rs belgesi; UM2967 s.17 "Windows 10 icin gerekmez" diyor - ikisi celisiyor, test et); MSVC derleme araclari (Visual Studio Build Tools "Desktop development with C++" - rustup kitabi) host tarafindaki build.rs/derleme icin. Windows ARM disi.
- Hata ayiklama: `probe-rs gdb`/DAP (VS Code probe-rs-debugger eklentisi) + RTT. SWO yok (bkz. 2.1).
- Bilinen: probe-rs #3516 (ST-LINK `--connect-under-reset` bazi kartlarda calismiyor; acik, V3MINIE/ozel kart), #3408 (U5'te HardFault yigin izi alinamiyor; 2025-12-23'te kapandi).
- Yukleme dis aracı: STM32CubeProgrammer CLI (`STM32_Programmer_CLI`) option byte (TZEN/RDP), ST-LINK firmware guncelleme ve kurtarma icin yine gerekir.

**C yolu**
- Derleme: arm-none-eabi-gcc (CubeIDE icinde 14.3.rel1; arm64 yerel), CMake + Ninja (CubeMX'in urettigi `CMakePresets.json`), yukleme `STM32_Programmer_CLI -c port=SWD -w ... -rst` veya ST-LINK_gdbserver + arm-none-eabi-gdb; Windows ayni araclar. CLT kurulumu macOS arm64'te v1.21.0 ve sonrasi.
- Bilinen: CubeIDE macOS 27 desteği yok (topluluk Eylul 2026: son desteklenen macOS 26; bu makine 26.5.2); Rosetta 2'nin macOS 27'de son surum olacagi soyleniyor (ilgili CLT konusu topluluk iletisi).

---

## 3. Mimari icin oneriler

1. **Firmware dili: Rust (Embassy HAL) + HAL'dan bagimsiz `no_std` kontrol cekirdegi**, C yalniz referans ve geri donus yolu.
   - Gerekce: (a) cip destegi (U5A9NJ ozelligi + FDCAN/USB/timer/ADC suruculeri); (b) probe-rs hedefi hazir ve macOS/Windows'ta komut satiri akisi tek arac zinciri; (c) ayni cekirdek SIL'de PC'de derlenir (`embassy-executor` `platform-std` ozelligi var); (d) kart "gecici": Embassy ayni API ile STM32'nin diger aileleri (ve baska uretici) hedefler, kart degisimi cekirdegi etkilemez; (e) Embassy gelisim temposu yuksek.
   - Cekirdegi `embedded-hal` 1.0 / kendi `trait`lerine (Imu, Baro, Actuator, Transport) dayandirin; Embassy'ye `hal-stm32u5` ince kabuginde dokunun. Boylece RTIC veya C'ye gecis kabuk degisimi olur.
2. **HIL ana taşıyıcı: USB HS CDC (CN4)**, yedek: ST-LINK VCP (en az 2-3 Mbaud olcumle) ve log icin RTT. Cerceveleme: COBS + CRC16/32, sabit kare boyu, sayac/zaman damgali; `Transport` trait'i ile SIL'de ayni kodu gercek soket/kanal uzerinden kullanin. Bu kartta sensor/aktuator gercek olmadigi icin aktuator cikislari ana yolda USB ile PC'ye gider; gercek PWM cikislari yalnizca zamanlama dogrulamasi icin ikincil.
3. **Embassy surumu**: USB HS gerektigi icin `embassy-stm32` git `main` (PR #7439 merge'i dahil, rev sabitlenmis) kullanin; 0.6.0'a sabitlenirseniz USB HS yok. `Cargo.lock` + rev sabitleme + `embassy` ve `stm32-metapac` surumlerini birlikte kilitleyin (API'ler hizli degisiyor).
4. **Donanim cevresi**: kart 1.8 V; HIL'de PWM/UART/CAN'in harici dunya ile baglandigi her sinyal icin 1.8 V uyumlu seviye cevirici veya 1.8 V VIO'lu transceiver. FDCAN icin PD0/PD1 + I2C6'dan vazgecmek gerekiyor.
5. **Spike (go/no-go) kriterleri, 1-2 gun**: (1) blinky + defmt/RTT probe-rs ile her iki OS'te; (2) 160 MHz saatin kararli olup olmadigi (Range1, SMPS) ve TZEN durumu; (3) USB HS CDC: echo + 256 B @ 1 kHz suresince sayac kaybi/gecikme, macOS ve Windows'ta; (4) FDCAN: dahili loopback sonra harici transceiver; (5) PWM: TIM1/2/3/4 kanallari analizorle; (6) USART3/LPUART1 DMA ring-buffer 2-3 Mbaud; (7) ST-LINK VCP yedek hizi.
6. **Basarisizlik halinde geri donus**: USB HS calismazsa -> VCP (olculmus hizda) veya harici USB-UART; Embassy DMA/USB kararsizsa -> ayni kontrol cekirdegi RTIC + PAC veya CubeMX/ThreadX+USBX C kabugu ("cekirdek Rust, kabuk C" ancak o zaman FFI).
7. **Cube araclarini koruyun**: CubeProgrammer (option byte, ST-LINK FW) ve CubeMX (pin/saat dogrulama referansi) Rust projesinin yan araclari olarak kalsin. Windows tarafinda ayni araclar ve ST-LINK surucusu ayrica dogrulanmali.
8. **Ekrani kapsam disi birakin** (DSI/HX8379C Rust surucusu yok, TouchGFX macOS'ta yok). Durum gostergesi icin LED + log yeterli.
9. **Torpido hedef algilama notu** (kapsam disi ama iliskili): kartta kamera konnektoru yok; DCMI/PSSI cipte var ama kart pinlerine cikmiyor. Algilama icin sensor secimi ayri arastirilmali.

---

## 4. Riskler ve acik sorular

| # | Risk / soru | Etki | Durum |
|---|---|---|---|
| R1 | USB OTG HS duzeltmesi yalniz `main`'de, donanimda dogrulanmadi; kararlilik ve Windows ile uyum bilinmiyor | HIL ana yolu | Spike (3) |
| R2 | Bu kart icin Embassy ornegi/HIL testi yok; U5A9 farkli die/kart | Beklenmedik cevre birimi hatalari | Spike |
| R3 | SMPS/regulator ve 1.8 V VDD ile 160 MHz kosulu Embassy tarafindan ayarlanmiyor | Kararsiz calisma / dusuk saat | Dogrulanmadi |
| R4 | Fabrika TZEN/RDP durumu belirsiz; yanlis option byte karti kilitler | Kart kaybi | CubeProgrammer ile once oku, yazma |
| R5 | ST-LINK-V3E VCP ust baud/gecikmesi birincil kaynakta yok | Yedek yolun yeterliligi | Olc |
| R6 | 1.8 V GPIO; 5 V toleransi dogrulanmadi | Donanim hasari | Seviye cevirici zorunlu varsayim |
| R7 | Tek FDCAN ve konnektorde etiketsiz pinler | CAN gerekiyorsa mimari kisit | PD0/PD1 |
| R8 | Embassy API degisimi ve surum sabitleme yuku; ST destegi yok | Bakim maliyeti | rev sabitle |
| R9 | Windows'ta ST-LINK surucusu (ST "gerekmez" - probe-rs "gerekir") ve MSVC gereksinimi | Kurulum surtunmesi | Windows'ta dene |
| R10 | Kart gecici: sonraki kartta farkli cevre birimleri/pinler | Yeniden calisma | HAL-bagimsiz cekirdek |
| R11 | TouchGFX/ekran stack'i macOS'ta yok; Rust'ta DSI paneli yok | Ekran gerekirse | Kapsam disi |
| R12 | Cube DK ornekleri sinirli (UART/TIM/FDCAN/USB yok) | C yolu "referans" degeri dusuk | Nucleo-U5A5ZJ ornekleri |
| R13 | CubeCLT -> bundle manager gecisi (tarih yok) | C CLI akisinin ileride degismesi | Izle |

Acik sorular:
- DK'nin fabrikada TZEN'i kapali mi? (CubeProgrammer ile salt-okunur oku.)
- Embassy `main` + U5A9NJ ile `Config::default()` 160 MHz'e (SMPS ile) cikiyor mu?
- Windows 11'de ST-LINK-V3E icin ek surucu gerekli mi?
- USB HS CDC'nin macOS/Windows'taki gercek tur-gecikmesi ve p99 jitter'i?
- Gercek CAN gereksinimi var mi (yarisma tarafindan)? Yoksa FDCAN riski silinir.

---

## 5. Bagimsiz dogrulanmasi gereken kritik iddialar

1. **"embassy-stm32 0.6.0'da `stm32u5a9nj` ozelligi var ve `stm32-metapac` U5A9NJ'de FDCAN1, USB_OTG_HS, DSIHOST, HSPI1, TIM1/2/3/4/8/15/16/17, ADC1/2/4 tanimli."** Kaynak: https://github.com/embassy-rs/embassy/blob/main/embassy-stm32/Cargo.toml (satir ~1990) ve https://github.com/embassy-rs/stm32-data-generated/blob/main/data/chips/STM32U5A9NJ.json
2. **"Embassy'de U5 USB OTG HS, PHY ayari eksik oldugu icin calismiyordu; duzeltme 2026-11-06 `main`'de (PR #7439), 0.6.0'da yok, PR yazarinin dali donanimda calistirilmadi."** Kaynak: https://github.com/embassy-rs/embassy/pull/7439 , https://github.com/embassy-rs/embassy/issues/6115 , https://crates.io/crates/embassy-stm32 (0.6.0 = 2026-03-20)
3. **"probe-rs 0.32.0, `STM32U5A9NJ` (STM32U5A9NJHxQ) hedefini dahili flash algoritmasiyla destekliyor."** Kaynak: https://github.com/probe-rs/probe-rs/blob/master/probe-rs/targets/STM32U5_Series.yaml (satir ~4141) ve https://github.com/probe-rs/probe-rs/releases
4. **"U5A9J-DK yalniz 1.8 V VDD icin tasarlanmis; genisleme konnektoru IOREF = VDD (1.8 V); VDDA = 1.8 V, VREFBUF kullanilamaz."** Kaynak: UM2967 Rev 2 s.2, s.18, s.22-24: https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf (bu oturumda erisilemedi; kopya https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf)
5. **"Kartta tek FDCAN (FDCAN1) var, transceiver yok; FDCAN1 pinleri PA11/PA12 USB-C HS ile cakisir; konnektorda ulasilabilir tek aday PD0/PD1 (CN10-35/37, varsayilan I2C6)."** Kaynak: UM2967 Tablo 29/30/34 + STM32U5A9NJ.json FDCAN1 pin listesi (AF9: PA11/PA12, PB8/PB9, PD0/PD1, PF7/PF8, PH13/PH14).
6. **"ST-LINK VCP, USART1 PA9/PA10; ST-LINK-V3E VCP'nin ust baud hizi ve gecikmesi birincil kaynakta dogrulanamadi."** Kaynak: UM2967 s.17 (Tablo 6, 8.4); V3MINIE icin ST: https://www.st.com.cn/resource/en/user_manual/um2910-stlinkv3minie-debuggerprogrammer-tiny-probe-for-stm32-microcontrollers-stmicroelectronics.pdf (736 bps-16 Mbps); topluluk: https://community.st.com/stm32-mcus-boards-and-hardware-tools-26/st-link-v3-bug-large-uart-strings-with-fast-baud-rate-fails-lost-characters-56012
7. **"STM32CubeCLT v1.21.0 (Subat 2026) macOS Apple Silicon'u yerel destekliyor; bu makinede CubeIDE 2.2.0 arm64 gcc 14.3.1 ve CubeProgrammer 2.23.0 kurulu."** Kaynak: https://community.st.com/stm32cubeide-mcus-28/stm32cubeclt-1-21-0-released-163146 ; yerel: `/Applications/STM32CubeIDE.app/.../gnu-tools-for-stm32.14.3.rel1.macosaarch64...`, `STM32_Programmer_CLI --version`
8. **"Embassy HIL testleri yalniz stm32u585ai ve stm32u5a5zj icin var (U5A9NJ icin yok)."** Kaynak: https://github.com/embassy-rs/embassy/blob/main/tests/stm32/Cargo.toml (satir ~408-409)
9. **"RTIC 2.4.0 tum Cortex-M cihazlarini tam destekler; STM32 monotonic'ler `stm32-metapac` ile."** Kaynak: https://github.com/rtic-rs/rtic/blob/master/README.md , https://github.com/rtic-rs/rtic/blob/master/rtic-monotonics/Cargo.toml
10. **"TouchGFX Designer macOS'ta yerel degil; STM32CubeU5 DK dizininde FDCAN/TIM/UART/USB-cihaz ornegi yok."** Kaynak: https://community.st.com/t5/stm32-mcus-touchgfx-and-gui/x-cube-touchfgx-software-on-mac/m-p/782755 ; https://github.com/STMicroelectronics/STM32CubeU5 (Projects/STM32U5x9J-DK/Examples listesi).

---

## 6. Kaynaklar

**ST resmi / kart**
- UM2967 STM32U5A9J-DK kullanim kilavuzu (Rev 2, Mart 2023): https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf (erisilemedi; kopya https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf)
- Kart urun sayfasi: https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html ; MCU sayfasi: https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html (dogrudan okunamadi; arama ozetleri)
- STLINK-V3MINIE UM2910: https://www.st.com.cn/resource/en/user_manual/um2910-stlinkv3minie-debuggerprogrammer-tiny-probe-for-stm32-microcontrollers-stmicroelectronics.pdf
- AN5347 TrustZone: https://www.st.com/content/st_com/en/technical-documents/an5347-introduction-to-arm-trustzone-features-on-stm32l5-stm32u5-and-stm32u3-mcus-stmicroelectronics_asset/chunk-004-topic-an5347-security-tz-enabled.html
- STM32CubeU5: https://github.com/STMicroelectronics/STM32CubeU5
- ST topluluk, bricked U5A9J-DK: https://community.st.com/t5/stm32cubeprogrammer-mcus/bricked-stm32u5a9j-dk/td-p/802238
- ST topluluk, CubeCLT 1.21.0: https://community.st.com/stm32cubeide-mcus-28/stm32cubeclt-1-21-0-released-163146
- ST topluluk, CubeCLT gelecegi: https://community.st.com/t5/stm32cubeide-for-visual-studio/the-future-of-stm32cubeclt/td-p/889860
- ST topluluk, TouchGFX macOS: https://community.st.com/t5/stm32-mcus-touchgfx-and-gui/x-cube-touchfgx-software-on-mac/m-p/782755
- ST topluluk, Rust destegi: https://community.st.com/stm32-mcus-embedded-software-32/rust-support-43741
- ST topluluk, ST-LINK V3 VCP hizli veri kaybi: https://community.st.com/stm32-mcus-boards-and-hardware-tools-26/st-link-v3-bug-large-uart-strings-with-fast-baud-rate-fails-lost-characters-56012

**Rust / Embassy / probe-rs / RTIC**
- embassy-stm32 Cargo.toml: https://github.com/embassy-rs/embassy/blob/main/embassy-stm32/Cargo.toml
- examples/stm32u5: https://github.com/embassy-rs/embassy/tree/main/examples/stm32u5 (usb_hs_serial.rs, fd_can.rs, ltdc.rs, hspi_memory_mapped.rs, adc.rs)
- embassy-stm32 DSIHOST: https://github.com/embassy-rs/embassy/tree/main/embassy-stm32/src/dsihost ; docs: https://docs.embassy.dev/embassy-stm32/git/stm32u5g9zj/dsihost/struct.DsiHost.html
- embassy-stm32 u5 RCC: https://github.com/embassy-rs/embassy/blob/main/embassy-stm32/src/rcc/u5.rs
- Embassy HIL testleri: https://github.com/embassy-rs/embassy/blob/main/tests/stm32/Cargo.toml
- Embassy rust-toolchain: https://github.com/embassy-rs/embassy/blob/main/rust-toolchain.toml
- Issue/PR: https://github.com/embassy-rs/embassy/pull/7439 ; https://github.com/embassy-rs/embassy/issues/6115 ; https://github.com/embassy-rs/embassy/issues/3815 ; https://github.com/embassy-rs/embassy/issues/5568 ; https://github.com/embassy-rs/embassy/issues/4452 ; https://github.com/embassy-rs/embassy/issues/3561 ; https://github.com/embassy-rs/embassy/issues/4069 ; https://github.com/embassy-rs/embassy/issues/6835
- stm32-data-generated U5A9NJ: https://github.com/embassy-rs/stm32-data-generated/blob/main/data/chips/STM32U5A9NJ.json
- crates.io embassy-stm32: https://crates.io/crates/embassy-stm32 ; embassy-executor (platform-std): https://github.com/embassy-rs/embassy/blob/main/embassy-executor/Cargo.toml
- probe-rs STM32U5 hedef tanimi: https://github.com/probe-rs/probe-rs/blob/master/probe-rs/targets/STM32U5_Series.yaml ; surumler: https://github.com/probe-rs/probe-rs/releases ; issues: https://github.com/probe-rs/probe-rs/issues/3516 , https://github.com/probe-rs/probe-rs/issues/3408
- probe-rs kurulum / probe kurulumu: https://probe.rs/docs/getting-started/installation/ , https://probe.rs/docs/getting-started/probe-setup/
- RTIC: https://github.com/rtic-rs/rtic ; rtic-monotonics: https://github.com/rtic-rs/rtic/blob/master/rtic-monotonics/Cargo.toml
- Rust platform support (thumbv8m.main-none-eabihf Tier 2): https://doc.rust-lang.org/nightly/rustc/platform-support.html
- rustup Windows MSVC: https://rust-lang.github.io/rustup/installation/windows-msvc.html

**Zephyr (ikincil kanit)**
- Kart belgesi: https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html
- DTS: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/st/stm32u5a9j_dk/stm32u5a9j_dk.dts
- MB1835 kalkani (HX8379C): https://docs.zephyrproject.org/latest/boards/shields/st_lcd_dsi_mb1835/doc/index.html

**Yerel kontrol (bu makine, salt okunur)**
- Yok: rustc, cargo, rustup, probe-rs, ayri arm-none-eabi-gcc, ayri cmake/ninja, openocd.
- Var: `/Applications/STM32CubeIDE.app` (2.2.0; icinde arm64 arm-none-eabi-gcc 14.3.1, cmake, ST-LINK_gdbserver), `/Applications/STMicroelectronics/STM32CubeMX.app` (6.18), `/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer` (v2.23.0), Homebrew 7.0.13, Python 3.12.4, macOS 26.5.2 arm64.
- Kart bagli degildi: `STM32_Programmer_CLI -l st` "No ST-Link detected!".
