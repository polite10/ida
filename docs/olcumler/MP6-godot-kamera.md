# MP6a — Godot 4.7.2'de görünmeyen pencerede kamera karesi (Mac ön turu)

| | |
|---|---|
| Görev | `docs/gorevler/W0-MP6a.md` |
| Dal | `olcum/mp6-godot-kamera` |
| Tarih | 2026-11-14 |
| Uygulayan | `ida-uygulayici` (Opus) |
| Durum | **Eksik.** a yolunun beş durumu tam süreyle koştu; b yolunun (`force_draw`) 5 dakikalık koşuları Mac'in kapağı kapanıp uykuya geçmesi nedeniyle yapılamadı. b yolu için yalnız kısa koşu (10–25 s) kanıtı var |

## 1. Ortam

| Kalem | Değer |
|---|---|
| Makine | Apple M3 Pro, 18 GB (`hw.memsize` 19327352832), dahili Liquid Retina XDR ekran, tek ekran |
| OS | macOS 26.5.2 (25F84) |
| Güç | Pil (koşu başında %100, sonunda %94); düşük güç kipi kapalı (`pmset -g`: `lowpowermode 0`) |
| Godot | 4.7.2-stable official, commit `ed1daf0bf001b61586d9930840f2f1394092c079`, standart (.NET değil), evrensel macOS ikilisi |
| Godot ikilisi | `Godot_v4.7.2-stable_macos.universal.zip`, resmî `godotengine/godot-builds` sürüm sayfası. SHA-256 `c58a24e31d720be9d62f60cb5627c4e695fb72f21b0cfe1bc9ccaa9a3b3ba63e` (GitHub sürüm varlığının yayımlanan `digest` alanıyla aynı). SHA-512 `38aa16e5…7bac69ca` (yayımlanan `SHA512-SUMS.txt` ile aynı). İmza: `TeamIdentifier=6K46PWY5DM`, hardened runtime |
| Kurulum yeri | `${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/`; sistem geneline kurulum yok |
| Görüntüleyici | Metal 4.0, Forward+, "Apple M3 Pro (Apple9)" (`godot.log`) |
| Proje ayarları | `vsync_mode=0`, `max_fps=120`, `thread_model=1` (varsayılan, ayrı render iş parçacığı yok), `frame_queue_size=2` (varsayılan), MSAA/TAA/FXAA/debanding kapalı, doğrusal ton eşleme |
| Derleyici, kart | Yok (GDScript; C++ uzantı bu işte yok; kart kullanılmadı) |

## 2. Düzenek

Kod: `tools/olcum/mp6/` (atılabilir prototip).

| Dosya | İş |
|---|---|
| `proje/olcum.gd` | Sahne, kamera `SubViewport`'u, istek zamanlaması, geri okuma, kare başına kayıt |
| `kos.sh` | Tek koşu: ölçüm sürecini başlatır, durumu oluşturur, 15 s'de bir dış gözlem alır, çıkışta bütün süreçleri kapatır |
| `kampanya.sh` | Tam kampanya (durum × yol, geri okuma, başsız) |
| `pencere.js` | JXA: `NSRunningApplication.hide` ve `CGWindowListCopyWindowInfo` sorgusu; başka uygulamaya Apple Event göndermez, izin istemez |
| `analiz.py`, `png.py` | Çözümleme; ham kareyi PNG'ye çevirme |

