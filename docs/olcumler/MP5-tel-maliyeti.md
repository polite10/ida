# MP5 — Kartta tel katmanının maliyeti (MAVLink 2 ayrıştırma ve kodlama)

Tarih: 2026-11-20. Görev: `docs/gorevler/W0-MP5a.md`. Yazan: `ida-uygulayici` (Opus). Dal: `olcum/mp5-tel-maliyeti` (taban `is/w2-protokol`, `5b34de8`). Prototip: `tools/olcum/mp5/` (atılabilir; ürün kodu değildir). Ürün kitaplığı `protocol/ida_link/` değiştirilmedi.

Bu ölçüm iskelet §8'in "A1 biçimi" satırını ve A1 §8.1 O2'yi kapatmak için yapıldı. Eşikler görev belgesinde ölçümden önce yazılmıştı; ölçüm tanımı sonuca göre değiştirilmedi.

> Bölüm 1–8 ilk ölçümdür (ürün `5b34de8`) ve olduğu gibi bırakıldı. Mimarın istediği iyileştirme turu (ürün `0fa569e` tabanlı kopya; eşiklerin iyileştirilmiş uygulamayla sağlanıp sağlanmadığı) **Bölüm 9**'dadır: iyileştirilmiş kopya E1'i 0,1531 ms, E2'yi 0,2641 ms ile sağlar (tek seferde ve 64 B'lık parçalarla besleme; bayt bayt beslemede sağlanmaz).

## Özet

**Ölçülen (kartta, `fw-release` derlemesi, 160 MHz, ICACHE açık):**

| # | Büyüklük | Ölçülen en kötü | Eşik | Sonuç |
|---|---|---:|---:|---|
| E1 | 1 050 B'lık E1 akışının ayrıştırılması, tek seferde besleme | 0,5562 ms | ≤ 0,20 ms | **geçmedi** (≈ 2,8 kat) |
| E1 | aynı, 64 B'lık parçalarla | 0,5664 ms | ≤ 0,20 ms | **geçmedi** |
| E1 | aynı, bayt bayt | 1,0938 ms | ≤ 0,20 ms | **geçmedi** |
| E1 | aynı, tek seferde + her çerçevenin `ida_link_coz` ile tipli yapıya çözülmesi | 0,6387 ms | ≤ 0,20 ms | **geçmedi** |
| E2 | 1 200 B'lık pencerenin ayrıştırılması (tek seferde) + kart → sim en kötü periyodunun (1 326 B, 23 çerçeve) kodlanması | 0,5693 + 0,2118 = 0,7810 ms | ≤ 0,50 ms | **geçmedi** (≈ 1,6 kat) |
| E2 | aynı, ayrıştırma 64 B'lık parçalarla / bayt bayt / çözmeyle | 0,7925 / 1,3986 / 0,8754 ms | ≤ 0,50 ms | **geçmedi** |
| E3 | 258 B'lık çerçeve: ayrıştırma (tek seferde / 64 B / bayt bayt / çözmeyle) | 80,6 / 83,2 / 215,4 / 100,4 µs | raporlanır | — |
| E3 | 258 B'lık çerçeve: kodlama | 39,1 µs | raporlanır | — |

