# W3-kart — kart yazılımı (dilim 1: kart destek katmanı, seri bağlantı, döngü)

Tarih: 2026-11-21. Yazan: `ida-uygulayici` (Opus). İnceleyen: sol (tek tur). Dal: `is/w3-kart`. Ortak kurallar: `docs/gorevler/W3-ortak.md`. **Kartı yalnız bu iş kullanır.**

## Amaç

`firmware/` altındaki yer tutucuyu, STM32U5A9J-DK üzerinde gerçekten koşan kart yazılımına çevirmek. Dilim 1, protokolün üst katmanını (`W3-oturum`, aynı anda yazılıyor) beklemeyen her şeyi kurar ve kartta doğrular; dilim 2 oturum modülünü bağlayıp çekirdeği A1 girdisiyle çalıştırır (Z2).

Okunacaklar: iskelet §2, §3 (A1 satırı), §4, §8; `docs/mimari/01-kart-yazilimi.md` §2.1–§2.6, §2.8, §2.11, §2.13, §3.2 (USB HS ile ilgili her şey K17 ile geçersizdir; tek taşıma USART1 üzerinden ST-LINK sanal seri portu, 3 000 000 baud); A4'ün platform kuralları; ölçüm raporları ve prototipleri: `docs/olcumler/MP1-arac-zinciri.md`, `MP3-vcp.md`, `MP4-dongu-yuku.md` ve `tools/olcum/mp1`, `mp3`, `mp4` (saat kurulumu, seri port ve DMA, DWT ölçümü, yükleme betikleri kartta kanıtlanmıştır; **bunları temel al**, yeniden keşfetme); `firmware/BENIOKU.md`, `firmware/CMakeLists.txt`.

## Kapsam (dilim 1)

