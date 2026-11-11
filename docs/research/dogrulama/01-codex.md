# Codex doğrulaması: 01-kart-ve-arac-zinciri

## Hüküm tablosu

Raporun tamamı okundu; dosyalar değiştirilmedi ve hiçbir şey kurulmadı. Kontrol tarihi: **10 Kasım 2026**. İlk 10 satır, raporun Bölüm 5’indeki maddelerle eşleşir.

**Erişim sınırı:** ST’nin güncel UM2967 PDF’si tam olarak alınamadı; boyut sınırı ve zaman aşımı hataları oluştu. Bu nedenle güncel revizyon doğrulanmış sayılmadı. ST tarafından yazılmış **UM2967 Rev 2’nin raporda belirtilen kopyası açıldı ve ilgili bölümleri okundu**. Datasheet için ST’nin **DS13543 Rev 3** belgesi okundu; bu ortak belge STM32U5A9NJ’yi kapsıyor. Donanım üzerinde çalışma, hız veya gecikme testi yapılmadı.

| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
|---|---|---|---|---|
| 1 | Embassy 0.6.0’da `stm32u5a9nj`; belirtilen çevre birimleri metaveride mevcut | DOĞRU | [0.6.0 özellikleri](https://docs.rs/crate/embassy-stm32/0.6.0/features), [çip verisi](https://github.com/embassy-rs/stm32-data-generated/blob/main/data/chips/STM32U5A9NJ.json) | FDCAN1, USB_OTG_HS, DSIHOST, HSPI1, belirtilen TIM ve ADC birimleri tanımlı. Bu, çalışan kart sürücüsü kanıtı değildir. |
| 2 | PR #7439 PHY düzeltmesi, birleşme tarihi ve 0.6.0 dışında olması | KISMEN DOĞRU | [PR #7439](https://github.com/embassy-rs/embassy/pull/7439), [0.6.0 USB kaynağı](https://github.com/embassy-rs/embassy/blob/embassy-stm32-v0.6.0/embassy-stm32/src/usb/otg.rs), [birleşen sürümün changelog’u](https://github.com/embassy-rs/embassy/blob/7199a750082764413b427c2266dbb48c31f74d3b/embassy-stm32/CHANGELOG.md) | 6 Kasım 2026’da `7199a750…` ile birleşmiş. Düzeltme 0.6.0’da yok; **Unreleased** altında. Ancak “0.6.0’da USB HS kullanılamaz” genellemesi kanıtlanmıyor. |
| 3 | probe-rs 0.32.0’da `STM32U5A9NJHxQ` ve dahili flash algoritması var | DOĞRU | [v0.32.0 hedef dosyası](https://github.com/probe-rs/probe-rs/blob/v0.32.0/probe-rs/targets/STM32U5_Series.yaml), [sürüm](https://github.com/probe-rs/probe-rs/releases/tag/v0.32.0) | `STM32U5A9NJ`, paket karşılığı ve `stm32u59x_4m_0800/0c00` mevcut. Fiziksel kartta yükleme doğrulanmadı. |
| 4 | Kart VDD/VDD_MCU=1,8 V; IOREF=VDD; mevcut VDDA ile VREFBUF kullanılamaz | DOĞRU | [UM2967 Rev 2, s.18, 23–24, 47](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf) | Kart besleme düzeni için doğru. |
| 5 | Tek FDCAN; transceiver yok; CN10 üzerinden PD0/PD1 kullanılabilir | DOĞRU | [UM2967](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf), [DS13543, Tablo 27–29](https://www.st.com/resource/en/datasheet/stm32u5a9bj.pdf) | PD0=RX, PD1=TX; CN10-35/37. PA11/PA12 USB ile ortak. Sonuç CN9/CN10 erişimi içindir. |
| 6 | VCP USART1 PA9/PA10’da; V3E için birincil kaynakta azami hız bulunamıyor | KISMEN DOĞRU | [UM2967, §8.4](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf), [UM2525, §6.3](https://www.st.com/resource/en/user_manual/dm00569152.pdf) | Bağlantı doğru. ST, **V3E için 15 MHz VCP** değeri yayımlamış. DK üzerindeki sürdürülebilir hız ve gecikme ayrıca ölçülmeli. |
| 7 | CubeCLT 1.21.0 Apple Silicon desteği; yerel CubeIDE/GCC/Programmer sürümleri | DOĞRU | [ST’nin 25 Şubat 2026 duyurusu](https://community.st.com/stm32cubeide-mcus-28/stm32cubeclt-1-21-0-released-163146), [yerel IDE bilgisi](/Applications/STM32CubeIDE.app/Contents/Info.plist), [Programmer sürüm dosyası](/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin/version) | IDE 2.2.0; GCC 14.3.1 ve Mach-O arm64; Programmer sürüm dosyası 2.23.0. |
| 8 | Embassy’nin U5 HIL hedefleri U585AI ve U5A5ZJ; U5A9NJ yok | DOĞRU | [HIL Cargo.toml](https://github.com/embassy-rs/embassy/blob/main/tests/stm32/Cargo.toml) | **U5 ailesi içinde** doğru. Hedef tanımı, her testin başarıyla donanımda koştuğunu tek başına göstermez. |
| 9 | RTIC 2.4.0 Cortex-M desteği; STM32 monotonikleri metapac üzerinden | DOĞRU | [v2.4.0 README](https://github.com/rtic-rs/rtic/blob/v2.4.0/README.md), [monotonik bağımlılıkları](https://github.com/rtic-rs/rtic/blob/master/rtic-monotonics/Cargo.toml) | Cortex-M desteği, U5A9 çevre birimlerinin hazır veya kartta sınanmış olduğu anlamına gelmez. |
| 10 | TouchGFX Designer macOS native değil; DK dizininde belirtilen örnekler yok | DOĞRU | [TouchGFX gereksinimleri](https://support.touchgfx.com/docs/introduction/prerequisites), [CubeU5 v1.9.0 Examples](https://github.com/STMicroelectronics/STM32CubeU5/tree/v1.9.0/Projects/STM32U5x9J-DK/Examples), [USBX dizini](https://github.com/STMicroelectronics/STM32CubeU5/tree/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX) | Designer Windows’a özgü. İncelenen DK örnek dizininde FDCAN/TIM/UART klasörleri ve USBX device örneği yok. |
| 11 | Dışarı kullanılabilecek UART sayısı ikiyle sınırlı | KISMEN DOĞRU | [DS13543, s.117, 146](https://www.st.com.cn/resource/en/datasheet/stm32u5a5qi.pdf), [USART1 alternatifleri](https://www.st.com/resource/en/datasheet/stm32u5a9bj.pdf) | Etiketli USART3/LPUART1 doğru; **USART6 PF0/PF1** ve USART1 PG9/PG10 alternatifleri atlanmış. |
| 12 | Üç I2C var; I2C1 kullanılırsa ADC pinlerinden vazgeçilmeli | KISMEN DOĞRU | [UM2967, Tablo 29–30](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf) | I2C1 ayrıca **CN10-2/4** üzerinde. ADC’den vazgeçmek zorunlu değil. |
| 13 | Dışarı çıkan SPI sayısı iki | KISMEN DOĞRU | [DS13543, Tablo 27–28](https://www.st.com/resource/en/datasheet/stm32u5a5qj.pdf) | Etiketli SPI2/SPI3 doğru; PA4–PA7 üzerinden SPI1 de mümkün. |
| 14 | Dışarı çıkan ADC girişi sayısı yedi | KISMEN DOĞRU | [DS13543 pin tablosu](https://www.st.com/resource/en/datasheet/stm32u5a5qj.pdf), [PD12 ADC4_IN16](https://www.st.com/resource/en/datasheet/stm32u5a9zj.pdf) | Yedi, etiketli kullanım sayısıdır. İşlev değiştirerek **13 fiziksel ADC pinine** ulaşılabilir. |
| 15 | Dört belirgin PWM pini ve SPI3 ile ortak iki PWM pini var | DOĞRU | [UM2967, Tablo 30](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf) | Bu altı eşleşme doğru; toplam kapasite veya bağımsız frekans sayısı değildir. |
| 16 | Kartta Ethernet yok | DOĞRU | [ST kart sayfası](https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html), [MCU datasheet](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf) | Yerleşik Ethernet MAC/PHY/RJ45 yolu bulunmuyor. |
| 17 | USB-C yalnızca device olabilir | KISMEN DOĞRU | [ST USBX host örneği ve donanım notu](https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/README.md) | **Sink-only** doğru; mutlak device-only sonucu yanlış. Host kullanımında harici VBUS gerekir. |
| 18 | 1,8 V nedeniyle bütün 3,3/5 V giriş sinyalleri mutlaka çevrilmeli | KISMEN DOĞRU | [DS13543, Tablo 33 ve dipnotlar](https://www.st.com/resource/en/datasheet/stm32u5a9vj.pdf) | Çıkış seviyesi ile giriş toleransı farklıdır. FT ve TT pinler aynı sınıfta değildir. |
| 19 | `Config::default()` ile 160 MHz’e çıkılması belirsiz | KISMEN DOĞRU | [Embassy U5 RCC kaynağı](https://github.com/embassy-rs/embassy/blob/main/embassy-stm32/src/rcc/u5.rs) | Kaynaktan cevaplanabiliyor: varsayılan sistem saati **MSIS 4 MHz**. |
| 20 | 160 MHz çalışmak için dahili SMPS kullanılması şart | YANLIŞ | [DS13543, LDO/SMPS Run tabloları](https://www.st.com/resource/en/datasheet/stm32u5a5vj.pdf) | Range 1’de hem LDO hem SMPS için 160 MHz koşulları verilmiş. |
| 21 | 100–1000 Hz HIL için 115200–921600 baud genel olarak yetmez | YANLIŞ | Raporun kendi §2.2 hesabı; bağımsız aritmetik kontrol | Yeterlilik, kare boyutu ve frekansa bağlıdır. 921600 baud, 64 B × 1 kHz ham akışını taşıyabilir. |
| 22 | Bu kartta Embassy USB HS’nin iki işletim sisteminde kararlı çalıştığı | DOĞRULANAMADI | [PR #7439](https://github.com/embassy-rs/embassy/pull/7439), [USB örneği](https://github.com/embassy-rs/embassy/blob/main/examples/stm32u5/src/bin/usb_hs_serial.rs) | U5A9J-DK üzerinde macOS/Windows ölçümü sunulmuyor. Rapor bunu risk olarak bırakmakta haklı. |

## Yanlış ya da yanıltıcı bulunanlar

1. **Arayüz etiketleri toplam donanım kapasitesi gibi sunuluyor.**  
   Rapordaki iki UART, iki SPI ve yedi ADC, belirli işlev atamalarının sayılarıdır. Alternatif işlevlerle USART6, SPI1 ve ek ADC girişleri kullanılabilir. UART açısından USART1’i PG9/PG10’a taşımak, aynı USART1’in VCP kullanımını etkiler; USART6 PF0/PF1 ise I2C2 pinlerini paylaşır. Kaynak: [DS13543 pin tablosu](https://www.st.com.cn/resource/en/datasheet/stm32u5a5qi.pdf), [USART1 alternatif pinleri](https://www.st.com/resource/en/datasheet/stm32u5a9bj.pdf).

   ADC için ulaşılabilir fiziksel küme **PA0, PA3–PA7, PB0–PB2, PC4–PC5, PD12 ve PF14** şeklindedir: toplam 13. Bunların tamamı diğer işlevlerle eşzamanlı kullanılamaz. Kaynak: [DS13543](https://www.st.com/resource/en/datasheet/stm32u5a5qj.pdf), [PD12 eşleşmesi](https://www.st.com/resource/en/datasheet/stm32u5a9zj.pdf).

2. **I2C1 için gereksiz ADC fedakârlığı tarif ediliyor.**  
   Rapor yalnız CN9’daki seçilebilir bağlantıya odaklanıyor. I2C1’in PG14/PG13 hatları CN10-2/4’te ayrıca bulunuyor. Kaynak: [UM2967 Rev 2, Tablo 30](https://evo.net.ua/content/files/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics-12836994.pdf).

   Benzer biçimde, FDCAN için PD0/PD1 seçmek **I2C6 çevre biriminden bütünüyle vazgeçmeyi** gerektirmez: I2C6 PF0/PF1’e taşınabilir; bu kez I2C2 atamasıyla çakışır. Kaynak: [DS13543, PF0/PF1](https://www.st.com.cn/resource/en/datasheet/stm32u5a5qi.pdf).

3. **“Bütün harici sinyallerde seviye çevirici zorunlu” fazla kapsamlı.**  
   Kartın 1,8 V mantık çıkışı doğrudur. Ancak FT girişlerde, ilgili koşullar sağlandığında daha yüksek giriş gerilimi kabul edilebilir; TT girişlerde sınır `VDDIOx + 0,3 V`’tur. Dahili pull dirençleri ve pinin çalışma modu da önemlidir. Karşı cihazın 1,8 V’u yüksek seviye sayması ayrıca kontrol edilmelidir. Kaynak: [DS13543, Tablo 33, dipnot 4–5](https://www.st.com/resource/en/datasheet/stm32u5a9vj.pdf).

4. **V3E için yayımlanmış hız değeri olmadığı izlenimi eksik araştırmaya dayanıyor.**  
   ST’nin UM2525 belgesi, doğrudan **STLINK-V3E** başlığı altında VCP için **15 MHz** verir. Bu UART bit hızı bağlamında yaklaşık **15 Mbaud**’dur; 15 MB/s değildir. Buna karşılık V3SET’in UM2448 belgesindeki 16 Mbps değeri başka bir ürünün özelliğidir. U5A9J-DK üzerinde kayıpsız sürekli akış ve gecikme garantisi bu belgelerden çıkarılamaz. Kaynak: [UM2525 §6.3](https://www.st.com/resource/en/user_manual/dm00569152.pdf), [UM2448](https://www.st.com/resource/en/user_manual/um2448-stlinkv3set-debuggerprogrammer-for-stm8-and-stm32-stmicroelectronics.pdf).

5. **“USB sink-only” ile “USB device-only” eşitleniyor.**  
   Sink, güç rolüdür. ST’nin ortak DK dizinindeki host örneği, kartın VBUS süremediğini ve host işlevi için haricen 5 V sağlanması gerektiğini açıkça söylüyor. Örneğin test edilen kartı U5G9J-DK1 olarak belirtilmiş; dolayısıyla bu, U5A9J-DK üzerinde yapılmış deney kanıtı da değildir. HIL’de PC host, kart device düzeni uygundur. Kaynak: [ST USBX host README](https://github.com/STMicroelectronics/STM32CubeU5/blob/v1.9.0/Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/README.md).

6. **USB düzeltmesinin kapsamından kesin çalışmazlık sonucu çıkarılıyor.**  
   #7439, PHY etkinleştirilmeden önce `COMPDISTUNE=010` ve `SQRXTUNE=000` yazılmasını ekliyor; device ve host yollarını kapsıyor. Birleşme ve sürüm bilgileri doğru. Yazarın kendi dalını donanımda çalıştırmadığı açıklaması da doğru. Ancak 0.6.0’da HS sürücü kodu zaten mevcut; eksik PHY ayarı, bütün U5A9 uygulamalarının kesin çalışmayacağını kanıtlamaz. Doğru ifade: **0.6.0 gerekli düzeltmeyi içermiyor; kart üzerinde doğrulanmış kabul edilmemeli.** Kaynak: [PR #7439](https://github.com/embassy-rs/embassy/pull/7439), [0.6.0 kaynağı](https://github.com/embassy-rs/embassy/blob/embassy-stm32-v0.6.0/embassy-stm32/src/usb/otg.rs).

7. **Varsayılan saat ve SMPS riski yanlış çerçeveleniyor.**  
   `Config::default()` otomatik 160 MHz sağlamaz; kaynakta MSIS 4 MHz seçilir. Ayrıca dahili SMPS, 160 MHz’in zorunlu koşulu değildir. Saat, voltaj ölçeği ve regülatör seçimi ayrı konulardır. Kaynak: [Embassy RCC](https://github.com/embassy-rs/embassy/blob/main/embassy-stm32/src/rcc/u5.rs), [ST LDO/SMPS tabloları](https://www.st.com/resource/en/datasheet/stm32u5a5vj.pdf).

8. **Seri bağlantı yeterliliği fazla genelleniyor.**  
   8N1’de 921600 baud’un teorik yük kapasitesi **92.160 B/s**’dir. 64 B × 1 kHz = 64.000 B/s; 256 B × 1 kHz = 256.000 B/s. İlki çerçeveleme payıyla değerlendirilebilir; ikincisi sığmaz. Aynı şekilde 115200 baud, 64 B × 100 Hz akışına teorik olarak yeterlidir. Bunlar raporun §2.2 verilerinden yeniden hesaplandı; gecikme garantisi değildir.

## Rapor dışı ek bulgular

- **Silikon errata incelemesi eksik.** ES0553 Rev 6, U5A9NJ için X/W silikon revizyonlarını ayırıyor. Donanım kabulünde `REV_ID` kaydedilmeli ve kullanılan çevre birimlerinin ilgili sınırlamaları kontrol edilmeli. Kaynak: [ST ES0553](https://www.st.com/content/st_com/en/technical-documents/es0553-stm32u59xxx-and-stm32u5axxx-device-errata-stmicroelectronics.html).

- **ADC4 ile ADC1/2 eşzamanlı dönüşümde hata oluşabiliyor.** ST; aynı kaynak, frekans ve fazdaki ADC saatlerini, bölücüsüz çalışma koşulunu ve en fazla 55 MHz’i içeren bir çözüm tarif ediyor. Analog HIL genişletilirse yalnız giriş sayısını saymak yeterli olmayacak. Kaynak: [ES0553 ADC4](https://www.st.com/content/st_com/en/technical-documents/es0553-stm32u59xxx-and-stm32u5axxx-device-errata-stmicroelectronics_asset/chunk-004-topic-function-es-adc4.html).

- **FDCAN’de belirli Tx buffer/FIFO birleşimlerinde gönderim sırası değişebiliyor.** Ayrıca kenar filtresi açıkken belirli koşullarda alım senkronizasyonu sorunu var. CAN doğrulaması yalnız loopback başarısından oluşmamalı; kullanılan kuyruk ve filtre düzenini kapsamalı. Kaynak: [ES0553 FDCAN](https://www.st.com/content/st_com/en/technical-documents/es0553-stm32u59xxx-and-stm32u5axxx-device-errata-stmicroelectronics_asset/chunk-007-topic-function-es-fdcan.html).

- **PWM pini sayısı bağımsız zamanlama sayısı değildir.** Aynı timer’ın kanalları ortak zaman tabanını kullanır. PWM kapasitesi; kanal, timer, frekans ve diğer pin işlevleri birlikte atanarak belirlenmeli. Embassy’nin [PWM uygulaması](https://github.com/embassy-rs/embassy/tree/main/embassy-stm32/src/timer) bu kaynak paylaşımını yansıtır.

- **Windows sürücü konusunda belgeler gerçekten farklı konuşuyor.** Güncel ST notu, V3’ün Windows 10’dan itibaren kurulum olmadan tanınabildiğini söylüyor; probe-rs belgesi ST’nin resmi sürücüsünü istiyor. “Aygıtın tanınması” ile belirli aracın erişimi aynı kabul edilmemeli. Kaynak: [ST RN0093](https://www.st.com/resource/en/release_note/rn0093-firmware-upgrade-for-stlink-stlinkv2-stlinkv21-and-stlinkv3-boards-stmicroelectronics.pdf), [probe-rs kurulumu](https://probe.rs/docs/getting-started/probe-setup/).

## Raporlar arası çelişki değerlendirmesi

Ayrı bir raporlar arası karşılaştırma istenmedi. Bu belge, **01-kart-ve-arac-zinciri** raporunun iddialarına ilişkin hükümler içerir; diğer araştırma raporları için genel doğrulama hükmü vermez.

## Mimari için net sonuçlar

1. **Rust/Embassy ve probe-rs uygulanabilir adaylardır; kart üzerinde doğrulanmış çözüm değildir.** Dil kararı donanım kabul deneyine bağlı kalmalı.
2. USB HS CDC uygun ana HIL adayıdır. #7439’u içeren **sabit commit** kullanılmalı; macOS ve Windows’ta kayıp, gecikme ve jitter ölçülmeli.
3. V3E VCP, yayımlanmış 15 Mbaud değeri bulunan ciddi bir alternatif olarak değerlendirilmeli; DK üzerindeki gerçek performansı ölçülmeden elenmemeli.
4. Kartın GPIO çıkışları 1,8 V kabul edilmeli; giriş toleransı ve karşı cihazın eşikleri **pin ve yön bazında** belirlenmeli.
5. FDCAN için PD0/PD1 uygundur. I2C6 taşınabilir; bütün arayüzler ortak bir pin atama tablosunda çözülmeli.
6. UART/SPI/ADC/PWM planı, rapordaki etiket sayılarına göre sınırlandırılmamalı; alternatif işlevler ve timer paylaşımı hesaba katılmalı.
7. 160 MHz açıkça yapılandırılmalı. Varsayılan 4 MHz ile yüksek hızlı kabul testleri yapılmamalı; dahili SMPS zorunlu sayılmamalı.
8. Kabul testlerine silikon revizyonu, ilgili errata ve bağlantı kesilmesinde firmware davranışı eklenmeli. Ekranın ilk HIL kapsamı dışında tutulması makuldür.