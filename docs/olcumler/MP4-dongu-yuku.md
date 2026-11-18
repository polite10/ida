# MP4a — Kartta 400 Hz döngü yükü, yer tutucu çekirdek adımıyla (ön tur)

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP4a.md`; devralma eki `docs/gorevler/W0-MP4a-devralma.md` (ekin düzeltmeleri önceliklidir) |
| Ölçüm | M5 (ön bilgi; gerçek çekirdekle W3 sonunda yinelenir) |
| Uygulayan | `ida-uygulayici` (Opus); 2026-11-16 taslağı ve kodu 2026-11-17'de devralındı; 2026-11-18 sol ilk incelemesi düzeltmesi (`docs/gorevler/W0-MP4a-sol-duzeltme.md`, yanıt `docs/olcumler/inceleme/MP4-yanit.md`) ve ikinci, son düzeltme turu (`docs/gorevler/W0-MP4a-sol-duzeltme-2.md`, yanıt `docs/olcumler/inceleme/MP4-yanit-2.md`) |
| Dal | `olcum/mp4-dongu-yuku` |
| Tarih | 2026-11-16 (ilk düzenek), 2026-11-17 (düzenek düzeltmesi, O2 kampanyası), 2026-11-18 (sol ilk incelemesi düzeltmesi, Og kampanyası) |
| Tamamlanma | Zorunlu kampanyalar tamamlandı ve düzeltilmiş çözümleyiciyle geçerli: -O2 ve -Og, 400 Hz, A+B+C, her faz 241.000 adım, her fazın kayıt süresi alt sınırı ≥ 602,4975 s. Ek: 200 Hz yalnız-B koşuları (-O2, -Og) geçerli. Kabul ve kapanış sol'un ikinci incelemesi ve mimardadır. Çözümleyici yalnız histogram taşması içermeyen kampanyaları destekler (mimar kapsam kararı 2026-11-18); bütün gerçek kayıtlar bu kapsamda |
| Sonuç | D1 **geçmedi** (A'da kaçırılan 0; B'de 241.000/241.000), D2 **geçmedi** (B en büyük 3322,38125 µs > 1,0 ms), D3 **geçti**, D4 **geçti** (Bölüm 5). 200 Hz'te B: -O2 aşımsız (%66,4), -Og aşımlı (Bölüm 5) |

Bu rapor bir **ön bilgi** raporudur: ölçülen, gerçek çekirdek değil, 02'nin öngördüğü adımın hesap hacmini taklit eden yer tutucu bir adımdır (Bölüm 2 ve 6). Sonuçlar gerçek çekirdekle (W3 sonu) yinelenmeden kontrol hızı kararına kesin girdi sayılmaz. Bu ölçüm **taşımayı içermez**: bağlantı (A1, ST-LINK sanal seri portu) bu işte yoktur; seri port yalnız sonuç bildirimi içindir, "varıştan çıktıya" süresi MP3'ten sonra ölçülür. Geçme ölçütleri görev belgesinin dondurduğu hedeflerdir; ölçülmüş eşik değildir. Görev belgesindeki yazım hatası devralma ekinde düzeltilmiştir: **200 Hz periyodu 5 ms'dir** (2,5 ms 400 Hz'e aittir).

## 1. Ortam

| Kalem | Değer | Nasıl alındı |
|---|---|---|
| Makine | Apple M3 Pro, macOS 26.5.2 (25F84) | `sysctl`, `sw_vers` |
| Güç | 2026-11-17 ve 2026-11-18 koşularında AC bağlı (%47–49, "not charging"); 2026-11-17'deki ilk -Og denemesi hub ve AC birlikte koptuğunda kesildi (Bölüm 4.1); her koşu `caffeinate -dims` altında (koşu betiği kendini `caffeinate` altında yeniden başlatır, `pmset -g assertions` çıktısı kaydedilir) | her koşunun `*-analiz.txt` dosyasında başta ve sonda |
| Kart | STM32U5A9J-DK, ST-LINK portundan hub üzerinden; tek kablo (K17). ST-LINK FW V3J17M11 (değiştirilmedi) | `STM32_Programmer_CLI` günlüğü |
| Kart revizyonu | `DBGMCU_IDCODE` = `0x30006481` → `DEV_ID` `0x481`, **`REV_ID` `0x3000`** | aygıt yazılımının açılış satırı (`B2`) |
| Kart derleyicisi | GNU Tools for STM32 14.3.rel1 (`arm-none-eabi-gcc 14.3.1 20250623`), CubeIDE paketinden | `--version` |
| libc / libm | newlib-nano (`--specs=nano.specs --specs=nosys.specs`), MP1 ile aynı | bağlayıcı bayrakları |
| Yükleme | STM32CubeProgrammer CLI 2.23.0, `-c port=SWD -w … -v -rst`; her yüklemeden önce `tools/olcum/mp1/yedek-dogrula.sh` | `tools/olcum/mp4/yukle.sh` |
| Derleme sistemi | CMake 4.5.0, Ninja 1.13.2 | `--version` |
| PC aracı | Apple clang 17.0.0, C++20 | `c++ --version` |
| ST kaynakları | MP1 ile aynı (`STM32CubeU5 v1.9.0`, HAL V1.6.3, `cmsis-device-u5` v1.4.3); `${IDA_CACHE}/src` | MP1 raporu §1 |

Kart yapılandırması (açılış satırı, kartın kendi yazmaç okumaları): SYSCLK 160 MHz (`hesap_hz=160000000`, PLL1 yazmaçlarından), regülatör SMPS (`PWR_SVMSR` = `0x00038002`), flash gecikmesi 4 ve önden getirme açık (`FLASH_ACR` = `0x00000104`), ICACHE açık (`ICACHE_CR` = `0x00000005`), FPU açık (`CPACR` = `0x00F00000`), `FPSCR` = 0 (FZ kapalı, en yakına yuvarlama), tembel FP yığınlama açık (`FPCCR` = `0xC0000018`). Saat dizisi MP1'in düzeltilmiş dizisidir (AHB ÷2 → ≥ 5 µs → ÷1). TIM2 periyodu açılışta DWT ile denetlendi (iki ardışık güncelleme olayı arası çevrim): 400 Hz imajlarında beklenen 400.000, ölçülen 399.998 (-O2 ve kısa) ve 400.000 (-Og); 200 Hz imajlarında beklenen 800.000, ölçülen 799.998 (-O2-200) ve 800.001 (-Og-200). Çözümleyici ±16 çevrim kabul eder. Seçenek baytlarına, harici flash'a ve ST-LINK aygıt yazılımına dokunulmadı.

## 2. Yer tutucu adım

Kod: `tools/olcum/mp4/adim/` (ISO C17, `-std=c17 -pedantic-errors`, yalnız `float`; HAL/CMSIS içerme yolu yok), platform: `tools/olcum/mp4/fw/platform.c` (`gnu17`). Bütün C kodu `-ffp-contract=off -fno-fast-math` ile; adım kodu ayrıca `-Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Wshadow -Wvla` ile uyarısız derlenir (09 §2.6, 02 §2.7). Adım kodu devralmada değiştirilmedi (yer tutucunun algoritmaları optimize edilmedi); değişiklikler yalnız ölçüm düzeneği ve çözümleyicidedir.

Adım 02 §2.2'deki sırayı izler; her parçanın süresi DWT ile ayrı toplanır (Bölüm 4.4):

| Parça | İçerik |
|---|---|
| `mesaj_giris` | 300 B gelen mesajın CRC-16'sı (MAVLink'in `crc_accumulate`'i, CRC-16/MCRF4XX + CRC_EXTRA), alanların `memcpy` ile çözülmesi, kabul denetimi (sonluluk, zaman ilerlemesi), IMU ve sensör tamponlarına yazım |
| `nominal` | Ufuk nominal yayılımı (füzyon zamanlarında bölünmüş aralıklarla), çıkış öngörücüsü yayılımı, 80 kayıtlı çıkış geçmişine yazım ve **80 kaydın hepsine** konum, hız ve kuaterniyon düzeltmesi |
| `kovaryans` | Yoğun `Φ` (15×15) kurulumu, `P⁻ = Φ P Φᵀ + Q_d` (yoğun üçlü döngü, 2 × 3.375 çarpma-toplama) |
| `fuzyon` | Vektör ölçüm başına yoğun `H P`, `S = H P Hᵀ + R`, Cholesky ile kapı (`yᵀS⁻¹y`), `R` Cholesky'si ile beyazlatma, sıralı skaler güncellemeler (Joseph formunun açılmış hali, 02 §2.3.4) |
| `sifirlama` | Enjeksiyon, `G P Gᵀ` (yalnız δθ satır ve sütunları), simetrikleştirme, varyans alt sınırı; LDLᵀ denetimi (15×15) |
| `kip` | Tazelik, ufuk/çıkış durumu ve `P` köşegeni sonluluk denetimi, basit kip ve izin hesabı |
| `gudum` | LOS/ILOS (8 rota noktası), üçüncü mertebe derinlik referans modeli, irtifa tutma |
| `kontrol` | Kuaterniyon hatasıyla tutum dış döngüsü, derinlik ve konum dış döngüleri; açısal hız PID (3), dikey hız PI + yüzerlik, yatay hız PI + sönüm; `Rᵀ R_ψ` dönüşümü, doyma kalıntısıyla integral dondurma, rejim kazanç karışımı |
| `tahsis` | Öncelik grupları `[K,M] > [Z] > [N] > [X,Y]` ile `B⁺` (8×6, başlatmada) üzerinden sıralı doyma giderme, `τ_a = B u` ve kalıntı, 33 noktalı itki eğrisi tersinin ikili aramayla alınması, ölü bant ve PWM |
| `mesaj_cikis` | 100 B çıkış mesajının alan kopyalaması ve CRC-16'sı |

Girdiler adımın dışında, belirlenimci bir sözde ölçüm üretecinden (`yt_uret`, xorshift32) gelir. Derleyicinin hesabı silmemesi için her kayıtlı adımın 100 B çıktısı FNV-1a sağlama toplamına katılır ve faz sonunda bildirilir. Girdi dizisi yalnız toplam adım sırasına bağlıdır (zamanlamaya bağlı değildir); bu yüzden aynı faz yapısındaki iki imajın sağlama toplamları aynı olmalıdır. Gözlenen (Bölüm 4.5): 2026-11-16'daki eski -O2 imajı ile 2026-11-17'deki düzeltilmiş -O2 imajı A fazında aynı toplamı verdi (`0xC02B9132`); iki kısa imaj (eski ve yeni, faz başına 4.000 adım) üç fazda da aynı toplamları verdi. Önceki taslaktaki "aynı imajın iki koşusunda ve -O2 ile -Og arasında karşılaştırıldı" cümlesi yazıldığı anda kanıtsızdı (yalnız farklı imajlı iki kısa koşu vardı, -Og hiç koşmamıştı); -O2/-Og karşılaştırması bu raporda Bölüm 4.5'tedir. Aynı imajın iki uzun koşusu yapılmadı.

**Bilerek pahalı tutulanlar** (gerçek çekirdekte olmayabilir): bütün 15×15 işlemler seyreklik ve simetri kullanılmadan yoğun; ölçüm Jacobian'ı yoğun matris olarak çarpılır; kapı sonucu sayılır ama güncelleme her durumda yapılır; çıkış geçmişinin 80 kaydının hepsine kuaterniyon düzeltmesi uygulanır.

### 2.1 02'de yapılan yorum seçimleri

| Konu | 02'nin söylediği | Seçilen yorum |
|---|---|---|
| Kovaryans yayılım sıklığı | Birikmiş aralık 10 ms'ye ulaşınca ya da ufukta bir füzyondan hemen önce (02 §2.2, §2.3.2) | Görev belgesi "her adımda" der: her adım sonunda kalan birikimle bir yayılım **ve** her füzyon zamanından hemen önce bir yayılım. A ve B bu kuralla koşar (02'ye göre daha pahalı). C 02'nin kendi en kötü yığılma tanımıdır |
| Sabit gecikmeli ufuk ek hesabı | IMU tamponu 80 kayıt; ufuk `t_imu − D`'de (D = 150 ms); füzyon zamanında aralık bölünür; düzeltme `x_o(şimdi)` ve tampondaki kayıtlara uygulanır (02 §2.3.2) | Hepsi taklit edildi: 80 kayıtlı IMU halkası, sensör başına 64 kayıtlı bekleyen ölçüm halkası, ölçüm zamanında aralık bölme, ufukta füzyon, 80 kayıtlı çıkış geçmişinin her adımda tümüyle düzeltilmesi. "Yeniden yayılım" (geçmişten yeniden entegrasyon) 02'de yoktur; yapılmadı |
| Ölçüm boyutları | Manyetometre 02'de yalnız yön (1 skaler); GNSS yatay konum 2 + hız 2; DVL 3; basınç 1 (02 §2.3.3) | Görev belgesi manyetometreyi 3 skaler sayar; görev belgesi izlendi (3). GNSS 4 |
| Adım başına skaler güncelleme sınırı | En çok 8 (tahmin; 02 §2.3.2) | A ve B'de sınır yok: B'de her adımda 11 skaler (1 + 3 + 3 + 4), sınırı aşar. C'de sınır 8 |
| Füzyon zamanları | Ölçüm zamanına bağlı | A'da hızlar basınç 25, DVL 15, manyetometre 10, GNSS 5 Hz; gecikmeler basınç 20,3, DVL 50,7, manyetometre 10,9, GNSS 120,1 ms (seçim; hepsi D'den küçük) → her sensör ayrı füzyon zamanı |
| Enjeksiyon ve sıfırlama | Füzyondan sonra | Her füzyon zamanından sonra bir kez (B'de adım başına 4) |
| LDLᵀ denetimi | 100 Hz | A'da 100 Hz (4 adımda bir), B ve C'de her adım |
| Güdüm, dış döngüler | 10 Hz ve 100 Hz (tahmin) | A'da 02'deki hızlar; B ve C'de her adım |

**Yük durumları:**

| Faz | Tanım | Adım başına kovaryans yayılımı | Skaler güncelleme |
|---|---|---|---|
| A — olağan | Ölçümler 02'deki hızlarında; çoğu adımda yalnız IMU | 1 (adım sonu) + füzyon zamanı başına 1 | Gelen ölçüm kadar |
| B — en kötü (görev belgesi) | Her adımda dört sensörün hepsi, dört ayrı füzyon zamanında | 4 + 1 = 5 | 11 |
| C — 02 §2.2 en kötü yığılma tanımı (ek) | Her adımda basınç, DVL, GNSS **aynı** füzyon zamanında; manyetometre yok | 1 | 8 (1 + 3 + 4) |

C görev belgesinde istenmedi; 02'nin "kovaryans yayılımı 6.750 + 8 optimize skaler güncelleme + kapı + sıfırlama + LDLᵀ + nominal, kontrol, tahsis ≈ 16.900 → 0,32–0,63 ms" tahminini (02 §2.2) doğrudan sınamak için eklendi; zorunlu A/B'nin yerini tutmaz. D1 ve D2 hükümleri A ve B'ye göredir.

## 3. Düzenek

### 3.1 Önceki düzenekte bulunan sorunlar (2026-11-16) ve düzeltmeleri

Önceki uzun -O2 koşusu (`${IDA_CACHE}/mp4/kos/o2-20261116-011044-*`) **kabul kanıtı değildir**: çözümleyici çıkış kodu 1, sıra boşluğu 765 → 1219, `uart_tasma=454`; B fazının `F` özet satırı kayboldu; koşu 1840 s'lik sabit sürede kesildiği için C fazı hiç bildirilmedi. Ham veri korunuyor; yalnız A fazının sağlama toplamı karşılaştırma için kullanıldı.

| # | Sorun | Neden | Düzeltme |
|---|---|---|---|
| 1 | Ağır yükte satır kaybı (454 satır, B'nin `F` satırı dahil) | B'de adım (≈ 3,32 ms) periyottan (2,5 ms) uzun olduğundan ana döngü boşta yoklamaya hiç düşmedi; UART halkası doldu, satırlar atıldı. O imajda adım başına gönderim yoktu (adım sonrası `uart_pompa()` koşudan sonra eklenmişti; çalışan imaj `d69d48…` bu kodu içermiyordu) | Durum satırı (`D`) yalnız halkada en büyük satıra yetecek yer varsa üretilir; yoksa hiç üretilmez, sıra numarası tüketilmez, `d_atlanan` sayılır. Özet ve histogram satırları hiç atılmaz: yer açılana dek beklenir (yalnız kayıt dışı geçiş adımlarında). UART TX FIFO (8 B) açıldı; her adımdan sonra (ölçülen bölgenin dışında) FIFO doldurulur. Sonuç: sıra numarasında boşluk artık yalnız gerçek iletim kaybı anlamına gelir |
| 2 | Sabit koşu süresi ağır yükte fazı bitirmedi | B'de 241.000 adım ≈ 821 s sürer (602,5 s değil) | Kart fazları bir kez koşar ve sonunda `T` satırı basar; PC kaydı `T` gelince biter. Azami süre yalnız güvenlik sınırıdır; `T` gelmezse kayıt "EKSİK" (çıkış 4) ve koşu geçersizdir |
| 3 | Eksik faz başarılı sayılabiliyordu | Çözümleyici fazların varlığını ve adım sayısını denetlemiyordu | Çözümleyici açılış satırındaki faz maskesindeki her faz için `F, G, Q, S, E` satırlarının tam bir kez, `H/L/M` histogram satırlarının `E`'deki sayıyla bulunmasını, `n = faz_adim`, histogram toplamı + taşan = `n`, en küçük/en büyük değerin histogramın ilk/son kutusunda olmasını, toplam sürenin histogram sınırları içinde kalmasını, `T` satırını ve (koşu betiğinden) `n ≥ 240.000`, kayıt süresi ≥ 600 s olmasını ister; biri tutmazsa 1 döner |
| 4 | Tetik gecikmesi histogramı taştı; yüzdelikler `4294967295` (anlamsız) çıktı | Gecikme histogramı 25,6 µs'de bitiyordu; B'de gecikme 2,5 ms'ye kadar çıkar | İki düzeyli histogram: 1 çevrimlik kutular 25,6 µs'ye kadar + 0,1 µs'lik kutular bir periyot boyunca; gecikme tanım gereği periyottan küçük olduğundan taşma olamaz (yine de sayılır). Adım süresi histogramı 0,1 µs kutularla 20 ms'ye genişletildi (−Og'de B 5 ms'yi aşabileceği için). Histogramlar ham olarak gönderilir; yüzdelikleri PC hesaplar. Yüzdelik histogram aralığının dışındaysa "belirlenemedi" yazılır, sayı üretilmez |
| 5 | Kesme/sayaç yarışı | Gecikme (`TIM2->CNT`) ile tetik sırası (`tik`) ayrı anlarda okunuyordu: arada güncelleme olursa gecikme bir önceki tetiğe ait kalabilirdi; adım sonunda güncelleme olmuş ama kesme henüz işlenmemişse aşım görülmeyebilirdi | Adım başında `tik`, `CNT`, `SR.UIF`, `tik` sırayla okunur; arada güncelleme olduysa ya da UIF bekliyorsa yeniden okunur. Adım sonunda aşım = `tik` değişti **ya da** UIF bekliyor (aşım gizlenmez; yalnız birkaç çevrimlik pencerede fazla sayabilir). Çapraz denetim: gecikme + süre ≥ periyot (`asim_dwt`) ayrıca sayılır |
| 6 | Koşunun imajla bağı zayıftı | Açılış satırında yalnız derleme etiketi vardı | Her derlemeye benzersiz kimlik (`MP4_KIMLIK`, ELF'te bulunduğu derlemede denetlenir) açılış satırında basılır; çözümleyici kimliği tutan açılıştan başlar, ikinci açılışı hata sayar. Koşu betiği imajın SHA-256'sını başta ve sonda kaydeder; farklıysa koşu geçersizdir |
| 7 | D3 denetiminde kör noktalar | Dallanma deseni bazı koşullu kuyruk dallanmalarını (`bvs` vb.) ve alt çizgili çift duyarlıklı libm adlarını (`__kernel_rem_pio2`) kaçırabiliyordu; dolaylı çağrı yalnız uyarıydı | Bütün `b/bl/blx/cbz/cbnz` + koşul son ekleri; çift duyarlıklı adlar genişletildi; `.f64` komutu da sayılır; erişilen işlevlerde dolaylı çağrı varsa çıkış 3 ("kanıtlanamadı") |
| 8 | Yığın ölçümü yalnız tek bir bütün-yığın değeriydi | — | Ek olarak her fazın geçişinin başında main çerçevesinin altındaki 32 KB yeniden boyanır, kayıt sonunda (faz dökümünden önce) en derin kullanım ölçülür (`yigin_kayit`, geçiş adımları dahil). Yeniden boyama `yigin_tepe`'nin de kapsamını o faza indirir; iki değerin gerçek kapsamı Bölüm 3.2'dedir (sol #4) |

**sol ilk incelemesi sonrası (2026-11-18, yalnız PC çözümleyicisi ve koşu betiği; firmware ve adım kodu değişmedi):** çözümleyici artık kampanyayı koşunun gerçek ayarlarıyla (`--beklenen-hz/-fazlar/-faz-adim`, zorunlu) doğrular, açılış başlığını, tek B2 satırını (160 MHz, saat/UART kurulumu başarılı, FPU, ICACHE, flash gecikmesi), K satırlarını ve T satırındaki toplam adımı denetler; her satır türü için kesin şema uygular (zorunlu alan, yalnız rakamlı taşmasız sayı, tanımsız alan yok); gecikme histogramının en küçük/en büyük/toplamla, parça sürelerinin adım süresiyle, aşım/atlanan/temiz gecikme sayaçlarının n ve tetik aralığıyla tutarlılığını denetler; herhangi bir hata varsa sayısal özet **basmaz**. Bulgu bulgu yanıt: `docs/olcumler/inceleme/MP4-yanit.md`.

**sol ikinci incelemesi sonrası (2026-11-18, aynı kapsam):** (Y1) bütün örnekler taşmış olsa da en küçük değer ve toplam süre histogram/taşma sınırlarıyla denetlenir; (Y2) yığın değerleri ölçülen imajın ELF'inden hesaplanan sınırla (`_estack − (_ebss + 256)`, kayıttan bağımsız, zorunlu `--yigin-azami`) ve `yigin_tepe > yigin_kayit` kapsama ilişkisiyle denetlenir; (Y3) bütün adımlar "temiz" ise temiz gecikme en büyüğü genel en büyüğe eşit olmalıdır; (Y4) histogram CSV'leri yalnız bütün kampanya denetimleri (T dahil) geçtikten sonra yazılır, var olan dosya ezilmez. Yanıt: `docs/olcumler/inceleme/MP4-yanit-2.md`.

**Mimar kapsam daraltması (2026-11-18, `docs/gorevler/W0-MP4a-kapsam-daraltma.md`):** sol'un üçüncü incelemesindeki (Y5) taşmalı kampanya tutarlılık açığı genel olarak çözülmedi; bunun yerine çözümleyici taşmalı kampanyaları desteklemez hale getirildi (fail-closed). Yukarıdaki Y1 denetimleri yerini bu kurala bıraktı. Yanıt: `docs/olcumler/inceleme/MP4-kapsam-yanit.md`.

Devralma ekinde istenen "ölçüm zamanına seri çıktı işi eklenmesin" koşulu: ölçülen bölge yalnız `yt_adim` çağrısıdır (`DWT->CYCCNT` okumaları arasında); UART yazımı, durum satırı biçimlendirmesi, istatistik ve girdi üretimi bu bölgenin dışındadır. Ağır yükte (B, 400 Hz) bu işler bir sonraki adımın başlamasını geciktirir; bu, adım süresine değil, atlanan tetik sayısına ve gecikmeye yansır.

### 3.2 Ölçüm tanımları

- **Tetik:** TIM2 (32 bit, `PSC = 0`, saati PCLK1 = 160 MHz) `ARR = 160 MHz / f − 1` ile güncelleme kesmesi (400 Hz'te 399.999), öncelik 1; kesme `CNT`'yi okur, sayaç artırır. SysTick 1 kHz (HAL), öncelik 15, açık. Ana döngü tetik sayacını yoklar; değişince adımı koşar.
- **Adım süresi:** adımın hemen öncesinde ve sonrasında `DWT->CYCCNT`; kesmeler açık (ölçülen süre o sırada gelen SysTick ve TIM2 kesmelerini içerir).
- **Kaçırılan adım (`asim`):** adım bittiğinde bir sonraki güncelleme olayı olmuşsa (adım bir sonraki tetikten önce bitmedi). **Atlanan tetik:** hiç adım başlatılmadan geçen tetik.
- **Tetik gecikmesi, bu raporda ölçülen:** zamanlayıcının **güncelleme olayından** ana döngünün tetiği tutarlı okuduğu ana (`TIM2->CNT`; `PSC = 0` olduğundan çevrim). Bu okuma adımın ölçülen bölgesini başlatan `DWT->CYCCNT` okumasından birkaç çevrim öncedir; adım sonunda ise aşım bayrağı DWT bitiş okumasından birkaç çevrim sonra okunur (bu küçük uç farkları yüzünden aşım, bitişe birkaç çevrim kala gelen bir tetikte fazla sayılabilir; az sayılmaz). Ana görevin tanımı "zamanlayıcı kesmesinden adımın başına"dır; burada başlangıç noktası kesme girişinden **daha erkendir** (güncelleme olayı), yani ölçülen değer görev tanımına göre kesme giriş gecikmesi kadar büyüktür (tutucu yön). Kesme girişi (güncelleme olayından TIM2 işleyicisinin `CNT` okumasına) yalnız en küçük/en büyük olarak ayrıca bildirilir; görev tanımındaki "kesme → adım başı" gecikmesinin yüzdelikleri bu iki ölçümden **türetilemez** (aynı adıma ait çiftler saklanmadı) ve bu raporda verilmez. Gecikmeye kesme girişi, işleyici, yoklama döngüsünün bir turu (bir UART baytı dahil) ve tutarlı okuma dahildir.
- **Önemli sınır:** adımlar periyodu aştığında (B, 400 Hz) bir sonraki adım önceki biter bitmez başlar; o fazdaki "gecikme" önceki adımın taşmasından kaynaklanır ve düzeneğin tetik gecikmesi olarak yorumlanamaz. Bu yüzden "önceki adım aşmamış ve tetik atlanmamış" adımlarda gecikme ayrıca bildirilir.
- **Yüzdelikler:** sıralı ⌈q·n⌉'inci örneğin bulunduğu kutunun kapsayıcı üst kenarı, en büyük değerle kırpılır (gerçek yüzdelik bu değerden büyük olamaz; tabloda "≤"). Tam sayı aritmetiğiyle hesaplanır ve çevrim/160 olarak beş ondalıkla **kesin** yazılır (160 MHz'te bir çevrim 0,00625 µs; yuvarlama yoktur, bu yüzden "≤" değeri hiçbir zaman aşağı kaymaz). Adım süresi kutusu 0,1 µs (16 çevrim); gecikme 25,6 µs'ye kadar 1 çevrim, üstünde 0,1 µs. Ortalamalar (≈) yuvarlanmış bilgi değerleridir, sınır değildir.
- **Çözümleyicinin kapsamı (mimar kararı, `docs/gorevler/W0-MP4a-kapsam-daraltma.md`):** adım süresi histogramı 20 ms'ye kadardır; herhangi bir fazda bu aralığın dışına taşan adım (`F.tasan > 0`) ya da gecikme histogramı taşması (`G.lat_tasan > 0`) varsa kampanya **geçersiz** sayılır (çıkış 1, sayısal özet ve histogram dışa aktarımı yok). Taşmalı kampanyaların tutarlılığı bu atılabilir çözümleyicide denetlenmez ve desteklenmez. Bu raporun bütün gerçek kayıtlarında taşma yoktur (`tasan = 0`, `lat_tasan = 0`, en büyük adım 6421,69375 µs).
- **Kayıt süresi:** firmware'in `duvar_cyc` alanı ilk kayıtlı adımın **başından son kayıtlı adımın başına** kadardır; son adımın süresi dahil değildir. Raporda bu değer **muhafazakâr süre alt sınırı** olarak kullanılır (mikrosaniyede aşağı yuvarlanır) ve 10 dakika koşulu bu alt sınırla denetlenir. Son adımın kesin süresi saklanmadığından, son adım dahil süre yalnız `[duvar + min, duvar + max]` aralığı olarak verilir.
- **Parça süreleri:** adım kodu parça sınırlarında `yt_cevrim()`'i çağırır. Damganın maliyeti açılışta ölçüldü: 100 çağrı 1.113 çevrim (-O2), 1.933 çevrim (-Og); bu pay adım süresine dahildir.
- **Yığın:** açılışta `_ebss + 256 B`'den yığın işaretçisinin 512 B altına kadar desenle boyanır; her fazın geçişinin başında main döngüsünün çerçevesinin altındaki 32 KB **yeniden boyanır**. Bu yüzden iki değer de "açılıştan beri" değildir ve fazdan faza azalabilir:
  - `yigin_tepe` (S satırı): desenin bozulduğu en düşük adresten `_estack`'e kadar; kapsamı **son yeniden boyamadan bu yana**, yani o fazın geçiş adımları + kayıtlı adımları + faz dökümünün S satırına kadarki kısmı + kesmeler; main'in kendi çerçevesi dahil. Koşu özeti olarak fazlarda gözlenen değerlerin **en büyüğü** verilir.
  - `yigin_kayit`: aynı yeniden boyanan bölgede main döngüsü çerçevesinin altında kullanılan en derin yığın; kapsamı **geçiş adımları dahil** (boyama 400 geçiş adımından önce yapılır), adım + üzerine binen kesmeler + adım dışındaki girdi üretimi, durum satırı ve UART işi. Faz dökümünden önce okunur. Yalnız adımın payı ayrı ölçülmedi; derleyicinin işlev başına statik çerçeveleri (`-fstack-usage`) ayrıca verilir.
  - `yigin_kayit_tasti` = 1 olsaydı 32 KB'lık bölge yetmemiş, değer yalnız alt sınır olurdu; çözümleyici bunu geçersiz sayar (bütün koşularda 0).
  - Çözümleyicinin sınırları: `yigin_tepe ≤ _estack − (_ebss + 256)` — ölçülen imajın ELF sembollerinden (`arm-none-eabi-nm`), kayıttan bağımsız hesaplanır: -O2 1.586.416 B, -Og 1.586.024 B, -O2-200 1.486.416 B, -Og-200 1.486.024 B. `yigin_kayit ≤ 33.280 B` (32.768 B boyama bölgesi + 256 B üst pay + 256 B çerçeve payı; son pay firmware kodundan çıkarılmıştır, ölçülmedi). `yigin_tepe > yigin_kayit` (kapsama ilişkisi).
- **Fazlar:** açılışta 2 s ısınma; maskedeki fazlar sırayla bir kez; her faz 1 s kayıt dışı geçişle başlar, ardından `faz_adim` kayıtlı adım (400 Hz'te 241.000; 200 Hz ek koşularında 121.000 = 605 s). Faz dökümü geçişte yapılır.

### 3.3 Komutlar (depo kökünden)

| Adım | Komut | Ham veri |
|---|---|---|
| Derleme ve ek gözlemler | `sh tools/olcum/mp4/derle.sh` (temiz dizinden o2, og, o2-200, og-200, kisa; PC araçları; kimlik; D3; boyutlar; `-fstack-usage`; herhangi bir derleyici uyarısında durur) | `${IDA_CACHE}/mp4/derle-20261117-185727/` |
| PC araçlarının sınaması | `python3 -I tools/olcum/mp4/test/analiz_test.py` | (çıktı ekranda) |
| Kısa düzenek denemesi | `sh tools/olcum/mp4/kos.sh kisa 300` | `${IDA_CACHE}/mp4/kos/kisa-20261117-185942*` |
| Uzun koşular | `sh tools/olcum/mp4/kos.sh o2 3300`, `… og 6600`, `… o2-200 1500`, `… og-200 1500` (betik kendini `caffeinate -dims` altında başlatır; port/ST-LINK başka süreçte açıksa koşmaz) | `${IDA_CACHE}/mp4/kos/<derleme>-<damga>*` |
| Çözümleme (koşu betiğinin içinden) | `tools/olcum/mp4/derleme/pc/mp4_seri analiz <kayıt.csv> --kimlik <K> --beklenen-hz 400 --beklenen-fazlar 7 --beklenen-faz-adim 241000 --en-az-adim 240000 --en-az-sn 600 --yigin-azami <ELF'ten> --hist <önek>` (200 Hz ek koşularında `200 2 121000 … 120000 600`; koşu betiği yığın sınırını ELF'ten hesaplar) | `*-analiz.txt`, `*-hist-<faz>-{10us,ince,gecikme}.csv` |
| Düzeltilmiş çözümleyiciyle yeniden analiz (2026-11-18; eski çıktılar korunur) | ilk düzeltme sonrası `<kayıt>-analiz-v2-20261118.txt`; ikinci düzeltme sonrası `<kayıt>-analiz-v3-20261118.txt`; **kapsam daraltması sonrası (nihai): `<kayıt>-analiz-v4-kapsam-20261118.txt`**, histogramlar `<kayıt>-hist-v4-kapsam-20261118-*`; her çıktının başında imajın SHA-256'sı, yığın sınırı ve tam komut | `${IDA_CACHE}/mp4/kos/` |