1. **Kart destek katmanı (`firmware/bsp/`):** başlangıç kodu ve bağlayıcı betiği; güç ve saat (160 MHz; MP1'in bulgusu: geçişte geçici AHB ÷2 ve bekleme); NVIC; DWT; 1 MHz duvar saati zamanlayıcısı; bağımsız gözetim zamanlayıcısı (IWDG); sıfırlama nedeni; hata işleyicileri; iki LED.
2. **Bağlantı sürücüsü (`firmware/link/`):** USART1 + DMA ile kesintisiz alım (dairesel tampon; yarı, tam ve boşta kesmeleri), blok hâlinde teslim ve alım damgası; DMA ile gönderim; taşma, çerçeve ve gürültü hatası sayaçları.
3. **Ana döngü ve çağırıcı (`firmware/app/`):** 01 §2.4'teki tek bağlamlı döngü; 200 Hz ızgara; `ida_core_t`'nin statik sahibi; bu dilimde çekirdek, girdi gelmediği için yalnız ızgarada `ida_core_tick` ile çağrılır; adım süresi DWT ile ölçülür (son, en kötü, histogram).
4. **Tel katmanı bağlantısı:** alınan bloklar `ida_link` tel katmanına **blok hâlinde** verilir; çözülen çerçeve ve hata sayıları tutulur.
5. **Tanılama:** sayaçlar 1 Hz'te A1'in tanılama mesajıyla (tel katmanının kodlayıcısıyla) gönderilir. Oturum henüz olmadığı için bu, yalnız bu dilimin duman testine özgü tek yönlü yayındır; dilim 2'de oturum kurallarına bağlanır.
6. **Derleme ve yükleme:** `fw-debug`, `fw-release`, `fw-measure` ön ayarları gerçek ikiliyi üretir; yükleme ve seri izleme betikleri `firmware/araclar/` altında (prototiplerdekilerden uyarlanır; programlayıcı aracının yeri ortam değişkeninden, kişisel yol yazılmaz).
7. **PC yanı duman aracı:** altın çerçevelerden üretilen geçerli A1 çerçevelerini belirli yükte gönderen ve kartın tanılama yayınını okuyan küçük araç (`firmware/araclar/`; Python ya da C).

## Kapsam dışı (dilim 2 ve sonrası)

Oturum, parti ve A1 ↔ A4 dönüştürücünün bağlanması; gönderim yöneticisinin iki sınıflı kuyruğu; sağlık ve platform kapısı; kalıcı bellek (parametre blokları); ekran; USB HS (hiçbir zaman).

## Dosya sahipliği

Yazılır: `firmware/**` (`firmware/yer_tutucu/` yeni yapıya taşınabilir; W2-depo'nun kartsız bağlama denetimi ve CI'daki firmware derlemesi çalışır kalmalı). `core/` ve `protocol/` kaynaklarına dokunulmaz, yalnız bağlanır.

## Kabul (kartta; hepsi raporda komut ve sayılarla)

1. Üç ön ayar derlenir; `fw-release` karta yüklenir, açılır, LED ve tanılama yayını görülür; sıfırlama nedeni doğru bildirilir.
2. **Saat:** 160 MHz ve 3 000 000 baud'un doğruluğu (tanılama yayınının hatasız okunması ve DWT ile 1 MHz zamanlayıcının birbirini doğrulaması).
3. **Alım:** PC aracı 600 s boyunca periyot başına ≈ 900 B (kapasitenin %60'ı) geçerli çerçeve gönderir; kartın çözdüğü çerçeve sayısı gönderilenle eşit, hata sayaçları sıfır. Sonra %90 yükte 120 s (sonuç bilgi amaçlıdır; kayıp varsa sayısıyla raporlanır).
4. **Döngü:** aynı koşuda 200 Hz ızgara kaçırması sıfır; adım süresinin en kötüsü ve histogramı raporlanır (ölçüm). Ölçüm koşusu sırasında Mac'te başka ağır iş çalıştığı raporda belirtilir (öteki W3 işleri aynı anda derleme yapıyor olabilir; bu yüzden bu sayılar ön değerdir, resmî ölçüm dilim 2'dedir).
5. **Tel katmanı maliyeti:** blok beslemeyle ayrıştırma süresi (DWT; ölçüm). `W3-tel-hiz` henüz birleşmediyse eski ayrıştırıcının süresi beklenen biçimde yüksektir; yalnız raporla.
6. **Gözetim:** ana döngü kasıtlı durdurulunca (yalnız `fw-measure`'da derlenen bir sınama komutu) IWDG kartı sıfırlar ve sıfırlama nedeni IWDG görünür.
7. Bellek ayırma denetimi ve kartsız bağlama denetimi geçer; yığın en yüksek kullanımı raporlanır.

Kart yanıt vermez ya da yükleme aracı kartı bulamazsa üç denemeden sonra dur ve gördüğünü raporla; kabloyla ilgili istekte bulunma.

## Dilim 2 (2026-11-21): oturum modülünün bağlanması ve kartta gerçek çağrı yolu

Dilim 1 kartta çalıştı ve sol incelemesinden geçti (5 kritik + 2 önemli bulgu düzeltildi; ayrıca kartta bulunan NVIC öncelik sorunu). Dilim 2, `ida_link` üst katmanını (`protocol/ida_link/include/ida_link/ust.h`; `main`'de) karta bağlar ve çekirdeği gerçek A1 girdisiyle çalıştırır. Sim tarafındaki köprü henüz yok; karşı taraf bu dilimde PC'de koşan bir **sahte sim**dir.

Okunacaklar: `ust.h` (çağrı akışı; W3-K10: `take` ile `put_out` arasında `rx` çağrılmaz; platform işlev tablosu; gönderim sınıfları); A1 §4 (el sıkışma, parti, kartın yükümlülükleri: A1-K11, K13, K44–K48, K80, K110) ve §3.9 (süreler); A4'ün platform kuralları (`plat` alanları, A4-K18 ızgara tick'i, K23 `prev_call_ns`, K59 platform kapısı, K78–K86 kalıcı blok ve açılış sayacı, K89 `core_ok`, K109 arka plan dilimi); `docs/mimari/01-kart-yazilimi.md` §2.6–§2.9; `docs/gorevler/W3-ortak.md` (W3-K10, K12, K14).

### Kapsam

1. **Platform işlev tablosu** (`ida_link_platform_t`): `zaman` (1 MHz duvar saati), `gonder` (aşağıdaki gönderim yöneticisi), `kuyruk_temizle`, `nonce` (RNG), `yeniden_baslat`, `kalici_blok`.
2. **Ana döngü:** alım blokları ve damgaları `ida_link_ust_rx`'e; `take` → `plat` alanları → `ida_core_step`/`tick` → `put_out` → `poll_tx`. W3-K10 penceresi: çağrı sürerken sürücünün topladığı baytlar bekletilir; `ida_link_ust_cagri_acik` ile denetlenir. Dilim 1'deki IDLE yayınları (`IDA_LINK_STATS`, kendi `BOARD_HELLO`'su) kaldırılır; duyuru ve sayaçlar oturum modülünden gelir.
3. **Gönderim yöneticisi:** üç sınıf (kritik, zorunlu, kaybedilebilir); kritik için ayrılmış kapasite; yalnız tam çerçeve; zorunlu sınıfta yer yoksa `false` (oturum `TX_OVERFLOW` ile biter); kaybedilebilir sınıfta düşürme ve sayaç (01 §2.6; A1-K110).
4. **Platform kapısı ve sağlık:** A4-K59 düzeltmesi protokol modülündedir; platform, `core_ok = 0`, alım durması ve gözetim koşullarında beslemeyi keser (dilim 1'deki kurallar korunur) ve güvenli beklemede çekirdeği çağırmaz.
5. **Kalıcı bellek:** açılış sayacı (`boot_count`; olay kimliklerinin ön koşulu) ve parametre A/B blokları flash'ın ayrılan 32 KB'ında; güç kesintisine dayanıklı yazma sırası; A4-K83 kayıt protokolü (`BUSY`/`OK`/`FAIL`, kuşak). Flash yazma süresi döngüyü bloklamaz ya da bloklama süresi ölçülüp raporlanır.
6. **Sahte sim (PC, `firmware/araclar/`):** el sıkışma, 200 Hz parti (zaman çerçevesi, IMU, basınç, manyetometre, batarya, sızıntı, emniyet örneği; sabit, durgun araç değerleri), aktüatör çıktılarının ve durumun okunması, komut gönderme (`ARM`, `SET_MODE`, `GUIDED_SP`, `DISARM`, parametre komutları), oturum sonu ve yeniden kurulum. Görev modülü `main`'e henüz girmediyse `ARM` reddedilebilir; test bunu kabul eder ve raporlar.

### Kabul (kartta; sayılar komut ve koşulla)

1. El sıkışma kurulur; 600 s boyunca 200 Hz parti: her partiye bir çekirdek `step` çağrısı ve bir aktüatör çıktısı; eksik, yinelenen ya da geç parti 0; `rx_stale_session` ve protokol hataları 0.
2. **Adım süresi (resmî ön ölçüm):** gerçek `step` yolunun (kestirim ve kontrol dahil) en kötü, ortalama ve histogramı; alımdan aktüatör çerçevesinin gönderimine kart içi gecikme (`S_kart`); 200 Hz ızgara ve periyot kaçırması 0. Bütçe: çekirdek 3,5 ms, periyot 5 ms (iskelet §4). Ölçüm sırasında Mac'te başka ağır iş koşuyorsa raporda belirt.
3. Tel katmanı maliyeti periyot başına en kötü (MP5 eşikleri: 1 050 B ayrıştırma 0,20 ms, periyot toplamı 0,50 ms); eşik aşılıyorsa nedeniyle raporla (dilim 1 ön ölçümü: %90 yükte 0,32 ms/periyot, 202 ns/bayt).
4. Oturum sonu ve yeniden kurulum; kablo çekilmeden PC programının kapatılıp açılması; `T_bağ` zaman aşımında güvenli çıktı.
5. Parametre kaydı: `PARAM_SET` → `COMMIT` → `SAVE` → kart sıfırlanır → değer ve kuşak korunur; kayıt sırasında sıfırlama (IWDG sınamasıyla) sonrası blok tutarlı (A ya da B geçerli).
6. Açılış sayacı her açılışta artar; `boot_count_ok = 1`; olay kimlikleri kullanılabilir.
7. Dilim 1 kabul maddeleri (alım kayıpsızlığı, gözetim) bozulmadı.

Rapor: W3-ortak'taki yedi başlık; kartta ölçülen her sayı komutu ve koşuluyla. Z2 (gerçek sim ile kapalı çevrim) bu dilimin değil, sim tarafındaki köprü işinin ardından yapılacak bütünlemenin konusudur.
