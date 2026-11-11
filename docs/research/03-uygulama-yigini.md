# 03 - Masaüstü Uygulama Yığını ve Proje Altyapısı

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Ayrıca dil kararı C/C++ olmadan önce, Rust varsayımıyla yazıldı; Rust'a özgü öneriler geçersizdir. Doğrulaması: [dogrulama/03-codex.md](dogrulama/03-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Araştırma tarihi: 2026-11-10. Kapsam: hibrit (yüzey + dalış) insansız deniz aracı için HIL/SIL laboratuvar uygulaması, macOS + Windows native.

Okuma notu: Her iddianın yanında kaynak URL'si vardır (kaynak numaraları Bölüm 6'dadır). "Doğrulanmadı" etiketi, bu oturumda birincil kaynaktan teyit edilemeyen veya yalnızca ikincil kaynağa/genel bilgiye dayanan ifadeleri gösterir. "Bilgi (kaynaksız)" etiketi, bu oturumda kaynak çekilmeden yazılan genel mühendislik bilgisidir ve güvenilir kabul edilmemelidir.

---

## 1. Özet (10 madde)

1. **Mimari: başsız (headless) simülasyon sunucusu + ince istemciler.** Simülasyon çekirdeği saf bir Rust kütüphanesi olsun; aynı kütüphane (a) bağımsız sunucu süreci, (b) UI içine gömülü tek süreçli mod, (c) CI'daki başsız test koşucusu olarak kullanılsın. HIL'de gerçek zamanlı köprü (kart ile seri/USB) sunucu tarafında yaşasın.
2. **UI için ana öneri: Tauri 2 + web ön yüzü (TypeScript, MapLibre GL JS, three.js).** Harita, görev planlama, senaryo editörü ve grafikler web ekosisteminde hazır ve olgun; Tauri macOS'ta WKWebView, Windows'ta WebView2 kullanır [S1]. Rust ile saf-native alternatif egui/eframe'dir; ancak harita/planlama tarafında olgun hazır bileşen eksikliği riski vardır (aşağıda).
3. **Yüksek frekanslı veri UI'ya ham gitmemeli.** Sunucu 30-60 Hz'e decimate edilmiş, ikili (binary) toplu paketler yollasın. Tauri'de akış için `Channel` önerilir; `Event` yalnızca JSON taşır ve sırasız işlenebilir [S2].
4. **IPC: dış arayüz WebSocket (binary) + HTTP kontrol düzlemi; dahili iş parçacıkları arası kanallar.** Foxglove WebSocket protokolü/SDK'sı (MIT, Rust crate var) [S12] kullanılırsa Foxglove/PlotJuggler gibi araçlar bedavaya takılabilir. zenoh/iceoryx2 yalnızca ölçülmüş bir ihtiyaç doğarsa (çok makine, GB/s sonar akışı) eklenmeli.
5. **Hazır araçlar "geliştirici gözlemleme" içindir, ürün çekirdeği değildir.** Foxglove 2.0 ile Studio'nun açık kaynak sürümü sonlandırıldı (1.x kaynağı duruyor, gelecek geliştirme kapalı) [S3]; Rerun MIT/Apache [S4]; PlotJuggler MPL-2.0 [S5]; QGroundControl Apache-2.0/GPLv3 çift lisans [S6]. Kendi yazılması gereken: sim kontrolü, senaryo editörü, hata enjeksiyonu paneli, sonar görüntüsü, yarışma-özel görev planlama.
6. **Kayıt: MCAP (ana format) + Parquet (analiz dışa aktarımı).** MCAP: zaman damgalı çok kanallı, rastgele erişim indeksli, chunk bazlı zstd/lz4 sıkıştırma, attachment ve metadata kayıtları [S7]; Rust crate `mcap` (0.25.0) [S8]; Foxglove, PlotJuggler ve Rerun MCAP okur [S3][S5][S4]. ULog PX4'e özgüdür; yalnızca firmware onu üretiyorsa anlamlı [S9].
7. **Senaryo formatı: TOML (+ JSON Schema, `schema_version`, tohum `seed`).** OpenSCENARIO XML/DSL karayolu araçları odaklıdır ve ASAM lisans koşulları vardır [S10]; SDF (Gazebo) dünya/model tarifi içindir ve Gazebo macOS/Windows'ta yalnızca "best-effort" desteklidir [S11]. Olay zaman çizelgesi için OpenSCENARIO'nun Story/Act/Event/Trigger kavramlarından ilham alınabilir, format olarak alınmamalı.
8. **Zaman: iki saat kavramı.** `SimTime` (tamsayı ns, sabit adım) ile `WallTime`. SIL'de hızlandırma/lockstep serbest; **HIL'de gerçek karttaki firmware duvar saatiyle koştuğu için 1x gerçek zaman zorunludur** (PX4 kaynakları lockstep'i yalnızca SITL/SIH-Gazebo için listeler [S13]). Tekrar üretilebilirlik için HIL girdi/çıktı akışı kaydedilip SIL'de yeniden oynatılır.
9. **Determinizm platformlar arası garanti değildir.** Rapier bile çapraz platform determinizm için `enhanced-determinism` özelliği ve nalgebra matematik fonksiyonları şartı koşar [S14]. Altın-iz (golden trace) testleri OS başına ayrı referans veya toleranslı karşılaştırma ile yapılmalı.
10. **Altyapı: tek repo, host workspace + ayrı firmware workspace'i, paylaşılan `no_std` crate'ler.** Cargo feature unification, `no_std` ve `std` crate'leri aynı derlemede karıştırınca sorun çıkarır; en güvenilir yöntem ayrı workspace/ayrı çağrılardır [S15]. CI: `macos-latest` (= macOS 26, arm64) ve `windows-latest` (= Windows Server 2025, x64) matrisi [S16]; test koşucusu `cargo-nextest` (macOS/Windows destekli) [S17]. STM32U5A9 için `embassy-stm32` 0.6.0'da `stm32u5a9nj` vb. feature'ları var [S18].

---

## 2. Bulgular

### 2.1 Masaüstü uygulama seçenekleri

Karşılaştırma tablosu (gereksinim: macOS + Windows native, 3B, harita, grafik, sonar görüntüsü, yüksek frekanslı veri):

| Seçenek | Mimari / 3B gömme | Yüksek frekanslı veri | Olgunluk / lisans | Rust uyumu | Başlıca risk |
|---|---|---|---|---|---|
| **Tauri 2** | Rust arka uç + sistem webview'i (Windows 10+: WebView2, macOS 10.15+: WKWebView) [S1]. 3B: webview içinde WebGL (three.js). Ayrıca wry, macOS/Windows'ta çocuk webview (`build_as_child`) destekler [S19]; bu, native wgpu yüzeyi ile webview'i yan yana koymanın teorik yoludur (karmaşık, doğrulanmadı) | `Channel` akış için önerilen mekanizma, bayt dilimi taşıyabilir; `Event` yalnızca JSON, sırasız olabilir; büyük binary için `tauri::ipc::Response` [S2] | "stable" rozeti, 112k+ yıldız; Apache-2.0/MIT [S1] | Birinci sınıf (arka uç Rust) | WebGL performansı OS webview'ine bağlı (WKWebView vs WebView2); bu oturumda ölçülmedi, prototipte test edilmeli |
| **egui / eframe** | Anlık mod (immediate mode) UI; macOS/Windows/Linux/Web; wgpu veya glow arka ucu [S20]. 3B: `Shape::Callback` ya da dokuya çizip `ui.image` [S20] | Her karede tam yerleşim, tipik 1-2 ms/kare CPU [S20]. `egui_plot` büyük veride yavaşlıyor diye topluluk raporları var (zayıf kanıt, ölçüm bulunamadı) [S21] | "Work in progress", sürümler arası kırıcı değişiklik; MIT/Apache [S20] | Mükemmel (saf Rust) | Harita/planlama/senaryo editörü için olgun hazır bileşen yok (harita crate'leri doğrulanmadı) |
| **Bevy + bevy_egui** | Bevy ECS + render; egui paneller bevy_egui ile; çok pencere ve render-to-texture destekli [S22]. 3B birinci sınıf | Bevy'nin kendi döngüsünde en iyi; grafik yine egui_plot'a bağlı | Bevy: yaklaşık 3 ayda bir API kıran sürüm, kendi sitesi "özellik eksik, dokümantasyon seyrek" diyor [S23]; son sürüm 0.20 (8 Ekim 2026) [S24]; bevy_egui 0.20↔0.43 eşlemesi [S22] | Mükemmel | Sürüm kayması bakım yükü; ürün-UI'ı (harita, form, editör) için ağır |
| **Slint** | Bildirimsel UI + Rust; GL doku içe aktarma 1.1'den beri [S25]; wgpu doku desteği için commit görüldü (API doğrulanmadı) [S25] | İyi (native), grafik için kendi bileşen gerekir | GPL-3.0 / Royalty-free 2.0 (atıf zorunlu, gömülü yasak) / ticari [S26] | İyi | Küçük ekosistem; harita/plot hazır değil; lisans atfı |
| **Dioxus** | Webview masaüstü (Tauri benzeri) veya "native" Blitz render (wgpu tabanlı HTML/CSS) | Webview modu Tauri ile benzer | Blitz "deneysel"; blitz crate'leri alpha (0.3.0-alpha.4, Mayıs 2026) [S27]. Dioxus ekibinin Cognition'a katıldığı haberi tek kaynaklı, doğrulanmadı [S27] | İyi | Native render üretime hazır değil |
| **Qt (C++/QML)** | QGroundControl bu yığındadır [S6]; Qt Quick 3D vb. | Çok iyi | Lisans: açık kaynak (LGPL/GPL) ya da ticari; bu oturumda resmi sayfa çekilemedi (404), ayrıntı doğrulanmadı. QGC dokümanı: Apache build ticari Qt, GPLv3 build açık kaynak Qt gerektirir [S6] | Zayıf: `cxx-qt` README'si "erken geliştirme, API sık değişiyor" der [S28] | Rust ile entegrasyon olgun değil |
| **Electron** | Chromium paketler: OS'ten bağımsız tutarlı WebGL; JS arka uç, Rust için native modül | Chromium'da WebSocket+WebGL iyi bilinir (bilgi, kaynaksız) | Çok olgun (bilgi, kaynaksız). Karşılaştırma yazıları bellek farkı konusunda çelişir [S29] | Dolaylı | Büyük paket, Rust ile iç içe değil |

Yorumlar:

- **Tauri vs egui kararı aslında "harita + editör ekosistemi" kararıdır.** Harita (MapLibre GL JS), 3B (three.js), formlu senaryo editörü ve kayan grafik kütüphaneleri web tarafında hazırdır. egui'de aynı özellikler büyük ölçüde elle yazılır. Karşılığında Tauri'de webview farklılığı (WKWebView/WebView2) ve çift dil (Rust + TS) bakım yükü doğar. Electron bu farkı Chromium ile kapatır ama Rust'tan uzaklaştırır.
- Tauri'de 3B'yi webview'e koymak yerine native bir wgpu penceresi/çocuk yüzey kullanmak mümkün görünse de [S19], bu yol Windows/macOS'ta bileşim (compositing) ve pencere yönetimi riski taşır; öneri değildir, yalnızca tasarım opsiyonudur. WebView2'nin çok süreçli yapısı bazı yer paylaşımı (overlay) senaryolarında kenar durumlar doğurabilir [S19 arama sonucu, doğrulanmadı].
- Sonar görüntüsü için: sunucu zaten bir 2B dizi (açı x menzil, uint8/uint16) üretir; web tarafında `canvas`/WebGL doku olarak çizmek yeterlidir (bilgi, kaynaksız). Büyük değişim gerekmez.
- Bevy'yi **UI olarak değil**, gerekirse ayrı bir "sahne görüntüleyici" olarak düşünmek daha güvenli; sim çekirdeği ise Bevy'ye bağlı olmamalı (sürüm kayması, [S23]).

### 2.2 Süreç mimarisi

Başsız sunucu + istemci ile tek süreç karşılaştırması:

| Ölçüt | Başsız sunucu + UI istemcisi | Tek süreç (UI içinde sim) |
|---|---|---|
| Çökme izolasyonu | UI çökse sim ve HIL köprüsü yaşar | UI hatası simi ve kart bağlantısını düşürür |
| CI / başsız koşu | Doğal: aynı sunucu ikili dosyası CI'da | Ayrı başsız hedef gerekir (yine de çekirdek kütüphane ortaktır) |
| Gecikme | Yerel yuva (soket) maliyeti; 1 kHz sim için sorun değil, UI'ya zaten decimate gider | En düşük; paylaşımlı bellek serbest |
| Zaman kontrolü (duraklat, hız, adım) | Sunucu tek yetkili saat sahibi | UI ile sim aynı döngüde karışabilir |
| Çoklu istemci (3B + harita + Foxglove + betik) | Doğal | Zor |
| Karmaşıklık | Protokol, sürümleme, yaşam döngüsü yönetimi | Daha az hareketli parça |

Öneri: sim çekirdeğini kütüphane olarak yazıp ikisini de desteklemek (tek süreç modu = sunucuyu UI süreci içinde bir iş parçacığı olarak başlatmak). Bu, "ya/ya da" tartışmasını ortadan kaldırır.

Süreçler arası iletişim seçenekleri:

| Seçenek | Güçlü yön | Zayıf yön | Ne zaman uygun |
|---|---|---|---|
| **WebSocket** (örn. axum) | Tarayıcı/webview'den doğrudan; ikili çerçeve; Foxglove protokolü ile araç uyumu; axum MIT [S30]. (Axum sayfası WebSocket'i açıkça belirtmedi; bu özellik bilgi, kaynaksız) | Sıralı tek akış, akış kontrolü sınırlı | UI ve dış araçlar için telemetri; varsayılan seçim |
| **gRPC (tonic)** | Tipli sözleşme, çift yönlü akış, TLS; MIT; MSRV 1.88 [S31] | Webview'den doğrudan değil (grpc-web gerekir, doğrulanmadı); ek protoc/üretim adımı | Rust-Rust ya da Python betikleri için kontrol API'si; UI'ya şart değil |
| **zenoh** | pub/sub + sorgu + depolama; 4-6 bayt başlık; EPL-2.0/Apache-2.0; Rust ve çok dilli bağlayıcılar; `shared-memory` özelliği isteğe bağlı [S32][S33]. Windows/macOS desteği sayfalarda açıkça yazmıyor, doğrulanmadı | Ek kavram yükü; "daha düşük gecikme" iddiası kıyaslama sunmuyor [S32] | Birden çok makine / birçok bağımsız düğüm; ROS benzeri topic mimarisi istenirse |
| **Paylaşımlı bellek (iceoryx2)** | Sıfır-kopya, kilitsiz; Rust çekirdeği; Windows/macOS "tier 2" ("kısıtlı güvenlik/emniyet özellikleri") [S34] | Aynı makineyle sınırlı; web/tarayıcı erişemez; benchmark sayıları sayfada yok | Yalnızca ölçülmüş darboğazda (örn. çok yüksek hızlı sonar/kamera akışı, sim ile ayrı render süreci) |
| **Dahili kanallar** (`tokio::sync`, `crossbeam`) | Sıfır ek yük | Tek süreç | Sim - köprü - kaydedici iş parçacıkları arası |

Frekans bütçesi önerisi (bilgi, kaynaksız; ölçümle teyit edilmeli): sim çekirdeği 500-1000 Hz; kayıt tam hızda (MCAP); UI akışı 30-60 Hz toplu ve decimate; sonar kare hızı sensör modeline göre.

### 2.3 Hazır araçlar: ne bırakılır, ne yazılır

| Araç | Lisans | macOS + Windows | Güçlü olduğu yer | Sınırı |
|---|---|---|---|---|
| **Foxglove** | Studio 2.0 (Mart 2024) ile açık kaynak sürüm sonlandırıldı; 1.x kaynağı GitHub'da kalıyor, gelecek geliştirme kapalı [S3]. Eski depo: çoğu MPL-2.0, resmi ikililer kapalı bileşen içerir [S35]. **SDK MIT, Rust crate `foxglove` var, MCAP yazar, yerel WebSocket ile canlı akış** [S12]. MCAP MIT [S36] | Masaüstü uygulama Linux/Windows/macOS; ilk açılışta oturum gerekir, sonra çevrimdışı çalışır; yerel dosya ve websocket çevrimdışı çalışır [S3] | 3B, plot, görüntü panelleri; MCAP + canlı WebSocket | Kapalı kaynak ürün; ücretsiz plan: 3 geliştirici koltuğu, ticari kullanım koşulları sayfada yok (ToS'a bakılmalı) [S37] |
| **Rerun** | MIT veya Apache-2.0 [S4]. Rust, egui, wgpu, Arrow üzerine kurulu [S38] | Resmi kurulum sayfası işletim sistemi listelemiyor (doğrulanmadı); GitHub release'lerinden `rerun-cli`, `cargo binstall`, pip [S39] | 3B, zaman serisi, görüntü; `.rrd` kayıt; MCAP içe aktarma [S4]; SDK, gRPC ile viewer/sunucuya akıtır [S40] | `.rrd` formatı sürümler arası geri/ileri uyumlu değil, viewer sürümü kütüphane sürümüyle eşleşmeli [S39][S40]; veri RAM'de tutulur, çok büyük veri kümeleri "planlanıyor" [S40]; viewer'ı kendi uygulamaya gömme API'si doğrulanmadı (ilgili sayfa 503 verdi) |
| **PlotJuggler** | MPL-2.0 (dosya bazlı copyleft) [S5][S41] | Windows kurulum paketi ve Linux var; resmi README'de macOS geçmiyor, sürüm 4.1.0'ın macOS varlığı doğrulanmadı [S5][S42]. Topluluk (RoboStack/conda) macOS paketleri var ama eski sürümler [S43] | Zaman serisi inceleme; UDP, WebSocket, ZeroMQ, MQTT canlı akış; MCAP, Parquet, ULog yükleme [S5] | macOS birinci sınıf olmayabilir; 3B/harita yok |
| **QGroundControl** | Apache-2.0 ve GPLv3 çift lisans; Apache build'i ticari Qt ister, GPLv3 build açık kaynak Qt kullanır; katkılar iki lisansa da uygun olmalı [S6] | Windows, macOS, Linux, Android, iOS [S6 arama özeti, doğrulanmadı] | MAVLink araçları için harita, görev planlama, parametre, uçuş ekranı | Çok rotorlu/uçak-merkezli kabuk; ArduSub için Blue Robotics yalnızca QGC'yi destekler [S44] (forum, tarihsiz); otopilotumuz MAVLink konuşmuyorsa işe yaramaz |
| **Mission Planner** | Yalnızca "About" menüsünde GPLv3 bağlantısı görüldü, lisans doğrulanmadı [S45] | İşletim sistemi bilgisi doğrulanmadı (genelde Windows odaklı bilinir; kaynaksız) | ArduPilot ekosistemi | ArduSub'ı desteklemediği forum yanıtında geçiyor [S44] (resmi dokümanla doğrulanmalı) |

