# sol ikinci incelemesi: mp3

## Hüküm

**Mevcut hedef kabul edilmez.** İlk düzeltmelerin çoğu uygulanmış; ancak kayıt çözümleyicisinde üç ayrı yanlış `GECTI` karşı örneğini çalıştırarak doğruladım. DMA düğüm geçişinin kapanış kanıtında da açık var.

Hedefteki 27 kaynak/betik dosyasını okudum. Ortak C, PC ve test kaynakları dosya üretmeyen sözdizimi denetiminden geçti. Mevcut 163/163 test günlüğünü okudum; dosya oluşturan test takımını yeniden çalıştırmadım. Mevcut çözümleyiciyle on kısa tarama kaydını yeniden değerlendirdim: dokuzu `GECTI`, 12 Mbaud akış `GECMEDI`.

Dosya değiştirmedim, kurulum yapmadım, karta erişmedim ve uzun koşu başlatmadım.

## İlk incelemedeki maddelerin durumu

| # | Durum | Not |
|---|---|---|
| 1 | giderildi | Adaylar BRR’ye kodlandıktan sonra geri çözülüyor. Beş tarama baud’unun BRR, kip, hız ve ppm sonuçlarını yeniden hesapladım; güncel beklentiler doğru. |
| 2 | kısmen | FIFO’daki ilk bayt artık hazır sayılmıyor. Aynı blok içinde BNDT→FIFOL sırası muhafazakâr alt sınır veriyor. TC hizmeti ile düğüm yeniden yüklemesinin ayrıldığı durum modelde yok; Y1. |
| 3 | giderildi | Eksik tür/bildirim hataları kalkmış. Güncel kaynakların sözdizimi denetimi geçti. `derle.sh` derleme ve test başarısızlığında sıfır dışı çıkıyor. Bu tur temiz derleme/link yapılmadı. |
| 4 | kısmen | Yavaş göndericiye karşı 10 ms ve pencere sınırları eklenmiş; normal büyüklükteki aşım reddediliyor. Alan aralıkları ve akış zamanlarının iç tutarlılığı eksik; Y2–Y3. |
| 5 | giderildi | R planları başlıktan üretiliyor; gönderim/yanıt sayıları ve azami/son zamanlar P ile karşılaştırılıyor. İlk karşı örnekteki bütün R zamanlarını sıfırlama artık `GECERSIZ`. Ek kuyruk tutarlılığı açığı Y4’te. |
| 6 | kısmen | Kabul, OS sonucu, BRR/ppm ve veri/denetim muhasebesi gerçekten denetleniyor. İlk karşı örneklerdeki `kart_kabul=0`, `os_ioctl=0`, `kart_brr=1` reddedildi. Sayısal alanların temsil sınırları eksik; Y2. |
| 7 | giderildi | Atlanan sıra kuyruğa girmiyor; yanıt ve timeout öğeyi bir kez çıkarıyor. İlk dizideki yapay doluluk giderilmiş. Yeni azami doluluk ölçümü ve kayıt denetimi sorunu Y4’te. |
| 8 | giderildi | Pozitif kısmi ilerlemeden sonraki her yeni yazma denemesinden önce deadline denetleniyor; kısmi bayt sayısı korunuyor. İlgili negatif test kaynakta mevcut. |
| 9 | giderildi | Akış sınırı sabit `%80`; echo sınırı ayrı `%100`. Profil kaydedilip yeniden üretiliyor. `--oran 0.9` kullanım hatasıyla, port açılmadan reddedildi. |
| 10 | giderildi | `kos.sh` araç çıkışını boru hattısız koruyor. `tara()` alt sonuçlardan başarısızlık üretiyor. Mevcut gerçek taramanın çıkış kodu 1, yeniden çözümlemeyle uyumlu. |
| 11 | giderildi | Geniş türde sınama daraltmadan önce yapılıyor. `65536` yük, `nan` süre ve sonuna karakter eklenmiş baud denemeleri çıkış 4 ile reddedildi. |

