# MP8a — Windows hedef bilgisayarı: donanım kaydı ve Godot kamera ön turu

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP8a.md` |
| Dal | `olcum/mp8-windows` |
| Tarih | Paket hazırlığı 2026-11-19. Windows koşusu: _(kullanıcı çalıştırınca)_ |
| Uygulayan | `ida-uygulayici` (Opus): paket ve Mac denetimleri. Windows koşusunu kullanıcı yapar |
| İnceleme | sol 1. tur: `docs/olcumler/inceleme/MP8-sol.md` (8 kritik, 1 önemli). Yanıt: `docs/olcumler/inceleme/MP8-yanit.md` |
| Durum | **Taslak, 1. düzeltme turu uygulandı.** Windows'ta hiçbir şey koşmadı. W1 tamamlanmadı (`pwsh` bu ortamda çalıştırılamıyor). H1–H5 boş |

## 1. Amaç

Kullanıcının düşük donanımlı Windows bilgisayarı (8 GB bellek, dahili ekran kartı; K16) için iki şey ölçülecek:

1. Donanım. İşlemci modeli dahil; şu an bilinmiyor.
2. MP6a'da Mac'te geçen kamera yolunun bu makinede çalışıp çalışmadığı ve ne kadar sürdüğü. Yol şudur: `force_draw(false, dt)`, `SubViewport` `UPDATE_ONCE` ve eşzamanlı geri okuma.

Sonuç, kalite kademesi tabanının ilk Windows girdisidir (iskelet §6, §8; Ö11; M12, M28, M30). Bu belge paketin yöntemini ve Mac'te doğrulananları içerir. Windows sonuçları kullanıcı paketi çalıştırınca 6. ve 7. başlıklara işlenir.

## 2. Paketin içeriği

Kod `tools/olcum/windows/` altındadır; atılabilir ölçüm prototipidir.

| Dosya | İş |
|---|---|
| `calistir.ps1` | Kullanıcının çalıştırdığı tek betik. Windows PowerShell 5.1 ile uyumludur, yalnız ASCII karakter içerir. Yönetici yetkisi istemez, bir şey kurmaz, sistem ayarı değiştirmez |
| `BENIOKU.md` | Kullanıcıya dönük çalıştırma yönergesi. Teknik ayrıntılar ve özet değerlerinin kaynağı da burada |
| `analiz_win.py` | Çıktı klasörünü çözümler. `tools/olcum/mp6/analiz.py`'nin `kos_coz` işlevini değiştirmeden kullanır. Hükümden önce deney koşullarını ve sürecin temiz bitişini denetler. Windows'a özgü dış gözlemi (ölçüm aralığıyla eşleştirilmiş), çizim yöntemi bandını, donanım kaydının anlamsal denetimini ve H1–H5'i ekler |
| `mac_denetim.ps1` | Mac'te `pwsh` ile durağan denetim. Kapsamı: ayrıştırma, PS 7'ye özgü sözdizimi taraması, ASCII denetimi, gömülü C#'ın C# 5 olarak ayrıştırılması ve derlenmesi, isteğe bağlı PSScriptAnalyzer (hedef 5.1). Bu ortamda koşturulamaz; mimar ya da kullanıcı koşturur (6.1, W1) |

Godot projesi `tools/olcum/mp6/proje` yerinden çağrılır; kopyalanmaz. `tools/olcum/mp6/**` dosyalarında değişiklik yok.

## 3. Yöntem

### 3.1 Adımlar

`calistir.ps1` adımları sırayla koşar. Her adım ayrı `try/catch` içindedir: biri başarısız olsa da sonrakiler denenir. Sonunda her adım `TAMAM`, `UYARI`, `ATLANDI`, `KISMI` ya da `HATA` diye listelenir. Hata varsa betik 1 çıkış koduyla biter. `-KuruCalistir` hiçbir şey indirmez, Godot'yu başlatmaz, klasör oluşturmaz ve hiçbir sürece dokunmaz; yalnız adımları, yolları ve komut satırlarını yazar.

Genel kurallar (1. düzeltme turundan):

- **Süreç sahipliği.** Betik yalnız kendi başlattığı süreçleri kapatır: `Start-Process -PassThru` ile aldığı süreç nesneleri, kimlikleriyle. Aynı ikiliden çalışan başka süreçler sahiplik sayılmaz, dokunulmaz. Önceki sürümdeki yol eşitliğiyle "artık süreç kapatma" kaldırıldı.
- **Küresel süre sınırı: 15 dk.** Betik başlangıcından sayılır. Kullanıcı sorusunda beklenen süre ve Godot indirme süresi sınıra eklenir; bu ikisi betiğin denetiminde değildir. Sınır aşılırsa kalan adımlar `ATLANDI` olur, Özet ve Zip yine yapılır. Ayrıca her koşu, sınıra kadar bitecek süre kalmamışsa başlatılmaz. Sınır çalışan koşunun içinde de uygulanır (sol-2 Y2):
  - Hazır bekleme ve izleme döngüleri kalan süreyi her turda denetler.
  - Süre dolunca ölçüm süreci betiğin kendi süreç nesnesiyle kapatılır.
  - `kos.log`'a `surec_temiz false kuresel_sure_siniri` yazılır; bu temiz bitiş sayılmaz.
- **Ölçüm yazıldı ile süreç temiz bitti ayrı denetlenir.** Süreç temiz bitti demek şudur: hata iletisi yok, süre aşımı yok, kapatılmak zorunda kalınmadı, çıkış kodu biliniyor ve 0, örtü süreci ölçümden önce kapanmadı. Sonuç `kos.log` içinde `surec_temiz true|false <neden>` satırına ve `sonuc.json` içindeki `surec_temiz` ve `duzenek` alanlarına yazılır.
  - Denemelerde bir yöntem yalnız ölçüm yazıldıysa, süreç temiz bittiyse ve kare üretildiyse seçilir.
  - Kampanya koşusunda ölçüm yazıldı ama süreç temiz bitmediyse adım `HATA` olur.
  - "Düzenek çalışmadı" ile "ölçüt geçmedi" ayrı kaydedilir.

1. **Ön denetim.** PowerShell sürümü, dil kipi ve proje yolu.
2. **Çıktı klasörü.** `Masaüstü\IDA-MP8-<zaman damgası>`; masaüstü yoksa betiğin yanı. OneDrive altındaysa uyarı yazar.
3. **Donanım kaydı.** `donanim.txt` ve `donanim.json` dosyalarına yazılır. Her alt kalem ayrı `try` içindedir; hatalar `hatalar` alanına girer. Kaynaklar:

   | Kalem | Kaynak |
   |---|---|
   | İşlemci | `Win32_Processor`: model, çekirdek, iş parçacığı, en yüksek MHz, önbellek |
   | Bilgisayar ve bellek | `Win32_ComputerSystem`, `Win32_PhysicalMemory`: modül, hız, yuva |
   | İşletim sistemi | `Win32_OperatingSystem` |
   | Ekran kartı | `Win32_VideoController`: ad, sürücü sürümü ve tarihi, çözünürlük, yenileme hızı |
   | Ekran ve güç | `System.Windows.Forms.Screen` (mantıksal sınırlar), `SystemInformation.PowerStatus` (priz ya da pil), `Win32_Battery` |
   | Güç planı | `powercfg /getactivescheme`; yalnız okunur sorgu. Çıkış kodu ayrıca kaydedilir. Çıkış kodu 0 değilse ya da GUID ayrıştırılamazsa plan kaydedilmez ve hata yazılır |
   | Ekran koruyucu | `Win32_Desktop`: yalnız etkinlik ve süre; kullanıcı ya da bilgisayar adı yazılmaz |
   | Ortam yükü | Toplam işlemci yüzdesi ve boş bellek |

   `CIM` olmazsa `Get-WmiObject` denenir.
4. **Güç ve ekran denetimi.** Prize takılı değilse uyarı verir ve bir kez Enter bekler. Ekran koruyucu 15 dakikadan kısa sürede açılacaksa uyarı yazar. Ayar değiştirmez.
5. **Godot ikilisi.** `Godot_v4.7.2-stable_win64.exe.zip` resmî dağıtımdan `%USERPROFILE%\.cache\ida\godot\4.7.2\` altına indirilir (`IDA_CACHE` tanımlıysa onun altına). Zip zaten varsa yeniden indirilmez. Zip her çalıştırmada yeniden açılır. Önceki açılmış dizin silinemezse (ör. önceki bir çalıştırmadan kalan Godot açıksa) betik o süreci kapatmaz; adım açık bir iletiyle `HATA` verir.
   - SHA-512 ve SHA-256 ikisi de betikte sabit yazılı değerlerle karşılaştırılır. Biri uyuşmazsa Godot çalıştırılmaz.
   - Değerler ve kaynakları 5. başlıkta.
   - Özet bilinmiyorsa (sabitler boşsa) betik özeti gösterir ve onay ister. Bu dal şu an devrede değildir, çünkü iki özet de birincil kaynaktan alındı.
6. **Pencere gözlem yardımcısı.** C# `Add-Type` ile derlenir (`user32`, `dwmapi`, `kernel32` P/Invoke). Derlenemezse dış gözlem "yok" diye kaydedilir; koşular yine yapılır.
7. **Uyku engeli.** `SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)`. Ayar değiştirmez; betik bitince geri alınır. Microsoft belgesine göre ekran koruyucuyu durdurmaz (doğrulanmadı).
8. **Çizim yöntemi denemesi.** Her deneme ayrı alt klasöre yazılır (`deneme_N_<yöntem>`). Deneme kısa bir görünür koşudur: 3 s ısınma, 5 s ölçüm, kare dökümü yok. İlk kare üreten yöntem kampanyada kullanılır. Sıra:
   1. varsayılan (proje ayarı Forward+)
   2. `--rendering-method mobile`
   3. `--rendering-driver opengl3`
9. **Kampanya.** `kampanya_<yöntem>/` altında yalnız yol (b), 640 × 480, 30 Hz istek ile şu koşular yapılır:

   | Koşu | Süre | Not |
   |---|---|---|
   | `b_gorunur` | 120 s | Referans |
   | `b_simge` | 120 s | |
   | `b_ortulu` | 120 s | |
   | `okuma_640x480_esz` | 60 s | Görünür pencere, `--geri=esz --cift=1` |

   Durum koşularında her 300. kare ham dökülür (koşu başına 12 döküm). Isınma 6 s'dir.
10. **Kare dökümü boyut denetimi.** Toplam 50 MB'ı aşarsa her koşuda tek sıralı dökümler silinir. 640 × 480 RGB8'de beklenen toplam yaklaşık 33 MB'tır (hesap).
11. **Özet ve zip.** `sonuc.json` ve `sonuc.txt` yazılır; klasör `.zip` yapılır. Zip yolu ekrana yazılır.

### 3.2 Godot komut satırları

Komut satırları MP6 `kos.sh` ile aynı biçimdedir. Ölçüm süreci:

```
<Godot.exe> --path <proje> --position 160,160 --resolution 480x360 [--always-on-top: yalnız görünür]
            --log-file <koşu>/godot_log.txt [çizim yöntemi argümanı]
            -- --yol=b --durum=<gorunur|simge|ortulu> --sure=120 --isinma=6 --cikti=<koşu> --ornek=300
```

Örtü süreci (yalnız örtülü durumda, ölçüm penceresi hazır olunca başlatılır):

```
<Godot.exe> --path <proje> --always-on-top --position 40,40 --resolution 1100x900
            --log-file <koşu>/ortu_godot_log.txt [çizim yöntemi argümanı] -- --rol=ortu --sure=240
```

Yollar Godot'ya ileri eğik çizgiyle verilir (`C:/...`). Boşluk içeren argümanlar `CommandLineToArgvW` kurallarıyla tırnaklanır. Godot'nun standart çıktısı ve hata çıktısı ayrı dosyalara yönlendirilir. Godot 4.7.2'nin Windows GUI ikilisi var olan dosya yönlendirmesini korur (`platform/windows/os_windows.cpp:161–184`, `RedirectIOToConsole`). `--log-file` ise proje ayarından bağımsız olarak dosya günlüğü açar (`main/main.cpp:2321–2339`).

### 3.3 Pencere durumları ve doğrulama

| Durum | Nasıl kurulur | İç gözlem (`olcum.gd`) | Dış gözlem (`user32`, 10 s'de bir) |
|---|---|---|---|
| görünür | Ölçüm penceresi `--always-on-top` | `frames_drawn` ilerliyor | Ana pencere görünür ve simge değil; istemci alanındaki 5 noktanın hepsinde en üstteki pencere ölçüm sürecinin |
| simge | Proje kendisi `window_set_mode(MINIMIZED)` çağırır | `frames_drawn` 1,5 s durdu | Ana pencere `IsIconic` |
| örtülü | İkinci Godot süreci (`--rol=ortu`), her zaman üstte, ölçüm penceresini kapsar | **Ölçüt değil** (aşağıda) | **Geometri:** DPI bağlamı açıkça `PER_MONITOR_AWARE_V2` yapılabildi; iki pencerenin görünen dış çerçevesi (`DWMWA_EXTENDED_FRAME_BOUNDS`) okunabildi; ölçüm penceresinin çerçevesi örtününkinin içinde; örtü Z sırasında ölçüm penceresinin üstünde (`GetWindow(GW_HWNDPREV)` ile yukarı yürüme); iki pencere de görünür ve simge değil. Geometri doğrulanamazsa örnek "doğrulanamadı" sayılır. 5 nokta denetimi yalnız ek gözlemdir |

**Dış gözlemin ölçüm aralığıyla eşleştirilmesi.** Örnek zamanı (`gecen_s`), betiğin `hazir` dosyasını gördüğü andan sayılır. Ölçüm aralığı aynı eksene şu kaynaklardan taşınır: `hazir` dosyasındaki Godot saati ile `ozet.json`'daki ölçüm başlangıç ve bitişi. Tolerans 1 s'dir (betik `hazir` dosyasını 0,5 s'de bir yoklar).

Hüküm yalnız ölçüm aralığındaki örneklerden kurulur:

- **Tutulmadı:** en az bir örnekte durum tutulmadı.
- **Doğrulanmadı:** şunlardan biri varsa:
  - bozuk satır
  - aralık içinde gözlem hatası
  - doğrulanamayan örnek
  - hiç örnek yok
  - en büyük boşluk 15 s'yi aşıyor (aralık başı ve sonu dahil; 1 s tolerans)
- **Tutuldu:** bunların hiçbiri yoksa.

Durum geçişindeki ilk örnek ölçüm aralığının dışında kaldığı için kendiliğinden hesaba girmez.

**Windows'ta örtülü durum.** Godot 4.7.2 Windows'ta yalnız örtülen pencerede çizmeye devam eder (07 §2.6, 12.1; D12 #1c). Bu yüzden `olcum.gd`'nin geçiş ölçütü ("`frames_drawn` 1,5 s durdu") örtülü durumda sağlanmayabilir. O zaman ölçüm 30 s sonra "30 s içinde oluşmadı" notuyla yine başlar.

- Örtülü durumun kanıtı dış gözlemdir. `analiz_win.py`, H2'de örtülü için iç gözlemi ölçüt olarak kullanmaz.
- Bu, görev belgesinde öngörülmemiş bir farktır ve ölçümün kendisini değiştirmez.
- Proje değiştirilmedi.

Dış gözlemin ayrıntıları:

- Kayıt yeri `pencere_win.jsonl`.
- Her örnekte yalnız bu çalıştırmanın süreçlerine ait pencerelerin ayrıntısı yazılır: ölçüm ve örtü süreçlerinin bütün üst düzey pencereleri; başlık, sınıf, görünürlük, simge durumu, her zaman üstte, DWM `cloak`, pencere dikdörtgeni ve dış çerçeve. Ayrıca DPI bağlamı ve geometri sonucu yazılır.
- Başka uygulamaların pencere başlıkları kaydedilmez (sol md.9). Ön plandaki pencere için yalnız sahiplik sınıfı (`olcum`, `ortu`, `diger`, `yok`) ve pencere sınıf adı yazılır. 5 noktada da yalnız sahiplik sınıfı yazılır.
- Süreç örnekleri `surec_win.csv` dosyasına gider: işlemci süresi, çalışma kümesi, öncelik sınıfı, yanıt veriyor mu.

### 3.4 Çizim yöntemi kaydı (H5)

Godot 4.7.2 Windows'ta kendiliğinden geri düşer: Vulkan çalışmazsa D3D12'ye, o da çalışmazsa OpenGL 3'e (Compatibility) geçer. Proje ayarları varsayılandır (`fallback_to_d3d12`, `fallback_to_opengl3` = true).

- Kaynak: `main/main.cpp:2396–2398`, `platform/windows/display_server_windows.cpp:8031–8142`.
- Bu yüzden "varsayılan" denemesi açılsa bile fiilen Compatibility ile çalışmış olabilir.
- Fiilî yöntem Godot'nun başlangıç bandından okunur:
  - RD için `"<API> <sürüm> - Forward+|Forward Mobile - Using Device #N: ..."` (`servers/rendering/rendering_device.cpp:8388`)
  - GL için `"OpenGL API <sürüm> - Compatibility - Using Device: ..."` (`drivers/gles3/rasterizer_gles3.cpp:196`)
- "switching to ..." uyarıları da ayrıca kaydedilir.
- `bilgi.json`'daki `rendering_method` proje ayarını okur. Komut satırıyla seçilen yöntemi göstermeyebilir; H5 için kullanılmaz.

`--rendering-driver opengl3` tek başına verildiğinde Godot yöntemi `gl_compatibility` seçer (`main/main.cpp:2587–2595`). Sürücü bütün yöntemlerde açılamazsa Godot kalıcı bir uyarı kutusu açar (`display_server_windows.cpp:8416–8455`). Betik süre sınırında (deneme 120 s) süreci kapatır; BENIOKU kullanıcıya "Tamam"a basmasını söyler.

### 3.5 Süre

| Kalem | Süre (hesap) |
|---|---|
| Ölçüm süreleri | 3 × 120 + 60 = 420 s |
| Deneme | Yaklaşık 15 s |
| Isınma ve başlatma | Koşu başına yaklaşık 10 s |
| Örtülü geçiş sınırı | 30 s (yukarıdaki nedenle) |
| Koşular arası bekleme | 3 s |

Hesaplanan toplam yaklaşık 9–9,5 dakikadır. Bu bir tahmindir, ölçülmedi. İndirme süresi buna dahil değildir. sol'un hesabı ideal durumda yaklaşık 499 s'dir (`MP8-sol.md`, "Yeniden hesaplananlar"). Denemeler hazır olma sınırına takılırsa süre uzar; küresel 15 dk sınırı bunu keser.

### 3.6 Çözümleme

```
python3 -I tools/olcum/windows/analiz_win.py <açılmış IDA-MP8-... klasörü>
```

Koşu başına `analiz.kos_coz` kullanılır (MP6 ile aynı sayım, kimlik boşluğu, gecikme ve geri okuma yüzdelikleri, MD5 ve MAD). Referans aynı kampanyadaki `b_gorunur` koşusudur.

**Hüküm kuralları (1. düzeltme turundan).**

- **Deney koşulları önce doğrulanır.** Koşul sağlanmazsa hüküm `kosul_disi` olur; deneme ya da test parametreleriyle alınmış kayıt kabul ölçümü sayılmaz.
  - H2 koşuları: `bilgi.json`'da yol b, `sure_s` 120, 640 × 480, 30 Hz, `geri=esz`, doğru durum; `ozet.json`'da 3600 istek; ölçülen ölçüm süresi 120 ± 1 s.
  - H4 koşusu: yol b, 60 s, 640 × 480, 30 Hz, `geri=esz`, `cift=1`, görünür, 1800 istek; ayrıca p50 ve p99 dolu, en az bir kare üretilmiş.
- **H2 sırası:**
  1. `kosul_disi`
  2. `duzenek_calismadi` (`surec_temiz false`)
  3. `gecersiz (durum tutulmadi)`: dış gözlem "tutulmadı" ya da iç gözlem başarısız. Görünür ve simgede iç gözlem `durum_dogrulandi` ve bütün kayıt aralıklarında `frames_drawn` ölçütüdür; örtülüde iç gözlem ölçüt değildir.
  4. `dogrulanmadi`: dış gözlem "doğrulanmadı" ya da süreç bitişi kaydı yok.
  5. Düşen kare < %1 ve kimlik kesintisiz ise `gecti`, değilse `gecmedi`. Bu iki şart ayrı alanlarda da raporlanır.
- **H3:**
  - Görünür referans önceden denetlenir. Geçerli sayılması için hepsi gerekir: dosyalar var, koşul uygun, süreç bitişi açıkça temiz (eksik ya da bilinmeyen kabul edilmez), kare üretilmiş, görünür H2 `gecti` (pencere durumu doğrulanmış; sol-2 Y1). Geçersizse referans çözümleyiciye verilmez, H3 "değerlendirilemedi (referans geçersiz: neden)" olur. Diğer koşular çözümlenmeye devam eder.
  - Her koşu ayrı `try` içinde çözümlenir.
  - H3 yalnız ilgili H2 durumu doğrulanmışsa hüküm verir; MAD ölçütü örnek karelerde en büyük değer üzerinden ≤ 2/255'tir.
- **H1:** zorunlu alanlar anlamca denetlenir:
  - işlemci modeli dolu; çekirdek ve iş parçacığı > 0; bellek > 0
  - en az bir ekran kartı kaydında ad, sürücü sürümü, iki çözünürlük boyutu ve gerçek yenileme hızı (2–1000 Hz). 0 "varsayılan", 4294967295 "optimal" anlamındadır; gerçek Hz değildir.
  - Windows sürümü dolu
  - güç durumu `Online` ya da `Offline` (`Unknown` geçmez)
  - güç planı için `powercfg` çıkış kodu 0 ve GUID

Üstüne şunlar eklenir:

- dış gözlem özeti
- süreç işlemci yüzdesi
- gerçekleşen kare hızı (üretilen kare / ölçüm süresi)
- operatör penceresinin çizim hızı (`frames_drawn` medyanı)
- çizim yöntemi bandı
- donanım kaydının eksiksizliği
- H1–H5 değerlendirmesi

Sonuç `mp8_ozet.json` dosyasına yazılır. MP6'nın `analiz.py`'si Windows çıktısını da okur: `kos.log` dosyasındaki `olcum_pid`/`ortu_pid` satırları ve koşu dosyaları aynı biçimdedir. Okuyamadığı tek şey Windows dış gözlemidir; Mac biçimi olan `pencere.jsonl` Windows'ta yazılmaz. Bu yüzden uyarlama gerekti.

## 4. Mac'te doğrulananlar

Ortam: Apple M3 Pro, macOS 26.5.2, Godot 4.7.2 Mac ikilisi (MP6a önbelleği), Python 3.12, numpy 2.1.1, .NET SDK 11.0.100 (1. turda `powershell` formülünün bağımlılığı olarak kurulmuştu; bu turda yeni kurulum yok).

**`pwsh` kullanılmadı.** Bu ortamda `pwsh` çağrısı reddediliyor. 1. turda `mac_denetim.ps1` bir Python başlatıcısı üzerinden dolaylı çağrıyla koşturulmuştu. Mimarın kararıyla bu yapılmamalıydı ve o sonuç kanıt sayılmaz; ayrıca o tarihten sonra betik büyük ölçüde değişti. Bu turda `pwsh` hiçbir yolla çalıştırılmadı. W1 tamamlanmadı (6.1).

1. **Gömülü C# yardımcısının C# 5 derlemesi (2. tur, son sürüm).**
   - Yöntem: `calistir.ps1` içindeki `$W32Kaynak` here-string'i bir Python betiğiyle dosyaya çıkarıldı. `dotnet build` ile `LangVersion=5` altında derlendi.
   - Sonuç: `Oluşturma başarılı oldu. 0 Uyarı, 0 Hata`.
   - Öz denetim: aynı projeye C# 6 özellikleri (`$"..."`, `?.`) içeren bir dosya eklendi. Derleme `CS8026 … C# 5'te kullanılamıyor` hatalarıyla reddetti, yani denetim etkili.
   - Bu derleme .NET 11'e karşıdır. .NET Framework `csc` ile derleme Windows'ta denenmedi.
2. **Kaba denge denetimi (ayrıştırıcı değil).**
   - Yöntem: Python ile yazılmış bir denetim. `()`, `{}`, `[]` eşleşmesini ve tek/çift tırnak, here-string ve yorum kapanmasını tarar.
   - `calistir.ps1` ve `mac_denetim.ps1` için sonuç `denge hatasi: 0`.
   - Öz denetim: bir ayraç silinince 1 hata, bir tırnak silinince 48 hata buldu. Here-string kapanışındaki biçim bozukluğunu (`'@` sonrası boşluk) yakalamadı.
   - Bu denetim PowerShell ayrıştırmasının yerine geçmez. Betik ASCII'dir (`grep` ile 0 ASCII dışı bayt).
3. **Godot komut satırı, 20 s ölçüm (W2, 2. tur).**
   - Komut satırı, betiğin görünür kampanya koşusu için kurduğu argümanlardır: `Olcum-Arglari` işlevinin sırası, ısınma 6 s, `--ornek=300`. Ölçüm süresi 20 s. Satır işlevden elle aktarıldı; betiğin kendisi çalıştırılamadığı için kurucunun gerçek çıktısıyla karşılaştırılamadı:

     ```
     Godot --path <depo>/tools/olcum/mp6/proje --position 160,160 --resolution 480x360 --always-on-top
           --log-file <çıktı>/godot_log.txt -- --yol=b --durum=gorunur --sure=20 --isinma=6 --cikti=<çıktı> --ornek=300
     ```

   - Süreç: 20:40:48 – 20:41:15 (toplam yaklaşık 27 s). Godot kendiliğinden kapandı, çıkış kodu 0, arkada süreç kalmadı.
   - Kare: `ozet.json` istek 600, üretildi 600. Ölçülen ölçüm süresi 20,007 s. İç gözlem "frames_drawn ilerliyor", 20/20 aralık.
   - Gecikme ve geri okuma: istekten hazıra p50/p99 6,22/8,15 ms; `get_image()` geri okuma p50/p99 4,34/6,12 ms.
   - Konum hatası ort/maks 0,140/0,309 px; gölge 600/600 karede.
   - İçerik: MP6a referansıyla (`20261115c/b_gorunur_2`) aynı k için MD5 600/600 eşit.
   - Kayıtlar: dökümler k = 0 ve 300. Başlangıç bandı hem `--log-file` dosyasında hem standart çıktıda: `Metal 4.0 - Forward+ - Using Device #0: Apple - Apple M3 Pro (Apple9)`. Standart hata boş.
   - Çözümleme: `analiz_win.py` bu koşuyu `kosul_disi` diye işaretledi (`sure_s=20.0 (beklenen 120.0); istek=600 (beklenen 3600)`). md.2'nin istediği davranış budur. Değiştirilmemiş `mp6/analiz.py` de aynı koşuyu hatasız okudu.
   - 1. turdaki 12 s'lik koşu (360/360 kare) bu koşuyla değiştirildi.
4. **Çözümleyici senaryoları (2. tur).** MP6a Mac ham verisinden sentetik Windows düzenleri kuruldu. Koşular dondurulan koşullara kısaltıldı: durum koşuları 120 s / 3600 istek, okuma koşusu 60 s / 1800 istek. Yapay dış gözlem ölçüm aralığına göre 10 s'de bir üretildi. Sonuçlar:

   | Senaryo | Beklenen | Çıkan |
   |---|---|---|
   | kusursuz | H1, H2 ×3, H3 ×2 geçer; H4 raporlanır | Aynı |
   | örtü bir örnekte ölçüm penceresini kapsamıyor | H2 örtülü geçmez | `gecersiz (durum tutulmadi)`; H3 örtülü `dogrulanmadi` |
   | örtülü koşuda geometri doğrulanamıyor (DPI bağlamı ya da dış çerçeve yok) | doğrulanmadı | `dogrulanmadi` |
   | görünür koşuda ölçüm aralığında bir gözlem hatası satırı | doğrulanmadı | `dogrulanmadi` |
   | simge koşusunda 40 s'lik gözlem boşluğu | doğrulanmadı | `dogrulanmadi (en büyük boşluk 40,0 s)` |
   | görünür referansın `kare.csv`'si yok | çözümleme durmaz, H3 değerlendirilemez | H3 ×2 `degerlendirilemedi (referans geçersiz)`; simge, örtülü ve okuma çözümlendi; çıkış 0 |
   | simge koşusunda `surec_temiz false kapatildi` | düzenek çalışmadı | H2 ve H3 simge `duzenek_calismadi` |
   | H1: yenileme 4294967295, dikey çözünürlük yok, güç `Unknown`, `powercfg` çıkış 1 | H1 geçmez | `gecmedi`; dört alan tek tek listelendi |

   | görünür H2 "doğrulanmadı" (ölçüm aralığında gözlem hatası), referans MAD 0 (2. düzeltme turu, sol-2 Y1) | H3 değerlendirilemez | H3 ×2 `degerlendirilemedi (görünür H2 'dogrulanmadi')` |
   | görünür koşunun `kos.log`'unda `surec_temiz` yok, MAD 0 (sol-2 Y1) | H3 değerlendirilemez | H3 ×2 `degerlendirilemedi (süreç bitişi kaydı yok)`; görünür H2 `dogrulanmadi` |

   Kusursuz senaryoda H4, MP6a ham verisinin ilk 60 s'sinden p50/p99 5,30/5,68 ms verdi. 2. düzeltme turundan önce "gözlem hatası" senaryosunda H3 "geçti" çıkıyordu (sol-2 Y1); artık "değerlendirilemedi".
5. **Godot Windows ikilisi (1. tur, değişmedi).** `Godot_v4.7.2-stable_win64.exe.zip` resmî adresten Mac'e indirildi.
   - Boyut 86 013 866 B.
   - SHA-512 ve SHA-256 betikteki sabitlerle aynı.
   - Arşivin kökünde `Godot_v4.7.2-stable_win64.exe` var. Betiğin beklediği yol budur.
6. **`tools/olcum/mp6/` temiz.** `git status --ignored` bu dizinde değişiklik ya da üretilmiş dosya göstermiyor. `analiz_win.py` bytecode yazmıyor.

## 5. Godot Windows ikilisinin özeti

| | Değer | Kaynak |
|---|---|---|
| Dosya | `Godot_v4.7.2-stable_win64.exe.zip`, 86 013 866 B | `gh api repos/godotengine/godot-builds/releases/tags/4.7.2-stable` (2026-11-19). Sürüm 2026-08-18'de yayımlanmış |
| SHA-512 | `83decd58fdf67b9d657958a1ae6bf1929c20785315a81effe245874cdc57acb709bf868e00778a96984338c1b29dafdb453c6847747694621c6ecf5da2259993` | Aynı sürümün `SHA512-SUMS.txt` varlığı. 2026-11-19'da indirilen kopya, MP6a'da 2026-11-14'te indirilenle `cmp` ile aynı; dosyanın kendi GitHub özeti `sha256:b8bdff6704f833e7a021df16ec08e1478edd5a285c1f894929b91a7f74a5c7c0` ile uyuşuyor |
| SHA-256 | `731980f9608d61333e5baf54a2ef17210acc7a538446c0cb9969f002aca1e953` | GitHub sürüm API'sindeki varlık `digest` alanı. sol bunu resmî sürüm varlıklarından bağımsız doğruladı (`MP8-sol.md`) |

SHA-512 değerini sol bağımsız doğrulayamadı: `SHA512-SUMS.txt` içeriğine erişemedi (`MP8-sol.md`). Betik iki özeti birlikte denetler. sol'un doğruladığı SHA-256 tek başına da indirilen dosyayı bağlar.

## 6. Ölçütler

### 6.1 Paket hazırlığı (bu işin bitiş ölçütleri)

| # | Ölçüt | Sonuç | Dayanak |
|---|---|---|---|
| W1 | `calistir.ps1` `pwsh` ile sözdizimi hatasız ayrışır; `-KuruCalistir` Mac'te uçtan uca koşar ve bütün adımları listeler | **Tamamlanmadı.** Betiğin ayrıştırılması ve kuru çalıştırması mimar ya da kullanıcı tarafından yapılacak | Ortam `pwsh` çağrısını reddediyor; dolaylı yol kullanılmadı (4. başlık). Yerine yapılanlar: C# 5 derlemesi ve kaba denge denetimi (4. başlık, madde 1–2). Komutlar aşağıda |
| W2 | Paketin kullandığı Godot komut satırı Mac'te 20 saniyelik koşuda kare üretir (yol b) | **Kısmi doğrulama.** Kare üretildi; komut satırı betiğin kurucusundan elle aktarıldı | 4. başlık, madde 3: 20 s ölçüm, 600/600 kare, MD5 MP6a ile 600/600 eşit, çıkış 0. Betiğin kendi kurduğu satır, kuru çalıştırma çıktısıyla karşılaştırılarak doğrulanmalı |
| W3 | `BENIOKU.md` tek komutla çalıştırmayı ve çıktının yerini eksiksiz anlatır | Yazarın değerlendirmesi: geçti | `BENIOKU.md` başlıkları: "Çalıştırma (tek komut)", "Çıktı nerede, mimara ne verilecek". Bağımsız inceleme bekliyor |
| W4 | Windows'ta sınanmayan her şey raporda madde madde yazılıdır | Geçti | 8. başlık |

**W1 için mimarın ya da kullanıcının koşturacağı komutlar** (Mac, depo kökü, PowerShell 7):

```
pwsh -NoProfile -File tools/olcum/windows/mac_denetim.ps1 tools/olcum/windows/calistir.ps1
pwsh -NoProfile -File tools/olcum/windows/calistir.ps1 -KuruCalistir
```

Beklenen sonuçlar:

- İlk komut `1) ayristirma hatasi: 0` … `SONUC: sorun 0` yazar.
- İkinci komut hiçbir şey indirmez, Godot'yu başlatmaz, klasör açmaz ve sürece dokunmaz. Bütün adımları `KURU:` satırlarıyla listeler ve sonda "KURU CALISTIRMA bitti" yazar.

W2'nin tam doğrulaması için ikinci komutun yazdığı `KURU: b_gorunur` satırı ile 4. başlık, madde 3'teki satırın argüman sırası karşılaştırılır. Fark yalnız yollar ve `--sure` olmalıdır. İsteğe bağlı 5.1 uyumluluk denetimi BENIOKU'da.

### 6.2 Windows ölçümü (kullanıcı çalıştırınca; görev belgesi dondurur, hedeftir)

| # | Ölçüt | Sonuç | Dayanak |
|---|---|---|---|
| H1 | Donanım kaydı eksiksiz (işlemci modeli dahil) | | |
| H2 | Görünür, simge ve örtülü durumların her birinde yol (b) ile 120 s'de düşen kare < %1 ve kare kimliği kesintisiz | | |
| H3 | Simge ve örtülü kareler görünür referansla ortalama mutlak fark ≤ 2/255 | | |
| H4 | 640 × 480 eşzamanlı geri okuma p50/p99 ve gerçekleşen kare hızı raporlanır (eşik yok) | | |
| H5 | Hangi çizim yöntemiyle (Forward+ / Mobile / uyumluluk) çalıştığı kayıtlı | | |

H2 ya da H3 geçmezse "geçmedi" geçerli sonuçtur.

## 7. Sonuçlar (kullanıcı çalıştırınca doldurulacak)

### 7.1 Donanım

| Kalem | Değer |
|---|---|
| İşlemci (model, çekirdek / iş parçacığı) | |
| Bellek (toplam, modüller, hız) | |
| Ekran kartı ve sürücü sürümü | |
| Ekran çözünürlüğü ve yenileme hızı | |
| Windows sürümü ve derleme | |
| Güç kaynağı ve güç planı | |
| Ekran koruyucu | |

### 7.2 Çizim yöntemi denemeleri

| Deneme | Açıldı mı | Üretilen / istek | Fiilî yöntem (bant) | Geri düşme uyarısı |
|---|---|---|---|---|
| varsayılan | | | | |
| mobile | | | | |
| opengl3 | | | | |

### 7.3 Durum × yol (b) (120 s, 30 Hz, 640 × 480, `get_image()`)

| Durum | Koşul | Süreç temiz | İstek | Üretilen | Düşen % (neden) | Kimlik boşluğu | Gecikme p50/p99/maks ms | İç gözlem | Dış gözlem (hüküm; örnek; en büyük boşluk) | MD5 eşit / karşılaştırılan | MAD ort/maks (/255, n) | Konum hatası ort/maks px | Gölge var % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| görünür | | | | | | | | | | referans | – | | |
| simge | | | | | | | | | | | | | |
| örtülü | | | | | | | | | | | | | |

### 7.4 Geri okuma (görünür, 60 s, `--geri=esz --cift=1`)

| İlk okuma p50/p99/maks ms | Yinelenen okuma p50/p99/maks ms | İstekten hazıra p50/p99 ms | Üretilen kare Hz | Operatör çizim Hz (medyan) | Döngü Hz (medyan) |
|---|---|---|---|---|---|
| | | | | | |

### 7.5 Süreç ve ortam

| Koşu | Godot işlemci % (tek çekirdek) | Öncelik sınıfı | Yanıt vermedi | Ortam yükü (koşu öncesi) |
|---|---|---|---|---|
| | | | | |

## 8. Windows'ta sınanmayanlar

Paketin hiçbir parçası Windows'ta koşmadı. Ayrıntılı liste:

**Betiğin kendisi**

1. `calistir.ps1`'in Windows PowerShell 5.1 altında koşması: ayrıştırma, işlev kapsamları, `Adim` sarmalayıcısı (`-Sinirli` anahtarı dahil), çıkış kodu. Betiğin son sürümü hiçbir PowerShell ayrıştırıcısından geçmedi: `pwsh` bu ortamda yok, 1. turdaki dolaylı ayrıştırma geçersiz ve eski sürüm içindi. Yalnız kaba denge denetimi (4. başlık, madde 2) ve satır satır okuma yapıldı.
2. BOM'suz ASCII dosyanın PS 5.1 tarafından okunması. Dosya ASCII olduğu için sorun beklenmez; denenmedi.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File ...` komutunun yürütme ilkesi engelini aşması. Grup ilkesiyle zorlanmış bir ilke bunu geçersiz kılar.
4. Küresel 15 dk sınırı: `Adim -Sinirli`, `Sure-Yeter`, kullanıcı beklemesi ve indirme süresinin sınıra eklenmesi (`Siniri-Ertele`).

**Donanım kaydı**

5. Her alt kalemin sorgusu:
   - CIM sınıfları: `Win32_Processor`, `Win32_ComputerSystem`, `Win32_PhysicalMemory`, `Win32_OperatingSystem`, `Win32_VideoController`, `Win32_Battery`, `Win32_Desktop`, `Win32_PerfFormattedData_PerfOS_Processor`
   - `Get-WmiObject` yedeği
   - `System.Windows.Forms.Screen` ve `SystemInformation.PowerStatus`
   - `Get-PSDrive`
6. `powercfg /getactivescheme`:
   - `$LASTEXITCODE` ile çıkış kodunun alınması
   - Türkçe Windows'ta GUID ve plan adının ayrıştırılması (konsol kod sayfası Türkçe harfleri bozabilir; ham çıktı ayrıca saklanır)
7. `Win32_Desktop` adının `*<kullanıcı adı>` ile eşleşmesi.
8. Windows 11 "güç modu" katmanı (en iyi performans vb.) kaydedilmiyor; yalnız etkin güç planı kaydediliyor.

**Kullanıcı etkileşimi**

9. `Read-Host` soruları: prize takılı değil uyarısı ve özet bilinmiyor dalı.

**Godot'nun indirilmesi ve açılması**

10. `Invoke-WebRequest` (TLS 1.2, GitHub yönlendirmesi, `-UseBasicParsing`) ve `WebClient` yedeği.
11. `Get-FileHash` ile SHA-512 ve SHA-256 hesabı ve karşılaştırma. Değerlerin kendisi Mac'te doğrulandı (4. başlık, madde 5).
12. Uyuşmayan zip'in kenara alınması.
13. Zip'in açılması: `ZipFile.ExtractToDirectory`, yedeği `Expand-Archive`.
14. Önceki açılmış dizin silinemediğinde verilen hata.
15. `Get-AuthenticodeSignature` ile imza kaydı.
16. Önbellekteki zip'in yeniden kullanılması (ikinci çalıştırma).

**Pencere gözlem yardımcısı ve uyku engeli**

17. C# yardımcısının .NET Framework `csc` ile derlenmesi. Mac'te yalnız .NET 11 SDK ile `LangVersion=5` derlemesi yapıldı.
18. Bütün P/Invoke çağrıları:
    - pencere ve süreç: `EnumWindows`, `GetWindowThreadProcessId`, `IsWindowVisible`, `IsIconic`, `GetWindowRect`, `GetClientRect`, `ClientToScreen`
    - en üstteki pencere ve sıra: `WindowFromPoint`, `GetAncestor(GA_ROOT)`, `GetForegroundWindow`, `GetWindow(GW_HWNDPREV)`
    - özellikler: `GetWindowTextW`, `GetClassNameW`, `GetWindowLongW`
    - DWM: `DwmGetWindowAttribute` (`DWMWA_CLOAKED`, `DWMWA_EXTENDED_FRAME_BOUNDS`)
    - DPI: `SetThreadDpiAwarenessContext(PER_MONITOR_AWARE_V2)` ve geri alınması
19. Ana pencerenin başlıktan seçilmesi. Başlığın "IDA MP6a" ile başladığı varsayılır; Godot'nun Windows'taki başlığı denenmedi.
20. Ekran ölçeklemesi (DPI) altında geometri denetiminin doğruluğu.
    - Doğrulanmayanlar: iş parçacığı bağlamı açıkça ayarlandıktan sonra dış çerçevelerin ve Z sırasının tutarlı gelmesi; `SetThreadDpiAwarenessContext`'in Windows 10 1607 öncesinde bulunmaması (o durumda geometri "doğrulanamadı" olur).
    - Microsoft belgelerine göre:
      - `GW_HWNDPREV` Z sırasında hemen üstteki pencereyi verir.
      - `DWMWA_EXTENDED_FRAME_BOUNDS` "ekran uzayında genişletilmiş çerçeve dikdörtgeni"dir.
      - `SetThreadDpiAwarenessContext` yalnız çağıran iş parçacığını değiştirir, geçersiz bağlamda NULL döner ve en az Windows 10 1607 ister.
      - Kaynaklar: learn.microsoft.com `nf-winuser-getwindow`, `ne-dwmapi-dwmwindowattribute`, `nf-winuser-setthreaddpiawarenesscontext` (2026-11-19).
    - DWM sayfasında DPI'ye ilişkin bir not yok. İki dikdörtgenin aynı koordinat düzeninde olduğu, aynı iş parçacığı bağlamında okunmalarına dayanan bir varsayımdır (doğrulanmadı).
    - Aynı belge, `GetWindow`'un döngü içinde çağrılmasının sonsuz döngü ya da yok edilmiş pencere tutamacı riski taşıdığını söyler. Yürüyüş 100 000 adımla sınırlıdır. Gözlem sırasında Z sırası değişirse tek örnek yanlış sonuç verebilir.
    - Bu çağrıların davranışı Windows'ta denenmedi.
21. `SetThreadExecutionState`'in uyku ve ekran kapanmasını gerçekten ertelemesi. Betiğin tek iş parçacığında koştuğu varsayılır.

**Godot süreçlerinin yönetimi**

22. `Start-Process` ile GUI Godot ikilisinin standart çıktı ve hata yönlendirmesi. Godot kaynağına göre korunur (3.2); denenmedi.
23. PS 5.1'de `ExitCode` için `Handle` erken okuma; temiz bitiş denetimi çıkış koduna dayanır.
24. Yalnız kendi süreç nesnelerinin `Process.Kill` ile kapatılması.
25. Ölçüm sürecinin kendiliğinden kapanması için 10 s'lik ek bekleme.
26. Örtünün erken kapandığının algılanması.
27. Ctrl+C'de `finally` bloklarıyla temizlik.
28. Boşluk ya da Türkçe harf içeren kullanıcı yollarının (ör. OneDrive `Masaüstü`) tırnaklanması.
29. Godot'nun `C:/...` biçimindeki `--path`, `--log-file` ve `--cikti` yollarını kabul etmesi.

**Godot'nun Windows davranışı**

30. Vulkan, D3D12 ve OpenGL arasındaki kendiliğinden geri düşmeler.
31. Başlangıç bandının biçimi. Mac'te yalnız Metal/Forward+ bandı görüldü; D3D12, Vulkan ve OpenGL bantları kaynak koddan çıkarıldı.
32. "Unable to initialize video driver" uyarı kutusunun davranışı.
33. Deneme sırasının (varsayılan, mobile, opengl3) gerçekten çalışması.

**Pencere durumları**

34. `window_set_mode(MINIMIZED)` ile simge durumunun Windows'ta kurulması. Godot kaynağında `ShowWindow(SW_MINIMIZE)`; davranışı denenmedi.
35. Örtü penceresinin (40,40; 1100 × 900; her zaman üstte) düşük çözünürlüklü ekranda (ör. 1366 × 768) ve ölçeklemeyle ölçüm penceresini tamamen kapsaması. Artık geometriyle denetlenir; kapsamazsa hüküm "tutulmadı" olur.
36. Windows'un ön plan kilidinin pencerelere etkisi.
37. Örtülü durumda Godot'nun çizmeye devam etmesi ve bunun sonucunda `olcum.gd`'nin 30 s geçiş sınırına takılması. Kaynağa ve 07 §2.6'ya göre beklenir; ölçülmedi.

**Süre, dökümler ve çıktı**

38. Kampanya süresinin 10 dakikanın altında kalması. 9–9,5 dk hesaptır, ölçülmedi.
39. Kare dökümlerinin toplam boyutu ve 50 MB seyreltmesi. Hesap yaklaşık 33 MB; Compatibility'de RGBA dönerse yaklaşık 44 MB.
40. Zip oluşturma: `ZipFile.CreateFromDirectory`, yedeği `Compress-Archive`.
41. Masaüstü yolunun bulunması ve OneDrive eşitlemesinin ölçüme etkisi.
42. Ekran koruyucunun koşu sırasında açılması. `SetThreadExecutionState` bunu engellemez (sol Microsoft belgesiyle doğruladı).

**Çözümleme**

43. `analiz_win.py`'nin gerçek Windows çıktısını okuması. Yalnız Mac koşusu ve sentetik `pencere_win.jsonl` ile denendi. Sentetik dosya C# yardımcısının yazdığı alan adlarıyla kuruldu; C# çıktısının kendisi hiç üretilmedi.
44. Gözlem zamanının ölçüm aralığına taşınması. `hazir` dosyasındaki Godot saati (`Time.get_ticks_usec`, Windows'ta QueryPerformanceCounter) ile betiğin kronometresinin aynı hızda ilerlediği varsayılır; tolerans 1 s.

## 9. Doğrulanmayanlar ve sınırlar

- **`pwsh` ve W1.** Bu ortam `pwsh` çağrısını reddediyor.
  - 1. turda `pwsh` bir Python başlatıcısıyla dolaylı yoldan çağrıldı. Mimarın kararıyla bu yapılmamalıydı; o koşunun sonucu kanıt sayılmaz.
  - Bu turda `pwsh` hiçbir yolla çalıştırılmadı.
  - Bu yüzden betiğin son sürümünün ayrıştırılması, `-KuruCalistir` ve PSScriptAnalyzer `PSUseCompatibleSyntax` (5.1) denetimi yapılmadı. Komutlar 6.1'de.
- **W2'nin komut satırı.** Komut satırı betiğin argüman kurucusundan elle aktarıldı; betiğin kendisi Godot'yu başlatmadı. Betiğin `Start-Process`, bekleme, izleme, temiz bitiş ve kapatma mantığı Mac'te de denenmedi.
- **Mac koşusunun kapsamı.** 20 s'lik tek görünür koşudur. Simge ve örtülü durumlar ile geri okuma koşusu Mac'te bu paketle koşturulmadı; Mac'teki kanıtları MP6a'dadır.
- **Çözümleyici sınaması.** Sentetik veriler MP6a Mac ham verisinin kısaltılmasıyla kuruldu: `sure_s` ve `istek` alanları 120 s / 3600 olacak biçimde yeniden yazıldı. Bunlar yalnız hüküm mantığının sınamasıdır; ölçüm sonucu değildir.
- **Kaynak kod dayanakları.** Godot davranışına ilişkin iddialar `godotengine/godot` deposunun `4.7.2-stable` etiketindeki dosyalardan okundu (3.2, 3.4). Windows'ta davranış olarak gözlenmedi.
- **Süre.** Süre ve kare hızı için Windows tahmini yazılmadı. 3.5'teki süre yalnız ölçüm sürelerinin ve sabit beklemelerin toplamıdır.