Karar çerçevesi:

- **Hazır araca bırak:** Zaman serisi inceleme/kıyaslama (PlotJuggler, Foxglove veya Rerun), ham kayıt gözden geçirme, MAVLink GCS (yalnızca otopilot MAVLink konuşursa ve ikinci aşamada).
- **Kendin yaz:** Sim çekirdeği, HIL köprüsü, sim zaman kontrolü (duraklat/hız/adım), senaryo editörü ve oynatıcı, hata enjeksiyon paneli, yarışma görev planlayıcı, sonar görüntüleyici, kayıt-oynatma ile senkron UI, kabul testi (pass/fail) otomasyonu.
- Gerekçe: hazır araçların hiçbiri sim kontrolünü, senaryoyu ve hata enjeksiyonunu bilmez. Bunlar ürünün farklılaştırıcı çekirdeğidir.
- MAVLink uyumluluk katmanı (ikinci aşama): `mavlink` Rust crate'i (MIT/Apache; 0.17.x güvenli sürüm, 0.18.0 docs.rs derlemesi başarısız) [S46]. HIL için gerekli mesaj tanımlarının varlığı kaynaktan doğrulanmadı.

### 2.4 Harita ve dünya verisi

Harita motoru:

| Seçenek | Not |
|---|---|
| **MapLibre GL JS** (webview içinde) | Önerilen. Çevrimdışı için tek dosyalı **PMTiles** arşivi: HTTP range istekleriyle yalnızca başlık, dizin ve gereken döşemeler okunur; v3 başlangıç yükü 16 KB; arşiv salt okunur; yerel, range destekli sunucu ile servis edilebilir [S47]. Vektör döşeme formatı olarak MLT desteği MapLibre GL JS 5.12.0'dan beri listeleniyor [S48] |
| MapLibre Native + Rust | `maplibre_native` crate'i (0.4.1) ve resmi C API üstü Rust bağlayıcısı var; çevrimdışı bölge API'si var; depoda son push 2025-09-02 görünüyor, aktivite yavaşlamış olabilir [S48]. Webview kullanmayan saf-native UI seçilirse düşünülür |
| Tek başına döşeme sunucuları (OSM) | OSM Vakfı ücretsiz harita API'si/döşeme sağlayamayacağını belirtir; `tile.openstreetmap.org` kullanım politikası uygulanır [S49]. Çevrimdışı laboratuvarda zaten kullanılmamalı |

