# sol incelemesi: mp3

## Hüküm

**Mevcut kod kabul edilmez.** Baud hesabında yanlış sayısal sonuç, DMA alımında veri bozulmasına açık bir yarış ve kayıt çözümleyicisinde yanlış başarı üreten tutarlılık açıkları var. Depodaki test hedefi de derlenmiyor.

Bu, `docs/gorevler/W0-MP3a.md` ölçütlerine göre **ön incelemedir**. Dosya değiştirmedim, derleme veya test çalıştırmadım, karta erişmedim. Aşağıdaki karşı örnekler, aksi belirtilmedikçe statik kod izleridir; kart üzerinde gözlenmiş sonuçlar değildir. Mevcut derleme günlüklerini ayrıca okudum.

Uzun kampanyalar, kısa kart denemeleri, Windows ve donanım doğrulaması tamamlanmış sayılmadı. Bunlar mevcut kod hatalarından ayrı, bilinen eksiklerdir. İnceleme sırasında beş betik ve ilk teslim raporu eklendi; kaynak sınırı aşağıda kayıtlıdır.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `tools/olcum/mp3/ortak/mp3_cerceve.c:319–335`; `tools/olcum/mp3/test/mp3_test.cpp:177–178` | **OVER8 hesabı, BRR kodlanırken kaybolan düşük biti hesaba katmıyor.** Örneğin 12 Mbaud için `d8=27`, BRR=`0x15`; BRR’den geri çözülen bölücü **26**. Kod `320 MHz/27` bildiriyor; yazdığı ayar `320 MHz/26` veriyor. PC aynı hatalı işlevi kullandığı için karşılaştırma hatayı yakalamıyor. Testler yanlış sonucu sabitliyor. | Önce BRR’yi kodla, ardından yazılabilir BRR’den etkin bölücüyü geri çöz. Kip seçimini, baud ve ppm hesabını bu değerle yap. ST’nin [`LL_USART_GetBaudRate`](https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/2552682ee5a0d6f826a6435750fc9267be65868b/Inc/stm32u5xx_ll_usart.h) uygulaması OVER8 geri dönüşümünü gösteriyor. | kritik |
| 2 | `tools/olcum/mp3/fw/platform.c:242–279` | **BNDT’den bulunan kaynak ilerlemesi, RAM’e tamamlanmış yazım kabul ediliyor.** Karşı örnek: ilk bayt RDR’den DMA FIFO’suna alınmış, `BNDT=16383`, hedef RAM henüz güncellenmemiş olsun. İşlev bir baytı hazır sayar; `memcpy` eski değeri alabilir. İkinci BNDT okuması bunu saptamaz; okuyucu baytı tüketilmiş sayar. PRIMASK, DMA aktarımını durdurmaz. ST’nin DMA sürücüsü de BNDT ile hedefe yazılmış baytları ayırıp alımda FIFO’daki baytları kalan miktara ekliyor. | Hedef belleğe tamamlanmış yazımları esas alan, FIFO ve düğüm geçişleriyle tutarlı bir ilerleme okuması kur. Bu zamanlama ile halka sınırındaki geçişleri bağımsız sınamaya ekle. Dayanak: [ST tarafından yazılan DMA3 sürücüsü, residue hesabı](https://github.com/torvalds/linux/blob/master/drivers/dma/stm32/stm32-dma3.c). Kartta oluşma sıklığı doğrulanmadı. | kritik |
| 3 | `tools/olcum/mp3/test/mp3_test.cpp:246–251,354–356,386–389,435`; `tools/olcum/mp3/derle.sh:27,43–45` | **Test hedefi derlenmiyor; derleme betiği buna rağmen başarıyla bitebiliyor.** `vector<Satir>` kullanıldığında tür eksik; `imzala2` çağrıldığı yerde bildirilmemiş. Mevcut `derle-20261118-185701` günlüğünde her iki derleyici hatası da var. Betik hatayı yalnız `test_derleme=BASARISIZ` yapıp sona devam ediyor. | Tür ve yardımcı bildirimlerini kullanımlarından önce tamamla. Test derleme hatasını betiğin başarısız çıkışına taşı. Eski `mp3_test` ikilisini güncel kaynak için kanıt sayma. | kritik |
| 4 | `tools/olcum/mp3/pc/oturum.cpp:416–460`; `tools/olcum/mp3/pc/kayit.cpp:394–405,435–442` | **Sabit hızlı kampanya, giderek geride kalan göndericiyle de geçebiliyor.** Her yazma istenen aralığın iki katı sürüp timeout’tan kısa kalırsa bütün çerçeveler sonunda gönderiliyor. 600 saniyelik plan yaklaşık 1200 saniyeye yayılıyor; hata sayaçları sıfırsa yalnız alt süre sınırlarına bakan çözümleyici bunu reddetmiyor. Rapordaki trafik hızı hâlâ planlanan aralıktan hesaplanıyor. Akıştaki drain sınırı, gönderim döngüsünün bu uzamasını sınırlamıyor. | Gönderim penceresini ve izin verilen takvim/kuyruk gecikmesini önceden sınırla. Bu sınır aşıldığında profili başarısız say. Gerçekleşen gönderim hızını ayrıca hesapla; planlanan hızı gerçekleşmiş trafik gibi sunma. | kritik |
| 5 | `tools/olcum/mp3/pc/kayit.cpp:435–442,488–508` | **Echo zaman kayıtları, başlık takvimi ve PC özetiyle eşleştirilmiyor.** Geçerli bir echo kaydındaki bütün R zamanlarını `0` yapıp satır CRC’lerini yenile: sıra numaraları ve yanıt sayısı aynı kalır; zaman kontrolleri geçer, RTT’ler sıfır çıkar. `R.plan == sıra × aralik_ns` ve son yazmanın P özetiyle eşitliği denetlenmiyor. | R takvimini başlıktan yeniden üret; yazma/yanıt sayıları ve son zamanları P özetiyle karşılaştır. Tutarsız kaydı `GECERSIZ` yap. Bu karşı örneği negatif teste ekle. | kritik |
| 6 | `tools/olcum/mp3/pc/kayit.cpp:321–340,394–434` | **Bazı zorunlu alanlar yalnız okunuyor, hükümde doğrulanmıyor.** Geçen bir kayıtta `kart_kabul=0`, `os_ioctl=0` veya `kart_brr=1` yapıp CRC’leri yenilemek mevcut başarı koşullarını değiştirmiyor. Benzer şekilde `rx_alinan>0` iken iki uçta da `rx_iyi=0`, `rx_bayt=0` bulunması reddedilmiyor. İki baud alanının eşitliği, BRR hesabının doğruluğunu kanıtlamaz. | Tamamlanmış koşuda hız kabulünü ve OS yapılandırma sonucunu zorunlu kıl. Baud/ppm değerlerini kayıttaki saat, BRR ve OVER8’den yeniden hesapla. Veri ve kontrol çerçevelerini açıkça muhasebeleştirerek alım bayt/çerçeve sayaçlarının tutarlılığını denetle. | kritik |
| 7 | `tools/olcum/mp3/pc/oturum.cpp:425–434` | **Echo bekleyen sayısı, atlanan sıra numaralarını da içeriyor.** Sınır 2 olsun: 0 ve 1 gönderilsin, 2 atlansın; yanıt 0’dan sonra 3 gönderilsin. `baslatilan=4`, alınan=1 olduğundan sayaç 3 gösterir; gerçekte iki istek bekler. Bu yapay doluluk yeni atlamalar üretir. 2 ve 4 atlandıktan, gönderilen 0/1/3/5’in tüm yanıtları geldikten sonra bile `6−4=2` kalabilir; boş kuyruk dolu görünür. | Sıra üst sınırını gönderilen istek sayısından ayır. Bekleyenleri gerçekten başlatılmış isteklerle tut; yanıt ve timeout muhasebesini açık uygula. Verilen diziyi kuyruk testi yap. | kritik |
| 8 | `tools/olcum/mp3/pc/seri_posix.cpp:85–100` | **Yazma deadline’ı, ilerleme olan dallarda denetlenmiyor.** `write` her seferinde pozitif kısmi sonuç döndürürse satır 91 sürekli `continue` yapıyor. Toplam süre sınırı aşılmış olsa da kalan baytlar yazılıp `Tamam` dönebiliyor. Yeniden üretim dizisi: 276 kez bir baytlık başarılı yazma, çağrılar arasında toplam süre timeout’tan uzun. Kısmi başarılı yazma API’nin izin verdiği davranıştır. | Deadline’ı her yinelemede, yeni yazma denemesinden önce denetle; timeout’ta gerçek kısmi bayt sayısını koru. Dayanak: [Apple `write(2)`](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/write.2.html). | kritik |
| 9 | `tools/olcum/mp3/pc/mp3_vcp.cpp:117–124`; `tools/olcum/mp3/pc/oturum.cpp:327–334`; `tools/olcum/mp3/pc/kayit.cpp:399` | **Dondurulmuş %80 sınırı, trafik seçeneğiyle birlikte değişiyor.** CLI `--oran 0.9` hem trafik oranını hem kabul sınırını %90 yapıyor; böyle bir kayıt `GECTI` olabilir. Kullanılan kabul sınırı kayda da yazılmıyor; yeniden çözümleme hükmü dışarıdan verilen değere bağlı. Yazar raporu da bu tutarsızlığı bildiriyor. | Deneysel trafik ayarını dondurulmuş kampanya kabulünden ayır. Kampanya değerlendirmesinde %80 üst sınırını koru; kullanılan değerlendirme profilini kaydet. | önemli |
| 10 | `tools/olcum/mp3/kos.sh:58`; `tools/olcum/mp3/pc/mp3_vcp.cpp:175` | **Ön taramanın süreç hatası başarı koduna dönüşebiliyor.** Sarmalayıcıda `$?`, `tee` sonucudur: araç 4, `tee` 0 dönerse kayıtlı çıkış kodu 0 olur. Ayrıca `tara`, bütün alt koşular başarısız olsa da koşulsuz 0 döner. Alt koşu sonuçları metinde kalsa da süreç sonucu başarıyı ayırt etmiyor. | Araç çıkış kodunu kaybetmeden yakala. Tarama tamamlanması ile adayların geçmesini ayrı raporla; kullanım/çalıştırma hatasını sıfıra dönüştürme. | önemli |
| 11 | `tools/olcum/mp3/pc/mp3_vcp.cpp:113–121` | **Yük boyutu daraltıldıktan sonra sınanıyor.** `--yuk 65536`, `uint16_t` dönüşümünde 0 olur ve üst sınır kontrolünü geçer. İstenen boyut sessizce başka bir boyuta dönüşür. | Sayıyı geniş türde, tam metin ve taşma denetimiyle çöz; 0–256 sınırını doğruladıktan sonra daralt. Diğer sayısal seçeneklerde de geçersiz ve sonlu olmayan değerleri reddet. | önemli |

## Yeniden hesaplananlar

**Baud:** Nominal USART saati 160 MHz, prescaler 1 için mevcut kodun seçtiği ayarlar:

| İstenen baud | OVER8 | Yazılan BRR | Kodun bildirdiği baud | BRR’den hesaplanan baud | İstenene göre hata |
|---:|---:|---:|---:|---:|---:|
| 921.600 | 1 | `0x155` | 922.190 | 924.855,491 | +3.532,434 ppm |
| 2.000.000 | 0 | `0x50` | 2.000.000 | 2.000.000 | 0 ppm |
| 3.000.000 | 1 | `0x65` | 2.990.654 | 3.018.867,925 | +6.289,308 ppm |
| 6.000.000 | 1 | `0x32` | 6.037.736 | 6.153.846,154 | +25.641,026 ppm |
| 12.000.000 | 1 | `0x15` | 11.851.852 | 12.307.692,308 | +25.641,026 ppm |

OVER8 için etkin bölücü:

`D = (BRR & 0xFFF0) | ((BRR & 7) << 1)`

`baud = 2 × usart_hz / D`

Bunlar **nominal saatten hesaplanan değerlerdir; fiziksel baud ölçümü değildir**. Formül ST’nin yukarıda bağlantılanan LL geri okuma koduyla denetlendi.

**Çerçeve ve kapasite:**

- Ek yük `16 + 4 = 20 B`; 64 B yükte 84 B, 256 B yükte 276 B.
- 921.600 baud, 8N1: yön başına `92.160 B/s`.
- Echo profillerinin yön başına tel yükleri:

| Yük | İstek/s | B/s | 921.600 baud kapasitesi |
|---:|---:|---:|---:|
| 64 B | 200 | 16.800 | %18,229 |
| 64 B | 400 | 33.600 | %36,458 |
| 256 B | 200 | 55.200 | %59,896 |
| 256 B | 400 | 110.400 | **%119,792 — sığmaz** |

2 Mbaud, 256 B yük ve %80 için aralık **1.725.000 ns**; 600 saniyeyi kapsayan plan **347.827 çerçeve**, yön başına **96.000.252 veri çerçevesi tel baytı** üretir. Plan süresi 600,001575 saniyedir. Yazmalar iki kat aralıkla gerçekleşirse aynı miktar yaklaşık 1200 saniyeye yayılır; gerçekleşen ortalama yaklaşık %40 kapasite olur. Bulgu 4’teki kabul açığı budur.

**Tampon ve sayaçlar:** 16.384 B halka, nominal 12 Mbaud’da 13,653333 ms; yarım halka 6,826667 ms veri tutar. 32 bit 1 MHz sayaç 4.294,967296 saniyede sarar; 600 saniyelik tek koşu bu sınırın altındadır.

**CRC ve yüzdelikler:** Bağımsız hesapta `"123456789"` için CRC-16 `0x29B1`, CRC-32 `0xCBF43926` çıktı. En yakın sıra yöntemiyle 240.000 örnekte p50/p99/p99,9 sıraları 120.000 / 237.600 / 239.760; yüzdelik formülü bu kullanımda doğru.

## İskelete ya da diğer bölümlere notlar

- K17 ve görev belgesi esas alındı. İskelet ve 01/09’daki eski “birincil USB HS, yedek VCP” ifadeleri bu prototip için gereklilik sayılmadı. Kontrol frekansı, bariyer bütçesi ve ürün protokolü hakkında karar çıkarılmadı. Dondurulmuş sözleşme dizini mevcut değildi.
- USART1 PA9/PA10 AF7 eşlemesi [resmî BSP](https://github.com/STMicroelectronics/stm32u5x9j-dk-bsp/blob/main/stm32u5x9j_discovery.h), RX/TX DMA istekleri 24/25 [resmî HAL başlığı](https://github.com/STMicroelectronics/stm32u5xx-hal-driver/blob/main/Inc/stm32u5xx_hal_dma.h) üzerinden canlı denetlendi. Apple `IOSSIOSPEED` tanımı [resmî Apple kaynağında](https://github.com/apple-oss-distributions/IOSerialFamily/blob/main/IOSerialFamily.kmodproj/ioss.h) doğrulandı.
- RM0456 PDF’nin tam metnine araç boyut sınırı nedeniyle erişilemedi. Open Group `write` sayfası 403 verdi; denenen AN5593 bağlı liste HTML adresi de açılamadı. Bu kaynaklar doğrulanmış sayılmadı; ilgili değerlendirmelerde erişilebilen birincil kaynaklar kullanıldı.
- Sonradan eklenen `docs/olcumler/MP3-vcp.md`, kart ölçümü yapılmadığını ve test kaynağının derlenmediğini açıkça bildiriyor. Bu dürüst sınırlamalar kod bulgularını kapatmıyor. Betiklerin ve raporun başlangıçta bulunmaması ayrıca kusur yazılmadı; uzun koşular beklemede.

**Kaynak sınırı:** `tools/olcum/mp3/derleme/` hariç dosyalar için SHA-256 manifesti alındı. Toplu özet, sıralanmış `göreli_yol + boşluk + dosya_SHA256 + LF` kayıtlarının SHA-256’sıdır.

| An | Dosya sayısı | Toplu SHA-256 |
|---|---:|---|
| Başlangıç, 2026-11-18 15:55:04 UTC | 19 | `41275ac3d224e84772e80862da905eddf75572f20691e3fe0a6562b8939def6f` |
| Son, 2026-11-18 16:03:16 UTC | 24 | `2d091c904729c3a0750e85987dd76a690ef754d51a71d17212c5d348fb6da2d3` |

Başlangıçtaki 19 dosyanın hiçbiri değişmedi. `derle.sh`, `kampanya.sh`, `kos.sh`, `onceki-imaj.sh`, `yukle.sh` eklendi; bunların yalnız eklenme sonrası içerikleri okundu, çalıştırılmadı. Hüküm bu kaynak sınırı içindir.