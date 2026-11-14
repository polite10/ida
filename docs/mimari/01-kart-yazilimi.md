# 01 — Kart yazılımı

Sürüm: 0.2 (2026-11-14). Durum: taslak; ikinci inceleme bekliyor. Sürüm 0.2: iskelet 0.7'ye uyum, 02 (0.2) ve 06 (0.2) ile arayüz eşitlemesi, [inceleme/01-sol.md](inceleme/01-sol.md) bulgularının giderilmesi; madde madde yanıt [inceleme/01-yanit.md](inceleme/01-yanit.md). Başlıca değişiklikler: HIL yolunda tek dönüştürücü olarak 06'nın `protocol/ida_link` modülü; alım zamanının kesmede damgalanması; IMU'dan bağımsız 10 ms duvar çağrısı ızgarası; kimlikli olaylar ve flash'ta kalıcı açılış sayacı; iç sağlık ile dış veri kullanılabilirliğinin ayrılması ve görev ilerleme sayaçlarıyla gözetim; iki sınıflı gönderim ve ayrılmış kritik kapasite; oturum değişiminde temizlik; yeniden hesaplanan zaman bütçesi.

Bu bölüm [00-iskelet.md](00-iskelet.md) Bölüm 9, satır 01'in kapsamını yazar; iskeletle çeliştiği yerde iskelet geçerlidir. Dayanak gösterimi: `08.12` gibi numaralar [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) (bundan sonra "00") Bölüm 4'teki düzeltme satırıdır; `K`, `Ö`, `Ç`, `M`, `A` aynı belgenin karar, çelişki, ölçüm ve açık soru numaralarıdır; `R08 §2.3` ham rapor bölümü, `D08 #14`, `D08 E:Watchdog` doğrulama maddesidir; `02 §3.1`, `06 §2.5` bu dizindeki mimari bölümlerdir. **(tahmin)** işaretli her sayı ölçülmemiş başlangıç değeridir; ilgili ölçüm çıkmadan kesinleşmez. **(doğrulanmadı)** işaretli donanım ayrıntısı uygulama sırasında RM0456, DS13543 ya da kart şemasından doğrulanır.

## 1. Amaç ve sorumluluklar

Kart yazılımı STM32U5A9J-DK'yı başlatır, A1 bağlantısını yürütür, A4 portunun kart tarafını uygular ve ortak çekirdeği zamanında çağırır (iskelet §2). Kart simülasyonda olduğunu bilmez. Kapsam (iskelet §1, K8): kartın tek dış bağlantısı bilgisayarla olan USB bağlantısıdır, yedeği ST-LINK sanal seri portudur (VCP). Gerçek sensör, ardıl kart, ek donanım ve fiziksel çıkış yoktur. Bütün sensör verisi A1'den mesaj olarak gelir; bütün çıkışlar (itici komutu, atış ve silahlandırma olayları, durum) A1'e mesaj olarak gider. Sürücü sınırı gerçek sürücülerin sonradan ekleneceği yerde tutulur (3.3); bu bir tasarım hedefidir, "yalnız BSP değişir" güvencesi değildir (13.9).

Sorumluluklar: başlatma ve saat, bağlantı sürücüsü ve kesmede alım damgası, `ida_link` modülünün karta bağlanması, döngü zamanlaması, gönderim önceliği, kesme ve bellek düzeni, zaman kaynakları, gözetim zamanlayıcısı ve platform güvenli durumu, kalıcı açılış sayacı ve parametre deposu, tanılama, karttaki ekran, karta özgü derleme ve yükleme.

| Yazmadığı konu | Bölüm | Bu bölümün ondan beklediği |
|---|---|---|
| Kestirim, kontrol, kipler, arıza eylemleri, atış izni ve çıkış kapısı | 02 | 02 §3.1 çağrıları; `alive_ok`'un iç sağlık ve dış veri kullanılabilirliği olarak ayrılması; `plat`'a eklenen alanlar (8.2) |
| Atış kontrol mantığı, sonar algılaması | 03 | `ida_fc_background`'un ≤ 300 µs dilimlerle çağrılabilmesi; iş yuvası sahipliği (03 §2.9) |
| A1 mesajları, `ida_link` modülü, oturum, parti, bariyer, A1 ↔ A4 eşlemesi | 06 | 06 §3'teki modül arayüzü; bu bölüm onu bağlar, ayrı dönüştürücü yazmaz |
| Ölçüm işlerinin yürütülmesi, otomatik derleme | 09 | PC tarafı ölçüm aracı; iki işletim sisteminde derleme |

Temel ilkeler:
1. **Tek bağlam.** Çekirdek ve `ida_link` yalnız ana döngüden çağrılır; kesmeler yalnız bayt, zaman damgası ve bayrak üretir.
2. **Hiçbir çağrı bloklamaz.** Başlatma bittikten sonra bekleme döngüsü, `HAL_Delay`, bloklayan flash işlemi ya da gönderim beklemesi yoktur (08.12).
3. **Bellek ayırma yoktur** (2.10).
4. **Karta özgü kod BSP ve bağlantı sürücüsünde toplanır.**
5. **Sağlık kanıtı ilerleyen yazılımdan gelir;** dış verinin yokluğu yazılım arızası sayılmaz (07.11; 2.8).
6. **Güvenliği artıran çıktı hiçbir kuyrukta değiştirilmez ya da atılmaz** (06 §2.4).

## 2. İç yapı

### 2.1 Katmanlar

| Katman | Yer (iskelet §7) | Sorumluluk |
|---|---|---|
| Kart destek katmanı (BSP) | `firmware/bsp/` | Başlangıç kodu, bağlayıcı betiği, güç ve saat, NVIC, MPU, IWDG, duvar saati zamanlayıcısı, DWT, flash sürücüsü, sıfırlama nedeni, hata işleyicileri, LED. Kart değişiminde yeniden yazılan yer |
| Bağlantı sürücüsü | `firmware/link/` | `ida_link_ops_t` (3.2): USB HS CDC (TinyUSB) ve VCP (USART1 + DMA); kesmede alım damgası (2.3) |
| Protokol modülü | `protocol/ida_link/` (06 §3) | Ayrıştırma, oturum, parti kapanışı, A1 ↔ A4 eşlemesi, olay tablosu. Kart ve SIL koşucusu aynı modülü bağlar; HIL yolundaki tek dönüştürücüdür |
| Gönderim yöneticisi | `firmware/app/tx` | İki sınıflı kuyruk, çerçeve bütünlüğü, ayrılmış kritik kapasite (2.6) |
| Çağırıcı | `firmware/app/runner` | `ida_core_t`'nin statik sahibi; `step`/`tick` çağrıları, süre ölçümü, `plat`, platform kapısı (2.8), 03 arka plan dilimi |
| Sağlık | `firmware/app/health` | İlerleme sayaçları, IWDG besleme kararı, platform durumları (2.8) |
| Kalıcı bellek | `firmware/app/nvm` | Açılış sayacı, parametre A/B blokları (2.9) |
| Tanılama, ekran | `firmware/app/diag`, `firmware/app/display` | Sayaçlar ve kayıtlar (2.11); durum göstergesi (2.12) |

Sürüm 0.1'deki `ida_conv_*` dönüştürücüsü ve `ida_acc_*` biriktiricisi kaldırıldı: HIL yolunda ikinci bir dönüştürücü olurdu (06 §3, §8).

```
 Kesmeler                              Ana döngü (tek bağlam)
 OTG HS ─ damga ┐                     ┌ A: kritik geçiş (kritik iş kalmayana dek yinelenir) ────────────────┐
 USART1 CM/IDLE ┼ bayt + damga ──────▶│ servis ▶ ida_link_rx ▶ ida_link_take ▶ step | tick ▶ put_out ▶ tx   │
 TIM 1 MHz ─ taşma, 10 ms ızgara ────▶│ ızgara tick'i ▶ (güvenli çıktıysa) put_out(async) ▶ tx              │
                                      │ poll_tx (olay yinelemesi, olağan çıktı, bütçeli) ▶ sağlık ▶ IWDG    │
                                      └ B: tek olağan dilim (03 arka plan | ekran | tanılama | parametre) ──┘
```

### 2.2 Başlatma ve saat

Her adımın sonucu açılış kaydına yazılır (3.4). Sıra:

