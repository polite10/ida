# sol ikinci incelemesi: mp1

## Hüküm

**Düzeltme gerekli.** Saat geçişi ve yükleme/yedek korumaları düzeltilmiş; mevcut ham kayıtlar, güncellenmiş ölçütlerle K2’nin “geçti” hükmünü destekliyor. Ancak çözümleyici eksik kanıtı kabul edebiliyor; hata ayıklama betiği de başarısızlığı gizleyebiliyor.

Dosya değiştirilmedi; derleme, kurulum veya kart işlemi yapılmadı. `derleme/` incelenmedi.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | HAL’in gerçek yazma sırası ve 80 MHz üzerinden bekleme hesabı doğru; ayrıntı aşağıda. |
| 2 | giderildi | `yukle.sh` CLI kodunu koruyor; doğrulama satırını da arıyor. K1 derle-yükle ve K2 tek-tur yükleme hatasında duruyor; `REGS` karşılaştırılıyor. |
| 3 | giderildi | İki okumanın başarı ve boyut denetimleri eklendi; yükleme yedek doğrulamasına bağlı. Eski okumaların çıkış kodlarının bulunmadığı açıkça belirtilmiş. Mevcut yedekleri bağımsız doğruladım. |
| 4 | kısmen | Reset, hata, iç sıra boşluğu, sayaç gerilemesi ve kısa pencere reddediliyor. Eksik kayıt/alan denetimleri hâlâ yetersiz; yeni bulgu 1. |
| 5 | giderildi | Bağıl saat farkı ile mutlak belirsizlik ayrılmış. Güncellenmiş K2 ölçütü altında “geçti” denmesi doğru. |
| 6 | giderildi | Ham `PLL1M=3`, gerçek bölücü `M=4` olarak düzeltilmiş. |
| 7 | giderildi | Denenen hata yolu ve kanıt sınırları ayrılmış; UART sayacı kayda eklenmiş. Yeni kayıtlarda iki açılış da mevcut. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| 1 | `tools/olcum/mp1/pc/mp1_seri.cpp:179–184, 242–282` | Eksik kanıt kabul ediliyor: ilk T sırası 10 olabilir; yalnız açılış K/P çifti yeterlidir; bir P’deki eksik `uart_hata`, diğer sıfırların maksimumunda kaybolur. `uart_hata=abc` de `strtoll` ile sıfır olur. Böylece kayıplı/bozuk koşu “GEÇERLİ” çıkabilir. | İlk T=1 olmasını, beklenen 0/60/120… K/P çiftlerini ve her zorunlu alanın eksiksiz sayısal çözümünü denetle; eksik/bozuk ölçüm satırını reddet. | kritik | MP1 çözümleyicisi |
| 2 | `tools/olcum/mp1/k1-hata-ayikla.sh:17–24`; `gdb/k1-iwdg.gdb:25–26`; rapor:47 | GDB hatası `tee` borusunda kayboluyor; kayıt hatası `wait … || true` ile yutuluyor. `IWDG_SONUC SIFIRLAMA_VAR` yalnız yazdırılıyor. Betik başarısız K1 denemesinden 0 dönebilir. Ayrıca `k2-iki-tur.sh`, ilk tur başarısızken ikinciye devam ediyor; rapordaki genel “ilk başarısızlıkta durur” ifadesi doğru değil. | GDB/kayıt kodlarını koru; başarısız IWDG sonucunu başarısız çıkışa bağla. İki-tur davranışıyla raporun ifadesini eşleştir. | kritik | MP1 deneme betikleri ve rapor |
| 3 | `docs/olcumler/MP1-arac-zinciri.md:137`; `tools/olcum/mp1/fw/main.c:155–178` | “Her adımın hatasında işlemci MSIS 4 MHz’te kalır” doğru değil. Örneğin ikinci `ClockConfig` başarısız olduğunda PLL1 zaten seçilmiştir; kod MSIS’e dönüş yapmaz. | Hata sonrasında mevcut saat durumunda ölçümün durdurulduğunu yaz; MSIS’te kalma iddiasını yalnız bunu sağlayan erken aşamalarla sınırla. | önemli | MP1 raporu |