## 4. Sonuçlar

Bütün değerler 160 MHz'te çevrim/160 olarak kesin yazılmıştır (µs). "≤" yüzdelikler kutunun kapsayıcı üst kenarıdır (Bölüm 3.2). Kaynak: kapsam daraltması sonrası çözümleyicinin `*-analiz-v4-kapsam-20261118.txt` çıktıları (bütün kayıtlar; sayısal satırlar v3, v2 ve koşu içindeki analizlerle birebir aynı). Histogram CSV'leri `*-hist-v4-kapsam-20261118-<faz>-{10us,ince,gecikme}.csv`.

### 4.1 Koşular ve bütünlük

| Koşu | İmaj (SHA-256, kimlik) | Başlangıç–bitiş (UTC) | Bütünlük | Fazların kayıt süresi alt sınırı |
|---|---|---|---|---|
| `o2-20261117-190043` (zorunlu, -O2, 400 Hz, A+B+C) | `9ad49a48…948f`, `20261117-185727-o2-a11153bb`; başta ve sonda aynı | 2026-11-17 16:00:43 – 16:35:00 | **Geçerli**: 3.539 satır, sıra 0…3538 kesintisiz, CRC/biçim hatası 0, tek açılış, T satırı var, `d_atlanan` 0; her fazda 241.000 kayıtlı adım; histogram toplamları = n; şema ve tutarlılık denetimleri geçti (v3 ve v4-kapsam dahil). sol bu kampanyayı bağımsız olarak da doğruladı (görev belgesi, 2026-11-18 notu; `MP4-sol-2.md`) | A 602,497500 s · B 821,428775 s · C 602,497499 s (hepsi ≥ 600 s) |
| `og-20261118-005058` (zorunlu, -Og, 400 Hz, A+B+C) | `5f0fd81f…3b5c`, `20261117-185727-og-be020939`; başta ve sonda aynı | 2026-11-17 21:50:58 – 22:37:50 | **Geçerli**: 3.604 satır, sıra 0…3603 kesintisiz, CRC/biçim hatası 0, tek açılış, T var, `d_atlanan` 0; her fazda 241.000 kayıtlı adım; koşu içindeki analiz, v3 ve v4-kapsam analizleri geçerli | A 602,497500 s · B 1573,712475 s · C 602,497499 s (hepsi ≥ 600 s) |
| `o2-200-20261118-013802` (ek, -O2, 200 Hz, yalnız B, 121.000 adım) | `1beaf4cb…ec6e`, `20261117-185727-o2-200-05c0b3de`; başta ve sonda aynı | 2026-11-17 22:38:02 – 22:48:13 | **Geçerli** (v3 ve v4-kapsam dahil): 623 satır, sıra kesintisiz, T var | B 604,994999 s |
| `og-200-20261118-014813` (ek, -Og, 200 Hz, yalnız B, 121.000 adım) | `e7e22797…a117`, `20261117-185727-og-200-5fe214ad`; başta ve sonda aynı | 2026-11-17 22:48:13 – 23:02:04 | **Geçerli** (v3 ve v4-kapsam dahil): 3.490 satır, sıra kesintisiz, T var | B 788,222534 s |
| `og-20261117-193512` | `5f0fd81f…3b5c` | 2026-11-17 16:35:12 – 16:39:14 | **Geçersiz**: USB bağlantısı (hub ve AC güç birlikte) 4. dakikada koptu (`read: Device not configured`); hiçbir faz tamamlanmadı. Kabul kanıtı değildir; yalnız durum satırları vardı. v3 ve v4-kapsam analizleri de geçersiz; histogram dosyası üretilmedi | — |
| `kisa-20261117-185942` (düzenek denemesi, faz başına 4.000) | `f331a952…5c04`, `…-kisa-bc64cc5d` | 2026-11-17 15:59:42 – 16:00:27 | Geçerli (kabul kampanyası değil) | — |
| `o2-20261116-011044` ve iki `kisa-20261116-*` | eski düzenek | 2026-11-16 | Geçersiz (Bölüm 3.1; eski biçim) | — |

