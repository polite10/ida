# MP6a — Godot 4.7.2'de görünmeyen pencerede kamera karesi (Mac ön turu)

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP6a.md` (ilk turdan sonraki ve tam kampanyadan sonraki kararlar dahil) |
| Dal | `olcum/mp6-godot-kamera` |
| Tarih | Tam kampanya 2026-11-14 23:12 – 2026-11-15 00:21. Düzeltme turu yinelemeleri 2026-11-15 12:02 – 12:16 |
| Uygulayan | `ida-uygulayici` (Opus). Tam kampanyayı mimar koşturdu |
| İnceleme | sol: `docs/olcumler/inceleme/MP6-sol.md`. Yanıt: `docs/olcumler/inceleme/MP6-yanit.md` |
| Durum | Dört ölçüt işaretlendi. Yol (a) birincil yol olarak elendi (mimar kararı). Yol (a)'nın geçersiz iki koşusu yinelenmedi |

## 1. Ortam

| Kalem | Değer |
|---|---|
| Makine | Apple M3 Pro, 18 GB (`hw.memsize` 19327352832), dahili Liquid Retina XDR ekran (tek ekran) |
| OS | macOS 26.5.2 (25F84) |
| Güç | Tam kampanya prizde (`AC attached`), `caffeinate -dims` altında (mimar). Düzeltme turu yinelemeleri pilde: %71 → %67 (`pmset -g batt`, `yineleme.log`), `caffeinate -dims` altında. Her yinelemeden önce `caffeinate -u -t 2` ile kullanıcı etkinliği bildirildi. Sistem ayarı değişmedi. `lowpowermode 0` |
| Godot | 4.7.2-stable official, commit `ed1daf0bf001b61586d9930840f2f1394092c079`, standart sürüm (.NET değil), evrensel macOS ikilisi |
| İndirme | `Godot_v4.7.2-stable_macos.universal.zip` (resmî `godotengine/godot-builds` sürüm sayfası). SHA-256 `c58a24e31d720be9d62f60cb5627c4e695fb72f21b0cfe1bc9ccaa9a3b3ba63e`: sürüm varlığının yayımlanan özetiyle aynı. SHA-512 `38aa16e5…7bac69ca`: indirilen `SHA512-SUMS.txt` ile aynı (sol bu dosyaya erişemedi, bağımsız doğrulamadı). İmza `TeamIdentifier=6K46PWY5DM` |
| Kurulum yeri | `${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/`; sistem geneline kurulum yok |
| Görüntüleyici | Metal 4.0, Forward+, "Apple M3 Pro (Apple9)" |
| Proje ayarları | `vsync_mode=0`, `max_fps=120`, `thread_model=1` (varsayılan; ayrı render iş parçacığı yok), `frame_queue_size=2` (varsayılan). MSAA, TAA, FXAA ve debanding kapalı. Doğrusal ton eşleme |
| Derleyici, kart | Yok. Yalnız GDScript kullanıldı; C++ uzantısı ve kart bu işte yok |

## 2. Düzenek

Kod: `tools/olcum/mp6/` (atılabilir prototip).

| Dosya | İş |
|---|---|
| `proje/olcum.gd` | Sahne ve kamera `SubViewport`'u; istek zamanlaması, geri okuma, kare başına kayıt. Örtü rolünde tam ekran kipini 15 s'de bir yazar |
| `kos.sh` | Tek koşu. Ölçüm sürecini başlatır, durumu oluşturur, 15 s'de bir dış gözlem alır, çıkışta bütün süreçleri kapatır |
| `kampanya.sh` | Tam kampanya: durum × yol, geri okuma, başsız |
| `pencere.js` | JXA. `NSRunningApplication.hide` ve `CGWindowListCopyWindowInfo` (pencere sahibi ve katmanı dahil). Başka uygulamaya Apple Event göndermez, izin istemez |
| `analiz.py`, `png.py` | Çözümleme (`--ref` ile açık referans seçimi) ve ham kareyi PNG'ye çevirme |

**Sahne.** Operatör penceresi 480 × 360 px (Retina'da 240 × 180 pt) ve kendi `Camera3D`'sini kullanır. Kamera `SubViewport`'u 640 × 480'dir, `own_world_3d = false` (aynı `World3D`), ve `UPDATE_DISABLED` kipinde başlar.

İçerik:

- Gölge düşüren yönlü ışık, gri zemin ve sabit bir direk.
- Gölgelemesiz kırmızı bir küre. Konumu kare kimliği `k`'den hesaplanır; kare başına yaklaşık 3–4 px yer değiştirir.
- Sol üstte 24 bitlik kare kimliği şeridi. Üst satır `k`'yi, alt satır tümleyenini taşır; çözülemeyen şerit "içerik geçersiz" sayılır.

Küre konumu ve şerit `RenderingServer.instance_set_transform` ile `canvas_item_set_visible` üzerinden doğrudan yazılır. Böylece `force_draw` onları aynı çağrıda çizebilir.

**İstek.** Monoton saatle 30 Hz yuvalar kullanılır: `k = ⌊(t − t0) / 33,333 ms⌋`. Saat `Time.get_ticks_usec` (macOS'ta `mach_absolute_time`). Atlanan yuva "döngü gecikti" sayılır. Her istekte `viewport_set_update_mode(ONCE)` çağrılır.

- **Yol (a):** olağan çizim döngüsü. Kare `RenderingServer.frame_post_draw` içinde okunur.
- **Yol (b):** `_process` içinde `RenderingServer.force_draw(false, 1/30)` çağrılır, ardından kare okunur.

**Geri okuma.** Durum koşularında `SubViewport.get_texture().get_image()` kullanılır (eşzamanlı; 07 §2.5'teki yedek yol). Okuma koşularında ayrıca iki yol ölçülür:

- `rdesz`: `RenderingDevice.texture_get_data` (eşzamanlı, dönüşümsüz).
- `async`: `RenderingDevice.texture_get_data_async`.

Eşzamanlı okuma koşularında (`--cift=1`) aynı doku hemen ikinci kez okunur. Bu, **yeni çizim olmadan yinelenen eşzamanlı geri okuma**dır. İkinci okuma da yeni bir ara tampon, GPU kopyası, gönderim ve `_flush_and_stall_for_all_frames()` içerir, yani saf aktarım süresi değildir. Hazır olma zamanı ilk okumadan hemen sonra kaydedilir. Bu düzeltmeden önceki `--cift=1` koşularında hazır zamanı ikinci okumadan sonra alınıyordu (4.4).

**Kare başına kayıt** (`kare.csv`):

- Zamanlar: planlanan zaman, istek zamanı, hazır zamanı, gecikme, ilk geri okuma süresi.
- Kimlik ve özet: çözülen kimlik, MD5.
- Konum: kürenin kırmızı maske ağırlık merkezi ve yer gerçeği izdüşümü (`Camera3D.unproject_position`). Küre bulunamazsa ağırlık merkezi −1 yazılır ve kare "küre bulunamadı" sayılır.
- Gölge oranı ve sonuç.

Her 30. kare ham olarak dökülür.

**Gölge ölçütü** (ölçümden önce donduruldu, mimar kabul etti): direğin orta yüksekliğinin zemindeki gölge noktası ile 1,2 m ötedeki aydınlık zemin noktasında 5 × 5 px parlaklık oranı ≤ 0,75 ise gölge var sayılır.

**İçerik referansı.** `20261115c/b_gorunur_2` koşusu (k = 0…8999). Bu koşu kesintisiz 300 s görünür tutuldu (4.2). Her koşu `analiz.py --ref` ile bu referansa göre karşılaştırıldı:

- MD5 bütün karelerde karşılaştırıldı.
- Ortalama mutlak fark (RGB, /255) dökülen her 30. karede hesaplandı.

Tablolar koşu başına karşılaştırılan ve karşılaştırılmayan kare sayısını verir.

**Durumun oluştuğunun ve sürdüğünün doğrulanması:**

1. İç gözlem. `Engine.get_frames_drawn()` yalnız `Main::iteration` çizdiğinde artar (`main/main.cpp` 5080–5096). Görünür durumda ilerlemesi, diğer durumlarda durması gerekir. Ölçüm, bu koşul 1,5 s sağlandıktan sonra başlar. Koşu boyunca yaklaşık 1 s'lik kayıt aralıklarıyla izlenir (`durum.csv`).
   - "Durum tutuldu" sütunu **kayıt aralığı sayısı**dır (tutulan / toplam).
   - Parantezdeki süre aralıkların zaman damgalarından toplanır.
2. Dış gözlem, 15 s'de bir:
   - `CGWindowList`: ölçüm penceresi ekranda mı; örtü önde ve onu kapsıyor mu; ekrandaki tek pencere örtü mü (boyutu doğrulanmaz: gerçek ekran sınırı ölçülmüyor); üst katman pencereleri.
   - `NSRunningApplication.hidden` ve `ps` önceliği.
   - Simge durumunda ayrıca `window_get_mode() == MINIMIZED`.

| Durum | Nasıl oluşturuldu |
|---|---|
| görünür | Ölçüm penceresi `--always-on-top` |
| örtülü | İkinci bir Godot süreci, her zaman üstte, 550 × 482 pt. Ölçüm penceresini tamamen kapsar |
| gizli | Uygulamanın gizlenmesi (`NSRunningApplication.hide`, Cmd+H eşdeğeri) |
| simge | `DisplayServer.window_set_mode(WINDOW_MODE_MINIMIZED)` |
| tam ekran | İkinci Godot süreci `--fullscreen` (macOS yerel tam ekran, ayrı Space) |

## 3. Komutlar ve ham veri

```
tools/olcum/mp6/kampanya.sh ${IDA_CACHE:-$HOME/.cache/ida}/mp6/ham/20261114b          # tam kampanya
caffeinate -dims tools/olcum/mp6/kos.sh b gorunur 300 …/20261115c/b_gorunur_2           # düzeltme turu
caffeinate -dims tools/olcum/mp6/kos.sh b gorunur 120 …/20261115c/okuma_640x480_esz --geri=esz --cift=1 --ornek=0
python3 -I tools/olcum/mp6/analiz.py --ref …/20261115c/b_gorunur_2 …/20261114b
```

Ham veri depoya girmez. Konumu `${IDA_CACHE:-$HOME/.cache/ida}/mp6/ham/`:

- `20261114b/`: tam kampanya, 19 koşu. Özetler `ozet_tablo.json` (varsayılan referans) ve `ozet_tablo_ref_20261115c.json` (yeni referans).
- `20261115c/`: düzeltme turu. `b_gorunur` (1. deneme), `b_gorunur_2`, `okuma_640x480_esz`, `yineleme.log`.

Godot ayrıca `~/Library/Application Support/ida_mp6/` altına shader önbelleği yazar.

## 4. Sonuçlar

### 4.1 Durum × yol (300 s, 30 Hz, 9000 istek, 640 × 480, `get_image()`)

Referans: `20261115c/b_gorunur_2`.

| Yol | Durum | Koşu | Durum tutuldu: aralık (süre) | Dış gözlem (15 s'lik örnekler) | Üretilen | Düşen % (neden) | Kimlik boşluğu | Gecikme p50/p99/maks ms | MD5 eşit / karşılaştırılan (karşılaştırılmayan) | Ort. mutlak fark ort/maks (n) | Konum hatası ort/maks px | Küre bulunamadı | Gölge var | Geçerli |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| b | görünür | `20261115c/b_gorunur_2` | 299/299 (300,0 / 300,0 s) | pencere 19/19 ekranda, her zaman üstte | 9000 | 0 | 0 | 6,39 / 7,13 / 11,49 | referans | – | 0,141 / 0,309 | 0 | 9000/9000 | evet |
| b | örtülü | `20261114b/b_ortulu` | 299/299 (300,0 s) | 19/19 örtü önde ve kapsıyor | 9000 | 0 | 0 | 6,91 / 8,32 / 8,91 | 9000/9000 (0) | 0 / 0 (300) | 0,141 / 0,309 | 0 | 9000/9000 | evet |
| b | gizli | `20261114b/b_gizli` | 299/299 (300,0 s) | 19/19 `hidden=true`, pencere ekranda değil | 9000 | 0 | 0 | 7,01 / 8,99 / 9,52 | 9000/9000 (0) | 0 / 0 (300) | 0,141 / 0,309 | 0 | 9000/9000 | evet |
| b | simge | `20261114b/b_simge` | 299/299 (300,0 s) | 19/19 pencere ekranda değil; kip 1 | 9000 | 0 | 0 | 5,97 / 8,50 / 9,35 | 9000/9000 (0) | 0 / 0 (300) | 0,141 / 0,309 | 0 | 9000/9000 | evet |
| b | tam ekran | `20261114b/b_tamekran` | 299/299 (300,0 s) | 19/19 ekrandaki tek pencere örtüydü (boyutu doğrulanmadı), ölçüm penceresi ekranda değil; örtü başlangıçta kip 3 bildirdi | 9000 | 0 | 0 | 7,21 / 9,06 / 10,33 | 9000/9000 (0) | 0 / 0 (300) | 0,141 / 0,309 | 0 | 9000/9000 | **durum doğrulanmadı** (gözlem) |
| a | örtülü | `20261114b/a_ortulu` | 299/299 (300,0 s) | 19/19 örtü önde ve kapsıyor | 0 | 100 (çizim yok) | – | – | – | – | – | – | – | evet |
| a | gizli | `20261114b/a_gizli` | 298/298 (300,0 s) | 19/19 `hidden=true` | 0 | 100 (çizim yok) | – | – | – | – | – | – | – | evet |
| a | simge | `20261114b/a_simge` | 299/299 (300,0 s) | 19/19 pencere ekranda değil; kip 1 | 0 | 100 (çizim yok) | – | – | – | – | – | – | – | evet |
| a | görünür | `20261114b/a_gorunur` | 109/299 (109,5 s; en uzun kesinti 168,4 s) | pencere 19 örneğin 4'ünde ekranda | 3125 | 65,28 (çizim yok 5875) | 5079 | 7,98 / 15,97 / 43,17 | 3125/3125 (0) | 0 / 0 (103) | 0,141 / 0,309 | 0 | 3125/3125 | **hayır, yinelenmedi** |
| a | tam ekran | `20261114b/a_tamekran` | 4/299 (4,0 s) | ekrandaki tek pencere örtü 0/19; ölçüm penceresi 19/19 ekranda | 8805 | 2,17 (çizim yok 195) | 146 | 7,67 / 14,54 / 24,07 | 8805/8805 (0) | 0 / 0 (293) | 0,141 / 0,309 | 0 | 8805/8805 | **hayır, yinelenmedi** |

Notlar:

- Gecikme istekten hazır olmaya kadar geçen süredir (monoton saat). Gölge oranı bütün koşularda 0,28–0,49 aralığındadır.
- Küre bütün koşularda bütün üretilen karelerde bulundu.
- Görünür olmayan koşularda tek uyumsuz kayıt aralığı yoktur.

### 4.2 Görünür koşular ve geçersiz koşular

- **`20261115c/b_gorunur_2`** G1 kanıtı ve içerik referansıdır. Bütün kayıt aralıklarında olağan döngü çizdi. Pencere 19 dış örneğin hepsinde ekranda ve her zaman üstteydi; üst katman (ekran koruyucu, kilit) penceresi görülmedi. Bu koşunun kareleri tam kampanyanın `b_gorunur` koşusuyla da 9000/9000 aynıdır.
- **`20261114b/b_gorunur`.** Ölçümün 108.–119. saniyelerinde, 10,1 s boyunca olağan döngü çizmedi. Pencere 15 s'lik örneklerde ekrandaydı. Nedeni doğrulanmadı. Bu koşu artık referans değil.
- **`20261115c/b_gorunur`** (düzeltme turunun 1. denemesi). Son 7,9 s'de olağan döngü çizmedi; pencere örneklerde ekrandaydı. Nedeni doğrulanmadı. Bu denemede üst katman pencereleri henüz kaydedilmiyordu.
  - Olası açıklama (çıkarım): ekran koruyucunun boşta süresi 600 s (`defaults -currentHost read com.apple.screensaver idleTime`). Koşudan önce kullanıcı uzun süre boşta kaldıysa ekran koruyucu koşu ortasında başlamış olabilir.
  - Bu yüzden geçersiz sayıldı. 2. denemeden önce `caffeinate -u` ile kullanıcı etkinliği bildirildi ve `pencere.js` üst katman pencerelerini kaydedecek biçimde genişletildi.
- **`20261114b/a_gorunur` ve `20261114b/a_tamekran`** geçersizdir ve mimar kararıyla **yinelenmedi**.
  - `a_gorunur`: kullanıcı başka bir Space'e geçmiş görünüyor.
  - `a_tamekran`: masaüstü 4 s sonra tam ekran Space'ten döndü; örtü 19 örneğin hiçbirinde ekrandaki tek pencere değil.
- **`20261114b/b_tamekran`: "başka uygulama tam ekranda" durumunun sürdüğü doğrulanmadı** (sol 2. inceleme md.1). Örtü başlangıçta tam ekran kipini (3) bildirdi ve 19/19 dış örnekte ekrandaki tek pencereydi. Ancak çözümleme gerçek ekran sınırlarını ölçmüyor; örtünün boyutu ve tam ekranda kaldığı kanıtlanmış sayılmaz. Bu koşunun 9000/9000 karesi ve bit düzeyinde eşitliği yalnız gözlemdir. Tam ekran durumu Windows turuyla birlikte yeniden ölçülecek. G2'nin üç zorunlu durumu bundan etkilenmez.
  - **Yol (a)'nın görünür ve tam ekran sonucu doğrulanmadı.** Önceki turun ilk ~100 s'lik kısmında yol (a) tam ekranda kare üretmemişti (4.7); bu, 5 dakikalık sonuç yerine geçmez.

### 4.3 Gizli pencerede asenkron kuyruk (`20261114b/okuma_640x480_async_gizli`, yol b, 120 s)

Durum 120/120 aralıkta tutuldu; 7 dış örnekte `hidden=true`.

- 3600 istekte 3598 kare. Koşu bittiğinde 2 istek hâlâ kuyruktaydı ("async gelmedi"). Bu, iki karenin kaybolduğunu kanıtlamaz.
- İstekten geri çağrıya: p50/p99/maks 58,56 / 70,52 / 72,72 ms.
- İstekten hazır olmaya: 69,52 / 77,58 / 78,58 ms.
- Görünür pencerede (`okuma_640x480_async`) istekten geri çağrıya 7,29 / 9,15 / 10,43 ms.

Yorum: kuyruk gizli pencerede `force_draw` çağrılarıyla ilerliyor. Çizim yalnız 30 Hz'lik `force_draw` olduğunda `frame_queue_size` (2) × 33,3 ms = 66,7 ms beklenir (hesap); ölçülen değer bununla uyumlu. Bu hesap tam gecikme modeli değildir: isteğin çizim çevrimindeki yeri ve ek çizimler sonucu etkiler. Godot kaynağında `swap_buffers(false)` da kare halkasını ilerletir.

### 4.4 Geri okuma maliyeti (yol b, 120 s, 3600 istek)

| Çözünürlük | API | Koşu | Durum tutuldu (aralık) | n | İlk okuma p50/p99/maks ms | Yeni çizim olmadan yinelenen eşzamanlı okuma p50/p99/maks ms | Çıktı |
|---|---|---|---|---|---|---|---|
| 640 × 480 | `ViewportTexture.get_image()` + `Image.get_data()` | `20261114b/okuma_640x480_esz` | 120/120 | 3600 | 5,33 / **5,69** / 5,94 | 1,37 / 1,80 / 2,36 | RGB8, 921.600 B |
| 640 × 480 | aynı (yineleme, düzeltilmiş hazır zamanı) | `20261115c/okuma_640x480_esz` | **15/120** (pencere ekranda değil) | 3600 | 4,14 / 7,21 / 10,95 | 1,38 / 2,46 / 5,84 | RGB8 |
| 640 × 480 | `RenderingDevice.texture_get_data` | `20261114b/okuma_640x480_rdesz` | 120/120 | 3600 | 4,89 / **5,32** / 5,57 | 0,95 / 1,36 / 1,61 | RGBA8, 1.228.800 B |
| 640 × 480 | `texture_get_data_async` (istekten geri çağrıya) | `20261114b/okuma_640x480_async` | 120/120 | 3600 | 7,29 / 9,15 / 10,43 | – | RGBA8 |
| 1280 × 720 | `get_image()` | `20261114b/okuma_1280x720_esz` | 120/120 | 3600 | 5,30 / 6,63 / 7,59 | 1,60 / 2,18 / 3,40 | RGB8, 2.764.800 B |
| 1280 × 720 | `texture_get_data` | `20261114b/okuma_1280x720_rdesz` | 120/120 | 3600 | 4,62 / 6,04 / 6,19 | 1,02 / 1,50 / 1,64 | RGBA8, 3.686.400 B |
| 1280 × 720 | `texture_get_data_async` (istekten geri çağrıya) | `20261114b/okuma_1280x720_async` | 112/120 | 3598 | 7,40 / **58,86** / 68,31 | – | RGBA8 |

Notlar:

- **İlk eşzamanlı okuma ölçülen bütün işi içerir.** Buna ara tampon, GPU kopyası, gönderim ve `_flush_and_stall_for_all_frames()` beklemesi dahildir; bekleme, o anda tamamlanmamış çizim işini de kapsar. İlk okumanın ne kadarının çizim beklemesi olduğu ayrıca ölçülmedi. İkinci okumayla aradaki fark yalnız bir üst sınır fikri verir.
- **1280 × 720 asenkron:** istekten geri çağrıya p99 58,86 ms ve maks 68,31 ms. İstekten hazır olmaya p50/p99/maks 7,77 / 68,00 / 70,51 ms. Bu koşuda 8 aralıkta (en uzun kesinti 7,3 s) pencere çizilemedi; kuyruk o sırada yalnız `force_draw` ile ilerledi. p99 bu aralıklardan etkileniyor.
- **640 × 480 eşzamanlı yineleme:** hazır zamanı düzeltmesiyle yinelendi. Ancak kullanıcının tam ekran uygulamasına geçtiği görülüyor (dış örneklerde ekranda yalnız başka bir uygulamanın iki penceresi var), bu yüzden durum 15/120 aralıkta tutuldu. Koşu görünür pencere koşulunda değildir. G4 için görünür pencerede ölçülmüş `20261114b` değeri kullanıldı. İlk okuma süresi hazır zamanı düzeltmesinden etkilenmez.
- Bu yinelemede düzeltilmiş istekten hazır olmaya süre p50/p99/maks 5,73 / 9,36 / 14,25 ms. `20261114b` okuma koşularının "gecikme" sütunu ikinci okumayı da içerdiği için bu raporda kullanılmadı. Durum koşularında (`--cift` yok) bu sorun yoktur.
- **Kopya sayısı (kaynaktan):**
  - `texture_get_data` her çağrıda yeni bir CPU görünür ara tampon açar, GPU kopyasını kuyruğa alır ve bütün kareleri bekler. Ara tampondan sıkı bir `Vector<uint8_t>`'ye 1 CPU kopyası yapar (satır adımı farklıysa satır satır).
  - `get_image()` ayrıca RGBA8 → RGB8 dönüşümü yapar: saydam olmayan render hedefinin `image_format`'ı RGB8'dir (`texture_storage.cpp:4289`) ve `texture_2d_get` `image->convert` çağırır (`texture_storage.cpp:1926–1927`).
  - Sürücü içindeki olası ek kopyalar ölçülmedi.

### 4.5 Başsız kip (`--headless`, 20 s, 600 istek)

Gözlenen ortam: `display_server = headless`, `RenderingServer.get_rendering_device() == null`, görüntü bağdaştırıcısı adı boş, `frames_drawn = 0`.

- Yol (a): 0 kare.
- Yol (b): `force_draw` hata vermeden döner, ama `get_image()` her istekte boş döner (`ERROR: Parameter "t" is null` at `servers/rendering/dummy/storage/texture_storage.h:110`). 0 kare.

Başsız kipte GPU kamera karesi üretilmez.

### 4.6 App Nap, kısma ve ana döngü

- Bütün geçerli 300 s'lik koşularda ana döngü saniyede medyan 118–121 kez döndü. 100'ün altındaki tek kayıt aralığı her koşunun son, kısmi aralığıdır.
- `ps` önceliği 97'de kaldı (başsız koşularda 31).
- Yol (b) isteğinin plandan sapması: p99 8,2–9,1 ms, en çok 19,7 ms.
- Görünmez durumlarda 5 dakika boyunca App Nap kaynaklı döngü kısması **gözlenmedi**. Bu, App Nap'ın devrede olmadığını kanıtlamaz; daha uzun süreler doğrulanmadı.
- Godot 4.7.2'nin macOS kaynağında App Nap'ı kapatan kod bulunmadı (7. başlık).

### 4.7 Önceki tur (2026-11-14 16:29–16:52, kısmi)

İlk kampanya pilde koştu.

- Kapak 16:51:52'de kapandı ve Mac "Clamshell Sleep"e girdi (`pmset -g log`).
- Geçerli kalanlar: yol (a)'nın örtülü, gizli ve simge koşuları ile yol (a) tam ekranın ilk ~100 s'si. Sonuç bu turla aynı: kare yok.
- Yol (b) yalnız kısa koşularla (10–25 s) sınandı: %0 düşme ve aynı içerik.
- Ham veri `…/mp6/ham/20261114/`, `deneme/`, `kuru/` altında. Sonuç tabloları bu verilere dayanmaz.

## 5. Geçme ölçütleri

| # | Ölçüt | Sonuç | Dayanak |
|---|---|---|---|
| G1 | Görünür pencerede en az bir yolla 5 dk boyunca düşen kare < %1 | **Geçti** (güç koşulu sapmasıyla, mimarca kabul edildi) | `20261115c/b_gorunur_2`: 9000 istekte 9000 kare, %0 düşme, kimlik boşluğu 0. Durum 299/299 aralıkta tutuldu (300,0 s). Pencere 19/19 örnekte ekranda. **Sapma:** bu koşu pilde alındı; görev belgesindeki "Mac prize takılı" şartı sağlanmadı (sol 2. inceleme md.2). Mimar pildeki kanıtı G1 ve G3 için kabul etti (görev belgesi, 2026-11-15). Gerekçe: koşu `caffeinate -dims` altında kesintisiz tamamlandı; kare sayımı, kimlik sürekliliği ve bit düzeyinde eşitlik güç kaynağına bağlı değil |
| G2 | Örtülü, gizli ve simge durumlarının her birinde en az bir yolla 5 dk boyunca düşen kare < %1 ve kesintisiz kimlik | **Geçti (yol b)** | `force_draw` üç durumda da 9000/9000 kare üretti, kimlik boşluğu 0. Durumlar iç gözlemde 299/299 (gizlide 298/298) aralıkta, dış gözlemde 19/19 örnekte doğrulandı. Yol (a) üç durumda 0 kare üretti |
| G3 | G2'yi sağlayan yolda içerik: ortalama mutlak fark ≤ 2/255; konum hatası ≤ 1 px ve konum bütün karelerde bulunur; gölge var | **Geçti (yol b)** (güç koşulu sapmasıyla, mimarca kabul edildi) | Örtülü, gizli ve simge durumlarında kareler `b_gorunur_2` referansıyla bit düzeyinde aynı: MD5 9000/9000, karşılaştırılmayan 0. Ortalama mutlak fark 0 (her durumda 300 örnek). Küre bulunamayan kare 0. Konum hatası ort 0,141, maks 0,309 px. Gölge 9000/9000 karede (oran ≤ 0,49). Referans koşu pilde alındı; sapma ve mimarın kabulü G1 satırındaki gibidir. Karşılaştırılan durum koşuları prizdeki gece kampanyasındandır. Tam ekran koşusunun eşitliği yalnız gözlemdir (4.2) |
| G4 | 640 × 480 geri okuma p99 raporlanmış | **Geçti** | Görünür pencerede ilk okuma p99: `get_image()` 5,69 ms, `texture_get_data` 5,32 ms; `texture_get_data_async` istekten geri çağrıya 9,15 ms. Gizli pencerede asenkron 70,52 ms. Hepsi prizdeki gece kampanyasından (`20261114b`); pildeki yineleme G4'e girmedi. Yollar ve kopya sayısı 4.4'te |

## 6. Mimari için sonuç (ölçülen kapsamla sınırlı)

**Ölçülen sonuç:** macOS'ta (M3 Pro, macOS 26.5.2, Godot 4.7.2, Forward+, `thread_model=1`) kamera karesi şu yolla **örtülü, gizli (uygulama gizli) ve simge** durumlarının her birinde 5 dakika boyunca kesintisiz üretildi:

1. Ana iş parçacığından `RenderingServer.force_draw(false, dt)`.
2. Kamera `SubViewport`'u `UPDATE_ONCE`.
3. Eşzamanlı geri okuma.

Kareler kesintisiz görünür pencere referansıyla bit düzeyinde aynı. 07 §6'daki birincil kamera yolu Mac'te bu üç durumda çalışıyor. Başka uygulamanın tam ekranda olduğu koşuda da kareler üretildi ve aynı çıktı, ancak tam ekran durumunun sürdüğü doğrulanmadı (4.2). Bu durum Windows turuyla birlikte yeniden ölçülecek.

- Olağan çizim döngüsü örtülü, gizli ve simge durumlarında kare üretmiyor: yalnız `UPDATE_ONCE`/`ALWAYS` ile kamera akışı sürdürülemez (Ç14). Yol (a)'nın tam ekran sonucu doğrulanmadı.
- Asenkron geri okuma gizli pencerede ilerliyor, ama gecikmesi gerçek çizim hızına bağlı: 30 Hz'lik `force_draw` ile p99 70,5 ms. 07 §2.4'teki "döngü kamera hızından hızlı koşar" varsayımı gizli pencerede kendiliğinden sağlanmıyor.

**Ölçülmeyenler:**

- Windows; 30 dakika ve 2 saat; ekran koruyucu ve kapalı ekran.
- C++ uzantısı, `thread_model=2`, zamansal etkiler, su altı optiği.
- 07 §2.4'teki iki zamanlı çizim düzeni.

Bu nedenle sonuç A23 ve iskelet §8 kamera satırı için Mac'te ve 5 dakikalık ön tur kapsamında olumlu bir kanıttır, nihai karar değildir. Ö5-k, M28 ve M30 kapanmış sayılmaz.

## 7. Godot 4.7.2 API dayanakları

Kaynaklar `godotengine/godot` deposunun `4.7.2-stable` etiketinden:

| İddia | Kaynak |
|---|---|
| `force_draw` doğrudan `RenderingServer::draw`'a bağlı | `servers/rendering/rendering_server.cpp:3573` |
| `draw` ana iş parçacığı dışında hata verir ve `frame_pre_draw` yayar. Ardından `_draw` → `draw_viewports(p_swap_buffers)` çalışır; sonda `frame_post_draw` yayılır | `servers/rendering/rendering_server_default.cpp:443–451, 76–134, 215–229` |
| Olağan çizim yalnız `(can_any_window_draw() \|\| has_additional_outputs()) && is_render_loop_enabled()` ya da bekleyen RD kaynak işi varken yapılır. `frames_drawn` yalnız orada artar | `main/main.cpp:5080–5096` |
| macOS'ta çizilebilirlik: `occlusionState & NSWindowOcclusionStateVisible` ve `isVisible` | `platform/macos/display_server_macos.mm:2713`, `platform/macos/godot_window_delegate.mm:377, 392` |
| `DISABLED` atlanır, `ALWAYS`/`ONCE` görünür sayılır, `WHEN_VISIBLE` dokunun kullanılmasını ister | `servers/rendering/renderer_viewport.cpp:818–851` |
| `SubViewport` varsayılan kipi `UPDATE_WHEN_VISIBLE` (2) | `doc/classes/SubViewport.xml:26` |
| Ana pencerenin görünürlüğü değiştirilemez | `scene/main/window.cpp:1017` |
| Asenkron okuma gecikmesi `frame_queue_size` kare | `doc/classes/RenderingDevice.xml:1191–1199` |
| Eşzamanlı okuma bütün kareleri bekler; 1 CPU kopyası yapar | `servers/rendering/rendering_device.cpp`, `RenderingDevice::texture_get_data` |
| `get_image()` RGB8 dönüşümü | `servers/rendering/renderer_rd/storage_rd/texture_storage.cpp:1882–1927, 4289` |
| Monoton saat `mach_absolute_time` (dış gözlemdeki `Date.now()` ve `date` takvim saatidir; aynı saat alanı değildir) | `drivers/unix/os_unix.cpp:113–123, 391–393` |
| App Nap kapatma kodu bulunmadı | `platform/macos/` altında `os_macos.mm`, `display_server_macos.mm`, `godot_application_delegate.mm` ve `godot_window_delegate.mm` dosyalarında `beginActivity`, `NSActivity` ve `NSAppSleepDisabled` geçmiyor. GitHub kod aramasında (varsayılan dal) sonuç yok. Bütün ağaç taranmadı |

## 8. Doğrulanmayanlar

- Yol (a)'nın görünür ve tam ekran durumundaki sonucu. Koşular geçersiz ve mimar kararıyla yinelenmedi.
- Yol (b)'de "başka uygulama tam ekranda" durumunun sürdüğü. Gerçek ekran sınırı ölçülmedi; Windows turuyla yeniden ölçülecek.
- Görünür referansın prizde alınması. Referans pilde alındı; mimarın kabulü G1 ve G3 satırlarında.
- `20261114b/b_gorunur` (10,1 s) ve `20261115c/b_gorunur` (7,9 s) koşularındaki çizimsiz aralıkların nedeni. Ekran koruyucu yalnız bir çıkarımdır.
- Görünür pencerede, düzeltilmiş hazır zamanıyla 640 × 480 eşzamanlı okumanın istekten hazır olmaya gecikmesi. Yineleme koşusunda pencere görünür kalmadı.
- 07 §2.4'teki operatör zamanı ile kamera zamanı arasında gidip gelen çizim düzeni. Bu prototipte operatör görünümü ve kamera aynı durumu çizer.
- 5 dakikadan uzun sürelerde App Nap ve kısma davranışı; 30 dakika ve 2 saat (MP6'nın kalan işi).
- Windows yarısı, C++ uzantısı (P1, M33), `thread_model=2`, ekran koruyucu ve kapalı ekran durumları.
- `SHA512-SUMS.txt` eşleşmesi yalnız bu işte denetlendi; sol bağımsız doğrulayamadı.

## 9. Düzeneğe ilişkin düzeltmeler

- İlk tur:
  - `kampanya.sh`, ek argümanlı koşulara çıktı dizinini yanlış sırada veriyordu. Düzeltildi; `kos.sh` artık göreli çıktı dizinini reddediyor.
  - `get_image()` RGB8 döndürüyor. Çözümleme her iki biçimi de kabul edecek biçimde düzeltildi.
  - Kimlik şeridinin çizim sırası `z_index` ile düzeltildi.
- Düzeltme turu:
  - Hazır olma zamanı artık ilk okumadan hemen sonra kaydediliyor.
  - `analiz.py`'ye `--ref`, karşılaştırılan ve karşılaştırılmayan kare sayısı, "küre bulunamadı" sütunu ve kayıt aralığı ile süre ayrımı eklendi. Ayrıca "ekrandaki tek pencere örtü" sayacı eklendi (ikinci turda bu adı aldı; boyutu doğrulamaz).
  - `pencere.js` pencere sahibini ve üst katman pencerelerini kaydediyor.
  - Örtü rolü tam ekran kipini 15 s'de bir yazıyor.
