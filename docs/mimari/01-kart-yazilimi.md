# 01 — Kart yazılımı

Sürüm: 0.1 (2026-11-12). Durum: taslak; mimar denetimi ve kullanıcı onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) Bölüm 9, satır 01'in kapsamını yazar; iskeletle çeliştiği yerde iskelet geçerlidir. Dayanak gösterimi: `08.12`, `07.10` gibi numaralar [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) (bundan sonra "00") Bölüm 4'teki düzeltme satırıdır; `K`, `Ö`, `Ç`, `M`, `A` aynı belgenin karar, çelişki, ölçüm ve açık soru numaralarıdır; `08 §2.3` ham rapor bölümü, `D08 #14`, `D08 Y8`, `D08 E:Watchdog` doğrulama maddesidir; `B02 3.1` mimari bölüm [02-ortak-cekirdek.md](02-ortak-cekirdek.md) içindeki başlıktır. **(tahmin)** işaretli her sayı ölçülmemiş başlangıç değeridir; ilgili ölçüm çıkmadan kesinleşmez. **(doğrulanmadı)** işaretli donanım ayrıntısı RM0456 ya da kart şemasından uygulama sırasında doğrulanır.

## 1. Amaç ve sorumluluklar

Kart yazılımı STM32U5A9J-DK'yı başlatır, A1 bağlantısını yürütür, A4 portunun kart tarafını uygular ve ortak çekirdeği (B02) zamanında çağırır. Kart simülasyonda olduğunu bilmez (iskelet §2): bugün bütün sensör verisi A1'den mesaj olarak gelir (K8); gerçek sensör, ardıl kart ve fiziksel çıkış bu aşamada yoktur. Sürücü sınırı yine de gerçek sürücülerin sonradan ekleneceği yerde tutulur (3.3).

Sorumluluklar: başlatma ve saat, bağlantı sürücüsü (USB ve yedek seri port), A1 ↔ A4 dönüştürücüsünün kart tarafı, döngü zamanlaması, kesme ve bellek düzeni, zaman kaynakları, gözetim zamanlayıcısı ve güvenli durum, kalıcı parametre deposu, tanılama, karttaki ekran, kart yazılımına özgü derleme ve yükleme.

| Yazmadığı konu | Bölüm | Bu bölümün ondan beklediği |
|---|---|---|
| Kestirim, kontrol, kipler, arıza eylemleri | 02 | B02 3.1'deki çağrı sözleşmesi; `alive_ok` tanımının 8.2'deki öneriyle netleşmesi |
| Atış kontrol mantığı, hedef algılama | 03 | `ida_fc_background` işinin sınırlı dilimlerle çağrılabilmesi (2.4) |
| A1 mesaj kümesi, adım bariyeri, el sıkışma, MAVLink eşlemesi | 06 | Mesaj alanları ile port alanlarının eşlemesi; tanılama ve açılış kaydı mesajları (3.4); tetik olayının tekilleştirilmesi (8.2) |
| Ölçüm işlerinin yürütülmesi, CI, genel derleme akışı | 09 | PC tarafı ölçüm aracı; iki işletim sisteminde otomatik derleme |

Temel ilkeler:
1. **Çekirdek tek bağlamda çağrılır.** `ida_core_t` üzerinde bütün çağrılar ana döngüden yapılır; kesme ile ana döngü arasında çekirdek verisi paylaşılmaz (2.4).
2. **Hiçbir çağrı bloklamaz.** Başlatma bittikten sonra bekleme döngüsü, `HAL_Delay`, bloklayan flash silme ya da USB yazımı yoktur (08.12).
3. **Bellek ayırma yoktur.** Bütün yapılar statiktir; yasak bağlama aşamasında denetlenir (2.6).
4. **Karta özgü kod tek katmanda toplanır.** Kart değişse ya da gerçek sürücü gelse yalnız kart destek katmanı ve sürücüler değişir (08 §3 ek öneri 1).
5. **Sağlık kanıtı ilerleyen yazılımdan gelir.** Serbest çalışan donanım zamanlayıcısı canlılık kanıtı sayılmaz (07.11).

## 2. İç yapı

### 2.1 Katmanlar

| Katman | Yer (`firmware/…`, iskelet §7) | Sorumluluk | Karta özgü mü |
|---|---|---|---|
| Kart destek katmanı (BSP) | `bsp/` | Başlangıç kodu, bağlayıcı betiği, saat ve güç dizisi, pin tablosu, NVIC öncelikleri, MPU, IWDG, duvar saati zamanlayıcısı, DWT, flash sürücüsü, sıfırlama nedeni, LED | Evet; kart değişiminde yeniden yazılan tek yer |
| Bağlantı katmanı | `link/` | `ida_link_ops_t` (3.2); USB HS CDC (TinyUSB) ve ST-LINK VCP (USART1 + DMA) gerçeklemeleri | USB/UART sürücü bağı karta özgü; arayüz değil |
| A1 ↔ A4 dönüştürücüsü | `app/conv` (paylaşım önerisi 8.2) | Baytları A1 çerçevelerine, çerçeveleri port örneklerine çevirir; çıktıları A1'e kodlar | Hayır; SIL koşucusuyla aynı C kodu olabilir |
| Örnek biriktirici | `app/acc` | Son adımdan beri gelen örnek ve komutları sabit kapasiteli yuvalarda tutar; sürücü sınırıdır (3.3) | Hayır |
| Çekirdek çağırıcı | `app/runner` | `ida_core_t`'nin statik sahibi; init, adım, duvar saati çağrısı, süre ölçümü, `plat` alanları, 03 arka plan dilimi, parametre kaydı | Hayır (DWT okuma BSP üzerinden) |
| Gözetim değerlendiricisi | `app/health` | Birleşik sağlık koşulu; IWDG besleme kararı; güvenli durum | Hayır (besleme çağrısı BSP'de) |
| Tanılama | `app/diag` | Sayaçlar, süre histogramları, açılış ve hata kaydı | Hayır |
| Ekran | `app/display` | Durum göstergesi; derlemeden çıkarılabilir | Panel sürücüsü karta özgü |

```
 USB OTG HS ISR ─┐ (TinyUSB olay kuyruğu)        Duvar saati ISR (taşma) ── 64 bit sayaç
 USART1 DMA ISR ─┤ (halka tampon)
                 ▼
 ana döngü: link.service ─► link.read ─► conv ─► acc ─┬─(IMU tamam)─► runner: ida_core_step ─► conv ─► link.write
                                                      └─(IMU yok > T)─► runner: ida_core_tick_wall ─┘
            ─► param kaydı adımı ─► diag dilimi ─► 03 arka plan dilimi ─► ekran dilimi ─► health ─► IWDG besleme
```

### 2.2 Başlatma ve saat

Sıra (her adımın sonucu açılış kaydına yazılır; 3.4):

1. **Sıfırlama işleyicisi:** yığın işaretçisi ve yığın sınırı (`MSPLIM`, 2.6), `.data`/`.bss` kurulumu, FPU açılması. **FPSCR sabitlenir:** en yakına yuvarlama, `FZ = 0`, `DN = 0`; değer hem `FPSCR`'ye hem varsayılan bağlam değerine yazılır, geri okunup kaydedilir. Gerekçe: PC varsayılanı subnormalleri korur; aynı politika SIL'de de seçilir (B02 2.7, 09.5). Bu ayar bit eşitliği sağlamaz (Ç7).
2. **Sıfırlama nedeni:** RCC durum bayrakları (BOR, pin, yazılım, IWDG, WWDG, düşük güç, option byte yüklemesi) okunur, kaydedilir, temizlenir (D11 E:BOR). Açılışta BOR bayrağının güç verme sırasında da kalkıp kalkmadığı, yani gerçek BOR ile ilk açılışın nasıl ayrılacağı **(doğrulanmadı)**. Önceki çalışmadan kalan hata kaydı `.noinit` bölgesinden CRC denetimiyle alınır (4).
3. **Silikon kimliği:** `DBGMCU->IDCODE` içinden `DEV_ID` ve `REV_ID` kaydedilir; ES0553 X/W revizyon ayrımı için (00 Bölüm 6, M6).
4. **TrustZone denetimi:** option byte yazmacından `TZEN` okunur. İmaj `TZEN = 0` varsayımıyla hazırlanır; `TZEN = 1` görülürse çekirdek çalıştırılmaz, platform güvenli beklemesine geçilir (2.7) ve durum bildirilir. "TZEN=1 iken non-secure imaj hiç çalışmaz" ifadesi fazla kesindir; sorun hazırlıksız imajla başlatmaktır (08.3). Firmware option byte yazmaz. İlk yüklemeden önce salt okunur `STM32_Programmer_CLI -c port=SWD -ob displ` ile durum alınır (08 §2.4, M6; sözdizimi kartla denenmedi). `BOR_LEV` de aynı okumada kaydedilir (4).
5. **Güç:** PWR saati açılır, sonra regülatör seçilir. Birincil **SMPS**, yedek **LDO**. SMPS 160 MHz için zorunlu değildir; DS13543 Tablo 38 LDO/Range 1/160 MHz'i 1,71–3,6 V kapsamında belgeler (01.1, 08.9, Ç4). SMPS'in birincil olma gerekçesi yalnızca ST'nin bu kart için yayımladığı örnekle aynı diziyi kullanmaktır (08 §2.4); iki yol M4'te ayrı sınanır. U585'te PWR saati açılmadan `ConfigSupply` çağrısının zaman aşımına uğradığı bildirilmiştir **(doğrulanmadı;** 08 §2.4). Gerilim ölçeği Scale 1 (Range 1); booster ve flash bekleme durumu ST örneğindeki diziyle ve RM0456'ya göre ayarlanır **(doğrulanmadı)**.
6. **Saat:** 160 MHz açıkça yapılandırılır; varsayılan saat MSIS 4 MHz'tir ve bu saatte hiçbir hız testi yapılmaz (01.8, D01 N7). Kaynak HSE 16 MHz (USB HS PHY ve DSI için gerekli; 08 §2.4). Birincil PLL1: `M=4, N=80, R=2` (bu kartın ST host örneği); yedek `M=1, N=20, R=2` (Nucleo ve TinyUSB U5A5 BSP'si). İkisi de 160 MHz verir (D08 #8). USB PHY saati: `RCC_USBPHYCLKSOURCE_HSE`, `SYSCFG_OTG_HS_PHY_CLK_SELECT_1` (16 MHz) (08 §2.2). Yapılandırmadan sonra `SYSCLK` geri okunur; HSE açılmaz ya da PLL kilitlenmezse MSIS'e sessizce dönülmez: "saat hatası" ile platform güvenli beklemesine geçilir (kırmızı LED deseni; USB HSE'siz zaten çalışmaz).
7. **Önbellek ve MPU:** ICACHE açılır; M5 ölçümü açık önbellekle ve soğuk önbellek senaryosuyla yapılır (önbellek en kötü durumu değişken kılar). MPU: sıfır adresi erişimsiz, yığın altı koruma bölgesi, RAM ve çerçeve tamponu yürütülemez.
8. **IWDG erken başlatılır:** başlatma süresini kapsayan uzun süreyle (2 s, tahmin); ana döngüye girişte çalışma süresine indirilir (2.7). Hata ayıklama derlemesinde çekirdek durdurulunca IWDG'yi donduran DBGMCU biti açılır (Zephyr'in kart ayarındaki `STOP_WATCHDOG 1` ile aynı amaç; 08 §2.1); sürüm derlemesinde açılmaz.
9. **Zaman kaynakları:** duvar saati zamanlayıcısı ve DWT çevrim sayacı (2.5).
10. **Parametreler:** flash'taki A/B bloklarından geçerli olan yüklenir (3.5); `ida_core_init` çağrılır. `sizeof(ida_core_t)` ile `ida_core_sizeof()` karşılaştırılır; uyuşmazlıkta platform güvenli beklemesi.
11. **Bağlantı:** seçili yol başlatılır (2.3). USB başlatma hatası (ör. `USB_CoreReset` zaman aşımı; 08 §2.2 forum bildirimleri) üç kez yeniden denenir (tahmin), sonra bildirilir.
12. **Ekran** (derlemede açıksa): panel başlatma ana döngüde adım adım ilerleyen bir durum makinesi olarak yapılır; başlatma sırasını bekletmez (2.9).
13. Başlatma tamamlandı bayrağı kalkar; bundan sonra `HAL_Delay` çağrısı tuzağa düşer (2.4).