**Sahne.** Operatör penceresi 480 × 360 px (Retina'da 240 × 180 pt), kendi `Camera3D`'si. Kamera `SubViewport`'u 640 × 480, `own_world_3d = false` (aynı `World3D`), `UPDATE_DISABLED` başlar. İçerik: gölge düşüren yönlü ışık, gri zemin, sabit direk, kare kimliği `k`'den hesaplanan konumda gölgelemesiz kırmızı küre (kare başına yaklaşık 3–4 px yer değiştirir), sol üstte 24 bitlik kare kimliği şeridi (üst satır `k`, alt satır tümleyeni; çözülemeyen şerit "içerik geçersiz" sayılır). Küre konumu ve şerit, aynı çağrıda `force_draw` çizebilsin diye `RenderingServer.instance_set_transform` ve `canvas_item_set_visible` ile doğrudan yazılır (düğüm dönüşüm bildirimi `_process` içinde henüz yayılmaz: `scene_tree.cpp`, `SceneTree::process` sırası).

**İstek.** Monoton saat (`Time.get_ticks_usec`, macOS'ta `mach_absolute_time`: `drivers/unix/os_unix.cpp`) ile 30 Hz yuvalar: `k = ⌊(t − t0) / 33,333 ms⌋`. Atlanan yuva "döngü gecikti" sayılır. Her istekte `viewport_set_update_mode(ONCE)`.

- **a yolu:** olağan çizim döngüsü; kare `RenderingServer.frame_post_draw` içinde okunur. Sonraki yuvaya kadar çizim olmazsa "çizim yok".
- **b yolu:** `_process` içinde `RenderingServer.force_draw(false, 1/30)`, hemen ardından okuma.

**Geri okuma.** Durum koşularında `SubViewport.get_texture().get_image()` (eşzamanlı, 07 §2.5'teki yedek yol). Okuma koşularında ayrıca `RenderingDevice.texture_get_data` (eşzamanlı, dönüşümsüz) ve `texture_get_data_async`.

**Kare başına kayıt** (`kare.csv`): planlanan zaman, istek zamanı, hazır zamanı, gecikme, geri okuma süresi, çözülen kimlik, MD5, kürenin kırmızı maske ağırlık merkezi, yer gerçeği izdüşümü (`Camera3D.unproject_position`), gölge oranı, sonuç. Her 30. kare ham olarak dökülür (ortalama mutlak fark için).

**Gölge ölçütü (bu işte tanımlandı, ölçümden önce donduruldu):** direğin orta yüksekliğinin zemindeki gölge noktası ile 1,2 m ötedeki aydınlık zemin noktasında 5 × 5 px parlaklık oranı ≤ 0,75 ise "gölge var". Görev belgesi bu işlemsel tanımı vermiyor.

**Durumun oluştuğunun doğrulanması.**

1. İç gözlem: `Engine.get_frames_drawn()` yalnız `Main::iteration` çizdiğinde artar (`main/main.cpp` 5080–5096). Görünürde ilerlemesi, diğer durumlarda 1,5 s durması beklenir; ölçüm penceresi bundan sonra başlar. Saniyelik `durum.csv` koşu boyunca izler.
2. Dış gözlem (15 s'de bir): `CGWindowList` (ölçüm penceresi ekranda mı, örtü penceresi önde ve onu kapsıyor mu), `NSRunningApplication.hidden`, `ps` önceliği.
3. Simge durumu için ayrıca `DisplayServer.window_get_mode() == MINIMIZED`.

| Durum | Nasıl oluşturuldu |
|---|---|
| görünür | Ölçüm penceresi `--always-on-top` |
| örtülü | İkinci bir Godot süreci, her zaman üstte, 550 × 482 pt; ölçüm penceresini tamamen kapsadığı `CGWindowList` ile doğrulandı |
| gizli | Uygulama gizleme (`NSRunningApplication.hide`, Cmd+H eşdeğeri). Ana pencerenin kendisi gizlenemiyor (bkz. 6. başlık) |
| simge | `DisplayServer.window_set_mode(WINDOW_MODE_MINIMIZED)` |
| tam ekran | İkinci Godot süreci `--fullscreen` (macOS yerel tam ekran, ayrı Space) |

## 3. Komutlar

```
tools/olcum/mp6/kos.sh <a|b> <durum> <sure_s> <mutlak_cikti_dizini> [--cozunurluk=GxY --geri=esz|rdesz|async --cift=1 --ornek=N]
tools/olcum/mp6/kampanya.sh ${IDA_CACHE:-$HOME/.cache/ida}/mp6/ham/20261114
python3 -I tools/olcum/mp6/analiz.py <kok_dizin> [koşu ...]
```

Ham veri (depoya girmez): `${IDA_CACHE:-$HOME/.cache/ida}/mp6/` altında `ham/20261114/` (tam koşular), `deneme/` ve `kuru/` (kısa koşular). Toplam 533 MB. Godot ayrıca `~/Library/Application Support/ida_mp6/` altına shader önbelleği yazdı (silinmedi).

## 4. Sonuçlar

### 4.1 Tam süreli koşular (300 s, 9000 istek)

| Koşu | Durum tutuldu (s / 298–299) | İstek | Üretilen | Düşen % | Neden | Gecikme p50/p99/maks ms | Geri okuma p50/p99/maks ms | İçerik |
|---|---|---|---|---|---|---|---|---|
| a, görünür | 196 / 298 | 9000 | 5823 | 35,3 | çizim yok 3151, döngü gecikti 26 | 6,58 / 8,55 / 903,98 | 4,87 / 6,40 / 18,05 | Referans; b ile aynı kimlikli 4336 karenin MD5'i birebir aynı |
| a, örtülü | 297 / 299 | 9000 | 11 | 99,88 | çizim yok 8989 | 8,75 / 25,88 / 27,70 | 8,32 / 9,89 / 10,03 | Üretilen 11 kare örtü kısa süre kalktığında |
| a, gizli | 299 / 299 | 9000 | 0 | 100 | çizim yok 9000 | – | – | – |
| a, simge | 299 / 299 | 9000 | 0 | 100 | çizim yok 9000 | – | – | – |
| a, tam ekran | yalnız ilk ~100 s geçerli | 9000 | 0 | 100 | çizim yok 8967, döngü gecikti 33 | – | – | – |
| b, beş durum | **yapılamadı** | | | | | | | |

Notlar:

- **a, görünür koşusu bozuldu:** yaklaşık 102 s boyunca ölçüm penceresi ekrandan çıktı (`CGWindowList`'te yok, `frames_drawn` durdu). En olası neden kullanıcının başka bir Space'e ya da tam ekran uygulamaya geçmesi; her zaman üstte pencere başka Space'e taşınmaz. Durumun tutulduğu saniyelerde düşen kare %1,67'dir; bunlar geçiş saniyelerindedir. Bu koşu G1 için geçersizdir. Maks gecikme 904 ms ve 26 "döngü gecikti" bu geçişlere denk gelir.
- **a, tam ekran koşusu:** ölçüm 16:50 civarında başladı; kapak 16:51:52'de kapandı (`pmset -g log`: "Clamshell Sleep"). Koşunun geri kalanı karanlık uyanmalarda (DarkWake) ilerledi; monoton saat uykuda durduğu için yuvalar kesintisiz görünür. Bu koşunun yalnız ilk ~100 s'si geçerlidir. a yolu bu sürede de kare üretmedi; sonuç değişmez. Yinelenemedi (kapak kapalı).
- **b, görünür:** 19:53'te başladı, kapak kapalıyken karanlık uyanmalarda yalnız ~82 s monoton süre ilerledi. Geçersiz; süreç durduruldu.
- **App Nap / kısma:** a yolunun örtülü, gizli ve simge koşularında 5 dakika boyunca ana döngü saniyede medyan 121 kez döndü. 100'ün altındaki tek saniye koşunun son, kısmi saniyesidir. Kapak açıkken `ps` önceliği 97'de kaldı. Kapak kapalıyken uykudaki süreçte 28 görüldü. Bu sürelerde App Nap kaynaklı döngü kısması **gözlenmedi**. Yine de App Nap'ın devrede olmadığı kanıtlanmış değildir; daha uzun sürede davranış doğrulanmadı. Kapatmak için bir ayar gerekmedi, denenmedi de. Godot 4.7.2'nin macOS kaynağında App Nap'ı kapatan bir kod bulunmadı (6. başlık).
- **Uyku:** Mac pilde ve kapak kapalıyken `pmset` uyku engeli varken bile "Clamshell Sleep"e girdi. Bu iş `caffeinate` kullanmadı; sistem ayarlarına dokunulmadı.

### 4.2 b yolu: kısa koşular (durum doğrulamalı, 10–25 s)

İki ayrı tur: `deneme/` (20–25 s) ve `kuru/` (10 s). Her durum iç ve dış gözlemle doğrulandı: `frames_drawn` ölçüm boyunca 0; gizlide `hidden=true`; simgede `window_get_mode=1` ve pencere ekranda değil; örtülüde örtü önde ve kapsıyor; tam ekranda pencere ekranda değil.

| Durum | İstek / üretilen (tur 1; tur 2) | Düşen | Kimlik boşluğu | Gecikme p50/p99 ms (tur 1; tur 2) | MD5 referansla aynı | Ortalama mutlak fark (/255) | Konum hatası maks px | Gölge var |
|---|---|---|---|---|---|---|---|---|
| görünür | 601/601; 300/300 | 0 | 0 | 5,31/6,87; 6,50/7,15 | referans | – | 0,309 | %100 |
| örtülü | 600/600; 300/300 | 0 | 0 | 6,99/8,31; 7,27/8,44 | 600/600; 300/300 | 0 (n=20; n=10) | 0,309 | %100 |
| gizli | 750/750; 300/300 | 0 | 0 | 5,94/9,80; 6,92/9,01 | 601/601*; 300/300 | 0 (n=21; n=10) | 0,309 | %100 |
| simge | 600/600; 300/300 | 0 | 0 | 6,42/8,54; 6,39/8,49 | 600/600; 300/300 | 0 (n=20; n=10) | 0,309 | %100 |
| tam ekran | 600/600; 300/300 | 0 | 0 | 7,91/8,98; 6,37/7,43 | 600/600; 300/300 | 0 (n=20; n=10) | 0,309 | %100 |

\* Referansta yalnız 601 kimlik vardı.

Bütün durumlarda b yolu kareleri görünür pencerede üretilen referansla **bit düzeyinde aynıdır** (MD5 eşit, ortalama mutlak fark 0). Aynı kimlikte a yolu ile b yolu da birebir aynıdır. Konum hatası (ağırlık merkezi ile izdüşüm arası) en çok 0,31 px'tir; referansla fark 0'dır. Gölge oranı 0,28–0,49 aralığındadır, eşik 0,75. Bu sonuçlar **5 dakikalık koşu değildir**; G2/G3'ün istediği süre sağlanmadı.

### 4.3 Başsız kip (`--headless`, 20 s)

`display_server = headless`, `RenderingServer.get_rendering_device() == null`, görüntü bağdaştırıcısı adı boş, `frames_drawn = 0`. a yolu: 600 istekte 0 kare ("çizim yok"). b yolu: `force_draw` hata vermeden döner, ama `get_image()` her istekte boş döner (`ERROR: Parameter "t" is null` at `servers/rendering/dummy/storage/texture_storage.h:110`), 600 istekte 0 kare. Başsız kipte GPU kamera karesi üretilmez; 00 belgesindeki "başsız kip sensör yolu değildir" hükmüyle uyumludur.

### 4.4 Geri okuma maliyeti (görünür pencere)

| Çözünürlük | Yol (API) | Kaynak | n | İlk okuma p50/p99/maks ms | GPU boştayken ikinci okuma p50/p99/maks ms |
|---|---|---|---|---|---|
| 640 × 480 | `ViewportTexture.get_image()` + `Image.get_data()` | a, görünür, 300 s | 5822 | 4,87 / **6,40** / 18,05 | – |
| 640 × 480 | aynı, b yolu | kısa koşular | 601; 300 | 3,52/4,77; 5,30/5,65 (p50/p99) | – |
| 640 × 480 | `RenderingDevice.texture_get_data_async` (istekten geri çağrıya) | kısa koşu, 10 s | 300 | 14,77 / 16,33 / 16,67 | – |
| 1280 × 720 | `get_image()` | kısa koşu, 5 s | 150 | 5,44 / 6,84 / 6,91 | 1,85 / 2,46 / 4,18 |
| 1280 × 720 | `RenderingDevice.texture_get_data` (eşzamanlı) | kısa koşu, 5 s | 150 | 4,99 / 6,07 / 6,12 | 1,17 / 1,68 / 2,15 |
| 1280 × 720 | `texture_get_data_async` | kısa koşu, 5 s | 150 | 7,98 / 8,56 / 10,32 (gecikme) | – |

- İlk eşzamanlı okuma, GPU'nun o kareyi bitirmesini beklemeyi de içerir: `texture_get_data`, `_flush_and_stall_for_all_frames()` çağırır (`servers/rendering/rendering_device.cpp`). "İkinci okuma" aynı dokunun hemen yeniden okunmasıdır ve aktarımın kendi maliyetine yaklaşır.
- **Kopya sayısı (kaynaktan):** `texture_get_data` her çağrıda yeni bir CPU görünür ara tampon açar ve GPU kopyasını kuyruğa alır, sonra bütün kareleri bekler; ara tampondan sıkı bir `Vector<uint8_t>`'ye 1 CPU kopyası yapar (satır adımı farklıysa satır satır). `get_image()` bu yolun üstüne bir dönüşüm daha ekler: saydam olmayan render hedefinin `image_format`'ı RGB8'dir (`texture_storage.cpp:4289`), GPU dokusu RGBA8'dir, ve `texture_2d_get` `image->convert(RGB8)` çağırır (`texture_storage.cpp:1926`). Yani 2 CPU kopyası ve 640 × 480'de 921.600 baytlık RGB8 çıktı. `RenderingDevice.texture_get_data` 1 CPU kopyasıyla 1.228.800 baytlık RGBA8 verir.
- 1280 × 720 ve asenkron değerleri 5–10 s'lik kısa koşulardır; p99 kuyruğu için yeterli değildir. Kampanyadaki 120 s'lik okuma koşuları yapılamadı.

## 5. Geçme ölçütleri

| # | Ölçüt | Sonuç | Gerekçe |
|---|---|---|---|
| G1 | Görünür pencerede en az bir yolla 5 dk, düşen < %1 | **Geçmedi (tamamlanamadı)** | a, görünür koşusunda pencere ~102 s ekrandan çıktı; düşen %35,3, durum tutulurken %1,67. b, görünür koşusu uykuda kaldı. Kısa koşularda b yolu görünürde %0 düşme |
| G2 | Örtülü, gizli, simge durumlarının her birinde en az bir yolla 5 dk, düşen < %1, kesintisiz kimlik | **Geçmedi** | a yolu: üç durumda 5 dk boyunca kare yok (%99,88–100 düşme), doğrulanmış durumlarda. b yolunun 5 dk koşusu yapılamadı. Kısa koşularda (10–25 s, iki tur) b yolu üç durumda ve tam ekranda %0 düşme, kimlik boşluğu 0 |
| G3 | G2 yolunda içerik: ortalama mutlak fark ≤ 2/255, konum ≤ 1 px, gölge var | **Geçmedi (tamamlanamadı)** | G2'yi sağlayan 5 dk'lık yol yok. Kısa koşularda b yolu: fark 0 (bit düzeyinde aynı), konum maks 0,31 px, gölge %100 |
| G4 | 640 × 480 geri okuma p99 raporlanmış | **Geçti** | `get_image()` p99 6,40 ms (n=5822, 300 s, görünür, a yolu). Yol ve kopya sayısı 4.4'te |

**Yorum (ölçülen, sınırlı):** Olağan çizim döngüsü (a yolu) macOS'ta örtülü, gizli, simge ve tam ekran durumlarında kamera karesi üretmiyor. Kaynakta görülen kapı (`can_any_window_draw`) ölçümle doğrulandı. `force_draw` (b yolu) aynı durumlarda kısa sürelerde kesintisiz ve bit düzeyinde doğru kare üretti. 5 dakikalık süreklilik henüz ölçülmedi; A23 için karar verdiren veri b yolunun tam süreli koşularıdır.

## 6. Godot 4.7.2 API dayanakları

Kaynaklar `godotengine/godot` deposunun `4.7.2-stable` etiketindeki dosyalardır:

| İddia | Kaynak |
|---|---|
| `force_draw` doğrudan `RenderingServer::draw`'a bağlı | `servers/rendering/rendering_server.cpp:3573` |
| `draw` ana iş parçacığı dışında hata verir; `frame_pre_draw` yayar; `_draw` → `draw_viewports(p_swap_buffers)`; sonunda `frame_post_draw` | `servers/rendering/rendering_server_default.cpp:443–451, 76–134, 215–229` |
| Olağan çizim yalnız `(can_any_window_draw() \|\| has_additional_outputs()) && is_render_loop_enabled()` ya da bekleyen RD kaynak işi varsa; `frames_drawn` yalnız orada artar | `main/main.cpp:5080–5096` |
| macOS'ta çizilebilirlik: `occlusionState & NSWindowOcclusionStateVisible` ve `isVisible` | `platform/macos/display_server_macos.mm:2713`, `platform/macos/godot_window_delegate.mm:377, 392` |
| `DISABLED` atlanır; `ALWAYS`/`ONCE` görünür sayılır; `WHEN_VISIBLE` kullanım ister | `servers/rendering/renderer_viewport.cpp:818–851` |
| `SubViewport` varsayılan kipi `UPDATE_WHEN_VISIBLE` (2) | `doc/classes/SubViewport.xml:26` |
| Ana pencerenin görünürlüğü değiştirilemez | `scene/main/window.cpp:1017` ("Can't change visibility of main window.") |
| Asenkron okuma gecikmesi `frame_queue_size` kare; render iş parçacığı koruması | `doc/classes/RenderingDevice.xml:1191–1199`; `rendering_device.cpp` `texture_get_data_async` |
| Monoton saat `mach_absolute_time` | `drivers/unix/os_unix.cpp:113–123, 391–393` |
| App Nap kapatma kodu yok | `platform/macos/` altındaki `os_macos.mm`, `display_server_macos.mm`, `godot_application_delegate.mm`, `godot_window_delegate.mm` dosyalarında `beginActivity`/`NSActivity`/`NSAppSleepDisabled` geçmiyor. GitHub kod aramasında (varsayılan dal) sonuç yok. Bütün kaynak ağacı taranmadı |

## 7. Doğrulanmayanlar ve kalan iş

- b yolunun beş durumda 300 s'lik koşuları ve a yolunun görünür ve tam ekran koşularının yinelenmesi. Kalan komut: `kampanya.sh` (b döngüsü, okuma koşuları, başsız). Kapak açık, ekran uyanık ve kullanıcının Space değiştirmediği bir sürede koşmalıdır. Tam ekran koşusu ekranı 5 dk kaplar.
- 1280 × 720 ve asenkron geri okumanın tam süreli p99'u. Gizli pencerede asenkron kuyruğun `force_draw` ile ilerleyip ilerlemediği ölçülmedi (kampanyada vardı, koşmadı).
- App Nap'ın 5 dakikadan uzun sürelerde davranışı; ekran koruyucu ve kapalı ekran durumları (görev kapsamı dışında).
- `thread_model = 2` (ayrı render iş parçacığı) altında `force_draw` davranışı denenmedi.

## 8. Düzeneğe ilişkin hatalar ve düzeltmeler

- İlk kuru çalıştırmada `kampanya.sh`, ek argümanlı okuma koşularına çıktı dizinini yanlış sırada verdi; koşular hiç başlamadı. Düzeltildi. `kos.sh` artık göreli çıktı dizinini reddediyor.
- `get_image()` saydam olmayan `SubViewport`'ta RGB8 döndürüyor; ilk sürüm RGBA8 beklediği için bütün kareleri geçersiz saydı. Çözümleme iki biçimi de kabul edecek biçimde düzeltildi.
- `RenderingServer` ile doğrudan açılan kardeş `CanvasItem`'ların çizim sırası belirlenimci değildi; kimlik şeridinin bir kısmı siyah zeminin altında kaldı. `z_index` ile düzeltildi.
