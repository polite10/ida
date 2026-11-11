# 02 - Mevcut Deniz Simülatörleri ve Grafik Motoru Seçimi

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Ayrıca dil kararı C/C++ olmadan önce, Rust varsayımıyla yazıldı; Rust'a özgü öneriler geçersizdir. Doğrulaması: [dogrulama/02-codex.md](dogrulama/02-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Araştırma tarihi: 2026-11-10. Kapsam: açık kaynak deniz/su altı simülatörleri, mimari fikirler, kur-ya-da-kullan kararı, render katmanı seçenekleri, deterministik başsız fizik çekirdeği desenleri.

Yöntem notu: Web sayfaları araç üzerinden özetlenerek okundu; GitHub meta verisi (lisans, son push, arşiv durumu) GitHub API'sinden 2026-11-10'da çekildi. Doğrudan doğrulanamayan her şey "doğrulanmadı" olarak işaretlidir. Aynı ifade "çıkarım" ise kaynakta açıkça yazmayan, kaynaklardan benim türettiğim sonuç demektir.

---

## 1. Özet

1. Hedef kısıt kümesini (macOS + Windows native, sonar, HIL, tek hibrit araç, torpido) birlikte karşılayan hazır bir simülatör bulunmadı. Hiçbiri "al ve kullan" adayı değil.
2. Stonefish teknik olarak en yakın deniz simülatörü (geometri tabanlı hidrodinamik, GPU sonar, DVL, XML senaryo) ama README'ye göre Windows sürümü yok ve macOS desteklenmiyor (OpenGL 4.3 yok). Lisansı GPL-3.0.
3. HoloOcean (Unreal 5.3) ve UNav-Sim (UE5 + AirSim) Linux+Windows; macOS yok. OceanSim Isaac Sim eklentisi (BSD-3-Clause), Isaac Sim'e bağımlı. DAVE/POSIM Gazebo+ROS 2 üzerinde; macOS için resmi yolu "Docker + RDP + yazılım render", CUDA sonar macOS'ta yok.
4. Gazebo (Harmonic/Jetty) Apache-2.0, aktif, Fossen tabanlı Hydrodynamics + Buoyancy + DVL sensörü var. Ancak macOS'ta GUI "kararsız", Windows "experimental". Başsız sunucu macOS Apple Silicon'da Homebrew ile çalışıyor. Ana yol değil, çapraz doğrulama aracı olabilir.
5. Dolaylı yollar (Docker/WSL) bu proje için pahalı: macOS'ta GPU/Metal yok, yazılım render; HIL için USB-seri geçişi ek risk (USB geçişi bu oturumda doğrulanmadı).
6. Öneri: fizik+sensör çekirdeğini sıfırdan Rust'ta, render'dan bağımsız, sabit adımlı, tek iş parçacıklı, tohumlu ve başsız yaz. Fikirleri ve yayımlanmış modelleri (Fossen, sonar yoğunluk modeli, DVL modeli) al; GPL kodunu kopyalama.
7. Render katmanı: Bevy (Rust, wgpu üzerinden Metal/DX12/Vulkan) opsiyonel izleyici olarak; çekirdek Bevy'ye bağımlı olmamalı. Bevy 0.20.1 (2026-10-28) ve yaklaşık 3 ayda bir kırıcı sürüm çıkıyor, sürüm sabitlemek gerekir.
8. Sonar için GPU ray tracing şart değil. İlk sürüm CPU ışın atımı (BVH) ile fizik tarafında yapılmalı; deterministik, başsız, macOS/Windows aynı. wgpu ray query hem Vulkan hem DX12 hem Metal'de var ama hâlâ `EXPERIMENTAL`; Bevy Solari ise buffer binding array istiyor ve Bevy 0.19 notlarına göre wgpu-Metal bunu desteklemiyor (çıkarım: Solari macOS'ta çalışmaz).
9. HIL zamanlaması mimarinin belkemiği: gerçek STM32 gerçek zamanda koşar. Sim saati üç kipli olmalı (duvar saati / serbest hızlı / kilitli adım). PX4'ün lockstep ve ArduPilot'un JSON/UDP arayüzü kanıtlanmış referans desenler.
10. Rapier'ın dokümanı çapraz platform determinizmi için `enhanced-determinism` özelliği, IEEE 754 uyumu ve `nalgebra` matematik fonksiyonları şartını koyuyor; bu bizim macOS (ARM) / Windows (x86) eşdeğerliği hedefimiz için doğrudan geçerli.

---

## 2. Bulgular

### 2.1 Simülatör karşılaştırma tablosu

Lisans/bakım sütunları GitHub API (2026-11-10) ve README'lerden. "Doğrulanmadı" = kaynakta bulunamadı.