Batimetri (deniz tabanı derinliği):

| Kaynak | Lisans / koşul | Çözünürlük, format | Uyarı |
|---|---|---|---|
| **GEBCO Grid** | Kamu malı, ücretsiz; ticari kullanım ve ürüne gömme serbest; atıf gerekir; "navigasyon/deniz güvenliği için kullanılmaz"; resmî onay izlenimi verilemez [S50] | netCDF, GeoTIFF, Esri ASCII; 8 döşeme veya özel alan indirme [S51]. Küresel ızgara çözünürlüğü yaklaşık 450 m (15 yay-saniye) olarak bilinir, bu oturumda doğrulanmadı | Derin okyanus ürünüdür, sığ şelf ayrıntısı yoktur [S51]; yarışma havuzu/liman ölçeği için yetersiz |
| **EMODnet Bathymetry DTM 2024** | Ücretsiz; DOI'li. 2022 sürümü CC BY 4.0 olarak yayımlanmış; 2024 kayıtlarında lisans açık yazmıyor, kullanmadan önce kayıttan doğrulanmalı [S52] | 1/16 yay-dakika (~115 m); LAT datumu; 59 döşeme; ESRI ASCII, XYZ, CSV, NetCDF, GeoTIFF [S52] | Yalnızca Avrupa denizleri |
| **NOAA ENC (S-57)** | Arşiv sayfaları ücretsiz indirme ve yeniden biçimlendirip satışa izin veriyor; güncel koşul doğrulanmadı [S53] | S-57; NOAA, S-101 geçiş planı yürütüyor (2023 planı) [S53] | Görsel/sim amaçlı kullanım; navigasyon amaçlı değil. Rust'ta S-57 ayrıştırma çözümü araştırılmadı (GDAL/OGR S57 sürücüsü bilgi, kaynaksız) |
| **OpenSeaMap** | Veri ODbL (atıf + paylaşım koşulu); harita döşemeleri CC BY-SA 2.0 [S54] | OSM tabanlı | Boya/işaret verisi için faydalı, derinlik için değil |