1. **Sıfırlama işleyicisi:** `MSP` ve yığın sınırı `MSPLIM` (2.10), `.data`/`.bss`, FPU. FPSCR ve varsayılan bağlam değeri sabitlenir: en yakına yuvarlama, `FZ = 0`, `DN = 0`; geri okunup kaydedilir. Aynı politika SIL'de de seçilir; bit eşitliği sağlamaz (02 §2.7, 08.14, Ç7).
2. **`HAL_Init`:** NVIC öncelik gruplaması (dört bitin hepsi önalım, alt öncelik yok; 2.4); SysTick 1 ms, varsayılan MSIS 4 MHz üzerinde. HAL'in güç ve saat beklemelerinin zaman aşımı kaynağı budur; başlatmadaki her bekleme zaman aşımlıdır (ST'nin DK örneği `HAL_Init`'i ilk çağırır; 01-sol md.17).
3. **Sıfırlama nedeni:** RCC bayrakları ham değeriyle saklanır (06'nın `reset_flags`'i) ve temizlenir; `.noinit`'teki hata kaydı ve ardışık arıza sayacı CRC denetimiyle okunur (2.8). BOR bayrağının ilk güç verişte de kalkıp kalkmadığı **(doğrulanmadı)**.
4. **IWDG başlatılır:** bölücü 16, reload 3999 → nominal 2,0 s (32 kHz LSI'de `4000 × 16 / 32.000`). Güç ve PLL beklemelerinden önce başlar; bu adımlarda bir kilitlenme de sıfırlamayla biter (01-sol md.17). Hata ayıklama derlemesinde çekirdek durunca IWDG'yi donduran DBGMCU biti açılır (R08 §2.1); sürüm derlemesinde açılmaz.
5. **Salt okunur denetimler:** `DEV_ID`/`REV_ID` (M6), `TZEN`, `BOR_LEV`, SRAM silme seçenek baytları. Firmware seçenek baytı yazmaz. `TZEN = 1` görülürse platform güvenli beklemesi (2.8). Bu, hazırlıksız bir imajın `TZEN = 1` altında bu denetime ulaşacağının güvencesi değildir (08.3); asıl koruma ilk yüklemeden önceki salt okunur denetimdir (K0, 7. başlık).
6. **Güç:** PWR saati açılır, regülatör seçilir (birincil SMPS, yedek LDO; ikisi de 160 MHz/Range 1 için belgeli: 01.1, 08.9, Ç4), gerilim ölçeği Range 1, `VOSRDY` zaman aşımlı beklenir; EPOD booster ST örneğindeki diziyle **(doğrulanmadı)**.
7. **Flash bekleme durumu** saat yükseltilmeden önce 160 MHz/Range 1 değerine kurulur (ST örneği; RM0456 ile doğrulanacak).
8. **HSE 16 MHz ve PLL1.** Her varyant tam bir yapılandırma kaydıdır; yalnız `M/N/R` değişmez (01-sol md.17):

| Alan | Birincil (ST'nin DK örneği) | Yedek (Nucleo-U5A5 ve TinyUSB U5A5 BSP'si) |
|---|---|---|
| PLL girişi `HSE / M` | 16 / 4 = 4 MHz | 16 / 1 = 16 MHz |
| `N`, `R`; VCO | 80, 2; 320 MHz | 20, 2; 320 MHz |
| Giriş aralığı `PLLRGE` | 4 MHz'i kapsayan aralık | 16 MHz'i kapsayan aralık |
| `PLLMBOOST`, `FRACN` | ST örneğindeki değerler | kaynak BSP'deki değerler |
| SYSCLK | 320 / 2 = 160 MHz | 160 MHz |

   Aralık kodları ve booster bölücüsü RM0456'dan **(doğrulanmadı)**. Yapılandırmadan sonra `SYSCLK` geri okunur; HSE açılmaz ya da PLL kilitlenmezse MSIS'e sessizce dönülmez: "saat hatası" ile platform güvenli beklemesi (USB HSE'siz zaten çalışmaz). Varsayılan 4 MHz'te hiçbir hız testi yapılmaz (01.8).
9. **SysTick** 160 MHz'e göre yeniden kurulur; USB PHY saati HSE, 16 MHz referans seçimi (R08 §2.2).
10. **ICACHE ve MPU:** önbellek açık (M5 soğuk önbellek senaryosunu da ölçer); MPU ile sıfır adresi erişimsiz, yığın altı koruma bölgesi, RAM ve çerçeve tamponu yürütülemez.
11. **Zaman kaynakları:** duvar saati zamanlayıcısı, 10 ms ızgara, DWT (2.5).
12. **Kalıcı açılış sayacı** (2.9). Yeni değer flash'a yazılıp doğrulanana kadar olay üretimi kapalıdır.
13. **Parametreler** (2.9), sonra `ida_core_init(c, p, boot_count, now_w)`; `sizeof(ida_core_t)` ile çekirdeğin bildirdiği boyut karşılaştırılır, uyuşmazlıkta platform güvenli beklemesi.
14. **Bağlantı:** seçili yol başlatılır; USB başlatma hatası (ör. `USB_CoreReset` zaman aşımı; R08 §2.2) en çok üç kez yeniden denenir (tahmin). Toplam süre 2 s'lik IWDG süresine sığacak biçimde sınırlıdır.
15. **Ekran** (derlemede açıksa): panel başlatma durum makinesi kurulur; ana döngüde adım adım ilerler, başlatmayı bekletmez.
16. **Çalışma kipine geçiş:** IWDG bölücü 4, reload 799 → nominal 100 ms (2.8); başlatma bitti bayrağı kalkar, bundan sonra `HAL_Delay` tuzağa düşer.

### 2.3 Bağlantı sürücüsü

**Birincil: USB 2.0 HS CDC** (CN4, gömülü HS PHY), **TinyUSB 0.21.0, `dwc2` sürücüsü** (Ö1, iskelet §8). TinyUSB MIT lisanslıdır, işletim sistemi gerektirmez; 0.21.0 kaynağı U59/U5A gömülü HS PHY ayrımını içerir (D08 #13). Kart üzerinde hiçbir yığınla doğrulanmış HS CDC yoktur (Ç8); TinyUSB güçlü adaydır, kanıtlanmış çözüm değildir (D08 N2). Yazılacaklar:
- **Kart dosyası:** TinyUSB'de U5A9J-DK BSP'si yoktur (D08 #6). Önce ST'nin Nucleo-U5A5ZJ USBX örneğinin PHY dizisi bu kartta "bilinen iyi başlatma" olarak çalıştırılır; kart dosyası o diziyi taklit eder. U5A5 BSP'sindeki ADC tabanlı VBUS algılama ve Type-C davranışı DK şemasıyla eşleştirilmeden kopyalanmaz (08.15).
- **PHY ayarı:** `COMPDISTUNE`/`SQRXTUNE` zorunluluğu kanıtlanmamıştır; ST örneği yalnız referans saat seçimini çağırır (08.2). Ayarlı ve ayarsız iki varyant M2'de sınanır.
- **VBUS algılama:** kapalı; oturum geçerliliği yazılımla zorlanır (`BVALOEN/BVALOVAL`; R08 §2.2). Kopma, askıya alma/sıfırlama olayları ve 06'nın eş canlılığıyla anlaşılır. Yedek PG1/UCPD/TCPP01, şemayla eşleştirildikten sonra (M2).
- **Uç nokta ve tamponlar:** bulk azami paket 512 B (yanlış değer Windows'ta "Code 43" verir; R01 §2.2). TinyUSB CDC alım FIFO'su 8 KB, gönderim FIFO'su 4 KB (tahmin; 06 §2.6'nın 4.096 B'lık toplu penceresi ve kritik pencere için). Alım FIFO'su dolarsa cihaz OUT paketlerini NAK'lar: veri kaybolmaz, gecikir; gecikme son tarih kaçırması olarak görünür.
- **Servis:** `tud_task_ext(0, false)` açıkça çağrılır. 0.21.0'da `tud_task()` `tud_task_ext(UINT32_MAX, false)`'tır; RTOS'suz OSAL'de beklemese de RTOS yedeğinde süresiz bekler (01-sol md.10). Geri çağrılar (`tud_cdc_rx_cb`, satır durumu, askıya alma) yalnız bayt taşır ve bayrak kurar; ayrıştırma ana döngüdedir. Bir servis çağrısı TinyUSB olay kuyruğunu boşaltır; olay sayısı kuyruk boyu ve pencere başına ≈ 15 USB kesmesiyle sınırlıdır (2.4). Süresi M5'te ölçülür.

**Alım damgası (USB).** OTG HS kesme vektörü ince bir sarmalayıcıya bağlanır: sarmalayıcı duvar saatini okur (2.5), CDC veri OUT uç noktasının aktarım tamamlandı bayrağı (`dwc2` `DOEPINTx.XFRC`) kalkmışsa damgayı 16 yuvalı damga halkasına yazar, sonra TinyUSB'nin `tud_int_handler`'ını çağırır. Bayrağın okunması onu temizlemez **(doğrulanmadı)**. `tud_cdc_rx_cb` her tamamlanan aktarım için sırayla çağrıldığından, geri çağrı FIFO'daki baytları tek parça olarak halkanın sıradaki damgasıyla alır. Çerçevenin alım zamanı son baytını taşıyan aktarımın damgasıdır (06 §2.8). Hata payı: tam 512 B'lık paketin ortasında biten çerçevede paketin kalanı kadar, HS'te en çok ≈ 8,5 µs (512 × 8 bit / 480 Mbit/s; hesap, protokol ek yükü hariç). Halka taşarsa ya da aktarım ve geri çağrı sayıları tutmazsa `rx_stamp_err` sayılır ve damga geri çağrı anının saati olur (kaba damga, tanılamada işaretli).

**Yedek: ST-LINK V3E VCP.** USART1 PA9/PA10 (D01 #6), DMA dairesel alım (4 KB halka; 3 Mbaud'da ≈ 13,7 ms veri, hesap), DMA ile gönderim. ST'nin V3E için yayımladığı 15 MHz, UART bağlamında yaklaşık 15 Mbaud'dur ve DK üzerinde kayıpsız akış ya da gecikme garantisi değildir (01.3, M3). **Alım damgası:** karakter eşleşmesi (MAVLink 2 başlangıç baytı `0xFD`), boşta hat ve DMA yarım/tam kesmelerinin her birinde ISR (DMA yazma indisi, duvar saati) çiftini halkaya koyar; çerçevenin son baytından sonraki ilk olayın damgası alım zamanıdır. Arka arkaya gelen çerçevelerde sonraki çerçevenin başlangıç baytı, son çerçevede boşta hat olay üretir; ikisi de bir karakter süresi içindedir (3 Mbaud'da 3,3 µs; hesap). Yükteki `0xFD` baytları fazladan zararsız olay üretir. USART1'in karakter eşleşme özelliği **(doğrulanmadı)**. Hız: 06 §2.6'nın yeni mesaj kümesiyle aşağı yön 400 Hz'te 50 m sonarla 90.864 B/s'dir, 921.600 baud kapasitesinin %99'u; bu yüzden VCP yedeği 200 Hz ve `L_act = 3h` ile, ≥ 3 Mbaud gelir (2 Mbaud yalnız kritik en kötü ≤ 320 B ise; 06 §2.6, §6). Baud M3 ile seçilir.

**İki yol arasında geçiş:** iki gerçekleme aynı `ida_link_ops_t`'yi uygular; üst katmanlar etkin yolu bilmez. Seçim kalıcı parametre `LINK_SEL` ile; açılışta B1 (PC13) basılıysa yedek yol zorlanır. Çalışırken kendiliğinden yol değiştirilmez. USB kipinde VCP yalnız metin günlüğü taşır. Yer istasyonu trafiği de aynı A1 kanalından gelir (06 §2.1).

### 2.4 Döngü zamanlaması

**Yapı: RTOS'suz, tek ana döngü** (Ö1, iskelet §8). Adımı, IMU örneğini içeren partinin kapanması tetikler: 06'da zaman çerçevesi (TF) partinin sonlandırıcısıdır, IMU partinin üyesidir (06 §2.3); iskelet §4'ün "döngüyü yeni IMU örneği tetikler" kuralı kartta böyle uygulanır. IMU'suz kapanan parti hemen `ida_core_tick`'e gider (06 §2.3). Kontrol adımı ana döngüde koşar: TinyUSB'nin veriyi uygulama bağlamında işlemesi (D08 #14, #15), çekirdeğin tek bağlamda kilitsiz kalması (D08 Y8), `ida_fc_step` ile `ida_fc_background` arasında yarış olmaması ve ISR'de ağır hesap ile HAL beklemesi riskinin doğmaması (08.12) gerekçedir.

**A geçişi (kritik).** Her B diliminden sonra koşar ve bekleyen kritik iş kalmayana dek yinelenir:
1. Bağlantı servisi (2.3).
2. `ida_link_rx` ile en çok `B_ayr` = 1 KB bayt ayrıştırılır (tahmin); parti kapanırsa ayrıştırma durur.
3. `ida_link_take` sonucuna göre: `STEP` → `ida_core_step`; `TICK` → `ida_core_tick`; `DUP` → `ida_core_step` (`DUP_SEQ` döner, çekirdek sürekli komutu güncel güvenlik durumuyla yeniden üretir; 02 §3.4). Her çıktı `ida_link_put_out` ile kritik sınıfa verilir. Kapanmış birden çok parti varsa sırayla, hiçbiri atlanmadan işlenir (06 §2.3).
4. 10 ms ızgarası dolduysa örneksiz ve komutsuz `ida_core_tick`; çıktı güvenliyse `put_out(async = true)` (2.5).
5. Gönderim pompası (2.6) ve `ida_link_poll_tx`.
6. Yeni bayt, kapanmış parti ya da dolmuş ızgara varsa 1'e dönülür; yoksa sağlık değerlendirmesi ve IWDG besleme kararı (2.8).

**B dilimi.** A'dan sonra tek bir olağan dilim, sabit dönüşümle: 03 arka plan, ekran, 03 arka plan, tanılama ve telemetri kodlama, 03 arka plan, parametre kaydı adımı. Arka plan her iki dilimden birini alır; alım tamponunda veri var diye atlanmaz (sürüm 0.1'deki atlama arka planı süresiz erteleyebiliyordu; 01-sol md.14). Dilim sınırları: arka plan 300 µs (03 §2.3), ekran 200 µs, tanılama 100 µs, parametre 10 µs (hepsi tahmin). Dilim sonunda A'ya dönülür; böylece kritik iş en çok bir dilim bekler (06 §2.5'in beklediği dilimler arası denetim).

**Kesme öncelikleri.** MCU dört öncelik biti kullanır (`__NVIC_PRIO_BITS = 4`, ST cihaz başlığı; 01-sol md.19); sayısal 0 en acildir.

| Öncelik | Kaynak | İşi |
|---|---|---|
| Sabit (negatif) | Reset, NMI, HardFault | NMI: flash ECC çift hatası (2.9); HardFault: hata kaydı (2.8) |
| 0 | MemManage, BusFault, UsageFault (SHCSR'de ayrı ayrı açılır; açılmazsa HardFault'a yükselir) | Hata kaydı ve sıfırlama (2.8) |
| 1 | Duvar saati zamanlayıcısı (taşma + 10 ms karşılaştırma) | Üst yarı artışı, ızgara sayacı |
| 2 | OTG HS (sarmalayıcı + TinyUSB) | Alım damgası; TinyUSB olayları |
| 3 | USART1 ve DMA (yalnız VCP kipinde) | Damga ve indis |
| 4 | Flash (işlem sonu ve hata) | Bayrak |
| 5 | DSI, LTDC, DMA2D (ekran açıksa) | Aktarım bitti bayrakları |
| 15 | SysTick | HAL zaman tabanı |

**Maskeleme.** `BASEPRI = n`, önceliği sayısal olarak n ve üstü olan bütün kesmeleri maskeler; tek bir kesmeyi değil (01-sol md.19). Ana döngü damga halkalarını ve indisleri `BASEPRI = 2` altında okur: OTG HS ve altı maskelenir, duvar saati kesmesi maskelenmez. Duvar saatinin 64 bit okuması `PRIMASK` altında birkaç komutluk kritik bölümdür (2.5). Kritik bölüm üst sınırı 2 µs (tahmin); kesme gecikmesine eklenir. `HAL_Delay` başlatmadan sonra tuzağa düşecek biçimde yeniden tanımlanır (zayıf sembol; 08.12).

**Bloklayan çağrı yasağı:** başlatmadan sonra bekleme döngüsü, `HAL_Delay`, sınırsız bayrak yoklaması, bloklayan flash işlemi, gönderim boşalmasını bekleme ve `printf` ailesi yoktur. Taşımaya yazılamayan veri beklenmez; 2.6'daki sınıf kurallarıyla tutulur ya da atılır.

**Zaman bütçesi (hepsi tahmin; M5 ölçer).** 06 §2.5'in `S_kart = e₃ − e₂` aralığı: `TF(n)`'nin son baytının kesme damgasından `ACTUATOR(n)`'in bağlantı sürücüsüne verilmesine kadar.

| Kalem | µs | Dayanak |
|---|---:|---|
| Damga anında başlamış en uzun B dilimi | 300 | 03 arka plan dilimi (03 §2.3); diğer dilimler daha kısa |
| Servis ve kritik pencerenin ayrıştırılması (olağan en kötü 261 B, tam en kötü 530 B; 06 §2.6) | 150 | sürüm 0.1'in değeri |
| Parti kapanışı, adım girdisinin kurulması | 20 | |
| `ida_core_step` (03'ün hızlı kısmı dahil) | 1000 | 02 §2.2 hedefi; 02'nin en kötü kestirimi 0,32–0,63 ms |
| Çıktının kodlanması (ACTUATOR, yeni olaylar), `us_from_thr`, kritik kuyruğa yazım | 100 | |
| Bu aralıktaki kesmelerin toplam süresi (pencere başına üst sınır) | 125 | aşağıda |
| **`S_kart` en kötü** | **1695** | |

Kesme üst sınırı: 2,5 ms'lik pencerede USB'den en çok 4.096 + 530 = 4.626 B gelir (06 §2.6 toplu bütçe + tam kritik pencere) → ⌈4.626 / 512⌉ = 10 OUT paketi; buna ≈ 5 IN tamamlanması, bir ızgara kesmesi ve birkaç ekran kesmesi eklenir. Kesme başına ≤ 6 µs ile ≈ 100 µs; 125 µs alındı. Bu bir üst sınır varsayımıdır, ortalama yük değildir; M5 pencere başına en büyük kesme süresini ölçer.

**Sonuç:** `S_kart` ≤ 1,70 ms (tahmin). 06 §2.5'in bütçesi (`R_maks` = 2,10 ms, `L_act = h`) taşımaya 0,40 ms bırakır; 06'nın tablosuyla aynıdır. Sonuç A geçişinin her dilimden sonra koşmasına dayanır: denetim olmasaydı dilimler arka arkaya koşar ve bekleme 300 + 200 + 100 + 10 = 610 µs'ye çıkardı (01-sol md.9'un 600 µs'sine parametre dilimi eklendi). A'nın kritik olmayan kısımları (ızgara tick'i ≤ 50, `poll_tx` ≤ 30, sağlık ≤ 20 µs; tahmin) en uzun dilimden kısadır ve A yinelendiği için bunlar sırasında gelen TF bir B dilimini beklemez. 200 Hz'te `S_kart` aynıdır; VCP yedeğinde `L_act = 3h` ile taşımaya 2,90 ms kalır (06 §2.5).

**Sürekli durum.** Periyot başına en kötü kritik iş 150 + 20 + 1000 + 100 + 125 + 50 = 1.445 µs < 2.500 µs; olağan dilimlere periyot başına ≥ 1,05 ms, yani en az üç dilim kalır; arka plan bunların en az birini alır. 03'ün tüketim süresi `K_bg` = 8 adım (20 ms; 03 §2.9) içinde arka plan ≥ 8 × 300 µs = 2,4 ms alır; 03'ün profil başına tahmini 0,1–0,3 ms'dir (03 §2.3). Bir döngü turunun en uzunu A + B ≈ 1,75 ms'dir (hesap, tahmini girdilerle). Ortalama yük en kötü durum yanıtını kanıtlamaz; karar DWT ölçümüyle verilir (08.12, M5).

**Taşma ve bayatlık politikası** (tek politika; 01-sol md.13):
- **Bayt düzeyi:** USB'de FIFO dolunca NAK ile akış denetimi, kayıp yok. VCP'de halka taşarsa eski baytların üzerine yazılır; CRC hataları ve `vcp_overrun` sayılır, parti eksik kapanır.
- **Parti düzeyi** (06 §2.3): aşamalı ve gelecek parti yuvaları 1 + `K_ileri` = 5; aralık dışı `step_seq` atılır ve sayılır. Parti başına kapasite 16 örnek, 8 komut, 1 sonar dilimi (02 §3.8); sim bunları aşmaz. Aşılırsa **yeni gelen üye** atılır, sayılır, parti `PARTİ_EKSİK` kapanır (06 §4 ile aynı; sürüm 0.1'deki "en eskiyi at" kaldırıldı). İstisna komutlardır: izni daraltan komutlar (`DISARM`, izin dışı kipe geçiş, `FC_ABORT`) önce yerleşir, kalanlar `ida_link`'teki 8 yuvalı erteleme tamponuyla sonraki çağrıya kalır (kapasite kartın yapılandırmasıdır; 06 §2.4). Erteleme tamponu da doluysa izni daraltmayan komut atılır; izni daraltan komut hiçbir doluluk koşulunda atılmaz, gerekirse izni daraltmayan bir komutun yerini alır.
- **Gecikme:** kapanmış birden çok parti varsa hepsi sırayla koşar; `overrun` sayılır ve sonraki ACTUATOR `ÖNCEKİ_SÜRE_AŞIMI` taşır. Adım süresi 1,0 ms'yi aşarsa `deadline_miss` ve süresi sonraki çağrının `plat`'ına girer. `DUP`, `OLD`, `GAP` 06 §2.3'teki gibidir.
- **Bayatlık:** kart tazelik kararı vermez. Her üyeye kesme damgası `t_rx` koyar ve çekirdek iki ölçütle karar verir (02 §4.1). Oturum dışı, yinelenen ya da çelişkili çerçeve `ida_link`'te atılır ve çekirdeğe ulaşmaz (06 §2.8). Sürüm 0.1'deki kart tarafı "son IMU varışı" sayacı (`T_gap`) kaldırıldı; yinelenen ya da eski veriyle yenilenebilecek bir kart tarafı canlılık ölçütü kalmadı (01-sol md.3).

**Yedekler.** (a) Çekirdek adımının en kötü süresi bütçeyi aşarsa kontrol hızı 200 Hz'e iner (iskelet §8). (b) Adım bütçeye sığdığı halde `S_kart` dilimlenemeyen bir iş yüzünden aşılıyorsa **FreeRTOS** (MIT, `ARM_CM33_NTZ`; R08 §2.3):
- Yüksek öncelikli görev A geçişinin tamamını yürütür (servis, ayrıştırma, `step`/`tick`, gönderim, sağlık). Tek bekleme noktası görev bildirimidir: USB, UART ve ızgara kesmeleri bildirir. USB servisi `tud_task_ext(0, false)` ile hiç beklemez; ızgara tick'i USB bekleyişine takılamaz (01-sol md.10).
- Düşük öncelikli görev B dilimlerini yürütür. 03'ün iş yuvası sahiplik devrinde platform bellek bariyeri (`__DMB`) sağlar (03 §2.9).
- Görevler, kuyruklar ve semaforlar statik oluşturulur: `configSUPPORT_DYNAMIC_ALLOCATION = 0`, hiçbir `heap_N.c` bağlanmaz, `pvPortMalloc` sembolü bulunmaz; TinyUSB'nin FreeRTOS OSAL'ı statik oluşturmayla derlenir **(doğrulanmadı)** (01-sol md.21).

### 2.5 Zaman kaynakları

- **Duvar saati (`ida_wtime_t`):** HSE'den türeyen 32 bit bir zamanlayıcı 1 MHz sayar (hangi TIM'in 32 bit olduğu **doğrulanmadı**); alt sayaç 2³² µs ≈ 71,6 dk'da taşar (hesap). Taşma kesmesi (öncelik 1) üst yarıyı artırır ve `UIF`'i temizler. **Okuma** (01-sol md.18): `PRIMASK` altında üst yarı `h` ve sayaç `l` okunur, `UIF` en son okunur; `UIF` kalkmışsa ve `l < 2³¹` ise `h + 1` kullanılır. Sayaç `UIF`'ten önce okunduğu için taşma sayaç okumasından sonra olduysa `l` büyük kalır ve düzeltme uygulanmaz; önce olduysa `l` küçüktür ve henüz işlenmemiş taşma eklenir. `PRIMASK` altında taşma kesmesi araya giremez; kesme bağlamındaki okuyucular (OTG HS, USART1) taşma kesmesinden daha az acildir ve onu bölemez. Hata işleyicileri duvar saatini okumaz (adım sayacı ve ham sayaç değerini kaydeder). Değer ns'ye çevrilir (`× 1000`); `int64` ns ≈ 292 yılda taşar (hesap). Duvar saati PC saatiyle eşlenmez (06 `TIMESYNC` ofseti PC tarafında kestirir).
- **10 ms ızgara:** aynı zamanlayıcının karşılaştırma kanalı; ISR `CCR += 10.000` (mod 2³²) yapar ve ızgara sayacını artırır. Ana döngü sayaç ilerlediyse bir `ida_core_tick` çalıştırır; kaçırılan ızgara anları birikmez, sayılır. Izgara mutlak zamandadır (açılıştan beri 10 ms'nin katları), son IMU varışına göre değildir; süre periyot sayısıyla değil süreyle verilir, 400 ve 200 Hz'te aynıdır (02 §2.2; 01-sol md.5). Bu, 02'nin beklediği ≥ 100 Hz duvar çağrısıdır.
- **Güvenli çıktıya geçiş sınırı** (01-sol md.5): çekirdek IMU'yu son kabul edilen örneğin `t_rx`'inden 20 ms sonra bayat sayar (02 §4.1). Sim TF göndermeyi sürdürüyorsa IMU'suz partiler her 2,5 ms'de bir `tick` üretir: son IMU alımından güvenli ACTUATOR'ın gönderim kuyruğuna verilmesine kadar ≤ 20 + 2,5 + 1,7 ≈ 24,2 ms (hesap; 1,7 ms `S_kart` sınırı). Hiç veri gelmiyorsa ızgara tick'iyle ≤ 20 + 10 + 0,35 ≈ 30,4 ms (0,35 ms: en uzun dilim + tick; tahmin). İkinci durumda çıktı sim'e ulaşamayabilir; sim'in kendi kuralı (kullanım süresi 50 ms, eş canlılığı 500 ms) zaten nötrler (06 §2.4, §2.9).
- **DWT çevrim sayacı:** 160 MHz'te 6,25 ns; 32 bit sayaç 2³² / 160·10⁶ ≈ 26,8 s'de taşar (hesap). Yalnız süre farkı içindir; bir saatlik ölçümde taşmalar biriktirilir. Çekirdek hata ayıklayıcıda durunca durur; duvar saatinin yerine geçmez.
- **Ölçüm zamanı (`ida_stime_t`):** kart üretmez; A1'deki `t_meas_ns` aynen aktarılır (06 §3 eşlemesi). **Alım zamanı** kesme damgasıdır (2.3) ve her parti üyesinin `ida_hdr_t.t_rx`'ine konur. İki alan hiçbir yerde birbirinden çıkarılmaz (02 §3.1).

### 2.6 Gönderim: çerçeve bütünlüğü ve kritik kapasite

- **Sınıflar.** *Kritik:* `IDA_ACTUATOR` (adım yanıtı ve eş zamansız güvenli çıktı) ve `IDA_EVENT` (ilk gönderim ve yinelemeleri). *Olağan:* `NAV_STATE`, `BOARD_STATUS`, `DIAG`, `FC_STATUS`, `DETECTION`, `BOOT_RECORD`, `FAULT_RECORD`, `HELLO`, `KEEPALIVE`, `TIMESYNC` yanıtı, GCS telemetrisi (06 §2.2).
- **Çerçeve bütünlüğü.** Kuyruğa yalnız tam çerçeve girer. Taşımanın `write`'ı kısmi kabul edebilir; yöneticinin "yazımdaki çerçeve" imleci vardır ve sıradaki her bayttan önce o çerçevenin kalanı yazılır. Bayt akışında iki çerçeve iç içe geçmez; öncelik yalnız çerçeve sınırında uygulanır (06 §2.5; 01-sol md.11).
- **Ayrılmış kapasite.** Taşımanın gönderim FIFO'sunda (USB'de TinyUSB CDC FIFO'su, VCP'de DMA gönderim tamponu) `R_krit` = 320 B her zaman kritik sınıfa ayrılır: 2 ACTUATOR × 59 B + 4 olay × 45 B = 298 B (06 §2.2 çerçeve boyları). Olağan çerçeve ancak `write_space() − R_krit` ≥ çerçeve boyu ise taşımaya verilir.
- **Olağan bütçe.** Bir periyotta taşımaya verilen olağan bayt `B_olağan` ile sınırlıdır; önde bekleyen olağan veri bir sonraki ACTUATOR'dan önce boşalır (06 §2.5). USB'de 2.048 B/periyot (tahmin; host'un bulk IN zamanlamasına bağlı, M1); VCP'de `hız × (P − 0,5 ms)`: 3 Mbaud ve 200 Hz'te 300.000 B/s × 4,5 ms = 1.350 B. Yukarı yön talebi 06 §2.6'ya göre 400 Hz'te 59.917 / 400 ≈ 150 B/periyot, 200 Hz'te 48.117 / 200 ≈ 241 B/periyot (ACTUATOR dahil); iki bütçe de yeterlidir. Bir ACTUATOR'ın önünde en çok `B_olağan` + yarım kalmış en büyük çerçeve (267 B) bekleyebilir.
- **Kritik kuyruk.** 2 ACTUATOR yuvası; olay çerçevelerini `ida_link`'in olay tablosu (4 yuva) verir. Gönderilmemiş daha eski olağan ACTUATOR yenisi gelince değiştirilir (sürekli komutta yalnız sonuncusu anlamlıdır; değiştirme sayılır, sim o adımı kaçırma sayar). Güvenli çıktı kuyruktaki daha eski olağan çıktıyı siler (sim onu zaten eski nesil diye reddeder; 06 §2.4); kendisi hiçbir zaman değiştirilmez ya da atılmaz. Yazımı başlamış çerçeve tamamlanır.
- **Taşıma boşalmazsa** (host okumuyor, kablo çıkık, PC uygulaması donmuş): kritik kuyruk dolu kalır, olaylar son geçerliliğe kadar tabloda bekler ve sonra düşer (06 §2.4), olağan çerçeveler atılır ve sayılır (`tx_drop`, `tx_crit_stall`). Bu dış nedendir; sağlık koşuluna girmez, sıfırlama üretmez (2.8).

### 2.7 Oturum değişimi ve temizlik

Kart iki durumda geçici durumu temizler (01-sol md.12): (a) **oturum sonu** (06 §2.9: `KOPUK`, oturum sırasında sim'in yeni `HELLO`'su, uyumsuzluk), (b) **platform güvenli durumuna giriş** (2.8). Temizlenenler 06 §2.9'un listesiyle aynıdır: alınmış ama ayrıştırılmamış baytlar ve damga halkaları (`reset_rx`), ayrıştırıcı durumu, aşamalı ve gelecek parti yuvaları, Ping360 yuvaları, komut kuyruğu ve komut erteleme tamponu (eski bir arm komutu yeni oturumda uygulanamaz), gönderim kuyrukları, onaylanmamış olaylar (silinir, sayılır, yeniden gönderilmez), son çıktı bilgisi ve güvenlik nesli. Taşımanın FIFO'sunda yarım kalmış çerçeve tamamlanmaz; karşı tarafın ayrıştırıcısı yeniden eşlenir (06 §4).

Korunanlar: açılış sayacı ve olay sayacı (aynı açılışta kimlik tekrarı olmasın diye olay sayacı sıfırlanmaz), tanılama sayaçları, parametreler. Oturum değişimi çekirdeğe `plat` bağlantı olayı olarak bildirilir; çekirdeğin arm ve kip durumunu platform değiştirmez (8.2 md.5). Arm durumu kalıcı belleğe ve `.noinit`'e yazılmaz: her sıfırlamadan sonra kart devre dışı açılır ve yeni oturumda gelen yeni bir arm komutu gerekir; host tamponlarında kalmış eski oturum baytları `sess` denetiminde reddedilir (06 §2.9).

### 2.8 Gözetim, sağlık ve güvenli durum

İki kavram ayrıdır (01-sol md.6):
- **İç sağlık:** kartın kendi yazılımı ilerliyor mu. IWDG beslemesini yalnız bu belirler.
- **Dış veri kullanılabilirliği:** sim veri gönderiyor mu, oturum var mı, host okuyor mu. Güvenli çıktı ve bildirim üretir, sıfırlama üretmez. Aksi halde sim başlamadan açılan kart sıfırlama döngüsüne girer.

02 §4.4'ün `alive_ok`'u taze IMU'yu da içerdiği için gözetimde olduğu gibi kullanılamaz; 02'den iç sağlık (`core_ok`) ve veri kullanılabilirliği (`data_ok`) olarak ayrılması beklenir (8.2 md.1). Ayrılana kadar kart, çekirdek çağrısının tamamlanmasını ve çıktının sonlu, sınır içinde olmasını iç sağlık kanıtı sayar; `alive_ok` yalnız bildirilir. 03'ün `alive_ok`'a verdiği (a) adım tamamlandı ve (b) iç tutarlılık koşulları iç sağlığa düşer; (c) arka plan ilerlemesi 03'ün kendi kararıyla sıfırlatmaz (03 §2.7).

**Platform durumları:** `BAŞLATMA` (besleme yok; 2 s'lik süre içinde bitmeyen başlatma sıfırlanır), `NORMAL`, `GÜVENLİ_BEKLEME`. **İlerleme koşulları** (her sayaç tek yönlü artar; koşul "bekleyen iş varken sayacın `S` süresinden uzun ilerlememesi"dir; 01-sol md.8):

| # | İzlenen | Bekleyen iş | `S` (tahmin) | Durum |
|---|---|---|---|---|
| 1 | Ana döngü turu | her zaman | — (döngü durursa besleme durur) | hepsi |
| 2 | Tamamlanan `step`/`tick` sayısı ve son çağrının iç sağlığı | her zaman (ızgara ≥ 100 Hz garanti eder) | 20 ms | `NORMAL` |
| 3 | Çalıştırılan ızgara tick'i | ızgara kesmesi sayacı ilerledi | 20 ms | `NORMAL` |
| 4 | Bağlantı servisi çağrısı | her zaman | 20 ms | bağlantı başlatıldıysa |
| 5 | Ayrıştırılan bayt; geri çağrının tükettiği damga | RX halkasında, CDC FIFO'sunda ya da damga halkasında veri var | 20 ms | bağlantı başlatıldıysa |
| 6 | Çekirdeğe teslim edilen parti | kapanmış parti var | 10 ms | `NORMAL` |
| 7 | Flash işlemi | donanım işlemi sürüyor | veri sayfasındaki en büyük sürenin 2 katı (**doğrulanmadı**) | hepsi |
| 8 | Bütünlük | her değerlendirme | — | hepsi: yığın deseni, `ida_core_t` ve `ida_link_t` uç koruma sözcükleri |
| 9 | Güvenli beklemenin nötr çıktısı | ızgara anı geldi | 20 ms | `GÜVENLİ_BEKLEME` |

Koşullara girmeyenler (dış neden): gönderim kuyruğunun boşalması, oturum yokluğu, IMU ve sensör tazeliği, eş canlılığı, 03'ün arka plan ilerlemesi (03 `SAFE`'e geçer ve sayar; arka plan dilimi kilitlenirse ana döngü durur ve 1 yakalar). Değerlendirme her A geçişinin sonunda yapılır (aralık ≤ 3 ms; tahmin); hepsi tutarsa IWDG beslenir. Besleme yalnız ana döngüden yapılır: zamanlayıcı kesmesinden beslemek ana döngü kilitlense bile sistemi canlı gösterir (D08 E:Watchdog). `GÜVENLİ_BEKLEME`'de çekirdek çağrılmadığı için 2, 3 ve 6 aranmaz; 9 aranır. Böylece güvenli bekleme de ilerleme kanıtıyla beslenir ve sıfırlama döngüsüne dönmez (01-sol md.7).

**IWDG süresi.** Nominal 100 ms: bölücü 4, reload 799 → `800 × 4 / 32.000 Hz`; çözünürlük 125 µs. Seçilebilir aralık 00'ın düzeltilmiş değeriyle nominal 32 kHz LSI'de ≈ 125 µs – 131 s'dir; ham rapordaki 16 ms – 4096 s yanlıştır (07.10). Gerçek süre LSI'nin sapmasına bağlıdır; `LSIPREDIV` ile 250 Hz seçimi süreleri değiştirir (D08 E:IWDG). Sapma DS13543'ten alınır ve M19'da ölçülür; aşağıdaki hesap ±%5 varsayımıyladır (tahmin): gerçek süre 95,2–105,3 ms.
- Ana döngü ya da bir dilim kilitlenirse son besleme kilitlenmeden öncedir: sıfırlama ≤ 105,3 ms içinde.
- Döngü dönerken bir görevin ilerlemesi durursa: fark etme ≤ `S` (20 ms) + değerlendirme aralığı (3 ms), son besleme bundan önce; sıfırlama ≤ 20 + 3 + 105,3 ≈ 128 ms (hesap; girdiler tahmin). Sürüm 0.1'in 50 ms'lik penceresiyle bu süre ≈ 150 ms'ye kadar çıkıyordu (01-sol md.8).
- Ölçüt: IWDG süresi, M5'te ölçülen en uzun besleme aralığının en az 4 katıdır. Beklenen en uzun aralık bir A + B turu ≈ 1,75 ms'dir (2.4).

**Platform kapısı (olaylar).** Atış izninin bağlayıcı güvencesi çekirdektedir: izni daraltan komutlar ve arıza eylemi atış modülünden önce uygulanır ve olay çıkış anındaki güncel izinle yeniden denetlenir (iskelet §3.2; 02 §2.2 md.4 ve md.7). Platform kapısı derinlemesine savunmadır, bağımsız bir engel değildir (aynı işlemci ve girdi; 03 §2.7). Bir olay ancak şunların hepsi tutarsa `ida_link`'e verilir (01-sol md.2):
1. Çağrı `ida_core_step`'tir ve `OK` döndü (`tick`, `DUP_SEQ`, `OLD_SEQ`, `BAD_INPUT` olay taşıyamaz).
2. Aynı çağrının komutları arasında `DISARM` ya da `FC_ABORT` yok.
3. `FIRE` için: aynı çıktıda `armed = 1` ve çıktı 06'nın sınıflamasında olağan (güvenli değil; 06 §2.4). `ARM_CHANGE` değeri 0 (emniyet kapatma) bu madde dahil her koşulda geçer.
4. Platform `NORMAL`'de, oturum `KOŞUYOR`'da; açılış sayacı geçerli (`event_id_ok`, 2.9).
5. `event_id`'nin üst yarısı güncel `boot_count`'a eşit ve sayaç, aynı açılışta geçen son olaydan büyük.

Silinen olay sayılır ve sonraki çağrıda `plat.gate_drop` ile çekirdeğe bildirilir; 03 tutarsızlıkta `FAULT_LATCHED`'e geçer (03 §2.7).

**Güvenli durumun düzeyleri.** Bu aşamada kartın fiziksel çıkışı yoktur; bütün çıkışlar A1 mesajıdır (K8).
- (a) **Çekirdeğin arıza eylemleri** (02 §4.2): çıktı nötr ya da devre dışı; kart aynen iletir, güvenli sınıfa geçişte `ida_link` güvenlik neslini artırır (06 §2.4).
- (b) **Platform güvenli beklemesi:** çekirdek çağrılamıyor ya da çağrılmamalı (saat hatası, `TZEN = 1`, boyut uyuşmazlığı, ardışık arıza sayacı ≥ 3). Platform 2.7'deki temizliği yapar; bağlantı çalışıyorsa el sıkışmayı sürdürür, her TF'ye ve her ızgara anında nötr güvenli ACTUATOR (`pwm_us` = 1500, `GÜVENLİ`, `armed = 0`) ve durum üretir, olay üretmez. Çıkış yalnız güç kesip açma, NRST ya da 06'nın `REBOOT` kancasıyladır.
- (c) **IWDG ya da hata sıfırlaması.**

**Sıfırlama ve kilitlenmede çıkışlar.** Sıfırlama, işlemci kilitlenmesi (lockup) ya da IWDG süresinin dolmasını bekleme sırasında kart A1'de sessizdir. Nötrlük sim kuralından gelir: kabul edilen son sürekli komut `valid_for_ns` (50 ms) sonunda "PWM sinyali yok" olur; eş canlılığı 500 ms'de kopunca sim iticilere hemen "darbe yok" uygular (06 §2.4, §2.9). Onaylanmamış olaylar RAM'le birlikte kaybolur; yeni açılışın kimlikleri eskileriyle çakışamaz (2.9), süresi geçmiş olay uygulanmaz. Kilitlenme anında USB çevre biriminin FIFO'sunda kalmış bir çerçeve sonradan host'a ulaşabilir; sim onu yinelenen ya da geç olağan çıktı diye reddeder (06 §2.4). Lockup'ın kendiliğinden sıfırlama üretmediği varsayılır **(doğrulanmadı)**; IWDG en geç 105,3 ms'de sıfırlar. Kartın sürdüğü tek fiziksel çıkış LED'lerdir; GPIO'ların sıfırlama durumu emniyetle ilgili değildir. Fiziksel çıkış eklenseydi reset boyunca güvenli seviye dış kapılama ve pasif dirençlerle sağlanmalıydı (00 Bölüm 6, D08 E:Watchdog); bu aşamanın kapsamı dışındadır (K8).

**Hata işleyicileri.** MemManage, BusFault, UsageFault ve HardFault'un ilk komutları `MSP`'yi 1 KB'lık ayrı bir acil yığına, `MSPLIM`'i onun tabanına kurar; C kodu bu yığında koşar. İşleyici `EXC_RETURN`'den hangi yığının kullanıldığını bulur; `CFSR`'deki yığına itme hatası bitlerini (`MSTKERR`, `BSTKERR`, `UFSR.STKOF`) ve SP'nin RAM aralığında, hizalı olduğunu denetler. Yığına itilmiş PC/LR/xPSR ancak bunlar tutarsa okunur; tutmazsa `stacking_valid = 0` (06 `FAULT_RECORD`). `CFSR`, `HFSR`, `MMFAR`, `BFAR`, adım sayacı ve ham duvar sayacı `.noinit`'e CRC'li yazılır, ardışık arıza sayacı artar, `NVIC_SystemReset` çağrılır. İşleyicinin kendisi hata verirse lockup olur ve IWDG sıfırlar (01-sol md.20).

**Ardışık arıza sayacı** (sürüm 0.1'in "60 s'de üç sıfırlama" kuralının yerine; resetler arası saat gerektirmez): `.noinit`'te CRC'li. Hata sıfırlamasında ve IWDG sıfırlamasında (RCC `IWDGRSTF`) artar; güç verme, NRST, planlı `REBOOT` ve `NORMAL` durumda `KOŞUYOR` oturumla `T_kararlı` = 30 s (tahmin) kesintisiz çalışma onu sıfırlar. Açılışta ≥ 3 ise platform güvenli beklemesi. `.noinit`'in sıfırlamada korunması SRAM silme seçenek baytlarına bağlıdır (K0 okur, firmware yazmaz); siliniyorsa sayaç her açılışta sıfırdır ve sonuç sonsuz sıfırlama döngüsü olabilir; her turda çıkışlar sim kuralıyla nötrdür ve açılış kaydı durumu bildirir.

### 2.9 Kalıcı bellek: açılış sayacı ve parametreler

**Yerleşim.** Kod Bank 1'de; Bank 2'nin son dört 8 KB sayfası:

| Sayfa | Başlangıç | İçerik |
|---|---|---|
| C0, C1 | `0x083F8000`, `0x083FA000` | Açılış sayacı kayıtları |
| P_A, P_B | `0x083FC000`, `0x083FE000` | Parametre blokları A ve B |

Bank 2'nin sonu `0x08400000`'dir (2 × 2 MB; hesap). DS13543 iki banka arasında okurken yazmayı (RWW) belgeler (01-sol md.15'in kaynağı): Bank 2'de işlem sürerken işlemci Bank 1'den kesintisiz okur. Bağlayıcı betiği Bank 2'ye bu sayfalardan başka bir şey koymaz; işlem sürerken Bank 2 okunmaz. Ayrıntılı koşullar **(doğrulanmadı)**; M5 kayıt sürerken adım süresini ölçer.

**Flash sürücüsü.** Yazmaç düzeyindedir: HAL'in `_IT` silme işlevi bile başlangıçta bekleme içerdiği için kullanılmaz (01-sol md.15). Her donanım işlemi (sayfa silme, 16 B quad-word yazımı) bir durum makinesi adımıdır: yalnız `BSY = 0` ise başlatılır (beklenmez), bitişi ana döngüde işlem sonu ve hata bayraklarıyla yoklanır, zaman aşımı sağlık koşulu 7'dir. İptal, yeni donanım işlemi başlatmamaktır; sürmekte olan tek işlem (en çok bir sayfa silme) biter ve sonucu kaydedilir. Yarım programlanmış bir quad-word okunduğunda çift bit ECC hatası NMI üretebilir **(doğrulanmadı)**: NMI işleyicisi adres bu dört sayfadaysa hatayı temizler, kaydı geçersiz sayar ve döner; başka adreste hata kaydı ve sıfırlama.

**Açılış sayacı** (iskelet §3.2; 06 §2.4; 06-sol-2 Y14):
- Kayıt 16 B'dır (flash yazma birimi): `magic` u32, `boot_count` u32, `~boot_count` u32, ilk 12 B'ın CRC32'si. Sayfa başına 512 kayıt.
- Açılışta iki sayfa taranır; geçerli kayıtların en büyüğü `N`. `N + 1` sıradaki boş yuvaya yazılır ve geri okunarak doğrulanır. Ancak bundan sonra `boot_count = N + 1` çekirdeğe ve `ida_link`'e verilir ve olay üretimi açılır.
- **Güç kesintisi:** yarım kalan kayıt geçersiz okunur ve atlanır; yuvası yeniden kullanılmaz, yazım bir sonrakine geçer. Doğrulama öncesinde olay kapalı olduğu için yarım kalmış `N + 1` değeriyle hiçbir olay çıkmamıştır; değerin bir sonraki açılışta yeniden ayrılması güvenlidir. Sayaç hiçbir koşulda geri gitmez.
- Bir sayfa dolunca diğeri başlatma sırasında silinir (başlatmada bloklamaya izin vardır; 2 s'lik IWDG içinde) ve yazım ona geçer; silme sürerken dolu sayfa en büyük değeri korur. Ömür: 512 kayıt × 2 sayfa × 10.000 silme döngüsü (döngü sayısı **doğrulanmadı**) ≈ 10,2 milyon açılış (hesap).
- **Tükenme ve hata:** `boot_count` `0xFFFFFFFF`'e ulaşırsa ya da sayaç ayrılamazsa (flash hatası) kart olaysız çalışır: olay üretimi kapalı, `plat.event_id_ok = 0` ile çekirdek arm'ı reddeder, açılış kaydı bildirir. Sarma yoktur.
- **Olay sayacı** çekirdeğin `event_id` kurulumundadır (`boot_count << 32 | sayaç`; 02 §3.4, 06 §2.4). 400 Hz'te her adımda bir olay olsa bile 2³² / 400 s ≈ 124 günde dolar (hesap). Dolduğunda aynı açılışta yeni olay üretilmemesi 02'den beklenir (8.2 md.3); platform kapısı ayrıca azalmayan kimliği denetler (2.8, kapı md.5).

**Parametre deposu** (02 §3.6 biçimi; 01-sol md.16). Blok düzeni, 8 KB'lık sayfa:

| Alan | Konum (B) | İçerik |
|---|---|---|
| Başlık, 32 B | 0 | `magic` "IDAP", `fmt_ver` u16, `schema_hash` u64 (02'nin parametre tablosunun özeti), `payload_len` u32, `gen` u32, `payload_crc` u32, `hdr_crc` u32 |
| Yük | 32 | 02'nin açık kodlanmış kaydı (küçük uçlu, sabit genişlikli; 08.11); en çok 8.192 − 32 − 16 = 8.144 B; azami boyut derlemede `_Static_assert` ile denetlenir |
| Tamamlanma kaydı, 16 B | 8.176 | `magic` "CMIT", `gen`, `~gen`, `hdr_crc`; en son yazılır |

- **Geçerlilik:** başlık CRC'si, yük CRC'si, tamamlanma kaydının `gen`'i eşleşiyor, `fmt_ver` ve `schema_hash` uyumlu. İki blok da geçerliyse `gen`'i seri aritmetikle (işaretli 32 bit fark) büyük olan kullanılır; sarma ancak 2³¹ kayıtta olur, flash ömrünün çok ötesindedir.
- **Kayıt sırası:** eski bloğun sayfası silinir → başlık ve yük yazılır → geri okunup CRC denetlenir → tamamlanma kaydı yazılır ve geri okunur. Tamamlanma kaydı olmayan blok açılışta geçersizdir ve diğer blok kullanılır; güç kesintisi ya da iptal eski geçerli bloğu bozmaz.
- **Koşullar ve bildirim:** kayıt yalnız çekirdek `param_save` istediğinde ve devre dışıyken başlar. Sonuç çekirdeğe `plat.param_save` (`IDLE`, `BUSY`, `OK(gen)`, `FAIL(neden)`) ile döner; `BUSY` iken arm reddedilir (02'den beklenen; 8.2 md.2). Güvenli duruma ya da oturum sonuna girişte kayıt iptal edilir ve `FAIL(İPTAL)` döner.
- **Açılış:** geçerli blok yoksa derlenmiş varsayılanlar yüklenir ve arm reddedilir (02 §3.6).

### 2.10 Bellek

Flash 2 × 2 MB (Bank 1 `0x08000000`, Bank 2 `0x08200000`; silme 8 KB, yazma 16 B). Ana SRAM: SRAM1 768 KB + SRAM2 64 KB + SRAM3 832 KB + SRAM5 832 KB = 2.496 KB, `0x20000000`'dan bitişik; ayrıca SRAM4 16 KB ve yedek SRAM 2 KB vardır, toplam 2.514 KB (DS13543 §3.5; 01-sol hesabı). SRAM3'ün ECC'si açılırsa erişilebilir kapasite azalır; ilk sürümde ECC seçenek baytı değiştirilmez, K0'da okunur.

| Bölge | İçerik |
|---|---|
| Flash Bank 1 | Vektör tablosu, kod, sabitler |
| Flash Bank 2, son dört sayfa | Açılış sayacı ve parametre blokları (2.9) |
| SRAM1 | `.data`, `.bss`: `ida_core_t` (< 64 KB; 02 §2.2), `ida_link_t` (≈ 12 KB, tahmin: 5 parti yuvası ve 2 Ping360 yuvası, 2 × 1.200 B veri; 06 §2.7), CDC FIFO'ları (8 + 4 KB), VCP halkaları (4 + 2 KB), damga halkaları, gönderim kuyrukları, tanılama |
| SRAM2 | Ana yığın 16 KB (tahmin), altında MPU koruma bölgesi; acil hata yığını 1 KB; `.noinit` (hata kaydı, ardışık arıza sayacı; CRC'li) |
| SRAM3 | 03'ün belleği (< 32 KB; 03 §2.3) |
| SRAM5 | Tek ekran çerçeve tamponu, 480 × 480 × 2 B = 460.800 B (RGB565). İki tam tampon 921.600 B > 851.968 B olduğundan sığmaz (01-sol hesabı); çift tampon gerekmez (2.12) |

Kartta ayrıca OctoSPI NOR, PSRAM ve eMMC vardır (R01 §2.1); bu aşamada kullanılmaz.

**Bellek ayırma yasağı ve denetimi** (08.13 düzeltilmiş hali; 01-sol md.21): (1) firmware `-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free` ile sarmalayıcılar tanımlanmadan bağlanır; herhangi bir başvuru bağlantı hatası verir ("boş malloc tanımı" hata üretmez, kullanılmaz). (2) Bağlanmış ELF `nm` ile `malloc` ailesi, `_malloc_r`, `_sbrk`, `printf` ailesi, `pvPortMalloc` ve bilinen kütüphane havuz ayırıcıları için taranır. (3) Sembol taraması özel havuz ayırıcılarını yakalamaz; üçüncü taraf ve proje kodundaki havuzlar kod incelemesinde denetlenir ve yasaktır. (4) Libc açıkça seçilir ve sabitlenir: birincil Picolibc (CubeMX 6.18 CMake varsayılanı; 08.7), yedek newlib-nano. (5) HAL ve TinyUSB bu denetimden geçer. Bağlayıcı betiğinde yığın (heap) bölgesi yoktur; bütün boyutlar `_Static_assert` ile sabittir.

**Yığın:** `-fstack-usage` ve `-Wstack-usage=` ile işlev başına sınır; özyineleme yasak (02 §2.7). Çalışmada `MSPLIM` (aşımı UsageFault), açılışta desenle boyama ve tanılamada en yüksek kullanım, büyük yapıların uçlarında koruma sözcükleri (2.8 koşul 8). Kabul: en yüksek kullanım ≤ %75 (12 KB), kesme ve hata bağlamları dahil; acil yığın ayrıdır.

### 2.11 Tanılama

Sayaçlar O(1) güncellenir; kodlama B dilimindedir. `IDA_DIAG` (06 §2.2; ≤ 255 B yük) 1 Hz'te bir grup taşır, gruplar sırayla döner; istek üzerine bütün grupların anlık görüntüsü ve sayaç sıfırlama komutu vardır. Alanların bayt düzeni sözleşme işinde sabitlenir.

| Grup | Sayaçlar |
|---|---|
| Adım | Adım ve tick sayısı; adım süresi son/en kötü/ortalama (DWT) ve 16 kutulu histogram; `deadline_miss`; `overrun`; `S_kart` (`e₃ − e₂`) en kötü ve histogram; ızgara kaçırması |
| Kesme | Pencere başına en büyük toplam kesme süresi; kesme kaynağı başına sayı; en uzun kritik bölüm |
| Parti ve alım | `DUP`, `OLD`, `GAP`, parti eksik, eski oturum, yineleme ve çelişki; üye taşması; komut erteleme tamponu doluluğu; `rx_stamp_err`; Ping360 eksik |
| Bağlantı | Bayt ve çerçeve; CRC ve çerçeveleme hataları; USB bus sıfırlaması, askıya alma, bağlanma ve kopma; VCP taşma, gürültü ve çerçeve hataları; DMA hataları |
| Gönderim | Kritik ve olağan kuyruk en yüksek doluluğu; değiştirilen ACTUATOR; `tx_drop`, `tx_crit_stall`; olay yinelemesi, süresi dolan ve oturum sonunda silinen olaylar; `gate_drop` |
| Döngü ve sistem | Tur süresi en kötü; dilim başına en kötü süre; boşta tur oranı (yük kestirimi, tahmin niteliğinde); yığın en yüksek kullanımı; en uzun besleme aralığı; beslemenin reddedildiği koşul bitleri; ardışık arıza sayacı; flash işlem süreleri ve hataları |

M5 ölçüm derlemesinde (`fw-measure`) adım süreleri ve `S_kart` ham akış olarak da verilir; tam dağılım PC'de çıkarılır. Dış ölçüm için adım başında ve sonunda bir GPIO isteğe bağlı değiştirilir; ölçüm cihazı 1,8 V seviyeyi algılamalıdır (01.6).

### 2.12 Ekran

Karttaki 2,47" yuvarlak 480 × 480 dokunmatik ekran (MIPI DSI; dokunmatik I2C5; R01 §2.1) yalnız durum göstergesidir; önceliği düşüktür (D01 N8). Gösterilenler: bağlantı yolu ve oturum durumu, kip ve arm, adım hızı, en kötü adım süresi ve `S_kart`, kaçırılan adım, sıfırlama nedeni, hata kaydı özeti, platform durumu. Dokunmatik yalnız sayfa değiştirir; arm, kip ya da atış ekrandan verilmez.

Çizim B dilimidir (≤ 200 µs; tahmin), 2–5 Hz'te güncellenir (500–200 ms; tahmin); doldurma ve kopyalama DMA2D ile. Panel başlatma (DSI saat hattının yazılımla ters çevrilmesi; R01 §2.3) durum makinesiyle ilerler. DSI video kipinde LTDC tamponu sürekli okur: 460.800 B × 60 Hz ≈ 27,6 MB/s (hesap; tazeleme hızı varsayım); bu bellek yolu yükü adım süresini etkileyebilir, M5 ekran açık ve kapalı ölçülür. Komut kipi ve GFXMMU **(doğrulanmadı)**. Grafik kütüphanesi kullanılmaz: sabit bit eşlemli yazı tipi ve DMA2D ile küçük bir çizim modülü; ST'nin DK BSP'si ve DSI örneği panel başlatma referansıdır. LVGL'in bu panel için portu yoktur (R08 §2.8), TouchGFX Designer macOS'ta yerel değildir (D01 #10, K4); bu tasarımda ikisi de yoktur. Derleme seçeneği `IDA_DISPLAY=0` DSI/LTDC/DMA2D kodunu ve kesmelerini çıkarır; ekran kapalıyken durum LED'lerle (PE0 yeşil, PE1 kırmızı) ve A1 üzerinden verilir.

### 2.13 Derleme, yükleme, hata ayıklama

Genel iş akışı 09'undur; burada yalnız karta özgü olanlar vardır.
- **Araç zinciri:** GNU Tools for STM32 14.3.rel1 sabitlenir, yedek Arm GNU 15.3.rel2 (R08 §3). CMake + Ninja + `CMakePresets.json` (08.6); ön ayarlar `fw-debug`, `fw-release`, `fw-measure`. Çekirdek C17 `-pedantic-errors` ile (02 §2.7); HAL, CMSIS ve TinyUSB GNU uzantıları kullandığı için platform kodu `gnu17` ile, uzantılar yalnız `firmware/` içinde.
- **Kütüphaneler:** `stm32u5xx_hal_driver`, `cmsis_device_u5`, CMSIS, TinyUSB 0.21.0 ve (yedekte) FreeRTOS git alt modülü olarak sürüm etiketine ya da Cube U5 v1.9.0'ın sabitlediği commit'e kilitlenir (D08 #3). Başlangıç kodu, bağlayıcı betiği, `system_*.c`, `stm32u5xx_hal_conf.h` bir kez kopyalanıp sahiplenilir. CubeMX `.ioc` dosyası pin ve saat referansı olarak tutulur, üretilen kod derlemeye girmez; bu bir tercih, zorunluluk değil (08.6).
- **Komut satırı (macOS ve Windows aynı):** `cmake --preset fw-release`, `cmake --build --preset fw-release`; yükleme `STM32_Programmer_CLI -c port=SWD -w <elf> -v -rst`; hata ayıklama `ST-LINK_gdbserver` + `arm-none-eabi-gdb`; yedek OpenOCD 0.12.0 (U5A flash desteği doğrulanmadı). Hiçbiri kartla denenmedi. Windows'ta ST-LINK sürücüsü gereksinimi M21 ile denenir. SWO kartta varsayılan olarak çalışmaz; günlük VCP'den alınır.
- **Derleme denetimleri:** bellek ayırma denetimi (2.10), `-fstack-usage` raporu, bağlayıcı haritası ve bölge doluluğu (Bank 2'de yalnız dört sayfa), `HAL_Delay` ve `printf` başvurusu taraması, statik analiz (02 §2.7'deki küme).

## 3. Arayüzler

### 3.1 A4'ün kart tarafı

Kart 02 §3.1'deki çağrıları yapar:

```c
void        ida_core_init(ida_core_t *c, const ida_params_t *p, uint32_t boot_id, ida_wtime_t now_w);
ida_step_rc ida_core_step(ida_core_t *c, const ida_step_in_t *in, ida_out_t *out);  /* IMU'lu parti, DUP */
void        ida_core_tick(ida_core_t *c, const ida_tick_in_t *in, ida_out_t *out);  /* IMU'suz parti, 10 ms ızgara */
void        ida_fc_background(const ida_fc_bg_in_t *in, ida_fc_bg_out_t *out);     /* B dilimi; 02 §3.7, 03 */
```

- `boot_id`: kartta kalıcı açılış sayacıdır (2.9); 06 §2.4 uyarınca rastgele değildir.
- **Girdiler:** örnekleri, komutları, sonar dilimini ve `time_frame`'i `ida_link_take` doldurur (06 §3 eşlemesi). Kart ekler: `now_w`, her örneğin `t_rx`'i (2.3), `bg_result` jetonu (03 §2.9), `plat`. Izgara tick'inde `time_frame`, örnek ve komut yoktur.
- **`plat` alanları (kartın katkısı):** önceki çağrının süre aşımı bayrağı ve süresi; `overrun`; `OLD`/`GAP`/atlanan adım, parti eksik ve eski oturum sayaçları (06 §3); bağlantı durumu ve olayları (oturum kuruldu, bitti, askıya alma); üye ve komut taşmaları; `param_save` sonucu ve meşgul bayrağı; `event_id_ok`; `gate_drop`; `tx_crit_stall`. Tam liste 02 §8.1 md.6 ile sözleşmede.
- **Kullanılan çıktılar:** sürekli komut alanları (`thr_valid`, `src`, `seq`, `t_meas`, `thr_cmd[8]`, `armed`, `valid_for_ns`), `events[]` (platform kapısından geçerek), `telem[]`, `param_save`, `alive_ok` (yalnız bildirim; 2.8).
- **Dönüş kodları:** `OK` → olağan yanıt. `DUP_SEQ` → çekirdeğin kapılanmış yeniden üretimi `YENİDEN` bayrağıyla, olay yok (06 §2.3). `OLD_SEQ` → yanıt yok. `BAD_INPUT` → kapılanmış yeniden üretim `YENİDEN` bayrağıyla gönderilir, olay yok, `bad_input` sayılır, tazelik yenilenmez (02 §3.1). Her durumda kart her kapanan partiye tek ACTUATOR verir (06 §1).

**`us_from_thr`** (06'nın beklediği dönüşüm; 01-sol md.22): `pwm_us = round(1500 + 400 · thr)`, [1100, 1900] µs'ye kırpılır. Değerler 04 §2.7.1'deki ESC'nin üretici değerleridir (1100/1500/1900 µs) ve 04'ün ters ilişkisi `u = (PWM − 1500)/400` ile birebir tutarlıdır. Dur ölü bandı (1500 ± 25 µs) çekirdeğin itki eğrisi tersinde ele alınır (02 §2.5); kart doğrusal eşlemeden başka bir şey yapmaz. `thr_valid = 0` → `CMD_VALID = 0` ve bütün `pwm_us` = 0 (sim komut saymaz); sonlu olmayan değer → çıktı nötr güvenli çıktıyla değiştirilir ve `plat` ile bildirilir. Üç değer B sınıfı parametredir (02 §3.6), 04 ile aynı A5 sürümünden.

### 3.2 Bağlantı sürücüsü arayüzü

```c
typedef struct {
    ida_rc            (*init)(void);
    void              (*service)(void);                                      /* bloklamaz */
    size_t            (*read)(uint8_t *buf, size_t max, ida_wtime_t *t_rx);  /* tek damga aralığı */
    size_t            (*write)(const uint8_t *buf, size_t n);                /* sığan kadar; kısmi olabilir */
    size_t            (*write_space)(void);
    bool              (*rx_pending)(void);                                   /* A geçişinin denetimi */
    void              (*reset_rx)(void);                                     /* oturum sonu temizliği */
    ida_link_status_t (*status)(void);                                       /* bağlı, askıda, sayaçlar */
} ida_link_ops_t;

extern const ida_link_ops_t ida_link_usb_cdc;    /* TinyUSB */
extern const ida_link_ops_t ida_link_stlink_vcp; /* USART1 + DMA */
```

`read` yalnız aynı damgayı taşıyan baytları döndürür; sonuç `ida_link_rx(l, b, n, t_rx)`'e aynen verilir (06 §3). Kısmi yazımın sürdürülmesi gönderim yöneticisinindir (2.6).

### 3.3 `ida_link` entegrasyonu ve sürücü sınırı

- `ida_link_cfg_t` (06 §3): `ctrl_rate_hz` (400 ya da 200), `max_ping360_samples` = 1200, `boot_id` ve `boot_count` (kartta aynı değer; 8.3 md.2), ham `reset_flags`, sürüm alanları, `us_from_thr` (3.1), `reboot` kancası (planlı `NVIC_SystemReset`; ardışık arıza sayacını artırmaz), `T_olay` = 10 ms (06 §2.4), kritik olmayan gönderim bütçesi `B_olağan` (2.6).
- `ida_link_put_out`: adım ve parti `tick`'i `async = false`; ızgara tick'inin güvenli çıktısı `async = true` (yalnız güvenli çıktı gönderilir; 06 §2.3). Dönen baytlar kritik sınıfa girer. ACTUATOR'ın tanı alanlarını kart verir: `rx_wall_us` (TF'nin kesme damgası, `e₂`), `step_us` (DWT), `rx_to_tx_us` (`e₃ − e₂`; 06 §2.5).
- `ida_link_poll_tx`: her A geçişinde; olay yinelemeleri kritik, diğerleri olağan sınıfa.
- **Sürücü sınırı:** A4 örnek yapıları (`ida_hdr_t` ve sensör yapıları; 02 §3.3) ve çağrı girdisinin kuruluşudur. Bugün tek üretici `ida_link_take`'tir. Gerçek bir sürücü eklenirse aynı yapıları doldurur ve kendi tetik düzenini getirir; bu aşamada tasarlanmaz (K8). `ida_link` kartın simülasyondan haberdar olan tek parçasıdır.

### 3.4 A1 üzerinden verilen kart bilgisi

Mesaj biçimleri 06 §2.2'nindir; kart şu içerikleri üretir:
- **`IDA_BOOT_RECORD`** (her oturum kuruluşunda): firmware sürümü ve derleme kimliği, ham `reset_flags` ve yorumlanan neden, `boot_count`, ardışık arıza sayacı, `DEV_ID`/`REV_ID`, `SYSCLK`, regülatör, PLL varyantı, `TZEN`, `BOR_LEV`, SRAM silme seçenek baytları, FPSCR, PHY ayar varyantı, bağlantı yolu, ekran, platform durumu, `event_id_ok`.
- **`IDA_FAULT_RECORD`** (varsa): `CFSR`, `HFSR`, `MMFAR`, `BFAR`, yığına itilmiş PC/LR/xPSR ve `stacking_valid`, adım sayacı, ham duvar sayacı, `fault_seq` (2.8).
- **`IDA_DIAG`** (2.11) ve **`IDA_BOARD_STATUS`** (06'nın alanları; `cpu_load` boşta tur oranından, tahmin niteliğinde).

## 4. Hata durumları ve davranışı

| Durum | Algılama | Davranış |
|---|---|---|
| Sim çalışmıyor, kablo çıkık, HIL duraklatıldı | Eş canlılığı (06 §2.9), IMU bayatlığı (çekirdek), USB olayları | Oturum `KOPUK`; 2.7 temizliği; çekirdek ızgara tick'iyle arıza eylemine gider (02 §4.3); sıfırlama yok (2.8) |
| Host okumuyor | Gönderim kuyruğu boşalmıyor | Olağan çerçeveler atılır, olaylar süresi dolunca düşer; sıfırlama yok (2.6) |
| Yeniden bağlanma | Sim `HELLO`'su | Yeni oturum; `BOOT_RECORD`; sayaçlar korunur, eski oturum baytları reddedilir |
| USB askıya alma | TinyUSB olayları | Kopma gibi; uzaktan uyandırma yok; PC'de seçici askıya almanın kapatılması 06 §4 ve 09'dan |
| USB başlatma hatası | Dönüş kodu, numaralandırma yok | Üç deneme (tahmin), sonra LED ve ekran; kendiliğinden VCP'ye geçilmez |
| İşlemci hata kesmesi, yığın sınırı aşımı | Ayrı işleyiciler | Acil yığın, `.noinit` kaydı, ardışık sayaç, sıfırlama; sonraki oturumda `FAULT_RECORD` (2.8) |
| İç kilitlenme ya da görev ilerlemesinin durması | Sağlık koşulları 1–8 | Besleme durur; sıfırlama ≤ 128 ms (2.8; tahmin) |
| Üç ardışık arıza ya da IWDG sıfırlaması | `.noinit` sayacı | Platform güvenli beklemesi |
| Saat hatası, `TZEN = 1`, boyut uyuşmazlığı | Başlatma | Platform güvenli beklemesi |
| Açılış sayacı ayrılamadı ya da tükendi | 2.9 | Olaysız çalışma, arm reddi, bildirim |
| Parametre CRC, sürüm ya da özet hatası | Başlatma | Varsayılanlar, arm reddi (02 §3.6) |
| Kayıt sırasında güç kesintisi ya da NRST | 2.9 | Eski blok geçerli kalır |
| Flash işlemi zaman aşımı | Sağlık koşulu 7 | Kayıt `FAIL`; besleme durur |
| Besleme düşmesi (BOR) | RCC bayrağı | Neden olarak bildirilir. Kartın VDD'si 1,8 V'tur (D01 #4); 11.19'daki eşiklere göre yalnız BOR0 (düşen 1,58–1,69 V) uyumludur, BOR1 ve üstü 1,8 V'un üstündedir (hesap). `BOR_LEV` okunur, yazılmaz. Sanal batarya çökmesi ile kart sıfırlaması ayrı testlerdir (11.19, M27) |
| Üye, komut ya da bayt taşması; süre aşımı; `DUP`/`OLD`/`GAP` | 2.4 | Sayaç ve `plat`; davranış 06 §2.3 ve 02 §4.3 |
| Platform kapısının sildiği olay | 2.8 | `gate_drop`; 03 `FAULT_LATCHED` (03 §2.7) |

## 5. Kalite kademelerinin etkisi

Kart yazılımında kalite kademesi yoktur (iskelet §6). Dolaylı etki: sonar kademesi Ping360 profilinin örnek sayısını değiştirir (1200 ya da 600), biçimini değiştirmez (06 §5); kamera kapalıysa dışarıdan algılama gelmez. Ekranın açık ya da kapalı olması ve ölçüme bağlı yedeklere geçiş kademe değildir.

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm | Yedeğe geçiş ölçütü (tahmin) |
|---|---|---|---|---|
| A1 taşıması | USB HS CDC, TinyUSB | VCP ≥ 3 Mbaud, 200 Hz ve `L_act = 3h` ile birlikte (06 §6) | M1, M2, M3 | İki işletim sisteminde 100/100 numaralandırma yoksa ya da 06 §7'nin bariyer kabulü `L_act` = h ve 2h ile sağlanamıyorsa |
| USB yığını | TinyUSB 0.21.0 | HAL PCD + upstream Eclipse USBX standalone (MIT; 08.4) | M2 | TinyUSB ile HS kararsızken ST dizisi kararlıysa |
| PHY ayarı | Ayarsız | `COMPDISTUNE`/`SQRXTUNE` ayarlı | M2 | Ayarlı varyant daha iyi sonuç verirse |
| VBUS algılama | Kapalı, oturum zorlanır | PG1/UCPD/TCPP01 | M2 | Kopma olaylarla güvenilir algılanmıyorsa |
| CDC / vendor-bulk | CDC | Vendor-bulk (WinUSB, libusb) | M1 | CDC, 06'nın `RTT_taşıma` p99,9 ≤ 0,40 ms eşiğini sağlamazken vendor-bulk sağlıyorsa |
| USB alım damgası | `DOEPINTx.XFRC` sarmalayıcısı | Geri çağrı anı (kaba damga) | M1, M5 | Sarmalayıcı TinyUSB'nin kesme işleyişini bozarsa ya da damga sırası tutmazsa |
| Regülatör | SMPS | LDO | M4 | SMPS yolunda 1 saatte hata ya da USB HS ile kararsızlık |
| PLL dizisi | Birincil (4 MHz giriş) | Yedek (16 MHz giriş) | M4 | Birincilde USB PHY ya da DSI kararsızsa |
| Döngü yapısı | RTOS'suz, A/B geçişi | FreeRTOS iki görev, statik | M5 | Adım ≤ 1,0 ms iken `S_kart` en büyüğü dilimlenemeyen iş yüzünden 1,70 ms'yi aşarsa |
| Kontrol hızı | 400 Hz | 200 Hz | M5, M17 | Adımın en kötü süresi > 1,0 ms ya da 06'nın bariyer kabulü 400 Hz'te sağlanamıyorsa |
| `B_olağan` (USB) | 2.048 B/periyot | Düşürülür | M1 | Olağan veri ACTUATOR'ın taşıma gecikmesini ölçülebilir biçimde artırıyorsa |
| IWDG süresi | 100 ms | Ölçülen en uzun besleme aralığı × 4 | M5, M19 | Ölçülen en uzun aralık 23,8 ms'yi (95,2 / 4) aşarsa süre büyütülür |
| Ekran | Kütüphanesiz çizim, açık | Kapalı (`IDA_DISPLAY=0`) | M5 | Ekran açıkken `S_kart` ya da adım süresi bütçeyi aşıyorsa |
| Libc | Picolibc | newlib-nano | M10 | Sayısal karşılaştırmada libm farkı açıklanamıyorsa |

## 7. Test yaklaşımı ve kabul ölçütleri

Her kart testi macOS ve Windows'ta tekrarlanır; sonuçlar ham veriyle ve açılış kaydıyla (silikon revizyonu dahil) saklanır (R08 §2.9). Sayılar başlangıç önerisidir; ilk kabul koşusundan önce dondurulur (02 §7 kuralı).

| # | Ölçüm | Araç | Ölçüt |
|---|---|---|---|
| K0 | Fabrika durumu (M6) | `STM32_Programmer_CLI -c port=SWD -ob displ`, açılış kaydı | `TZEN`, `RDP`, `BOR_LEV`, SRAM silme ve ECC seçenek baytları, `REV_ID` kayıtlı; yazma yok; ilk yüklemeden önce |
| K1 | Derleme, yükleme, hata ayıklama (M21) | CMake ön ayarları, Programmer CLI, gdb | İki işletim sisteminde tekrarlanabilir; durdurulunca IWDG sıfırlamaz (`fw-debug`) |
| K2 | 160 MHz ve regülatör (M4) | `SYSCLK` geri okuma; DWT'nin PC saatine oranı | SMPS ve LDO ayrı ayrı 1 saat, USB HS yükü altında, hatasız |
| K3 | Numaralandırma ve PHY (M2) | Soğuk/sıcak takma, 100 tekrar × 2 PHY varyantı × 2 işletim sistemi | %100, HS hızında |
| K4 | Kayıp (M1) | 256 B × 1 kHz iki yönde, ≥ 30 dk | Kayıp 0, sıra sürekli |
| K5 | Uçtan uca bariyer yolu (M1, M17; 06 §2.5) | 06'nın `e₁`–`e₄` noktaları; `e₁ − W(t_n)` dahil; 400 Hz, `L_act = h`, ≥ 30 dk (720.000 adım) | Her adımda `e₄ ≤ D_n` (kaçırma 0); `S_kart` p99,9 ve en büyük raporlanır, en büyük ≤ 1,70 ms; tutmazsa 06'nın sırası (`2h`, sonra 200 Hz ve `3h`) |
| K6 | VCP gerçek hızı (M3) | 2, 3, 6, 12 Mbaud; sayaç + CRC, ≥ 10 dk | Kayıpsız en yüksek baud ve gecikme |
| K7 | Döngü süresi (M5) | DWT histogramları; boşta, tam sensör yükü, en hızlı Ping360, 03 arka planı, ekran açık/kapalı, parametre kaydı sürerken, soğuk önbellek; ≥ 1 saat | Adım en kötü ≤ 1,0 ms; `S_kart` en büyük ≤ 1,70 ms; pencere başına kesme süresi ≤ 125 µs; dilimler sınır içinde; bariyerli 1 saatlik koşuda kaçırma ve `bg_late` 0 |
| K8 | Gözetim (M19) | Ana döngüde, adımda, ayrıştırıcıda, arka plan diliminde sonsuz döngü; servisin, ızgaranın, parti tesliminin durdurulması; koruma sözcüğü bozma; LSI ölçümü | Her iç arıza ölçülen LSI ile 2.8'deki formülün verdiği sürede sıfırlar; sim kapalıyken açılış, kablo çekme, HIL duraklatma, host'un okumayı kesmesi ve güvenli bekleme 10 dk boyunca sıfırlama üretmez |
| K9 | Hata işleyicileri | NULL erişimi, hizasız erişim, `MSPLIM` aşımı, geçersiz SP ile hata | Kayıt doğru, `stacking_valid` doğru; üç ardışık arızada güvenli bekleme; NRST sayacı sıfırlar |
| K10 | Sıfırlama nedeni (M27) | Yazılım, NRST, IWDG | Her neden açılış kaydında doğru; arm kalıcı değil |
| K11 | Askıya alma | Host uykusu, seçici askıya alma | Kopma gibi işlenir; devamda yeni oturum |
| K12 | Bellek | Bağlama denetimi, `nm`, harita, `-fstack-usage`, 1 saat sonra boyalı yığın | Ayırma başvurusu yok; yığın ≤ %75 |
| K13 | Tazelik ve IMU kesilmesi | Hata enjektörüyle (06 §2.12) yinelenen, eski, oturum dışı IMU; IMU durdurma (TF sürer); bütün akışı durdurma | Geçersiz veri tazeliği yenilemez; son IMU alımından nötr çıktıya ≤ 24,2 ms (TF sürerken) ve ≤ 30,4 ms (veri yok; tick kuyruğa verilişi) |
| K14 | IMU'suz komut | IMU'suz partide `DISARM` | Aynı partinin ACTUATOR'ı `armed = 0` |
| K15 | Olay ve açılış sayacı | Olay kaybı, çoğaltma; olay sırasında sıfırlama; sayaç yazımı sırasında güç kesintisi (anahtarlı USB hub; 06 §7'deki araç), ≥ 100 tekrar | Olay tam bir kez; yeniden başlatmadan sonra eski kimlik ve eski arm uygulanmaz; sayaç hiç geri gitmez, kimlik tekrarı yok |
| K16 | Gönderim | Sim okumayı 1 s keser, sonra sürdürür; olağan trafik en yüksek | Sıfırlama yok; CRC hatası 0 (çerçeve bütünlüğü); güvenli çıktı atılmaz; olağan bütçe aşılmaz |
| K17 | Oturum temizliği | Bekleyen arm komutu ve olay varken oturum değişimi | Yeni oturumda uygulanmaz |
| K18 | Sonar tampon ömrü | En hızlı Ping360 + arka plan, bariyerli | `bg_late` 0; yuva adım dönüşünde serbest |
| K19 | Parametre deposu | Kayıt sırasında NRST ve güç kesintisi, ≥ 100 tekrar | Açılışta eski ya da yeni blok tam geçerli; arm, kayıt `BUSY` iken reddedilir |
| K20 | Duvar saati | Sayaç taşmaya yakın değerle başlatılır; `PRIMASK` ve `BASEPRI` altında ve kesme bağlamında 10⁷ okuma | Monoton; geri sıçrama ve 2³² µs'lik ileri sıçrama yok |

**PC'de birim testleri** (Unity; 02 §7): gönderim yöneticisi (kısmi yazım, çerçeve sınırında öncelik, `R_krit`, değiştirme ve silme kuralları), sağlık değerlendiricisi (her koşulun tek başına beslemeyi kesmesi; dış nedenlerin kesmemesi; durum başına koşul kümesi), platform kapısı (her madde tek başına olayı siler), açılış sayacı ve parametre deposu (sahte flash ile yarım yazım, ECC hatası, sayfa geçişi, tükenme), duvar saati okuma algoritması (taşma ve bayrak sıraları), VCP damga eşleme. Bağlantı katmanı sahte `ida_link_ops_t` ile sınanır.

## 8. Açık noktalar ve iskelete öneriler

### 8.1 Açık noktalar

1. **Doğrulanmamış donanım ayrıntıları:** 32 bit zamanlayıcı seçimi; `PLLRGE` kodları, booster ve flash bekleme durumu dizisi; `DOEPINTx.XFRC` okumasının yan etkisizliği; USART1 karakter eşleşmesi; RWW koşulları; yarım yazımda ECC davranışı ve NMI; flash silme ve yazma süreleri ile döngü ömrü; lockup davranışı; LSI sapması; SRAM silme seçenek baytlarının fabrika değerleri; BOR bayrağının ilk güç verişteki durumu.
2. **TinyUSB ayrıntıları:** OSAL'ın statik oluşturması (FreeRTOS yedeği) ve servis çağrısının en kötü süresi (M5).
3. **WWDG** ikinci bir dar koruma olarak eklenebilir; ilk sürümde yoktur.

### 8.2 02 numaralı bölümden beklenenler

1. **`alive_ok`'un ayrılması:** iç sağlık (`core_ok`: çağrı tamamlandı, sayısal çökme yok, iç tutarlılık; 03'ün (a) ve (b) koşulları) ile veri kullanılabilirliği (`data_ok`: taze IMU, bağlantı). 02 §4.4'ün tanımı taze IMU'yu da içerdiğinden kart onu gözetimde kullanamaz (2.8). İskelet §8'deki "kontrol, haberleşme ve veri tazeliği denetimlerinin birleşik sonucu" ifadesi kartta "denetimlerin koşması ve ilerlemesi" olarak uygulandı; tazeliğin kendisi beslemeye girmez.
2. **`plat` alanları:** `param_save` sonucu ve `BUSY` iken arm reddi; `event_id_ok = 0` iken arm reddi; `gate_drop`; bağlantı olayları (3.1).
3. **Olay kimliği:** `ida_core_init`'in `boot_id`'si kartta kalıcı açılış sayacıdır; olay sayacı tükenince aynı açılışta yeni olay üretilmemesi.
4. **Çıktıda arıza eylemi düzeyi:** 06'nın güvenli/olağan sınıflaması ve platform kapısı için `ida_out_t`'de bulunmalı.
5. **Oturum değişiminde arm:** kart oturum sonunu `plat` ile bildirir, arm'ı değiştirmez. Yeni oturumda arm'ın sürüp sürmeyeceği 02 ve 03'ün kararıdır; öneri: oturum sonu `DISARM` eşdeğeri sayılsın (atış izni yeni oturumda yeniden kurulsun).
6. **IMU bayatlığı sınırı:** 02 §7'nin dondurduğu "≤ 20 ms + bir duvar çağrısı periyodu" sınırına kartın dağıtım gecikmesi (ızgara durumunda ≤ 0,35 ms; 2.5) eklenmeli.

### 8.3 06 ve 03 numaralı bölümlere notlar

1. **06:** `BAD_INPUT` yanıtı için `YENİDEN`'den ayrı bir bayrak gerekip gerekmediği; `IDA_DIAG`'ın grup düzeni (2.11) ve bayt düzeni sözleşmede.
2. **06:** `HELLO`'daki `boot_id` ve `boot_count` kartta aynı değeri taşır; iki alanın ayrı anlamı varsa sözleşmede tanımlanmalı.
3. **03:** 300 µs'lik arka plan dilimi `S_kart`'ın en büyük platform kalemidir. Dilim 150 µs'ye indirilirse `S_kart` 1,545 ms'ye iner ve `L_act = h`'de taşıma payı 0,40'tan 0,55 ms'ye çıkar (hesap).

### 8.4 İskelete öneriler

1. **§4 tetik ifadesi:** "Kartta döngüyü yeni IMU örneğinin gelişi tetikler" yerine "IMU örneğini içeren adım partisinin kapanışı tetikler" (06 §2.3 ile aynı anlam; 2.4).
2. **§2 tablosu "Kart yazılımı C17":** çekirdek ISO C17; platform kodu GNU lehçesiyle (`gnu17`), uzantılar yalnız `firmware/` içinde (R08 §2.6).
3. **§7 depo düzeni:** `firmware/third_party/` (alt modüller) ve `firmware/cubemx/` (referans `.ioc`).
4. **§8 A1 taşıması yedeği:** "ST'nin USB yığını" yerine "HAL PCD + upstream Eclipse USBX (MIT)" (08.4).

İskeletle çelişen bir gereklilik yoktur. Bariyerde geciken adımın politikası iskelet §4'te bağlanmıştır (son komutla ilerle, say, çalıştırmayı geçersiz işaretle) ve 06 §2.5 uygular; bu bölümde açık nokta değildir. Ham rapor 08'in "adım 400 Hz zamanlayıcı kesmesinde" önerisi yerine iskeletin veri tetiklemeli modeli uygulandı (2.4).