| Simülatör | Motor / dil | Modellenenler | Sonar yaklaşımı | Lisans | macOS / Windows native | Bakım (son push) | HIL |
|---|---|---|---|---|---|---|---|
| **Stonefish** | C++, Bullet tabanlı özel fizik + özel OpenGL render | Gövde geometrisine dayalı hidrodinamik, okyanus/atmosfer/su altı render (dalga boyuna bağlı soğurma/saçılma); kamera, DVL, INS, IMU, basınç vb. | FLS, MSIS, SSS "vision sensor" olarak GPU'da işleniyor (sahneden girdi görüntüsü üretilip sensöre göre işleniyor, ardından gürültü ekleniyor); Profiler ve multibeam link sensörü, GPU detayı dokümanda yok | GPL-3.0 | **Hayır**: README "Windows sürümü yok, macOS OpenGL 4.3 olmadığı için desteklenmiyor" | Aktif (2026-11-05), son sürüm v1.5 (2025-06-10) | Yok (ROS arayüzü `stonefish_ros`); doğrulanmadı |
| **HoloOcean** | Unreal Engine 5.3 (v2.0), Python istemci | Buoyancy, akustik haberleşme, çoklu sensör (kamera, DVL, IMU, derinlik, LiDAR) | Imaging, sidescan, profiling, echo sounder. v1: octree önbelleği (yavaş); v2: doğrudan ray tracing, ama ön baskıya göre şimdilik yalnızca tek ışınlı echo sounder'da test edilmiş | Belirtilmemiş (BYU FRoSt Lab telif notu; UE EULA şart). `holoocean-ros` MIT. Ana repo anonim erişimde 404 | **Linux + Windows**; macOS yok | Docs güncel (2026-11-04); ana repo görünmüyor | ROS 2 köprüsü ile HIL testinde kullanılmış (ön baskı); başsız çalışma destekli |
| **DAVE / POSIM** | Gazebo + ROS 2 (C++/CUDA) | Fossen tabanlı araç modelleri, DVL, çoklu sensör, sualtı sahne efektleri | Çok ışınlı sonar: ışın tabanlı nokta saçılımı (faz, reverb, speckle), **NVIDIA CUDA gerekli** | Apache-2.0 | Windows: WSL; **macOS: Docker+RDP, yazılım render, CUDA sonar yok** | `Field-Robotics-Lab/dave` son push 2024-08; `IOES-Lab/dave` arşivlendi (2026-10), yerine `IOES-Lab/POSIM` aktif (2026-11-10) | Yok (doğrulanmadı) |
| **UUV Simulator** | ROS + Gazebo (eski) | Fossen hareket denklemleri, iticiler, kanatçıklar, 3D akıntı | Sonar eklentisi yok/özel; doğrulanmadı | Apache-2.0 (repo sayfası; API "NOASSERTION") | Hayır (ROS Kinetic/Melodic) | **Arşiv (6 Şub 2023)** | Yok |
| **VRX** | Gazebo Harmonic + ROS 2 Jazzy | Yüzey araçları (WAM-V), dalga/rüzgar/hidrodinamik parametre sayfaları var; yalnızca su üstü | Yok (su üstü) | Apache-2.0 | macOS/Windows belirtilmemiş (doğrulanmadı) | Aktif (2026-08-02) | Yok |
| **MARUS** | Unity (C#) | USV hareketi, sensör seti (depth, GNSS, IMU, lidar, kamera, sonar, su altı render), ROS | Dokümanda "Sonar" sensörü var, yöntem bulunamadı | `marus-core` Apache-2.0; bir makale "bazı parçalar özel/kapalı" diyor (doğrulanmadı) | Doğrulanmadı | Son push 2026-03 | Yok |
| **UNav-Sim** | UE5 + AirSim | Fotogerçekçi su altı render, BlueROV2 modeli, VSLAM yığını; AirSim sensörleri (GPS, IMU, kamera, mesafe) | Sonar tarif edilmiyor | "Other" (AirSim MIT + özgün lisans) | Linux + Windows (macOS yok); UE kaynak erişimi için Epic hesabı gerek | Son push 2025-05 | Yok |
| **OceanSim** | NVIDIA Isaac Sim 5.0 eklentisi (Python, Warp) | Kamera (su altı görüntü modeli), imaging sonar, barometre, DVL | GPU ray tracing ile nokta bulutu sorgusu + yoğunluk modeli + Rayleigh/Gauss gürültü (HoloOcean'dan esinlenmiş) | BSD-3-Clause (LICENSE dosyası) | Isaac Sim'e bağlı; işletim sistemi/GPU şartı sayfada yok (NVIDIA RTX ihtiyacı doğrulanmadı) | Aktif (2026-10-04) | Sonraki makalede HIL kurulumu anılıyor |
| **Gazebo Harmonic/Jetty** | C++ (gz-sim ECS) | `Hydrodynamics` (Fossen 6-DOF sönümleme, akıntı, eklenmiş kütle SDF'ten), `Buoyancy`, DVL sensörü (gz-sensors) | Yerleşik sonar yok; DAVE CUDA eklentisi var; **wgpu tabanlı sonar eklentisi** 2026-05 topluluk toplantısında duyuruldu (Vulkan/DX/Metal hedefli, olgunluk doğrulanmadı) | Apache-2.0 | macOS: Homebrew (Sequoia arm64 hazır paket), **GUI kararsız**; Windows: **experimental** (conda/pixi, Harmonic'te sunucu ve GUI ayrı süreç) | Çok aktif (2026-11-10) | Yok (PX4/ArduPilot entegrasyonları ayrı) |
| **MOOS-IvP uSimMarine** | C++, MOOS pub/sub | Basit kinematik model (thrust, rudder, turn_rate parametreleri); hidrodinamik değil | Sadece menzil sensörü simülatörleri (uSimBeaconRange vb.) | NOASSERTION (karma GPL/LGPL olabilir, doğrulanmadı) | Doğrulanmadı | Aktif (2026-10-29) | Yok |
| **ArduSub SITL** | ArduPilot C++ | Otopilot yazılımı kendisi; fizik dışarıdan | Yok | GPL-3.0 | SITL "Linux ve Windows'ta native" (resmi sayfa); macOS o sayfada geçmiyor | Çok aktif | SITL için JSON/UDP arayüzü (aşağıda) |

### 2.2 Simülatör bazında önemli ayrıntılar

**Stonefish.** Sensör sayfasına göre FLS, MSIS ve SSS görüntü tabanlı ve "tamamen GPU tabanlı"; gürültü yoğunluğa eklemeli ve çarpımsal. DVL: dört ışınlı, irtifa için en kısa mesafeyi veriyor, ama hız Doppler'den değil simüle edilmiş hareketten hesaplanıyor (dokümanda "ileride iyileştirilebilir" notu var). Senaryo XML dosyası, `stonefish_ros` düğümü yüklüyor; XML ayrıştırıcı bağımsız bir simülatörde de kullanılabiliyor. Kurulum belgesi: yeterli GPU yoksa "console mode" sınırlı işlevle çalışıyor. Yani macOS'a taşımak Metal/OpenGL 4.1 sınırı nedeniyle yeniden yazım gerektirir; bunu yapmanın GPL-3.0 sonuçları var.

**HoloOcean.** v2.0 ön baskısı: UE 4.27 -> 5.3, sonar octree yerine doğrudan ray tracing (her tick'te), tick tabanlı (hız testi 30 tick/s), ROS 2 köprüsü HIL testinde kullanılmış, sabit adım/determinizm anlatılmamış. Dokümanda "başsız çalışabilir" ve "2x gerçek zamana kadar" yazıyor. Kurulum: Epic hesabı + EULA onayı zorunlu, PyPI'da yok; Windows ikilileri elle derleniyor ve eski olabilir. OceanSim makalesine göre HoloOcean'ın imaging sonarı 1.5 - 3.8 FPS, ortam başına 317 - 502 sn octree önbelleği gerektiriyor (yazarların kendi ölçümü, RTX A6000).

**OceanSim.** Isaac Sim üzerinde Replicator ile görüş alanındaki geometri (normal, semantik) alınıp GPU ray tracing ile sorgulanıyor; yoğunluk = yansıtma × eğim (incidence) terimi × üstel menzil sönümü; menzil-azimut kutularına toplanıyor; eklemeli Rayleigh + çarpımsal Gauss gürültü. Varsayılan Oculus M750d (130° x 20°). Ölçüm: ~28 - 32 FPS (yazarların sonucu). Bu yoğunluk modeli sıfırdan yazılabilecek kadar basit; kod BSD-3 olduğundan referans alınabilir.

**DAVE.** Çok ışınlı sonar ışın tabanlı, nokta saçılımı; Gazebo özel sensörü + GPU lidar nokta bulutu -> derinlik görüntüsü -> CUDA. Bu yüzden NVIDIA'ya bağımlı; macOS'ta çalışmıyor. POSIM macOS sayfası bunu açıkça yazıyor: Gazebo yazılım render ile koşuyor, "karmaşık sahneler yavaş", CUDA sonar ARM64 imajında yok.

**Gazebo.** Hydrodynamics: Fossen 6-DOF doğrusal/kuadratik sönümleme, akıntı; eklenmiş kütle/Coriolis fizik motoruna `<fluid_added_mass>` ile devrediliyor; Buoyancy ile birlikte kullanılması öneriliyor. Dalga için Gazebo'da yerleşik bir sistem bulamadım (VRX kendi dalga alanı belgelerine sahip; ayrıntısı okunmadı). macOS'ta Jetty/Harmonic için hazır paket yalnızca Sequoia arm64; GUI için iki terminal (sunucu + GUI) gerekiyor.

**MOOS-IvP uSimMarine.** Hidrodinamik yok; thrust ve rudder'ı hız ve dönüş hızına çeviren kinematik model. Görev planlama için yeterli, bizim ihtiyacımız (dalış, torpido, sonar) için yetersiz. Mimari olarak pub/sub değişken tahtası (MOOSDB) ilginç bir fikir.

**ArduPilot SITL JSON arayüzü.** UDP; SITL'den fizik motoruna ikili paket: `magic(18458) | frame_rate | frame_count | pwm[16]`; fizikten otopilota JSON satırı: `timestamp` (mutlak fizik zamanı), `imu.gyro`, `imu.accel_body`, `position`, `velocity`, `attitude` veya `quaternion`; isteğe bağlı rangefinder, rüzgar, RC. `frame_rate` fizik adımını önerir, fizik motoru yok sayabilir. `frame_count` kayıp/çift paketi ve yeniden başlatmayı algılatır. Dokümanın kendisi kilitli adım olup olmadığını söylemiyor; bu doğrulanmadı.

### 2.3 Kur / yeniden kullan / sıfırdan değerlendirmesi

| Seçenek | Artı | Eksi | Karar |
|---|---|---|---|
| Stonefish'i kullan/portla | Deniz için en olgun özellik seti, aktif | macOS/Windows yok, OpenGL 4.3, GPL-3.0 (kodu projeye katmak lisans bulaşması), HIL yok, C++ | Kullanma. Dokümanı ve sensör tanımlarını referans al |
| HoloOcean | Sonar araştırma kalitesinde, HIL denemesi var | macOS yok, UE EULA, Epic hesabı, ana repo anonim erişimde yok, lisans belirsiz, ağır | Kullanma |
| OceanSim | Hızlı GPU sonar, BSD-3 | Isaac Sim (NVIDIA ekosistemi), macOS tarafı doğrulanmadı | Kullanma; sonar modelini referans al |
| Gazebo | Apache-2.0, Fossen Hydrodynamics, DVL, aktif, başsız mümkün | macOS GUI kararsız, Windows experimental, HIL için kendimiz yazmalıyız, torpido/sonar özel eklenti | Ana yol değil. **Opsiyonel çapraz doğrulama** (bizim hidrodinamiğimizi aynı SDF'te karşılaştırma) |
| Docker/WSL ile Linux simülatörü | Hızlı başlangıç | Aşağıya bakın | Ana yol olamaz |
| **Sıfırdan Rust çekirdek + parça alma** | Tek kod tabanı, iki işletim sisteminde native, HIL ve lockstep ilk günden tasarlanır, deterministik | Geliştirme emeği, doğrulama yükü | **Önerilen** |

**Docker/WSL gerçek maliyeti.**
- macOS: POSIM'in kendi dokümanına göre "Docker + RDP", Gazebo yazılım render, Metal Linux konteynerine CUDA/GPU olarak açılmıyor. Yani GPU sonar ve gerçekçi render ortadan kalkıyor (kaynak: POSIM requirements ve install-macos).
- Windows: WSL 2 + WSLg; POSIM'in sayfası GPU/CUDA geçişi için bir şey söylemiyor (doğrulanmadı).
- HIL: STM32 kartı USB üzerinden bağlanacak; Docker Desktop (macOS) ve WSL 2'de USB-seri aygıtın konteynere/WSL'e geçirilmesi ek araç ve gecikme jitter'ı getirir. Bu iddia bu oturumda kaynakla doğrulanmadı, bağımsız kontrol edilmeli.
- Zaman: ek sanallaştırma katmanı gerçek zamanlı HIL'in jitter bütçesini yer (ölçülmedi, çıkarım).

### 2.4 Mimari fikirler (simülatörlerden)

1. **Sensör eklenti yapısı.** Gazebo: sensörler `gz-sensors`'ta ayrı kütüphane, "sistem" (ECS) eklentileri fizikten veri çıkarıyor; DAVE multibeam bunu özel sensör olarak yazıp DVL kaynağını örnek almış. Stonefish: sensörler üç sınıfa ayrılmış (joint / link / vision), her biri kendi gürültü modeliyle. Çıkarım: bizde `Sensor` trait'i (`step(&WorldState, t) -> Option<Measurement>`) + sensör başına örnekleme periyodu + gürültü modeli + tohum.
2. **Senaryo formatı.** Stonefish XML (dünya + robot + sensör tek dosya), Gazebo SDF, HoloOcean JSON (doğrulanmadı). Öneri: sürümlü şema, `serde` ile RON/TOML/YAML; senaryo `seed`, `dt`, hedefler, torpido, çevre (akıntı, dalga) içersin.
3. **Zaman yönetimi.** PX4: simülatör ve SITL lockstep, böylece hızlandırma/yavaşlatma/duraklatma mümkün; Gazebo'da lockstep kapatılamıyor, hız `PX4_SIM_SPEED_FACTOR` ile (PX4 dokümanları). PX4 HITL ise gerçek donanımla USB/Ethernet MAVLink; "topluluk destekli, güncel PX4 ile çalışmayabilir" uyarısı var ve lockstep anlatmıyor. HoloOcean tick tabanlı. ArduPilot JSON: otopilot `frame_rate` önerir, fizik `timestamp` ile yanıt verir.
4. **Fizik/görselleştirme ayrımı.** Gazebo sunucu + GUI ayrı süreç (macOS ve Windows Harmonic'te zaten zorunlu). Stonefish "console mode". HoloOcean başsız. Çıkarım: sunucu/istemci bölünmesi kanıtlanmış ve platform dostu.
5. **Dikkat.** Sonar render ile fizik aynı döngüye bağlandığında (HoloOcean v1: 1.5 - 3.8 FPS) gerçek zaman bozuluyor; sonar ayrı bir bütçeye ve hesaplama yoluna sahip olmalı.

### 2.5 Render katmanı seçenekleri (kendi simülatörümüz için)

Sürümler: Bevy 0.20.1 (crates.io, 2026-10-28), wgpu 30.0.1 (2026-08-22), gdext (godot crate) 0.5.5, Rapier 0.36.1, Avian 0.8.0.

| Kriter | Bevy | Doğrudan wgpu | Godot 4 (+ gdext) | Unity | Unreal 5 |
|---|---|---|---|---|---|
| Rust entegrasyonu | Yerel Rust, ECS | Yerel Rust | gdext (MPL-2.0), "kullanılabilir", ara sıra kırıcı değişiklik | C# (Rust için FFI), Unity-Rust köprüsü ek iş | C++ (Rust için FFI/ek iş) |
| macOS (Metal) / Windows | wgpu üzerinden Metal, DX12, Vulkan | Aynı | Forward+ için Metal / D3D12 / Vulkan sürücüleri | macOS Metal destekli; HDRP Metal ray tracing 2025'te "değerlendirmede" | Mac: Sonoma 14.5+, M1+; Windows tam |
| Okyanus/su | Topluluk crate'leri (`bevy_water`, `bevy-aqua` 0.1.x: Bevy 0.19 için FFT/analitik dalga, köpük; çok yeni, doğrulanmadı), kendin yaz | Kendin yaz (FFT için `wgsl-fft` crate'i var) | Topluluk projeleri (GodotOceanWaves, FFT compute) | HDRP Water System (yakın kıyı kırılma dalgası yok); bir Mac M2 hatası bildirilmiş | Water plugin (Mac durumu doğrulanmadı) |
| GPU ışın izleme / derinlik sonar | wgpu `EXPERIMENTAL_RAY_QUERY`; Solari deneysel, `BUFFER_BINDING_ARRAY` ister, wgpu-Metal'de yok (çıkarım: macOS'ta yok); derinlik/compute yolu serbest | Ray query (Vulkan, DX12, Metal v29+ ama deneysel); compute shader serbest | Compute shader var (Forward+); donanım ray tracing API'si dokümanda yok (doğrulanmadı) | DXR Windows; Metal RT yok (2025 roadmap) | HWRT Mac'te "Experimental, M2+"; Nanite Mac "Beta" |
| Başsız çalışma | `ScheduleRunnerPlugin`, `headless` ve `headless_renderer` (pencere olmadan GPU'ya render + geri okuma) örnekleri resmi | Pencere gerektirmez (adapter/device doğrudan) | `--headless` (display driver headless); GPU'suz ortamlarda gerekli ama render yok; `--fixed-fps` | Batch mod, ağ gerekir | `-nullrhi` render'ı kapatır; `-RenderOffscreen` belgelenmemiş; `-deterministic`/`-fps` sabit adım |
| Lisans | MIT/Apache-2.0 (bilgi tabanı, bu oturumda doğrulanmadı) | MIT/Apache-2.0 (aynı not) | MIT (motor), gdext MPL-2.0 | Ticari; Personal için son 12 ay gelir/finansman < 200K USD | EULA; 1M USD sonrası %5 royalty; GPL (copyleft) kodla birleştirilemez |
| Hafiflik / kontrol | Orta (ECS, hızlı sürüm değişimi) | En yüksek kontrol, en çok iş | Orta | Ağır | En ağır |

Çıkarım ve yorum:
- **Bevy:** Rust-yerel, aynı repo içinde fizik çekirdeğini çağıran izleyici için en doğrusu. Bedeli sürüm hızı: README'ye göre yaklaşık 3 ayda bir kırıcı sürüm. Fizik çekirdeği Bevy'den bağımsız tutulursa bu risk yalnızca izleyiciyi etkiler.
- **Doğrudan wgpu:** Yalnızca GPU sonarı (compute / ray query) yazacaksak mantıklı; görselleştirme için çok iş.
- **Godot:** İzleyici için makul yedek; fakat Rust bağlayıcı ek bağımlılık ve ray tracing yok.
- **Unity/Unreal:** Bu proje için aşırı; Rust entegrasyonu zayıf, lisans/EULA yükü (UE'de GPL ile birleştirme yasağı), Mac'te donanım RT sınırlı. HoloOcean ve UNav-Sim bu yolun maliyetini gösteriyor (Epic hesabı, macOS yok).

### 2.6 Fizik çekirdeği: deterministik, başsız, render'dan bağımsız (kanıtlanmış desenler)

1. **Sabit adım + akümülatör** (Fiedler, "Fix Your Timestep"): gerçek süreyi biriktir, sabit `dt` ile adımla, render için ara durumlar arasında enterpole et. Değişken zaman adımı determinizmi bozar; akümülatör üst sınırı "ölüm sarmalı"nı engeller.
2. **Lockstep** (PX4 SITL/Gazebo): simülatör ve denetleyici aynı hızda kilitli, hızlandırma ve duraklatma mümkün. HIL'de (gerçek donanım gerçek zamanda koşar) lockstep ancak donanım yazılımı dış zaman kaynağını kabul ediyorsa mümkün; aksi halde sim duvar saatine bağlanır. Bu STM32 firmware tasarımına bağlı açık soru.
3. **Deterministik simülasyon testi** (FoundationDB, Rust: madsim, turmoil, moonpool, diviner): yürütme, rastgelelik, zaman ve G/Ç kontrol altında; çekirdek G/Ç'den ayrılmış. S2 yazısı Rust `HashMap`'in kasıtlı rastgele sıralamasına dikkat çekiyor.
4. **Rapier determinizm rehberi:** varsayılan yalnızca "yerel" determinizm (aynı makine, aynı Rapier ve derleyici sürümü). Çapraz platform için `enhanced-determinism`, IEEE 754-2008 uyumlu hedefler, ve başlangıç değerlerinde yerleşik `f32::sin` yerine `nalgebra` `ComplexField` fonksiyonları. `enhanced-determinism` ile `simd8` aynı anda açılamaz; `parallel` ile birleşebilir.
5. **PX4 SIH:** fizik+denetleyici aynı donanımda, bilgisayar yalnızca görüntü; senkronizasyon sorunu yok (PX4 dokümanı). Bizim için değil ama "fizik modelini firmware ile aynı repoda tutmak" fikri HIL'e yedek bir yol açabilir (ör. fizik STM32'de koşar) - gerçek firmware testi amacını zayıflatır, öneri değil, sadece not.

---

## 3. Mimari için öneriler

Her öneri bulguya dayanıyor; "çıkarım" etiketli olanlar benim yorumum.

1. **Üç katman, sıkı bağımlılık yönü.**
   - `sim-core` (kütüphane): dünya durumu, 6-DOF araç dinamiği, ortam (akıntı, dalga yüksekliği), sensör modelleri (IMU, derinlik/basınç, DVL, sonar, GNSS), aktüatörler, torpido ve hedefler. **Render, GPU, işletim sistemi, ağ, saat bağımlılığı yok.** Girdi: `(durum, komutlar, dt)`, çıktı: yeni durum + sensör ölçümleri.
   - `sim-host` (başsız yürütücü): zaman kipi, senaryo yükleme, HIL/SIL köprüsü (seri/UDP), kayıt/tekrar oynatma.
   - `sim-view` (Bevy, opsiyonel): durum akışını/kaydı çizer. Çekirdek bunu bilmez.
   Gerekçe: Gazebo sunucu/GUI ve Stonefish console modu aynı ayrımı yapıyor; macOS/Windows'ta GUI kararsızlığı yalnızca izleyiciyi etkiler.
2. **Zaman: üç kipli `Clock`.** (a) `WallClock`: HIL, sim duvar saatiyle adımlanır; (b) `Free`: SIL, olabildiğince hızlı; (c) `Lockstep`: her adımda firmware'e tick + sensör çerçevesi gönderip aktüatör yanıtını bekle (firmware SIL derlemesi veya firmware'de HIL-tick kipi gerekir). Protokol ArduPilot JSON'a benzer: `frame_count`, mutlak `timestamp`, kayıp/çift paket tespiti, yeniden başlatma. Gerekçe: PX4 lockstep, ArduPilot JSON arayüzü; gerçek STM32 gerçek zamanda koştuğu için (a) varsayılan olmalı. İlk sürümü yalnızca (a)+(b) ile bitirmek yeterli.
3. **Fizik: kendi 6-DOF Fossen modeli.** Gövde dinamiği (eklenmiş kütle, Coriolis, doğrusal+kuadratik sönümleme, yüzdürme, itici/pervane, kontrol yüzeyleri, akıntı) kendi kodun olsun; Rapier/Avian'ı yalnızca çarpışma/temas gerekirse yardımcı olarak kullan. Gerekçe: Gazebo Hydrodynamics ve UUV Simulator aynı Fossen modelini yayımlamış (Apache-2.0, referans alınabilir); genel oyun fizik motorları su dinamiğini bilmiyor; Stonefish bile Bullet'ı deniz için genişletmek zorunda kalmış.
4. **Determinizm kuralları.** Sabit `dt` (ör. 1 kHz iç, sensör periyotları dt'nin katı), tek iş parçacıklı çekirdek, `f64`, tohumlu RNG (kendi PRNG'n, platformdan bağımsız), `HashMap` yerine sıralı yapılar, trig için `libm`/`nalgebra ComplexField` benzeri platformdan bağımsız uygulama, Rapier kullanılırsa `enhanced-determinism`. CI'de macOS ve Windows'ta "altın iz" (durum karması) karşılaştırması. (Çıkarım: `libm` tercihi Rapier'ın matematik fonksiyonu uyarısından türetildi.)
5. **Sonar: sensör trait'i + üç gerçekleme, CPU ile başla.**
   - v1: CPU ışın atımı (BVH ile) fizik tarafında, OceanSim/HoloOcean yoğunluk modeli (yansıtma × eğim × üstel sönüm, menzil-azimut kutulama, Rayleigh + Gauss gürültü). Deterministik, başsız, SIL'de GPU gerektirmez, her iki OS'ta aynı sonuç. İşlem yükü tahmini: OceanSim örneği 3000×460 ışın kullanıyor (1.38 M ışın/kare); biz çok daha az ışınla (örn. onbinlerce) başlayabiliriz (tahmin, ölçülmedi).
   - v2: wgpu compute ile derinlik tabanlı (Stonefish/DAVE'in yaklaşımına benzer), Metal/DX12/Vulkan'da çalışır.
   - v3: wgpu `EXPERIMENTAL_RAY_QUERY` (Metal destekli, deneysel); Solari'ye bağlanma.
   Hangi sonar türünün gerekeceği (ekosounder, ileri bakışlı, çok ışınlı) açık soru.
6. **DVL.** Stonefish'in yaklaşımı (hız simüle edilmiş harekettan, 4 ışınlı irtifa, katman ortalaması, gürültü yüzdesi + sabit) başlangıç için yeterli; OceanSim'in "menzile bağlı örnekleme hızı ve dropout" fikri eklenebilir.
7. **Senaryo formatı.** Sürümlü, `serde` ile (RON veya TOML), `seed` içeren; hedefler, torpido tanımı ve ortam parametreleri tek dosyada. Stonefish XML ve Gazebo SDF'in dünya+robot+sensör tek-dosya fikrinden ilham.
8. **Render: Bevy izleyici + kayıt tekrar oynatma.** İzleyici canlı UDP akışından veya kayıt dosyasından çizsin; Bevy sürümünü sabitle, okyanus için önce basit Gerstner/analitik yüzey, sonra FFT (`wgsl-fft`, GodotOceanWaves referansı). Gerekçe: Bevy'nin başsız örnekleri ve wgpu'nun Metal/DX12 desteği; sürüm kırılganlığı çekirdeğe sızmaz.
9. **Gazebo'yu yalnızca bağımsız doğrulama için kullan (opsiyonel).** Aynı araç SDF olarak Gazebo Harmonic başsız sunucuda koşturulup bizim çekirdeğin hız/yüzdürme yanıtı karşılaştırılabilir. macOS'ta `gz sim -s` çalışıyor (Homebrew). Çıkarım: tam bir doğrulama değil, sağlama.
10. **Lisans hijyeni.** Stonefish (GPL-3.0) ve ArduPilot (GPL-3.0) kodunu kopyalama; yayımlanmış denklemleri, makaleleri ve Apache/BSD/MIT kodunu (Gazebo, VRX, DAVE, OceanSim) referans al. UE/Unity gibi EULA'lı bağımlılıklardan kaçın. (Hukuki danışmanlık değildir.)
11. **HIL protokolü.** UART/USB-CDC üzerinde basit, çerçeveli, CRC'li ikili protokol (ArduPilot JSON paketindeki `magic`, `frame_count` fikri); sim -> firmware: sensör çerçevesi; firmware -> sim: aktüatör komutları. MAVLink HIL_SENSOR / HIL_ACTUATOR_CONTROLS PX4'te var ama kendi firmware'imiz için gereksiz bir ağırlık olabilir (karar sonraya).

---

## 4. Riskler ve açık sorular

**Riskler**
- **Kapsam/emek:** Sıfırdan simülatör büyük iş; mevcut projelerin yıllarca biriken sensör/ortam modellerini yeniden yazmak gerekir. Hafifletme: v1'i dar tut (tek araç, düz seabed, basit hedefler).
- **Doğrulama:** Kendi hidrodinamiğimizin gerçekçiliği ölçülmeli (Gazebo karşılaştırması, havuz/deneme verisi). Ölçüm olmadan "gerçekçi" iddiası zayıf.
- **HIL gerçek zaman jitter'ı:** USB-seri ve OS zamanlayıcıları (macOS ve Windows) jitter üretir; Windows zamanlayıcı çözünürlüğü ve macOS güç yönetimi etkisi bu oturumda araştırılmadı.
- **Çapraz platform float farkları:** Rapier doğrulaması tek bir motor için; bizim kendi matematiğimiz için macOS ARM / Windows x86 eşdeğerliği test edilmeli (kaynaklı iddia yok, doğrulanmadı).
- **wgpu ray query hâlâ deneysel;** Metal'de ve Bevy Solari'de ek sınırlar var. GPU sonar tek bir deneysel özelliğe bağlanmamalı.
- **Bevy kırılma hızı** ve ekosistem crate'lerinin geride kalması (bevy-aqua Bevy 0.19 hedefli, güncel 0.20.1).
- **Kaynak kalitesi:** Bazı kaynaklar ön baskı veya yazar kendi ölçümü (HoloOcean 2.0 ön baskısı, OceanSim FPS tablosu); bağımsız kopya yok.
- **Proje durumu hareketli:** DAVE'in aktif çatalı 2026-10'da arşivlenip POSIM'e taşınmış; bakım durumu hızlı değişiyor.

**Açık sorular**
1. STM32U5 firmware'i dış zaman kaynağıyla (tick) sürülebilir mi? Evetse lockstep HIL mümkün, hayırsa HIL yalnızca gerçek zamanlı.
2. Donanım bağlantısı: USB-CDC mi, UART mı, Ethernet mi? Hedef gecikme ve örnekleme hızı ne?
3. Hangi sonar türü ve hangi çıktı biçimi (ham menzil mi, görüntü mü) firmware'in beklediği?
4. Geliştirme makinelerinin GPU'ları (Apple Silicon çipi, Windows tarafı GPU) -- özellikle Mac'te M3+ donanım RT var mı?
5. Hedefler ve torpido için gereken gerçekçilik düzeyi (sadece çarpışma/vuruş testi mi, hidrodinamik de mi)?
6. Sensör gürültü parametreleri için gerçek veri kaynağı var mı?
7. Dalga ve akıntı modeli ne kadar detaylı olmalı (yüzeyde seyir için dalga etkisi araç dinamiğine giriyor mu)?

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **Stonefish macOS ve Windows'ta desteklenmiyor.** README: "A version for Windows is not available at this time. MacOS is not supported due to its lack of support for OpenGL 4.3." Lisans GPL-3.0. Kaynak: https://github.com/patrykcieslak/stonefish (README) ve https://api.github.com/repos/patrykcieslak/stonefish
2. **HoloOcean 2.0 UE 5.3 kullanıyor, Linux+Windows destekli, sonar doğrudan ray tracing'e geçti ama ön baskıya göre şu ana kadar yalnızca tek ışınlı echo sounder'da test edildi; macOS desteği yok.** Kaynak: https://arxiv.org/html/2510.06160v1 ve https://byu-holoocean.github.io/holoocean-docs/v2.0.0/usage/installation.html
3. **OceanSim BSD-3-Clause, Isaac Sim 5.0 eklentisi; imaging sonar yaklaşık 28 - 32 FPS, HoloOcean 1.5 - 3.8 FPS (RTX A6000, yazarların ölçümü).** Kaynak: https://arxiv.org/html/2503.01074v2 ve https://github.com/umfieldrobotics/OceanSim (LICENSE: https://raw.githubusercontent.com/umfieldrobotics/OceanSim/main/LICENSE)
4. **Gazebo: macOS'ta GUI "known to be unstable", Windows "experimental"; Harmonic Windows'ta sunucu ve GUI ayrı süreç olmak zorunda.** Kaynaklar: https://gazebosim.org/docs/all/getstarted/ ve https://gazebosim.org/docs/harmonic/install_windows
5. **POSIM/DAVE macOS yolu Docker+RDP, yazılım render, CUDA multibeam sonar macOS imajında yok; `IOES-Lab/dave` arşivlendi, yerine POSIM.** Kaynaklar: https://ioes-lab.github.io/POSIM/install-macos.html , https://ioes-lab.github.io/POSIM/requirements.html , https://api.github.com/repos/IOES-Lab/dave
6. **wgpu Metal ray tracing: Metal için acceleration structure v29.0.0'da (2026-03-18) eklendi, ama özellik hâlâ `EXPERIMENTAL_RAY_QUERY` ve "major bug'lar içerebilir".** Aynı anda Bevy Solari `BUFFER_BINDING_ARRAY` istiyor, Bevy 0.19 notları "wgpu-Metal texture binding array'i destekliyor, buffer binding array'i desteklemiyor" diyor. Çıkarım olarak Solari macOS'ta çalışmaz. Kaynaklar: https://raw.githubusercontent.com/gfx-rs/wgpu/trunk/CHANGELOG.md , https://wgpu.rs/doc/wgpu/documentation/extensions/ray_tracing/index.html , https://raw.githubusercontent.com/bevyengine/bevy/main/crates/bevy_solari/src/lib.rs , https://bevy.org/news/bevy-0-19/
7. **Rapier çapraz platform determinizmi: `enhanced-determinism`, IEEE 754-2008, `nalgebra` matematik fonksiyonları; `simd8` ile birlikte açılamaz; varsayılan yalnızca yerel determinizm.** Kaynak: https://rapier.rs/docs/user_guides/rust/determinism/
8. **PX4: SITL ve simülatör lockstep'te; Gazebo'da lockstep kapatılamıyor; HITL gerçek firmware'i gerçek donanımda koşturur, "topluluk destekli, güncel PX4 ile çalışmayabilir".** Kaynaklar: https://docs.px4.io/main/en/simulation/ , https://docs.px4.io/main/en/simulation/hitl , https://docs.px4.io/main/en/sim_gazebo_gz/
9. **ArduPilot JSON SITL arayüzü: UDP, çıkış paketi `magic 18458, frame_rate, frame_count, pwm[16]`, giriş JSON'u mutlak `timestamp`, `imu`, `position`, `velocity`, `attitude/quaternion`.** Kaynak: https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/SITL/examples/JSON/readme.md
10. **Unreal Engine macOS: donanım ray tracing + MegaLights "Experimental, M2+"; Nanite "Beta, M2+"; Xcode 26.4 uyumsuz.** Kaynak: https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine

Doğrulanmadı ve ayrıca kontrol edilmesi gerekenler: Docker Desktop/WSL 2'de USB-seri aygıt geçişi ve gecikmesi; VRX/MARUS/MOOS-IvP'nin macOS-Windows durumu; Isaac Sim'in macOS desteği ve NVIDIA RTX zorunluluğu; Bevy/wgpu lisansları (bilgi tabanından, bu oturumda fetch edilmedi); Godot'un donanım ray tracing desteği olmadığı (dokümanda geçmiyor, açık doğrulama yok); `bevy-aqua` ve `bevy_water` kalitesi.

---

## 6. Kaynaklar

Simülatörler
- Stonefish: https://github.com/patrykcieslak/stonefish , https://stonefish.readthedocs.io/en/latest/ , https://stonefish.readthedocs.io/en/latest/sensors.html , https://stonefish.readthedocs.io/en/latest/install.html
- HoloOcean: https://byu-holoocean.github.io/holoocean-docs/v2.0.0/index.html , https://byu-holoocean.github.io/holoocean-docs/v2.0.0/usage/installation.html , https://arxiv.org/html/2510.06160v1 , https://frostlab.byu.edu/research/HoloOcean-Underwater-Simulator , https://github.com/byu-holoocean/holoocean-ros
- OceanSim: https://github.com/umfieldrobotics/OceanSim , https://umfieldrobotics.github.io/OceanSim , https://arxiv.org/html/2503.01074v2
- DAVE / POSIM: https://github.com/Field-Robotics-Lab/dave , https://github.com/IOES-Lab/dave , https://github.com/IOES-Lab/POSIM , https://ioes-lab.github.io/POSIM/requirements.html , https://ioes-lab.github.io/POSIM/install-macos.html , https://discourse.openrobotics.org/t/gsoc-2025-migrating-and-optimizing-dave-s-physics-based-sonar-plugin-for-ros-2-and-gazebo-harmonic/49835
- UUV Simulator: https://github.com/uuvsimulator/uuv_simulator
- VRX: https://github.com/osrf/vrx , https://github.com/osrf/vrx/wiki
- MARUS: https://marusimulator.github.io , https://github.com/MARUSimulator/marus-core , https://arxiv.org/html/2506.07781v1
- UNav-Sim: https://github.com/open-airlab/UNav-Sim , https://arxiv.org/pdf/2310.11927
- Gazebo: https://gazebosim.org/docs/all/getstarted/ , https://gazebosim.org/docs/harmonic/install_osx , https://gazebosim.org/docs/harmonic/install_windows , https://gazebosim.org/docs/jetty/install_osx , https://gazebosim.org/api/sim/9/theory_hydrodynamics.html , https://discourse.openrobotics.org/t/community-meeting-wgpu-based-sonar-plugin-for-gazebo-underwater-simulation/54783
- MOOS-IvP: https://github.com/moos-ivp/moos-ivp , https://oceanai.mit.edu/svn/moos-ivp-aro/trunk/ivp/src/dep_uSimMarine/uSimMarine.moos
- ArduPilot SITL: https://ardupilot.org/dev/docs/sitl-with-JSON.html , https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/SITL/examples/JSON/readme.md , https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html
- PX4: https://docs.px4.io/main/en/simulation/ , https://docs.px4.io/main/en/simulation/hitl , https://docs.px4.io/main/en/sim_gazebo_gz/ , https://docs.px4.io/main/en/sim_sih

Grafik/render
- Bevy: https://bevy.org/news/bevy-0-17/ , https://bevy.org/news/bevy-0-19/ , https://raw.githubusercontent.com/bevyengine/bevy/main/crates/bevy_solari/src/lib.rs , https://docs.rs/bevy/0.18.1/src/headless/headless.rs.html , https://dev-docs.bevyengine.org/src/headless_renderer/headless_renderer.rs.html , https://crates.io/api/v1/crates/bevy , https://docs.rs/crate/bevy-aqua/0.1.1
- wgpu: https://wgpu.rs/doc/wgpu/documentation/extensions/ray_tracing/index.html , https://raw.githubusercontent.com/gfx-rs/wgpu/trunk/CHANGELOG.md , https://docs.rs/wgsl-fft
- Godot: https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html , https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html , https://github.com/godot-rust/gdext
- Unity: https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.3/manual/System-Requirements.html , https://activation.unity3d.com/roadmap/2220-hdrp-raytracing-effects-on-metal , https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.4/manual/water-capabilities-of-the-water-system.html , https://unity.com/pricing-updates
- Unreal: https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine , https://www.unrealengine.com/en-US/license , https://www.unrealengine.com/en-US/faq , https://gpuopen.com/unreal-engine-performance-guide/

Mimari desenler
- Fix Your Timestep: https://gafferongames.com/post/fix_your_timestep/
- Rapier determinizm: https://rapier.rs/docs/user_guides/rust/determinism/
- Deterministik simülasyon testi (Rust): https://s2.dev/blog/dst , https://pierrez.github.io/moonpool/part2-foundations/01-determinism.html , https://pierrez.github.io/fdb-book/meet_fdb/correctness.html
