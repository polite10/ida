# MP4a — Og ve 200 Hz ham veri doğrulaması

Tarih: 2026-11-18. İnceleyen: `gpt-6.1-sol`, `high`.

## Hüküm ve kapsam

**Üç tamamlanmış koşunun ham verisi bağımsız bütünlük, şema, histogram, özet ve süre denetimlerinden geçti. Gerçek veride kritik sorun bulunmadı.** Ölçülen performansın periyoda yetişmemesi veri geçersizliği değildir.

Bu doğrulama yalnız aşağıdaki sabit ham kayıtlara ve ilgili analiz/yükleme/imaj kayıtlarına ilişkindir. O2 400 Hz koşusunun önceki bağımsız doğrulaması yinelenmedi. PC çözümleyicisinin bilinen bozuk girdi bulguları, devam eden düzeltmeleri ve ürün/çekirdek tasarımı bu incelemenin kapsamı değildir. Kaynak, test ve ana ölçüm raporu değiştirilmedi; karta, hata ayıklayıcıya veya seri porta erişilmedi. Yalnız bu inceleme çıktısı mimarın izniyle yazıldı.

Ham veri kökü: `${IDA_CACHE:-$HOME/.cache/ida}/mp4/kos/`. Aşağıdaki adların `.csv`, `-analiz.txt` ve `-yukle.log` dosyaları okundu:

- `og-20261118-005058`
- `o2-200-20261118-013802`
- `og-200-20261118-014813`

Karşılaştırılan derleme kaydı: `${IDA_CACHE:-$HOME/.cache/ida}/mp4/derle-20261117-185727/derle-ozet.txt`. ELF ve kimlik dosyaları: `tools/olcum/mp4/derleme/{og,o2-200,og-200}/{mp4.elf,kimlik.txt}`.

## Bağımsız yöntem

Projenin çözümleyicisini veya test üreticisini çağırmadan, tek kullanımlık Python standart kitaplık koduyla ham CSV baytları okundu. Her koşuda ilk `B 0` satırından T sonuna kadar aşağıdakiler denetlendi:

1. CSV başlığı ve artan/aynı kalabilen varış zaman damgaları; satır biçimi; her gövdenin CRC-16/MCRF4XX değeri; kesintisiz ve yinelenmeyen satır sırası; tek B/B2, X yokluğu ve son T.
2. Beklenen derleme/kimlik, hız, periyot, faz maskesi, adım hedefi ve histogram boyutları; B2'de `saat_adim=0`, `hesap_hz=hal_hz=160000000`, `uart_hata=0`.
3. Yalnız beklenen fazların bulunması; faz başına tek F/G/Q/S/E; F..E içindeki özet/histogram sıraları; E'nin H/L/M satır adetleri.
4. Her histogram kutusunun aralığı, pozitif sayımı ve yinelenmemesi; H toplamı ve L+M toplamının F.n'ye eşitliği; taşma yokluğu.
5. Adım ve gecikme min/maks değerlerinin uç kutularla uyumu; süre/gecikme toplamlarının kutuların kapsayıcı alt/üst sınır toplamları içinde olması; temiz örnek ve ISR değerlerinin sınırları.
6. Q'daki on parçanın sonlu işaretsiz toplam/maksimum değerleri; `maksimum ≤ toplam ≤ n×maksimum`, parça maksimumunun adım maksimumunu aşmaması ve parça toplamları toplamının F.top'u aşmaması.
7. Aşım sayacının örnek sayısı içinde olması ve `asim=asim_dwt`; yığın taşma bayrağı ve bildirilen CRC/tampon/yeniden hizalama/LDL/geç örnek/durum satırı kaybı sayaçlarının sıfır olması.
8. Her fazın süre alt sınırının 600 s'yi aşması; DWT süre toplamıyla tetik aralığının bir periyottan küçük uç farkı içinde uyuşması; T toplam adımının ısınma, geçişler ve kayıt hedefleriyle eşleşmesi.
9. Ham F sağlama alanının okunması ve biçimi; güncel ELF SHA-256'sının koşu başı/sonu ve derleme kaydıyla eşleşmesi, kimliğin B/kimlik.txt/ELF içinde aynı olması; yükleme günlüğündeki başarılı doğrulama ve reset kaydı.