"En kötü": 10 000 yinelemenin en büyüğü, ICACHE açıkken sıcak ve soğuk (her yinelemeden önce geçersiz kılınmış) kiplerin ve üç alan deseninin (altın, kırpılmamış, sıfır) en kötüsü; boş ölçüm bölgesinin maliyeti (1 çevrim) çıkarılmış. Süre dağılımları neredeyse sabittir: 352 serinin (iki ürün koşusu) 238'inde tek, 114'ünde iki farklı değer görüldü; p99,9 ile en büyük arasındaki fark en çok 2 çevrimdir. Eşik karşılaştırması en büyük değerle yapıldı. Ana akışlarda ayrıştırma kırpılmamış ve altın desende bayt başına 76–85 çevrim, sıfır desende (13 B'lık çerçeveler) 200–202 çevrim tutar.

**Maliyet modeli (ölçümden doğrusal uyum, dialect'in 37 mesajı, tek çerçeve, kırpılmamış; R² ≥ 0,9997):** ayrıştırma ≈ 2 110 çevrim (13,2 µs) çerçeve başına + 41,0 çevrim (0,256 µs) telde bayt başına; kodlama ≈ 189 çevrim (1,18 µs) çerçeve başına + 23,0 çevrim (0,144 µs) bayt başına. Kodlamanın çerçeve başına sabiti küçüktür; ayrıştırmanınki büyüktür (Bölüm 3.2).

**İki iyileştirme denemesi** (eşik aşıldığı için, ürün kaynağının prototip içindeki kopyası üzerinde; Bölüm 6): tablolu CRC ayrıştırmayı %8–15, kodlamayı %30 kısaltır, 512 B flash ekler, eşikleri sağlamaz (E1 0,5044 ms, E2 0,6578 ms). `-Os` her seride %12–67 yavaştır (E1 0,7676 ms). Ayrıştırmanın çerçeve başına maliyetinin büyük kısmı, teslim edilen çerçevenin yük kuyruğunun newlib-nano'nun bayt bayt `memset`'iyle 255 bayta kadar sıfırlanmasından gelir (çıkarım; ölçülmedi); bu ve başka öneriler yalnız yazıldı, uygulanmadı.

**Kartın bırakıldığı durum:** dahili flash'ta MP5 ölçüm yazılımının ürün imajı (`tools/olcum/mp5/derleme/o2/mp5.elf`, SHA-256 `9136f5b0897afa466d71a7b87e5db0cdb79fcc7279710db8c30e0badeaa373ff`, kimlik `20261120-113532-o2-7b23157d`); kampanyasını bitirdi (T satırı) ve yeşil LED yanık olarak boş döngüde bekliyor; her sıfırlamada kampanyayı yeniden koşar (≈ 10,5 dakika, yalnız seri çıktı üretir). Yalnız yazılan flash sayfaları silindi; seçenek baytlarına, koruma düzeyine, önyükleyiciye ve harici flash'a dokunulmadı. Her yüklemeden önce fabrika yedeğinin geçerliliği denetlendi (`yedek-dogrula.sh`).

## 1. Düzenek

- **Kart yazılımı** (`tools/olcum/mp5/fw/`): kart açılışı, 160 MHz saat ve DWT ölçümü MP4a'nın `platform.c`'sinden kopyalandı ve kısaltıldı (SMPS, Range 1, HSE 16 MHz + PLL1 M=4 N=80 R=2; AHB ÷2 → ≥ 5 µs → ÷1; MP1 bulgusu). Flash gecikmesi 4, önden getirme açık, ICACHE açık (açılış satırı: `FLASH_ACR` = `0x00000104`, `ICACHE_CR` = `0x00000005`, hesaplanan saat 160 000 000 Hz, `IDCODE` = `0x30006481`). Çıktı ST-LINK sanal seri portundan (USART1) 115 200 8N1 metin satırları; her satır sıra numarası ve CRC-16 taşır (MP4a biçimi). Seri çıktı yalnız ölçüm bölgelerinin dışında, yoklamayla gönderilir.
- **Ölçülen kod:** ürün kitaplığı, depo kökünün `fw-release` ön ayarıyla derlenmiş `build/fw-release/protocol/ida_link/libida_link.a` (W2-depo/W2-protokol derleme düzeni) olduğu gibi bağlandı. `tel.c`'nin derleme bayrakları (depo derlemesinin `compile_commands.json`'ından): `-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard --specs=nano.specs -ffunction-sections -fdata-sections -O2 -g -DNDEBUG -std=c17 -Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Werror -ffp-contract=off -fno-fast-math -pedantic-errors -fstack-usage`. Derleyici GNU Tools for STM32 14.3.rel1 (`arm-none-eabi-gcc` 14.3.1). Bağlantı newlib-nano iledir (ürün araç zinciriyle aynı); bu kitaplıkta `memcpy` ve `memset` bayt bayt döngülerdir (sökümden; Bölüm 6).
- **Ölçüm bölgesi:** `__disable_irq()`; (soğuk kipte ICACHE geçersiz kılma ve bitişinin beklenmesi); `c0 = DWT->CYCCNT`; ölçülen işlev; `c1 = DWT->CYCCNT`; `__enable_irq()`. Açılıştan sonra SysTick durdurulur ve başka kesme kaynağı açılmaz: ölçüm boyunca hiçbir kesme yoktur. Bir yineleme akışın tamamıdır (ayrıştırmada bütün baytlar, kodlamada bütün mesajlar). Her seride 10 000 yineleme; süreler tek tek saklanır, seri sonunda sıralanıp (değer:adet) olarak gönderilir.
- **Ölçülen işlevler** (`fw/main.c`; bütün serilerde aynı kod, `noinline`): `ayr_tek` (her `ida_link_al` çağrısına kalan bütün baytlar), `ayr_p64` (64 B'lık parçalar), `ayr_bayt` (her çağrıda 1 bayt), `ayr_alcoz` (tek seferde + teslim edilen her çerçeve için `ida_link_coz`), `kodla` (`ida_link_gonder` ile mesajlar sırayla çıkış tamponuna). Teslim edilen her çerçevede ölçülen işlev bir özet toplar (`msgid + len + yuk[len-1]`; çözmede yapının ilk ve son baytı): ayrıştırmanın sonucunu kullanan en küçük iş; maliyeti birkaç çevrimdir ve ölçüme dahildir.
- **Girdi akışları** derleme zamanında host'ta üretilir (`pc/mp5_uret.c`): ürün kaynağı host derleyicisiyle derlenip A1 Ek B'nin altın çerçeveleri (`protocol/ida_link/veri/altin.json`'dan üretilen veri) çözülerek mesaj yapıları kurulur, akışlar `ida_link_gonder` ile seq 0'dan kodlanır ve kartta sabit veri (`mp5_akislar.c`) olarak bulunur; ölçümden önce SRAM'e kopyalanır (ürün alım tamponu SRAM'dedir). Üreteç ayrıca her altın yapının yeniden kodlanınca Ek B baytlarını verdiğini denetler.
- **Önbellek kipleri:** *sıcak* = ICACHE açık, ölçülmeyen bir ısınma yinelemesinden sonra; *soğuk* = ICACHE açık, her yinelemeden hemen önce geçersiz kılınmış (bölüm 01 §2 md.10'un soğuk önbellek senaryosu); *kapalı* = ICACHE kapalı (ürün yapılandırması değildir, bilgi için). Eşik karşılaştırması ürün yapılandırmasıyla (ICACHE açık) yapılır ve sıcak ile soğuğun en kötüsünü alır.
- **Boş ölçüm bölgesi:** iki ardışık `CYCCNT` okuması her kipte 1 çevrim (10 000 örnekte sabit); bütün "net" değerlerden çıkarıldı. Aynı bölgede işlev göstericisi üzerinden boş bir işlev çağrısı sıcakta 6, soğukta 23, kapalıda 26 çevrimdir (çağrının kendisi; ölçülen değerlerde kalır, payı ihmal edilebilir).

## 2. Girdi akışları

Her akış üç alan deseniyle koşuldu: **altın** (A1 Ek B'nin alan değerleri; gerçekçi kırpma), **ham** (yükün her baytı sıfırdan farklı: hiç kırpılmaz; çerçeveler en büyük boylarında), **sıfır** (yapı sıfır: yük 1 bayta kırpılır, her çerçeve 13 B). Görev belgesinin "kırpılmış ve kırpılmamış iki uç"u ham ve sıfır desenleridir. Akışların boyları A1 Tablo 3.3'ün kırpılmamış boylarıyla (ham desen) kuruldu; çerçeve dökümleri üreteçten (`derleme/uretilen/akis-dokum.md`).

**(a) E1 — 1 050 B, 22 çerçeve.** Bir pencerenin sonar dışı en yoğun karışımı (sonar çerçevesi bayt başına en ucuz çerçevedir; onu dışarıda bırakmak bayt başına maliyeti en kötüye çeker). Sıra A1-K36/K39'a göre: önce toplu ve atanmış üyeler, sonra kritik üyeler, en sonda TF. Kritik üyeler 501 B ≤ `B_krit` 520 B; A1-K34 sınırları içinde (4 olay sonucu, her daraltıcı türden 1 komut, 4 daraltıcı olmayan komut, bir algılama partisi: başlık + 4 kayıt).

| Kimlik | Ad | Telde boy (B; ham) | Adet |
|---|---|---:|---:|
| 46035 | DET_BATCH | 39 | 1 |
| 46036 | DET_RECORD | 90 | 4 |
| 46041 / 46042 | CMD_MANUAL / CMD_GUIDED | 43 / 56 | 1 / 1 |
| 46045 / 46040 | CMD_FC / CMD_SIMPLE (daraltıcı olmayan) | 29 / 22 | 1 / 1 |
| 46021 / 46024 / 46025 / 46027 | MAG / DVL / PING1D / LEAK | 39 / 89 / 48 / 28 | 1 her biri |
| 46020 / 46028 | IMU / SAFETY | 59 / 34 | 1 / 1 |
| 46040 / 46045 | CMD_SIMPLE / CMD_FC (daraltıcı) | 22 / 29 | 1 / 1 |
| 46046 | EVENT_RESULT | 31 | 4 |
| 46010 | TIME_FRAME | 29 | 1 |
| | **Toplam** (altın: 1 050 B; sıfır: 286 B) | **1 050** | **22** |

**(b) PEN — tam pencere, 1 200 B (= `B_pencere`), 20 çerçeve.** Algılama partisi (başlık + 1 kayıt), Ping360 başlığı ve iki veri parçası, 2 daraltıcı olmayan komut, 5 sensör (MAG, BARO, PING1D, BATT, LEAK), IMU, emniyet, 1 daraltıcı komut, 4 olay sonucu, TF; kritik 465 B. Altın desende Ping360 parçalarından biri dolu (240 örneğin hepsi sıfırdan farklı; son parça olmayan parça), öteki Ek B'nin kısmi parçasıdır (138 B); altın akış 1 080 B, sıfır akış 260 B.

**(c) KS — kart → sim en kötü periyot, 1 326 B (ham), 23 çerçeve** (A1 E19'un çerçeve kümesi, her sınıf tam sayıya yukarı yuvarlanarak; E19'un toplamı 1 282,5 B'dır): (i) 2 ACTUATOR (TF yanıtı + `ASYNC`; 100 B); (ii) 10 EVENT (E19'da 473,7 B/periyot; 490 B) ve 6 CMD_RESULT (234 B); (iii) `R_iii` = 500 B payı: 2 CALL_TRACE, NAV, STATUS, FC_STATUS (502 B). Altın: 1 318 B (ACTUATOR 46 B'a kırpılır), sıfır: 299 B.

**(d) BUY / BUYK — dialect'in en büyük çerçevesi:** kırpılmamış PING360_DATA, 258 B. Kart bu mesajı yalnız alır; kodlama ölçümü (BUYK) için sim rolündeki gönderici kullanıldı (aynı kitaplık kodu).

**Doğrusal uyum kümesi (FIT):** dialect'in 37 mesajının her biri, ham desen, tek çerçeve (15–258 B); ayrıştırma (tek seferde) ve kodlama; sıcak kipte.

## 3. Sonuçlar

Değerler çevrimdir (160 MHz'te 1 µs = 160 çevrim); "net" boş bölge (1 çevrim) çıkarılmış değerdir. Bütün tablolar `ozet.py tablo` çıktısıdır (Bölüm 8). Aşağıdaki değerler koşu 1'dendir; koşu 2 ile farkı Bölüm 4'te.

### 3.1 Ana akışlar, ICACHE açık

| Akış | Desen | Bayt | Çerçeve | Besleme | İşlem | Sıcak: en küçük / ortanca / p99,9 / en büyük | Soğuk: en büyük | Net en kötü (ms) | Net ortanca çevrim/B |
|---|---|---:|---:|---|---|---|---:|---:|---:|
| E1 | altın = ham | 1 050 | 22 | tek | al | 88 722 / 88 722 / 88 722 / 88 722 | 88 992 | 0,5562 | 84,50 |
| E1 | altın = ham | 1 050 | 22 | p64 | al | 90 367 (dört değer aynı) | 90 624 | 0,5664 | 86,06 |
| E1 | altın = ham | 1 050 | 22 | bayt | al | 174 741 (aynı) | 175 001 | 1,0938 | 166,42 |
| E1 | altın = ham | 1 050 | 22 | tek | alcoz | 101 842 (aynı) | 102 186 | 0,6387 | 96,99 |
| E1 | sıfır | 286 | 22 | tek / p64 / bayt | al | 57 398 / 57 832 / 79 241 | 57 666 / 58 087 / 79 499 | 0,3604 / 0,3630 / 0,4969 | 200,7 / 202,2 / 277,1 |
| PEN | ham | 1 200 | 20 | tek | al | 90 824 (aynı) | 91 085 | 0,5693 | 75,69 |
| PEN | ham | 1 200 | 20 | p64 | al | 92 671 (aynı) | 92 919 | 0,5807 | 77,22 |
| PEN | ham | 1 200 | 20 | bayt | al | 189 641 (aynı) | 189 892 | 1,1868 | 158,03 |
| PEN | ham | 1 200 | 20 | tek | alcoz | 105 848 (aynı) | 106 178 | 0,6636 | 88,21 |
| PEN | altın | 1 080 | 20 | tek / bayt | al | 85 904 / 174 641 | 86 165 / 174 892 | 0,5385 / 1,0931 | 79,54 / 161,70 |
| PEN | sıfır | 260 | 20 | tek / bayt | al | 52 284 / 72 141 | 52 543 / 72 390 | 0,3284 / 0,4524 | 201,1 / 277,5 |
| KS (kodlama) | ham | 1 326 | 23 | — | kodla | 33 718 (aynı) | 33 884 | 0,2118 | 25,43 |
| KS (kodlama) | altın | 1 318 | 23 | — | kodla | 33 598 / 33 598 / 33 598 / 33 600 | 33 764 | 0,2110 | 25,49 |
| KS (kodlama) | sıfır | 299 | 23 | — | kodla | 18 309 (aynı) | 18 485 | 0,1155 | 61,23 |
| BUY | ham | 258 | 1 | tek / p64 / bayt | al | 12 681 / 13 096 / 34 251 | 12 898 / 13 306 / 34 458 | 0,0806 / 0,0832 / 0,2154 | 49,2 / 50,8 / 132,8 |
| BUY | ham | 258 | 1 | tek | alcoz | 15 800 | 16 062 | 0,1004 | 61,2 |
| BUYK (kodlama) | ham | 258 | 1 | — | kodla | 6 115 | 6 262 | 0,0391 | 23,7 |

- **Dağılım:** her seride en çok iki farklı değer; en küçük, ortanca, p99,9 ve en büyük ya aynıdır ya da 6 çevrim içindedir (p99,9 ile en büyük arasında en çok 2 çevrim). Ayrıştırmada farklı olan tek değer seri içindeki ilk yinelemedir (soğukta 6, sıcakta ölçülmeyen ısınma yinelemesinde 8 çevrim kısa); alıcının ilk çerçevede seq boşluğu saymamasıyla açıklanır (çıkarım; sonraki her yineleme seq 0'dan başladığı için `rx_seq_gap` bir kez artar).
- **İçerik bağımsızlığı:** E1'in altın ve ham desenleri bayt bayt aynı boydadır (Ek B'nin bu mesajlarda kırpılacak sıfır sonu yok) ve süreleri çevrimi çevrimine aynıdır: maliyet içeriğe değil, bayt ve çerçeve sayısına bağlıdır.
- **Soğuk önbellek:** her yinelemeden önce ICACHE'in geçersiz kılınması 1 050–1 200 B'lık akışlarda 250–340 çevrim (≈ %0,3) ekler; kitaplığın sıcak kodu küçüktür (Bölüm 5).
- **Besleme biçimi:** 64 B'lık parçalar tek seferde beslemeye göre %1,9–2,0; bayt bayt besleme %97–109 ekler (her `ida_link_al` çağrısının sabit maliyeti, ≈ 80 çevrim/bayt; hesap: (174 741 − 88 722) / 1 050).
- **Çözme (`ida_link_coz`):** tek seferde ayrıştırmaya E1'de %14,8, PEN'de %16,5, 258 B'lık çerçevede %24,6 ekler.

### 3.2 Bayt başına ve çerçeve başına maliyet

| İşlem | Çerçeve başına sabit `a` | Bayt başına `b` | R² | En büyük artık |
|---|---:|---:|---:|---:|
| Ayrıştırma (`ida_link_al`, tek seferde; 37 nokta) | 2 110 çevrim (13,19 µs) | 40,97 çevrim (0,256 µs) | 0,99991 | 50 çevrim |
| Kodlama (`ida_link_gonder`; 37 nokta) | 189 çevrim (1,18 µs) | 22,97 çevrim (0,144 µs) | 0,99972 | 51 çevrim |

Model: süre = `a` + `b` × telde çerçeve boyu (12 + yük). Ana akışlarla sağlaması (hesap): E1 ham için 22 × 2 110 + 1 050 × 40,97 = 89 440 çevrim, ölçülen net 88 721 (%0,8); PEN ham için 91 360, ölçülen 90 823 (%0,6); KS ham kodlama için 23 × 189 + 1 326 × 22,97 = 34 806, ölçülen 33 717 (%3,2).

Ayrıştırmanın çerçeve başına sabitinin büyük kısmı, `cerceve_bitir`'in yükün kullanılmayan kuyruğunu sıfırlamasından (`memset(&c->yuk[len], 0, 255 − len)`) ve newlib-nano'nun bayt bayt `memset` döngüsünden gelir. Bu bir **çıkarımdır (ölçülmedi)**: sıfır desende çerçeve 13 B iken ayrıştırma çerçeve başına ≈ 2 600 çevrim tutar (E1 sıfır: 57 397 / 22); `ida_link_coz`'un ek maliyetinden (yapının `memset` + `memcpy`'si) türetilen bayt başına ≈ 13–16 çevrim (BUY: (15 799 − 12 680) / 246 = 12,7; PEN: 15 024 çevrim / 960 B yapı = 15,6; ikisi de çerçeve başına çağrı maliyetini içerir) `memset` ve `memcpy` döngülerinin her birine bayt başına ≈ 6–7 çevrim düşürür; 254 baytlık sıfırlama da buna göre ≈ 1 500–1 800 çevrimdir (tahmin).

Eşiğe çevirme (hesap, modelden; ICACHE açık, tek seferde besleme): 0,20 ms = 32 000 çevrim, E1 karışımının ortalama çerçevesi (47,7 B) için çerçeve başına ≈ 4 065 çevrim → bir çekirdek adımı sürerken ≈ 7,9 çerçeve ≈ 375 B ayrıştırılabilir; 1 050 B'lık birikim ≈ 89 000 çevrimdir.

### 3.3 ICACHE kapalı (ürün yapılandırması değil)

| Seri | Net en büyük | ICACHE açık sıcağa göre |
|---|---:|---:|
| E1 ayrıştırma, tek seferde | 0,9338 ms (149 413 çevrim) | +%68 |
| E1 ayrıştırma, bayt bayt | 1,7487 ms | +%60 |
| PEN ayrıştırma, tek seferde (ham) | 0,9256 ms | +%63 |
| KS kodlama (ham) | 0,3242 ms | +%54 |

## 4. Doğruluk ve iki bağımsız koşu

**Ölçülen işin yapıldığının kanıtı** (her koşuda `ozet.py denetle` geçti; denetlenmeden özet basılmaz):

1. *Ölçülmeyen doğrulama geçişi* (V satırları; 44 ayrıştırma akışı × 3 besleme biçimi): kartta teslim edilen her çerçevenin (msgid, seq, len, yük) FNV-1a'sı, `ida_link_coz` çıktılarının FNV-1a'sı ve çerçeve sayısı host'ta aynı kaynağın ürettiği referansla aynı; üç besleme biçimi aynı çerçeveleri verdi.
2. *Ayrıştırma serileri:* her yinelemenin özeti host referansıyla aynı (176 serinin hiçbirinde ayrım yok); seri sonunda alıcı sayaçları beklenen: `rx_frames` = çerçeve × yineleme, `rx_bytes` = bayt × yineleme, `rx_crc_err` = `rx_resync_bytes` = `rx_unknown_msgid` = `rx_bad_len` = `rx_wrong_sys` = `rx_iflag` = `rx_mav1` = `rx_overflow` = 0, `rx_seq_gap` = yineleme − 1 (her yineleme seq 0'dan başlar; davranış değişmez). Örnek: PEN altın, kapalı kip: `rx_frames` = 200 000, `rx_bytes` = 10 800 000.
3. *Kodlama serileri:* her yinelemede (ölçüm bölgesinden sonra) çıkış baytlarının boyu ve FNV-1a'sı host'ta `ida_link_gonder` ile üretilen referansla aynı; çıkış tamponu her yinelemeden önce sıfırlanır.
4. Kitaplık ayrı derlenmiş bir arşivdir ve bağlama zamanı eniyilemesi kullanılmadı; ölçülen işlevler kitaplık çağrılarının sonucunu kullanır. Derleyicinin ölçülen işi silmediği yukarıdaki sayaç ve özet eşitlikleriyle gösterilir.

**İki bağımsız koşu:** aynı ürün imajı (SHA-256 `9136f5b0…`, kimlik `20261120-113532-o2-7b23157d`) iki kez ayrı ayrı yüklenip sıfırlanarak koşuldu: koşu 1 08:35–08:46 UTC, koşu 2 09:04–09:15 UTC; aralarında karta üç deneme imajı yüklendi. 176 serinin **176'sında** (değer:adet) dağılımı iki koşuda birebir aynıdır: ortanca ve en büyük farkı 0 çevrim. Ayrıca kopya `-O2` denetim imajı (ayrı derlenmiş, ayrı bağlanmış) ortak 142 seride ürünle sıcak kipte en çok 1 çevrim, soğuk kipte en çok 17 çevrim (%0,03; kod adreslerinin farklı olmasıyla açıklanır, çıkarım) farklıdır (`fark.py`). Ölçüm bu koşullarda belirlenimcidir; ölçüm yönteminin kendi saçılımı görülmedi.

## 5. Bellek

| Kalem | Değer | Kaynak |
|---|---:|---|
| Kitaplık kodu, bağlanmış imajda | 1 480 B: `ida_link_al` 812, `ida_link_gonder` 376, `ida_link_coz` 132, `crc_hesapla` 86, `ida_link_alici_kur` 44, `ida_link_gonderici_kur` 30 | `nm -S` (`boyut.sh`) |
| Kitaplık kodu, arşivin tamamı | 1 624 B (`ida_link_mesaj_bilgisi` 96 ve `ida_link_alici_tasma` 44 bu imajda kullanılmadığı için bağlanmadı) | `size -A libida_link.a` |
| Salt okunur veri | 444 B (mesaj tablosu `TABLO`, 37 satır × 12 B) | aynı |
| Değişebilir genel veri (.data/.bss) | 0 B | aynı |
| `sizeof(ida_link_alici_t)` | 324 B | kart açılış satırı |
| `sizeof(ida_link_gonderici_t)` | 2 B | aynı |
| `sizeof(ida_link_cerceve_t)` | 264 B | aynı |
| Statik yığın (`-fstack-usage`) | `ida_link_al` 88 B, `ida_link_gonder` 40 B, `ida_link_coz` 32 B, `crc_hesapla` 12 B | depo derlemesinin `tel.c.su`'su |
| Ölçülen yığın derinliği (main'in SP'sinden; ölçüm işlevi + kitaplık) | `ayr_tek` 148 B, `ayr_p64` 156 B, `ayr_bayt` 140 B, `ayr_alcoz` 156 B, `kodla` 116 B | kartta yığın boyama (Y satırı); üst sınırdır, ölçüm işlevinin çerçevesini içerir |

W2-protokol raporundaki boyut değerleri bu çalışma kopyasında bulunmuyor (rapor mimara iletildi, depoda dosyası yok); bu yüzden "W2-protokol raporundaki değerlerin bu derlemede doğrulanması" yapılamadı. Yukarıdaki değerler aynı ön ayarın (`fw-release`) derlemesidir ve karşılaştırmayı mimar yapabilir.

## 6. İyileştirme denemeleri

E1 ve E2 eşikleri aşıldığı için görev belgesindeki iki deneme, ürün kaynağının prototip içindeki kopyası üzerinde ölçüldü (`tools/olcum/mp5/kopya/`; ürün kitaplığına dokunulmadı). Kopyalar ürünün `fw-release` bayraklarıyla derlenir (yalnız eniyileme düzeyi değişir; derleme komutu `ninja -t commands` ile karşılaştırıldı); ölçüm yazılımı, akışlar ve doğrulama aynıdır; deneme koşularında ICACHE yalnız açık (sıcak ve soğuk) koşuldu. Her koşu `ozet.py denetle`'den geçti (kopyaların ürettiği çerçeveler ve kodlama çıktıları host referansıyla aynı).

- **kopya-O2** (denetim): `tel.c`'nin bayt bayt aynı kopyası, `-O2`. Ürün kitaplığıyla aynı sonucu vermeli; verdi (142 ortak seride en büyük fark sıcakta 1, soğukta 17 çevrim; doğrusal uyum aynı). Kopya düzeneğinin kendisinin fark yaratmadığı böylece gösterildi.
- **kopya-Os:** aynı kopya, `-Os`.
- **tablo-O2:** kopyada yalnız `crc_hesapla` 256 girdili tabloyla (`CRC_TABLO`, 512 B salt okunur veri; "123456789" → 0x6F91 ve 100 000 rastgele (crc, bayt) çiftinde `crc_accumulate` ile aynı olduğu host'ta denetlendi), `-O2`.

| Seri (net en kötü, ICACHE açık) | Ürün `-O2` | kopya `-O2` | kopya `-Os` | tablolu CRC `-O2` |
|---|---:|---:|---:|---:|
| E1 ayrıştırma, tek seferde | 0,5562 ms | 0,5562 ms | 0,7676 ms (+%38,0) | 0,5044 ms (−%9,3) |
| E1 ayrıştırma, 64 B | 0,5664 ms | 0,5664 ms | 0,7765 ms (+%37,1) | 0,5138 ms (−%9,3) |
| E1 ayrıştırma, bayt bayt | 1,0938 ms | 1,0938 ms | 1,2230 ms (+%11,8) | 0,9919 ms (−%9,3) |
| E1 ayrıştırma + çözme | 0,6387 ms | 0,6387 ms | 0,8534 ms (+%33,6) | 0,5869 ms (−%8,1) |
| PEN ayrıştırma, tek seferde | 0,5693 ms | 0,5693 ms | 0,8137 ms (+%42,9) | 0,5100 ms (−%10,4) |
| PEN ayrıştırma, bayt bayt | 1,1868 ms | 1,1869 ms | 1,3366 ms (+%12,6) | 1,0698 ms (−%9,9) |
| KS kodlama | 0,2118 ms | 0,2118 ms | 0,3171 ms (+%49,7) | 0,1477 ms (−%30,3) |
| BUY ayrıştırma (258 B) | 80,6 µs | 80,6 µs | 134,6 µs (+%67,0) | 68,9 µs (−%14,5) |
| BUYK kodlama (258 B) | 39,1 µs | 39,1 µs | 60,0 µs (+%53,2) | 27,5 µs (−%29,7) |
| **E1 (tek seferde) eşik 0,20 ms** | geçmedi | geçmedi | geçmedi | **geçmedi** |
| **E2 (tek seferde) = PEN + KS, eşik 0,50 ms** | 0,7810 ms, geçmedi | 0,7811 ms, geçmedi | 1,1308 ms, geçmedi | **0,6578 ms, geçmedi** |
| Doğrusal uyum, ayrıştırma (`a` + `b`·bayt) | 2 110 + 40,97 | 2 110 + 40,97 | 2 026 + 74,97 | 2 106 + 32,96 |
| Doğrusal uyum, kodlama | 189 + 22,97 | 189 + 22,97 | 172 + 35,97 | 196 + 14,97 |
| Kitaplık kodu / salt okunur veri (bağlanmış imaj) | 1 480 B / 444 B | 1 480 B / 444 B | 1 204 B / 444 B | 1 478 B / 956 B |

Sonuçlar:

- **Tablolu CRC** bayt başına ayrıştırmada 8,0, kodlamada 8,0 çevrim kazandırır (uyum eğimlerinin farkı; CRC'nin bayt başına maliyetinin kendisi ayrıca ölçülmedi); çerçeve başına sabit değişmez. Ayrıştırma %8–15, kodlama %30 kısalır; bedeli 512 B flash'tır (kod boyu aynı kalır: `crc_hesapla` satır içine alındı, `ida_link_al` 812 → 880 B, `ida_link_gonder` 376 → 392 B). **Tek başına E1'i ya da E2'yi eşiğin altına indirmez.**
- **`-Os`** kodu 276 B küçültür ama her şeyi yavaşlatır (%12–67): bu düzeyde `crc_accumulate` ayrı işlev olarak bırakılır ve bayt başına çağrılır (`nm`: 34 B'lık yerel `crc_accumulate`; ayrıştırmada bayt başına 41 → 75 çevrim). Ürünün `-O2` seçimi bu ölçümle desteklenir.

**Yalnız yazılan öneriler (uygulanmadı, ölçülmedi; kazanç tahminleri ölçülen modelden ve sökümden türetilmiş tahmindir):**

1. *Teslim edilen çerçevenin yük kuyruğunun sıfırlanması.* `cerceve_bitir` her çerçevede `yuk[len, 255)`'i sıfırlar ve newlib-nano'nun `memset`'i bayt bayt döngüdür; ayrıştırmanın çerçeve başına ≈ 2 110 çevrimlik sabitinin büyük kısmı buradan gelir (Bölüm 3.2; tahmin ≈ 1 500–1 800 çevrim). Sıfırlamanın `yuk_azami`'ye kadar yapılması ya da `ida_link_coz`'a bırakılması çerçeve başına bunun çoğunu kaldırır; E1'de 22 çerçeve × ≈ 1 500 ≈ 33 000 çevrim ≈ 0,2 ms (tahmin). **Dikkat:** `tel.h` bugün "yuk[len, IDA_LINK_YUK_AZAMI) sıfırdır" der; bu bir API değişikliğidir ve W2-protokol'ün işidir.
2. *Sözcük genişliğinde `memcpy`/`memset`.* newlib-nano'nun ikisi de bayt döngüsüdür (söküm: `strb` döngüsü); kart yazılımının libc seçimi ya da kitaplık içinde sözcük genişliğinde kopyalama hem ayrıştırmanın kopyasını hem `ida_link_coz`'u (çözmenin ek %15–25'i) kısaltır.
3. *Bayt başına durum makinesi.* Başlık doğrulandıktan sonra yük ve CRC baytları her baytta durum dalından, örnek alanlarının bellekten yeniden okunmasından (uint8_t tampon yazımı derleyiciye takma ad olasılığı bırakır; sökümde her baytta `ozel_dolu`, `ozel_durum` yeniden yüklenir) ve ayrı bir CRC geçişinden geçer. Kalan baytları eldeki parçadan toplu kopyalamak ve CRC'yi aynı geçişte hesaplamak bayt başına maliyeti düşürür (ne kadar düşüreceği ölçülmedi).
4. *Besleme biçimi.* Bayt bayt besleme (ör. her UART kesmesinde bir bayt) maliyeti iki katına çıkarır (≈ +80 çevrim/bayt); W3'ün alım yolu baytları parça parça (DMA halkasından) vermelidir. 64 B'lık parçalar tek seferde beslemeye göre yalnız %2 ekler.
5. Tablolu CRC (ölçüldü, yukarıda) öteki önerilerle birlikte düşünülebilir; birleşik etkisi ölçülmedi.

Bu önerilerin hiçbiri bu işte uygulanmadı; eşiğin bunlarla sağlanıp sağlanmayacağı ölçülmedi. Yedek biçim (iskelet §8, Ö2) ayrı karardır ve bu işte yazılmadı.

## 7. Ölçülmeyenler ve sınırlar

- **Gerçek alım yolu ölçülmedi:** UART alımı, DMA, kesme, halka tampon ve gerçek bağlantı üzerinden uçtan uca süre bu işin kapsamı dışındadır (W3, W4). Akışlar SRAM'den, kesmesiz beslendi; gerçek yolda kesmelerin, DMA'nın ve önbellek çakışmasının ek maliyeti yoktur.
- **Çekirdekle birlikte önbellek durumu:** soğuk kip ICACHE'in tamamen boşaltıldığı durumu ölçer; gerçek çekirdek adımıyla aynı ICACHE'i paylaşmanın etkisi bunun içinde kalmalıdır (ICACHE'in tamamen boş olması en kötü durumdur; doğrulanmadı). Kapalı ICACHE yalnız bilgi içindir.
- **Flash bekleme durumu:** flash'tan kod alımı 4 bekleme durumu ve önden getirme iledir (ürün yapılandırması varsayıldı; W3 değiştirirse sonuç değişir).
- **A1 ↔ A4 dönüştürücüsü** yoktur, maliyeti ölçülmedi. `ida_link_coz` ölçüldü (tipli yapıya kopya); A4 alan eşlemesi bunun üstüne eklenecektir.
- **Hatalı girdi:** akışlarda bozuk çerçeve, yeniden eşleme, MAVLink 1 baytı ya da taşma yoktur; bu yolların maliyeti ölçülmedi.
- **Akış karışımları:** E1 ve PEN A1'in pencere planına uyan, mimarın tanımına göre bu işte seçilen karışımlardır; başka karışımın maliyeti doğrusal modelden (Bölüm 3.2) hesaplanabilir, ölçülmedi. KS, E19'un sınıf paylarının tam sayıya yukarı yuvarlanmasıdır; tek periyotta sim komut kovasının patlaması (E17) daha çok komut sonucu üretebilir, bu durumda kodlama süresi modelden ölçeklenir (hesap).
- **Windows ve SIL'de maliyet** ölçülmedi (kapsam dışı).

## 8. Yeniden üretim

Ham kayıtlar `tools/olcum/mp5/veri/` altındadır (git'in yok saydığı dizin; depoya girmez). Komutlar depo kökünden:

```sh
tools/olcum/mp5/derle.sh o2 kisa kopya-o2 kopya-os tablo-o2   # ürün kitaplığı (fw-release), akış üretimi, imajlar
tools/olcum/mp5/kos.sh o2 3600                                 # yükle + kaydet + denetle (ham kayıt veri/kos/)
tools/olcum/mp5/kampanya.sh kopya-o2 kopya-os tablo-o2 o2      # ardışık koşular
python3 -I tools/olcum/mp5/ozet.py denetle <kayıt> --kimlik <K> --beklenen <kayıt>-beklenen.json
python3 -I tools/olcum/mp5/ozet.py tablo <koşu1> <koşu2> --kimlik <K1> --kimlik2 <K2>
python3 -I tools/olcum/mp5/ozet.py deneme <ürün> <kopya-o2> <kopya-os> <tablo-o2> --kimlik <K0>,<K1>,<K2>,<K3>
python3 -I tools/olcum/mp5/fark.py <kayıt1> <K1> <kayıt2> <K2>  # iki kaydın en büyük değer farkı
tools/olcum/mp5/boyut.sh o2 kopya-o2 kopya-os tablo-o2
```

Bu raporun koşuları (`veri/kos/` altında; her koşunun ham kaydı `<ad>.txt`, beklenen referans `<ad>-beklenen.json`, yükleme günlüğü ve koşu özeti `<ad>-kos.txt`):

| Koşu | Derleme kimliği | İmaj SHA-256 (ilk 16) | Seri | Denetim |
|---|---|---|---:|---|
| `o2-20261120-113552` (koşu 1) | `20261120-113532-o2-7b23157d` | `9136f5b0897afa46` | 176 | geçti |
| `kopya-o2-20261120-114633` | `20261120-113532-kopya-o2-5389cfe0` | `665111dd4703a1a5` | 142 | geçti |
| `kopya-os-20261120-115220` | `20261120-113532-kopya-os-7e875f63` | `c2a2b22964d5b0cb` | 142 | geçti |
| `tablo-o2-20261120-115934` | `20261120-113532-tablo-o2-4198e698` | `4322c24d05aa5c7a` | 142 | geçti |
| `o2-20261120-120451` (koşu 2) | `20261120-113532-o2-7b23157d` | `9136f5b0897afa46` | 176 | geçti |

Üretilen verinin kimliği (`MP5_VERI_KIMLIK`, bütün koşularda aynı) `0x4F920EDC`; kartın açılış satırı ve `beklenen.json` bunu taşır, denetim eşitliğini sınar. Ayrıca düzeneğin ilk denemesi kısa derlemeyle (200 yineleme) koşuldu ve denetimden geçti (sayılar raporda kullanılmadı).

Ön koşullar: ST kaynakları `$IDA_CACHE/src` altında (MP1 `kaynak-indir.sh`, main'de), STM32CubeIDE paketindeki GNU Tools for STM32 14.3.rel1 ve STM32CubeProgrammer (yollar ortam değişkeniyle ezilebilir; `ortam.sh`), geçerli fabrika yedeği (`yedek-dogrula.sh`; yükleme yedeksiz yapılmaz). Prototip MP4a'dan (`tools/olcum/mp4/`, main'de) şunları kopyalar: saat ve UART kurulumu, kesme işleyicileri, `ortam.sh`, `yukle.sh`, `kos.sh` ve seri kayıt işlevinin düzeni; MP1a'dan araç zinciri dosyası ve `yedek-dogrula.sh`.

## 9. İyileştirme denemeleri, ikinci tur: iyileştirilmiş tel katmanı

Mimarın sorusu (2026-11-20): iyileştirilmiş bir uygulama E1 ≤ 0,20 ms ve E2 ≤ 0,50 ms'yi sağlayabiliyor mu? Yöntem, akışlar, eşik tanımı ve derleme bayrakları Bölüm 1–3 ile aynıdır; en kötü değer yine sıcak ve soğuk ICACHE kiplerinin ve üç alan deseninin en kötüsüdür (bu turda ICACHE kapalı kip koşulmadı). Ürün kitaplığı değiştirilmedi.

### 9.1 Taban ve düzenek

- **Ürünün güncel kaynağı:** `origin/is/w2-protokol` @ `0fa569e` (aday dışı `0xFE`'nin hem `rx_mav1`'e hem `rx_resync_bytes`'a sayılması dahil). Dalın tabanı (`5b34de8`) değiştirilmedi; güncel kaynak, scratch altında açılan geçici bir çalışma ağacından kullanıldı (`MP5_URUN_KOK`), orada `fw-release` ve `host-debug` derlendi (host'ta ürünün 21 protokol testi geçti).
- **Ürün tabanı yeniden ölçüldü** (`urun2`, `0fa569e`'nin `fw-release` kitaplığı): E1 0,5562 ms (Bölüm 3 ile aynı), E2 0,7809 ms (Bölüm 3'te 0,7810; KS kodlaması 23 çevrim kısa, `crc_hesapla`'nın argüman sırasının değişmesiyle açıklanır, çıkarım).
- **Kopya:** `tools/olcum/mp5/kopya/tel_iyi.c`, `0fa569e`'nin `tel.c`'sinden alındı. Her iyileştirme bir derleme bayrağıdır (`MP5_K`, `MP5_W`, `MP5_B`, `MP5_CRC`, `MP5_E`, `MP5_H`; anlamları dosyanın başında); bütün bayraklar kapalıyken (`d0`) davranış ve yapı ürünle aynıdır. Çeşitler `kopya/cesitler.txt`'tedir; kopya ürünün `fw-release` bayraklarıyla, `-O2` derlenir.
- **Denetim çeşidi `d0`** üründen %0,4–3,6 hızlıdır (E1 0,5540, E2 0,7698 ms). Bunun nedeni, kodun bayraklara bölünürken yeniden düzenlenmesidir (CRC döngüsünün ayrı işleve alınması, teslimin ortak işlevde toplanması). Aşağıda "tek başına" kazançlar `d0`'a göre verilmiştir.
- Kartta her çeşit ürünün host'ta ürettiği referansla doğrulandı: teslim edilen çerçevelerin (yükün `[0, yuk_azami)` kısmı) ve çözülen yapıların FNV-1a'sı, alıcı sayaçları, kodlama çıktısı. 17 koşunun hepsi `ozet.py denetle`'den geçti.

### 9.2 Adım adım sonuçlar (kartta ölçülen; net en kötü, ICACHE açık, 10 000 yineleme)

Kitaplık boyu bağlanmış imajdan (`boyut.sh`). Ürün: 1 480 B kod, 444 B salt okunur veri. RAM: örnek yapılarının boyu hiçbir çeşitte değişmez (alıcı 324 B, gönderici 2 B, çerçeve 264 B); yazılabilir genel veri 0 B.

**Tek başına** (her biri `d0` üzerine tek bayrak):

| Çeşit | Değişiklik | E1 tek seferde | E2 tek seferde | Ayrıştırma `a` + `b`·B (çevrim) | Kodlama `a` + `b`·B | Kod / salt okunur (B) |
|---|---|---:|---:|---|---|---|
| ürün `0fa569e` | — | 0,5562 ms | 0,7809 ms | 2 110 + 40,97 | 188 + 22,97 | 1 480 / 444 |
| `d0` | denetim (bayraklar kapalı) | 0,5540 | 0,7698 | 2 142 + 39,96 | 193 + 21,97 | 1 550 / 444 |
| `k` | 1. kuyruk sıfırlaması `yuk_azami`'ye kadar | 0,3432 (−%38,0) | 0,5889 (−%23,5) | **275** + 46,96 | 193 + 21,97 | 1 546 / 444 |
| `w` | 2. sözcük genişliğinde kopya/sıfırlama | 0,3630 (−%34,5) | 0,5790 (−%24,8) | 608 + 42,88 | 252 + 18,50 | 1 808 / 444 |
| `b` | 3. gövde blok hâlinde, CRC tek geçişte | 0,4244 (−%23,4) | 0,6117 (−%20,5) | 2 440 + **13,96** | 189 + 21,97 | 2 054 / 444 |
| `t256` | 4a. CRC 256 girdili tablo | 0,5055 (−%8,8) | 0,6599 (−%14,3) | 2 114 + 32,96 | 203 + 14,97 | 1 518 / 956 |
| `t16` | 4b. CRC 16 girdili yarım bayt tablosu | 0,5505 (−%0,6) | 0,7666 (−%0,4) | 2 115 + 39,96 | 190 + 21,97 | 1 618 / 476 |
| `e` | 5. kodlama: sözcük sözcük kırpma taraması, kopya ve CRC aynı geçişte | 0,5542 (0) | 0,7290 (−%5,3) | 2 096 + 40,96 | 244 + 15,97 | 1 666 / 444 |
| `h` | 6. kimlikten doğrudan dizin (100 B), `rx_bytes` çağrı başına bir kez, `ida_link_coz` yapıyı bir kez yazar | 0,5310 (−%4,2) | 0,7379 (−%4,1) | 2 067 + 37,99 | 135 + 22,00 | 1 454 / 544 |

**Birikimli** (görev belgesindeki sırayla):

| Çeşit | E1 tek | E1 64 B | E1 bayt bayt | E1 tek + çözme | PEN tek | KS kodlama | E2 tek | Ayrıştırma `a` + `b`·B | Kodlama `a` + `b`·B | Kod / salt okunur (B) |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| `k` | 0,3432 | 0,3530 | 0,8564 | 0,4257 | 0,3850 | 0,2040 | 0,5889 | 275 + 46,96 | 193 + 21,97 | 1 546 / 444 |
| `kw` | 0,3310 | 0,3409 | 0,8443 | 0,3691 | 0,3688 | 0,1844 | 0,5532 | 338 + 43,74 | 252 + 18,50 | 1 808 / 444 |
| `kwb` | 0,2038 | 0,2201 | 0,8976 | 0,2419 | 0,2133 | 0,1837 | **0,3971** | 641 + 17,99 | 249 + 18,50 | 2 324 / 444 |
| `kwbt256` | **0,1661** | **0,1843** | 0,8479 | 0,2042 | 0,1699 | 0,1291 | **0,2990** | 644 + 12,00 | 263 + 11,50 | 2 180 / 956 |
| `kwbt16` (4b yerine) | 0,2149 | 0,2310 | 0,8896 | 0,2530 | 0,2265 | 0,1842 | 0,4107 | 625 + 19,99 | 250 + 18,50 | 2 376 / 476 |
| `kwbt256e` | 0,1661 | 0,1844 | 0,8479 | 0,2043 | 0,1699 | 0,1142 | 0,2841 | 644 + 12,00 | 252 + 9,97 | 2 236 / 956 |
| **`kwbt256eh`** (en iyi) | **0,1531** | **0,1710** | 0,8168 | **0,1770** | 0,1576 | 0,1065 | **0,2641** | 547 + 12,02 | 190 + 10,00 | 2 136 / 1 056 |
| `kwbt16eh` | 0,2007 | 0,2165 | 0,8583 | 0,2247 | 0,2129 | 0,1754 | 0,3884 | 519 + 20,02 | 159 + 19,00 | 2 344 / 576 |
| `wbt256eh` (1. madde olmadan) | **0,1841** | 0,2020 | 0,8478 | 0,2081 | 0,1823 | 0,1064 | **0,2887** | 809 + 11,16 | 190 + 10,00 | 2 126 / 1 056 |

Okuma:

- **Hedefe ilk ulaşılan nokta `kwbt256`'dır** (madde 1 + 2 + 3 + 4a): E1 0,1661 ms, E2 0,2990 ms (tek seferde ve 64 B'lık parçalarla). E2 tek başına `kwb`'de sağlanır (0,3971 ms), E1 orada 0,2038 ms'dir (sınırın %2 üstü).
- Madde 5 ve 6 eklenince en iyi birleşim `kwbt256eh`: E1 0,1531 ms (eşiğin %77'si), E2 0,2641 ms (%53). Çözme dahil E1 de sağlanır (0,1770 ms).
- **Madde 1 (açık arayüz değişikliği) olmadan da eşikler sağlanır:** `wbt256eh` E1 0,1841 ms (%92), E2 0,2887 ms. Madde 1 olmadan pay küçüktür ve 64 B'lık parçalarla E1 0,2020 ms'dir (sınırın %1 üstü).
- **Yarım bayt tablosu bu çekirdekte kazanç getirmez:** tek başına %0,6; birikimli zincirde bit işlemli CRC'den yavaştır (`kwbt16` 0,2149 > `kwb` 0,2038). 256 girdili tablo bayt başına 6 çevrim kazandırır (ayrıştırma `b` 18,0 → 12,0).
- **Bayt bayt besleme hiçbir çeşitte eşiği sağlamaz** (en iyisi 0,8168 ms). Her `ida_link_al` çağrısının sabit maliyeti en iyi çeşitte bayt başına ≈ 101 çevrimdir (hesap: (130 694 − 24 498) / 1 050). W3'ün alım yolu baytları parça parça vermelidir; 64 B'lık parçalar yeterlidir.
- E3 (en iyi çeşit, 258 B'lık çerçeve): ayrıştırma 25,5 µs, 64 B'lık parçalarla 31,1 µs, çözme dahil 29,0 µs, bayt bayt 202,5 µs; kodlama 19,4 µs.

### 9.3 Son durumda eşik tablosu (`kwbt256eh`)

| # | Büyüklük | Ölçülen en kötü | Eşik | Sonuç |
|---|---|---:|---:|---|
| E1 | 1 050 B ayrıştırma, tek seferde | 0,1531 ms (24 498 çevrim) | ≤ 0,20 ms | **geçti** |
| E1 | 64 B'lık parçalarla | 0,1710 ms | ≤ 0,20 ms | **geçti** |
| E1 | tek seferde + `ida_link_coz` | 0,1770 ms | ≤ 0,20 ms | **geçti** |
| E1 | bayt bayt | 0,8168 ms | ≤ 0,20 ms | geçmedi |
| E2 | PEN ayrıştırma (tek seferde) + KS kodlama | 0,1576 + 0,1065 = 0,2641 ms | ≤ 0,50 ms | **geçti** |
| E2 | 64 B'lık parçalarla / çözme dahil | 0,2865 / 0,2882 ms | ≤ 0,50 ms | **geçti** |
| E2 | bayt bayt | 1,0393 ms | ≤ 0,50 ms | geçmedi |
| E3 | 258 B: ayrıştırma / kodlama | 25,5 / 19,4 µs | raporlanır | — |

### 9.4 Kalan maliyet ve sınır

En iyi çeşitte ayrıştırma çerçeve başına 547 çevrim (3,4 µs) + bayt başına 12,0 çevrim (0,075 µs); kodlama çerçeve başına 190 + bayt başına 10,0 çevrim. Dökümün ölçülen kısmı adımların farklarıdır; işlev düzeyinde ayrı ölçüm yapılmadı.

- **Bayt başına (ayrıştırma, 12,0 çevrim):** 256 girdili tabloyla CRC, gövde baytlarının blok kopyası ve yükün çerçeveye sözcük kopyası. Tablo CRC'nin payı ölçülmedi. Tahmini (sökümden): bayt başına bir yükleme, bir tablo okuması ve iki işlem; her bayt bir önceki CRC'ye bağlı olduğu için sıralı bir zincirdir, yaklaşık 6–8 çevrim. Bu zincir, bayt başına maliyetin alt sınırını belirler.
- **Çerçeve başına (547 çevrim):** başlığın 10 baytı hâlâ bayt bayt durum makinesinden geçer; çağrıya giriş ve dönüş, başlık denetimi, `cerceve_teslim` ve yükün `yuk_azami`'ye kadar sıfırlanması da buradadır. Ölçülen farklar:
  - madde 1 çerçeve başına ≈ 1 870 çevrim kaldırır (`d0` 2 142 → `k` 275);
  - madde 3, kuyruk sıfırlaması kalırken çerçeve başına ≈ 300 çevrim ekler, bayt başına ≈ 26 çevrim kazandırır;
  - madde 6 çerçeve başına ≈ 100 çevrim kazandırır (`kwbt256e` 644 → `kwbt256eh` 547).
- **Ne sınır koyuyor:** tek seferde ve parçalı beslemede CRC zinciri ile başlığın bayt bayt işlenmesi; bayt bayt beslemede `ida_link_al`'ın çağrı başına sabit maliyeti (≈ 100 çevrim). E1 akışı 22 çerçevedir; en iyi çeşitte süre ≈ 22 × 547 + 1 050 × 12 ≈ 24 600 çevrimdir (hesap; ölçülen 24 498).

### 9.5 Davranış eşitliğinin kanıtı

Ürünün birim testleri (`protocol/ida_link/test/test_tel.c`) ve bağımsız kabul testleri (`tests/kabul/protokol/`, `0fa569e`) prototipin kendi derleme dizininde (`tools/olcum/mp5/test.sh`) ürün kaynağı yerine her çeşit bağlanarak derlendi ve koştu; test dosyalarına dokunulmadı. Ayrıca yazılan farksal sınama (`pc/mp5_fark_test.c`), ürün kaynağını ve çeşidi aynı programa bağlar (çeşidin açık işlevleri `v_` önekiyle). 20 000 senaryoda iki tarafa aynı girdi verilir; bir çeşit için ≈ 25,6 milyon karşılaştırma yapılır. Karşılaştırılanlar:

- her `ida_link_al` dönüşü, `*tuketilen`, teslim edilen çerçevenin bütün alanları ve yükü, her çağrıdan sonra bütün sayaçlar;
- `ida_link_alici_tasma`;
- `ida_link_gonder`'in dönüşü, yazılan baytlar ve seq;
- `ida_link_coz` ve `ida_link_mesaj_bilgisi`.

Senaryolar: geçerli çerçeveler (iki yön), bozuk CRC, yanlış kimlik, `incompat_flags`, sıfır ve aşırı `len`, dialect dışı kimlik, yanlış yön, kesik çerçeve, `0xFD`/`0xFE` dahil gürültü, 1–300 baytlık rastgele parçalar; gönderici için rastgele kırpma, yanlış boy ve dar tampon.

Sonuç (`veri/test-20261120-123839.txt`; 228 test):

| Çeşit | Birim (1) | Kabul (11) | Farksal sınama |
|---|---|---|---|
| ürün kaynağı (`urun`) | geçti | 11 / 11 geçti | — |
| `d0`, `w`, `b`, `t256`, `t16`, `e`, `h`, `wbt256eh` (madde 1'siz) | geçti | 11 / 11 geçti | geçti, 0 fark |
| `k`, `kw`, `kwb`, `kwbt256`, `kwbt16`, `kwbt256e`, `kwbt256eh`, `kwbt16eh` (madde 1'li) | **geçmedi** | 2 / 11 geçti (`k08`, `sayac`) | geçti, 0 fark (yük `[0, yuk_azami)` karşılaştırılarak) |

**Madde 1 yüzünden kırılan testler** (hepsi aynı beklentiden; testler değiştirilmedi):

- **Birim testi:** `test_tel.c:440` `test_k05_kirpilmis_yuk_sifirla_doldurulur`. Beklenti: `yuk[1, 255)` sıfır. Çeşit `yuk[246, 255)`'i yazmaz; test bu kısmı 0x5A ile ön doldurduğu için ilk farkta durur ("Expected 0 Was 90").
- **Kabul testleri:** teslim edilen her çerçevede çağrılan ortak denetim, `kabul_ortak.c` satır 294–303 (`kabul_besle`). Beklenti: "yuk[len, 255) sıfır" (açıklamasında "başlık: yuk[len, 255) sıfır"). Bu denetim çerçeve kabul eden her test işlevini ilk teslimde durdurur. Etkilenenler:
  - `k02` (`c`, `d`), `k04` (`c`), `k05` (`cde`), `k06` (`cd`);
  - `k07` (`a`, `b`, `cd`, `c`, `e` ×2), `ek_b` (`2_3_4`, `5`, `6`, `7`);
  - `saglamlik` (`a`, `c`, `d`), `k101` (`a` ×2, `bc`);
  - `belirsiz` (`B1`, `B2`, `B3`, `B8`).
  - Çeşit başına 26 test işlevi.
- Kırılan beklentilerin hepsi yalnız `[yuk_azami, 255)` aralığıyla ilgilidir. Bu, görev belgesinin madde 1'deki tanımıdır. Bu testler ilk farkta durduğu için madde 1'li çeşitlerde öteki iddialarına ulaşamaz. Bu yüzden eşitlik iki yoldan gösterildi:
  - (a) madde 1 dışındaki bütün iyileştirmeleri içeren `wbt256eh` bütün birim ve kabul testlerini geçer;
  - (b) farksal sınama madde 1'li sekiz çeşitte de, yükün `[0, yuk_azami)` kısmı dışında hiçbir fark bulmaz.
- **Farksal sınamanın duyarlılığı:** madde 1'li her çeşit yükün tamamı (255 B) karşılaştırılarak sınandığında fark bulunur (`*.fark.deneme_yuk_tamami_farkli`, `WILL_FAIL`; sekizi de bekleneni verdi).

### 9.6 Ürüne taşınacak değişiklikler (en iyi birleşim `kwbt256eh`; uygulanabilir liste)

Hepsi `protocol/ida_link/src/tel.c` içindedir; kaynak `tools/olcum/mp5/kopya/tel_iyi.c`'deki bayraklı kod. Ürüne bayraksız alınmalıdır.

1. **`cerceve_bitir` (kopyada `cerceve_teslim`): yük kuyruğunun sıfırlanması `memset(&c->yuk[len], 0, IDA_LINK_YUK_AZAMI − len)` yerine `yuk_azami − len` bayt.**
   - **Açık arayüz değişir:** `tel.h`'de `ida_link_cerceve_t.yuk` açıklaması "yuk[len, IDA_LINK_YUK_AZAMI) sıfırdır" yerine "yuk[len, yuk_azami) sıfırdır; [yuk_azami, IDA_LINK_YUK_AZAMI) yazılmaz (önceki içerik kalır)" olur. `ida_link_al`'ın "*cerceve yalnız … yazılır" sözü bu aralık için de geçerli kalır.
   - `ida_link_coz` yalnız `yuk_azami` bayt okuduğu için çözme sonucu değişmez.
   - Etkilenen testler 9.5'teki listedir (birim `test_k05_kirpilmis_yuk_sifirla_doldurulur`; kabul `kabul_besle` denetimi); W2-protokol ve kabul testi yazarı güncellemelidir.
   - İsteğe bağlıdır: madde 1 olmadan da eşikler sağlanır (`wbt256eh`), pay küçülür.
2. **Yerel sözcük genişliğinde `kopyala`/`sifirla`**: sabit boylu `memcpy(&w, p, 4)` ile, döngü kuyruğu döngüsüz, ISO C17, takma ad kuralına uygun. `cerceve_bitir`, `ida_link_gonder` ve `ida_link_coz`'daki `memcpy`/`memset` çağrılarının yerini alır. `objdump` ile ölçülen yolda kitaplık `memcpy`/`memset` çağrısı kalmadığı denetlendi (yalnız `ida_link_alici_kur`'da kalır). Açık arayüz değişmez.
3. **`ida_link_al`: başlık geçtikten sonra gövde blok hâlinde (`govde_isle`).**
   - Çerçeve parçada tamamlanmıyorsa eldeki baytlar tampona tek kopyayla alınır.
   - Tamamlanıyorsa CRC tampondaki kısım ve gelen parça üzerinden tek geçişte hesaplanır ve yük çerçeveye doğrudan iki kaynaktan kopyalanır (aday kapanınca durum boşta).
   - Başlık (ilk 10 bayt), yeniden tarama ve ret kuralları bayt bayt yolda aynen kalır.
   - Açık arayüz ve örnek yapısı değişmez.
4. **CRC: 256 girdili tablo** (`CRC_TABLO`, 512 B salt okunur; yansıtılmış 0x8408). `crc_hesapla`, alıcının tek geçişli CRC'si ve kodlayıcı bunu kullanır. Yarım bayt tablosu önerilmez (9.2).
5. **`ida_link_gonder`:**
   - sondaki sıfırlar önce sözcük sözcük taranır (`len >= 5` iken), sonra bayt bayt;
   - yük kopyası ve CRC aynı geçişte yapılır.
   - Açık arayüz değişmez.
6. **Kimlikten satıra doğrudan dizin:**
   - `satir_bul` ikili arama yerine 100 baytlık `DIZIN[msgid − 46000]` kullanır. Dizin üretilmiş tablo makrosundan derleme zamanında kurulur; her kimliğin 46000–46099 aralığında olduğu `_Static_assert` ile denetlenir.
   - `rx_bytes` çağrı başına bir kez toplanır.
   - `ida_link_coz` yapıyı bir kez yazar: önce `yuk_azami` bayt kopya, sonra varsa kalan sıfır.
   - Açık arayüz değişmez.

**Kaynak maliyeti:** kod 1 480 → 2 136 B (+656 B), salt okunur veri 444 → 1 056 B (+612 B: CRC tablosu 512 B, dizin 100 B); RAM'de örnek yapıları aynı. Statik yığın: `ida_link_al` 88 → 80 B, `cerceve_teslim` 32 B, `kopyala` 20 B. Kartta ölçülen derinlik (ölçüm işlevi + kitaplık): ayrıştırmada 148 → 180 B, kodlamada 116 → 108 B.

### 9.7 Ölçülmeyenler (ikinci tur)

- Ürüne taşındıktan sonraki hâl ölçülmedi (kopya ölçüldü); W2-protokol uygulamasının bayraksız yazımı ve kendi testleriyle yeniden ölçülmesi gerekir.
- İşlev düzeyinde ayrı süre (CRC zinciri, başlık yolu) ölçülmedi; 9.4'teki pay ayrımı adım farklarından ve sökümden türetilmiş tahmindir.
- Bayt bayt beslemenin çağrı maliyetini düşüren bir değişiklik denenmedi (ör. tek baytlık çağrı için kısa yol). Hedef tek seferde ve parçalı beslemede sağlandığı için durduruldu.
- `-O2` dışındaki eniyileme düzeyleri bu turda denenmedi.
- ICACHE kapalı kip, gerçek alım yolu (UART/DMA), çekirdekle birlikte önbellek paylaşımı: Bölüm 7'deki gibi ölçülmedi.
- MSVC ve Windows'ta testler koşulmadı (yalnız macOS host, Apple Clang).

### 9.8 Yeniden üretim (ikinci tur)

```sh
git worktree add --detach <scratch>/w2p origin/is/w2-protokol    # 0fa569e; orada: cmake --preset fw-release/host-debug + derle
export MP5_URUN_KOK=<scratch>/w2p
tools/olcum/mp5/derle.sh urun2 iyi-d0 iyi-k iyi-w iyi-b iyi-t256 iyi-t16 iyi-e iyi-h iyi-kw iyi-kwb \
    iyi-kwbt256 iyi-kwbt16 iyi-kwbt256e iyi-kwbt16eh iyi-wbt256eh iyi-kwbt256eh
tools/olcum/mp5/kampanya.sh <aynı liste>                             # kartta, ardışık
tools/olcum/mp5/adimlar.sh adimlar <aynı liste>                      # 9.2'nin tabloları
tools/olcum/mp5/boyut.sh -o <aynı liste>                             # kod / salt okunur boyları
tools/olcum/mp5/test.sh                                              # 9.5: birim, kabul ve farksal sınama
```

Koşuların ham kayıtları `veri/kos/<çeşit>-20261120-1231…1341*.txt` (17 koşu, her biri `denetle`'den geçti); test çıktısı `veri/test-20261120-123839.txt`.

**Kartın bırakıldığı durum (ikinci tur sonu):** dahili flash'ta MP5 ölçüm yazılımının `iyi-kwbt256eh` imajı var (`tools/olcum/mp5/derleme/iyi-kwbt256eh/mp5.elf`, SHA-256 `f72d218f768438ab9dcf997b72e2e5529aceec02a8fcadaf8509f740ced3b044`, kimlik `20261120-123123-iyi-kwbt256eh-0aef5b77`). Kampanyasını bitirdi (T satırı); her sıfırlamada yeniden koşar (≈ 3 dakika, yalnız seri çıktı). Seçenek baytlarına, koruma düzeyine ve önyükleyiciye dokunulmadı; her yüklemeden önce fabrika yedeği denetlendi. Bölüm "Özet"teki kart durumu ilk turun sonunu anlatır.
