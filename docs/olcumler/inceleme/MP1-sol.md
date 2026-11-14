# sol incelemesi: mp1

## Hüküm

**Düzeltme gerekli; kritik bulgular kapanmadan kabul edilmemeli.** Özellikle 160 MHz’e geçiş dizisi eksik, betikler yükleme ve yedekleme hatalarını güvenilir biçimde durdurmuyor.

Mevcut ham CSV’lerden rapordaki DWT/PC eğimlerini yeniden ürettim. İki flash yedeğinin boyutu ve SHA-256’sı da raporla uyuşuyor. IWDG dondurma denemesi, karşı denemesiyle birlikte sonucu destekliyor. Betiklerde seçenek baytı yazma, harici flash programlama veya açık toplu silme komutu bulmadım.

K2’nin **“geçmedi”** bırakılması doğru; ölçülen oran mutlak frekans doğruluğunu kanıtlamıyor. İncelemede dosya değiştirilmedi, derleme veya kart işlemi çalıştırılmadı; `derleme/` dışlandı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `tools/olcum/mp1/fw/main.c:136–145`; `docs/olcumler/MP1-arac-zinciri.md:117–126` | Range 1’de MSIS 4 MHz’ten PLL1 160 MHz’e AHB ÷1 ile doğrudan geçiliyor. Kullanılan HAL da geçici ÷2 ve beklemeyi eklemiyor. ST örneğiyle aynı olması, dizinin donanım gereğini karşıladığını göstermiyor. | STM32U59x/5Ax için ST’nin istediği geçişi uygula: AHB ÷2, PLL1’e geçiş, belgelenmiş bekleme, AHB ÷1. [AN5766](https://www.st.com/content/st_com/en/technical-documents/an5766-migrating-within-stm32u5-series-microcontrollers-stmicroelectronics_asset/chunk-004-topic-an5766-power.html) 3 µs belirtir; [RM0456 açıklamasına verilen ST yanıtı](https://community.st.com/stm32-mcus-products-25/rm0456-10-5-4-unclear-restrictions-for-stm32u59x-5ax-5fx-5gx-devices-129223) 5 µs belirtir. Kullanılan belge revizyonunu kaydet; düzeltilmiş diziyle iki turu yenile. | kritik |
| 2 | `tools/olcum/mp1/yukle.sh:11–12`; `k1-derle-yukle.sh:19`; `k2-tur.sh:18` | CLI hatası boru hattında kayboluyor; sondaki `|| true` başarısız yüklemeyi de başarıyla bitiriyor. K2 eski imajla devam edebilir. Çözümleyici istenen regülatörle `P` satırlarındaki `REGS` değerini karşılaştırmıyor. | CLI çıkış kodunu filtrelemeden önce koru; yükleme/doğrulama başarısızsa dur. Kayıtta istenen varyantı ve gerçek `REGS` değerini doğrula. Mevcut kayıtlarda doğru varyantlar görülüyor; sorun hata durumundaki davranışta. | kritik |
| 3 | `tools/olcum/mp1/yedek-al.sh:19–29`; `tools/olcum/mp1/yukle.sh` | Yedek okumalarının CLI çıkış kodları kayboluyor. İki dosyanın eşitliği denetleniyor fakat her birinin 4.194.304 bayt olduğu denetlenmiyor; aynı eksik içerik de eşit SHA verir. İlk yükleme girişinde geçerli yedek önkoşulu da yok. Bu, görevin “okuma başarısızsa yükleme yapma” korumasını sağlamıyor. | İki okumanın başarılı olduğunu, tam boyutu ve özet eşitliğini ayrı ayrı doğrula. İlk yükleme akışını bu doğrulanmış yedeğe bağla; başarısız okumadan kalan dosyaları geçerli yedek sayma. Mevcut yedeklerin tam ve eşit olduğunu bağımsız doğruladım. | kritik |
| 4 | `tools/olcum/mp1/pc/mp1_seri.cpp:134–166, 213–258` | Ölçüm penceresinde reset/hata/sıra boşluğu bulununca yalnız uyarı basılıyor; eğim hesaplanıp çıkış kodu 0 dönüyor. Reset sonrası `p.cyc - c0` işaretsiz çıkarımı 2⁶⁴ üzerinden sararak anlamsız sayısal sonuç üretebilir. Bütün noktaların aynı zaman damgasında olması da `sxx=0` üretir. | Geçersiz koşuyu eğim hesaplamadan reddet ve başarısız dön. Sayaç ve zaman monotonluğunu, pozitif pencere süresini, gerekli ≥600 s süreyi ve sonucun sonlu olduğunu denetle. Yeni açılış ayrı ölçüm penceresi olacaksa açıkça ayır. | kritik |
| 5 | `docs/olcumler/MP1-arac-zinciri.md:165, 172, 206` | İki PC saatinin yaklaşık 18 ppm farklı eğim vermesi, kullanılan referansın belirsizliğinin **en az 18 ppm** olduğunu göstermez. Örneğin ham saat doğru, sistem saati 18 ppm hızlı olabilir. Ölçülen fark, mutlak hata sınırı değildir. | “İki saat arasında ≈18 ppm bağıl hız farkı ölçüldü; mutlak referans hatası ve belirsizlik sınırı belirlenmedi” yaz. K2’nin geçmedi hükmünü koru. | kritik |
| 6 | `docs/olcumler/MP1-arac-zinciri.md:133` | `PLL1M = 0 (÷4)` çözümü yanlış. Ham alan 3’tür; bölücü 3+1=4’tür. Kod bunu doğru çözüyor. | `PLL1M[11:8] = 0x3 → M=4` olarak düzelt. Alan 0 olsaydı ÷1 olurdu. Alan konumları [ST CMSIS başlığında](https://raw.githubusercontent.com/STMicroelectronics/cmsis-device-u5/624374fa1e21ca195d6f2102ac0caaa50d0ea4c8/Include/stm32u5a9xx.h) tanımlı. | önemli |
| 7 | `docs/olcumler/MP1-arac-zinciri.md:126, 128, 170–171, 189–197`; `tools/olcum/mp1/fw/main.c:70` | Kanıt kapsamı olduğundan geniş anlatılıyor. LDO CSV’sindeki ilk `K/P` çifti sıra 60’tır; açılış geri okuması kayıtta yoktur. Gerçekleştirilen saat hatası denemesi adım 6’dır; HSE açılmaması ve PLL kilitlenmemesi denenmemiştir. Ayrıca UART hata sayacı artırılıyor ama raporlanmıyor; genel “hatasız” ifadesi bu sayacı kapsamıyor. | Açılışta kodun yaptığı işlemlerle kayıtta gözlenenleri ayır. Bu sınırları “Doğrulanmayanlar”a ekle; “hatasız” sonucunu gözlenen reset, saat hatası ve sıra sürekliliği kontrolleriyle sınırlandır veya UART hata sayacını da kaydet. | önemli |

## Yeniden hesaplananlar

**PLL1 ve SYSCLK**

`PLL1CFGR=0x00040313` çözümü:

- `PLL1SRC=3`: HSE.
- `PLL1RGE=0`, `PLL1FRACEN=1`.
- `PLL1M=(değer >> 8) & 0xF = 3`; dolayısıyla **M=4**.
- `PLL1MBOOST=0`: ÷1.
- `PLL1REN=1`; P ve Q çıkışları kapalı.

`PLL1DIVR=0x01010E4F` için ham N/P/Q/R alanları sırasıyla 79/7/1/1; gerçek değerler **80/8/2/2**. `FRACR=0` ile:

`fREF=16/4=4 MHz`, `fVCO=4×80=320 MHz`, **`fSYSCLK=320/2=160 MHz`**.

`CFGR1=0xF` içinde `SWS=3`, yani PLL1 seçilmiş. Bunlar nominal HSE değerine dayalı hesaplamalardır; HSE’nin mutlak frekans ölçümü değildir. Alan çözümü [ST CMSIS tanımlarıyla](https://raw.githubusercontent.com/STMicroelectronics/cmsis-device-u5/624374fa1e21ca195d6f2102ac0caaa50d0ea4c8/Include/stm32u5a9xx.h) uyumludur.

**Seçenek baytları ve kimlik**

`FLASH_OPTR=0x7BEFF8AA` için:

| Alan | Yeniden çıkarılan değer |
|---|---|
| `RDP[7:0]` | `0xAA` |
| `BOR_LEV[10:8]` | 0 |
| `SRAM_RST`, bit 15 | 1 |
| `IWDG_SW`, bit 16 | 1 |
| `SRAM2_RST`, bit 25 | 1 |
| `TZEN`, bit 31 | 0 |

Bit değerleri ham CLI kaydıyla uyuşuyor. SRAM silmeme ve ECC kapalı yorumları da CLI dökümüyle tutarlı; ancak seçenek baytlarının okunması, uygulamadaki `.noinit` kalıcılığının çalıştırılarak sınanması değildir.

`DBGMCU_IDCODE=0x30006481` için **DEV_ID=`0x481`, REV_ID=`0x3000`**. Erişebildiğim [ES0553 Rev 6](https://www.st.com/resource/en/errata_sheet/es0553-stm32u59xxx-and-stm32u5axxx-device-errata-stmicroelectronics.pdf) X=`0x3001`, W=`0x3003` listeliyor; `0x3000` listede yok. Rapordaki doğrulanmamış W=`0x3002` aktarımı güncel birincil kaynakla değiştirilmelidir; C revizyonuna bu errata tablosu doğrudan uygulanmış sayılamaz.

**DWT/PC hesabı**

CSV’leri bağımsız okuyup aynı pencere seçimini ve merkezlenmiş en küçük kareler hesabını uyguladım:

| Sonuç | SMPS | LDO |
|---|---:|---:|
| Atılan ilk T satırı | 80 | 1 |
| Tutulan T satırı | 629 | 629 |
| Sıra aralığı | 6–634 | 5–633 |
| PC penceresi | 628,003606 s | 628,003804 s |
| Eğim | 159.999.170,9858 Hz | 159.999.090,2243 Hz |
| `(eğim/160.000.000−1)×10⁶` | **−5,181339 ppm** | **−5,686098 ppm** |
| Artık RMS | 61,03 µs | 63,63 µs |

Sonuçlar raporla uyuşuyor. SMPS açılış satırı tutulan ilk T’den yaklaşık 1 s önce; mevcut veride pencere dışında bırakılması doğru. Tutulan iki pencerede sayaçlar monoton ve sıra kesintisiz.

LDO’da sıra 600 civarında iki saat arasındaki fark **−91,111084 ms** sıçrıyor; bütün tur için `system_clock` eğiminin geçersiz sayılması doğru. LLVM’nin Apple uygulaması `steady_clock` için `CLOCK_MONOTONIC_RAW` kullanıyor; bu, kalibrasyon kanıtı değildir. [LLVM kaynak kodu](https://github.com/llvm/llvm-project/blob/main/libcxx/src/chrono.cpp)

32 bit DWT taşma süresi **26,8435456 s**. Ana döngüdeki modüler farkla 64 bite genişletme, bu süreden kısa aralıklarla çağrıldığı mevcut koşuda doğru. 64 bit toplamın taşması yaklaşık **3653 yıl** sonra olur; bu turlarda sorun değildir.

**IWDG kanıtı**

Nominal süre `32×(999+1)/32.000 = 1 s`. Ham GDB kaydı 10 saniyelik beklemeyi gösteriyor; T47–T48 varış aralığı **11,450304 s**, sıra devam ediyor ve arada açılış yok. Kesme noktası atamadan önce olduğundan gerçek `mp1_sayac` 46→47, fonksiyon argümanı 47→48 ilerliyor.

Karşı denemede dondurma biti temizleniyor ve yeni açılışta `iwdgrst=1` görülüyor. Dolayısıyla kanıt, watchdog’un etkin olduğunu ve dondurmanın çalıştığını destekliyor; gerçek LSI frekansını ölçmüyor.

**Flash güvenliği ve yedek**

İki yedek de **4.194.304 bayt**; ikisinin SHA-256’sı:

`1f13870b132a61d34f3649c96e9a789914f692dd45081f10dcd80250e0e22457`

`0xFF` olmayan bayt sayısı **3.907.315**; raporla aynı.

Betiklerde `-ob displ` dışında seçenek baytı komutu, `-e all`, koruma kaldırma veya harici loader seçimi yok. Mevcut yükleme logları yalnız dahili sektör **0–2** silindiğini gösteriyor: **3×8192=24.576 bayt**. GDB’nin yazdığı adres DBGMCU dondurma yazmacı; seçenek baytı veya flash değil.

## İskelete ya da diğer bölümlere notlar

- `docs/mimari/01-kart-yazilimi.md` §2.2’ye, bulgu 1’deki U59x/5Ax saat geçişi gereği taşınmalı. Range 1, booster’ın HAL tarafından yönetilmesi ve flash gecikmesinin yükseltmeden önce ayarlanması kaynak koduyla destekleniyor; bunlar geçici AHB adımının yerini tutmuyor. [ST HAL RCC](https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/2552682ee5a0d6f826a6435750fc9267be65868b/Src/stm32u5xx_hal_rcc.c), [ST DK örneği](https://github.com/STMicroelectronics/STM32CubeU5/blob/d88042df24f16799957c24f00c2b234c9e306188/Projects/STM32U5x9J-DK/Applications/USBX/Ux_Host_CDC_ACM/Core/Src/main.c)
- Rapordaki “4–8 MHz yalnız ikincil kaynakta görüldü” ifadesi eksik: sabitlenen HAL’in `stm32u5xx_hal_rcc.h` dosyasında `__HAL_RCC_PLL_VCIRANGE` açıklaması bunu açıkça yazıyor. Bu, RM0456 tablosunun ayrıca doğrulandığı anlamına gelmez.
- **Erişim sınırı:** RM0456 tam PDF’i araç boyut sınırı nedeniyle okunamadı; booster giriş aralığı ve flash gecikme tablosunu RM0456’dan doğrulanmış saymıyorum. DS13543 açıldı ancak ilgili tabloya sonraki erişimler zaman aşımına uğradı. Kart şeması da boyut sınırına takıldı. [UM2967 Rev 3](https://www.stmcu.jp/wp/wp-content/uploads/2023/02/UM2967_Rev3.pdf) ve ST’nin indekslenen UM2967 içeriği 16 MHz kristali destekliyor; parça numarası ve toleransı doğrulanamadı.
- Ön turun 10 dakika olması, Windows ve USB HS yükünün sonraya bırakılması görev belgesiyle uyumlu. Bu sonuçlarla M4/M6/M21’in bütünü kapanmış veya ürün saat yapılandırması kesinleşmiş sayılmamalı.