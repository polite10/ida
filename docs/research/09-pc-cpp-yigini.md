# 09 - PC Tarafı C++ Yazılım Yığını

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Doğrulaması: [dogrulama/09-codex.md](dogrulama/09-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Tarih: 11 Kasım 2026. Kapsam: kart yazılımı ve ortak çekirdek C, PC tarafı (simülatör, köprü, laboratuvar uygulaması) C++. Rust önerilmedi. Grafik motoru ve GPU sensör simülasyonu bu belgenin dışındadır; yalnızca arayüzle birleşme noktası verilmiştir.

Bu belge 03 numaralı raporu (Rust varsayımlı) ve Codex doğrulamalarını (dogrulama/03-codex.md, dogrulama/06-codex.md) temel alır. Dilden bağımsız bulgular (MCAP seçimi, harita verisi, senaryo formatı, hazır gözlem araçları, HIL zaman kipi ayrımı) yeniden araştırılmadı; yalnızca C++ yolunda değişenler eklendi.

İşaretler: **[K]** kaynaktan doğrudan okundu. **[İ]** benim çıkarımım veya mühendislik yargım, kaynakla kanıtlanmadı. **[D-]** doğrulanmadı (arama sonucu zayıf, birincil kaynak okunamadı ya da bilgiden aktarıldı).

---

## 1. Özet

1. **Derleyici tabanı:** C++20 hedeflenmeli, C++23 "ortak alt küme" olarak yalnızca CI'da üç derleyicide derlenenler kullanılmalı. Apple clang tarafında `std::jthread`/`stop_token` cppreference tablosunda listelenmiyor, `<chrono>` saat dilimi ve `std::print` kısmi; bunlardan kaçınmak güvenli. [K]
2. **Derleme sistemi:** CMake presets + vcpkg manifest modu (builtin-baseline + overrides) + küçük, sabit-commit'li bağımlılıklar için CPM. vcpkg'nin `x-gha` ikili önbelleği kaldırıldı; CI önbelleği NuGet/GitHub Packages üzerinden kurulmalı. [K]
3. **Paylaşılan C çekirdek:** tek CMake ağacında iki araç zinciriyle derlenen statik kütüphane (host + `arm-none-eabi`). Cortex-M33 yalnızca tek duyarlıklı FPU taşıdığından çekirdek `float` ile yazılmalı ve SIL de aynı tipi kullanmalı. GCC GNU kipinde varsayılan `-ffp-contract=fast` kullanır; SIL ile kart arasında FMA politikası bilinçli eşitlenmeli. [K/İ]
4. **Determinizm beklentisi:** aynı ikili + aynı platformda bit-bit tekrar üretilebilirlik gerçekçi; macOS-arm64 ile Windows-x64 arasında bit-bit eşitlik garanti değildir. Hedef "toleranslı eşdeğerlik + platform içi bit-bit" olmalı. Üç somut tuzak: FMA daralması (clang varsayılanı `on`, MSVC 2022 varsayılanı `off`), libm transandantal fonksiyonları, `std::normal_distribution` gibi taşınabilir olmayan dağılımlar. [K]
5. **Simülasyon çekirdeği:** tek dünyayı tek iş parçacığında, tam sayı nanosaniye zamanıyla, saat okumayan sabit adımlı bir `World::step()` olarak yazın; üç zaman kipi yalnızca dışarıdaki "sürücü" (pacing) farkıdır. Çok iş parçacığı: G/Ç, kayıt, UI akışı, GPU sensörleri ve çok dünyalı toplu koşular için; tek dünya içinde değil. [İ]
6. **Kart bağlantısı:** USB CDC için standart baud sorun değildir (cihaz baud'u yok sayar [İ]); gerçek risk Windows `usbser.sys` ve macOS CDC davranışındaki saha raporlarıdır, kaynaklar anekdottur. Gecikme ölçülmeden karar verilmemeli; yedek yol olarak WinUSB (MS OS 2.0 tanımlayıcıları) + libusb ile vendor bulk. Kartın Type-C portu kullanıcı kılavuzu içindekilerinde "High Speed" görünüyor, doğrulanmadı. [K/D-]
7. **Arayüz önerisi:** C++ başsız sunucu + WebSocket + web ön yüz (three.js, MapLibre GL JS, uPlot) birincil laboratuvar uygulaması; Dear ImGui yalnızca geliştirici içi hata ayıklama panosu. Qt'nin cazip parçalarından Qt Quick 3D ve Qt Graphs açık kaynakta yalnızca GPLv3, Qt Location "Technology Preview". Ekip JS/TS yazmak istemezse ikinci tercih Qt Widgets/QML (LGPL kısımlarıyla) ya da ImGui. [K]
8. **Süreç mimarisi:** simülasyon sunucusu ayrı süreç; UI ile WebSocket (JSON kontrol + ikili akış), HIL yolu sunucu içinde ayrı gerçek zamanlı iş parçacığı. GPU sensör süreci ayrıysa paylaşımlı bellek halka tamponu. gRPC gerekli görünmüyor. [İ]
9. **Kayıt:** MCAP C++ kütüphanesi (başlık-yalnız, lz4/zstd) doğrudan kullanılmalı; Foxglove SDK C++ (MIT) ek olarak canlı WebSocket ve Foxglove uyumu için isteğe bağlı, ancak Rust ile derlenmiş bir C kütüphanesini sarar ve türlü Foxglove şema kanalları C++'ta eksikti (sürüm notlarına göre değişmiş olabilir). [K]
10. **QGroundControl:** Apache-2.0 derlemesi ticari Qt ister, GPLv3 derlemesi açık Qt kullanır; yalnızca Qt 6.12.0 destekleniyor; özel otopilotta GenericFirmwarePlugin yetenek bayraklarını muhafazakâr bırakır. QGC "gerçek yer istasyonu provası" olarak kalmalı, laboratuvar uygulamasının yerini almamalı. [K]

---

## 2. Bulgular

### 2.1 Derleyici ve standart

Platformlar: Windows'ta MSVC (ve clang-cl), macOS'ta Apple clang. cppreference C++20/23 desteği tablosundan:

| Özellik | MSVC STL | Apple clang | Not |
|---|---|---|---|
| `std::format` | 19.29 (VS 16.10) | 15.0.0 (dipnotlu) | dipnot sayfa metninde yoktu |
| `std::span`, ranges, `bit_cast`, `numbers`, `atomic_ref` | var | var (ranges 14.0.3, atomic_ref 16.0.0) | |
| `std::jthread`, `std::stop_token` | 19.28 | **listelenmiyor** | libc++ bunları Eylül 2024'te "deneysel" bayrağından çıkardı, Xcode'un hangi sürümünde olduğu doğrulanmadı [D-] |
| `std::chrono` takvim/saat dilimi | 19.29 | **kısmi** | kullanmayın |
| `std::expected` | 19.33 | 15.0.0 | |
| `std::print` | 19.37 | 15.0/16.0 (notlu) | |
| `std::mdspan` | 19.39 | 15.0 | |
| `std::flat_map`, `import std`, `std::generator`, deducing this | var | **listelenmiyor** | C++23 hedeflemeyin |

Kaynaklar: [cppreference C++20](https://en.cppreference.com/w/cpp/compiler_support/20), [cppreference C++23](https://en.cppreference.com/w/cpp/compiler_support/23), [libc++ jthread commit](https://lists.llvm.org/pipermail/libcxx-commits/2024-September/091245.html).

İki platformda aynı davranış için kurallar [İ]:
- Standart: `CMAKE_CXX_STANDARD 20`, `CXX_EXTENSIONS OFF`; C çekirdek için `C_STANDARD 11`, `C_EXTENSIONS OFF`. Gerekçe: GCC'de ISO modu `-ffp-contract=off`, GNU modu `fast` varsayılan veriyor (aşağıda 2.3), yani standart bayrağı sayısal sonucu bile etkileyebiliyor.
- Üç derleyici + bir Linux işi (uyarıları ve farklı libstdc++'ı yakalamak için) CI matrisinde; `/W4 /permissive-` ve `-Wall -Wextra -Wconversion -Wdouble-promotion` (çekirdekte `float` → `double` sızıntısını yakalar) uyarı-hata olarak.
- Modüller (`import`) ve coroutine kullanımından kaçının; araç zinciri uyumu üç platformda eşit değil [D-].
- Windows'ta clang-cl isteğe bağlı; ASan için MSVC'de ASan bileşeni var, UBSan/TSan yok (aşağıda 2.10). MSVC `/fp:precise` varsayılan.
- Visual Studio CMake presets'te `CC/CXX=clang-cl` veya `toolset: ClangCL` ile seçilebilir; Ninja ile komut satırı derlemesinde `vcvarsall.bat` ortamı kullanıcı tarafından kurulmalı. [K] [MS CMake Presets](https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs)

### 2.2 Derleme ve bağımlılık yönetimi

| Ölçüt | vcpkg (manifest) | Conan 2 | CPM.cmake / FetchContent |
|---|---|---|---|
| Sürüm sabitleme | `builtin-baseline` + `version>=` + `overrides` [K] | profiller + (kilit dosyaları, bu sayfada doğrulanmadı) [D-] | commit hash ile sabitleme, `URL_HASH`, `CPMUsePackageLock` [K] |
| İkili önbellek | var; `x-gha` kaldırıldı, NuGet + GitHub Packages öneriliyor [K] | ikili paketler, platform/konfigürasyon başına [K] | yok; kaynaktan derler, `ccache` önerilir [K] |
| Qt gibi büyük paketler | kaynaktan derleme, ağır [İ] | benzer [İ] | uygun değil |
| macOS arm64 + Windows | iki platformda yaygın [İ] | iki platformda yaygın [İ] | sorunsuz (CMake) |
| Çevrimdışı çalışma | önbellek/asset cache ile | yerel cache | `CPM_SOURCE_CACHE` ile yapılandırma çevrimdışı olabilir [K] |

Kaynaklar: [vcpkg sürümleme](https://learn.microsoft.com/en-us/vcpkg/users/versioning), [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake), [Conan 2 giriş](https://docs.conan.io/2/introduction.html), [vcpkg ikili önbellek referansı](https://learn.microsoft.com/en-gb/vcpkg/reference/binarycaching), [vcpkg Haziran 2025 notu](https://devblogs.microsoft.com/cppblog/whats-new-in-vcpkg-june-2025/). vcpkg'nin son sürümü bu çalışmada 2026.10.16 (20 Ekim) görüldü: [vcpkg releases](https://github.com/microsoft/vcpkg/releases).

Tekrar üretilebilir derleme [İ]: vcpkg baseline commit'i + `overrides` ile bütün sürümler repoda; araç zinciri sürümleri CI'da açık (hareketli `latest` etiketi yerine açık runner: Codex, `macos-26-intel` gibi sürüm/mimari seçimini öneriyor); `CMakePresets.json` hem IDE hem CI hem komut satırı için tek kaynak. Microsoft: aynı preset dosyası Visual Studio, VS Code, CI ve komut satırında kullanılabilir; CMake ≥ 3.20 komut satırı için gerekli. [K]

Ortak C çekirdeği hem kartta hem PC'de derleme [İ]:
- Çekirdek kendi başına CMake hedefi (`ida_core`, STATIC, saf C11, `malloc` yok, işletim sistemi bağımlılığı yok, yalnızca `core/include` genel başlıkları).
- Aynı `CMakeLists.txt` iki "kök" altında kullanılır: PC kökü (ana CMake) ve firmware kökü (`CMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake` preset'iyle). Firmware preset'i çekirdeği `add_subdirectory(core)` ile alır. Bu, CMake'in belgelenmiş toolchain dosyası mekanizmasıdır ([MS belgesi toolchain dosyalarını açıklıyor](https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs)).
- Çekirdeğin giriş/çıkışı salt veri yapılarıdır (IMU örneği, derinlik, komut vb.); zaman `dt`/örnek zaman damgası olarak girer (Codex'in "algoritmik örnek zamanı" önerisiyle uyumlu).
- Kartta GCC `arm-none-eabi` ve ST'nin kendi araçlarıyla derleme sürtünmesi (CubeMX/CubeIDE) bu araştırmanın dışındadır [D-].

Önerilen repo düzeni (tek repo) [İ]:

```
ida/
  CMakePresets.json            # windows-msvc, windows-clangcl, macos-appleclang, linux-ci, firmware-arm
  vcpkg.json                   # builtin-baseline, overrides
  cmake/                       # toolchain dosyaları, uyarı bayrakları, fp-policy.cmake
  core/                        # SAF C11: estimation, guidance, control, fire-control
    include/ida/*.h  src/*.c  test/
  proto/                       # tek kaynak: kart<->PC çerçeve başlıkları (C), mavlink dialect XML
  firmware/                    # kart portu: HAL, linker, board support; core'u tüketir
  sim/                         # C++20: clock, world, dynamics, env, sensors/, actuators/, scenario
    plugins/                   # sensör eklentileri (sonar, DVL, IMU, ...)
  hil/                         # taşıma (CDC/bulk), çerçeveleme, köprü, zaman sürücüleri
  server/                      # sim sunucusu: WebSocket, kayıt (MCAP), komut API
  app-web/                     # TS/HTML ön yüz (veya app-imgui/)
  scenarios/  data/  tools/
  third_party/                 # CPM ile sabitlenmiş küçük başlık-yalnız bağımlılıklar
  .github/workflows/
```

### 2.3 Sayısal temel ve kayan nokta determinizmi

**Eigen**
- Eigen 5.0, C++14 gerektiren son büyük sürüm; master/sonrası C++17 gerektirecek [K]. Eigen 3.4 hâlâ hata düzeltmesi alıyor [K]. 5.0.1'de Windows BLAS/LAPACK düzeltmesi var; 5.0.0'ın CMake hedefi dil standardını geçişli olarak istemiyor (issue) [K]. [Eigen 5.0.0](https://newreleases.io/project/gitlab/libeigen/eigen/release/5.0.0), [5.0.1](https://newreleases.io/project/gitlab/libeigen/eigen/release/5.0.1), [CMake issue](https://gitlab.com/libeigen/eigen/-/issues/2968).
- Eigen'in rastgele sayı üretimi sürümler ve platformlar arasında tutarlı garanti etmiyor [K] (aynı 5.0 notları).
- Alternatifler: sabit boyutlu küçük matrisler için kendi minimal `float`/`double` vektör-kuaterniyon kütüphanesi (çekirdek C'de zaten gerekli, PC tarafı aynı kodu kullanır); glm/Blaze vb. bu çalışmada araştırılmadı [D-]. [İ] Çekirdekte (C) Eigen kullanılamaz; yani kestirim/kontrol hesabı kendi küçük C matris yardımcılarına dayanır. PC tarafındaki fizik için Eigen mantıklı; ancak SIMD genişliği (x64 AVX/SSE, arm64 NEON) toplama sırasını değiştirebileceği için determinizm koşusunda `EIGEN_DONT_VECTORIZE` açılmalı ve etkisi ölçülmeli [D-: bu mekanizma Eigen belgesinden okunmadı].

**Kayan nokta politikası (kaynaklı olgular)**
- Clang: `-ffp-contract` varsayılanı `on` (CUDA/HIP dışı); `-ffast-math` bunu `fast` yapar; `-ffp-model=strict` FMA daralmasını kapatır. Apple clang'e özel bir not sayfada yok. [Clang kılavuzu](https://clang.llvm.org/docs/UsersManual.html)
- MSVC: varsayılan `/fp:precise`; VS 2022'den itibaren `/fp:precise` altında daralma (FMA) **varsayılan üretilmez**, önceki sürümlerde üretilirdi; `/fp:contract` ile açılır. [MS /fp](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior)
- GCC: ISO C kipinde (`-std=c11`) `-ffp-contract=off`, aksi halde `fast`. [GCC Optimize Options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
- Cortex-M33: isteğe bağlı yalnızca tek duyarlıklı FPU (FPv5-SP); `double` donanımda yok. [Arm blogu](https://community.arm.com/groups/processors/blog/2016/10/25/five-key-features-of-the-arm-cortex-m33-processor), [ST PM0264](https://www.st.com/resource/en/programming_manual/pm0264-stm32cortexm33-mpu-programming-manual-stmicroelectronics.pdf). STM32U5'te `double`'ın yazılımla emüle edildiği doğrudan doğrulanmadı [D-].
- Transandantal fonksiyonlar (sin, cos, exp, atan2) IEEE 754 tarafından doğru yuvarlanmak zorunda değildir; platformlar arası farkın ana kaynağıdır. [bugnet](https://bugnet.io/blog/how-to-debug-floating-point-determinism-across-platforms) (ikincil kaynak) ve [Feltor makalesi](https://arxiv.org/pdf/1807.01971).
- `std::normal_distribution` çıktısı standartta zorunlu değil; libstdc++/libc++/MSVC aynı motorla farklı dizi üretebilir. Motorlar (`mt19937`) sabittir. [Quuxplusone](https://quuxplusone.github.io/blog/2019/10/22/psa-stateful-distributions) ve StackOverflow başlıkları (ikincil).

**Gerçekçi beklenti [İ, yukarıdaki olgulara dayalı]**

| Düzey | Gerçekçi mi? | Koşul |
|---|---|---|
| Aynı ikili, aynı makine, aynı girişle bit-bit | Evet | tek iş parçacıklı dünya, sabit sıra, kendi RNG'si |
| Aynı kaynak, aynı OS, farklı CPU/derleyici sürümü | Çoğunlukla | `-ffp-contract=off`, fast-math yok, aynı libm |
| macOS-arm64 ↔ Windows-x64 bit-bit | Garanti edilemez | kendi `sin/cos/exp` gerçeklemeleri + FMA kapalı + vektörleşme kapalı ile mümkün olabilir, kanıtlanmalı |
| macOS ↔ Windows toleranslı eşdeğerlik (ör. durum 1e-6 bağıl) | Evet, kısa ufukta | kaotik/kararsız senaryolarda zamanla ıraksar |
| SIL ↔ kart (float32, Cortex-M33) bit-bit | Hedeflenmemeli | farklı libm, farklı FMA, farklı sıra; "toleranslı eşdeğerlik" |

Öneriler [İ]:
1. Tüm hedeflerde `-ffp-contract=off` (MSVC'de varsayılan zaten kapalı, `/fp:precise`; clang'de açıkça kapat). Kartta da (GCC) açıkça `off` verilirse SIL ile eşleşir; FMA'lı hızlı yol kasıtlıysa SIL'de de `fmaf()` ile açıkça yazılmalı.
2. `-ffast-math`/`/fp:fast` hiçbir yerde yok.
3. Simülasyonda kendi RNG'si: sayaç tabanlı (örn. Philox/PCG) + kendi normal dağılımı (Box-Muller); bileşen başına ayrı akış. `<random>` dağılımları kullanılmaz. (Algoritma adları bilgiden, üzerinde arama yapılmadı [D-].)
4. Transandantal fonksiyonlar için tek bir `ida_math` katmanı: önce `std::` ile başla, farklar ölçülünce gerekirse sürümlenmiş kendi polinom yaklaşımı veya doğru-yuvarlamalı bir kütüphane (ör. CORE-MATH, burada kontrol edilmedi [D-]) ile değiştir.
5. Sınama: (a) birim test: "altın iz" (golden trace) — kısa senaryo, her N adımda durum hash'i ve durum vektörü; (b) CI'da `macos-latest` (arm64) ve `windows-latest` (x64) işlerinde aynı senaryo; (c) bit-bit hash karşılaştırması yalnızca aynı platform iki koşuda, çapraz platformda ULP/bağıl tolerans bandı ve ilk ıraksayan adımın raporu; (d) Eigen vektörleşmesi açık/kapalı kıyası; (e) sürüm yükseltmelerinde (Xcode, MSVC) altın iz yeniden üretilmesi. Runner eşleşmesi: `macos-latest` macOS 26 arm64, `windows-latest` Server 2025 x64 (Codex doğrulaması, [runner-images](https://github.com/actions/runner-images)).

### 2.4 Simülasyon çekirdeği mimarisi [İ, aksi belirtilmedikçe]

**Katmanlar**
1. `sim-core` (saf C++20, başsız, işletim sistemi G/Ç yok): `Clock`, `World`, `Entity/State`, dinamik modeller, ortam, sensör/aktüatör arayüzleri, hata enjeksiyonu, senaryo yürütücü.
2. `sim-io`: HIL taşıma, kayıt, ağ. Çekirdeği yalnızca arayüzle çağırır.
3. `server`: süreç, iş parçacıkları, WebSocket, komut API.

**Zaman**: tek `int64` nanosaniye tick; sabit `dt` (ör. 1 kHz taban, sensörler tam sayı oranlarla: 200 Hz IMU, 10 Hz DVL...). Çekirdek içinde `std::chrono::steady_clock` okunmaz. `World::step()` yalnızca bir sonraki tick'e ilerler.

| Kip | Sürücü | Davranış |
|---|---|---|
| SIL (hızlandır/duraklat) | `FreeRunDriver(speed, paused)` | döngü olabildiğince hızlı ya da hız çarpanıyla, duraklatılabilir |
| 1x gerçek zamanlı HIL | `RealtimeDriver` | duvar saatine göre tick; kart kendi saatini koşturur; kaçırılan son tarihleri sayaç olarak raporla |
| Adım eşlemeli gerçek zamanlı HIL | `LockstepDriver` | her IMU çerçevesi için aktüatör cevabını bekle (Codex'in 06'daki "sensör–aktüatör bariyeri"); toplam bütçe örnek periyodunda (200 Hz'de 5 ms) |

Bu tablo Codex'in üç durum ayrımıyla birebir uyumludur; bariyerin tam firmware determinizmi vermediği ve hızlandırma/duraklatma için sanal firmware saati gerektiği oradaki hükümdür.

**Sensör eklenti yapısı**: ortak arayüz `ISensorModel { configure(json); reset(seed); step(t, truth_view, env_view) -> optional<Sample>; }`, kayıt için çalışma zamanı kaydı (registry) + statik bağlanan eklentiler (dinamik kütüphane yüklemesi iki platformda ABI/yol/derleyici sorunları getirir). Her sensörün: örnekleme periyodu, gecikme modeli (kuyruk), gürültü akışı (kendi RNG akışı), hata enjeksiyon kancası (kayıp, donma, sapma). GPU tabanlı sensörler (sonar, kamera) aynı arayüzü uygular: `step()` iş gönderir, sonuç `latency_ns` sonra hazır olur. Böylece grafik/GPU araştırmasıyla birleşme arayüzü şudur: girdi = kesin durum + ortam görünümü + zaman; çıktı = zaman damgalı ikili örnek (ör. sonar görüntü satırları) — süreç içi kütüphane ya da ayrı süreç + paylaşımlı bellek (bkz. 2.7).

**FMI notu**: FMI 3.0 co-simulation FMU'ları adım tabanlı (`doStep`) olup per-platform ikili klasörleri (ör. `aarch64-darwin`, `x86_64-windows`) taşır [K, ikincil: [FMI 3.0 belgeleri](https://fmi-standard.org/docs/3.0), [fmi4c](https://github.com/Ecos-platform/fmi4c)]. Hidrodinamik/akustik modelleri üçüncü taraf FMU olarak almak ileride seçenek olabilir; bu aşamada eklenti arayüzü yeterli, fmi4c'nin macOS arm64/Windows derlemesi doğrulanmadı [D-].

**Senaryo dosyası okuma**: biçim kararı 03/Codex'te verildi; C++ kütüphaneleri (nlohmann/json, yaml-cpp, toml++, JSON Schema doğrulayıcı) bu çalışmada araştırılmadı [D-]. Öneri [İ]: senaryo şeması (JSON Schema) tek kaynak, hem sunucu hem web editörü aynı şemayla doğrular.

**Çok iş parçacığı**
- Dünya başına tek iş parçacığı (deterministik). Ölçekleme: bağımsız dünyalar paralel (Monte-Carlo, regresyon). Görev grafı ihtiyacı doğarsa Taskflow (başlık-yalnız, MIT; v4.0.0 C++20 gerektiriyor, bu sayfa ikincil) [D-].
- Ayrı iş parçacıkları: HIL G/Ç (gerçek zamanlı öncelik), kayıt (MCAP yazıcı), WebSocket yayını, GPU sensör işleri. Aralarında sınırlı SPSC kuyruklar; hızlı yol kilit içermemeli. Kütüphane önerisi (moodycamel vb.) bu çalışmada doğrulanmadı [D-]; kendi halka tamponu yazmak da basit.

**İşletim sistemi zamanlama gerçekleri**
- Windows: `timeBeginPeriod` Windows 10 2004'ten itibaren işlem kapsamlı; Windows 11'de pencere sahibi işlem tamamen gizlenir/simge durumuna alınırsa yüksek çözünürlük garantisi yok. [MS timeBeginPeriod](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod). Yüksek çözünürlüklü bekleme zamanlayıcısı (`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`) Windows 10 1803+ (ikincil kaynak, MS sayfası okunmadı) [D-]. Sonuç: sunucu başsız/arka plan sürecinde de ölçülmeli.
- macOS: çekirdek yerleşimi QoS ile belirlenir; arka plan QoS iş parçacıkları E çekirdeklerine gider; zaman kısıtlı/audio-workgroup iş parçacıklarının P çekirdeğinde koşması garanti değil (forum raporu). [Eclectic Light](https://eclecticlight.co/2025/05/09/what-is-quality-of-service-and-how-does-it-matter/), [Apple forum](https://developer.apple.com/forums/thread/726096). Gerçek zamanlı HIL için tek yol ölçüm: sonuç = tick gecikme dağılımı raporu.

### 2.5 Kartla haberleşme

**Kart bilgisi**: ST-LINK sanal COM portu USART1 (PA9/PA10), 115200 8N1 varsayılan; ST-LINK USB konnektörü CN5 (Micro-B); ayrıca ayrı bir USB Type-C (CN4) bölümü başlıklarında "High Speed, Sink Only" geçiyor — kullanıcı portu olarak USB HS cihaz kullanılabilirliği doğrulanmadı [D-]. [UM2967](https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf) (arama özeti; sayfa okunmadı). Codex ayrıca kartın ST tarafından üretimden kalktığını kaydetti.

**Kütüphaneler**

| Kütüphane | Dil/Lisans | Güçlü yan | Zayıf yan / not |
|---|---|---|---|
| Boost.Asio `serial_port` | C++ / Boost | asenkron model, WebSocket (Beast) ile aynı çatı | özel baud (termios) sınırı bildirildi, macOS'ta IOSSIOSPEED yok (eski kaynaklar, güncel kod okunmadı) [D-]; USB CDC'de baud önemsiz [İ] |
| libserialport (sigrok) | C / LGPL-3.0+ (kaynak ikincil) | minimal, çapraz platform | son sürüm 0.1.2 (Eylül 2022) bu çalışmada; yeni sürüm var mı doğrulanmadı [D-] |
| Qt SerialPort | C++ / Qt lisansı | Qt kullanılırsa hazır | Qt seçilmedikçe gereksiz |
| libusb 1.0.29 | C / LGPL (lisans bu çalışmada okunmadı [D-]) | vendor bulk, eşzamansız aktarım, WinUSB arka ucu | Windows'ta WinUSB sürücüsü gerekir (aşağıda) |
| Düz POSIX termios + Win32 `CreateFile`/overlapped | C | tam denetim, bağımlılık yok | iki kod yolu |

Kaynaklar: [libserialport sürüm özeti](https://www.freshports.org/devel/libserialport/), [libusb 1.0.29 duyurusu](https://sourceforge.net/p/libusb/mailman/message/59190121/), [Asio özel baud tartışması](https://lists.boost.org/archives/list/boost-users@lists.boost.org/thread/FBOLQGDT566NSPZQ6BZ3C662LG7BCQNH).

**USB CDC tuzakları (anekdot düzeyi, ölçümle doğrulanmalı)**
- Windows: OSR forumunda `usbser.sys` ile yüksek hızda bütün bulk paketlerin (FS'de 64 bayt) sessizce kaybolduğu bildirildi; çözüm olarak çerçeve başına tek paket ve ≤63 bayt önerildi; doğrulanmış sürücü hatası değil. [OSR başlığı](https://community.osr.com/t/about-supposed-bug-in-usbser-sys-with-cdc-devices-on-some-microcontrollers/59965)
- ST topluluğu: macOS CDC'ye duyarlı; bir ST moderatörü, kontrol isteği işleyicisindeki bir firmware değişikliğinin macOS'ta davranış değiştirdiğini yazdı. [ST: macOS'ta H7 CDC](https://community.st.com/stm32-mcus-embedded-software-32/stm32h7-usb-cdc-read-fails-on-mac-mini-m4-macos-15-5-152452)
- Nordic nRF52840 (STM32 değil) macOS'ta 4 ms'lik patlamalar bildirdi; 1 kHz'de 128 baytlık yazma ve farklı aktarım yöntemiyle giderildi. [Nordic forumu](https://devzone.nordicsemi.com/f/nordic-q-a/33408/burstiness-of-nrf-52840-dev-kit-usb-cdc-adm-code-on-mac-os)
- ST toplulukta performans için CDC yerine bulk aktarım ve büyük tamponlar öneriliyor. [ST: yüksek hızda paket kaybı](https://community.st.com/stm32-mcus-embedded-software-32/usb-cdc-packet-loss-at-high-data-rates-160879)
- **Hiçbir kaynakta işletim sistemi başına ölçülmüş gecikme/jitter sayısı bulunamadı.** Bu yüzden ilk iş bir "ping-pong" deneyi: PC → kart → PC gidiş-dönüş gecikme histogramı (1 kHz ve 200 Hz, 64 B ve 256 B yük, 30 dakika, her iki OS).

**Vendor bulk yedeği**: Windows'ta MS OS 2.0 tanımlayıcıları WinUSB'yi INF olmadan yükletir (Windows 8.1+); bunun için BOS gerekli, BOS ise USB 2.1 (bcdUSB 0x0210) ister; bileşik cihazda `usbccgp.sys` WinUSB'ye engel olabilir. macOS/Linux'ta genel sürücü libusb ile uç noktalara doğrudan erişir. [MS: USB cihazı kurma](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/building-usb-devices-for-windows), [libusb listesi](https://sourceforge.net/p/libusb/mailman/message/37874132), [Cynthion WCID](https://cynthion.readthedocs.io/en/latest/tutorials/gateware_usb_device_02.html)

**Protokol (C ile)**: 03/06 raporlarının `postcard + COBS` tercihi Rust'a özgüydü. C'de [İ]: `proto/` altında tek C başlığı, sabit düzenli, açık endian'lı yapılar (`_Static_assert` ile boyut/ofset), COBS çerçeveleme + CRC (Codex: postcard'ın COBS'u CRC eklemez, ayrı gerekir), sürüm alanı ve `seq`. Alternatif: MAVLink özel dialect'i (aşağıda). Hangisi seçilirse seçilsin, aynı başlık C çekirdek ve C++ köprü tarafından derlenir.

**MAVLink C/C++**
- `c_library_v2`: başlık-yalnız, "Official reference C / C++ library", MAVLink 2; özel dialect için `mavgen` ile kendi başlıklarınızı üretirsiniz; tek dialect başlığı dahil edilir; kanal sayısı varsayılan 16 (Windows/Linux/macOS), iş parçacığı güvenliği sayfada anlatılmıyor. [mavlink.io C kütüphanesi](https://mavlink.io/en/mavgen_c/), [c_library_v2](https://github.com/mavlink/c_library_v2). Lisans bu çalışmada okunamadı (sayfada görünmedi) [D-].
- PX4: özel mesaj eklerseniz QGC/MAVSDK'yı da aynı tanımla derlemeden kullanamazsınız. [PX4 mesaj ekleme](https://docs.px4.io/v1.16/en/mavlink/adding_messages.html)
- MAVSDK C++ yolu bu çalışmada incelenmedi [D-]. Sonar/DVL için MAVLink eksiklerinin sınırları 06'da Codex tarafından düzeltilmiş durumda (özel protokol zorunlu değil, `ENCAPSULATED_DATA`/`TUNNEL` vb. mümkün).

### 2.6 Laboratuvar uygulaması arayüzü

Ölçütler: harita, gömülü 3B görünüm, yüksek frekanslı grafik, sonar görüntüsü, senaryo editörü, iki platformda paketleme, lisans.

**Lisans olguları (Qt)**: Qt çift lisanslıdır (ticari veya açık kaynak). Açık kaynakta bazı modüller LGPL v3 değil GPL'dir: aralarında **Qt Quick 3D, Qt Graphs, Qt Quick Timeline, Qt GRPC, Qt Canvas Painter** var. [Qt lisans belgesi](https://doc.qt.io/qt-6/licensing.html). Qt Quick 3D sayfası "ticari ya da GPLv3" diyor, Qt Graphs de. [Qt Quick 3D](https://doc.qt.io/qt-6/qtquick3d-index.html), [Qt Graphs](https://doc.qt.io/qt-6/qtgraphs-index.html). Qt Location LGPL v3 / GPL v2 / ticari; ama Qt 6.12 modül tablosunda **"Technology Preview"** (kaynak/ikili uyumluluk vaadi yok); Qt Charts, Qt Data Visualization, Qt 3D "Deprecated". [Qt modül listesi](https://doc.qt.io/qt-6/qtmodules.html), [Qt Location](https://doc.qt.io/qt-6/qtlocation-index.html). LGPL v3 için: paylaşılan kütüphane mekanizması (4d1) veya yeniden bağlanabilirlik (4d0), lisans metni ve uyarı gereklidir. [Qt LGPL](https://doc.qt.io/qt-6/lgpl.html)

Harita: Qt 6'da yerleşik sağlayıcı kapsamı daraldı, OSM kaldı; MapLibre eklentisi ayrı projede (`maplibre-native-qt`, Qt ≥ 6.5). [Qt Location değişiklikleri](https://doc.qt.io/qt-6.8/qtlocation-changes-qt6.html), [maplibre-native-qt](https://github.com/maplibre/maplibre-native-qt). MapLibre Native çekirdeği C++, ancak resmî SDK'lar Android/iOS; Linux/Windows/macOS "derlenebilir" ve C++ API'si "unstable". [maplibre-native](https://github.com/maplibre/maplibre-native)

**Dear ImGui/ImPlot**: ImGui MIT, Metal/DX11/DX12/Vulkan/OpenGL/WebGPU arka uçları ve GLFW/SDL3/Win32/OSX platform katmanı; docking+multi-viewport `docking` dalında; RTL/çift yönlü metin ve erişilebilirlik desteklenmiyor; resmî yan önerisi etiketli sürüm yerine master/docking izlemek. [imgui](https://github.com/ocornut/imgui). ImPlot MIT; "onlarca-yüz binlerce nokta sorunsuz, milyonlar akıcı değil", ısı haritası var, GPU hızlandırma deneysel dalda; sürüm listesi boş görünüyor, bakım durumu sayfadan doğrulanamadı; ImPlot3D deneysel. [implot](https://github.com/epezent/implot). 1.92.x/1.93 ImGui sürümleri tarih olarak görüldü. Harita için hazır yapı yok (Rust'taki Walkers C++ karşılığı doğrulanmadı) [D-].

**Web ön yüz**: three.js (`WebGLRenderer` r163+ WebGL2 gerektirir), MapLibre GL JS 6 (WebGL1 kaldırıldı) — Codex doğrulaması: [three.js](https://threejs.org/docs/pages/WebGLRenderer.html), [MapLibre v6 geçiş](https://maplibre.org/maplibre-gl-js/docs/guides/v5-to-v6-migration-guide/). uPlot: MIT, ~50 KB, kendi benchmarkına göre 166 650 noktayı 25 ms'de, 3 600 nokta 60 fps akışta %10 CPU; sınır ~100k görünür nokta (üretici iddiası, benchmark 2023). [uPlot](https://github.com/leeoniya/uPlot)

**Karşılaştırma** (puanlar benim yargım [İ], kanıt değil; 1 zayıf, 5 çok iyi)

| Ölçüt | Qt 6 Widgets/QML | Dear ImGui + ImPlot | C++ sunucu + web ön yüz | wxWidgets |
|---|---|---|---|---|
| Harita | 3 (Location TP; MapLibre Qt eklentisi) | 2 (kendi çiziminiz) | 5 (MapLibre GL JS) | 1 |
| Gömülü 3B | 4 (Quick 3D; **GPLv3 veya ticari**) | 3 (kendi render'ınız) | 4 (three.js, WebGL2) | 1 |
| Yüksek frekanslı grafik | 3 (Qt Graphs GPL; Charts deprecated) | 5 (ImPlot, kısıt: ~yüzbinler) | 4 (uPlot) | 1 |
| Sonar görüntüsü | 4 (doku/QQuickImage) | 5 (doku yükle, hızlı) | 4 (canvas/WebGL, ikili akış) | 2 |
| Senaryo editörü | 4 (QML/Widgets, ağaç/tablo) | 2 (metin/biçim, i18n sınırlı) | 5 (şema formları, kod editörü) | 3 |
| Kurulum/paketleme | 3 (macdeployqt/windeployqt, notarization) | 5 (tek exe) | 4 (tarayıcı ya da tek sunucu exe) | 3 |
| Lisans yükü | Orta-yüksek | Düşük (MIT) | Düşük | Düşük |
| Ekip dili | C++ | C++ | C++ + TS/JS | C++ |

wxWidgets bu çalışmada araştırılmadı; satır yalnızca kıyas için ve [D-].

**Öneri mantığı**: Ekibin dil kararı "çekirdek C, PC C++" idi; web ön yüz ek olarak TS/JS getirir. Buna karşılık: harita ve senaryo editörü (iki en ağır parça) web ekosisteminde olgun, Qt'de ise Quick 3D/Graphs lisansı ve Location durumu sorun, ImGui'de harita ve editör sıfırdan. Yarışma hazırlığında laboratuvar uygulamasının dağıtımı yalnızca ekip içindir; bu, GPLv3 kaygısını azaltabilir ama "yarışma/sponsor ile paylaşım" varsa dağıtım sayılır — hukuki görüş değil, kontrol edilmeli.

### 2.7 Süreç mimarisi ve iletişim

Önerilen düzen [İ]:

```
 [sim-server (C++)]  ------ WebSocket (JSON kontrol, ikili akış) ------> [web UI / Foxglove / diğer istemciler]
   |  sim thread (deterministik dünya)
   |  hil thread (RT öncelik, kart G/Ç)  <==USB==> [STM32 kartı]
   |  recorder thread (MCAP)
   |  (isteğe bağlı) gpu-sensor süreci <== paylaşımlı bellek halka tamponu
```

| Kanal | Aday | Gerekçe/uyarı |
|---|---|---|
| Sunucu ↔ UI | WebSocket (Boost.Beast, IXWebSocket veya uWebSockets) | tarayıcı yerel; ikili çerçeveler |
| Sunucu ↔ UI kontrol | JSON-RPC benzeri komutlar | basit, hata ayıklaması kolay |
| Sunucu ↔ GPU sensör süreci | paylaşımlı bellek + sinyal | Boost.Interprocess taşınabilir kip dosya sistemi ömürlü; macOS POSIX paylaşımlı bellek kullanır; Windows'a özgü tür kernel-ömürlü ama taşınabilir türle birlikte çalışmaz; çökme sonrası temizlik kendi işiniz [K, ikincil: [Boost belgesi](https://www.boost.org/doc/libs/1_56_0/doc/html/interprocess/acknowledgements_notes.html)] |
| gRPC | gerekli görülmedi | tarayıcıdan doğrudan yok (grpc-web proxy), ağır derleme [İ, gRPC için kaynak araştırılmadı, D-] |

WebSocket kütüphaneleri (bu çalışmada yalnızca belge düzeyinde): IXWebSocket (Boost'suz; macOS/Windows test edilmiş, Windows CI'da TLS kapalı; MinGW yok), uWebSockets (C++11, zlib/libpng lisansı, kendi benchmarkına göre hızlı, Windows için VS), Boost.Beast (Boost gerektirir, esnek, kurulum yükü), libwebsockets (C). [IXWebSocket](https://github.com/thomas-bernhard/IXWebSocket/blob/master/README.md), [karşılaştırma tabloları (eski)](https://wiki.krisyotam.com/content/wikipedia_en_all_nopic_2026-03/Comparison_of_WebSocket_implementations). Önerim [İ]: Boost zaten Asio için gelecekse Beast; hızlı prototip için IXWebSocket. Kapasite (1 kHz telemetri ≈ UI'ya 30-60 Hz özetlenmiş akış, ham veri MCAP'e) bir hedeftir, ölçülmedi (Codex da "yerel soket 1 kHz" iddiasını doğrulanamadı olarak işaretledi).

**Kural [İ]**: HIL döngüsü `io_context` içinde olmamalı; ayrı iş parçacığında bloklayıcı veya overlapped okuma, UI/ağ kuyruğundan bağımsız. UI yavaşlarsa sunucu kare atar, HIL durmaz.

### 2.8 Kayıt ve tekrar oynatma

- **MCAP C++**: "başlık-yalnız" (tek `.cpp`'de `MCAP_IMPLEMENTATION`), lz4 ve zstd bağımlılığı (`MCAP_COMPRESSION_NO_LZ4/ZSTD` ile kapatılabilir), vcpkg'de üçüncü taraf port, Conan için `conanfile.py` var; yazıcı iş parçacığı güvenliği belgelenmemiş. [mcap/cpp](https://github.com/foxglove/mcap/tree/main/cpp), [MCAP C++ rehberi](https://mcap.dev/guides/cpp/protobuf).
- **Foxglove SDK C++**: MIT; C++17+; Rust çekirdeğe C kütüphanesi üzerinden bağlanır, önceden derlenmiş kütüphaneler yayımlanır; MCAP dosyasına yazma, yerel WebSocket sunucusu, uzak erişim ağ geçidi (ayrıntı yok); v0.7.0 notları "özellik tamam değil", v0.8.0 Foxglove şemalarını destekliyor, ancak Foxglove şemaları için tür-güvenli kanallar C++'ta yoktu; docs v0.9.0. [SDK tanıtımı](https://docs.foxglove.dev/docs/sdk/introduction), [SDK deposu](https://github.com/foxglove/foxglove-sdk), [C++ arama sonucu özeti](https://foxglove.github.io/foxglove-sdk/cpp/index.html) (doğrudan açılamadı, 404). Hesap/lisans gereksinimi sayfada belirtilmiyor [D-].
- **Öneri [İ]**: kayıt için `mcap` C++ kütüphanesi doğrudan (az bağımlılık, şema serbestliği); Foxglove canlı bağlantısı isteniyorsa SDK'yı yalnızca canlı yayın için ekleyin. Şema/koordinat çerçevesi/zaman damgası sözleşmesi (Codex): MCAP geçerli dosya yapar, görüntü/poz/sonar olarak otomatik çizilebilir yapmaz.
- Tekrar oynatma: kayıtlı giriş olayları + senaryo + tohum; "gözlem tekrarı" (kayıtlı durum) ile "yeniden koşturma" (aynı girişle yeniden simülasyon, determinizm gerekir) ayrılmalı.

### 2.9 Yer istasyonu (QGroundControl)

- Lisans: Apache-2.0 veya GPLv3 seçimi; Apache derlemesi ticari Qt ister, GPLv3 açık Qt'yi kullanabilir. [QGC lisansı](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/contribute/licences.html)
- Derleme: Qt **6.12.0** tek desteklenen sürüm (diğerleri "kararlılık ve güvenlik" riski); CMake ≥ 3.28, Ninja, Python 3.10+; Windows, macOS, Linux, Android, iOS. [QGC başlangıç](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/getting_started/index.html)
- Özel derleme: `QGCCorePlugin`, `FirmwarePlugin`, `AutoPilotPlugin` alt sınıflarıyla; ArduPilot/PX4 fabrikaları `CustomOverrides.cmake` ile kapatılabilir. [Özel derleme](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/custom_build/custom_build.html)
- Genel (generic) MAVLink aracı: `GenericAutoPilotPlugin` yedek; `GenericFirmwarePlugin` bütün görev komutlarını destekli sayar, `isCapable` varsayılanı `false`; bir PX4 forumunda özel otopilotun "Guided mode not supported" hatası bildirildi. Bunlar eski/yansı kaynaklardır; güncel master okunmadı [D-]. [GenericAutoPilotPlugin](https://api.qgroundcontrol.com/master/classGenericAutoPilotPlugin.html), [forum](https://discuss.px4.io/t/confused-with-guided-mode-in-qgroundcontrol-using-mavlink-v2-protocol/8091)
- **İş bölümü [İ]**: QGC = standart MAVLink (HEARTBEAT, ATTITUDE, GLOBAL_POSITION_INT, görev yükleme, parametre) ile "gerçek yer istasyonu" provası; kartın MAVLink'i sınırlı alt küme konuşursa yeter. Laboratuvar uygulaması = sim kontrolü, senaryo, hata enjeksiyonu, sonar, kayıt/tekrar, harita üzerinde görev planlama. İki uygulamanın aynı anda tüketebilmesi için sunucu MAVLink'i UDP/TCP'de de sunabilir. QGC'yi 3. ayda prototip testiyle doğrulayın (Codex'in aynı önerisi).

### 2.10 Test ve altyapı

| Konu | Bulgu / öneri |
|---|---|
| Test çerçevesi | GoogleTest (mock, geniş araç desteği; 38.7k yıldız, 2026-06 karşılaştırması), Catch2 (v3; BDD/benchmark), doctest (en hızlı derleme; v2.5.3 2026-07 bildirildi). Sanitizer ve CTest entegrasyonu için sonuç bulunamadı [D-]; öneri [İ]: GoogleTest + `gtest_discover_tests`, C çekirdek için aynı C++ test düzeneği. Kaynak: [pistack karşılaştırması](https://www.pistack.xyz/posts/2026-06-21-cpp-unit-testing-frameworks-catch2-doctest-gtest-boost-test/) (düşük güvenilirlik). |
| Sanitizer | MSVC/clang-cl'de AddressSanitizer var; TSan ve MSan MSVC'de yok; UBSan MSVC'de belirsiz/yok sayın. [Conan sanitizer tablosu](https://docs.conan.io/2/security/sanitizers), [VS 2022 17.14 ASan](https://devblogs.microsoft.com/cppblog/address-sanitizer-updates-for-visual-studio-2022-17-14/). VS'de CMake presets `addressSanitizerEnabled`'i desteklemez, bayraklar elle verilir (`/fsanitize=address`). [MS CMake Presets](https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs). Apple clang'de UBSan/TSan/ASan kullanılabilir (bilgiden [D-]); öneri: TSan ve UBSan işlerini macOS veya Linux'ta çalıştırın. |
| Statik analiz | clang-tidy, cppcheck, MSVC `/analyze`, CodeQL — yalnızca aday liste; bu çalışmada araştırılmadı [D-]. Çekirdek C için ayrıca `-Wconversion -Wdouble-promotion` ve MISRA benzeri alt kural seti kararı ekibin. |
| GitHub Actions | `macos-latest` = macOS 26 arm64, `windows-latest` = Server 2025 x64 (Codex doğrulaması); açık sürüm etiketi tercih. vcpkg ikili önbelleği NuGet+GitHub Packages (`VCPKG_BINARY_SOURCES`, paket yazma yetkili token). [vcpkg ikili önbellek](https://learn.microsoft.com/en-gb/vcpkg/reference/binarycaching) |
| Paketleme (Qt yolu) | macOS: `macdeployqt` + `codesign` (hardened runtime) + `notarytool` + stapling; Qt-posta listesi, imzalama/notarization hataları için `-sign-for-notarization` kullanımı bildiriyor. [Qt interest](https://lists.qt-project.org/pipermail/interest/2025-October/040325.html), [Qt GH Actions rehberi (ikincil)](https://smithery.ai/skills/svetzal/qt-release). Windows tarafı (windeployqt, CPack/WiX/NSIS, imzalama) araştırılmadı [D-]. |
| Paketleme (web yolu) | tek sunucu çalıştırılabilir + statik varlıklar zip; kullanıcı mevcut tarayıcıyı açar. Electron/ayrı kabuk gerekmez; macOS'ta ekip içi dağıtımda imzasız derleme yerelde derlenip çalışır (dağıtım için notarization gerekir, kapsam dışı) [İ] |
| Performans izleme | Tracy gibi profilleyici adayı; bu çalışmada araştırılmadı [D-] |

---

## 3. Mimari için öneriler

### 3.1 Önerilen yığın (tek tablo)

| Katman | Seçim | Gerekçe | Yedek / karar koşulu |
|---|---|---|---|
| Diller/standart | Çekirdek C11 (`-std=c11`, uzantı yok); PC C++20 | ISO kipi `-ffp-contract=off`; üç derleyicide ortak alt küme | C++23 yalnızca CI'da üç derleyicide derlenen parçalar |
| Derleyiciler | MSVC (VS 2022+), Apple clang; clang-cl isteğe bağlı; CI'da +1 Linux | kullanıcı gereksinimi; cppreference uyumu | `jthread`, `chrono` tz, modüller kullanılmaz |
| Derleme | CMake + CMakePresets (tek kaynak) + Ninja | IDE, CI, komut satırı aynı preset | |
| Bağımlılıklar | vcpkg manifest (baseline + overrides), küçük başlık-yalnız için CPM (hash'li) | sürüm kilidi; CI önbelleği NuGet | Conan 2, lockfile gereksinimi kanıtlanırsa |
| Repo | tek repo, `core/` (C) hem `firmware/` hem `sim/` tarafından tüketilir | ortak kod, tek test | iki workspace yerine tek ağaç + iki araç zinciri preset'i |
| Matematik | çekirdekte kendi `float` yardımcıları; PC fizikte Eigen 3.4/5.x | çekirdek C; FPU yalnızca SP | determinizm işinde vektörleşme kapat |
| FP politikası | `-ffp-contract=off`, fast-math yok, kendi RNG + normal dağılım, tek `ida_math` katmanı | 2.3'teki kaynaklı farklar | libm farkı ölçülürse kendi fonksiyonlar |
| Sim çekirdeği | başsız, `int64 ns` zaman, dünya başına tek iş parçacığı, statik sensör eklentileri, ortak `ISensorModel` | deterministik, 3 zaman kipi sürücüyle | dinamik eklenti yüklemesi sonraya |
| HIL taşıma | önce USB CDC ile ölç; yedek WinUSB/libusb vendor bulk | OS gecikme verisi yok | Kart HS Type-C kullanılabilirse bulk |
| Protokol | `proto/` ortak C başlığı + COBS + CRC (veya MAVLink özel dialect) | tek kaynak, C ve C++ aynı başlık | MAVLink, QGC/MAVSDK doğrudan konuşacaksa |
| Sunucu ağ | Boost.Beast (veya IXWebSocket) ayrı iş parçacığı | tarayıcı yerel | uWebSockets ölçüm gerektirirse |
| Laboratuvar UI | web ön yüz: three.js (WebGL2), MapLibre GL JS, uPlot, şema tabanlı senaryo editörü | harita ve editör ekosistemi; lisans sorunu yok | Qt Widgets/QML (LGPL parçalar) ya da ImGui |
| Geliştirici panosu | Dear ImGui + ImPlot, süreç içi, isteğe bağlı derleme bayrağı | MIT, hızlı; hata ayıklama | |
| Kayıt | `mcap` C++ (lz4/zstd), kendi şemalarımız | doğrudan kontrol | Foxglove SDK canlı akış için isteğe bağlı |
| Yer istasyonu | QGC (GPLv3 derlemesi, Qt 6.12.0) provası | lisans ve ticari Qt çıkarımı | özel eklenti sonraya |
| GPU sensör arayüzü | `ISensorModel` + zaman damgalı ikili örnek; ayrı süreçse paylaşımlı bellek halka | grafik araştırmasından bağımsız | |
| Test | GoogleTest/CTest, altın iz regresyonu, ASan (MSVC) + TSan/UBSan (macOS/Linux) | 2.10 | doctest hız gerekirse |
| CI | GitHub Actions: macos-latest/windows-latest (açık sürümlü) + Linux; NuGet önbelleği | Codex doğrulaması | |

### 3.2 Gerekçeli kararlar ve sıralama

1. **Önce ölçüm kapıları (hafta 1-2)**: (a) USB gidiş-dönüş gecikme histogramı her iki OS'ta; (b) aynı altın izi macOS-arm64 ve Windows-x64'te koşturup ıraksama raporu; (c) Apple clang'de kullanılacak std özelliklerini derleme testi; (d) QGC genel araç prototipi. Dört kapının ikisi başarısızsa yığın yeniden gözden geçirilir.
2. **UI'yı iki aşamalı kurun**: sunucu + ImGui panosu ilk günden (hata ayıklama); web UI harita/editör geldiğinde. Sunucu protokolü kararlı olmalı ki UI değişimi (web → Qt) çekirdeği etkilemesin.
3. **Çekirdek–PC sözleşmesi**: `core/` hiçbir şekilde C++ başlığına bağlanmaz; `extern "C"` sarmalayıcıyla PC çağırır. Kartta ve PC'de aynı `float`.

---

## 4. Riskler ve açık sorular

| # | Risk / soru | Etki | Azaltma |
|---|---|---|---|
| 1 | Apple clang'de `jthread`/stop_token ve diğer C++20 kütüphane parçaları sürüme göre değişebilir | derleme kırığı | ortak alt küme + CI feature-test makroları |
| 2 | macOS ↔ Windows bit-bit eşitlik hedeflenirse libm, FMA, vektörleşme farkı | senaryo tekrarları platform bağımlı | toleranslı eşdeğerlik hedefi; platform içi bit-bit; ölçüm |
| 3 | SIL ↔ kart farkı (SP FPU, farklı libm, GCC `fast` daralma) | "SIL'de çalışıyor kartta çalışmıyor" | aynı FP politikası, `float` tipleri, ortak testler; kartta çıkarılmış izle karşılaştırma |
| 4 | USB CDC sürücü davranışı (usbser.sys paket kaybı raporu, macOS duyarlılığı) | HIL zaman bütçesi | ölçüm, bulk yedeği; ST HS Type-C doğrulaması |
| 5 | Kart üretimden kalktı (Codex); HS USB portu doğrulanmadı | tekrar üretilebilirlik | yedek kart, soyut BSP |
| 6 | Qt Quick 3D/Graphs GPL-only, Location Technology Preview | Qt yolunun maliyeti | web veya ImGui yolu; Qt seçilirse GPLv3'ü kabul edin veya ticari lisans |
| 7 | Web yolu TS/JS ekler; ekip dil kararıyla çelişebilir | beceri | arayüz protokolü dar tutulur; ImGui yedeği |
| 8 | WebGL2 tabanı; macOS WKWebView değil sistem tarayıcısı kullanılırsa sorun yok, Electron/Tauri gömülürse Codex'in WebGPU uyarıları | 3B görünümde regresyon | tarayıcıyı birincil tut, gömülü kabuk sonra |
| 9 | Foxglove SDK C++ olgunluğu (Rust sarmalı, şema kanalları) | kayıt boru hattı | MCAP kütüphanesini doğrudan kullan |
| 10 | QGC `GenericFirmwarePlugin` yetenek bayrakları, Qt 6.12.0 kilidi | QGC provası | erken prototip; QGC'yi zorunlu yapma |
| 11 | vcpkg ve Qt kaynaktan derleme süreleri | CI süresi | ikili önbellek (NuGet), Qt kullanılmazsa sorun yok |
| 12 | Windows zamanlayıcı çözünürlüğü ve macOS çekirdek yerleşimi | HIL jitter | ölçüm, öncelik ayarları, süreç gizliyken test |
| 13 | Açık: sonar/kamera GPU süreci süreç içi mi ayrı mı? | arayüz | grafik araştırması sonrası; `ISensorModel` her ikisine uyar |
| 14 | Açık: kart protokolü MAVLink mi özel çerçeve mi? | QGC uyumu | HIL veri yolu ayrı, MAVLink yalnızca "telemetri/komut" yolu olabilir |
| 15 | Açık: lisans (Qt, QGC, LGPL kütüphaneler: libserialport, libusb) | dağıtım | hukuki doğrulama; dinamik bağlama |

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **Qt Quick 3D ve Qt Graphs açık kaynakta yalnızca GPLv3'tür (LGPL yok); Qt Location 6.12'de "Technology Preview"dir.** Kaynak: [Qt lisans](https://doc.qt.io/qt-6/licensing.html), [Qt Quick 3D](https://doc.qt.io/qt-6/qtquick3d-index.html), [Qt Graphs](https://doc.qt.io/qt-6/qtgraphs-index.html), [Qt modüller](https://doc.qt.io/qt-6/qtmodules.html). Not: Qt Location sayfasının kendisi uyarı vermiyor, tablo veriyor.
2. **MSVC 2022 `/fp:precise` altında varsayılan FMA daralması üretmez; clang `-ffp-contract` varsayılanı `on`; GCC GNU kipinde `fast`, ISO C kipinde `off`.** Kaynak: [MS /fp](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior), [Clang kılavuzu](https://clang.llvm.org/docs/UsersManual.html), [GCC](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html). Doğrulanmadı: Apple clang ve clang-cl varsayılanları.
3. **`std::normal_distribution` ve `<random>` dağılımları standart kitaplıklar arasında taşınabilir sonuç vermez; motorlar verir.** Kaynak: [Quuxplusone](https://quuxplusone.github.io/blog/2019/10/22/psa-stateful-distributions) (ikincil, StackOverflow tartışmaları).
4. **vcpkg `x-gha` ikili önbellek sağlayıcısı kaldırıldı; önerilen yol NuGet + GitHub Packages.** Kaynak: [vcpkg binary caching](https://learn.microsoft.com/en-gb/vcpkg/reference/binarycaching), [Haziran 2025 notu](https://devblogs.microsoft.com/cppblog/whats-new-in-vcpkg-june-2025/).
5. **Apple clang'de `std::jthread`/`stop_token` cppreference'ta listelenmiyor; libc++ bunları Eylül 2024'te deneysel bayraktan çıkardı; Xcode'un hangi sürümünde olduğu bilinmiyor.** Kaynak: [cppreference](https://en.cppreference.com/w/cpp/compiler_support/20), [libc++ commit](https://lists.llvm.org/pipermail/libcxx-commits/2024-September/091245.html).
6. **STM32U5A9J-DK'nin USB Type-C (CN4) portu High-Speed device (sink-only) arayüzüdür ve kullanıcı yazılımıyla CDC/bulk için erişilebilir.** Kaynak: [UM2967](https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf) (yalnızca arama özetinden; kılavuz açılmadı).
7. **Windows `usbser.sys` ile STM32 CDC'de bulk paket kaybı ve macOS CDC duyarlılığı tekrarlanabilir olgulardır.** Kaynak: [OSR](https://community.osr.com/t/about-supposed-bug-in-usbser-sys-with-cdc-devices-on-some-microcontrollers/59965), [ST H7 macOS](https://community.st.com/stm32-mcus-embedded-software-32/stm32h7-usb-cdc-read-fails-on-mac-mini-m4-macos-15-5-152452). Anekdot; ölçüm gerekir.
8. **Foxglove SDK C++ Rust çekirdeğini sarar, MIT lisanslıdır, MCAP yazar ve yerel WebSocket sunar; Foxglove şemaları için tür-güvenli kanallar C++'ta henüz yoktu (v0.8/0.9 döneminde).** Kaynak: [SDK tanıtımı](https://docs.foxglove.dev/docs/sdk/introduction), [foxglove-sdk](https://github.com/foxglove/foxglove-sdk), [v0.8.0 notu](https://docs.foxglove.dev/changelog/sdk/v0.8.0). Güncel sürümde değişmiş olabilir.
9. **QGroundControl yalnızca Qt 6.12.0'ı destekler; Apache-2.0 derlemesi ticari Qt, GPLv3 derlemesi açık Qt kullanır.** Kaynak: [QGC başlangıç](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/getting_started/index.html), [QGC lisans](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/contribute/licences.html).
10. **Cortex-M33 yalnızca tek duyarlıklı donanım FPU'su taşır (bu projedeki STM32U5A9 dâhil).** Kaynak: [Arm blogu](https://community.arm.com/groups/processors/blog/2016/10/25/five-key-features-of-the-arm-cortex-m33-processor), [ST PM0264](https://www.st.com/resource/en/programming_manual/pm0264-stm32cortexm33-mpu-programming-manual-stmicroelectronics.pdf); U5A9'a özgü FPU yapılandırması doğrulanmadı.

---

## 6. Kaynaklar

Birincil/resmî
- cppreference C++20 desteği: https://en.cppreference.com/w/cpp/compiler_support/20
- cppreference C++23 desteği: https://en.cppreference.com/w/cpp/compiler_support/23
- libc++ jthread commit: https://lists.llvm.org/pipermail/libcxx-commits/2024-September/091245.html
- Clang kullanıcı kılavuzu: https://clang.llvm.org/docs/UsersManual.html
- MSVC /fp: https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior
- GCC Optimize Options: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- MS CMake Presets: https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs
- vcpkg sürümleme: https://learn.microsoft.com/en-us/vcpkg/users/versioning
- vcpkg ikili önbellek: https://learn.microsoft.com/en-gb/vcpkg/reference/binarycaching
- vcpkg Haziran 2025: https://devblogs.microsoft.com/cppblog/whats-new-in-vcpkg-june-2025/
- vcpkg releases: https://github.com/microsoft/vcpkg/releases
- CPM.cmake: https://github.com/cpm-cmake/CPM.cmake
- Conan 2: https://docs.conan.io/2/introduction.html , sanitizer: https://docs.conan.io/2/security/sanitizers
- Eigen 5.0.0/5.0.1: https://newreleases.io/project/gitlab/libeigen/eigen/release/5.0.0 , https://newreleases.io/project/gitlab/libeigen/eigen/release/5.0.1 , https://gitlab.com/libeigen/eigen/-/issues/2968
- Arm Cortex-M33: https://community.arm.com/groups/processors/blog/2016/10/25/five-key-features-of-the-arm-cortex-m33-processor
- ST PM0264: https://www.st.com/resource/en/programming_manual/pm0264-stm32cortexm33-mpu-programming-manual-stmicroelectronics.pdf
- ST UM2967 (kart): https://www.st.com/resource/en/user_manual/um2967-discovery-kit-with-stm32u5a9nj-mcu-stmicroelectronics.pdf
- MS timeBeginPeriod: https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
- MS USB cihazı kurma (WinUSB/MS OS 2.0): https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/building-usb-devices-for-windows
- libusb 1.0.29: https://sourceforge.net/p/libusb/mailman/message/59190121/
- MAVLink C kütüphanesi: https://mavlink.io/en/mavgen_c/ , https://github.com/mavlink/c_library_v2
- PX4 mesaj ekleme: https://docs.px4.io/v1.16/en/mavlink/adding_messages.html
- Qt lisans: https://doc.qt.io/qt-6/licensing.html , LGPL: https://doc.qt.io/qt-6/lgpl.html , modüller: https://doc.qt.io/qt-6/qtmodules.html
- Qt Quick 3D: https://doc.qt.io/qt-6/qtquick3d-index.html , Qt Graphs: https://doc.qt.io/qt-6/qtgraphs-index.html , Qt Location: https://doc.qt.io/qt-6/qtlocation-index.html , değişiklikler: https://doc.qt.io/qt-6.8/qtlocation-changes-qt6.html
- maplibre-native: https://github.com/maplibre/maplibre-native , Qt: https://github.com/maplibre/maplibre-native-qt
- Dear ImGui: https://github.com/ocornut/imgui , ImPlot: https://github.com/epezent/implot
- uPlot: https://github.com/leeoniya/uPlot
- three.js: https://threejs.org/docs/pages/WebGLRenderer.html , MapLibre v6: https://maplibre.org/maplibre-gl-js/docs/guides/v5-to-v6-migration-guide/
- MCAP C++: https://github.com/foxglove/mcap/tree/main/cpp , https://mcap.dev/guides/cpp/protobuf
- Foxglove SDK: https://docs.foxglove.dev/docs/sdk/introduction , https://github.com/foxglove/foxglove-sdk , https://docs.foxglove.dev/changelog/sdk/v0.8.0
- QGC: https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/contribute/licences.html , https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/getting_started/index.html , https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/custom_build/custom_build.html , https://api.qgroundcontrol.com/master/classGenericAutoPilotPlugin.html
- GitHub runner images: https://github.com/actions/runner-images
- FMI 3.0: https://fmi-standard.org/docs/3.0 , fmi4c: https://github.com/Ecos-platform/fmi4c
- Boost.Interprocess notları: https://www.boost.org/doc/libs/1_56_0/doc/html/interprocess/acknowledgements_notes.html

İkincil / anekdot (düşük güven)
- OSR usbser.sys: https://community.osr.com/t/about-supposed-bug-in-usbser-sys-with-cdc-devices-on-some-microcontrollers/59965
- ST H7 macOS CDC: https://community.st.com/stm32-mcus-embedded-software-32/stm32h7-usb-cdc-read-fails-on-mac-mini-m4-macos-15-5-152452
- ST CDC paket kaybı: https://community.st.com/stm32-mcus-embedded-software-32/usb-cdc-packet-loss-at-high-data-rates-160879
- Nordic macOS patlamaları: https://devzone.nordicsemi.com/f/nordic-q-a/33408/burstiness-of-nrf-52840-dev-kit-usb-cdc-adm-code-on-mac-os
- Asio özel baud: https://lists.boost.org/archives/list/boost-users@lists.boost.org/thread/FBOLQGDT566NSPZQ6BZ3C662LG7BCQNH
- libserialport: https://www.freshports.org/devel/libserialport/
- IXWebSocket: https://github.com/thomas-bernhard/IXWebSocket/blob/master/README.md
- FP determinizmi: https://bugnet.io/blog/how-to-debug-floating-point-determinism-across-platforms , https://arxiv.org/pdf/1807.01971
- `<random>` dağılımları: https://quuxplusone.github.io/blog/2019/10/22/psa-stateful-distributions
- macOS QoS/çekirdek: https://eclecticlight.co/2025/05/09/what-is-quality-of-service-and-how-does-it-matter/ , https://developer.apple.com/forums/thread/726096
- Test çerçeveleri karşılaştırması: https://www.pistack.xyz/posts/2026-06-21-cpp-unit-testing-frameworks-catch2-doctest-gtest-boost-test/
- Qt macdeployqt/notarization: https://lists.qt-project.org/pipermail/interest/2025-October/040325.html
- Önceki projeden: docs/research/03-uygulama-yigini.md, docs/research/dogrulama/03-codex.md, docs/research/dogrulama/06-codex.md