Pratik sonuç: Yarışma alanı büyük olasılıkla küçük bir havuz/göl/liman olduğu için GEBCO/EMODnet'in 100+ m çözünürlüğü işe yaramaz (yarışma alanı bilinmiyor: açık soru). Sim, **kendi dahili batimetri biçimini** (örn. yerel koordinatlı Float32 GeoTIFF veya yükseklik haritası + üst veri) tanımlamalı; GEBCO/EMODnet yalnızca "gerçekçi deniz" örnek senaryoları için içe aktarıcı olarak desteklenmeli.

### 2.5 Kayıt ve tekrar oynatma

| Format | Yapı / güçlü yön | Zayıf yön | Rust | Uygunluk |
|---|---|---|---|---|
| **MCAP** | Başlık, şema, kanal, mesaj, chunk (isteğe bağlı sıkıştırma: zstd, lz4), mesaj/chunk indeksleri, özet, attachment ve metadata kayıtları; zaman damgaları ns uint64; kodlama serbest metin alanı (protobuf, JSON vb. spec tarafından tanımlanmaz) [S7]. Footer'daki özetten zaman/topic bazlı rastgele erişim [S7] | Büyük ikili görüntüler için spec ayrıca bir yönerge vermez [S7]; kesik dosyadan kurtarma spec'te tanımlı değil (özet isteğe bağlı olduğundan baştan sıralı okuma mümkün, çıkarım) [S7]; `mcap recover` komutu README'de doğrulanamadı [S36] | `mcap` crate'i 0.25.0, `IndexedReader` var, zstd/lz4 özellikleri [S8] | **Ana format.** Sensör zaman serisi, olay kanalı, sonar kareleri (kanal başına chunk), senaryo dosyası ve git commit'i attachment/metadata olarak |
| **ULog** (PX4) | İkili, little-endian, tanım + veri bölümleri, düz metin format mesajları [S9] | PX4'e özgü şema modeli; sonar görüntüsü ve olay kaydı için tasarlanmamış | Rust crate'i arama sonuçlarında doğrulanamadı [S9] | Yalnızca firmware ULog üretirse/ikincil olarak |
| **Parquet** (Arrow) | Sütunlu, analiz için ideal; `parquet` crate'i 60.1.0, Apache-2.0, senkron/asenkron yazıcı, zstd/lz4/snappy/brotli [S55] | Sürekli yazma (append) için tasarlanmamış (satır grupları, kapanışta footer); heterojen kanallar/ikili görüntü için uygun değil | `parquet`, `arrow-rs` | Kayıt sonrası dışa aktarma: Python/pandas analizi, toplu metrik |
| **Rerun .rrd** | Rerun'a özgü, Arrow tabanlı | Sürümler arası uyumluluk garantisi yok [S40] | `rerun` crate'i | Kalıcı arşiv formatı olarak önerilmez; görselleştirme için MCAP'ten içe aktarılır |

Öneri: **MCAP birincil, kayıpsız "tek doğru kaynak"**. Her koşu bir MCAP dosyası: kanallar `telemetry/*`, `sensors/*` (ham, gürültülü), `truth/*` (sim gerçeği), `actuators/*`, `sonar/*`, `events/*` (hata enjeksiyonu, mod değişimi, torpido atışı), `hil/link` (köprü ham baytları). Attachment: senaryo TOML'u, sim yapılandırması, git hash, firmware ikilisi hash'i. Chunk bazlı sıkıştırma ile sonar için zstd.

### 2.6 Senaryo formatı

Mevcut örnekler:

- **SDF (Gazebo):** dünya, model, sensör ve fizik tanımı için XML. Gazebo Jetty LTS (Eyl 2025 - May 2031 destek) resmi olarak Ubuntu'da; macOS/Windows "best-effort" [S11]. Dolayısıyla Gazebo'yu çalışma ortamı olarak almak macOS+Windows hedefiyle çelişir; SDF yalnızca sim çıktısının dışa aktarımı/ilham için anlamlı.
- **OpenSCENARIO XML (1.3.1: 2024-11-21; 1.4.0: 2026-05-08) ve DSL 2.1.0:** ASAM; spec belgeleri ASAM e.V.'ye ait telif, kullanım lisans koşullarıyla sınırlı; DSL indirmesi ücretsiz; OpenSCENARIO API Apache-2.0 [S10]. Karayolu trafik senaryosu odaklıdır (bilgi, kaynaksız); deniz aracı, hata enjeksiyonu ve sensör modelleri için genişletme gerekir.
- **Simülatör özel formatları** (PX4, CARLA vb.) bu araştırmada incelenmedi.

Öneri (tasarım): **TOML tabanlı senaryo dosyası + Rust tipinden üretilen JSON Schema.**

- Neden TOML: yorum satırı destekler, git diff'te temiz, `[[events]]` dizi-tablo yapısı zaman çizelgesi için doğal okunur. YAML'ın çok satırlı girinti/tip belirsizliği tuzakları (bilgi, kaynaksız) yoktur. JSON yorum desteklemez.
- Zorunlu alanlar: `schema_version`, `seed` (u64), `duration`, `time_mode`, `[vehicle]` (model/parametre başvurusu), `[environment]` (derinlik, akıntı, dalga/rüzgâr, fırtına profili), `[[events]]` (zaman veya koşul tetikli: `fail_sensor`, `motor_fault`, `storm`, `set_current` ...), `[[checks]]` (kabul kriterleri).
- Sürüm kontrolü: senaryolar repo içinde `scenarios/` altında; çalıştırma sonucu MCAP'e senaryo dosyası ve hash'i gömülür.

Alternatifler (RON, KDL, YAML) bu araştırmada incelenmedi.

### 2.7 Zaman yönetimi

Modlar ve kısıtlar:

| Mod | Tanım | HIL ile uyumlu mu? | Not |
|---|---|---|---|
| Gerçek zamanlı (1x) | `SimTime` duvar saatine kilitli | Evet (tek seçenek) | Gerçek firmware duvar saatiyle koşar. Jitter ölçülmeli |
| Hızlandırılmış | `SimTime` hızı k kat | Hayır | Yalnızca SIL; PX4'te `PX4_SIM_SPEED_FACTOR` Gazebo ve SIH için [S13] |
| Lockstep | Sim ve otopilot ardışık adımlanır; hız serbest, duraklatılabilir | Kartta hayır (firmware zamanı donanımdan) | PX4: "SITL ve simülatörler lockstep ile aynı hızda koşar"; tabloda lockstep Gazebo ve SIH için işaretli; HIL için lockstep belirtilmemiş [S13] |