## Yeni bulgular

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem | Kapatılacağı yer |
|---|---|---|---|---|---|
| Y1 | `tools/olcum/mp3/ortak/mp3_halka.c:7`; `tools/olcum/mp3/fw/platform.c:234`; `tools/olcum/mp3/test/mp3_test.cpp:123` | **TC sayımı ile BNDT yeniden yüklemesinin aynı dönemi temsil ettiği varsayılıyor.** TC hizmet edilmiş, yeniden yükleme henüz bitmemiş durumda `(yarim=2, BNDT=0, FIFOL=0, N=16384)` işlev `alt=ust=32768` hesaplıyor; tamamlanan gerçek veri 16384 B. Sonraki yeniden yüklemede `ust=16384` olurken tekdüzelik zorlaması `alt=32768` değerini koruyor. Test TC ve yeniden yüklemeyi tek adımda yaptığı için bu durumu dışlıyor. Kartta bu ara durumun gözlenebilirliği doğrulanmadı; mevcut kanıt bunu dışlamıyor. | TC olayı, yazmaç yeniden yüklemesi ve okuyucu konumunu tutarlı bir dönemle ilişkilendir. Modele TC bayrağı, kesme hizmeti ve LLI yazmaç güncellemelerini ayrı adımlar olarak ekle. Ara durumun donanımda okunamayacağına dayanılacaksa bunu U5’e özgü birincil kaynakla kanıtla. | kritik | MP3 DMA alımı ve halka testi |
| Y2 | `tools/olcum/mp3/pc/kayit.cpp:358`; `tools/olcum/mp3/pc/kayit.cpp:583` | **İşaretsiz kayıt değerleri sınanmadan işaretli zamana çevriliyor.** Temiz 2 Mbaud akış kaydında yalnız `P.tx_gec_azami_ns=18446744073709551615` yapıp CRC’yi yenilemek, `--kampanya --en-az-sn 10` altında hâlâ `GECTI`. Dönüşüm bu ortamda −1 oluyor ve 10 ms denetimini atlıyor. Aynı değer `P.t_son_yaz_ns`, `K.tx_gec_azami_us` ve `K.tx_son_us` alanlarında da yanlış başarı verdi. | P zamanlarını dönüşümden önce `INT64_MAX` ve işlem sınırlarıyla; K alanlarını tel biçimindeki `uint32_t` sınırıyla doğrula. Çarpım/toplamları taşma denetimiyle yap. Temsil edilemeyen kayıt `GECERSIZ` olmalı. | kritik | MP3 kayıt çözümleyicisi ve negatif testler |
| Y3 | `tools/olcum/mp3/pc/kayit.cpp:482`; `tools/olcum/mp3/pc/kayit.cpp:583` | **Akışın son zamanları kendi takvimiyle karşılaştırılmıyor.** Temiz 2 Mbaud, 5798 çerçevelik kayıtta yalnız `K.tx_son_us=0` değişikliği, `--kampanya --en-az-sn 10` ile `GECTI`. Son çerçeve yaklaşık 9,999825 s için planlıyken tamamının t0’da halkaya konduğu iddiası kabul ediliyor. P/K son gönderim zamanlarını birlikte sıfırlamak da süre beklentisi verilmediğinde geçiyor ve gerçekleşen hızları 0 B/s gösteriyor. | PC son yazmasının son planından önce olmadığını doğrula. Kart son gönderimini kendi son planı, azami gecikmesi ve koşu süresiyle karşılaştır; µs yuvarlamasını açık ele al. İmkânsız kronoloji `GECERSIZ` olmalı; yalnız üst sınır sınaması yeterli değil. | kritik | MP3 akış takvimi doğrulaması |
| Y4 | `tools/olcum/mp3/pc/oturum.cpp:483`; `tools/olcum/mp3/pc/oturum.cpp:502`; `tools/olcum/mp3/pc/kayit.cpp:543` | **Azami bekleyen sayısı eksik ölçülüyor ve R’den denetlenmiyor.** Ölçüm eklemeden önce, yanıtlananlar çıkarıldıktan sonra yapılıyor. Beş gerçek echo kaydı da azami 0 bildiriyor; yazma sonu–yanıt aralıklarında bekleyen istekler var. Daha güçlü karşı örnek: temiz 2 Mbaud echo kaydında her R yanıtını `max(eski_yanit, 500000000 + sira×1000000)` yapmak, P/K’yi değiştirmeden `GECTI` veriyor. Zaman çizelgesinde timeout’a uğramamış **100** istek aynı anda bekliyor; sınır 64. | Kuyruk ölçümünün hangi anı temsil ettiğini tanımla ve ekleme sonrası doluluğu da kaydet. Çözümleyicide R yazma/yanıt/timeout olaylarından bekleyenleri yeniden üret; sınırı ve bildirilen muhasebeyi doğrula. | kritik | MP3 echo kuyruk muhasebesi ve kayıt negatif testleri |