Yüzdelikler bağımsız olarak sıralı `ceil(q×n)` örneğinin kutusundan hesaplandı. Adım kutusunun kapsayıcı üst kenarı `(k+1)×16−1` çevrimdir ve kayıtlı maksimumla kırpılır. İnce gecikme kutusu tek çevrim, kaba gecikme kutusu 16 çevrimdir. Dönüşüm, açılışta bildirilen 160 MHz için 160 çevrim/µs olarak yapıldı. Aşağıdaki yüzdelik üst sınırları beş ondalık basamakla tam verildi.

## Satır, faz ve histogram bütünlüğü

| Koşu | B öncesi dışarıda bırakılan satır | B..T satır sayısı / son sıra | Beklenen fazlar / faz başına örnek | T.adim | Hata |
|---|---:|---:|---|---:|---|
| Og 400 Hz | 2 | 3604 / 3603 | A+B+C / 241000 | 725000 | Yok |
| O2 200 Hz | 1 | 623 / 622 | B / 121000 | 121600 | Yok |
| Og 200 Hz | 1 | 3490 / 3489 | B / 121000 | 121600 | Yok |

Her koşuda CRC hatası, sıra boşluğu, yinelenen sıra ve yeniden açılış sayısı sıfırdır. Son T'de beklenen faz maskesi ve `d_atlanan=0` bulunur. Kayıt ve koşu betiği çıkış kodları sıfırdır.

| Koşu / faz | H satırı | L satırı | M satırı | H örneği | L örneği | M örneği |
|---|---:|---:|---:|---:|---:|---:|
| Og 400 A | 43 | 33 | 26 | 241000 | 224910 | 16090 |
| Og 400 B | 5 | 111 | 1547 | 241000 | 2492 | 238508 |
| Og 400 C | 4 | 3 | 0 | 241000 | 241000 | 0 |
| O2 200 B | 5 | 1 | 0 | 121000 | 121000 | 0 |
| Og 200 B | 6 | 35 | 2832 | 121000 | 616 | 120384 |

Her satırda `H=n` ve `L+M=n`; `tasan=lat_tasan=0`. Kutu adetleri E ile aynı, min/maks/top değerleri ham histogramlarla tutarlıdır.

## Adım süreleri ve aşımlar

| Koşu / faz | Min, µs | p50 üst sınır, µs | p99 üst sınır, µs | p99,9 üst sınır, µs | p99,99 üst sınır, µs | Maks, µs | Maks adım sırası |
|---|---:|---:|---:|---:|---:|---:|---:|
| Og 400 A | 1120.41875 | 1126.69375 | 2570.19375 | 2571.79375 | 2572.19375 | 2572.78125 | 161931 |
| Og 400 B | 6414.07500 | 6418.59375 | 6420.29375 | 6420.89375 | 6421.39375 | 6421.69375 | 85199 |
| Og 400 C | 2301.64375 | 2305.29375 | 2306.19375 | 2306.79375 | 2307.29375 | 2307.84375 | 103827 |
| O2 200 B | 3314.32500 | 3320.19375 | 3321.19375 | 3321.59375 | 3321.96875 | 3321.96875 | 53856 |
| Og 200 B | 6412.91875 | 6418.99375 | 6420.29375 | 6420.69375 | 6421.19375 | 6421.45000 | 50589 |

| Koşu / faz | Örnek | Periyot, µs | asim | asim_dwt | atlanan | tik_aralik |
|---|---:|---:|---:|---:|---:|---:|
| Og 400 A | 241000 | 2500 | 3013 | 3013 | 0 | 241000 |
| Og 400 B | 241000 | 2500 | 241000 | 241000 | 388488 | 629486 |
| Og 400 C | 241000 | 2500 | 0 | 0 | 0 | 241000 |
| O2 200 B | 121000 | 5000 | 0 | 0 | 0 | 121000 |
| Og 200 B | 121000 | 5000 | 121000 | 121000 | 36646 | 157646 |