HIL için pratik kısıt: Kartın zamanı kendi osilatöründen akar. Sim sabit adımlı (örn. 1 ms) ise ve sensör mesajlarında sim zaman damgası taşırsa, firmware zaman damgası/duvar saati farkı ölçülüp kayıtta tutulabilir. "Karta tick göndererek zamanı sim yönetsin" yaklaşımı (kart içi zaman kaynağını sim tick'ine bağlamak) firmware tasarımı gerektirir; ayrı karar konusudur (açık soru).

Determinizm tasarım kuralları:

1. `SimTime` tamsayı nanosaniye; adım sabit; gerçek zaman yalnızca "ne zaman adımlanır" sorusunu yönetir, "adım ne kadardır" sorusunu değil. Bevy kullanılırsa `FixedUpdate`'te delta sabit adımı temsil eder, ancak 32 ms'lik tek adım ile 2x16 ms adım farklı sonuç verir [S56]. Bu nedenle adım boyutu senaryo parametresi olmalı ve kayıtta saklanmalı.
2. Tek `seed`; alt sistem başına ayrı RNG akışı (sensör gürültüsü, dalga, hata zamanlaması). Tohum senaryoda ve MCAP'te.
3. Rapier kullanılırsa: aynı makine/sürüm/derleyicide yerel determinizm varsayılan; çapraz platformda `enhanced-determinism`, IEEE 754 uyumu, aynı oluşturma ve ekleme sırası, nalgebra `ComplexField/RealField` trigonometri; `simd8` ile birlikte kullanılamaz; `parallel` ile birlikte kullanılabilir [S14]. macOS arm64 ve Windows x64 birebir aynı sonucu vermeyebilir; kendi kodumuzda sistem libm yerine `libm` crate'i/nalgebra fonksiyonları tercih edilmeli (bilgi, kaynaksız).
4. Tekrar oynatma iki türlüdür: (a) **yeniden hesaplama** (senaryo + tohum ile aynı sim; yalnızca aynı platformda bit-tam beklenir); (b) **kayıttan oynatma** (MCAP'teki sensör/aktüatör akışını doğrudan UI'ya ve/veya firmware'e verir; platform bağımsız, HIL için doğru yöntem).
5. Altın-iz regresyon testleri: OS başına ayrı referans veya toleranslı karşılaştırma.

### 2.8 Proje altyapısı

**Cargo workspace düzeni (öneri):**

```
repo/
  Cargo.toml                 # [workspace] host crate'ler, resolver = "3", exclude = ["firmware"]
  rust-toolchain.toml
  crates/
    ida-types/               # #![no_std] + feature "std"; mesajlar, birimler, protokol
    ida-hilproto/            # #![no_std] HIL köprü protokolü, CRC, çerçeveleme
    ida-control/             # #![no_std] kontrol algoritmaları (kartta ve SIL'de aynı kod)
    sim-core/                # std; dinamik, sensör/aktüatör modelleri, hata enjeksiyonu
    sim-server/              # tokio/axum, WS+HTTP, kaydedici (MCAP), HIL köprüsü
    lab-app/ (src-tauri)     # Tauri kabuğu + web ön yüzü (apps/lab-ui)
    cli/                     # senaryo koşucu, doğrulayıcı
  firmware/
    Cargo.toml               # AYRI workspace; hedef thumbv8m.main-none-eabihf
    .cargo/config.toml       # runner = probe-rs, target
  scenarios/  docs/  xtask/
```

Gerekçeler:

- Cargo, workspace'in tek `target/` ve tek `Cargo.lock` kullandığını, `[patch]`/`[profile]`'ın yalnızca kökte tanındığını, çalışma alanı dizini içindeki path bağımlılıklarının otomatik üye sayıldığını, `exclude` ve `default-members` ile bunun kontrol edilebildiğini belirtir; sanal manifestte `resolver` açıkça yazılmalıdır [S57].
- `no_std` ve `std` paketleri aynı derlemede feature birleşimi (unification) nedeniyle sorun çıkarır; çözüm olarak ayrı cargo çağrıları, `default-members`, ayrı workspace önerilir; `per-package-target` kararsızdır ve açık hataları vardır [S15]. Bu yüzden firmware ayrı workspace'te, paylaşılan `no_std` crate'ler path bağımlılığı ile kullanılır (çalışır görünür, ancak bu oturumda uygulamalı denenmedi: doğrulanmadı).
- `[workspace.lints]`, `[workspace.dependencies]`, `[workspace.package]` ile sürüm/lint tek yerde (Rust 1.64+/1.74+) [S57].
- STM32U5A9 için `embassy-stm32` 0.6.0'da `stm32u5a9bj/nj/vj/zj` feature'ları mevcut [S18]. DK kartının tam çip kodu (NJ varyantı olduğu bilgi, kaynaksız; Zephyr kart sayfası U5A9J-DK'yi listeler [S58]) ve `probe-rs` hedef listesinde U5A9 olup olmadığı doğrulanmadı [S59].

**CI (GitHub Actions):**

- Matris: `macos-latest` (macOS 26, arm64), `windows-latest` (Windows Server 2025, x64); gerekirse `macos-15`, `windows-11-arm` [S16]. Intel macOS koşucusu bu tablolarda yok (Intel Mac hedeflenecekse açık soru).
- Aşamalar: `cargo fmt --check`, `clippy -D warnings`, `cargo nextest run` (macOS/Windows destekli, doctest'ler ayrıca `cargo test --doc`) [S17], firmware için yalnızca çapraz derleme (`thumbv8m.main-none-eabihf`), Tauri paketleme, senaryo regresyonu.
- HIL testi (gerçek kart) GitHub'ın barındırdığı koşucularda yapılamaz; kartın bağlı olduğu self-hosted koşucu gerekir (bilgi, kaynaksız). `embedded-test` + `probe-rs run` ile cihaz üzeri test altyapısı vardır [S60].

**Test katmanları (öneri):** (1) birim testleri (`no_std` crate'ler host'ta), (2) bileşen testleri (sensör/aktüatör modelleri, hata enjeksiyonu), (3) senaryo regresyonu (SIL, başsız, MCAP karşılaştırma), (4) protokol uyumluluğu (HIL köprüsü, sahte kart ile), (5) cihaz üzeri testler (embedded-test), (6) HIL kabul koşuları (self-hosted), (7) UI duman testleri.

**Dokümantasyon araçları:** `rustdoc` (API), mdBook (kullanıcı/tasarım kitabı), şemalardan üretilen senaryo referansı; mdBook'un macOS/Windows desteği bu oturumda doğrulanmadı (bilgi, kaynaksız).

---

## 3. Mimari için öneriler

1. **Katmanlama (sim-core hiçbir UI/Ağ bilmesin).**
   `sim-core` (kütüphane) -> `sim-server` (ağ + kayıt + HIL köprüsü) -> istemciler (Lab UI, CLI, betik, Foxglove/PlotJuggler). Gerekçe: aynı çekirdek CI'da, UI içinde ve sunucuda; Bevy gibi hızlı değişen bir motora bağımlı kalınmaz [S23].
2. **UI: Tauri 2 + web ön yüzü ile başla; 2 haftalık bir "WebGL/akış prototipi" ile doğrula.** Ölçülecekler: WKWebView ve WebView2'de three.js 3B + MapLibre + 60 Hz grafik + 1 saat sürekli akışta kare süresi/bellek. Geçme kriteri başarısız olursa yedek: egui/eframe (+ ayrı harita çözümü) ya da Electron. Gerekçe: harita/editör ekosistemi (bkz. 2.1) ve Tauri'nin binary akış desteği [S2].
3. **Telemetri taşıması: WebSocket binary çerçeveler; kontrol: HTTP/JSON (veya aynı WS üzerinde komut mesajları).** gRPC şart değil (webview'den doğrudan değil, doğrulanmadı). Foxglove WebSocket/SDK'sı uyumlu bir yayın kanalı olarak eklenirse hazır panellerden ve PlotJuggler'dan yararlanılır [S12][S5]. zenoh ve iceoryx2 "ölçüm yapana kadar yok" listesinde; ancak mesaj şemaları baştan taşıma-bağımsız tutulmalı.
4. **Kayıt: MCAP birincil.** Sim kayıt kanalı MCAP'e yazan ayrı iş parçacığı; UI "canlı" ve "tekrar" için aynı veri yolunu (kanal adı/şema) kullanır. Böylece canlı izleme ve tekrar oynatma aynı UI kodudur. Parquet yalnızca analiz için dışa aktarma.
5. **Hazır araçlar:** Geliştirici iş akışı için Rerun veya PlotJuggler (MPL dosya bazlı copyleft'tir, ikili olarak çağrıldığı için sorun çıkarmaz; kodumuza bağlanmaz) ve Foxglove (yalnızca isteğe bağlı, kapalı kaynak ürün) kullanılabilir; ürün çekirdeği hiçbirine bağlı olmamalı. Rerun'un `.rrd` uyumsuzluğu nedeniyle kalıcı arşiv MCAP'te kalmalı [S40].
6. **MAVLink: ikinci aşamada isteğe bağlı uyumluluk katmanı.** Otopilot MAVLink konuşursa QGC, hazır bir GCS olur (ücretsiz harita/parametre) [S6][S46]. Hibrit yüzey-dalış araç kavramı ArduSub (ROV) görev modeline tam oturmayabilir (çıkarım). Bu nedenle özel görev planlayıcı yazmaktan vazgeçmek için neden yok.
7. **Senaryo: TOML + JSON Schema + tohum + MCAP'e gömme.** Hata enjeksiyonu "olay" olarak senaryo içinde; elle enjeksiyon da aynı olay yolundan geçip MCAP'e `events/*` olarak yazılır. Böylece her manuel müdahale tekrar üretilebilir.
8. **Zaman: `SimClock` soyutlaması (SimTime ns, adım sabit) + üç sürücü:** `RealTime` (HIL ve etkileşimli), `Accelerated` (SIL), `Lockstep` (SIL, adım komutu ile). HIL modunda hız kontrolü devre dışı. Köprü gecikmesi/jitter ölçümü sürekli kaydedilsin.
9. **Repo: tek repo, host workspace + ayrı `firmware/` workspace'i, paylaşılan `no_std` crate'ler.** CI'da host işleri macOS+Windows matrisinde, firmware çapraz derleme yalnızca derleme doğrulaması; gerçek kart testleri self-hosted koşucuda.
10. **Batimetri: dahili ızgara formatı + içe aktarıcılar.** GEBCO (kamu malı, atıf, navigasyon yasağı) ve EMODnet (lisans kaydından doğrulanarak) isteğe bağlı içe aktarma; yarışma alanı için elle/sentetik derinlik haritası öncelikli [S50][S52].

---

## 4. Riskler ve açık sorular

Riskler:

- **Webview farkı (Tauri):** WKWebView ve WebView2'de WebGL/canvas performans ve hata davranışı ayrışabilir; bu oturumda ölçüm yapılmadı.
- **Foxglove bağımlılığı:** Kapalı kaynak ve ticari koşullar değişebilir (Mart 2024'te açık kaynak sonlandırıldı) [S3]; çekirdek MCAP/SDK MIT olduğundan bağımlılık bu katmanda kalmalı [S12][S36].
- **Rerun sürüm uyumsuzluğu:** `.rrd` kararsız, viewer-SDK eşleşmesi zorunlu [S39][S40].
- **PlotJuggler macOS desteği belirsiz** [S5][S42].
- **Bevy/egui kırıcı değişiklik ritmi:** Bevy ~3 ayda bir [S23]; egui "WIP" [S20].
- **Determinizm yanılgısı:** macOS arm64 ve Windows x64 arasında bit-tam sonuç varsayılmamalı [S14].
- **HIL gerçek zamanlılık:** USB/seri köprü gecikmesi ve jitter'ı; masaüstü OS'lerde (macOS/Windows) gerçek zamanlı garanti yoktur (bilgi, kaynaksız). Ölçülmeli.
- **Cargo feature unification** ile `no_std` kirlenmesi [S15].
- **Lisans yükümlülükleri:** Slint atıf (royalty-free) veya GPL [S26]; Qt/QGC lisans ikilemi [S6]; GEBCO atıf ve "navigasyon dışı" [S50]; OSM/OpenSeaMap atıf-paylaşım [S54].
- **iceoryx2 "tier 2"** (kısıtlı güvenlik/emniyet) Windows/macOS'ta [S34].

Açık sorular:

1. Otopilot firmware'i MAVLink konuşacak mı, yoksa özel bir protokol mü? (QGC/Mission Planner değerlendirmesi bunu belirler.)
2. Yarışma alanı neresi (havuz, göl, liman, açık deniz)? Batimetri ve harita gereksinimini belirler.
3. HIL köprüsü hangi fiziksel kanaldan (ST-LINK sanal COM, USB CDC, Ethernet)? STM32U5A9J-DK'nin kullanılabilir arayüzleri ve gecikmeleri bu araştırmada incelenmedi.
4. Firmware, sim tick'i ile zaman sürülebilir mi (HIL'de hızlandırma için)? Aksi halde HIL yalnızca 1x.
5. Intel Mac gerekiyor mu? (`macos-latest` arm64'tür [S16].)
6. `probe-rs`'te U5A9 hedefi var mı ve DK'nin tam çip kodu nedir? [S18][S59]
7. Sonar veri hacmi: kare boyutu, kare hızı, MCAP boyut bütçesi?
8. Senaryo editörü "görsel" mü olmalı (harita üzerinde sürükle-bırak) yoksa metin + önizleme yeterli mi?
9. EMODnet 2024 DTM lisansının kayıt düzeyinde teyidi [S52].
10. Foxglove ücretsiz plan ve ToS ticari/yarışma kullanımına izin veriyor mu? [S37]

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **Foxglove Studio'nun açık kaynak sürümü 2.0 (11 Mart 2024) ile sonlandırıldı; 1.x kaynağı GitHub'da kalıyor ama gelecek geliştirme kapalı; masaüstü uygulama ilk açılışta oturum ister, sonra çevrimdışı çalışır.** Kaynak: https://discourse.openrobotics.org/t/foxglove-2-0-integrated-ui-new-pricing-and-open-source-changes/36583 (Ayrıca güncel durum Foxglove sitesinden teyit edilmeli; bu kaynak 2024 tarihli.)
2. **Rerun `.rrd` dosyaları sürümler arası tam geri/ileri uyumlu değil ve viewer sürümü SDK sürümüyle eşleşmeli.** Kaynaklar: https://www.rerun.io/docs/getting-started/installing-viewer ve https://github.com/rerun-io/rerun/blob/main/ARCHITECTURE.md
3. **QGroundControl Apache-2.0 ve GPLv3 çift lisanslı; Apache-2.0 ile derlemek ticari Qt lisansı gerektirir, GPLv3 ile açık kaynak Qt kullanılır.** Kaynak: https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/contribute/licences.html
4. **Gazebo (Jetty, LTS) macOS ve Windows'u yalnızca "best-effort" destekler; resmi platform Ubuntu amd64.** Kaynak: https://gazebosim.org/docs/latest/install/
5. **PX4 dokümanında lockstep Gazebo ve SIH için listelenir; HIL'in lockstep olup olmadığı belirtilmez (HIL = gerçek donanımda normal firmware).** Kaynak: https://docs.px4.io/main/en/simulation/
6. **Rapier çapraz platform determinizmi için `enhanced-determinism`, IEEE 754 uyumu, aynı kurulum sırası ve nalgebra matematik fonksiyonlarını şart koşar; `simd8` ile uyumsuzdur.** Kaynak: https://rapier.rs/docs/user_guides/rust/determinism/
7. **GEBCO Grid kamu malıdır, ticari kullanım ve gömme serbesttir, atıf gerekir, navigasyon/deniz güvenliği için kullanılamaz.** Kaynak: https://www.gebco.net/data-products/gridded-bathymetry/terms-of-use
8. **Tauri 2'de akış için önerilen mekanizma `Channel`'dır; `Event` yalnızca JSON taşır ve hızlı güncellemelerde sırasız işlenebilir.** Kaynak: https://v2.tauri.app/develop/calling-rust/
9. **GitHub Actions: `macos-latest` = macOS 26 arm64, `windows-latest` = Windows Server 2025 x64.** Kaynak: https://github.com/actions/runner-images
10. **`embassy-stm32` 0.6.0 `stm32u5a9bj/nj/vj/zj` feature'larına sahip; `probe-rs`'te U5A9 hedefi ve DK'nin tam çip kodu ayrıca doğrulanmalı.** Kaynaklar: https://docs.rs/crate/embassy-stm32/latest/features ve https://probe.rs/targets/
11. **bevy_egui sürüm eşlemesi: Bevy 0.20 ↔ bevy_egui 0.43; Bevy 0.20 8 Ekim 2026'da yayımlandı.** Kaynaklar: https://github.com/mvlabat/bevy_egui ve https://bevy.org/news/

---

## 6. Kaynaklar

- [S1] Tauri deposu (lisans, platform/webview tablosu): https://github.com/tauri-apps/tauri
- [S2] Tauri 2 - Rust'tan çağırma (Channel/Event/Response): https://v2.tauri.app/develop/calling-rust/
- [S3] Foxglove 2.0 duyurusu: https://discourse.openrobotics.org/t/foxglove-2-0-integrated-ui-new-pricing-and-open-source-changes/36583
- [S4] Rerun deposu: https://github.com/rerun-io/rerun ; fiyat/lisans sayfası: https://rerun.io/pricing
- [S5] PlotJuggler deposu: https://github.com/facontidavide/PlotJuggler
- [S6] QGroundControl lisans dokümanı: https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/contribute/licences.html
- [S7] MCAP spesifikasyonu: https://mcap.dev/spec
- [S8] `mcap` Rust crate'i: https://docs.rs/mcap/ (0.25.0 kaynak: https://docs.rs/crate/mcap/0.25.0/source/src/lib.rs)
- [S9] PX4 ULog formatı: https://docs.px4.io/main/en/dev_log/ulog_file_format
- [S10] ASAM OpenSCENARIO: https://openscenario.asam.net/ASAM_OpenSCENARIO_XML/v1.4.0/index.html ; DSL: https://asam.net/standards/detail/openscenario-dsl/
- [S11] Gazebo kurulum/platform desteği: https://gazebosim.org/docs/latest/install/
- [S12] Foxglove SDK: https://github.com/foxglove/foxglove-sdk
- [S13] PX4 simülasyon (lockstep, hız faktörü, HIL): https://docs.px4.io/main/en/simulation/
- [S14] Rapier determinizm: https://rapier.rs/docs/user_guides/rust/determinism/
- [S15] Cargo feature unification / karma hedef workspace tartışmaları: https://rust-lang.github.io/rfcs/3692-feature-unification.html ; https://internals.rust-lang.org/t/i-found-that-cargo-workspace-is-not-very-friendly-for-multi-target-crates/22595 ; https://internals.rust-lang.org/t/per-package-target-feature-unification/24349
- [S16] GitHub runner images (etiket eşlemesi): https://github.com/actions/runner-images
- [S17] cargo-nextest: https://nexte.st/
- [S18] embassy-stm32 feature listesi: https://docs.rs/crate/embassy-stm32/latest/features ; HAL: https://github.com/embassy-rs/embassy/tree/main/embassy-stm32
- [S19] wry (çocuk webview, platformlar): https://github.com/tauri-apps/wry ; wgpu örneği: https://docs.rs/crate/wry/0.46.3/source/examples/wgpu.rs
- [S20] egui: https://github.com/emilk/egui
- [S21] egui_plot: https://github.com/emilk/egui_plot ; performans tartışması (topluluk, zayıf kanıt): https://reddit.sentinel-team.org/posts/1pnvb4g/snapshots/2025-12-16T18%3A22%3A30.381793Z
- [S22] bevy_egui: https://github.com/mvlabat/bevy_egui
- [S23] Bevy giriş (olgunluk uyarısı, lisans): https://bevy.org/learn/quick-start/introduction/
- [S24] Bevy sürüm haberleri: https://bevy.org/news/
- [S25] Slint 1.1 duyurusu (GL doku): https://slint.dev/blog/slint-1.1-released ; wgpu doku commit'i: https://git.joshthomas.dev/language-servers/slint/src/branch/olivier/examples (ikincil, doğrulanmadı)
- [S26] Slint lisanslama: https://slint.dev/terms-and-conditions
- [S27] Dioxus/Blitz: https://github.com/DioxusLabs/blitz ; https://dioxuslabs.com/blog/release-070 ; blitz-html sürümleri: https://lib.rs/crates/blitz-html ; Cognition haberi (tek kaynak): https://www.unite.ai/?p=465293
- [S28] cxx-qt: https://github.com/KDAB/cxx-qt
- [S29] Tauri vs Electron karşılaştırmaları (blog, ikincil): https://github.com/tauri-apps/tauri/issues/5889 ; https://dev.to/gethopp/tauri-vs-electron-performance-bundle-size-and-the-real-trade-offs-1el4
- [S30] axum: https://github.com/tokio-rs/axum
- [S31] tonic: https://github.com/hyperium/tonic
- [S32] zenoh genel bakış: https://zenoh.io/docs/overview/what-is-zenoh/
- [S33] zenoh deposu: https://github.com/eclipse-zenoh/zenoh
- [S34] iceoryx2: https://github.com/eclipse-iceoryx/iceoryx2
- [S35] Eski Foxglove Studio deposu (MPL): https://www.github.com/foxglove/studio
- [S36] MCAP deposu: https://github.com/foxglove/mcap
- [S37] Foxglove fiyatlandırma: https://www.foxglove.dev/pricing
- [S38] Rerun hakkında: https://rerun.io/docs/reference/about
- [S39] Rerun viewer kurulumu: https://www.rerun.io/docs/getting-started/installing-viewer
- [S40] Rerun mimarisi: https://github.com/rerun-io/rerun/blob/main/ARCHITECTURE.md
- [S41] MPL-2.0 özeti: https://opensource.org/license/mpl-2-0
- [S42] PlotJuggler sürümleri: https://github.com/facontidavide/PlotJuggler/releases
- [S43] RoboStack PlotJuggler paketleri (macOS, eski sürüm): https://anaconda.org/robostack-humble/ros-humble-plotjuggler
- [S44] Blue Robotics forumu (ArduSub - QGC / Mission Planner, tarihsiz): https://discuss.bluerobotics.com/t/qgc-necessary-or-custom-interface-possible/10639
- [S45] ArduPilot Mission Planner sayfası: https://ardupilot.org/planner/
- [S46] rust-mavlink (docs.rs): https://docs.rs/crate/mavlink/latest
- [S47] PMTiles: https://docs.protomaps.com/pmtiles/ ; https://protomaps.com/blog/pmtiles-v3-whats-new
- [S48] MapLibre Native Rust: https://lib.rs/crates/maplibre_native ; MLT durumu: https://maplibre.org/maplibre-tile-spec/implementation-status/
- [S49] OSM telif/lisans: https://osmfoundation.org/wiki/Licence_and_Legal_FAQ ; https://www.openstreetmap.org/copyright
- [S50] GEBCO kullanım koşulları: https://www.gebco.net/data-products/gridded-bathymetry/terms-of-use
- [S51] GEBCO ızgara verisi: https://www.gebco.net/data-products/gridded-bathymetry-data
- [S52] EMODnet Bathymetry DTM 2024: https://emodnet.ec.europa.eu/en/emodnet-bathymetry-dtm-2024-release
- [S53] NOAA ENC / S-101 geçiş planı: https://iho.int/uploads/user/Inter-Regional%20Coordination/WEND-WG/WENDWG14/WENDWG14_2024_05.1Ac_EN_NOAA%20US%20Transition%20Plan%20S-101%2021%20November%202023.pdf ; indirme: https://catalog.data.gov/dataset/noaa-electronic-navigational-charts-enc1/resource/d0ee1f2d-cefa-4591-9de4-9ba2203a34b6
- [S54] OpenSeaMap lisansı: https://wiki.openstreetmap.org/wiki/OpenSeaMap ; SSS: https://openseamap.org/index.php?id=faq&L=1
- [S55] parquet crate'i: https://docs.rs/parquet/latest/parquet/
- [S56] Bevy sabit zaman adımı: https://bevy-cheatbook.github.io/fundamentals/fixed-timestep.html ; headless örnek: https://docs.rs/crate/bevy/0.18.1/source/examples/app/headless.rs (determinizm tartışması Reddit yorumu, resmi değil)
- [S57] Cargo workspace dokümanı: https://doc.rust-lang.org/cargo/reference/workspaces.html
- [S58] Zephyr STM32U5A9J-DK kart sayfası: https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html
- [S59] probe-rs hedef listesi: https://probe.rs/targets/
- [S60] embedded-test: https://lib.rs/crates/embedded-test
