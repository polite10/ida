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
