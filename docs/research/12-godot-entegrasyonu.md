# 12 - Godot entegrasyonu, sahne/varlık hattı ve çarpışma kütüphanesi

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Doğrulaması: [dogrulama/12-codex.md](dogrulama/12-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Tarih: 2026-11-11. Durum: ham araştırma raporu; `docs/research/00-duzeltmeler-ve-kararlar.md` ile çelişirse 00 geçerlidir. Bu rapor 09/10 ve doğrulamalarının (Ö5, Ö6, M13, A5) açık bıraktığı entegrasyon ayrıntılarını kapatmayı amaçlar. Hiçbir performans rakamı ölçülmemiştir; "hedef" diye yazılanlar kabul ölçütü önerisidir, ölçüm değildir. Bu makinede hiçbir şey derlenmedi veya çalıştırılmadı; bulgular belge ve kaynak kod okumasına dayanır.

## 1. Özet

1. **Sürümler (doğrulandı):** Güncel kararlı Godot 4.7.2 (18 Ağustos 2026); 4.7 18 Haziran 2026. godot-cpp 10.0.0-stable (15 Eylül 2026) Godot'dan bağımsız sürümlenir ve 4.3+ hedefler. Önerilen: Godot'yu 4.7.x'te sabitle, `api_version=4.7` ile derle.
2. **En önemli bulgu (kaynak koddan doğrulandı):** Godot 4.7.2'de hiçbir pencere çizilemiyorsa (macOS'ta pencere başka pencerelerce örtülü/gizli, Windows'ta minimize) `Main::iteration()` `RenderingServer::draw`'ı hiç çağırmıyor. Yani **SubViewport'lar da yenilenmiyor**; `UPDATE_ALWAYS` bunu çözmez. Kamera sensörü hattı bu davranışa karşı tasarlanmalı (M13'ün somut hali).
3. Çözüm adayları: (a) pencere her zaman görünür ve örtülmemiş tutulur (operatör 3B görünümü zaten bu), (b) `RenderingServer.force_draw()` ile elle çizim (üçüncü taraf bir PR bunu macOS'ta çalışan çözüm olarak raporluyor; **4.7.2'de doğrulanmadı**), (c) `--disable-render-loop` ile sim saatine bağlı kare üretimi. `--headless` seçenek değil: RenderingDevice yok.
4. **Bağlantı:** Poz akışı için paylaşımlı bellek (tek yazar, çok okuyucu, sürüm sayaçlı) önerilir; kontrol/olay/metadata için yerel TCP. Godot tarafı yalnızca "oynatıcı"dır: fizik yok, sim zamanı etiketli poz anlık görüntüleri arasında enterpolasyon yapar. Kamera karelerini sim'e geri vermek de paylaşımlı bellekle yapılır. Gecikme rakamları ölçülmedi.
5. **GDExtension derlemesi:** Resmi yol SCons; CMake desteği godot-cpp içinde var ama "ikincil sistem". MSVC ve Apple Silicon destekleniyor. Uzantı yeni Godot'da çalışır, eskide çalışmaz. Uzantıyı **ince** tut (IPC + görsel güncelleme); Embree/Jolt/Eigen gibi ağır bağımlılıkları sim sunucusunda bırak.
6. **Okyanus:** Hazır, MIT lisanslı Godot FFT okyanus projeleri var (GodotOceanWaves, godot4-oceanfft) ama ikisi de Godot 4.3 döneminden, son değişiklikleri 2025'te; su altı görüntüsü "temel" düzeyde. Fizik-görüntü tutarlılığı için dalga durumunu fizik çekirdeğinin sahibi olduğu bir **bileşen listesi/h0 ızgarası** olarak Godot'ya aktarmak gerekir; bu hazır projelerde yok (kendi uyarlama işi).
7. **Arayüz kararı (Ö6) için öneri:** Harita ve zaman serisi grafikleri web (MapLibre GL JS + uPlot) ile, **ayrı pencerede, Godot'ya gömülmeden**. Gerekçe: Godot'ya web gömme çözümlerinin macOS desteği yok (gdcef) ya da Rust tabanlı (godot_wry); Godot'nun yerel harita/grafik eklentileri çok küçük projeler. Godot yalnızca 3B + HUD + kamera/sonar görüntüsü gösterir. Bu bir önerilen başlangıçtır; prototiple sınanır.
8. **Çarpışma kütüphanesi:** Jolt Physics (MIT) önerilir; **yalnızca çarpışma algılama** için kullanılır (`NarrowPhaseQuery::CollideShape` / `CastShape`), temas tepkisi Fossen dinamiğine dış kuvvet olarak özel yazılır (penaltı + sönüm + sürtünme). Bullet (zlib) ve FCL (BSD) yetenekli alternatifler; Jolt'un belgelenmiş çapraz platform determinizmi ve macOS ARM CI doğrulaması belirleyici.
9. **Varlık hattı:** Blender → glTF 2.0 tek kaynak; C++ tarafında `cgltf` (MIT, C) önerilir; akustik/optik malzeme verisi `extras` alanında (Blender "Custom Properties" dışa aktarımı) ya da adlı bir vendor `extensions` bloğunda; dışbükey ayrıştırma CoACD (MIT, macOS ARM + Windows destekli) ya da V-HACD (BSD-3), çevrimdışı derleme adımında.
10. **Paketleme:** macOS'ta GDExtension için "Disable Library Validation" yetkisi gerekir; notarization Developer ID ister. Bu bir laboratuvar aracı olduğundan ilk aşamada ad-hoc imza + yerel çalıştırma yeterli olabilir (karar sizde).

## 2. Bulgular

### 2.1 Godot sürümü, godot-cpp, CMake, uyumluluk