Y1’in dayanağı: yapılandırılan `TCEM=0`, ST’nin U5 LL başlığında blok sonu olayıdır; LLI sonu ayrı kiptir. Bunları tek atomik olay sayan test yeterli kapanış kanıtı değildir. [ST U5 LL DMA](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/main/Inc/stm32u5xx_ll_dma.h). Ayrıca dayanak gösterilen ST yazımı Linux sürücüsü, güvenilir residue görüntüsü için kanalı askıya alıyor; yalnız FIFO çıkarma formülü aktif yazmaç görüntüsünün tutarlılığını kanıtlamıyor. [ST DMA3 sürücüsü](https://raw.githubusercontent.com/torvalds/linux/master/drivers/dma/stm32/stm32-dma3.c).

## Yeniden hesaplananlar

**Baud geri hesabı:** OVER16’da `f/BRR`; OVER8’de `D=(BRR & 0xFFF0) | ((BRR & 7)<<1)`, hız `2f/D`. Bit çözümü resmî LL uygulamasıyla doğrulandı. Prototip sonucu en yakın tam baud’a yuvarlıyor; bağlantılanan LL uygulaması tam sayı bölmesi kullanıyor. [ST U5 LL USART](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/2552682ee5a0d6f826a6435750fc9267be65868b/Inc/stm32u5xx_ll_usart.h).

| İstenen baud | Kip | BRR | Nominal saatten hesaplanan baud | Kodun tam baud / ppm değeri |
|---:|---|---:|---:|---:|
| 921600 | OVER16 | 174 | 919540,230 | 919540 / −2235 |
| 2000000 | OVER16 | 80 | 2000000 | 2000000 / 0 |
| 3000000 | OVER16 | 53 | 3018867,925 | 3018868 / +6289 |
| 6000000 | OVER16 | 27 | 5925925,926 | 5925926 / −12345 |
| 12000000 | OVER8 | `0x15` | 12307692,308 | 12307692 / +25641 |

Bunlar fiziksel baud ölçümü değildir. Ppm sütunu kodun yuvarlanmış tam baud üzerinden hesabıdır.

**Kapasite ve takvim:**

- Çerçeve ek yükü 20 B; 64/256 B yükler için toplam 84/276 B.
- 921600 baud’da yön başına nominal 8N1 kapasitesi 92160 B/s.
- Echo yükleri: 64 B × 200/400 Hz → 16800/33600 B/s; 256 B × 200/400 Hz → 55200/110400 B/s. Son profil kapasitenin `%119,792`’sidir; koşturulmadan reddedilmesi doğru.
- 2 Mbaud, 256 B, `%80`: aralık **1725000 ns**; 600 s için **347827 çerçeve**, yön başına **96000252 B**, plan penceresi **600,001575 s**.
- Aynı hızın kısa kaydında 5798 çerçeve için son plan **9,999825 s**, pencere **10,00155 s**, bitiş payı **100,0155 ms**. Y3’teki sıfır kart zamanı bu takvimle uyuşmuyor.

**DMA:** Aynı dönem içinde `S(t₁)−F(t₂) ≤ W(t₂)` olduğundan BNDT’nin FIFOL’dan önce okunması muhafazakâr alt sınır fikrini destekliyor. İlk bayt FIFO’dayken `(0,16383,1)` sonucu `alt=0, ust=1`; ilk hata bu durumda giderilmiş. HT sınırının iki yanında konum 8192 kalıyor. Y1’deki dönem uyuşmazlığı ise bağımsız hesapta konumu 16384’ten 32768’e sıçratıyor.

**Çalıştırılan negatif denemeler:** Mevcut `mp3_vcp` ikilisinin SHA-256’sı rapordaki `8c0fa4b6…14af94a` ile aynı. Kayıt değişikliklerini yalnız bellekte yaptım; her değişen satırın CRC-32’sini yeniden hesaplayıp çözümleyiciye standart girdiden verdim.

| Deneme | Gözlenen sonuç |
|---|---|
| Kabul=0, ioctl=0 veya BRR=1 | `GECERSIZ` |
| PC azami gecikme=10000001 ns | `GECMEDI` |
| Echo bütün R zamanları=0 | `GECERSIZ` |
| Y2: PC gecikmesi=`UINT64_MAX`, kampanya ve 10 s beklentisi | **`GECTI`** |
| Y3: kart son gönderim zamanı=0, kampanya ve 10 s beklentisi | **`GECTI`** |
| Y4: R’den yeniden hesaplanan bekleyen azami=100, kayıt sınırı=64 | **`GECTI`** |

CRC kontrol değerleri bağımsız hesapta `0x29B1` ve `0xCBF43926` çıktı. 1000 örnekte p99,9 sırası 999; 240000 örnekte p50/p99/p99,9 sıraları 120000/237600/239760.

## İskelete ya da diğer bölümlere notlar

- K17 ve iki görev belgesi esas alındı. Eski USB HS/yedek VCP ifadeleri bu prototipe gereklilik olarak uygulanmadı. Kontrol frekansı veya A1 hakkında karar çıkarılmadı.
- Sayaç pencereleri kaynakta uygulanmış: kartta başlatan ESLE sonrası–ilk BITIR dahil; PC’de ilk ESLE_YANIT sonrası–ilk OZET dahil. Geri çağrı sırasında henüz tüketilmemiş sınır çerçevesinin bayt düzeltmesi hesaba katılıyor.
- `IOSSIOSPEED` tanımı ve kısmi başarılı `write()` davranışı canlı birincil kaynaklardan doğrulandı. [Apple seri başlığı](https://raw.githubusercontent.com/apple-oss-distributions/IOSerialFamily/main/IOSerialFamily.kmodproj/ioss.h), [Apple write(2)](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/write.2.html).
- **Erişim sınırı:** RM0456 tam PDF’si araç boyut sınırına takıldı; denenen ST HTML adresi açılamadı. HT’nin ayrıntılı donanım zamanlaması ve Y1 ara durumunun kartta gözlenebilirliği doğrudan doğrulanmış sayılmadı. AN5593’ün ST HTML içeriğine erişildi; PDF isteği zaman aşımına uğradı.
- 600 s kampanya bilinen sonraki aşamadır; yapılmamış olması yeni bulgu sayılmadı. Kısa tarama sonuçları sürdürülebilir hız kanıtı değildir.