Bütün geçerli koşularda B2 satırı: `saat_adim=0`, `hesap_hz=hal_hz=160000000`, `uart_hata=0`, `usart_cr1=0x2000000D` (UE, RE, TE, FIFOEN). TIM2 periyodu (DWT ile, çevrim): 400 Hz koşularında beklenen 400.000 — -O2 399.998, -Og 400.000, kısa 399.998; 200 Hz koşularında beklenen 800.000 — -O2-200 799.998, -Og-200 800.001.

### 4.2 Adım süresi (µs)

| Derleme / faz | n | min | p50 ≤ | p99 ≤ | p99,9 ≤ | p99,99 ≤ | maks | ort ≈ | maks / 2500 µs |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| -O2 A olağan | 241.000 | 595,34375 | 601,69375 | 1336,49375 | 1338,09375 | 1338,49375 | **1338,91875** | 699,683 | %53,6 |
| -O2 B en kötü | 241.000 | 3315,04375 | 3319,39375 | 3321,09375 | 3321,59375 | 3322,09375 | **3322,38125** | 3319,263 | %132,9 |
| -O2 C 02 tanımı (ek) | 241.000 | 1295,63125 | 1299,19375 | 1300,09375 | 1300,69375 | 1301,09375 | **1301,73125** | 1298,975 | %52,1 |
| -Og A olağan | 241.000 | 1120,41875 | 1126,69375 | 2570,19375 | 2571,79375 | 2572,19375 | **2572,78125** | 1318,603 | %102,9 |
| -Og B en kötü | 241.000 | 6414,07500 | 6418,59375 | 6420,29375 | 6420,89375 | 6421,39375 | **6421,69375** | 6418,417 | %256,9 |
| -Og C 02 tanımı (ek) | 241.000 | 2301,64375 | 2305,29375 | 2306,19375 | 2306,79375 | 2307,29375 | **2307,84375** | 2305,097 | %92,3 |
| -O2 B, **200 Hz** (ek) | 121.000 | 3314,32500 | 3320,19375 | 3321,19375 | 3321,59375 | 3321,96875 | **3321,96875** | 3319,637 | %66,4 (/ 5000 µs) |
| -Og B, **200 Hz** (ek) | 121.000 | 6412,91875 | 6418,99375 | 6420,29375 | 6420,69375 | 6421,19375 | **6421,45000** | 6418,523 | %128,4 (/ 5000 µs) |