Og 400 A'da periyot aşımı olup tetik atlanmaması mümkündür: sonraki adım gecikerek başlayabilir ve takip eden tetikten önce toparlayabilir. Og 400 B'de `atlanan`, `tik_aralik−n` değerinden 2 büyüktür; kodun tanımı ilk kayıtlı adımla önceki kayıt dışı geçiş adımı arasındaki atlamaları da içerir. `tik_aralik` ise yalnız ilk ve son kayıtlı adımların tetik sıralarından hesaplanır. Bu iki sayının doğrudan eşit olması gerekmez; fark kayıt giriş sınırıyla uyumludur.

**Ölçüm yorumu:** Og 400 Hz olağan ve ağır fazlarında periyot aşımları vardır; C'de yoktur. O2 200 Hz ağır fazı 121000 kayıtlı adımı aşım ve atlanan tetik olmadan tamamlamıştır. Og 200 Hz ağır fazındaki bütün adımlar 5 ms periyodu aşmıştır. Bunlar bildirilen sayılarla tam eşleşmektedir.

Görevdeki resmi D1/D2 eşikleri O2 400 Hz'e tanımlıdır; önceki O2 hükmü bu incelemede yeniden verilmedi. 200 Hz ek koşuları bu eşikleri değiştirmez veya zorunlu 400 Hz kampanyasının yerine geçmez. 200 Hz kayıtları faz başına 121000 adımdır; 400 Hz zorunlu fazların 240000 örnek koşuluyla karıştırılmamalıdır. Genel yük ölçümü kapsamı ve gerçek çekirdekle tekrar gereği korunur.

## Süre alt sınırları

`duvar_cyc` ilk kayıtlı adımın başlangıcından son kayıtlı adımın başlangıcına kadardır. Bu nedenle `duvar_cyc/160000000` muhafazakâr bir alt sınırdır. Son adım süresi ayrı kaydedilmediğinden gerçek son-bitiş süresi tam olarak geri çıkarılamaz; kayıtlı min/maks kullanılarak aşağıdaki aralık verilebilir. Aralık sonlarının sonlu ondalık değerleri yuvarlanmadan yazılmıştır.

| Koşu / faz | duvar_cyc | Süre alt sınırı, s | Son adım bitişini içeren süre aralığı, s |
|---|---:|---:|---|
| Og 400 A | 96399600009 | 602.49750005625 | [602.498620475, 602.5000728375] |
| Og 400 B | 251793996080 | 1573.7124755 | [1573.718889575, 1573.71889719375] |
| Og 400 C | 96399599975 | 602.49749984375 | [602.4998014875, 602.4998076875] |
| O2 200 B | 96799199993 | 604.99499995625 | [604.99831428125, 604.998321925] |
| Og 200 B | 126115605487 | 788.22253429375 | [788.2289472125, 788.22895574375] |

**Bütün fazlar 600 s'yi alt sınırla bile sağlar.** Ağır Og fazlarının daha uzun sürmesi hedeflenen adım sayısının tamamlanmasıyla uyumludur. Host B..T varış aralıkları Og 400 için 2808.576497833 s, O2 200 için 608.367283333 s, Og 200 için 828.290608041 s'dir; bunlar ısınma, geçiş ve seri döküm sürelerini de içerir ve faz ölçüm süreleri yerine kullanılmadı.

## Gecikme ve yığın

| Koşu / faz | Gecikme min, µs | Gecikme p99,9 üst sınır, µs | Gecikme maks, µs | yigin_kayit, B | yigin_tepe, B |
|---|---:|---:|---:|---:|---:|
| Og 400 A | 0.50000 | 176.29375 | 179.57500 | 2216 | 2704 |
| Og 400 B | 0.38750 | 2497.19375 | 2499.96250 | 2264 | 2752 |
| Og 400 C | 0.50000 | 0.75000 | 0.81250 | 2216 | 2704 |
| O2 200 B | 0.36875 | 0.46875 | 0.47500 | 2440 | 3392 |
| Og 200 B | 0.42500 | 4995.29375 | 4999.92500 | 2264 | 2752 |

Gecikme, güncelleme olayından ana döngünün tetiği gördüğü örnekleme noktasına göredir; kesme girişinden adım başlangıcına kadar olan farklı ölçüm olarak yorumlanmadı.