| Konu | Bulgu | Kaynak |
|---|---|---|
| Kararlı sürümler | 4.7.2 (2026-08-18), 4.7.1 (2026-07-14), 4.7 (2026-06-18), 4.6.3 (2026-05-20) | [Godot arşiv](https://godotengine.org/download/archive/), [GitHub releases](https://api.github.com/repos/godotengine/godot/releases/latest) |
| godot-cpp sürümü | 10.0.0-stable, 2026-09-15; 10.x Godot'dan bağımsız sürümlenir, `api_version` ile 4.3+ hedefler | [godot-cpp README](https://github.com/godotengine/godot-cpp), [releases](https://api.github.com/repos/godotengine/godot-cpp/releases) |
| İkili uyumluluk | Önceki minör için derlenen uzantı sonraki minörlerde çalışır, tersi çalışmaz; 4.0 hedefli uzantı 4.1+'da çalışmaz; `compatibility_minimum` eski Godot'da yüklemeyi engeller | [GDExtension C++ örneği (4.7)](https://docs.godotengine.org/en/4.7/tutorials/scripting/cpp/gdextension_cpp_example.html), godot-cpp README |
| Yapı sistemi | Resmi şablon ve rehber SCons. godot-cpp deposunda `CMakeLists.txt` ve `cmake/{macos,windows,...}.cmake` var; belge "CMake ikincil sistemdir, bazı SCons özellikleri eksik olabilir" diyor | [CMake belgesi](https://docs.godotengine.org/en/latest/tutorials/scripting/cpp/build_system/cmake.html), [CMakeLists.txt](https://github.com/godotengine/godot-cpp/blob/master/CMakeLists.txt), [godot-cpp-template](https://github.com/godotengine/godot-cpp-template) |
| CMake ayrıntıları | `GODOTCPP_TARGET`, `GODOTCPP_PRECISION` (single/double), `GODOTCPP_USE_HOT_RELOAD`; macOS universal `CMAKE_OSX_ARCHITECTURES` ile; MSVC çoklu-yapılandırma üretici (`--config Release`); `CMAKE_MSVC_RUNTIME_LIBRARIES` godot-cpp tarafından ayarlanır, bu yüzden diğer bağımlılıklardan önce eklenmeli | CMake belgesi |
| Sıcak yeniden yükleme | `reloadable = true`, yalnızca debug derlemelerde | GDExtension C++ örneği |
| API değişikliği | 4.6'dan itibaren GDExtension arayüzü JSON ile tanımlanıyor, C başlığı üretiliyor; API'de "required" parametre işaretleri | [4.6 sürüm notları](https://godotengine.org/releases/4.6/) |
| Bilinen sorunlar (açık) | macOS Metal'de aralıklı "timeout waiting for fence" donması (4.6.1, M1 Max); macOS 26 + M4/M5'te 4.7.1 ile compute/canvas shader derlerken kilitlenme (Windows'ta sorun yok); 4.7'de compute push-constant dolgu davranışı değişti (4.6'da yok); D3D12'de `image2DMS` yazan compute pipeline oluşturulamıyor | [#119436](https://github.com/godotengine/godot/issues/119436), [#123481](https://github.com/godotengine/godot/issues/123481), [#120097](https://github.com/godotengine/godot/issues/120097), [#120333](https://github.com/godotengine/godot/issues/120333) (hepsi 2026-11-11 itibarıyla açık) |

Yorum: Godot 4.7 yeni ve macOS 26 üzerinde açık donma raporları var; bu, Ö5'in kabul prototipinin Apple Silicon üzerinde **uzun süreli (saatlik) çalışma testi** içermesini gerektirir.

### 2.2 Sim sunucusu ile Godot arasındaki bağlantı

Godot uzantısı normal bir C++ paylaşımlı kütüphane olduğu için OS API'lerini (POSIX `shm_open`/`mmap`, Windows `CreateFileMapping`, soketler) doğrudan kullanabilir; Godot tarafında bu konuda kısıt aramadım. Karşılaştırma (aşağıdaki gecikme notları literatürdeki niteliksel sıralamadır, bu proje için ölçülmemiştir):

| Seçenek | Artı | Eksi / risk |
|---|---|---|
| Paylaşımlı bellek + sürüm sayacı (seqlock / çift tampon) | En düşük gecikme sınıfı; kamera kareleri (ör. 640x480 RGBA8 ≈ 1.2 MB, 30 Hz ≈ 37 MB/s, aritmetik hesap) için kopyasız; yazar-okuyucu ayrık | İki OS'ta farklı API; macOS POSIX paylaşımlı bellek adı en çok 31 karakter ([kaynak](https://pikers.dev/pikers/piker/src/branch/macos_dnks_vibed_fixes/docs/macos/compatibility-fixes.md), [Apple liste](https://lists.apple.com/archives/darwin-dev/2008/Aug/msg00035.html)); uyandırma mekanizması gerekir (yoklama ya da soket sinyali); süreç çökünce kalıntı nesne temizliği |
| Yerel TCP / UDP (loopback) | İki OS'ta aynı; hata ayıklaması kolay; Windows'ta loopback hızlı yolu var ([MS](https://learn.microsoft.com/en-us/archive/blogs/wincat/fast-tcp-loopback-performance-and-low-latency-with-windows-server-2012-tcp-loopback-fast-path)) | Kopya ve sistem çağrısı maliyeti; kamera karelerinde bant genişliği/tampon yönetimi |
| Adlandırılmış boru / Unix domain soket | Windows named pipe ve macOS UDS ayrı API'ler | Platforma özgü kod; TCP'ye göre belirgin kazanç ölçülmeden iddia edilemez (anekdotal: Windows'ta ~15 µs vs ~25 µs tek bayt gidiş-dönüş, [SO](https://kaweah.freedombox.rocks/kiwix/stackoverflow.com_en_all_2023-11/questions/10872557/how-slow-are-tcp-sockets-compared-to-named-pipes-on-windows-for-localhost-ipc)) |
| WebSocket | Web panelleriyle aynı protokol | Çerçeveleme/maskeleme yükü; Godot'da `WebSocketPeer` var ama GDExtension'dan doğrudan soket daha basit |

Dış-fizikli Godot örnekleri (dersler): **Godot RL Agents** Python ile Godot arasında eşzamanlı (lockstep) TCP senkronu kullanır ([HF](https://huggingface.co/learn/deep-rl-course/unitbonus3/godotrl)); **rclgd** ROS 2 köprüsünü GDExtension olarak yapar ([belge](https://rclgd.readthedocs.io/en/stable/getting_started/introduction.html)). İkisi de "Godot'yu dışarıdan sürülen istemci" kalıbını gösterir; yüksek frekanslı poz akışı için bir referans ölçüm bulunamadı.

Zaman eşleme (tasarım önerisi, kanıt değil): sim, sabit adımla `sim_time` etiketli poz anlık görüntüsü yayımlar. Operatör görünümü `render_time = son_sim_time - 2 adım` gibi kasıtlı bir gecikmeyle iki anlık görüntü arasında enterpolasyon yapar (Godot'nun yerleşik fizik enterpolasyonu sizin akışınıza uygulanmaz; kendi enterpolasyonunuzu yazarsınız). Kamera sensörü için ise sim `camera_trigger(t_sim)` yollar, Godot o ana enterpole/ekstrapole edilmiş pozla çizer ve kareyi `t_sim` etiketiyle geri verir. Karenin HIL'e teslimi modellenmiş sensör gecikmesine göre sim tarafında geciktirilir; 00'daki 10.8 düzeltmesi (asenkron okuma `frame_queue_size` kare gecikebilir) bu bütçeye girmelidir.

Lockstep SIL için iki yerleşik bayrak var: `--fixed-fps <fps>` (gerçek zaman senkronunu kapatır) ve `--disable-render-loop` ("yalnızca betik açıkça istediğinde çizer") ([komut satırı belgesi](https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html)). Gizli pencerede birlikte davranışları denenmedi.

### 2.3 Kamera sensörü hattı (SubViewport, geri okuma, gizli pencere)

**SubViewport kipleri** ([Viewports belgesi](https://docs.godotengine.org/en/latest/tutorials/rendering/viewports.html)): Never, Once, Always, When Parent Visible; varsayılan (Disabled/"when visible") davranışta dokusu çizilmeyen ya da görünmez SubViewport yenilenmez. Ebeveyn viewport'un `World3D`'sini paylaşır (kendi `world_3d` atanmadıkça); bu çoklu kamera için tek sahne demektir. Her SubViewport ayrı bir render geçişidir; N kamera yaklaşık N ek sahne çizimi demektir (bu çıkarımdır, ölçülmedi).

**Geri okuma yolu:** `viewport.get_texture().get_rid()` -> `RenderingServer.texture_get_rd_texture(rid, srgb)` ile RD dokusu -> `RenderingDevice.texture_get_data_async(texture, layer, callback)`. Belgede gecikme "şu an `rendering/rendering_device/vsync/frame_queue_size` kare" ve büyük dokularda yine de pahalı; `texture_download_region_size_px` ve `staging_buffer/block_size_kb` ayarlarla hız/bellek takası yapılabilir ([kaynak XML, 4.7.2](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingDevice.xml), [RenderingServer XML](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingServer.xml)). Geri okuma çağrısı render iş parçacığı bağlamı ister; `RenderingServer.call_on_render_thread(callable)` mevcut (belge imzası). Basit alternatif `get_texture().get_image()` (eşzamanlı, GPU'yu bekletir, ilk kare `frame_post_draw` sonrası). Format RD dokusunun ham formatıdır (ör. RGBA8); sRGB/doğrusal ayrımı `srgb` bayrağıyla.

**Gizli/arka plan pencere (kaynak kod okuması, 4.7.2-stable etiketi):**

- `main/main.cpp`: `wants_present = (DisplayServer::can_any_window_draw() || has_additional_outputs()) && is_render_loop_enabled()`; `if (wants_present || has_pending_resources_for_processing) { ... RenderingServer::draw(...) }`. Koşul sağlanmazsa o karede **çizim yapılmaz** ([main.cpp](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/main/main.cpp)).
- macOS: `can_any_window_draw()` herhangi bir pencerede `is_visible` ise true; `is_visible = (occlusionState & NSWindowOcclusionStateVisible) && isVisible` ([display_server_macos.mm](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/platform/macos/display_server_macos.mm), [godot_window_delegate.mm](https://raw.githubusercontent.com/godotengine/godot/master/platform/macos/godot_window_delegate.mm), master). Yani pencere başka pencerelerce tamamen örtülürse ya da gizlenirse çizim durur.
- Windows: `can_any_window_draw()` minimize olmayan pencere varsa true ([display_server_windows.cpp, master](https://raw.githubusercontent.com/godotengine/godot/master/platform/windows/display_server_windows.cpp)); minimize = çizim yok, örtülü ama minimize olmayan pencere çizmeye devam eder (kod okumasından çıkarım; Windows'ta ayrıca DWM/GPU davranışı ölçülmedi).
- Üçüncü taraf bir PR (godot-mcp #116) macOS'ta örtülü/minimize pencerede `Engine.get_frames_drawn()` değişmezken işlem karelerinin sürdüğünü, `RenderingServer.force_draw(false, delta)` ile viewport dokularının yine de çizildiğini ve "bir başlatma seçeneğiyle çözülemeyeceğini" raporluyor; Godot master (4.8 beta 1) üzerinde denenmiş ([PR](https://github.com/zeegzoog/godot-mcp/pull/116)). `force_draw` belgede "tüm viewport'ları bir kerede yeniden çizmeye zorlar, ana iş parçacığından çağrılmalı" ([XML](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingServer.xml)). **4.7.2'de gizli pencerede `force_draw`'ın gerçekten SubViewport'u güncellediği doğrulanmadı.**
- `--headless` = `--display-driver headless --audio-driver Dummy`; RenderingDevice headless'ta yoktur ([RenderingDevice sınıf belgesi](https://docs.godotengine.org/en/latest/classes/class_renderingdevice.html), [komut satırı](https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html)); eski forum yanıtı da headless'ta doku okunamadığını söylüyor ([forum](https://ask.godotengine.org/150510/godot-headless-not-same-godot-window-cant-render-the-backend), eski olabilir). Linux'ta Xvfb kullanılır, macOS/Windows için eşdeğeri yok.

Pratik çözüm basamakları (prototiple sınanacak, hepsi önerilir): (1) Operatör görünümü penceresi her zaman görünür, küçük de olsa ekranda; sensör/operatör süreci tek süreçse bu yeterli. (2) Pencere kapatılabilmeli ise sensör işi için ayrı, "gözetimsiz" bir Godot süreci düşünülür; yine de bir pencere gerektirir; ekranın bir köşesine küçük, salt-okunur, her zaman üstte olmayan pencere. (3) `force_draw` yedeği: pencere N işlem karesidir çizmiyorsa `force_draw` çağır. (4) Hiçbiri tutmazsa sensör sahnesi için macOS'ta sanal ekran / ikinci monitör ya da çıktı ekran olmadan çalışan başka bir yol (Metal başsız render) araştırılmalı; bu raporda **çözüm bulunamadı**.

### 2.4 RenderingDevice compute ile GPU sonar

- Compute shader'lar Forward+ ve Mobile'da çalışır, Compatibility'de çalışmaz; Mobile "genel olarak zayıf" (sürücü hataları) ([belge](https://docs.godotengine.org/en/latest/tutorials/shaders/compute_shaders.html)). Sürücüler: Vulkan, Direct3D 12, Metal; macOS'ta yerli Metal (Apple Silicon), x86'da MoltenVK yedek; Godot 4.7'den itibaren macOS 26+'ta Metal 4, eski macOS'ta Metal 3 ([iç mimari belgesi](https://docs.godotengine.org/en/4.7/engine_details/architecture/internal_rendering_architecture.html)). Windows'ta yeni projeler 4.6'dan beri varsayılan olarak D3D12 ([4.6 notları](https://godotengine.org/releases/4.6/)).
- İki yol: ana `RenderingDevice` (render iş parçacığında, `RenderingServer.call_on_render_thread`) ya da `RenderingServer.create_local_rendering_device()` (ayrı iş parçacığında hesap yapabilir; `sync()` CPU'yu bekletir; belgeye göre 2-3 kare beklenmeli). GodotOceanWaves yazarı yerel RD ile asenkron kademe hesabını denemiş ve CPU-GPU doku aktarımı yüzünden "belirgin ek yük" yaşamış ([README](https://github.com/2Retr0/GodotOceanWaves)); sonar için sonuç GPU'dan geri okunacağı için ilgili bir uyarıdır, ölçüm değildir.
- Windows TDR uyarısı: uzun compute işleri bölünmeli (compute belgesi).
- Donanım ışın izleme (BLAS/ray query) Godot RD API'sinde var ama belgelenen destek Vulkan; macOS desteklenmiyor (00, düzeltme 02.3).
- Godot'da hazır bir GPU sonar örneği **bulunamadı**. Embree CPU ana yol olduğuna göre (Ö4) Godot compute yalnızca isteğe bağlı ikinci kademe olmalı; Godot'ya bağımlı sonar hem başsız SIL'i hem de determinizmi bozar.

### 2.5 Okyanus ve su altı görüntüsü

| Proje | Lisans | Durum | Not |
|---|---|---|---|
| [2Retr0/GodotOceanWaves](https://github.com/2Retr0/GodotOceanWaves) | MIT | Son push 2025-04-29, 3.3k yıldız, `project.godot` Godot 4.3 / Forward+ | Tessendorf FFT, TMA+JONSWAP spektrum, yönsel yayılım, kademeler, köpük (Jacobian), RD compute. Su altı/yüzerlik yok |
| [tessarakkt/godot4-oceanfft](https://github.com/tessarakkt/godot4-oceanfft) | MIT | Son push 2025-11-19, 613 yıldız; "erken aşama", Godot 4.3+ | FFT, CDLOD, "temel" yüzerlik, "temel" su altı efekti (README, "su altı efektini iyileştir" yapılacaklar listesinde) |
| [Swell](https://forum.godotengine.org/t/swell-a-free-water-system-for-godot-4-7-2-some-4-8-features/146916) | doğrulanmadı | 2026-11-08 önizleme, Godot 4.7.2+ | Gerstner/FFT, su altı sis/kostik/ışık hüzmesi iddiası; henüz sürüm/lisans yok |
| [paddy-exe/godot-realtimecaustics](https://github.com/paddy-exe/godot-realtimecaustics) | MIT | Son push 2024-10 | Hacim içi kostik demosu |
| [rdgh0st/FFT-Ocean-Godot](https://github.com/rdgh0st/FFT-Ocean-Godot) | MIT | 2 yıldız | Küçük |

Su altı için hazır, bakımı olan bir eklenti bulunamadı; Beer-Lambert zayıflaması, bulanıklık ve kostik **kendi son işleminiz** olacak. Yol: `CompositorEffect` (post-opaque/post-transparent geri çağrımı, derinlik arabelleğine erişim, compute shader ile piksel başına işlem). Belge: yalnızca Mobile ve Forward+ destekler, sınıf **deneysel** etiketli, `_render_callback` render iş parçacığında çalışır, `render_data` saklanmamalı ([öğretici](https://docs.godotengine.org/en/stable/tutorials/rendering/compositor.html), [sınıf](https://docs.godotengine.org/en/stable/classes/class_compositoreffect.html)). Açık sorun: mobil renderer'da compositor için scene data kullanılamıyor ([#120019](https://github.com/godotengine/godot/issues/120019)) - Forward+ ile kal. Yerleşik alternatifler (Environment fog, volumetric fog) ucuz başlangıçtır; spektral (kanal başına) zayıflama için özel geçiş gerekir.

**Fizik-görüntü tutarlılığı (tasarım önerisi):** FFT'nin kendisi iki yerde ayrı koşarsa (GPU'da görsel, CPU'da fizik) kayan nokta farkları ve rastgele faz dizisi farkı yüzünden yükseklik uyuşmaz. İki seçenek: (A) **Bileşen listesi:** çekirdek spektrumdan tohumlu, deterministik bir dalga bileşenleri listesi (k, omega, genlik, faz) üretir; fizik bunları doğrudan toplar (nokta başına N bileşen), Godot aynı listeyi gölgelendirici uniform/tampon olarak alıp toplam yerine FFT kullanır ya da doğrudan toplar. (B) **h0 ızgarası:** çekirdek h0(k) ızgarasını (tohumlu Gauss) üretip Godot'ya gönderir; Godot yalnızca zaman evrimini + IFFT'yi yapar, fizik yüzerlik için seçilmiş en büyük K bileşeni toplar; test: yarıçaplı örnek noktalarda yükseklik farkı eşiği. GodotOceanWaves h0'ı kendi GPU gölgelendiricisinde Gauss sayılarla üretiyor ve harici h0 alma arayüzü yok (README tarifine göre; kod ayrıntısı bu raporda incelenmedi) - uyarlama gerekir (MIT izin verir). Gauss üreteçlerinin platformlar arası aynı dizi vermemesi 00/Ç belgelerindeki determinizm uyarısıyla örtüşür: dalga tohum + kendi RNG'nizle çekirdekte üretilmeli, Godot rastgele sayı üretmemeli.

### 2.6 Godot arayüz yetenekleri ve harita/grafik kararı (Ö6)

Godot `Control` düğümleri (form, `Tree`, `GraphEdit`/`GraphNode` ile düğüm tabanlı senaryo editörü, `TabContainer`, `CodeEdit`) yerleşiktir; bunlar için üçüncü taraf gerekmez (sınıf adları belgeden bilinen; bu raporda tek tek denenmedi). Zayıf alanlar:

| Alan | Godot içi | Web (MapLibre/uPlot) |
|---|---|---|
| Harita | Hobi düzeyi eklentiler: [godot-osm](https://github.com/coderbloke/godot-osm), [maplibre-native-godot](https://github.com/yuiseki/maplibre-native-godot) (MIT, 3 yıldız, "Linux/Windows/macOS'ta çalışıyor" iddiası, maplibre-native WebGPU başsız arka ucu; bakım riski yüksek) | [MapLibre GL JS](https://github.com/maplibre/maplibre-gl-js) (BSD-3, çok aktif, 12k yıldız); [maplibre-native](https://github.com/maplibre/maplibre-native) (BSD-2, C++) |
| Zaman serisi grafik | [TauPlot](https://godotengine.org/asset-library/asset/5037), [Graph2D](https://github.com/LD2Studio/godot4-graph2d), GodPlot gibi küçük eklentiler; yüksek örnek sayısında performans kanıtı yok | [uPlot](https://github.com/leeoniya/uPlot) (MIT, 10k yıldız, bu iş için tasarlanmış) |
| Form/senaryo editörü | Yerleşik `Control`/`GraphEdit` iyi | Web de iyi |
| Gömme | Godot penceresini başka uygulamaya gömmek desteklenmiyor. LibGodot PR'ı ([#90510](https://github.com/godotengine/godot/pull/90510)) 2026-11-11'de **birleştirilmemiş**; forumda "kütüphane olarak çalışamaz" ve borderless pencereyi üstüne yerleştirme çözümleri | Web paneli Godot'ya gömmek: [gdcef](https://github.com/Lecrapouille/gdcef) (MIT, C++, CEF) README'ye göre Linux ve Windows, **macOS için katkıcı aranıyor**; [godot_wry](https://github.com/doceazedo/godot_wry) (MIT) Rust ile yazılmış ve webview her zaman üstte çiziliyor |

Kanıta dayalı öneri: **harita ve grafikler web'de, ayrı pencerede; Godot sadece 3B, kamera/sonar görüntüsü ve HUD**. Operatör iki pencereyle çalışır (Godot + tarayıcı ya da bir kabuk). Bu karar gömme sorununu tamamen ortadan kaldırır. Eğer "bütün arayüz tek native uygulama olmalı" kesin şart ise Godot içi `Control` ağacı + maplibre-native-godot prototiplenmelidir, ama bu yolun mevcut kanıtı zayıftır (tek geliştirici, 3 yıldız). A2 (Rust bağımlılıkları) kararı godot_wry'ı zaten dışlar. Web kabuğu (Tauri) da 00'da K3 ile düştü; bu yüzden tarayıcı tabanı ya da CEF tabanlı bir C++ kabuk ayrıca değerlendirilmelidir (bu raporun kapsamı dışında, açık soru).

### 2.7 Tek kaynaklı sahne/varlık hattı

Akış: Blender (`.blend`) -> **glTF 2.0 (`.glb`)** -> (a) Godot görsel modeli, (b) Embree sonar geometrisi, (c) yüzerlik/çarpışma ağları. glTF 2.0 ISO/IEC 12113:2022 standardıdır ([Khronos](https://github.com/KhronosGroup/glTF)).

| Kütüphane | Dil / lisans | Not |
|---|---|---|
| [cgltf](https://github.com/jkuhlmann/cgltf) | C, MIT, tek başlık, son push 2026-02 | Küçük, bağımlılıksız; `extras` ham JSON olarak okunabilir (jsmn tabanlı); C++ sim sunucusu ve "ortak çekirdek C" ruhuna uygun. **Önerilen** |
| [tinygltf](https://github.com/syoyo/tinygltf) | C++, MIT | Başlık tabanlı, `extras`/`extensions` için `tinygltf::Value` ağacı; nlohmann/stb benzeri bağımlılıklar getirir |
| [assimp](https://github.com/assimp/assimp) | C++, BSD benzeri | Çok biçimli, ağır; glTF'e özgü `extras`/vendor alanlarını tam taşımayabilir (doğrulanmadı); gereksiz |

Malzeme verisi: glTF'de standart akustik alan yoktur; seçenekler `extras` (serbest JSON) ve vendor `extensions` bloğu (ör. `IDA_acoustic_material`, `IDA_optical_water`). Blender'da "Custom Properties" dışa aktarımı `extras` üretir; Godot importer bunları `metadata/extras` olarak taşır ama betikten okumak resmi belgelenmemiş, topluluk çözümü `GLTFDocumentExtension` ile ham JSON'dan okumaktır ([Godot içe aktarım yapılandırması](https://docs.godotengine.org/en/4.7/tutorials/assets_pipeline/importing_3d_scenes/import_configuration.html), [forum](https://forum.godotengine.org/t/gltf-import-how-does-one-get-metadata-extras/105711)). Öneri: standart dışı veri **adlı vendor uzantısı** (diğer araçlar yok sayar, şema sürümlenebilir); okuyucu hem cgltf (sunucu) hem `GLTFDocumentExtension` (Godot) ile aynı JSON şemasını kullanır. Sürümleme: dosyada `asset.extras.ida_schema_version` + sahne içeriği için SHA-256; derleme adımı "varlık manifesti"nde (hash, şema sürümü, malzeme tablosu) üretir; sim ve Godot başlangıçta manifest hash'ini karşılaştırır, uyuşmazlıkta reddeder (tasarım önerisi).

Çarpışma/yüzerlik ağları: Godot'nun `-col`/`-convcol`/`-convcolonly` ad sonekleri Godot'nun kendi fizik düğümleri içindir ([belge](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/node_type_customization.html)); sim sunucusu bunlardan faydalanmaz. Bu yüzden: Blender'da ayrı "COL_*" ve "BUOY_*" nesneleri (ad kuralı veya extras bayrağı), çevrimdışı araçla işlenir:

| Araç | Lisans | Platform | Not |
|---|---|---|---|
| [CoACD](https://github.com/SarahWeiii/CoACD) | MIT | C++ ve Python; Linux x86_64, Windows amd64, macOS x86_64 + Apple Silicon (README 2023-10 notu); 2026-04'te gerçek metrik kip | Kaliteli, yavaş olabilir; sonuç çevrimdışı derleme çıktısı olarak sürümlenir; çıktı tohum/iş parçacığı bağımlı olabilir (doğrulanmadı) - çıktıyı üretip depoda sakla, çalışma zamanında yeniden üretme |
| [V-HACD](https://github.com/kmammou/v-hacd) | BSD-3 | C++ başlık | Hızlı, daha kaba; son push 2025-09 |

Yüzerlik için dışbükey bölünme gerekmez: yüzerlik/hidrostatik genellikle ayrı, kapalı bir "ıslanma hacmi" ağı (watertight) ister; bunun için Blender'da kapalı, manifold bir ağ ve otomatik kontrol (kapalılık, hacim, ağırlık merkezi) gerekir. Fossen modeli parametrelerini (M_A, damping) bu ağdan türetme bu raporun kapsamı dışında.

### 2.8 Çarpışma / temas kütüphanesi

Gereksinim: araç dinamiği özel 6-DOF Fossen modelinde (6x6 M_A); kütüphaneden **yalnız algılama + temas bilgisi** alınacak, tepki özel uygulanacak.

| Kriter | Jolt Physics | Bullet 3 | FCL | libccd | Embree ile özel |
|---|---|---|---|---|---|
| Lisans | MIT | zlib | BSD-3 | BSD-3 | Apache-2.0 |
| Son sürüm | v5.6.0 (2026-07-11) | 3.25 (2022-04; ama depoda 2026-10 aktivite) | 0.7.0 (2021-09; depoda 2026-10 aktivite) | 2023-12 | 4.4.1 (00 belgesi) |
| Yalnızca sorgu kullanımı | `NarrowPhaseQuery::CollideShape` / `CastShape`; `CollideShapeResult`: penetrasyon derinliği, eksen, iki temas noktası | `btCollisionWorld::contactTest` / `convexSweepTest` | `collide`/`distance` (temas noktası, derinlik) | Yalnız dışbükey-dışbükey GJK/EPA | Işın/nokta sorguları; dışbükey-dışbükey temas yok, kendiniz yazarsınız |
| Determinizm | **Belgelenmiş**: `CROSS_PLATFORM_DETERMINISTIC` ile (~%8 yavaş) MSVC/clang/gcc, Windows/macOS/Linux, x86/ARM aynı sonuç; her derlemede macOS ARM64 NEON ve Windows MSVC x64 AVX2 doğrulanıyor | Garanti belgelenmemiş (bulunamadı) | Garanti yok (bulunamadı) | Garanti yok | Platformlar arası bit eşitliği yok (00, 10.9) |
| Önkoşullar | Dış sabit `-ffp-contract=off`, `/fp:precise`, aynı tanımlar, tek kaynak; broadphase sorguları ve dinleyici sıralaması **deterministik değil** (sıralamak gerek) | - | - | - | - |
| macOS ARM + Windows MSVC | Evet (NEON, MSVC 2022) | Evet | Evet (Eigen/ccd bağımlılığı) | Evet | Evet |
| Mesh vs mesh | Dinamik mesh pahalı, mesh-mesh çarpışma desteklenmez | Var | Var (BVH) | Yok | - |
| Hızlı küçük nesne (torpido) | `CastShape` / `LinearCast` motion quality | `convexSweepTest` | Sürekli çarpışma sınırlı | - | Işın ile uygun |

Kaynaklar: [Jolt Mimari belgesi: determinizm](https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Architecture.md), [Jolt sürüm notları](https://github.com/jrouwe/JoltPhysics/blob/master/Docs/ReleaseNotes.md), [CollideShapeResult](https://jrouwe.github.io/JoltPhysics/class_collide_shape_result.html), [Bullet lisansı](https://raw.githubusercontent.com/bulletphysics/bullet3/master/LICENSE.txt), [FCL lisansı](https://raw.githubusercontent.com/flexible-collision-library/fcl/master/LICENSE), [libccd](https://github.com/danfis/libccd).

Jolt'u "yalnızca algılama" olarak kullanma planı (öneri):

1. `PhysicsSystem` kurulur; havuz duvarları/taban **statik** bedenler (kutu veya ağ), hedef/engel statik ya da sensör, araç ve torpido **kinematik veya sorgu şekli** olarak modellenir. Araç şekli CoACD'den çıkan dışbükey parçalardan `StaticCompoundShape`.
2. Her sim adımında Fossen entegrasyonu aracı yeni pozuna getirir; sonra `NarrowPhaseQuery::CollideShape(vehicleShape, pose, settings, collector)` çağrılır (`PhysicsSystem::Update` çağırmak zorunlu değil; sorgu için statik sahne yeterli). Her temas için `mPenetrationDepth`, `-mPenetrationAxis.Normalized()` (temas normali) ve temas noktaları alınır.
3. **Temas tepkisi** özel: gövde çerçevesinde nokta kuvveti `F = k_n d + c_n v_n` (normal), Coulomb sürtünme, ve `tau = r x F`; bu `tau_ext` olarak Fossen denklemine eklenir. Sert yay/sönüm katsayıları adım süresiyle kararlılık sınırı gerektirir; alternatif olarak çarpışma sonrası pozu penetrasyon eksenine düzeltme (pozisyon projeksiyonu) + hız bileşenini sıfırlama. Eklenmiş kütle etkisi bu yöntemde zaten Fossen'de; kütüphanenin kütlesine ihtiyaç yok.
4. Torpido isabeti: `CastShape` (hareket segmenti) ya da `CastRay`; sensör gövde (`mIsSensor`) ile giriş olayı da alınabilir ama sensör "yalnız aktif bedenleri" algılar ve çözülmez (Mimari belge, Sensors bölümü). Küçük/hızlı nesne için `Discrete` hareket kalitesi tünelleme yapar, bu yüzden sorguya dayalı yaklaşım tercih edilir.
5. Determinizm: tek iş parçacıklı iş sistemi (`JobSystemSingleThreaded` Jolt'ta mevcut), sorgu sonuçlarını sıralayarak işle (sıra garantili değil), `CROSS_PLATFORM_DETERMINISTIC` açık, FP bayrakları sabit. Bu yine de çapraz platform bit eşitliğini kanıtlamaz; HIL'de gereken değil, SIL regresyon için gereken seviyede ölçülmelidir.

Değerlendirme: Jolt'un daha önceki Godot entegrasyonunda (Godot 4.6'dan beri varsayılan 3B fizik, [4.6 notları](https://godotengine.org/releases/4.6/)) olgunluğu bir ek kanıttır ama bizim kullanımımızda Godot'nun fiziği devre dışı kalır. FCL daha çok robotik (OMPL/MoveIt) için ama doğrulanabilir determinizm belgesi yok; Bullet'in son resmi sürümü 2022. Embree ile özel çözüm: torpido-hedef ışın/süpürme için yeterli olabilir, ama araç-gövde dışbükey-dışbükey temas için ayrı algoritma yazmayı gerektirir; önerilmez.

### 2.9 Paketleme ve dağıtım

- **macOS:** Resmi şablonlar tek "Universal 2" `.app` üretir; imza ve notarization varsayılan olarak açık, Developer ID gerekir; imza yoksa "ad-hoc" ve notarization kapalı seçilebilir (Gatekeeper engelleyebilir). GDExtension kullanırken **"Disable Library Validation" yetkisi** önerilir. Bağlı dinamik kütüphaneler için ayrıca imza ayrıntısı belgede yok ([macOS dışa aktarma](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_macos.html)). Uzantı `.framework` olarak dağıtılır (godot-cpp örnek `.gdextension`).
- **Windows:** x86_64 varsayılan, arm64 ayrı; imza için SignTool ya da osslsigncode; PCK gömme sınırı yaklaşık 3.75 GB ([Windows dışa aktarma](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html)). Uzantı DLL'inin yerleşimi ve MSVC çalışma zamanı bu sayfada anlatılmıyor (doğrulanmadı); `.gdextension` dosyasındaki yol dışa aktarmada kopyalanır (godot-cpp README örneği).
- **Dağıtım biçimi:** Laboratuvar aracı olduğundan "kullanıcıya ürün" kadar katı olmayabilir; ilk aşamada derleme betiği + imzasız yerel çalıştırma yeterli, ikinci aşamada CI'da imza/notarization.

## 3. Mimari için öneriler

1. **Üç süreç, açık sorumluluk:** (i) `sim-server` (başsız C++; fizik, zaman, HIL, Embree sonar, Jolt sorgusu, glTF okuma), (ii) `view` (Godot 4.7.x, tek sahne, ince GDExtension: IPC okuyucu + poz uygulayıcı + kamera sensörü SubViewport'ları + dalga görseli + HUD), (iii) web panelleri (tarayıcıda MapLibre + uPlot; sim sunucusuyla WebSocket). Gerekçe: Godot çökmesi/gizlenmesi fiziği ve HIL'i etkilemez; sonar ve çarpışma Godot'ya bağımlı olmaz.
2. **IPC iki kanal:** paylaşımlı bellek (poz anlık görüntüleri, sim->view; kamera kareleri, view->sim; seqlock) ve yerel TCP (komut, olay, metadata, uyandırma). İlk prototipte yalnızca TCP ile başla (iki OS'ta aynı), ölçüm yetersizse paylaşımlı belleğe geç - ön optimizasyon yapma. macOS paylaşımlı bellek adlarını 31 karakterin altında tut.
3. **Godot'da fizik yok:** Düğümlere doğrudan poz yaz, `physics_interpolation` bağımlılığı kurma; kendi enterpolasyonunu sim_time ile yap; Godot'nun fizik motoru (Jolt dahil) kapalı kalsın.
4. **Gizli pencere politikası (M13'e giriş):** Varsayım olarak "sensör süreci görünür bir pencereye sahiptir"; uzaktan/gizli çalışma gerekirse `force_draw` yedeği ve `--disable-render-loop` + sim saatli çizim prototiple sınanır. Bu çözülmeden "operatör gizlenmiş pencerede çalışırken kamera akışı sürer" vaadi verilmez.
5. **Kamera çıkışı:** Viewport başına sabit çözünürlük, `UPDATE_ALWAYS` (veya sim tetikli `UPDATE_ONCE`), `texture_get_data_async` + sim tarafından etiketlenmiş karelerin halka tamponu; `frame_queue_size`'ı proje ayarında düşük tut ve gecikmeyi ölç. Büyük kareler için bölge/blok ayarlarını ölçerek seç.
6. **Okyanus:** Önce çekirdekte tohumlu spektrum -> bileşen listesi; Godot'ya aktar. Görsel için GodotOceanWaves'ı çatallayıp (MIT) h0/bileşen girdisini dışarıdan al. Su altı: kendi `CompositorEffect` (Forward+); dalga yüksekliği uyumu için test noktaları (CPU vs GPU'dan geri okunan yükseklik) otomatik karşılaştırılır.
7. **Varlık hattı:** Tek `.glb` + vendor uzantılı malzeme verisi + manifest hash; `cgltf` ile sunucuda, `GLTFDocumentExtension` ile Godot'da oku; çarpışma/yüzerlik ağları çevrimdışı CoACD ile üretilip depoya artefakt olarak girer.
8. **Çarpışma:** Jolt, yalnızca sorgu modunda, özel tepki; ilk prototipte tek hedef: araç-duvar/taban penetrasyon sorgusu + penaltı kuvveti ile kararlı temas ve torpido `CastShape` isabeti.
9. **Arayüz:** Başlangıç web panelleri ayrı pencere; Godot içi HUD yalnız kamera/sonar görüntüleri ve temel durum için. İkinci bir karar noktası: bütün-native zorunluysa Godot `Control` + maplibre-native-godot prototipi.
10. **Derleme:** godot-cpp'yi CMake ile eklemek mümkün ama ikincil olduğu için, Godot uzantısı için SCons'u yedek tut; sim sunucusu için bağımsız CMake projesi. MSVC'de `CMAKE_MSVC_RUNTIME_LIBRARIES`'in tüm bağımlılıklarda tutarlı olması gerekir.

## 4. Riskler ve açık sorular

| # | Risk / soru | Etki | Azaltma |
|---|---|---|---|
| R1 | Pencere örtülü/minimize iken SubViewport çizilmiyor (4.7.2 kaynağında doğrulandı); `force_draw` çözümü 4.7.2'de doğrulanmadı | Kamera akışı kesilir | M13 prototipi birinci gün; yedek: görünür küçük pencere |
| R2 | Godot 4.7 macOS 26'da Metal donmaları (#119436, #123481 açık) | Uzun süreli SIL/HIL güvenilirliği | Saatlik koşu testi; gerekirse 4.6.x'e sabitle (4.6.3 raporlarda sorunsuz) |
| R3 | `texture_get_data_async` gecikmesi `frame_queue_size`'a bağlı ve "şu an" ifadesi sürümler arası değişebilir | Sensör gecikme bütçesi | Her yükseltmede yeniden ölç |
| R4 | `CompositorEffect` deneysel sınıf; API değişebilir | Su altı geçişi bakım yükü | Etkiyi tek dosyada izole et |
| R5 | Hazır okyanus projeleri Godot 4.3 döneminden ve bakımsız (son push 2025); Metal/D3D12 üzerinde doğrulanmadı | Görsel kalite, taşıma işi | Çatallama bütçesi; Swell olgunlaşırsa yeniden değerlendir |
| R6 | Web panellerini gömmek mümkün değil; iki pencere deneyimi | Operatör ergonomisi | Pencere düzeni betiği; tek-uygulama zorunluysa Ö6'yı yeniden aç |
| R7 | Jolt tepkisinin Fossen ile birleşimi: yay-sönüm sertliği adım süresiyle kararsızlık | Temas anında patlama | Çözülmesi gereken sayısal tasarım; küçük adım veya konum projeksiyonu |
| R8 | CoACD/V-HACD çıktısının tekrarlanabilirliği (tohum, iş parçacığı) | Varlık hattı tutarlılığı | Çıktıyı depoda sakla |
| R9 | Embree'yi Godot uzantısına statik bağlamak sembol/TBB çakışması riski taşıyabilir (Godot'nun kendi Embree'si yok edilmiş olabilir; doğrulanmadı) | Yükleme hatası | Embree'yi sim sunucusunda tut |
| R10 | maplibre-native-godot tek geliştirici; "macOS'ta çalışıyor" iddiası doğrulanmadı | Harita Godot içi yolu | Yalnızca B planı |

Açık sorular: (S1) Ekip operatörün Godot penceresini her zaman açık tutmayı kabul eder mi? (S2) Tek-uygulama native zorunluluğu var mı (Ö6/A5)? (S3) Hedef Mac çipi (A11) ve macOS sürümü (Metal 3 mü 4 mü)? (S4) Kamera çözünürlüğü/hızı ve kaç kamera (geri okuma bütçesi)? (S5) Torpido ve araç için sürekli çarpışma gereksinimi (tünelleme toleransı)?

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **Godot 4.7.2 çizimi atlıyor:** `main/main.cpp` içinde `wants_present`, `can_any_window_draw()` false iken (macOS'ta `NSWindowOcclusionStateVisible` olmayan, Windows'ta minimize pencere) `RenderingServer::draw` çağrılmıyor; dolayısıyla SubViewport'lar güncellenmiyor. Kaynak: [main.cpp @4.7.2-stable](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/main/main.cpp), [display_server_macos.mm @4.7.2-stable](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/platform/macos/display_server_macos.mm), [godot_window_delegate.mm](https://raw.githubusercontent.com/godotengine/godot/master/platform/macos/godot_window_delegate.mm), [Windows](https://raw.githubusercontent.com/godotengine/godot/master/platform/windows/display_server_windows.cpp). (Windows ve godot_window_delegate yolları master'dan okundu; 4.7.2'de aynı olduğu doğrulanmalı.)
2. **`force_draw` yedeği çalışıyor mu:** Üçüncü taraf PR macOS örtülü pencerede `RenderingServer.force_draw(false, delta)` ile viewport dokularının çizildiğini söylüyor (Godot 4.8 beta 1). 4.7.2'de ve Windows minimize durumunda doğrulanmadı. [PR #116](https://github.com/zeegzoog/godot-mcp/pull/116).
3. **Async geri okuma gecikmesi:** `texture_get_data_async` geri çağırımı `frame_queue_size` kare gecikmeyle gelir; büyük dokular için "yasaklayıcı maliyet" uyarısı. [RenderingDevice.xml @4.7.2](https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingDevice.xml).
4. **godot-cpp uyumluluğu:** 10.0.0-stable 4.3+ hedefler, önceki minör için derlenen uzantı sonrakinde çalışır, tersi çalışmaz; CMake desteği ikincildir. [README](https://github.com/godotengine/godot-cpp), [CMake belgesi](https://docs.godotengine.org/en/latest/tutorials/scripting/cpp/build_system/cmake.html).
5. **Jolt determinizmi:** `CROSS_PLATFORM_DETERMINISTIC` ile MSVC/clang/gcc ve Windows/macOS/Linux, x86/ARM sonuç eşitliği; macOS clang ARM64 NEON ve Windows MSVC x64 AVX2 her derlemede doğrulanıyor; narrowphase sorgu sonuç sırası ve dinleyici sırası deterministik değil. [Architecture.md](https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Architecture.md).
6. **Jolt yalnızca algılama kullanımı:** `NarrowPhaseQuery::CollideShape` penetrasyon derinliği/ekseni/temas noktaları verir; sensör çözümleme yapmaz ve yalnız aktif bedenleri algılar. [Architecture.md](https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Architecture.md), [CollideShapeResult](https://jrouwe.github.io/JoltPhysics/class_collide_shape_result.html). (Belgede `mContactPointOn1` açıklamasında iç tutarsızlık var, kaynaktan doğrulanmalı.)
7. **Gömme seçeneklerinin durumu:** LibGodot PR #90510 birleştirilmemiş; gdcef macOS desteklemiyor; godot_wry Rust tabanlı ve webview her zaman üstte. [PR](https://github.com/godotengine/godot/pull/90510), [gdcef](https://github.com/Lecrapouille/gdcef), [godot_wry](https://github.com/doceazedo/godot_wry).
8. **`CompositorEffect` kısıtları:** Yalnız Forward+/Mobile, deneysel sınıf, render iş parçacığı; depoda açık #120019 (mobilde scene data). [öğretici](https://docs.godotengine.org/en/stable/tutorials/rendering/compositor.html), [sınıf](https://docs.godotengine.org/en/stable/classes/class_compositoreffect.html), [#120019](https://github.com/godotengine/godot/issues/120019).
9. **macOS dışa aktarma:** GDExtension için "Disable Library Validation" yetkisi gerekli; notarization Developer ID ister. [belge](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_macos.html).
10. **Açık Godot hataları:** macOS Metal "timeout waiting for fence" (#119436) ve macOS 26 + M4/M5'te 4.7.1 kilitlenmesi (#123481) 2026-11-11'de açık. [#119436](https://github.com/godotengine/godot/issues/119436), [#123481](https://github.com/godotengine/godot/issues/123481).

## 6. İki haftalık prototip planı

Kabul ölçütleri **hedeftir** (önerilen eşik), ölçüm değildir; ölçülen değerler tabloya yazılacak. Donanım: Apple Silicon dizüstü (çip modeli A11'e bağlı) ve orta sınıf Windows GPU. Her test iki OS'ta.

| Gün | İş | Kabul ölçütü (hedef) |
|---|---|---|
| 1-2 | Godot 4.7.2 + godot-cpp 10 + (SCons ve CMake ikisini dene) ile "merhaba" uzantısı, iki OS'ta derle ve yükle | Her iki OS'ta uzantı yüklenir; CMake yolu çalışmıyorsa SCons'a karar ver |
| 2-3 | Sahte sim sunucusu (C++), 100 Hz poz anlık görüntüsü, TCP ve paylaşımlı bellek; Godot uzantısı okur, bir küre/araç düğümünü sürer | Poz yayımından Godot'da uygulanana kadar gecikme p50/p99 ölçülür (iki yöntem); 1 saatte kayıp/tekrarlayan kare sayısı = 0 hedefi |
| 3-4 | Enterpolasyon + sim_time eşleme; sabit gecikmeli oynatıcı | Görsel takılma yok (gözle + kare süresi histogramı); render_time hatası sınırı < 1 sim adımı |
| 4-6 | Kamera sensörü: 2 SubViewport (aynı World3D), `texture_get_data_async`, paylaşımlı bellek halka tamponu ile sim'e geri | 640x480 RGBA8 30 Hz: kare düşürme < %1, tetikten teslime gecikme dağılımı (p50/p99) kaydedilir, `frame_queue_size` 1-3 için karşılaştır |
| 6-7 | **Gizli pencere testleri (M13):** pencere örtülü, minimize, başka uygulama tam ekran, ekran koruyucu/uyku; `UPDATE_ALWAYS`, `force_draw` yedeği, `--disable-render-loop` + `--fixed-fps` | Her durumda: kamera kare hızı sürüyor mu (evet/hayır tablosu), `Engine.get_frames_drawn()` ve teslim edilen kare sayısı; macOS App Nap gözlemi |
| 7-8 | Okyanus: GodotOceanWaves çatalı, harici h0 / bileşen listesi girdisi; çekirdek (C++) aynı bileşenlerden yükseklik üretir | 25+ test noktasında CPU yüksekliği ile Godot'dan geri okunan yükseklik farkı eşiği (eşik: dalga yüksekliğinin küçük bir yüzdesi, prototipte belirlenecek) |
| 8-9 | Su altı: `CompositorEffect` ile Beer-Lambert zayıflaması + bulanıklık; (kostik opsiyonel) | Kare süresi artışı ölçülür; gözle kalite incelemesi |
| 9-10 | Jolt yalnız sorgu: araç şekli (CoACD dışbükey parçalar) ve havuz duvarı/taban; penaltı kuvvetli Fossen temasında kararlılık; torpido `CastShape` | 10 dk simülasyonda patlama yok; aynı koşu iki OS'ta karşılaştırılır (sapma raporlanır, eşitlik hedeflenmez); torpido isabeti 100/100 tünelleme yok senaryosu |
| 10-11 | Varlık hattı: Blender sahnesi -> `.glb`; cgltf ile sunucu, `GLTFDocumentExtension` ile Godot; manifest hash uyuşmazlık reddi | İki okuyucu aynı malzeme tablosunu okur; hash uyuşmazlığı reddedilir |
| 11-12 | Çoklu-saat uzun koşu (Mac üzerinde #119436/#123481 tarzı donma gözlemi) | 2 saat kesintisiz, "timeout waiting for fence" yok |
| 12-13 | Web paneli (MapLibre + uPlot, 60 Hz, 1 kHz telemetri alt örneklemesi) ile yan yana; operatör pencere düzeni | 1 saatte takılma yok; tarayıcı + Godot birlikte CPU/GPU kullanımı kaydedilir (M20) |
| 14 | Paketleme denemesi: macOS ad-hoc/imzalı `.app`, Windows exe; kayıt ve karar notu | Temiz bir makinede açılır; imzalama adımları belgelenir |

## 7. Kaynaklar

Godot ve godot-cpp
- Godot arşiv/sürümler: https://godotengine.org/download/archive/ ; https://api.github.com/repos/godotengine/godot/releases/latest
- Godot 4.6 sürüm notları: https://godotengine.org/releases/4.6/ ; 4.7: https://godotengine.org/releases/4.7/
- godot-cpp: https://github.com/godotengine/godot-cpp ; sürümler: https://api.github.com/repos/godotengine/godot-cpp/releases ; CMakeLists: https://github.com/godotengine/godot-cpp/blob/master/CMakeLists.txt
- GDExtension C++ örneği: https://docs.godotengine.org/en/4.7/tutorials/scripting/cpp/gdextension_cpp_example.html
- CMake ile godot-cpp: https://docs.godotengine.org/en/latest/tutorials/scripting/cpp/build_system/cmake.html
- godot-cpp-template: https://github.com/godotengine/godot-cpp-template
- Viewports: https://docs.godotengine.org/en/latest/tutorials/rendering/viewports.html
- Compute shaders: https://docs.godotengine.org/en/latest/tutorials/shaders/compute_shaders.html
- RenderingDevice sınıfı: https://docs.godotengine.org/en/latest/classes/class_renderingdevice.html ; XML (4.7.2): https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingDevice.xml
- RenderingServer XML (4.7.2): https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/doc/classes/RenderingServer.xml
- main.cpp (4.7.2): https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/main/main.cpp
- macOS/Windows DisplayServer: https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/platform/macos/display_server_macos.mm ; https://raw.githubusercontent.com/godotengine/godot/master/platform/macos/godot_window_delegate.mm ; https://raw.githubusercontent.com/godotengine/godot/master/platform/windows/display_server_windows.cpp
- Gizli pencere PR (üçüncü taraf): https://github.com/zeegzoog/godot-mcp/pull/116
- Komut satırı: https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html
- İç render mimarisi (Metal/Metal 4): https://docs.godotengine.org/en/4.7/engine_details/architecture/internal_rendering_architecture.html
- Render sürücüleri/renderer: https://docs.godotengine.org/en/4.7/tutorials/rendering/renderers.html
- CompositorEffect: https://docs.godotengine.org/en/stable/tutorials/rendering/compositor.html ; https://docs.godotengine.org/en/stable/classes/class_compositoreffect.html
- Açık sorunlar: https://github.com/godotengine/godot/issues/119436 ; /123481 ; /120097 ; /120333 ; /120019
- Dışa aktarma: https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_macos.html ; https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html
- glTF içe aktarma/ekstralar: https://docs.godotengine.org/en/4.7/tutorials/assets_pipeline/importing_3d_scenes/import_configuration.html ; https://forum.godotengine.org/t/gltf-import-how-does-one-get-metadata-extras/105711 ; https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/node_type_customization.html
- LibGodot PR: https://github.com/godotengine/godot/pull/90510

Okyanus ve görüntü
- https://github.com/2Retr0/GodotOceanWaves ; https://github.com/tessarakkt/godot4-oceanfft ; https://github.com/rdgh0st/FFT-Ocean-Godot ; https://github.com/paddy-exe/godot-realtimecaustics ; https://forum.godotengine.org/t/swell-a-free-water-system-for-godot-4-7-2-some-4-8-features/146916

Arayüz
- https://github.com/leeoniya/uPlot ; https://github.com/maplibre/maplibre-gl-js ; https://github.com/maplibre/maplibre-native ; https://github.com/yuiseki/maplibre-native-godot ; https://github.com/coderbloke/godot-osm ; https://github.com/Lecrapouille/gdcef ; https://github.com/doceazedo/godot_wry ; https://godotengine.org/asset-library/asset/5037 ; https://github.com/LD2Studio/godot4-graph2d

Bağlantı / IPC
- https://huggingface.co/learn/deep-rl-course/unitbonus3/godotrl ; https://rclgd.readthedocs.io/en/stable/getting_started/introduction.html ; https://learn.microsoft.com/en-us/archive/blogs/wincat/fast-tcp-loopback-performance-and-low-latency-with-windows-server-2012-tcp-loopback-fast-path ; https://lists.apple.com/archives/darwin-dev/2008/Aug/msg00035.html ; https://pikers.dev/pikers/piker/src/branch/macos_dnks_vibed_fixes/docs/macos/compatibility-fixes.md

Çarpışma ve varlık
- Jolt: https://github.com/jrouwe/JoltPhysics ; https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Architecture.md ; https://github.com/jrouwe/JoltPhysics/blob/master/Docs/ReleaseNotes.md ; https://jrouwe.github.io/JoltPhysics/class_collide_shape_result.html
- Bullet: https://github.com/bulletphysics/bullet3 ; FCL: https://github.com/flexible-collision-library/fcl ; libccd: https://github.com/danfis/libccd
- CoACD: https://github.com/SarahWeiii/CoACD ; V-HACD: https://github.com/kmammou/v-hacd
- glTF: https://github.com/KhronosGroup/glTF ; cgltf: https://github.com/jkuhlmann/cgltf ; tinygltf: https://github.com/syoyo/tinygltf ; assimp: https://github.com/assimp/assimp