Taşma (20 ms üstü) hiçbir fazda yok; gecikme histogramı taşması da yok. Bu, çözümleyicinin kapsamının (taşmasız kampanya) koşuludur.

**10 µs kutulu histogramlar** (alt kenar µs: sayı; boş kutular yazılmadı; tamamı `*-hist-v4-kapsam-20261118-<faz>-10us.csv`, 0,1 µs'lik ham hali `*-ince.csv`):

- -O2 A: 590: 26.402 · 600: 117.452 · 610: 745 · 620: 12 · 630: 3.000 · 640: 48.521 · 650: 11.729 · 1140: 417 · 1150: 14.646 · 1260: 3.015 · 1270: 9.067 · 1280: 2.981 · 1330: 3.013
- -O2 B: 3310: 209.765 · 3320: 31.235
- -O2 C: 1290: 238.232 · 1300: 2.768
- -Og A: 1120: 129.542 · 1130: 15.057 · 1150: 12 · 1160: 3.000 · 1210: 3.723 · 1220: 56.021 · 1230: 506 · 2190: 198 · 2200: 14.865 · 2420: 18 · 2430: 3.073 · 2440: 8.959 · 2450: 3.013 · 2560: 478 · 2570: 2.535
- -Og B: 6410: 236.001 · 6420: 4.999
- -Og C: 2300: 241.000

A'nın çok tepeli dağılımı, parça sürelerinden çıkarıldığına göre, ölçüm zamanlarının (25/15/10/5 Hz) çakışmasından gelir: yalnız IMU (≈ 600 µs), bir füzyon zamanı (≈ 640–660 µs ve ≈ 1150 µs), iki füzyon zamanı (≈ 1270–1340 µs).

### 4.3 Kaçırılan adım ve tetik

| Derleme / faz | Kaçırılan adım (`asim`) | DWT çapraz denetimi | Atlanan tetik | Tetik aralığı |
|---|---:|---:|---:|---:|
| -O2 A | **0** | 0 | 0 | 241.000 |
| -O2 B | **241.000** (hepsi) | 241.000 | 87.573 | 328.572 |
| -O2 C | 0 | 0 | 0 | 241.000 |
| -Og A | **3.013** | 3.013 | 0 | 241.000 |
| -Og B | **241.000** (hepsi) | 241.000 | 388.488 | 629.486 |
| -Og C | 0 | 0 | 0 | 241.000 |
| -O2 B, 200 Hz | **0** | 0 | 0 | 121.000 |
| -Og B, 200 Hz | **121.000** (hepsi) | 121.000 | 36.646 | 157.646 |

B'de (400 Hz) her adım periyodu aşar; adımlar arka arkaya koşar ve saniyede ≈ 293 adım (-O2; 241.000 / 821,4 s) ya da ≈ 153 adım (-Og; 241.000 / 1573,7 s) yapılabilir. -Og A'da 3.013 adım (iki füzyon zamanının çakıştığı, 2560–2580 µs kutusundaki adımların tamamı) periyodu aşar; atlanan tetik yine 0'dır, çünkü aşım bir periyottan kısadır. Aşım gizlenmedi: tetik sayacı + UIF yöntemi ile DWT çapraz denetimi bütün fazlarda aynı sayıyı verdi.

**Tetik gecikmesi** (güncelleme olayı → ana döngünün tetiği gördüğü an; Bölüm 3.2'deki tanım farkına dikkat):

| Derleme / faz | min | p50 ≤ | p99 ≤ | p99,9 ≤ | maks | Kesme girişi min–maks |
|---|---:|---:|---:|---:|---:|---:|
| -O2 A | 0,36875 | 0,40000 | 0,42500 | 0,45000 | **0,53750** | 0,10625–0,12500 |
| -O2 C | 0,38125 | 0,40000 | 0,42500 | 0,47500 | 0,51250 | 0,10625–0,12500 |
| -O2 B | 0,33750 | 1249,79375 | 2476,29375 | 2497,79375 | 2499,95625 | 0,10000–0,21250 |
| -Og A | 0,50000 | 0,58125 | 161,79375 | 176,29375 | **179,57500** | 0,10625–0,12500 |
| -Og C | 0,50000 | 0,58125 | 0,65625 | 0,75000 | 0,81250 | 0,10625–0,12500 |
| -Og B | 0,38750 | 1249,29375 | 2475,29375 | 2497,19375 | 2499,96250 | 0,10000–0,18125 |
| -O2 B, 200 Hz | 0,36875 | 0,38750 | 0,41875 | 0,46875 | 0,47500 | 0,10625–0,12500 |
| -Og B, 200 Hz | 0,42500 | 2501,29375 | 4949,99375 | 4995,29375 | 4999,92500 | 0,10000–0,20000 |

B satırı (ve Og'nin aşımlı fazları) düzeneğin tetik gecikmesi değildir: önceki adım periyodu aştığı için adım, en son tetikten sonra önceki adımın bitişine kadar bekler; "önceki adım aşmamış ve tetik atlanmamış" adım -O2 B'de hiç yoktur. -O2 A ve C'de, -Og C'de bütün adımlar bu koşulu sağlar (241.000/241.000). -Og A'da 237.987 adım sağlar ve bunlarda en büyük gecikme **65,15625 µs**'dir: önceki adım periyodu aşmasa bile, adımdan sonraki ölçüm dışı işler (girdi üretimi `yt_uret`, istatistik, durum satırı; -Og'de daha yavaş) periyodun sonuna taşarsa bir sonraki adımın başlangıcı gecikir. Bu açıklama kodun yapısından çıkarılmıştır; ayrıca ölçülmedi (doğrulanmadı). -Og A'nın p99 ve üstü gecikmeleri bu etkiyle ve 3.013 aşımın ardından gelen adımlarla belirlenir; düzeneğin boşta tetik gecikmesi değildir.

### 4.4 Parça süreleri (ortalama / en büyük, µs)

| Parça | -O2 A | -O2 B | -O2 C |
|---|---|---|---|
| mesaj_giris | 39,301 / 70,57500 | 70,597 / 70,89375 | 62,531 / 62,53125 |
| nominal | 68,979 / 73,71875 | 90,061 / 90,66875 | 69,747 / 69,75625 |
| kovaryans | 484,196 / 851,31250 | **2128,424** / 2129,14375 | 425,650 / 425,75000 |
| fuzyon | 22,858 / 280,03750 | **803,819** / 804,35000 | 591,370 / 591,48750 |
| sifirlama | 15,243 / 46,93125 | 149,032 / 149,50625 | 72,463 / 72,46250 |
| kip | 4,450 / 4,45000 | 4,451 / 4,75000 | 4,475 / 4,47500 |
| gudum | 0,586 / 8,61875 | 9,353 / 9,89375 | 9,327 / 9,47500 |
| kontrol | 8,668 / 10,85000 | 9,364 / 10,76875 | 9,223 / 10,50625 |
| tahsis | 37,606 / 38,71250 | 36,338 / 38,65000 | 36,274 / 38,01875 |
| mesaj_cikis | 15,575 / 15,57500 | 15,580 / 15,87500 | 15,672 / 15,77500 |

-Og (ortalama / en büyük, µs):

| Parça | -Og A | -Og B | -Og C |
|---|---|---|---|
| mesaj_giris | 39,984 / 75,99375 | 76,025 / 76,35000 | 66,562 / 66,56250 |
| nominal | 99,013 / 105,33750 | 126,418 / 127,10625 | 99,812 / 99,86250 |
| kovaryans | 1018,791 / 1791,30625 | **4478,377** / 4478,95000 | 895,664 / 895,71875 |
| fuzyon | 37,459 / 498,72500 | **1364,830** / 1365,71250 | 1002,198 / 1002,52500 |
| sifirlama | 29,495 / 93,85625 | 269,373 / 269,91250 | 137,738 / 137,73750 |
| kip | 6,022 / 6,15625 | 6,084 / 6,45625 | 6,013 / 6,08125 |
| gudum | 0,621 / 9,58125 | 10,401 / 10,96875 | 10,451 / 10,60000 |
| kontrol | 10,958 / 13,54375 | 11,945 / 13,41875 | 11,785 / 13,20000 |
| tahsis | 58,735 / 60,17500 | 57,414 / 59,68125 | 57,332 / 59,11250 |
| mesaj_cikis | 16,562 / 16,56250 | 16,569 / 16,88750 | 16,562 / 16,56250 |

B'de süre kovaryans yayılımına (5 yayılım × ≈ 425,7 µs ≈ %64) ve füzyona (11 skaler güncelleme, ≈ %24) gider. Tek bir yoğun 15×15 yayılım -O2'de ≈ 425,7 µs, -Og'de ≈ 895,7 µs'dir (C'de tek yayılım). -Og adımı -O2'ninkinin ≈ 1,78–1,93 katıdır (ortalamalar: A 1318,6 / 699,7; B 6418,4 / 3319,3; C 2305,1 / 1299,0 µs). Parça damgalarının maliyeti (≈ 11 çevrim/damga, -O2) parça sürelerine ve adım süresine dahildir.

### 4.5 Sağlama toplamları (derleyicinin hesabı silmediğinin göstergesi; belirlenimciliğin kanıtı değil, tutarlılık göstergesi)

| Faz | -O2 (2026-11-17) | -O2 eski imaj (2026-11-16) | -Og (2026-11-18) |
|---|---|---|---|
| A | `0xC02B9132` | `0xC02B9132` | `0xC02B9132` |
| B | `0x8F3440B8` | (satır kayboldu) | `0x8F3440B8` |
| C | `0x18529BF5` | (koşmadı) | `0x18529BF5` |

-O2 ve -Og'nin her fazdaki 32 bit FNV-1a sağlama toplamları eşleşti (aynı girdi dizisi, `-ffp-contract=off -fno-fast-math`, aynı libm). Bu, tekil çıktıların bit eşitliğinin kanıtı **değildir**: tekil çıktı baytları kayıtta yoktur ve 32 bitlik bir toplamın eşleşmesi farklı çıktı dizileriyle de olabilir (olasılığı düşük olsa da dışlanmadı); bit eşitliği doğrulanmadı. Kısa imajlarda (eski ve yeni, faz başına 4.000 adım) üç fazın toplamları da aynıdır (`0xBDBB2DA1`, `0x65C471A7`, `0x76FF5ABF`). Aynı imajın iki uzun koşusu yapılmadı.

### 4.6 Ek gözlemler (D3, D4)

| Kalem | -O2 | -Og | Kaynak |
|---|---|---|---|
| `yt_adim`'den erişilen işlev | 46 | 61 | `mp4_cagri`, `${IDA_CACHE}/mp4/derle-20261117-185727/d3-{o2,og}.txt` |
| Erişilen işlevlerde çift duyarlıklı yazılım kayan nokta yordamı / libm işlevi / `.f64` komutu | **0** | **0** | aynı |
| Erişilen işlevlerde dolaylı çağrı/dallanma | 0 | 0 | aynı (varsa denetim "kanıtlanamadı" der) |
| ELF'in tamamında `__aeabi_d*` / `__*df*` sembolü | 0 | 0 | `arm-none-eabi-nm` |
| Adım kitaplığının bağlanmış kodu (`.text`) / sabit / `.data` | 15.848 / 468 / 40 B | 16.436 / 468 / 40 B | `derle-ozet.txt` |
| Erişilen bütün işlevlerin kodu (libm ve libgcc dahil) | 16.898 B (46 işlev) | 17.866 B (61 işlev) | aynı |
| Adımın durum belleği (`sizeof(yt_t)`) + girdi/çıktı tamponları | 35.560 B + 400 B | aynı | açılış K satırı |
| En büyük statik çerçeveler (`-fstack-usage`) | `kov_yay` 2.024, `sensor_fuzyon` 1.288, `ldl_denetle` 1.008 B | `kov_yay` 2.000, `ldl_denetle` 968, `vektor_fuzyon` 704 B | `.su` dosyaları |
| `yigin_kayit` (fazların en büyüğü; kapsam Bölüm 3.2) | 2.500 B | 2.264 B | S satırları |
| `yigin_tepe` (fazların en büyüğü; kapsam Bölüm 3.2) | 3.452 B | 2.752 B | S satırları |
| `yigin_kayit_tasti` | 0 (bütün fazlar) | 0 (bütün fazlar) | S satırları |
| Boyanabilen yığın bölgesi (`_estack − (_ebss + 256)`; `yigin_tepe` üst sınırı) | 1.586.416 B | 1.586.024 B | ELF sembolleri |

Bütün imaj (-O2): `.text` 43.752 B, `.data` 52 B, `.bss` 970.712 B; `.bss`'in ≈ 1 MB'ı düzeneğin histogramlarıdır, adımın değildir.

200 Hz koşularında (yalnız B) FNV sağlama toplamı -O2 ve -Og'de eşleşti (`0xEBC826A4`; bit eşitliği doğrulanmadı; faz yapısı farklı olduğu için 400 Hz B'ninkinden farklıdır). Yığın: -O2-200 `yigin_tepe` 3.392 B / `yigin_kayit` 2.440 B; -Og-200 2.752 / 2.264 B; taşma yok.

-Og'nin yığın değerleri -O2'den küçüktür; nedeni (derleyicinin satır içine alma ve çerçeve kararları olabilir) incelenmedi. Yığın değerleri tek bir koşunun gözlemidir; erişilebilir en kötü yol analizi değildir.

## 5. Ölçütler

Görevin dondurduğu ölçütler -O2 ile ve yalnız A ve B fazlarına göre hükme bağlanır. C ve -Og bilgi içindir.

| # | Ölçüt | Ölçülen (-O2, 400 Hz, her faz 241.000 adım, kayıt süresi alt sınırı ≥ 602,4975 s) | Hüküm |
|---|---|---|---|
| D1 | İki yük durumunda, `-O2` ile 10 dakikada kaçırılan adım 0 | A: kaçırılan 0, atlanan tetik 0 (602,4975 s). B: **241.000 / 241.000 kaçırılan**, 87.573 atlanan tetik (821,4 s) | **Geçmedi** (A'da sağlandı, B'de sağlanmadı) |
| D2 | *En kötü* yük durumunda adım süresinin en büyük değeri ≤ 1,0 ms (`-O2`) | B: en büyük **3322,38125 µs** (p99,9 ≤ 3321,59375 µs) | **Geçmedi** (≈ 3,32 kat) |
| D3 | Adım yolunda çift duyarlıklı yazılım kayan nokta çağrısı yok | `yt_adim`'den erişilen 46 (-O2) ve 61 (-Og) işlevde çift duyarlıklı yordam/libm işlevi/`.f64` komutu 0, dolaylı çağrı 0; ELF'in tamamında `__aeabi_d*`/`__*df*` sembolü 0 | **Geçti** |
| D4 | Histogram, yığın tepe kullanımı ve kod boyutu raporlanmış (eşik yok) | 10 µs ve 0,1 µs histogramlar (Bölüm 4.2, CSV'ler), yığın (Bölüm 4.6; kapsam Bölüm 3.2), kod/RAM boyutları (Bölüm 4.6) | **Geçti** |

**D2 geçmediği için istenen döküm:** B'de (-O2) süre kovaryans yayılımına (5 × ≈ 425,7 µs = 2128,4 µs, %64) ve füzyona (803,8 µs, %24) gider; geri kalan her şey (mesaj, nominal, sıfırlama, kip, güdüm, kontrol, tahsis) ≈ 387 µs'dir (Bölüm 4.4).

**200 Hz (periyot 5 ms):**

200 Hz'te gerçekten koşuldu (ek; görev zorunlu kılmadı): yalnız B fazı, 121.000 kayıtlı adım (kayıt süresi alt sınırı ≥ 604,99 s), aynı adım kodu.

| Derleme | En büyük adım | 5 ms'ye oran | Kaçırılan adım | Hüküm (D1/D2 dili ile, bilgi) |
|---|---|---|---|---|
| -O2 | 3321,96875 µs | %66,4 | **0 / 121.000** | B yükü 200 Hz'te periyoda sığdı; 1,0 ms hedefi yine sağlanmadı |
| -Og | 6421,45000 µs | %128,4 | **121.000 / 121.000** | B yükü -Og'de 200 Hz'te de sığmadı |

Sınırlar: (1) Bu yer tutucunun adım içeriği hızdan bağımsızdır (zaman damgaları ve kontrol `dt`'si 2,5 ms varsayımıyla kalır); gerçek bir 200 Hz çekirdeğinde adım başına iki IMU örneği işlenmesi gibi farklar olabilir, ölçülmedi. (2) 200 Hz'te A (olağan) ve C koşulmadı; A ve C'nin 200 Hz durumu **hesaptır**: -O2 A en büyük 1338,91875 µs (%26,8 / 5 ms), C 1301,73125 µs (%26,0), -Og A 2572,78125 µs (%51,5), C 2307,84375 µs (%46,2); 400 Hz'te ölçülen adım süresinin 200 Hz'te değişmeyeceği varsayımına dayanır (B için bu varsayım -O2'de 3322,38 → 3321,97 µs, -Og'de 6421,69 → 6421,45 µs ile tutarlı çıktı).

## 6. Yer tutucunun sınırları

- Ölçülen gerçek çekirdek değildir; 02'nin yapısını ve boyutlarını izleyen, **bilerek pahalı** bir yer tutucudur: bütün 15×15 işlemler yoğun (seyreklik ve simetri kullanılmadı), ölçüm Jacobian'ı yoğun matrisle çarpılır, kapı reddinde de güncelleme yapılır, çıkış geçmişinin 80 kaydının hepsine kuaterniyon düzeltmesi uygulanır. B ayrıca 02'nin adım başına 8 skaler sınırını ve kovaryans yayılım kuralını aşar (görev belgesinin "üst sınır" tanımı). Gerçek çekirdek bu sonuçlardan hızlı ya da yavaş olabilir; sonuç **ön bilgidir** ve W3 sonunda gerçek çekirdekle yinelenmelidir.
- Bu iş algoritmayı optimize etmedi (kapsam dışı). Bu yüzden ölçüm, "02'nin bütçesi bu kartta tutar mı" sorusunu değil, "bu hacimdeki yoğun yer tutucu ne kadar sürer" sorusunu yanıtlar.
- **Taşıma ölçüme dahil değildir:** A1 bağlantısı (ST-LINK sanal seri portu üzerinden gelen/giden mesaj), alma/gönderme kesmeleri ve tamponlama yoktur; girdiler kartta üretilir. Gerçek sistemde adım süresine ve gecikmeye bunlar eklenir (MP3 sonrası ölçülecek).
- RTOS, ekran, gerçek sensör sürücüsü, atış kontrol yoktur. Kesmeler yalnız SysTick (1 kHz) ve TIM2'dir.
- Girdiler belirlenimci ve kısa periyotludur; gerçek sensör verisindeki seyrek pahalı yollar (sayısal yeniden hizalama, kapı reddi zincirleri vb.) temsil edilmeyebilir. Adım süresi dağılımı bu yüzden dar ve çok tepelidir.
- Önbellek: ICACHE açık, flash önden getirme açık; veri SRAM'de. Gerçek çekirdeğin bellek yerleşimi farklı olabilir.

## 7. 02 ile karşılaştırma ve öneriler (karar değildir)

- 02 §2.2'nin en kötü yığılma tahmini "≈ 16.900 işlem → 0,32–0,63 ms" idi. Bu tanıma en yakın faz C, -O2'de **1298,975 µs ortalama / 1301,73125 µs en büyük** sürdü: tahminin üst ucunun ≈ 2,1 katı. Ölçülen parça dökümüne göre C'nin süresinin çoğu tek yoğun 15×15 yayılım (≈ 425,7 µs) ve füzyondur (≈ 591,4 µs); yer tutucunun seyreklik ve simetri kullanmamasının bu farka katkısı ayrıca ölçülmedi; 02'nin işlem sayısından süreye dönüşümü (çevrim/işlem) bu ölçümle doğrulanmadı.
- Öneri (mimar ve kullanıcı kararına girdi; karar değildir): (1) 400 Hz'te "her adımda dört sensör + adım başına beş yayılım" (B) tanımı bu yer tutucuyla periyoda sığmaz; 02'nin kendi yayılım ve skaler sınırı kuralları (C) -O2'de %52'de kalır. (2) Kovaryans yayılımının yapısı (seyreklik, simetri, yayılım sıklığı) gerçek çekirdekte ölçüm sonucunu en çok değiştirecek kalemdir; W3 ölçümünde ayrı izlenmesi önerilir. (3) -Og ile olağan yükte (A) 241.000 adımın 3.013'ü periyodu aştı; bu yer tutucuyla, hata ayıklama derlemesinde 400 Hz'te aşımsız koşu beklenmemelidir.
- Açık kalanlar: gerçek çekirdekle yineleme (W3); taşıma dahil uçtan uca süre (MP3 sonrası); aynı imajın iki uzun koşusuyla tekrar üretilebilirlik (yapılmadı; belirlenimcilik yalnız sağlama toplamıyla gösterildi); görev tanımındaki "kesme → adım başı" gecikmesinin yüzdelikleri (ölçülmedi; Bölüm 3.2).