Koşu başına gözlenen faz tepelerinin maksimumu: Og 400'de `yigin_tepe=2752 B`, `yigin_kayit=2264 B`; O2 200'de `3392/2440 B`; Og 200'de `2752/2264 B`. Tüm `yigin_kayit_tasti` değerleri sıfırdır. Yeniden boyama nedeniyle son fazın tek `yigin_tepe` değeri ömür boyu tepe sayılmadı. `yigin_kayit` geçiş adımlarını ve ölçüm çevresindeki işleri de kapsar.

## Sağlama toplamları ve imaj bağı

Ham F satırlarındaki CRC ile korunan çıktı sağlama alanları:

| Koşu | A | B | C |
|---|---|---|---|
| Og 400 | `0xC02B9132` | `0x8F3440B8` | `0x18529BF5` |
| O2 200 | — | `0xEBC826A4` | — |
| Og 200 | — | `0xEBC826A4` | — |

Og 400 alanları önceki bağımsız O2 400 doğrulamasındaki A/B/C alanlarıyla aynı; yeni O2/Og 200 alanları da birbirine eşittir. Karşılık gelen S algoritmik sayaçları da derlemeler arasında eşleşir. **Her adımın 100 baytlık çıkışı ham kayıtta bulunmadığından bu çıktı FNV sağlama toplamları sıfırdan yeniden hesaplanamadı.** Doğrulanan, alanların satır CRC'si, karşılaştırmalı eşitliği ve imaj bağıdır; sağlama eşitliği sayısal doğruluk veya bit düzeyinde iz eşitliğinin bağımsız kanıtı sayılmaz.

| Koşu | Kimlik | ELF SHA-256 |
|---|---|---|
| Og 400 | `20261117-185727-og-be020939` | `5f0fd81f63a38bdcc014e77e2157ec841f639e007dc833704e6e1372b6223b5c` |
| O2 200 | `20261117-185727-o2-200-05c0b3de` | `1beaf4cb861a7d7800f512f383655a8a89883c7c0edff012fdac559722faec6e` |
| Og 200 | `20261117-185727-og-200-5fe214ad` | `e7e22797e47ba1e1bb0e2033c7b1b7370fad2c604cee9e40869e566741a1a117` |

Üç kimlik B satırı, `kimlik.txt` ve ELF içeriğinde aynıdır. Üç güncel ELF SHA-256'sı koşu başı/sonu ve derleme kaydıyla aynıdır; yükleme günlüklerinde `Download verified successfully` ve `Software reset is performed` vardır. Kartın flash'ı bu incelemede yeniden okunmadı. Önceki D3/çağrı grafiği denetimi tekrarlanmadı; bu rapor 200 Hz ELF'leri için yeni bir D3 hükmü vermez.

Ham kayıtların bu incelemede hesaplanan SHA-256 değerleri:

| Dosya | SHA-256 |
|---|---|
| `og-20261118-005058.csv` | `5f97f89849b35b89a5f8626ea2fba50d9097a09b5d03dc2af4775d3ad354b989` |
| `o2-200-20261118-013802.csv` | `2fa7eeaf3c0e3727760104189bc369ae3bd94bd7ce46078c6c7720c51e291f35` |
| `og-200-20261118-014813.csv` | `e81339c7c864ecfa7570fb3198491519c5cca3b2bf299b2e06a89f70fd7e62c2` |

## Sınırlar

- Ham kayıtta adım başına süre ve çıkış baytları değil, özet ve histogramlar vardır. Özetlerin veriyle tutarlılığı denetlendi; tekil sürelerin veya FNV çıktısının sıfırdan yeniden üretimi yapılmadı.
- Zaman dönüşümü B2'de bildirilen 160 MHz ve kayıtlı periyot yapılandırmasına dayanır; mutlak saat frekansı harici cihazla ölçülmedi.
- Analiz/imaj günlüklerinde AC güç ve `caffeinate` başlangıç kanıtı vardır; bağımsız fiziksel güç/ısı ölçümü yapılmadı.
- Kodun son PC düzeltmesi ve ana raporun son sürümü ayrıca inceleme gerektirir. Bu rapor yalnız burada listelenen tamamlanmış ham verilerin doğrulamasıdır.