### 2.3 Bağlantı katmanı

**Birincil: USB 2.0 HS CDC (CN4, gömülü HS PHY), TinyUSB 0.21.0, `dwc2` sürücüsü** (Ö1, iskelet §8). TinyUSB MIT lisanslıdır, işletim sistemi gerektirmez; 0.21.0 kaynağı U59/U5A gömülü HS PHY ayrımını içerir (D08 #13). Kart üzerinde hiçbir yığınla doğrulanmış HS CDC yoktur (Ç8, 08 R1); TinyUSB güçlü adaydır, kanıtlanmış çözüm değildir (D08 N2). CDC'nin birincil olma gerekçesi iki işletim sisteminin yerel sürücüsüdür (01 §2.2); vendor-bulk (WinUSB/libusb) M1'de karşılaştırılır.

Bu kart için eksik olan ve yazılacak parçalar:
- **Kart dosyası:** TinyUSB'de U5A9J-DK BSP'si yoktur (D08 #6). U5A5 Nucleo BSP'sindeki saat ve PHY dizisi örnek alınır; ama U5A5 BSP'sindeki ADC tabanlı VBUS algılama, bölücü değerleri ve Type-C davranışı DK şemasıyla eşleştirilmeden kopyalanmaz (08.15, D08 E:TinyUSB). Önce ST'nin Nucleo-U5A5ZJ USBX örneğinin PHY dizisi bu kartta "bilinen iyi başlatma" referansı olarak çalıştırılır (08 §3 md.3, D08 N2); kart dosyası o diziyi taklit eder.
- **PHY ayarı:** `SYSCFG->OTGHSPHYTUNER2` içindeki `COMPDISTUNE` ve `SQRXTUNE`; HAL başlığı +%5,9 ve +%15'i "önerilen" işaretler, ST'nin Nucleo örneği bu üç ayar işlevini çağırmaz, yalnız referans saat seçimini çağırır (08.2, D08 #3). Zorunluluğu kanıtlanmamıştır; ayarlı ve ayarsız iki derleme varyantı M2'de sınanır.
- **VBUS algılama:** kart ST-LINK portundan beslenir, CN4 kendinden beslemeli cihazdır (01 §2.1). Birincil: VBUS algılama kapalı, oturum geçerliliği yazılımla zorlanır (Nucleo örneği ve TinyUSB BSP'sindeki `BVALOEN/BVALOVAL` yöntemi; 08 §2.2). Kopma, USB askıya alma/sıfırlama olayları ve A1 kalp atışıyla anlaşılır. Yedek: PG1 / UCPD / TCPP01 üzerinden algılama, şemayla eşleştirildikten sonra (M2, 08 R3). DK'da VBUS algılama açıkken kesmenin gelmediği bildirilmiştir; kök neden kanıtlanmadı (08 §2.2).
- **Uç nokta ve tamponlar:** HS bulk azami paket 512 bayt (Windows'ta yanlış değer "Code 43" verir; 01 §2.2). CDC RX ve TX yazılım tamponları 4 KB (tahmin; en az iki Ping360 profili ve birkaç adım çerçevesini tutacak boyut).

Bilinen riskler: HS kararlılığı ve PHY ayarı (M2); bulk aktarımda ayrılmış bant genişliği yoktur, CDC ya da vendor-bulk seçimi 400 Hz'te uçtan uca gecikme garantisi vermez (D08 E:USB HS, 00 Bölüm 6); Windows ve macOS CDC sorunları için kesinleşmiş işletim sistemi kusuru kanıtı yoktur, ölçülür (09.8, M1); USB yedek yığını gerekirse HAL PCD + **upstream Eclipse USBX standalone (MIT)**; ST'nin ThreadX/USBX çatalı lisansı listelenen donanımla sınırlıdır (08.4).

**Yedek: ST-LINK V3E sanal seri portu (VCP).** USART1 PA9/PA10 (D01 #6), DMA dairesel alım + boşta hat kesmesi, DMA ile gönderim. ST, V3E için 15 MHz VCP değeri yayımlar (UART bağlamında yaklaşık 15 Mbaud, 15 MB/s değil; 01.3). Bu değer DK üzerinde kayıpsız sürekli akış ya da gecikme garantisi değildir; kullanılacak baud M3 ile seçilir. Yeterlilik kare boyu ve hızına bağlıdır (01.2): 8N1'de 921600 baud ≈ 92.160 B/s, 2 Mbaud ≈ 200.000 B/s (hesap). Örnek: adım başına 128 B sensör çerçevesi 400 Hz'te 51,2 KB/s, buna Ping360 için 1200 B × 30 profil/s ≈ 36 KB/s eklenirse ≈ 87 KB/s olur (çerçeve boyu ve profil hızı tahmin; kesin boyutlar 06'nın); 921600 baud sınırda, ≥ 2 Mbaud gerekir.

**İki yol arasında geçiş:** iki gerçekleme aynı `ida_link_ops_t` arayüzünü uygular (3.2); dönüştürücü, çağırıcı ve çekirdek hangi yolun etkin olduğunu bilmez. Seçim kalıcı parametre `LINK_SEL` ile yapılır; açılışta B1 (PC13) basılıysa yedek yol zorlanır. Çalışırken kendiliğinden yol değiştirilmez (PC tarafı karışmasın diye). USB kipinde VCP yalnız metin günlüğü taşır; VCP kipinde günlük A1 içine tanılama mesajı olarak katılır (06).

Kartta tek A1 kanalı vardır; yer istasyonu trafiği sim sunucusunun bağlantı emülatöründen aynı kanaldan gelir (iskelet A1, 06).

### 2.4 Döngü zamanlaması

**Yapı: RTOS'suz, tek ana döngü, adım IMU örneğiyle tetiklenir** (Ö1, iskelet §4, §8). Kesmeler yalnız donanım olaylarını işler; çekirdek, dönüştürücü, 03'ün arka plan işi ve ekran ana döngüde, sabit sırayla koşar.

**Kontrol adımı ana döngüde koşar.** Gerekçe:
1. Tetik bir zamanlayıcı değil, verinin gelişidir. TinyUSB'nin modelinde sürücü kesmesi olayları kuyruğa atar, veri uygulama bağlamında `tud_task()` ile işlenir (D08 #14, #15). IMU çerçevesi zaten ana döngüde çözülür; adımı bir kesmeye taşımak, çözümleme ana döngüde beklediği sürece gecikmeyi azaltmaz.
2. Çekirdek verisi tek bağlamda kalır: tutarlı sensör görüntüsü için kilit, çift tampon ya da kesme maskesi gerekmez (D08 Y8).
3. `ida_fc_step` ile `ida_fc_background` aynı bağlamda olduğu için 03'ün durumunda yarış yoktur.
4. ISR içinde ağır hesap ve HAL zaman tabanına bağlı bekleme riski doğmaz (08.12).

Bedeli: ana döngüdeki her iş sınırlı süreli dilimlere bölünmelidir; IMU varışından adımın başlamasına kadarki en kötü gecikme, o an koşan dilimin süresi kadar büyür. 08'in "400 Hz zamanlayıcı kesmesinde adım" önerisi serbest çalışan döngü içindi; iskeletin IMU tetiklemeli modelinde bu yapı daha basittir. D08 doğrulaması iki seçeneğin de ölçümle savunulabileceğini söyler (D08 Y8).

**Ana döngünün bir turu:**
1. `link.service()` (USB'de `tud_task`, VCP'de DMA indeks güncellemesi).
2. Gelen baytlar dönüştürücüye verilir; bir IMU çerçevesi tamamlanınca besleme durur, kalan baytlar adımdan sonra işlenir.
3. IMU tamamsa: adım girdisi biriktiriciden kurulur, `ida_core_step`, çıktı kodlanır ve yazılır. IMU yoksa ve son IMU varışından beri `T_gap = 10 ms` (4 periyot; B02 4) geçtiyse `ida_core_tick_wall` 100 Hz'te çağrılır (tahmin).
4. Parametre kaydı durum makinesinin bir adımı (3.5).
5. Tanılama ve telemetri kodlama dilimi (hız sınırlı).
6. 03 arka plan dilimi; alım tamponunda bekleyen bayt varsa bu turda atlanır.
7. Ekran dilimi; aynı koşulla atlanır.
8. Gözetim değerlendiricisi ve IWDG besleme kararı (2.7); kalp atışı LED'i.

Döngü `WFI` kullanmaz; gecikmeyi düşürmek için sürekli döner (güç tüketimi bu aşamada önemsiz).

**Kesme öncelikleri** (göreli sıra; mutlak değerler CMSIS'teki öncelik bit sayısına göre uygulamada verilir, **doğrulanmadı**):

| Sıra | Kaynak | İşi |
|---|---|---|
| Sabit | HardFault, MemManage, BusFault, UsageFault (ayrı işleyiciler açık) | Hata kaydı ve sıfırlama (4) |
| 1 (en yüksek atanabilir) | Duvar saati zamanlayıcısı taşma kesmesi | 64 bit sayacın üst yarısını artırır; birkaç komut |
| 2 | USB OTG HS (TinyUSB `dcd` işleyicisi) | Sıfırlama, numaralandırma, uç nokta ve aktarım olayları; uygulama işi yapmaz (D08 #15) |
| 3 | USART1 DMA ve boşta hat (yalnız VCP kipinde) | Halka tampon indeksleri |
| 4 | DSI/LTDC/DMA2D (ekran açıksa) | Aktarım bitti bayrakları |
| En düşük | SysTick (HAL zaman tabanı, 1 ms) | Yalnız başlatma sırasındaki HAL zaman aşımları için |

Kritik bölümler kısa tutulur; ana döngü yalnız ilgili kesmeyi `BASEPRI` ile maskeler. `HAL_Delay` başlatmadan sonra tuzağa düşecek biçimde yeniden tanımlanır (HAL'de zayıf sembol); düşük öncelikli tick'e bağlı bekleme ISR'yi kilitleyebilir (08.12).

**Bloklayan çağrı yasağı:** başlatmadan sonra bekleme döngüsü, `HAL_Delay`, sınırsız bayrak yoklaması, bloklayan flash silme/yazma, TX boşalmasını bekleme, `printf` ailesi yoktur. USB ya da UART'a yazılamayan veri beklenmez: önce aktüatör/tetik çerçevesi yazılır, telemetri kalan yere sığarsa gider, sığmayan telemetri sayılarak atılır; aktüatör çerçevesi sığmazsa ayrı sayaç artar.

**Bütçe (hepsi tahmin; M5 ölçer):** periyot 2,5 ms.

| Kalem | Bütçe |
|---|---|
| `tud_task` + çözümleme (IMU çerçevesi dahil) | ≤ 150 µs |
| Adım girdisinin kurulması | ≤ 20 µs |
| `ida_core_step` (03'ün hızlı kısmı dahil) | ≤ 1000 µs (B02 2.2 hedefi) |
| Çıktının kodlanması ve yazılması | ≤ 100 µs |
| 03 arka plan dilimi | ≤ 300 µs |
| Ekran dilimi | ≤ 200 µs |
| Tanılama/telemetri dilimi | ≤ 100 µs |
| Kesmelerin toplam payı | ≤ %5 |

Buna göre IMU varışından çıktının yazılmasına en kötü gecikme ≈ 300 + 150 + 20 + 1000 + 100 ≈ 1,57 ms, bir döngü turunun en uzunu ≈ 1,9 ms olur (hesap, tahmini girdilerle). Ortalama işlemci yükü en kötü durum yanıtını kanıtlamaz; karar DWT ölçümüyle verilir (08.12, M5). 400 Hz bariyerde 2,5 ms'lik bütçe iki yöndeki USB aktarımını ve PC tarafını da kapsar (iskelet §4, M17); kart tarafı en kötü gecikmenin 1,5 ms civarında kalması hedeftir (tahmin).

**Veri paylaşımı ve tutarlı görüntü:** kesmelerle paylaşılan veri yalnızca (a) TinyUSB'nin kendi olay kuyruğu, (b) VCP halka tampon indeksleri, (c) duvar saatinin üst yarısıdır. Çekirdeğe giden adım girdisi ana döngüde, tek seferde biriktiriciden kopyalanır; adım koşarken biriktirici değişmez.

**Kuyruk taşması, kaçırılan adım, bayat veri:**
- **Biriktirici taşması:** yuvalar sensör türü başına sabit kapasitelidir (toplam `IDA_IN_MAX` = 16; B02 3.1, tahmin). Bir tür dolarsa o türün en eski örneği atılır, yenisi kalır, `acc_overflow[tür]` artar ve sonraki adımın `plat` alanında bildirilir. Ping360 profili ayrı çift tampondadır; tüketilmeden yenisi gelirse eskisinin üzerine yazılır, sayılır.
- **IMU birikmesi:** IMU örnekleri atlanmaz (çekirdek her örneği entegre eder; B02 2.2); her biri sırayla bir adım üretir. Sırada birden çok IMU varsa son tarih kaçırılmıştır: `overrun` sayılır ve `plat`'ta bildirilir. IMU kuyruğu 4'ü aşarsa en eski atılır ve kayıp sayılır (tahmin). Bariyer kipinde sim yanıt beklediği için birikme 1'i geçmemelidir.
- **Süre aşımı:** adım süresi bütçeyi (1,0 ms) aşarsa `deadline_miss` sayılır, süresiyle birlikte sonraki adımın `plat` alanına konur (B02 3.1).
- **Sıra numarası:** yinelenen IMU (`DUP_SEQ`) adım çalıştırmaz, önceki çıktı yeniden gönderilir, ama **tetik alanları sıfırlanarak** (8.2); eski sıra numarası (`OLD_SEQ`) atılır ve sayılır.
- **Bayat veri:** örneğin bayat olup olmadığına çekirdek ölçüm zamanıyla karar verir (B02 2.2 md.2); platform her çerçevenin varış duvar saatini tanılamaya yazar, ölçüm zamanına dokunmaz.

**Yedekler:** (a) çekirdek adımının en kötü süresi bütçeyi aşarsa kontrol hızı **200 Hz**'e iner (iskelet §8; bütçe 5 ms). (b) Adım bütçeye sığdığı halde varıştan çıktıya gecikme, dilimlenemeyen arka plan işi yüzünden aşılıyorsa **FreeRTOS** (MIT, `ARM_CM33_NTZ` portu; 08 §2.3) ile iki görev kurulur: yüksek öncelikte USB servis + çözümleme + adım, düşük öncelikte 03 arka plan + ekran. Bu durumda 03'ün durumu iki görev arasında paylaşılacağı için 03'ten bir devir düzeni gerekir. Geçiş ölçütleri 6. başlıktadır.

### 2.5 Zaman kaynakları

- **Duvar saati (`ida_wtime_t`):** HSE'den türeyen bir donanım zamanlayıcısı 1 MHz sayar; taşma kesmesiyle 64 bite genişletilir ve ns'ye çevrilir (çözünürlük 1 µs). Hangi zamanlayıcının 32 bit olduğu RM0456'dan seçilir **(doğrulanmadı)**. Okuma, üst yarı ile alt yarının tutarlılığını yeniden okuyarak sağlar. Duvar saati PC saatiyle eşlenmez; buna gerek yoktur. Kullanım: bağlantı zaman aşımı, IMU kesilme denetimi, gözetim penceresi, emniyet süreleri (iskelet §4).
- **Süre ölçümü:** DWT çevrim sayacı (160 MHz'te 6,25 ns; 32 bit sayaç ≈ 26,8 s'de taşar, hesap). Yalnız süre farkı için kullanılır; duvar saati yerine geçmez (çekirdek hata ayıklayıcıda durunca DWT de durur).
- **Ölçüm zamanı (`ida_stime_t`):** kart üretmez; A1 mesajlarındaki sim zamanı dönüştürücüde birim çevrilerek aynen aktarılır (Ping360'ın `t_meas`'ı dahil; B02 3.3). Gerçek sürücü geldiğinde aynı alanı veri hazır anındaki kart saati doldurur (B02 3.2 md.3).
- **IMU kesilince:** son IMU varışından beri `T_gap` geçince ana döngü `ida_core_tick_wall`'ı duvar saatiyle 100 Hz'te çağırır (tahmin); çıktısı (itici nötr, tetik sıfır) A1'e "adım dışı" çıktı olarak gider (biçimi 06'dan beklenen). Arıza güvenliği böylece sensör akışına bağlı kalmaz (iskelet §4). HIL'de sim duraklarsa kartın duvar saati ilerler ve bu yol çalışır; istenen davranıştır (B02 4, Ç1).

### 2.6 Bellek

**Yerleşim.** Flash 2 × 2 MB (Bank 1 `0x08000000`, Bank 2 `0x08200000`; silme 8 KB, yazma 16 bayt); SRAM1 768 KB + SRAM2 64 KB + SRAM3 832 KB + SRAM5 832 KB = 2496 KB, `0x20000000`'dan başlayarak bitişik (01 §2.1; kaynak `stm32-data-generated`, ikincil; RM0456 ile doğrulanacak).

| Bölge | İçerik |
|---|---|
| Flash Bank 1 | Vektör tablosu, kod, sabitler |
| Flash Bank 2, son iki 8 KB sayfa | Kalıcı parametre blokları A ve B (3.5) |
| SRAM1 | `.data`, `.bss`: `ida_core_t` (< 64 KB, tahmin; B02 2.2), biriktirici, dönüştürücü, TinyUSB, tanılama |
| SRAM2 | Ana yığın (MSP), 16 KB (tahmin); altında MPU koruma bölgesi |
| SRAM3 | 03'ün çalışma belleği, Ping360 çift tamponu |
| SRAM5 | Ekran çerçeve tamponu (480 × 480 × 2 B = 460.800 B, RGB565; hesap) |
| `.noinit` (SRAM2 sonu) | Hata kaydı ve açılış sayacı; sıfırlamada korunur, CRC'lidir |

Kartta ayrıca OctoSPI NOR, PSRAM ve eMMC vardır (01 §2.1); bu aşamada kullanılmaz.

**Statik ayırma:** bütün yapılar derleme zamanında boyutludur; boyutlar `_Static_assert` ile sabitlenir. Bağlayıcı betiğinde yığın (heap) bölgesi tanımlanmaz, `_sbrk` sağlanmaz.

**Ayırma yasağının bağlama aşamasında denetimi** (08.13 düzeltilmiş hali): (1) firmware `-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free` ile ve sarmalayıcılar **tanımlanmadan** bağlanır; herhangi bir başvuru bağlantı hatası verir. "Boş malloc tanımı" bağlantı hatası üretmez, kullanılmaz. (2) Bağlanmış ELF ve nesneler `nm` ile `malloc`, `calloc`, `realloc`, `free`, `_malloc_r`, `_sbrk`, `printf` ailesi için taranır; yeniden girişli libc girişleri ayrıca denetlenir (08.13). (3) Libc açıkça seçilir ve sabitlenir: birincil Picolibc (CubeMX 6.18 CMake varsayılanı; 08.7), yedek newlib-nano; derleyici, libc ve bağlayıcı ayarları birlikte kaydedilir. (4) Üçüncü taraf kodun (HAL, TinyUSB) bu denetimden geçtiği kabul ölçütüdür. Ekran kütüphanesi istisnası 2.9'dadır.

**Yığın boyutu ve taşma tespiti:** `-fstack-usage` ve `-Wstack-usage=` ile işlev başına sınır raporlanır; özyineleme yasaktır (B02 2.7). Çalışmada üç katman: (a) Cortex-M33 yığın sınırı yazmacı `MSPLIM` yığın tabanının biraz üstüne kurulur; aşım UsageFault üretir ve hata işleyicisinin çalışacağı pay bırakılır (ayrıntı uygulamada doğrulanır). (b) Yığın açılışta desenle boyanır; en yüksek kullanım tanılamada raporlanır. (c) `ida_core_t` ve biriktiricinin iki ucunda koruma sözcükleri; gözetim değerlendiricisi denetler (2.7).

### 2.7 Gözetim ve güvenli durum

**IWDG süresi.** U5'te reload 12 bit, bölücü 4–1024; `t = (reload + 1) × bölücü / f_LSI`; nominal 32 kHz LSI'de ≈ **125 µs – 131 s** (07.10; ham rapordaki 16 ms – 4096 s yanlıştır). Gerçek süre LSI toleransına bağlıdır; `LSIPREDIV` ile 250 Hz seçimi süreleri değiştirir (D08 E:IWDG). Çalışma süresi **100 ms** (tahmin; 40 kontrol periyodu); M5'te ölçülen en uzun ana döngü aralığının en az dört katı olacak biçimde ve M19'da ölçülen gerçek LSI frekansıyla düzeltilerek kesinleşir. Başlatma süresince 2 s (2.2).

**Besleme yalnız ana döngüden ve birleşik koşulla yapılır.** Zamanlayıcı kesmesinden besleme yasaktır: 400 Hz bir ISR'den beslemek USB ya da ana döngü kilitlense bile sistemi canlı gösterir (D08 E:Watchdog). Değerlendirici her turda son `W = 50 ms` (yarım süre, tahmin) içinde şunların **hepsini** arar:
1. **Kontrol:** bir `ida_core_step` ya da `ida_core_tick_wall` çağrısı tamamlandı ve çekirdeğin `alive_ok` bayrağı doğru (B02 4).
2. **Haberleşme:** bağlantı servisi (`tud_task` ya da VCP servisi) çağrıldı ve döndü; alınan bayt varsa çözümleyici ilerledi (çerçeve çözüldü ya da hata sayıldı).
3. **Veri tazeliği denetimi:** IMU kesilme denetimi koştu; IMU çerçevesi çözüldüyse karşılığında adım çalıştı (çözümleme ile adım arasında iç tıkanma yok).
4. **Bütünlük:** yığın deseni ve koruma sözcükleri sağlam.

Bir koşul tutmazsa besleme durur, IWDG süresi sonunda sıfırlar. Yazılım hâlâ ilerliyorsa aynı anda güvenli duruma da geçer.

**Dış neden ile iç arıza ayrılır.** PC'nin hiç veri göndermemesi (sim çalışmıyor, kablo çıkmış, HIL duraklatılmış) yazılım arızası değildir: güvenli durum ve bildirim üretir, sıfırlama üretmez. Aksi halde sim başlamadan açılan kart sıfırlama döngüsüne girer. IWDG yalnız kartın kendi ilerlemesinin bozulmasını yakalar. (B02 4'teki "IMU yokken `alive_ok = false` uzun sürerse gözetim zamanlayıcısı resetler" ifadesiyle bu nokta 8.2'de bildirilmiştir.)

**Serbest çalışan zamanlayıcı kanıt sayılmaz** (07.11): canlılık LED'i, A1 kalp atışı ve tanılama mesajı ana döngüden, sağlık değerlendirmesinden sonra üretilir; hiçbiri donanım zamanlayıcısının ya da DMA'nın kendiliğinden ürettiği bir sinyale bağlanmaz. İleride fiziksel bir çıkış eklenirse (A13) izin, ilerleyen yazılımın sürekli yeniden kurması gereken süreli bir izin olmalıdır.

**Güvenli durumun yazılımdaki anlamı.** Bu aşamada fiziksel fırlatıcı ve itici çıkışı yoktur; atış komutu bir A1 mesajıdır. Güvenli durum şudur:
1. Bütün itici komutları nötr (0) gönderilir.
2. Tetik alanları sıfırdır ve platformda ayrıca maskelenir: platform tetiği yalnız çekirdeğin o adımın çıktısında `armed` ve `fire[i]` birlikteyse iletir; duvar saati çağrısının ve yeniden gönderilen çıktının tetiği her zaman sıfırdır. Bu, 03'ün kararını değiştirmez; ikinci bir kapıdır.
3. Arm durumu kalıcı belleğe ve `.noinit`'e yazılmaz; her sıfırlamadan sonra kart **devre dışı** açılır, yeni arm komutu gerekir.
4. Süren parametre kaydı iptal edilir (A/B düzeni yarım yazımı korur; 3.5).
5. Bağlantı ve tanılama sürer; durum ve neden bildirilir.

Üç düzey vardır: (a) çekirdeğin arıza eylemleri (`HOLD` … `DISARM`; B02 4); (b) **platform güvenli beklemesi:** çekirdek çağrılamıyorsa (saat hatası, `TZEN = 1`, boyut uyuşmazlığı, hata sıfırlaması döngüsü) platform çekirdeği çağırmadan nötr çıktı ve durum mesajı üretir; yalnız açık komutla ya da güç kesip açmayla çıkılır; (c) IWDG ya da hata sıfırlaması.

**Sıfırlama sırasında ve sonrasında çıkışlar:** fiziksel çıkış olmadığı için sıfırlama boyunca kart A1'de sessizdir; PC bağlantı kopması görür. U5'te GPIO'ların sıfırlama durumunun çoğunlukla analog kip olduğu beklenir **(doğrulanmadı)**; fiziksel çıkış eklenirse reset boyunca güvenli seviye dış kapılama ve pasif dirençlerle elektriksel olarak sağlanmalıdır, IWDG'nin varlığından çıkmaz (00 Bölüm 6, D08 E:Watchdog, M19). Sıfırlamadan sonra kart açılış kaydını gönderir; HIL çalıştırması geçersiz sayılır (06 ve 04'ten beklenen).

### 2.8 Tanılama

Sayaçlar `app/diag` içinde O(1) güncellemelerle tutulur; adımın süresini etkilemesin diye kodlama ana döngü diliminde yapılır.

| Grup | Sayaçlar |
|---|---|
| Adım | Adım sayısı; süre son/en kötü/ortalama (DWT); 16 kutulu süre histogramı; `deadline_miss`; `overrun`; varıştan çıktı yazımına gecikme en kötü/ortalama ve histogram; duvar saati çağrısı sayısı |
| IMU | Alınan, sıra boşluğu, `DUP_SEQ`, `OLD_SEQ`, kuyruk en yüksek doluluğu, kesilme olayı ve süresi, varış aralığı en küçük/en büyük |
| Kuyruk | Tür başına biriktirici taşması; Ping360 üzerine yazma; RX/TX tampon en yüksek doluluğu; atılan telemetri; yazılamayan aktüatör çerçevesi |
| Bağlantı | Alınan/gönderilen bayt ve çerçeve; CRC, çerçeveleme ve bilinmeyen mesaj hataları; USB bus sıfırlaması, askıya alma/devam, bağlanma/kopma; VCP taşma, gürültü ve çerçeve hataları; DMA hataları |
| Döngü | Tur süresi en kötü; dilim başına en kötü süre (bağlantı servisi, 03, ekran, tanılama); boşta tur oranı (yük kestirimi, tahmin niteliğinde) |
| Sistem | Sıfırlama nedeni, açılış sayısı, son hata kaydı, `DEV_ID`/`REV_ID`, ölçülen `SYSCLK`, regülatör, `TZEN`, `BOR_LEV`, FPSCR, yığın en yüksek kullanımı, IWDG süresi, en uzun besleme aralığı, beslemenin reddedildiği koşul bitleri, platform durumu |

Dışarı veriliş: A1 üzerinden düzenli tanılama mesajı (1 Hz, tahmin), istek üzerine anlık görüntü, sayaçları sıfırlama komutu ve her bağlantı kuruluşunda bir açılış kaydı (3.4). M5 ölçüm derlemesinde (`fw-measure`) adım ve gecikme süreleri ham akış olarak da verilir; tam dağılım PC'de çıkarılır. Adım başlangıcı ve bitişinde bir GPIO'yu değiştirmek dış ölçüm için isteğe bağlıdır; ölçüm cihazının 1,8 V seviyeyi algılaması gerekir (01.6).

### 2.9 Ekran

Karttaki 2,47" yuvarlak 480 × 480 dokunmatik ekran (MIPI DSI, MB1835; dokunmatik I2C5; 01 §2.1) yalnız durum göstergesidir; önceliği düşüktür, ilk HIL kapsamı dışında tutulması makuldür (D01 N8). Gösterilenler: bağlantı yolu ve durumu, kip ve arm, adım hızı, en kötü adım süresi, kaçırılan adım, sıfırlama nedeni, hata kaydı özeti, platform durumu. Dokunmatik yalnız sayfa değiştirir; araç durumunu değiştiren hiçbir işlev (arm, kip, atış) ekrandan verilmez.

**Kontrol döngüsünü etkilememesi:** çizim ana döngünün en düşük dilimidir (≤ 200 µs, tahmin), 2–5 Hz'te güncellenir (tahmin), alım tamponunda veri varken atlanır. Doldurma ve kopyalama DMA2D ile yapılır. Panel başlatma (DSI saat hattının yazılımla ters çevrilmesi gerekir, 01 §2.3; panel denetleyicisi Zephyr belgesine göre HX8379C, ikincil kaynak) ana döngüde durum makinesi olarak ilerler. DSI video kipinde LTDC çerçeve tamponunu sürekli okur: 480 × 480 × 2 B × 60 Hz ≈ 27,6 MB/s (hesap; tazeleme hızı varsayım); bu bellek yolu yükü adım süresini etkileyebilir, M5 ekran açık ve kapalı ölçülür. Panel komut kipini destekliyorsa yalnız değişen karede aktarım tercih edilir **(doğrulanmadı)**. Yuvarlak ekranda bellek tasarrufu için GFXMMU kullanımı ileride değerlendirilir **(doğrulanmadı)**.

**Grafik kütüphanesi:** birincil **kütüphane yok**: sabit bit eşlemli yazı tipi ve DMA2D doldurmalarıyla küçük bir çizim modülü. Panel başlatma için ST'nin DK BSP'si ve DSI örneği referans alınır (01 §2.4). Gerekçe: LVGL'in MB1835 yuvarlak DSI paneli için portu yoktur (08 §2.8); TouchGFX Designer yalnız Windows'tadır, macOS'ta yerel değildir (08 §2.8, D01 #10), bu da K4 ile uyuşmaz. Daha zengin arayüz gerekirse aday **LVGL** (C, platformdan bağımsız araçlar): kendi port katmanı yazılır; LVGL'in iç bellek havuzu bellek ayırma yasağına istisna olur, istisna statik boyutlu ve ekran modülüyle sınırlı tutulur ve mimar onayı gerekir (8.1).

**Tamamen kapalı çalışma:** derleme seçeneği `IDA_DISPLAY=0` DSI/LTDC/DMA2D kodunu ve kesmelerini derlemeden çıkarır; çalışma parametresi panel başlatmayı atlatır. Ekran kapalıyken bütün işlevler aynıdır; durum yalnız LED'lerle (PE0 yeşil, PE1 kırmızı; 01 §2.1) ve A1 üzerinden verilir.

### 2.10 Derleme, yükleme, hata ayıklama

Genel iş akışı 09'undur; burada yalnız kart yazılımına özgü olanlar vardır.

- **Araç zinciri:** GNU Tools for STM32 14.3.rel1 (CubeCLT/CubeIDE içindeki) sürümü sabitlenir ve yerel arşivde saklanır; yedek Arm GNU 15.3.rel2 (08 §3). CMake + Ninja + **`CMakePresets.json`** (08.6: dosya adı `.json`); toolchain dosyası `cmake/toolchains/` altında; ön ayarlar `fw-debug`, `fw-release`, `fw-measure`. Çekirdek C17 `-pedantic-errors` ile derlenir (B02 2.7); HAL, CMSIS ve TinyUSB GNU uzantıları kullandığı için platform kodu `gnu17` ile derlenir, GNU uzantıları yalnız `firmware/` içinde kalır (08 §2.6).
- **Üreticinin kütüphaneleri:** `stm32u5xx_hal_driver`, `cmsis_device_u5`, CMSIS ve TinyUSB 0.21.0, git alt modülü olarak sürüm etiketine ya da Cube U5 v1.9.0'ın sabitlediği commit'e kilitlenir (08 §2.1; D08 #3 o commit'i gösterir). DK BSP'sinden yalnız ekran sürücüsü ve başlangıç referansı alınır. Başlangıç kodu, bağlayıcı betiği, `system_*.c` ve `stm32u5xx_hal_conf.h` bir kez kopyalanıp sahiplenilir.
- **Yapılandırma aracı (CubeMX):** `.ioc` dosyası pin ve saat doğrulama referansı olarak depoda tutulur; üretilen kod derlemeye girmez. 00'ın düzeltmesine göre bu bir zorunluluk değil tercihtir: ST üst düzey `CMakeLists.txt`'deki kullanıcı değişikliklerini korur; ezilen dosya `cmake/stm32cubemx/CMakeLists.txt`'dir (08.6). Tercihin gerekçesi: derlemenin üretici aracının sürüm değişikliklerinden bağımsız ve iki işletim sisteminde aynı ön ayarla çalışması.
- **Komut satırı (macOS ve Windows aynı):** derleme `cmake --preset fw-release` ve `cmake --build --preset fw-release`; yükleme `STM32_Programmer_CLI -c port=SWD -w <elf> -v -rst`; hata ayıklama `ST-LINK_gdbserver` (CubeCLT 1.21.0'da `-cp <CubeProgrammer/bin>` gerekir) + `arm-none-eabi-gdb`; yedek OpenOCD 0.12.0 (U5A flash desteği doğrulanmadı) (08 §2.1; hiçbiri kartla denenmedi). Windows'ta ST-LINK sürücüsü gereksinimi kaynaklarda çelişkilidir (D01 E); M21 ile uçtan uca denenir. SWO kartta varsayılan olarak çalışmaz (01 §2.1); günlük VCP'den alınır.
- **Derleme denetimleri:** bellek ayırma denetimi (2.6), `-fstack-usage` raporu, bağlayıcı haritası ve bölge doluluğu, `HAL_Delay`/`printf` başvurusu taraması, statik analiz (B02 2.7'deki kümenin firmware'e uygulanması).

## 3. Arayüzler

### 3.1 A4'ün kart tarafı

Kart, B02 3.1'deki çağrıları şu düzende yapar:

```c
void        ida_core_init(ida_core_t *c, const ida_params_t *p, ida_wtime_t now_w);   /* başlatmada bir kez */
ida_step_rc ida_core_step(ida_core_t *c, const ida_step_in_t *in, ida_step_out_t *out); /* her IMU çerçevesinde */
void        ida_core_tick_wall(ida_core_t *c, ida_wtime_t now_w, ida_step_out_t *out); /* IMU kesikken 100 Hz */
void        ida_fc_background(ida_fc_t *fc, ...);                                       /* ana döngü dilimi; 03 tanımlar */
```

Kartın doldurduğu `ida_step_in_t` alanları: `imu` ve biriktiricideki `samples[]` (dönüştürücüden ya da ileride gerçek sürücülerden), `ping360` (çift tampondaki son tam profil, `const` işaretçi), `cmds[]`, `now_w` (2.5), `plat`: önceki adımın `deadline_miss` bayrağı ve süresi (ns), `overrun` sayısı, tür başına biriktirici taşması, bağlantı olayları (kuruldu, koptu, askıya alındı, devam etti). `ida_step_out_t`'den kartın kullandıkları: `seq`, `thr_cmd[8]`, `fire[2]` (2.7'deki maskeyle), `telem[]`, `events[]`, `param_save`, `alive_ok`.

### 3.2 Bağlantı arayüzü

```c
typedef struct {
    ida_rc   (*init)(void);
    void     (*service)(void);                          /* ana döngüden; bloklamaz */
    size_t   (*read)(uint8_t *buf, size_t max);         /* eldeki kadar; bloklamaz */
    size_t   (*write)(const uint8_t *buf, size_t n);    /* sığan kadar; bloklamaz */
    size_t   (*write_space)(void);
    void     (*flush)(void);
    ida_link_status_t (*status)(void);                  /* bağlı, askıda, hata sayaçları */
} ida_link_ops_t;

extern const ida_link_ops_t ida_link_usb_cdc;   /* TinyUSB */
extern const ida_link_ops_t ida_link_stlink_vcp; /* USART1 + DMA */
```

### 3.3 A1 ↔ A4 dönüştürücüsü ve sürücü sınırı

```c
void   ida_conv_init(ida_conv_t *cv);
ida_conv_ev ida_conv_feed(ida_conv_t *cv, ida_acc_t *acc, const uint8_t *b, size_t n,
                          ida_wtime_t now_w, size_t *used);   /* IMU tamamlanınca IMU_READY ile döner */
size_t ida_conv_encode_out(ida_conv_t *cv, const ida_step_out_t *out, uint8_t *buf, size_t cap);
size_t ida_conv_encode_diag(ida_conv_t *cv, const ida_diag_t *d, uint8_t *buf, size_t cap);

void   ida_acc_push(ida_acc_t *acc, const ida_sample_t *s);      /* sürücü sınırı */
void   ida_acc_push_imu(ida_acc_t *acc, const ida_imu_t *imu, uint32_t seq);
void   ida_acc_push_cmd(ida_acc_t *acc, const ida_cmd_t *cmd);
bool   ida_acc_take(ida_acc_t *acc, ida_step_in_t *in);         /* adım girdisini kurar, yuvaları boşaltır */
```

Gereksinimler: statik tamponlar; çerçeve başına sınırlı süre; CRC ve biçim hataları sayılır, çözümleyici kendini bir sonraki çerçeve başında yeniden eşler; sim zamanı ns `ida_stime_t`'ye çevrilir; Ping360 profili parçalardan birleştirilir, eksik parça gelirse profil atılır ve sayılır (parçalama düzeni 06'nın). Mesaj alanları ile port alanlarının eşlemesi 06'dan beklenir.

**Sürücü sınırı biriktiricidir.** Bugün tek kaynak dönüştürücüdür; ileride gerçek bir IMU, basınç ya da DVL sürücüsü aynı `ida_acc_push*` çağrılarıyla aynı yapıları (B02 3.3) doldurur, adımı gerçek IMU'nun veri hazır kesmesi tetikler. Çekirdek, çağırıcı ve gözetim değişmez. Dönüştürücü kartın simülasyondan haberdar olan tek parçasıdır (B02 2.8).

### 3.4 A1 üzerinden verilen kart bilgisi

Mesaj biçimleri 06'nındır; kart şu içerikleri üretir: **açılış kaydı** (her bağlantı kuruluşunda: firmware sürümü ve derleme kimliği, sıfırlama nedeni, açılış sayısı, `DEV_ID`/`REV_ID`, `SYSCLK`, regülatör, PLL dizisi, `TZEN`, `BOR_LEV`, FPSCR, PHY ayar varyantı, bağlantı yolu, ekran açık/kapalı, platform durumu); **hata kaydı** (varsa; 4); **tanılama** (2.8); **adım dışı çıktı** (duvar saati çağrısı).

### 3.5 Kalıcı parametre deposu

`ida_params_t` (B02 3.5) Bank 2'nin son iki 8 KB sayfasında A/B düzeniyle durur: her blokta sürüm, boyut, artan yazım sayacı ve CRC32. Açılışta geçerli ve en yeni blok yüklenir; ikisi de bozuksa varsayılanlar yüklenir ve çekirdek arm'ı reddeder (B02 3.5). Kayıt yalnız çekirdek `param_save` istediğinde ve devre dışıyken yapılır; eski blok silinip yazılır, yazım bloklamayan bir durum makinesiyle ana döngüde ilerler. Bir bankayı silerken diğerinden kod okumanın kesintisiz sürüp sürmediği RM0456'dan doğrulanır **(doğrulanmadı)**; sürmüyorsa kayıt yalnız devre dışıyken ve adım duraklatılmış haldeyken yapılır.

## 4. Hata durumları ve davranışı

| Durum | Algılama | Davranış |
|---|---|---|
| Bağlantı kopması (kablo, host kapanması, sim durması) | A1 kalp atışı yok; IMU kesildi; USB bağlantı/askıya alma olayı; VCP'de veri yok | Çekirdek `tick_wall` ile arıza eylemine gider (B02 4); TX tamponu boşaltılır, yazma beklenmez; sayaç; sıfırlama yok (2.7). Yeniden bağlanınca açılış kaydı yeniden gönderilir; sayaçlar korunur |
| Kart sıfırlanması (yazılım, pin, IWDG, hata) | Açılışta RCC bayrakları | Devre dışı açılır, parametreler yeniden yüklenir, açılış ve hata kaydı gönderilir; HIL çalıştırması geçersiz sayılır (06/04'ten beklenen) |
| USB askıya alma | TinyUSB askıya alma/devam olayları | Bağlantı kopması gibi işlenir; uzaktan uyandırma kullanılmaz; döngü çalışmayı sürdürür. PC'nin seçici askıya almayı kapatması 09'dan beklenen |
| USB başlatma/numaralandırma hatası | Başlatma dönüş kodu; numaralandırma olmaması | Üç yeniden deneme (tahmin), sonra durum LED'i ve ekran; kendiliğinden VCP'ye geçilmez (2.3) |
| İşlemci hata kesmeleri (HardFault, MemManage, BusFault, UsageFault, yığın sınırı aşımı) | Ayrı işleyiciler | En az yığınla: CFSR, HFSR, MMFAR, BFAR, yığına itilmiş PC/LR/xPSR, adım sayacı `.noinit`'e CRC'li yazılır; `NVIC_SystemReset`. Sonraki açılışta A1'e ve ekrana verilir. 60 s içinde üç hata sıfırlaması (tahmin) olursa platform güvenli beklemesinde açılır |
| Besleme düşmesi sıfırlaması (BOR) | RCC BOR bayrağı | Sıfırlama nedeni olarak bildirilir. Kartın VDD'si 1,8 V'tur (D01 #4); 11.19'daki eşiklere göre yalnız BOR0 (düşen 1,58–1,69 V) bu beslemeyle uyumludur, BOR1 ve üstü (≥ 1,90 V) 1,8 V'un üstündedir (hesap, 11.19 tablosundan). `BOR_LEV` açılışta okunur, BOR0 değilse bildirilir; yazılmaz. Sanal batarya çökmesi, kart sıfırlaması ve gerçek besleme çökmesi ayrı testlerdir (11.19, M27); gerçek çökme testi A22'ye bağlıdır |
| IMU akışı kesildi | `T_gap` aşıldı | `tick_wall` 100 Hz; itici nötr, tetik sıfır (2.5) |
| Biriktirici/IMU kuyruğu taşması, süre aşımı | 2.4 | Sayaç, `plat` bildirimi; davranış çekirdeğin (B02 4, "Adım kaçırma") |
| Saat hatası, `TZEN = 1`, boyut uyuşmazlığı | Başlatma | Platform güvenli beklemesi (2.7) |
| Parametre CRC/sürüm hatası | Başlatma | Varsayılanlar, arm reddi (B02 3.5) |
| Ana döngü, çözümleyici ya da adım kilitlenmesi | Gözetim değerlendiricisi | Besleme durur; IWDG sıfırlaması (2.7) |

## 5. Kalite kademelerinin etkisi

Kart yazılımında kalite kademesi yoktur; kademe PC tarafının ayarıdır (iskelet §6). Dolaylı etki: sonar kademesi Ping360 profilinin içeriğini değiştirir, biçimini değiştirmez (B02 5); kamera kapalıysa dışarıdan algılama mesajı gelmez. Ekranın açık ya da kapalı olması kademe değil derleme ve çalışma seçeneğidir (2.9); ölçüme bağlı yedeklere geçiş de kademe değildir (iskelet §6).

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm | Yedeğe geçiş ölçütü (tahmin) |
|---|---|---|---|---|
| A1 taşıması | USB HS CDC, TinyUSB | ST-LINK VCP (M3'te ölçülen baud) | M1, M2, M3 | M2'de iki işletim sisteminde 100/100 numaralandırma sağlanamazsa ya da M1'de 30 dk'da kayıp > 0 giderilemezse |
| USB yığını | TinyUSB 0.21.0 | HAL PCD + upstream USBX standalone (MIT) | M2 | TinyUSB ile HS kararlı çalışmazken ST dizisi çalışıyorsa |
| PHY ayarı | Ayarsız (ST Nucleo dizisi) | `COMPDISTUNE`/`SQRXTUNE` ayarlı | M2 | Ayarlı varyantın numaralandırma ya da kayıp sonucu daha iyiyse |
| VBUS algılama | Kapalı, oturum geçerliliği zorlanır | PG1/UCPD/TCPP01 | M2 | Kopma algılaması olaylarla güvenilir değilse |
| CDC / vendor-bulk | CDC | Vendor-bulk (WinUSB/libusb) | M1 | CDC'nin p99.9 gecikmesi 2,5 ms'lik bariyer bütçesine sığmazken vendor-bulk sığıyorsa |
| Regülatör | SMPS | LDO | M4 | SMPS yolunda 1 saatlik çalışmada hata ya da USB HS ile kararsızlık |
| PLL dizisi | `M=4, N=80, R=2` | `M=1, N=20, R=2` | M4 | Birincil dizide USB PHY ya da DSI kararsızsa |
| Döngü yapısı | RTOS'suz ana döngü, adım ana döngüde | FreeRTOS iki görev | M5 | Adım bütçeye sığarken varıştan çıktıya p99.9 gecikme 1,5 ms'yi aşıyor ve neden dilimlenemeyen arka plan işiyse |
| Kontrol hızı | 400 Hz | 200 Hz | M5, M17 | Çekirdek adımının en kötü süresi 1,0 ms'yi ya da kart tarafı döngü turu 2,0 ms'yi aşarsa; bariyerde gidiş-dönüş p99.9 > 2,5 ms ise |
| IWDG çalışma süresi | 100 ms | M5'in en uzun tur aralığı × 4 | M5, M19 | Ölçülen en uzun aralık 25 ms'yi aşarsa süre büyütülür |
| Ekran | Kütüphanesiz çizim, açık | Kapalı (`IDA_DISPLAY=0`) | M5 | Ekran açıkken adım süresi ya da gecikme bütçeyi aşıyorsa |
| Libc | Picolibc | newlib-nano | M10 | Sayısal karşılaştırmada libm farkı açıklanamıyorsa |

## 7. Test yaklaşımı ve kabul ölçütleri

Her kart testi macOS ve Windows'ta tekrarlanır, sonuçlar ham veriyle saklanır, her çalıştırmada açılış kaydı (silikon revizyonu dahil) ölçümle birlikte kaydedilir (08 §2.9). Ölçütlerdeki sayılar başlangıç önerisidir, kaynaklı eşik değildir.

| # | Ölçüm | Araç | Ölçüt |
|---|---|---|---|
| K0 | Fabrika durumu (M6) | `STM32_Programmer_CLI -ob displ` (salt okunur), açılış kaydı | `TZEN`, `RDP`, `BOR_LEV`, `REV_ID` kayıtlı; yazma yapılmadı |
| K1 | Derleme, yükleme, hata ayıklama (M21) | CMake ön ayarları, Programmer CLI, `ST-LINK_gdbserver` + gdb | İki işletim sisteminde tekrarlanabilir; kesme noktası ve yığın izi çalışır; durdurulunca IWDG sıfırlamaz (`fw-debug`) |
| K2 | 160 MHz ve regülatör (M4) | `SYSCLK` geri okuma; DWT sayımının PC saatine oranı (1 saat); erişilebilirse MCO + sayıcı | 160 MHz ± HSE kristal toleransı; SMPS ve LDO ayrı ayrı 1 saat, USB HS yükü altında, hatasız |
| K3 | USB HS numaralandırma ve PHY (M2) | Soğuk/sıcak takma, 100 tekrar × 2 PHY varyantı × 2 işletim sistemi | %100 numaralandırma, HS hızında (PC'de hız bilgisi) |
| K4 | USB kayıp (M1) | 256 B × 1 kHz iki yönde aynı anda, ≥ 30 dk, sıra numarası ve CRC | Kayıp 0, sıra sürekli |
| K5 | USB gecikme ve jitter (M1) | Yankı turu; kart DWT damgası + PC yüksek çözünürlüklü saat; 200/400/1000 Hz, 64/256 B, ≥ 30 dk; CDC ve vendor-bulk; host duraklaması ve kablo çekme dahil | Medyan, p99, p99.9, en büyük raporlanır; 400 Hz'te gidiş-dönüş p99.9 + kart adım süresi ≤ 2,5 ms ise 400 Hz bariyer uygun |
| K6 | VCP gerçek hızı (M3) | USART1 DMA; 921600, 2, 3, 6, 12 Mbaud; sayaç + CRC, ≥ 10 dk | Kayıpsız en yüksek baud ve gecikme; yayımlanmış 15 MHz garanti sayılmaz |
| K7 | Döngü süresi (M5) | DWT histogramları, `fw-measure` ham akışı; senaryolar: boşta, tam sensör yükü, Ping360 en yüksek hızda, 03 arka planı açık, ekran açık/kapalı, telemetri en yüksek; ≥ 1 saat | Çekirdek adımı en kötü ≤ 1,0 ms; varıştan çıktıya p99.9 ≤ 1,5 ms; 1 saatlik bariyerli HIL'de kaçırılan adım 0 |
| K8 | Gözetim (M19) | Hata enjeksiyonu: ana döngüde, adımda, çözümleyicide sonsuz döngü; bağlantı servisinin durması; koruma sözcüğü bozma; LSI frekansı ölçümü | Her iç kilitlenme ölçülen LSI'ye göre hesaplanan sürede sıfırlar; sim durdurulunca ya da kablo çekilince sıfırlama olmaz, güvenli durum bildirilir |
| K9 | Hata kesmeleri | NULL erişimi, hizasız erişim/sıfıra bölme tuzakları, yığın taşırma | Sıfırlama sonrası hata kaydı A1'de doğru; üç hata döngüsünde platform güvenli beklemesi |
| K10 | Sıfırlama nedeni (M27) | Yazılım, NRST, IWDG; gerçek besleme çökmesi yalnız A22 onaylanırsa | Her neden açılış kaydında doğru ayrılır; arm kalıcı değil |
| K11 | USB askıya alma | Host uykusu, seçici askıya alma | Kopma gibi işlenir; devamda bağlantı kendiliğinden kurulur |
| K12 | Bellek | Bağlama denetimi, `nm`, harita, `-fstack-usage`; 1 saatlik çalışmadan sonra boyalı yığın | Ayırma başvurusu yok; yığın en yüksek kullanımı ≤ %75 |

**PC'de birim testleri** (Unity; B02 7): dönüştürücü (bozuk ve parçalı çerçeve, CRC hatası, yeniden eşleme, Ping360 birleştirme), biriktirici (taşma politikası, sıra), gözetim değerlendiricisi (her koşulun tek başına beslemeyi kesmesi; dış veri yokluğunun kesmemesi), tetik maskesi (duvar saati çağrısı ve yeniden gönderimde sıfır), parametre A/B (yarım yazım, CRC). Bağlantı katmanı sahte `ida_link_ops_t` ile sınanır. Aynı testler kartta da koşabilir (08 §2.6).

## 8. Açık noktalar ve iskelete öneriler

### 8.1 Açık noktalar

1. **Fiziksel çıkış (A13) ve gerçek besleme çökmesi testi (A22)** kullanıcı kararıdır. Bu bölüm fiziksel çıkış olmadığını varsayar; çıkış eklenirse süreli izin, dış kapılama ve reset boyunca elektriksel güvenli seviye ayrıca tasarlanır (D08 E:Watchdog, M19).
2. **LVGL istisnası:** LVGL'e geçilirse iç havuz ayırıcısı bellek ayırma yasağına istisnadır; mimar onayı gerekir.
3. **Donanım ayrıntıları** (**doğrulanmadı** işaretliler): NVIC öncelik bit sayısı, 32 bit zamanlayıcılar, BOR bayrağının güç verme ile ayrımı, GPIO sıfırlama durumu, çift banka okuma-yazma eşzamanlılığı, flash bekleme durumu ve booster dizisi, `MSPLIM` aşımında hata işleyicisinin yığın payı, DSI komut kipi, GFXMMU. Uygulama RM0456, DS13543 ve kart şemasından doğrular.
4. **Bariyerin 400 Hz'te sığması** M1 ve M17'ye bağlıdır; geciken adım politikası (A8) 06'da kesinleşir.
5. **WWDG** (pencereli gözetim) ikinci, daha dar bir koruma olarak eklenebilir; ilk sürümde yok.

### 8.2 İskelete ve 02 numaralı bölüme öneriler

1. **B02 4 ve `alive_ok`:** 02, IMU akışı yokken "`alive_ok = false` uzun sürerse gözetim zamanlayıcısı resetler" diyor ve `alive_ok`'u IMU tazeliğine bağlıyor. Bu, sim çalışmazken açılan kartı sıfırlama döngüsüne sokar ve dış nedeni yazılım arızasıyla karıştırır. Öneri: `alive_ok` çekirdeğin iç sağlığını (NaN yok, durum tutarlı, çağrı tamamlandı) göstersin; IMU yokluğu güvenli durum ve bildirim üretsin, sıfırlama üretmesin. Bu bölüm 2.7'yi buna göre yazdı; 00'ın "kontrol, haberleşme ve veri tazeliği denetimlerinin birleşik sonucu" koşulu, denetimlerin koşması ve iç tıkanma olmaması biçiminde uygulandı.
2. **B02 3.1 `DUP_SEQ`:** "platform önceki çıktıyı yeniden gönderebilir" kuralı tetik alanlarını da yeniden gönderirse simde çift atış olabilir. Öneri: yeniden gönderilen çıktıda `fire[]` sıfırlanır (bu bölüm böyle uygular); 06'dan beklenen: tetik olayının sim tarafında sıra numarasıyla tekilleştirilmesi.
3. **Dönüştürücünün yeri:** iskelet dönüştürücüyü kartta 01'e, PC'de SIL koşucusuna verir; B02 2.8 tek paylaşılan C modülü önerir. Çelişki yok, ama kaynak yeri tanımsız. Öneri: `core/` dışında (çekirdek A1'i bilmemeli), ör. `protocol/conv/`; gereksinimleri 01 ve 02, eşlemeyi 06 yazar.
4. **B02 2.2 md.1:** "zamanı IMU'dan büyük olan örnek sonraki adıma kalır" ifadesinde örneği kimin tuttuğu belirsiz. Bu bölüm, son adımdan beri gelen bütün örnekleri verir ve tutmayı çekirdeğin `ingest`'ine bırakır; 02 bunu açıkça yazmalı.
5. **03'ten beklenen:** `ida_fc_background` sınırlı dilimlerle (≤ 300 µs, tahmin), yeniden başlatılabilir biçimde çağrılabilmeli; aksi halde döngü yapısı FreeRTOS yedeğine itilir (2.4).
6. **İskelet §2 tablosu "Kart yazılımı C17":** HAL, CMSIS ve TinyUSB GNU uzantıları kullandığından platform kodu `gnu17` lehçesiyle derlenir (08 §2.6, 08 §3). Öneri: "çekirdek ISO C17; platform C17'nin GNU lehçesi, uzantılar yalnız `firmware/` içinde".
7. **İskelet §7 depo düzeni:** üçüncü taraf alt modüller için `firmware/third_party/` ve CubeMX referansı için `firmware/cubemx/` eklenmesi önerilir.
8. **İskelet §8 A1 taşıması yedeği** "ST'nin USB yığını" diyor. ST paketindeki USBX çatalı lisansı listelenen donanımla sınırlıdır (08.4); öneri: "upstream Eclipse USBX (MIT)".

İskeletle çelişen bir gereklilik bulunmadı; 8.2'deki maddeler netleştirme önerisidir. Ham rapor 08'in "adım 400 Hz zamanlayıcı kesmesinde" önerisi yerine iskeletin IMU tetiklemeli modeli uygulandı (2.4).