## Yeniden hesaplananlar

**Saat geçişi:** sabitlenen [ST HAL kaynak kodu](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/2552682ee5a0d6f826a6435750fc9267be65868b/Src/stm32u5xx_hal_rcc.c), `RCC_CFGR2.HPRE=0x8` yazımını `RCC_CFGR1.SW=3` seçiminden önce yapıyor. Ardından uygulama bekliyor ve ikinci çağrıyla `HPRE=0` oluyor. HAL ilk çağrı sonunda `SystemCoreClock` değerini **80 MHz** olarak güncelliyor.

[AN5766’nın ST HTML metni](https://www.st.com/content/st_com/en/technical-documents/an5766-migrating-within-stm32u5-series-microcontrollers-stmicroelectronics_asset/chunk-004-topic-an5766-power.html) **3 µs**, [ST’nin 2024-03-08 açıklaması](https://community.st.com/stm32-mcus-products-25/rm0456-10-5-4-unclear-restrictions-for-stm32u59x-5ax-5fx-5gx-devices-129223) **5 µs** istiyor. Görevdeki büyük süreyi kullanma koşuluyla gereken **5 µs = 400 çevrim**; kayıttaki **420 çevrim = 5,25 µs** yeterli.

**PLL:** ham M/N/R alanları 3/79/1 → gerçek 4/80/2; `16/4×80/2 = 160 MHz`. Alan konumlarını [ST CMSIS başlığından](https://raw.githubusercontent.com/STMicroelectronics/cmsis-device-u5/624374fa1e21ca195d6f2102ac0caaa50d0ea4c8/Include/stm32u5a9xx.h) doğruladım.

Yeni ham CSV’lerden bağımsız regresyon:

| Sonuç | SMPS | LDO |
|---|---:|---:|
| T sayısı / sıra | 638 / 1–638 | 638 / 1–638 |
| Pencere | 637,004442 s | 637,002607 s |
| `steady_clock` eğimi | 159.998.933,115 Hz | 159.999.387,341 Hz |
| Nominale göre oran | −6,668030 ppm | −3,829120 ppm |
| Artık RMS | 61,410 µs | 543,476 µs |

İkisinde de 11 K/P çifti, doğru regülatör, sıfır UART hatası ve kesintisiz T sırası var. SMPS `system_clock` oranı **+18,442429 ppm**; LDO’da **+179,759 ms** sıçrama nedeniyle bu saatin eğimini dışlamak doğru.

İki yedek de **4.194.304 bayt**, SHA-256’ları rapordaki `1f13870b…e22457` ile aynı; ≠`0xFF` bayt sayısı **3.907.315**.

Bulgu 1’in somut karşı örneği: SMPS kaydından ilk dokuz T çıkarılınca sıra **10–638**, süre **628,004351 s** kalıyor; mevcut ret koşulları bu kaybı yakalamıyor.

## İskelete ya da diğer bölümlere notlar

- Güncel `main` görevindeki K0/K2 düzeltmeleri esas alındı. Mutlak frekans toleransı şartı yeniden aranmadı; ön tur, M4/M6/M21’in tamamını kapatmıyor.
- Geçici AHB bölücüsü gereği `docs/mimari/01-kart-yazilimi.md` §2.2’ye taşınmalı.
- [ES0553 Rev 6](https://www.st.com/resource/en/errata_sheet/es0553-stm32u59xxx-and-stm32u5axxx-device-errata-stmicroelectronics.pdf) X=`0x3001`, W=`0x3003` listesini doğruladım; `0x3000` listelenmiyor.
- **Erişim sınırı:** RM0456 PDF’i boyut sınırına, AN5766 ve UM2967 PDF’leri zaman aşımına takıldı. RM0456 metnini ve PDF revizyonlarını doğrudan doğrulanmış saymıyorum; AN5766’nın 3 µs dizisi ST HTML üzerinden doğrulandı.