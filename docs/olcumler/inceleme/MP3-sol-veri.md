# sol incelemesi: MP3-vcp

## Hüküm

**Kampanya hükümleri, sayaçlar ve RTT tablosu doğrulandı; rapor bu haliyle kapanmamalı.** Özellikle 6 Mbaud koşusundaki “bir kez” takvim aşımı sayısı yanlış. Host yükü anlatımında da kanıtın desteklemediği kesinlik var.

Dosya değiştirmedim; derleme, test takımı veya kart işlemi çalıştırmadım.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `docs/olcumler/MP3-vcp.md:43` | 6 Mbaud’da “bir kez” aşım yanlış. Aralık 582188 ns, azami gecikme 18116452 ns. Ardışık gönderimde bu gecikmenin ardından 13 isteğin daha gecikmesi 10 ms üzerinde kalır. | **En az 14 istek** sınırı aşmıştır: `18116452 − 13×582188 = 10548008 ns`. Son yazma yeniden plana yetişmiştir. Kesin sayı akış kaydında tutulmadığından yeniden sayılamaz; “bir kez” kaldırılmalı. | kritik |
| 2 | `docs/olcumler/MP3-vcp.md:88` | Yükün 19:28’de başladığı ve 1/2/3b’nin “etkilenmediği” kayıtlarla doğrulanamıyor. Günlük 19:45:45’te başlıyor. Ayrıca `bfs` CPU değeri **%523,0** görüyor; verilen yaklaşık %180–300 aralığını aşıyor. | Örneklenen dönem ile kaydedilmemiş gözlemleri ayır; 19:45 öncesindeki yükü doğrulanmamış say. “Nedensellik kurulamaz” değerlendirmesi korunabilir. | önemli |
| 3 | `docs/olcumler/MP3-vcp.md:96` | “Her koşudan önce ve sonra KIMLIK eşleşti” genellemesi kopan koşu için doğru değil. | Sekiz tamamlanan koşuda eşleşti; koşu 3’ün son sorgusunda port açılamadı. | küçük |

## Yeniden hesaplananlar

Manifestteki **40/40 dosyanın SHA-256’sı**, dokuz kaydın bütün satır CRC’leri ve numaraları tuttu. Kaynak özeti, ELF ve PC aracı özetleri raporla eşleşti. B başlıklarının profil/mod/baud/yük/hız/oran/aralık/adet değerlerini sabit beklentilerle denetledim; dokuzu da uygun. Echo sınırları **64 bekleyen / 1000 ms**.

Akış için `aralık = ceil(276×10×10⁹ / (0,8×etkin baud))`, adet `ceil(600×10⁹/aralık)`:

| Koşu | Çerçeve/yön | Tel baytı/yön | PC azami gecikme, µs | Hüküm |
|---|---:|---:|---:|---|
| 921600 | 159920 | 44137920 | 4408,667 | GECTI |
| 2000000 | 347827 | 96000252 | 1143,291 | GECTI |
| 3000000 sabah | PC gönderim/alım: 70116/70144 | Tamamlanmadı | 1898430,667 | GECERSIZ |
| 3000000 yineleme | 521740 | 144000240 | 7437,834 | GECTI |
| 6000000 | 1030595 | 284444220 | 18116,452 | GECMEDI |

Tamamlanan akışların PC yazma azamileri **154,083 / 194,625 / 3295,875 / 11308,250 µs**; kart gecikmeleri **13/54/54/54 µs**, halka azamileri **8/16/30/63 B**: raporla aynı.

Echo sonuçları:

| Profil | İstek = yanıt | Takvim aşımı | PC azami gecikme, µs | Azami bekleyen | Hüküm |
|---|---:|---:|---:|---:|---|
| 64 B, 200/s | 120000 | 2 | 11484,917 | 3 | GECMEDI |
| 256 B, 200/s | 120000 | 1 | 11839,583 | 3 | GECMEDI |
| 64 B, 400/s | 240000 | 1 | 10096,708 | 5 | GECMEDI |
| 256 B, 400/s | 240000 | 0 | 4625,458 | 6 | GECTI |

Tamamlanan **sekiz koşuda** iki uç çerçeve/bayt muhasebesi eşleşiyor; veri/CRC/sıra/UART/DMA/taşma sayaçları sıfır. Echo timeout ve atlanan istek sıfır. Süre, bitiş ve dönüş ölçütleri uygun. **4–7’nin tek geçmeme nedeni PC takvim sınırı.**

Yüzdelikleri bağımsız Python hesabıyla, sıralı örneklerde **`x[ceil(p×n)]`** yöntemiyle hesapladım; raporla yöntem farkı yok, enterpolasyon kullanmadım. Aşağıdaki değerler ns düzeyinde eşleşti; birim µs:

| Profil | Plandan p50 / p99 / p99,9 / maks | Yazma başından p50 / p99 / p99,9 / maks |
|---|---|---|
| 64 B, 200/s | 833,000 / 982,750 / 1100,875 / 12463,792 | 829,917 / 977,375 / 1062,459 / 11837,959 |
| 256 B, 200/s | 2188,250 / 2296,417 / 2488,000 / 14073,875 | 2184,708 / 2287,834 / 2404,875 / 12536,167 |
| 64 B, 400/s | 814,541 / 895,791 / 1114,416 / 12474,625 | 813,833 / 894,667 / 1071,458 / 11198,167 |
| 256 B, 400/s | 2165,000 / 2266,166 / 2340,833 / 14508,041 | 2163,500 / 2264,292 / 2334,625 / 10527,541 |

Yazma p99/maks değerleri de eşleşti. Plandan RTT’si 10 ms’yi aşan yanıt sayıları **4/4/4/2**.

Kopma kaydı son yazmayı **t0+82,530724042 s** gösteriyor; kart özeti yok. Portun kaybolması, AC’den pile geçiş ve sonraki açılışta koşu sayacının sıfırlanması anlatımla tutarlı; fiziksel neden kanıtlanmıyor.

Host günlüğünde **91 örnek**, yük ortalaması **3,44–5,76**; `sig` ve `misc` her örnekte mevcut. Bu veri nedensellik kurmaya yetmez.

## İskelete ya da diğer bölümlere notlar

K17 ve dondurulmuş kapsam esas alındığında taşıma seçimi tutarlı. Kontrol hızı, A1 kabulü veya ürün başarımı kararı çıkarılmamış.

ST-LINK VCP ve USART1 PA9/PA10 eşlemesi canlı [ST ürün sayfası](https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html) ve [ST BSP başlığıyla](https://raw.githubusercontent.com/STMicroelectronics/stm32u5x9j-dk-bsp/main/stm32u5x9j_discovery.h) doğrulandı. Baud hesabı [ST LL kaynağıyla](https://raw.githubusercontent.com/STMicroelectronics/stm32u5xx-hal-driver/2552682ee5a0d6f826a6435750fc9267be65868b/Inc/stm32u5xx_ll_usart.h) uyumlu; prototip en yakın tam baud’a yuvarlıyor.

UM2967 ve RM0456 tam PDF’lerine araç boyut sınırı nedeniyle erişemedim. DMA varsayımları ve reset bayrakları doğrulanmış sayılmadı. Güncel “doğrulanmayanlar” listesine **19:45 öncesindeki host yükü ve akıştaki kesin aşım sayısı** eklenmeli; geçmişte belirtilen ASan/TSan ve ısıl etki sınırlamalarına açık atıf yapılmalı.