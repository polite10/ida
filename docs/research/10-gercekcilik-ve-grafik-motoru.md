# 10 — Azami Gerçekçilik: Grafik Motoru ve GPU Üzerinde Sensör Simülasyonu

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Doğrulaması: [dogrulama/10-codex.md](dogrulama/10-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

> Kapsam: Tek hibrit (yüzey + dalış) insansız deniz aracı için HIL/SIL laboratuvarı. PC tarafı **C++**, kart ve ortak çekirdek **C**; Rust yok. Fizik çekirdeği (6-DOF Fossen, anlık batmış hacim, dalga, akıntı) başsız, sabit adımlı ve render'dan bağımsız. Hedef: macOS (Apple Silicon, Metal) ve Windows'ta native.
> Tarih: 2026-11-11. Önceki raporlar (02, 04, 05) ve doğrulamaları (dogrulama/02, 04, 05) okundu; çelişkide doğrulama esas alındı. Doğrulanmış konular burada yeniden araştırılmadı, yalnız atıf yapıldı.
> Etiketler: **[doğrulandı]** = birincil kaynakta (resmi doküman, kaynak kod, changelog, makale) bu oturumda okundu; **[ikincil]** = yalnız arama özeti/ikincil kaynak; **[doğrulanmadı]** = bilgi birikimi veya tahmin; **[çıkarım]** = kaynaklardan benim türettiğim sonuç; **[tahmin]** = ölçülmemiş sayısal kestirim.

---

## 1. Özet

1. **C++ kararı motor tablosunu değiştiriyor.** Bevy/wgpu-Rust yolu düştü. Gerçekçi adaylar: **Godot 4.7 (GDExtension C++)**, **Unreal Engine 5.x** ve **taşınabilir RHI üzerinde özel render** (bgfx, WebGPU-native: Dawn/wgpu-native, Filament). O3DE'nin macOS desteği resmi dokümanda hâlâ "deneysel"; Magnum OpenGL'e dayanıyor; Diligent'in Metal arka ucu ticari lisanslı. Bu üçü ana yol olamaz.
2. **Donanım ışın izleme, macOS ve Windows'ta ortak bir C++ soyutlamasıyla bugün yok.** Metal RT API'si M1 (Apple6+) ve sonrasında var, RT donanım birimi M3 (Apple9) ile geldi. MoltenVK'da ise ray tracing yok (1.4.2, 2026-07-20). Godot RD ray tracing yalnız Vulkan'da çalışıyor ve "MoltenVK sınırları nedeniyle macOS'ta yok". Slang'in Metal hedefi RT desteklemiyor. wgpu-native C başlığında RT açılmamış. Sonuç: **sonar ana yolu donanım RT'ye bağlanmamalı.**
3. **Platform bağımsız, deterministik ve başsız sonar için en güçlü yol CPU'da Intel Embree** (Apache-2.0; x86 ve Apple M1 ARM; v4.4.1). GPU sonar ikinci yol olmalı: Stonefish tarzı rasterleştirme + compute. Bu yöntem Metal/D3D12/Vulkan'ın hepsinde çalışır. HoloOcean 2.4'ün "GPU sonar"ı da "rendered scene (image-based)" yani rasterleştirme tabanlı.
4. **HoloOcean 2.4.0 (2026-11-03) dersleri:**
   - Octree → Raycast (CPU) + GPU sonarlara geçildi.
   - Malzeme bağımlı yansıtıcılık bir CSV dosyasından okunuyor.
   - FFT dalgaları Niagara'da CPU+GPU ortak çalışıyor. Mesh tabanlı yüzerlik "bir tahmin, gerçek dünyayı yansıtmayabilir" notuyla geliyor ve `wave_force` ayarıyla düzeltiliyor.
   - İstemci ile motor semafor + paylaşımlı bellek ile kilit adımda (lockstep) çalışıyor.
   - Yeni raycast/GPU sonar örneklerinde `MultiPath: False`. Multipath yalnız eski octree sonarda var.
   - Platform hâlâ yalnız Linux/Windows; Vulkan GPU şart.
5. **Ping360 için "azami gerçekçi" model ucuz.** Her açı adımı tek ışınlı bir ping olduğundan şu zincir CPU'da gerçek zamanlı çalışabilir [çıkarım]:
   - faz-uyumlu (coherent) saçıcı toplamı,
   - darbe zarfı ile evrişim,
   - ışın deseni,
   - havuz duvarı ve yüzeyinden 1–2 sekmeli speküler multipath,
   - TVG/kazanç ve 8-bit kuantalama.

   Pahalı olan FLS ve SSS görüntü sonarlarıdır. Onlarda T2 (rasterleştirme/ışın + istatistiksel speckle) gerçek zamanlı ana kademe olmalı.
6. **Kamerada gerçekçiliği motorun aydınlatmasından çok su optiği belirliyor.** Akkaynak–Treibitz modeline göre β_D, menzile (z), yansıtıcılığa, ışık spektrumuna ve kameranın spektral yanıtına bağlı; β_B ise farklı bir katsayı. OceanSim kanal başına sabit β kullanıyor ve katsayıları "elle ayarlıyor". Azami gerçekçilik için iki şey gerekir:
   - birkaç spektral bantta ışık taşıyıp kamera yanıtıyla RGB'ye entegre etmek (Jerlov IOP tabloları ile),
   - araç lambası için Jaffe–McGlamery tipi geri saçılımı ray-march ile hesaplamak.
7. **Fizik–görüntü tutarlılığı için fizik çekirdeği yetkili olmalı.** Render tarafına yükseklik alanı değil, *spektrum parametreleri + tohum + sim zamanı* gönderilmeli. GPU FFT ve çekirdekteki doğrudan seri toplamı, aynı ayrık Fourier serisini değerlendirirse sonuçlar kayan nokta hassasiyetinde eşleşir [çıkarım]. Tessendorf'un "choppy" yatay ötelemesi kullanılırsa çekirdeğin yükseklik sorgusu da bu ötelemeyi ters çevirmeli; aksi halde görüntü ile yüzerlik ayrışır.
8. **Gerçekçiliği simülatör kodundan çok bilinmeyen parametreler sınırlıyor:**
   - hedef ve havuz malzemelerinin 750 kHz yansıtıcılığı,
   - Ping360'ın gerçek ışın deseni, yan lob seviyesi ve TVG eğrisi,
   - havuz suyunun optik özellikleri,
   - kameranın spektral yanıtı,
   - aracın hidrodinamik katsayıları.

   Standart APL-UW deniz tabanı geri saçılım modeli yalnız 10–100 kHz için geçerli. Bu yüzden kalibrasyon ölçümü (gerçek Ping360 kaydı, menzili bilinen renk kartı) kod yazmaktan daha fazla gerçekçilik kazandırır.
9. **Literatürde gerçek veriyle nicel doğrulanmış sonar simülatörü azdır.** Çoğu karşılaştırma niteldir (Choi 2021, OceanSim). Nicel örnekler şunlar:
   - tankta hedef/duvar eko seviyesi eşleşmesi (Sensors 2025),
   - veri kümesi düzeyinde histogram ve LBP dokusu KL/JS/EMD (ACOUSIM 2026),
   - sim'de eğit, gerçekte test et farkı (OceanSim'in 2026 deniz kestanesi çalışması: "kayda değer fark kalıyor").

   Doğrulama metrikleri baştan tasarlanmalı.
10. **Öneri:**
    - Fizik ve sonar çekirdeği C/C++, başsız; Ping360 ve tek ışın sonarları Embree ile CPU'da.
    - Görselleştirme + kamera + GPU sonar ayrı bir **"render/sensör süreci"** olsun. İlk aday **Godot 4.7 + GDExtension (MIT)**; çünkü Metal, D3D12 ve Vulkan sürücüleri, compute ve küçük takım maliyeti dengeli.
    - **Unreal** yalnız ekipte UE deneyimi varsa ve Mac'te 2 haftalık prototip başarılı olursa seçilsin.
    - Hangisi seçilirse seçilsin, sonar ve kamera fiziği motordan bağımsız, taşınabilir shader/C++ modülleri olarak yazılsın.

---

## 2. Bulgular

### 2.1 Grafik motoru seçenekleri (PC = C++)

Sürüm ve lisans bilgileri GitHub API'den (2026-11-11) ve resmi dokümanlardan alındı.

| Seçenek | macOS (Apple Silicon) | Windows | Okyanus / su altı gerçekçiliği | Kamera: dokuya render + geri okuma | GPU compute (sonar) | Donanım RT | Başsız | Dış fizik yetkili, motor yalnız görüntü | Küçük takım yükü | Lisans |
|---|---|---|---|---|---|---|---|---|---|---|
| **Unreal Engine 5.x** (doküman 5.9 başlıklı, sürüm tablosu 5.8'e kadar) | Native. Sonoma 14.5+; M1 minimum, M3 önerilen. Lumen SW: M1+. **HWRT + MegaLights: M2+, "Experimental"**. Nanite/VSM: M2+, "Beta". **Xcode 26.4 uyumsuz** [doğrulandı] | Tam (DX12, DXR) | En yüksek hazır kalite: Water plugin (Gerstner; CPU'dan `GetWaveInfoAtPosition` sorgusu) [doğrulandı], volumetrik sis, Lumen. FFT okyanus hazır değil; HoloOcean kendi Niagara FFT sistemini kurdu [doğrulandı] | SceneCapture + RenderTarget. HoloOcean: her tick kamera render'ı "motorun derinlerinde bağlam değişimi" yaptığı için pahalı; 512² kamera kare süresini 8.5'ten 35 ms'ye çıkarıyor (eski Holodeck ölçümü) [doğrulandı] | RDG compute shader (HLSL → MSL çapraz derleme) [doğrulanmadı ayrıntı] | Win: var. Mac: deneysel (Lumen yolu) | `-RenderOffscreen` resmi olarak belgelenmemiş, forumlarda Vulkan'a özgü olduğu söyleniyor [ikincil]; `-nullrhi` render'ı kapatır | Örnekler var: AirSim/Colosseum `ExternalPhysicsEngine` + `simSetVehiclePose` [ikincil]; HoloOcean semafor lockstep [doğrulandı] | **En ağır**: editör, derleme süreleri, Xcode kırılmaları, motor yükseltmeleri | Epic EULA; oyunlarda 1M USD sonrası %5 royalty; kaynak erişimi Epic–GitHub hesap bağlantısı ister. Lisans ayrıntısı dogrulama/02 #21'de "kısmen doğru" |
| **Godot 4.7 (4.7.2-stable, 2026-08-18) + GDExtension (godot-cpp)** | Native. Forward+ "Vulkan, Direct3D 12 veya Metal" kullanıyor [doğrulandı] | Native. 4.6'dan itibaren Windows varsayılanı D3D12 [ikincil] | Orta. Volumetrik sis ve PBR hazır; okyanus için topluluk FFT projeleri (GodotOceanWaves, compute) [ikincil]; su altı post-process kendin yazılacak | SubViewport + `texture_get_data` / `texture_get_data_async` [doğrulandı: API listesi] | `RenderingDevice` compute (Forward+ destekli); ayrı iş parçacığında "local rendering device" [doğrulandı] | RD'de BLAS/TLAS/RT pipeline API'si var ama deneysel; **"yalnız Vulkan; macOS/iOS'ta MoltenVK sınırları nedeniyle desteklenmiyor"** [doğrulandı] | `--headless` görüntü sürücüsünü kapatıyor; **"RenderingDevice headless kipte yok"** [doğrulandı]. GPU sensörü için gizli/küçük pencere gerekir [çıkarım] | GDExtension ile C++ düğümü; fizik dış süreçten poz alır (Godot fiziği kapalı) [çıkarım] | Orta-düşük | MIT (motor ve godot-cpp) |
| **O3DE 2605.0** | Resmi dokümana göre macOS host ve hedef **"deneysel"** [doğrulandı, arama özeti + resmi sayfa] | Var | Atom renderer; okyanus hazır değil [doğrulanmadı] | Var [doğrulanmadı] | Var [doğrulanmadı] | — | — | — | Yüksek | Apache-2.0/MIT [doğrulanmadı] |
| **bgfx** (BSD-2) | Metal | D3D11/12, Vulkan; WebGPU (yalnız Dawn) [doğrulandı] | Hiçbiri hazır; tüm sahne/PBR/ocean senin işin | Var (blit/readTexture) [doğrulanmadı ayrıntı] | Compute var [bilgi] | Yok | Pencere gerektirmez [doğrulanmadı] | Doğal (kütüphane) | Yüksek (motor yazmak) | BSD-2 |
| **WebGPU native: Dawn / wgpu-native (v29.0.1.1)** | Metal | D3D12/Vulkan | Hiçbiri hazır | `mapAsync` ile tanımlı geri okuma | WGSL compute, taşınabilir | **C API'de yok** (wgpu-native `wgpu.h`'de RT bayrağı bulunmadı; Dawn'da resmi uzantı bulunamadı) [doğrulandı/doğrulanmadı] | Pencere gerekmez | Doğal | Yüksek | Apache-2.0/MIT (wgpu), Dawn BSD [doğrulanmadı] |
| **Filament v1.80.0** | Metal, Vulkan (MoltenVK), GL, WebGPU [doğrulandı] | Vulkan, GL, WebGPU (D3D yok) [doğrulandı] | Çok iyi PBR; su/okyanus yok | Var [doğrulanmadı] | Kullanıcıya açık genel compute API'si yok [doğrulanmadı] | Yok | Offscreen swapchain [doğrulanmadı] | Doğal | Orta (render motoru hazır, sahne/su değil) | Apache-2.0 |
| **OGRE-Next 3.0** (son sürüm 2024-10) | Metal ("highly recommended") [doğrulandı] | D3D11, Vulkan | Terra arazisi; okyanus yok [doğrulanmadı]. Gazebo'nun render katmanı | Var | Compute var [doğrulanmadı] | Yok | Var (Gazebo) | Gazebo örneği | Orta; seyrek sürüm | MIT [doğrulandı] |
| **Magnum** | OpenGL tabanlı (macOS'ta GL 4.1 tavanı) | GL; Vulkan kısmi [doğrulanmadı] | — | — | Mac'te GL compute yok [bilgi] | — | — | — | — | MIT [doğrulanmadı] |
| **The Forge** | Metal, Apple Silicon [doğrulandı] | DX12, Vulkan | Yok | Var | Var | Hibrit RT örnekleri (Win/Mac) [doğrulandı, README] | — | — | Yüksek; şirket odaklı, doküman zayıf [çıkarım] | Apache-2.0 (konsol hariç) |
| **Diligent Engine** | **Metal arka ucu ticari lisanslı**; Mac'te ücretsiz yol MoltenVK [doğrulandı] | D3D11/12, Vulkan, WebGPU | — | — | Var | RT var (D3D12/Vulkan) | — | — | — | Apache-2.0 (Metal hariç) |
| **Doğrudan Metal (metal-cpp) + D3D12/Vulkan** | En iyi Mac erişimi (Metal RT + MPS) | DXR/Vulkan RT | Her şey senin | Tam kontrol | Tam | **Her iki platformda tam RT** ama iki ayrı arka uç | Var | Doğal | **En yüksek** (iki API, iki shader dili) | — |
| **three.js (WebGPURenderer)** | Safari 26 / macOS Tahoe'da WebGPU varsayılan açık [ikincil]; WKWebView durumu doğrulanmadı | WebView2/Chromium | Çok sayıda örnek okyanus | Tarayıcı sürecinden kare çekmek IPC maliyeti ister [çıkarım] | WebGPU compute | Yok | Hayır | Görüntüleyici olarak uygun | Düşük | MIT |

**Değerlendirme [çıkarım]:**
- **Unreal** görsel kalitede tartışmasız önde. Ancak Mac'teki kritik özellikler (HWRT, Nanite) deneysel veya beta. Xcode sürüm uyumsuzluğu resmi olarak belgelenmiş. HoloOcean bile macOS'u desteklemiyor ve Vulkan GPU şart koşuyor. Küçük bir takım için en büyük maliyet motor yükseltmeleri ve iki platformda derleme zinciri bakımı olur. UE seçilirse sonar ve kamera fiziği UE'ye gömülmemeli, ayrı modül olarak yazılmalı. HoloOcean da sonarlarını kendi C++ kodunda tutuyor.
- **Godot 4.7** üç platform sürücüsünü native destekliyor, MIT lisanslı ve C++ ile genişletilebiliyor. Compute shader ve asenkron doku okuma API'si var. Zayıf yanları üç tane:
  - (a) GPU RT Mac'te yok; bu bizim için engel değil, çünkü sonar RT'ye bağlanmayacak.
  - (b) `--headless` kipinde RenderingDevice yok; GPU sensör üretimi için pencereli ama gizli çalışma gerekecek (macOS'ta davranışı prototiplenmeli).
  - (c) Su altı ve okyanus görselliği kendin yazılacak.
- **Özel render** (bgfx veya WebGPU-native) en temiz "yalnız sensör üreten başsız GPU bileşeni"dir. Ama kamera için gerçekçi sahne (PBR, gölge, dokular, varlık hattı) sıfırdan yazılmak zorunda. Küçük takım için bu ancak sonar-GPU modülü için mantıklı.
- **Gazebo/OGRE-Next dersi:** Aynı mimari (sunucu fizik + OGRE-Next render) macOS'ta GUI'si "kararsız" bir ürüne dönüşmüş (dogrulama/02 #4). Motoru Mac'te CI'de sürekli koşturmak şart.

### 2.2 HoloOcean'ın güncel yaklaşımından dersler

Kaynak: HoloOcean 2.4.0 dokümanı ve changelog (resmi doküman deposundan) [doğrulandı].

| Konu | HoloOcean 2.4 | Bizim için ders [çıkarım] |
|---|---|---|
| Sonar uygulamaları | Her sonar türü için üç uygulama: Octree (ilk çalışmada yavaş), **Raycast (CPU, ışın sayısıyla ölçeklenir, "Medium-High" çözünürlük, collision mesh kullanır)**, **GPU ("rendered scene, image-based", "High", dedicated GPU)** | Aynı üçlü katman bizde de mantıklı. CPU raycast deterministik referans, GPU rasterleştirme hızlı kip. Octree'ye gerek yok |
| Açısal örnekleme | Dokümanda, menzil kutularını doldurmak için gereken açısal adımı veren kapalı form var: θ = Θ/2 − arccos(R_max/(R_max−R_res)·cos(Θ/2)) | Işın yoğunluğu menzil çözünürlüğünden türetilmeli. Aksi halde kutu boşlukları "çizgili" yapay görüntü üretir |
| Malzeme | `materials.csv` ile malzeme başına yansıtıcılık (raycast ve GPU sonar) | Malzeme kimliği hem görsel hem akustik özellik taşımalı. Tek kaynak "malzeme kütüphanesi" |
| Multipath | Octree sonarlarda `MultiPath: True, ClusterSize: 5`. Yeni raycast/GPU örneklerinde `MultiPath: False` | Hızlı sonar yollarında multipath henüz yok. Havuz için bunu biz eklemeliyiz |
| Dalga | FFT dalgalar Niagara, CPU+GPU ortak. 4 kaskad; `sea_state` 0–9 rüzgâr hızına eşleniyor. Yüzerlik "mesh based ... an estimation and may not reflect real world"; `wave_force`, `max_influence_depth` düzeltme katsayıları | Render'ın dalgasından fizik türetmek ayar katsayılarına (fudge) yol açıyor. Fizik çekirdeği yetkili olmalı |
| Senkronizasyon | İstemci (Python) ile motor (UE C++) iki semafor ve paylaşımlı bellekle **lockstep**. Motor beklerken pencere donar | Lockstep render için paylaşımlı bellek + semafor kanıtlanmış desen. HIL gerçek zaman kipinde ise render bekletilemez |
| Kamera maliyeti | Kare başı kamera render'ı pahalı; viewport'u kapatmak %40'a kadar kazanç sağlıyor | Kamera hızı (Hz) ve çözünürlüğü sensör tanımında olmalı; görsel izleyici ayrı ve kapatılabilir olmalı |
| Platform | 64-bit Linux/Windows; Vulkan GPU; Epic–GitHub hesap bağlantısı; Docker imajı Ubuntu 24.04 + CUDA 12.9 | Mac desteği hedefse UE tabanlı akademik projelerin hiçbiri referans uygulama sunmuyor |

Ek veri: 2.2.0'da su altı akıntıları, 2.3.0'da derinlik ve semantik kamera, raycast lidar eklendi. 2.4.0 bellek birikimi çökmelerini "özellikle sonarlarla" düzeltti [doğrulandı, changelog].

### 2.3 Donanım ışın izleme: erişim ve olgunluk

| Yol | macOS (Apple Silicon) | Windows | Olgunluk / not | Kaynak |
|---|---|---|---|---|
| **Metal RT API** (MSL intersector, acceleration structure) | Compute ve render pipeline'larında RT: **Apple6+** (M1 = Apple7 dahil). Hareket enterpolasyonlu RT: Apple9 | — | Kararlı API. Donanım RT birimi M3 (Apple9) ile geldi; M1/M2'de aynı API daha yavaş çalışır [çıkarım + ikincil] | Metal Feature Set Tables [doğrulandı]; Apple M3 tech talk [ikincil] |
| **DXR (D3D12)** | — | Var | Olgun; DXR destekli GPU gerekir (RTX 20+, RX 6000+, Arc) [doğrulanmadı, genel bilgi] | — |
| **Vulkan RT** (`VK_KHR_ray_query/pipeline`) | MoltenVK'da **yok** (1.4.2 changelog'unda RT girdisi yok; Godot dokümanı aynısını söylüyor) | Var | Win/Linux olgun; Mac'te yol yok | MoltenVK Whats_New [doğrulandı]; Godot RD [doğrulandı] |
| **WebGPU** | Standartta RT yok | — | Rust wgpu'da deneysel ray query var (dogrulama/02 #6a). wgpu-native C API'sinde bulunamadı. Dawn'da resmi uzantı bulunamadı | wgpu-native `ffi/wgpu.h` [doğrulandı, grep]; Dawn [doğrulanmadı] |
| **Godot RD RT** | Yok | Vulkan ile var, deneysel | 4.7'de altyapı düzeyinde | Godot RD dokümanı [doğrulandı] |
| **Unreal HWRT** | M2+ "Experimental" (Lumen/MegaLights) | Var | Mac deneysel | Epic macOS gereksinimleri [doğrulandı] |
| **Slang** (çapraz shader dili) | Metal hedefinde RT **No** | D3D12/Vulkan Yes | RT shader'ı tek kaynaktan Metal'e indirgenemiyor | Slang target-compatibility [doğrulandı] |
| **Embree (CPU)** | ARM (Apple M1) destekli | x86 destekli; Windows ARM deneysel | Olgun, deterministik çalıştırılabilir [çıkarım] | Embree README [doğrulandı] |

**Sonuç [çıkarım]:** Mac'te donanım RT'ye ulaşmanın tek native yolu doğrudan Metal (metal-cpp) ya da UE'nin deneysel HWRT'si. Windows için DXR veya Vulkan RT ayrıca yazılmalı. Bu, aynı sonar çekirdeğinin iki ayrı RT arka ucu demek. Küçük takım için kabul edilebilir yol şöyle:
- CPU'da Embree (referans, her yerde aynı sonuç),
- GPU'da rasterleştirme + compute (her API'de çalışır).

Donanım RT yalnız "kademe 3" optimizasyonu olarak düşünülmeli.

### 2.4 Sonar simülasyonunda gerçekçilik kademeleri

05 raporundaki T0–T4 kademeleri ve doğrulaması (dogrulama/05) geçerli. Burada eksik olan iki şey ele alınıyor: "azami gerçekçi" modelin ne olduğu ve hangi platformda koştuğu.

#### 2.4.1 Yöntem aileleri

| Aile | Örnek | Neyi doğal üretir | Neyi üretemez | Platform |
|---|---|---|---|---|
| **Rasterleştirme** (sonar bakış açısından derinlik + normal + malzeme kimliği render'ı, ardından compute ile menzil-azimut histogramı) | Stonefish FLS/MSIS/SSS [dogrulama/05 #7]; HoloOcean GPU sonarları; Cerqueira 2017 | Birincil yansıma, akustik gölge (z-buffer), Lambert yoğunluk, menzil kutulama | Multipath (ikinci sekme), saydam veya çok katmanlı saçılım; elevasyon örneklemesi piksel ızgarasıyla sınırlı | Metal/D3D12/Vulkan'da aynı yöntem; RT gerekmez |
| **Işın tabanlı** (ışın başına kesişim + nokta saçıcı) | HoloOcean Raycast; OceanSim (Isaac/RTX); DAVE/Choi 2021 (CUDA) | Gölge, keyfi geometri, ikinci sekme (multipath) eklenebilir, ışın deseni ağırlığı | Işın sayısıyla maliyet artar | CPU (Embree) her yerde; GPU RT yalnız Metal-native veya DXR/Vulkan |
| **Faz-uyumlu nokta saçıcı** (karmaşık genlik toplamı, frekans alanı, IFFT, sinc yan lob) | Choi vd. 2021 (Frontiers); Sensors 2025 (aynı ekip, ROS-Gazebo) | Fiziksel speckle (Rayleigh/Rician), yan loblar, darbe uzunluğu etkisi; tankta hedef ve duvar eko seviyeleri deneyle eşleşti | Yayılım ortamı (ses hızı profili, kırılma) yok | 512 ışında 0.5–10 Hz (RTX 2080 Ti) [dogrulama/05 #6]; CUDA'ya bağlı uygulama |
| **Yayılım modelleri** (ışın/beam tracing, ses hızı profili) | BELLHOP/Acoustics Toolbox [doğrulanmadı, bu oturumda açılmadı] | Ses hızı profili ile kırılma, yüzey/dip sekmeleri, uzun menzil | Görüntü sonarının ayrıntılı saçılımı | CPU; havuz ölçeğinde (<50 m) gereksiz [05 raporu, mühendislik yargısı] |
| **Veri güdümlü / sinirsel** | GAN tabanlı sonar stili aktarımı; ACOUSIM yalnız istatistik doğrulaması | Gerçek görüntüye istatistiksel benzerlik | Fiziksel nedensellik; eğitim verisi gerektirir | Çevrimdışı |

#### 2.4.2 Modellenmesi gereken etkiler ve havuz için önem sırası

| Etki | Fiziksel kaynak | Model önerisi | Havuzda önemi [çıkarım] | Kanıt |
|---|---|---|---|---|
| Akustik gölge | En yakın kesişim | Ray/z-buffer'da kendiliğinden oluşur | **Çok yüksek**: Ping360 havuz deneyinde "en belirgin" etki | Hasan vd. 2024 (Ping360, 3×6×5 m havuz) [doğrulandı] |
| Duvar ve yüzey yansımaları / multipath | Seramik karo duvar, su yüzeyi | 1–2 sekmeli speküler ikincil ışın; yüzeyde ayna (dalgada pürüzlü) | **Çok yüksek**: aynı deneyde yansımalar havuz sınırlarının ötesinde göründü | Hasan vd. 2024 [doğrulandı]; HoloOcean octree MultiPath [doğrulandı] |
| Yakın alan gürültüsü | Dönen kafa ve dönüştürücü | Yaklaşık ilk 0.75–1 m'de gürültü bandı | Yüksek (Ping360) | Hasan vd. 2024 [doğrulandı] |
| Benek (speckle) | Çözünürlük altı saçıcıların faz toplamı | Coherent toplam (Choi) veya Rayleigh/K dağılımı | Orta-yüksek: algılama eşiğini doğrudan etkiler | dogrulama/05 #6, #5 |
| Yan loblar / ışın deseni | Dizi veya piston deseni | Sinc ağırlığı veya ölçülmüş desen | Orta. Güçlü duvar ekosu yan lobdan "hayalet" üretir | Choi 2021 [dogrulama/05]; Ping360 yan lob seviyesi yayımlanmamış [doğrulanmadı] |
| Havuz yankılanması (reverberation) | Kapalı hacimde çoklu sekme | Sönen çok-sekme kuyruğu (istatistiksel) | Orta. Kısa menzilde zemin gürültüsü | Nicel kaynak bulunamadı [doğrulanmadı] |
| Ses hızı profili | Sıcaklık ve tuzluluk | Havuzda sabit c (tatlı su formülü) | Düşük (menzil ölçeğinde %c = %menzil) | 05 raporu |
| Malzeme yansıtıcılığı | Empedans farkı ve pürüzlülük | Malzeme başına A_r ve geliş açısı yasası | **Yüksek, ama değeri bilinmiyor** | HoloOcean materials.csv [doğrulandı] |
| Deniz tabanı geri saçılımı | Sediment, pürüzlülük, geliş açısı | Lambert (μ·sin²θ) veya APL-UW | Açık deniz SSS için yüksek | **APL-UW modeli yalnız 10–100 kHz için geçerli**; 450–750 kHz'de doğrulanmış standart model bulunamadı [ikincil] |
| Hedef kuvveti (TS) | Geometri/dalga boyu oranı | Küre için TS = 10·log(a²/4) (a ≫ λ); 750 kHz'de λ≈2 mm | Hedef tasarımına bağlı | FAO [ikincil]; elastik malzeme farkı var |
| Hareket bozulması | Tarama boyunca araç hareketi | Her ping kendi poz ve zamanıyla | **Çok yüksek** (Ping360 tarama süresi saniyeler mertebesinde) | 05 raporu ve dogrulama/05 |

#### 2.4.3 Sonar türüne göre "azami gerçekçi" model

**Mekanik taramalı (Ping360).** Her açı adımında tek ışın çalışır, bu yüzden en zengin model ucuzdur [çıkarım]:
1. Işın konisi (2°×25°) içinde menzil çözünürlüğüne göre örneklenmiş N ışın atılır (HoloOcean'ın açısal adım formülü). Her ışın ışın deseni ağırlığı taşır.
2. Her kesişim için şunlar hesaplanır:
   - malzeme A_r ve geliş açısı yasası,
   - iki yönlü yayılım ve soğurma (genlik/yoğunluk birim ayrımına dikkat; dogrulama/05),
   - rastgele fazlı karmaşık genlik (saçıcı yoğunluğu yüzey alanıyla orantılı).
3. 1–2 sekmeli speküler ikincil ışınlar: su yüzeyi (o anki dalga alanından) ve havuz duvarları.
4. Karmaşık sinyal, darbe zarfıyla (`transmit_duration`) menzil ekseninde evriştirilir; zarf alınır.
5. TVG ve `gain_setting` uygulanır, ardından doygunluk ve 8-bit kuantalama yapılır. Örnekleme `sample_period` × 25 ns ile olur (05 raporundaki formül).
6. Yakın alan gürültüsü ve zemin gürültüsü eklenir.
7. Her ping o anın araç pozu ve zamanıyla üretilir.

Hesap yükü [tahmin]: ping başına birkaç bin ışın × 2 sekme. Embree ile CPU'da milisaniyenin altında beklenir; ping hızı ise menzille sınırlı (7 m'de ~100 Hz'den az). **Ölçülmeli.**

**İleri bakan görüntüleme sonarı (FLS, ör. Oculus M750d/BlueView P900):**
- Gerçek zamanlı ana kademe: GPU rasterleştirme (Stonefish/HoloOcean GPU) + istatistiksel speckle + azimut şeritleri + menzil normalizasyonu.
- Azami kademe: Choi tipi coherent model, düşük hızda (≤10 Hz) veya çevrimdışı veri üretimi.
- Elevasyon belirsizliği her iki yaklaşımda da kendiliğinden oluşur.
- Multipath için rasterleştirme yetmez; ışın tabanlı ikinci geçiş gerekir.

**Yan taramalı sonar (SSS):**
- Ping başına tek satır üretilir; görüntü aracın ilerlemesiyle oluşur (waterfall). Hesap ucuz.
- Gerçekçiliği belirleyenler: deniz tabanı mikro-pürüzlülüğü ve geri saçılım-geliş açısı ilişkisi, gölgeler, yaw/roll hareket bozulmaları.
- Xie–Bore–Folkesson (2022) türevlenebilir SSS renderer'ı Lambert + Gauss harmanlama kullanıyor ve yalnız SSS görüntüsüyle batimetri yeniden kurabiliyor. Bu, "Lambert + batimetri ağı" düzeyinin gerçek veriyle tutarlı olabildiğine dolaylı kanıt [ikincil].

#### 2.4.4 Gerçek veriyle doğrulanmış olanlar

| Çalışma | Ne doğrulandı | Nicel mi? | Kaynak |
|---|---|---|---|
| Choi vd. 2021 (Gazebo, coherent multibeam) | Benzer ortam görüntüleriyle | **Nitel** ("qualitatively realistic") | Frontiers [ikincil özet + dogrulama/05] |
| Sensors 2025, ray-based multibeam (ROS-Gazebo) | Tank merkez hattında alüminyum çubuk ve tank duvarı tepe eko seviyeleri deneyle eşleşti; görüntünün nicel doğrulaması "gelecek çalışma" | Kısmen | doi:10.3390/s25051516 [ikincil, yayıncı 403] |
| OceanSim 2025 | Sonar: tankta kaya platformu, yan yana görsel. Kamera: renk kartında RGB açısal hata 7.88°/13.78° | Sonar nitel; kamera kısmen nicel | arXiv 2503.01074 [doğrulandı] |
| ACOUSIM 2026 | Sentetik ve gerçek SSS veri kümeleri (KLSG-II, SCTD) arasında yoğunluk ve LBP histogramlarının KL/JS/EMD'si. Uçak sınıfı iyi uyuşuyor, gemi sınıfı kötü (KL 0.18'e karşı 0.92) | Nicel (istatistiksel) | arXiv 2605.19712 [doğrulandı] |
| SEE 2025 (HoloOcean'da tank ikizi + gerçek BlueView P900) | Gerçek alt küme toplandı; sim'den gerçeğe aktarım sonucu raporlanmadı | Hayır | arXiv 2505.15465 [doğrulandı] |
| Hasan vd. 2024 (gerçek Ping360 havuz verisi, simülasyon yok) | Gölge, duvar/yüzey yansıması, yakın alan gürültüsü | Gerçek veri kaynağı | arXiv 2411.05863 [doğrulandı] |

**Sonuç [çıkarım]:** "Gerçek veriyle nicel doğrulanmış, gerçek zamanlı ve havuzda multipath üreten" bir açık kaynak sonar simülatörü bulunamadı. Gerçekçilik iddiası ancak kendi kalibrasyon kayıtlarımızla ve tanımlı metriklerle desteklenebilir.

### 2.5 Su altı kamera gerçekçiliği

#### 2.5.1 Fiziksel modeller

- **Jaffe–McGlamery (Jaffe 1990):** Görüntü = doğrudan + ileri saçılım + geri saçılım bileşenlerinin toplamı. Geri saçılım, ışık kaynağı–kamera geometrisine bağlı bir hacim integrali. Araç lambası ile çalışan sahneler için temel model (05 raporu; künye doğrulanmış).
- **Akkaynak–Treibitz (CVPR 2018) / Sea-thru (CVPR 2019)** [doğrulandı, Sea-thru PDF Denk. 2–7]:
  - I_c = J_c·e^(−β_c^D(v_D)·z) + B_c^∞·(1 − e^(−β_c^B(v_B)·z))
  - v_D = {z, ρ, E, S_c, β}; v_B = {E, S_c, b, β}. Yani **doğrudan sinyal katsayısı menzile ve nesne yansıtıcılığına bağlı**, geri saçılım katsayısı bağlı değil.
  - Geniş-bant katsayılar spektral fiziksel büyüklüklerden integral olarak türetiliyor: Denk. 3–4, S_c(λ), ρ(λ), E(d,λ) = E(0,λ)·e^(−K_d(λ)·d) ve B^∞(λ) = b(λ)E(d,λ)/β(λ).
  - Yazarlar "β^D en çok z'den, β^B en çok su tipi ve aydınlatmadan etkilenir" diyor. Ayrıca katsayılar "görüntüler arasında aktarılamaz" diye uyarıyor.
- **OceanSim'in uygulaması:** Kanal başına sabit β_attn, β_bs, B^∞. Katsayılar GUI'den **elle ayarlanıyor**; Jerlov tipleri kullanılmıyor. Kostik, parçacık ve yapay ışık yok [doğrulandı].
- **Su tipi verisi:** Solonenko & Mobley 2015 (Applied Optics 54(17)) Jerlov su tipleri için 300–700 nm a(λ) ve b(λ) tablosu veriyor. Williamson & Hollins 2022 ölçülmüş değerlerle tamamlıyor [ikincil].

**Çıkarım (azami gerçekçi kamera):** Sahneyi RGB yerine 6–8 spektral bantta işlemek gerekir. Işık zayıflaması bant başına uygulanır (Jerlov veya ölçülmüş havuz IOP'leri), ardından kamera spektral yanıtı S_c(λ) ile RGB'ye entegre edilir. Böylece β^D'nin menzil bağımlılığı **kendiliğinden** oluşur. Maliyet: G-buffer'dan post-process olarak yapılırsa bant sayısı × birkaç ALU işlemi; ihmal edilebilir [tahmin]. Sınırlayan, havuzun gerçek IOP değerlerinin bilinmemesi.

#### 2.5.2 Kamera kademeleri ve gerçek zamanlı uygulanabilirlik

| Kademe | İçerik | Gerçek zamanlı mı? [tahmin] | Not |
|---|---|---|---|
| K1 | Derinlik tamponundan Akkaynak–Treibitz post-process (RGB, sabit β) | Evet, her donanımda | OceanSim düzeyi |
| K1+ | Spektral bant + kamera S_c(λ) + Jerlov/ölçülmüş IOP; menzile bağlı β^D | Evet | Önerilen ilk "gerçekçi" kademe |
| K2 | Araç lambası: ışık konisi ve faz fonksiyonuyla ekran-uzayı ray-march geri saçılımı; ileri saçılım için menzile bağlı PSF bulanıklığı | Yarım çözünürlükte orta sınıf GPU'da evet; M1'de 720p sınırında olabilir | Jaffe–McGlamery'nin gerçek zamanlı yaklaşımı |
| K3 | Asılı parçacıklar (marine snow, lambada parlayan), yüzey kostikleri (dalga alanından ışık projeksiyonu), su yüzeyi Snell penceresi ve toplam iç yansıma, yüzey altından görünen dalga | Kostik/parçacık ucuz; doğru yüzey kırılması orta maliyetli | Havuzda kostik belirgin [05 raporu, doğrulanmadı] |
| K4 | Sensör modeli: Poisson + okuma gürültüsü, otomatik pozlama/beyaz dengesi, hareket bulanıklığı, rolling shutter, lens bozulması, H.264/MJPEG sıkıştırma | Evet (compute) | Algılama algoritmaları için kritik ve ucuz |
| K5 | Yol izleme (path tracing) ile hacimsel tam çözüm | Hayır; çevrimdışı veri üretimi | Doğrulama/kıyas amaçlı |

#### 2.5.3 Sentetik-gerçek farkı (kamera)

- OceanSim ekibinin 2026 çalışması: sentetik veriyle deniz kestanesi tespitinde aktarım çalışıyor, ama "kayda değer bir fark kalıyor". Karada tasarlanmış saf domain randomization çoğu zaman yetmiyor [ikincil, arXiv 2609.20680 arama özeti; tam metin okunmadı].
- Sonar için de benzeri bildiriliyor: SimWave SSS veri kümesinde yalnız sentetikle eğitilen dedektör gerçek veride kesinlik ve duyarlılık kaybediyor; karma veriyle eğitim daha iyi [ikincil].
- **Çıkarım:** Algılama ağları için üç kural:
  - (1) Sentetik veri yalnız başına kabul ölçütü olamaz.
  - (2) Değerlendirme seti her zaman gerçek görüntülerden oluşmalı.
  - (3) Simülatör, *algoritmanın kapalı döngü davranışını* (yaklaşma, atış kararı) sınamak için kullanılmalı. Ağın nihai başarımı gerçek veride ölçülmeli.

### 2.6 Fizik ve görüntünün tutarlılığı

1. **Tek kaynak dalga alanı [çıkarım].**
   - Fizik çekirdeği spektrumu (JONSWAP/PM, yönsel yayılım) ve tohumu sahiplenir; ayrık bileşen kümesini (k_i, A_i, φ_i) üretir. Render sürecine bu küme veya onu yeniden üretecek parametreler + sim zamanı t gönderilir.
   - Render FFT ızgarası aynı k-ızgarasını kullanırsa GPU FFT ile çekirdeğin noktasal doğrudan toplamı (DFT değerlendirmesi) matematiksel olarak aynı seridir.
   - Fark yalnız kayan nokta hassasiyetinden (GPU f32, çekirdek f64) gelir.
   - 04 raporundaki "Gerstner'i yalnız render'da kullanmak tutarsızlık yaratır" uyarısı burada da geçerli.
2. **Choppy yer değiştirme tuzağı.** Tessendorf'ta yüzey yatay olarak x + D(x,t) konumuna ötelenir; D, ik/|k| ile çarpılmış spektrumdan gelir. Jakobyen negatifse yüzey kendi içine kıvrılır [ikincil, Tessendorf notları]. Render choppiness kullanırsa (x,y)'deki yükseklik artık ζ(x,y) değildir. Çekirdek bunu ya iteratif ters ötelemeyle hesaplamalı ya da fizik ve render aynı lineer (choppy olmayan) yüzeyi kullanmalı. Yalnız birinde choppiness olması yüzerlik ile görüntünün ayrışmasına yol açar.
3. **Yüzerlik ağı ile görsel ağın eşleşmesi.** Hidrostatik ağ kapalı ve düşük çokgenli olmalı. Gerçek deplasmanı temsil etmeli: su dolan boşluklar hariç, basınç gövdeleri dahil (dogrulama/04 ek bulgusu). Görsel ağ ise ayrıntılıdır. İkisi aynı gövde çerçevesi ve orijini paylaşmalı. CI testleri:
   - hidrostatik ağ hacmi ve hacim merkezi ile CAD değerleri arasındaki fark eşik altında olmalı,
   - render'dan okunan su hattı ile çekirdeğin hesapladığı batmış hacmin su hattı aynı pozda karşılaştırılmalı.
4. **Yüzey geçişinde görsel tutarlılık.**
   - Kamera yarı batıkken iki ortam aynı karede görünür: menisküs, su damlası, Snell penceresi. Su altı post-process'i, kamera pikseli bazında dalga yüzeyinin altında/üstünde maskesiyle uygulanmalı [çıkarım].
   - Sonar yüzeye yakınken o anki dalga yüzeyinden speküler yansıma alır. Fizikteki dalga alanı sonar ışınlarının da yüzeyi olmalı.
   - GNSS anten maskesi ve DVL ışınlarının havaya çıkması da aynı yüzeyi kullanmalı (05 raporu).
5. **HoloOcean karşı-örneği:** Render tarafında üretilen FFT dalgasından yüzerlik türetiliyor ve `wave_force`/`max_influence_depth` ile ayarlanıyor; dokümanın kendisi bunun bir tahmin olduğunu söylüyor [doğrulandı]. Fizik çekirdeğini yetkili yapmak bu ayar katsayılarını ortadan kaldırır.
6. **Unreal Water plugin seçeneği:** UE Gerstner dalgalarını CPU'dan sorgulatabiliyor (`GetWaveInfoAtPosition`, `bSimpleWaves`) [doğrulandı, API]. Bu, motorun yetkili olduğu yöne itiyor. Biz tersini istiyoruz: dalgayı çekirdek üretmeli, UE yalnız çizmeli.

### 2.7 Dünya ve ortam gerçekçiliği

- **Havuz modeli:** Ölçülü geometri (duvar, merdiven, tahliye ızgarası, şerit halatları), karo malzemesi (akustik: yüksek yansıtıcılık; Hasan 2024 seramik karoyu "yansıtıcı" buldu), su yüzeyi (yarışma sırasında dalgacık), aydınlatma (tavan lambaları veya güneş + kostik), klorlu suyun renk tonu [doğrulanmadı]. Gerçekçiliği en çok artıracak şey **hedef havuzun lazer metre veya fotogrametri ile ölçülmüş ikizi**dir [çıkarım].
- **Açık deniz:** Batimetri ızgarası (yükseklik haritası + ayrıntı gürültüsü), sediment sınıfına göre akustik geri saçılım, Jerlov su tipi, ışık zayıflaması (K_d), asılı parçacık yoğunluğu.
- **Hedefler:** Her hedef için tek bir "malzeme kaydı" tutulmalı:
  - görsel PBR (albedo, pürüzlülük),
  - akustik (A_r, geliş açısı yasası, gerekirse TS),
  - optik (spektral yansıtıcılık).

  Kayıt hem render'a hem sonar çekirdeğine aynı kimlikle verilmeli (HoloOcean materials.csv deseni).
- **Torpido ve hedef etkileşimi:** Torpido kabarcık izi kamera ve sonarda görünür. Bunun modellenmesi için kaynak bulunamadı [doğrulanmadı]; ileri kademe.

### 2.8 Gerçekçiliğin ölçülmesi ve açık veri kümeleri

**Metrikler:**

| Alan | Metrik | Kaynak / not |
|---|---|---|
| Sonar, piksel/eko düzeyi | Bilinen hedef ve duvar için tepe eko seviyesi farkı (dB); SNR–menzil eğrisi; gölge uzunluğunun geometriyle uyumu | Sensors 2025 [ikincil] |
| Sonar, istatistik | Yoğunluk histogramı ve LBP doku histogramı için KL/JS/EMD; speckle dağılım parametresi (Rayleigh/K şekli) uydurması | ACOUSIM [doğrulandı] |
| Sonar, görev | Aynı senaryoda P_d/P_fa (ROC) farkı; sim'de eğit–gerçekte test et farkı | SimWave [ikincil] |
| Kamera | Menzili bilinen renk kartında RGB açısal hata; β^D(z) ve β^B uydurma farkı; tespit mAP farkı | OceanSim [doğrulandı]; Sea-thru [doğrulandı] |
| Dinamik | Serbest sönüm periyodu, limit hız, adım yanıtları; serbest manevrada yörünge RMSE; dalga yükünde normalize hata | 04 raporu ve dogrulama/04 (Aracri: hata değerleri kısmen doğrulandı) |

**Açık veri kümeleri:**

| Veri kümesi | İçerik | Kullanım | Kaynak / durum |
|---|---|---|---|
| Ping360 havuz kayıtları (Hasan vd. 2024) | 3×6×5 m havuz, 7 m menzil, 1200 örnek, PVC boru ve kova hedefler, 10 senaryo | **Ping360 modelimizin doğrudan kıyası** (gölge, duvar yansıması, yakın alan gürültüsü) | github.com/junayed/Sonar-Scan-Dataset-with-Ping-360 [doğrulandı, makalede]; lisans doğrulanmadı |
| Underwater Caves (Mallios vd. IJRR 2017) | İki MSIS (Tritech Micron, SeaKing) + DVL + 2 IMU + derinlik; CSV ve ROS bag | MSIS modeli + seyrüsefer füzyonu | cirs.udg.edu, Zenodo [ikincil] |
| Marine Debris FLS (Valdenegro-Toro; 2021 ve 2025) | ARIS Explorer 3000; su tankı, döner tabla, su basmış taş ocağı | FLS görüntü istatistiği, hedef tespiti | GitHub (2021), Zenodo 10.5281/zenodo.15101686 (2025) [ikincil] |
| SEE (2025) | HoloOcean tank ikizi + gerçek BlueView P900 (BlueROV2 Heavy), 64.7 GB | Sim–gerçek FLS kıyası için hazır ikiz | github.com/guilhermecdo/SEE [doğrulandı] |
| KLSG-II, SCTD | Gerçek SSS görüntüleri | SSS istatistik doğrulaması | ACOUSIM'de kullanıldı [doğrulandı] |
| SQUID (Berman, Treibitz) | 57 stereo çift, renk kartları, 4 saha | Kamera zayıflama/geri saçılım modeli kalibrasyonu | Haifa Viseaon [ikincil] |
| FLSea (Randall, Treibitz 2023) | İleri bakan stereo + VI, fotogrametri derinlik doğruluğu | Kamera modeli ve VIO | arXiv 2302.12772 [ikincil] |
| AQUALOC | Kamera + IMU + basınç | VIO kıyası | IJRR 2019 [ikincil] |
| BlueROV2 tether veri kümesi | Hareket yakalama pozu + MAVLink (IMU, RC çıkışı/PWM) | Dinamik model kıyası (ip etkisi var) | PMC12808086 [ikincil] |
| Gabl vd. 2021 | Sabitlenmiş BlueROV2 üzerinde akıntı ve dalga yükleri | Dalga ve sürükleme yükü doğrulaması | 04 raporu |
| SOLAQUA (SINTEF) | BlueROV2, DVL + IMU, ağ kafes ortamı | Seyrüsefer | arXiv 2504.01790 [ikincil] |

### 2.9 Eş-simülasyon mimarisi ve zaman

**Bileşenler [çıkarım]:**
- `sim-core` (C/C++, başsız, yetkili): fizik, dalga, sonar (CPU/Embree), seyrüsefer sensörleri, olay kuyruğu (t_measure, t_deliver; 05 raporu).
- `sim-render` (ayrı süreç; Godot veya UE veya özel): yalnız görüntü, kamera ve GPU sonarı. Kendi fiziği kapalıdır.
- `hil-bridge`: STM32 ile USB/UART bağlantısı.
- `companion-bridge`: kamera karelerini yardımcı bilgisayara veya PC'deki görüntü işleme sürecine gönderir.

**Akış:**
1. `sim-core`, t_k anında bir "render isteği" yayımlar. İçeriği: istek kimliği, t_measure (pozlama ortası), kamera/sonar pozları, aktör pozları, dalga parametreleri ve t, ışık durumu.
2. `sim-render` kareyi üretir. Sonucu istek kimliği ve t_measure ile paylaşımlı belleğe yazar ve semaforu tetikler (HoloOcean deseni).
3. `sim-core` veya köprü kareyi t_deliver = t_measure + gecikme modeli (pozlama, ISP, kodlama, iletim) anında teslim eder.

**Kiplere göre davranış:**

| Kip | Render davranışı | Kare geç kalırsa |
|---|---|---|
| Hızlandırılmış SIL | Lockstep: çekirdek render sonucunu bekler; hız render ile sınırlanır. Kamera kapalıyken çekirdek serbest koşar | Bekler (deterministik) |
| Gerçek zamanlı HIL | Render boru hattı 1–2 kare önde çalışır (pozlar zaten bilinir, çünkü render çekirdeğin geçmiş durumunu çizer) | Kare düşürülür, sayaç ve log tutulur; teslim zamanı yalan söylemez |
| Adım eşlemeli HIL | Firmware dış tick kabul ediyorsa lockstep (02 raporu açık sorusu) | Bekler |

**Gecikme bütçesi örneği (30 fps kamera, gerçek zamanlı HIL) [tahmin, ölçülmeli]:**
- render 8–20 ms,
- GPU→CPU geri okuma 1–3 ms (asenkron, 1–2 kare kuyruk),
- kodlama (MJPEG/H.264) 2–10 ms,
- yerel ağ/USB ile yardımcı bilgisayara iletim 1–5 ms.

Toplam yaklaşık 15–40 ms. Bu, gerçek bir USB/IP kameranın tipik gecikmesinden (doğrulanmadı) düşük olmalı ki gecikme modeli ile *yapay olarak artırılabilsin*. Simülatör gerçek kameradan daha hızlı olmalı; daha yavaşsa gerçekçi gecikme ifade edilemez.

**Sonar zamanı:** Sonar karesi hesaplama anında değil, akustik zamanda (2R/c + işlem) teslim edilmeli. Ping360'ta her açı adımı ayrı t_measure taşır (dogrulama/05, "açı başına zamanlanan sensör").

### 2.10 Hesaplama bütçesi ve donanım

Aşağıdaki tablo **ölçülmemiş tahmindir**. Amaç benchmark planına çerçeve vermek. "Apple Silicon dizüstü" = M1/M2 temel çip ile M3 Pro arası; "orta sınıf Windows" = RTX 4060 / RX 7600 sınıfı.

| Bileşen | Apple Silicon dizüstü | Orta sınıf Windows GPU | Dayanak |
|---|---|---|---|
| Fizik çekirdeği 1 kHz + dalga | Gerçek zamanlı (tek çekirdek) | Aynı | 04 raporu bütçesi (doğrulanmadı) |
| Ping360 coherent + 2 sekme (Embree, CPU) | Gerçek zamanlı, rahat | Aynı | Ping başına küçük ışın sayısı [tahmin] |
| FLS 512 ışın, T2 GPU raster + compute, 10–20 Hz | Muhtemel | Rahat | Stonefish/HoloOcean GPU yöntemi; OceanSim RTX A6000'de ~30 FPS (farklı iş yükü) |
| FLS T3 coherent | Hayır (çevrimdışı) | ≤ birkaç Hz | Choi: 0.5–10 Hz (RTX 2080 Ti) |
| Kamera 1280×720 @30, K1+ + K4 | Evet | Evet | Post-process maliyeti düşük |
| Kamera + K2 (lamba geri saçılımı ray-march) + K3 | 720p sınırda; M3 Pro rahat | Evet | [tahmin] |
| UE Lumen HW + Nanite tam kalite | M2+ deneysel/beta; kamera + viewport aynı anda zor | Evet | Epic gereksinimleri; HoloOcean kamera maliyeti tablosu |

**Gerçekçiliği koddan değil bilinmeyen parametreden sınırlanan alanlar:**
- **Sonar:**
  - Hedef ve karo malzemelerinin 750 kHz'deki yansıtıcılığı ve geliş açısı yasası.
  - Ping360'ın ışın deseni ve yan lob seviyesi (üretici yalnız −3 dB genişliğini veriyor).
  - Firmware'in TVG/otomatik kazanç eğrisi.
  - Su yüzeyi pürüzlülüğü.
  - 10–100 kHz üstü için standart deniz tabanı modeli yok.

  Bunlar ölçülmeden coherent model bile "doğru görünen ama yanlış genlikli" görüntü üretir.
- **Kamera:**
  - Havuz suyunun spektral a(λ), b(λ) değerleri.
  - Kameranın S_c(λ) eğrisi.
  - Otomatik pozlama ve beyaz dengesi algoritmaları (ISP kapalı kutu).
- **Dinamik:** Eklenmiş kütle, sönüm, yüzey geçişindeki M_A(s) ve D(s). dogrulama/04 bunların deneysel olarak doğrulanmamış yaklaşım olduğunu söylüyor.

---

## 3. Mimari için öneriler

1. **Yetki ayrımı (kesin).** `sim-core` (C/C++) tek gerçek: fizik, dalga, malzeme, sonar (CPU), seyrüsefer sensörleri, zaman. `sim-render` yalnız `sim-core` durumunu çizer, kamera ve GPU sonar karesi üretir; hiçbir fiziksel durumu değiştirmez. Gerekçe: HoloOcean'da render tarafında üretilen dalgaya bağlı yüzerliğin ayar katsayısı gerektirmesi; Gazebo ve Stonefish'in sunucu/görüntü ayrımı (02 raporu).

2. **Render/sensör süreci için motor kararı:**
   - **Birincil öneri: Godot 4.7 + GDExtension (C++), ayrı süreç.**
     - Gerekçe: Metal/D3D12/Vulkan sürücüleri native; MIT; compute (`RenderingDevice`) ve asenkron doku okuma API'si var; editör hafif; küçük takım için öğrenme ve bakım yükü UE'den çok düşük.
     - Riskler: başsız kipte RD yok (gizli pencere gerekebilir); Mac'te RT yok (sonar için gerek yok); okyanus ve su altı görselliği kendimiz yazılacak.
     - **Kabul prototipi (2 hafta):**
       - (a) M1/M2 Mac ve Windows'ta 720p@30 kamera karesi + asenkron okuma + paylaşımlı bellek,
       - (b) RD compute ile FLS raster-sonar histogramı,
       - (c) pencere gizliyken render'ın sürmesi.
   - **Alternatif: Unreal Engine 5.x.** Ancak şu koşullarda: ekipte UE deneyimi varsa, kamera görüntü gerçekçiliği birinci öncelikse ve Mac'te (Xcode sürüm kısıtı dahil) aynı prototip geçiyorsa. Yükseltmeleri sabit bir motor sürümünde dondur. Sonar ve kamera fiziği UE'den bağımsız modül olsun.
   - **Özel render (bgfx veya WebGPU-native)** yalnız "başsız GPU sonar/kamera üreticisi" olarak, Godot/UE başsız kipte yetersiz kalırsa devreye girsin. Tüm sahneyi sıfırdan yazmayı hedeflemeyin.
   - **three.js** yalnız laboratuvar arayüzündeki izleyici için (03 raporu kapsamı). Sensör üretimi için kullanılmasın.

3. **Sonar yol haritası:**
   - **S1:** T0 (semantik) + Ping1D/Ping360 için Embree ile CPU raycast. Lambert, malzeme A_r, gölge, Rayleigh speckle; per-ping poz/zaman; Ping protokol kodlayıcısı.
   - **S2:** Ping360 için coherent toplam + darbe evrişimi + ışın deseni + 1–2 sekmeli multipath (havuz duvarı/yüzey) + yakın alan gürültüsü + TVG.

     **Hasan 2024 havuz verisiyle kıyas** yapılsın: gölge uzunluğu, duvar ve yüzey yansımalarının konumu, histogram KL.
   - **S3:** FLS/SSS için GPU rasterleştirme (render sürecinde) + istatistiksel speckle + azimut şeritleri. Doğrulama SEE ve Marine Debris FLS verisiyle (histogram, LBP KL/JS/EMD).
   - **S4:** FLS'te ışın tabanlı ikinci sekme (CPU Embree veya platforma özel RT). Coherent FLS çevrimdışı veri üretimi.

   Her kademe aynı `Sensor` arayüzünü ve malzeme kaydını kullansın.

4. **Kamera yol haritası:**
   - **C1:** K1+ (spektral bant + Jerlov/ölçülmüş IOP + S_c(λ)) ve K4 (sensör gürültüsü, pozlama, sıkıştırma). Kalibrasyon: hedef havuzda 0.5–5 m arası menzili bilinen renk kartı çekimi; β^D(z), β^B, B^∞ uydurması (SQUID/Sea-thru yöntemi).
   - **C2:** Araç lambası geri saçılımı (ray-march) ve ileri saçılım PSF'i.
   - **C3:** Kostik, parçacık, yüzey kırılması ve yarı batık kamera.
   - **C4:** Çevrimdışı yol izleme ile referans görüntüler.

5. **Dalga tutarlılığı:**
   - Spektrum ve tohum çekirdekte tutulsun; render'a bileşen kümesi + t gönderilsin.
   - Render FFT ızgarası çekirdekle aynı k-ızgarasını kullansın.
   - Choppiness ya her ikisinde olsun (çekirdekte ters öteleme ile) ya hiçbirinde.
   - CI testi: render'dan geri okunan prob noktası yükseklikleri ile çekirdek değerleri karşılaştırılsın; eşik mm mertebesi [çıkarım].

6. **Malzeme kaydı:** Tek dosya (sürümlü). Her malzeme için PBR + spektral yansıtıcılık + akustik A_r/açı yasası + kaynak/ölçüm notu. Bilinmeyen değerler "tahmini" bayrağıyla işaretlensin; rapor üretilirken gösterilsin.

7. **Zaman ve kare protokolü:**
   - Her render isteği ve sonucu `request_id`, `t_measure`, `t_rendered_wall`, `t_deliver` taşısın.
   - Gerçek zamanlı HIL'de geç kare düşürülsün ve sayılsın.
   - SIL'de lockstep.
   - Paylaşımlı bellek + semafor, iki işletim sisteminde native API ile (POSIX shm / Windows file mapping) [çıkarım].

8. **Doğrulama paketi baştan:**
   - Gerçek Ping360 havuz kaydı alınana kadar Hasan 2024 verisi kullanılsın.
   - Kamera için renk kartı çekimi yapılsın.
   - Dinamik için 04 raporundaki testler uygulansın.
   - Metrikler (eko dB farkı, histogram KL, RGB açısal hata, yörünge RMSE) her sürümde otomatik raporlansın.

**Kademeli yol haritası (özet):**

| Faz | Sonar | Kamera | Render/motor | Hedef donanım |
|---|---|---|---|---|
| F0 | T0 semantik | Yok veya basit izleyici | Laboratuvar izleyicisi (three.js/Godot) | Hepsi |
| F1 | S1 (Embree CPU, Ping360/Ping1D) | C1 (K1+ + K4) | Godot render süreci, lockstep | M1+ / orta sınıf |
| F2 | S2 (coherent + multipath Ping360) + gerçek veri kıyası | C2 (lamba) | Dalga tutarlılık testi, yüzey geçişi | M1+ / orta sınıf |
| F3 | S3 (GPU raster FLS/SSS) | C3 (kostik, parçacık, yüzey) | Gerekirse başsız GPU üretici | M2 Pro+ / orta sınıf |
| F4 | S4 (multipath FLS, coherent çevrimdışı) | C4 (yol izleme referansı) | İsteğe bağlı platforma özel RT | Windows RTX veya M3+ |

---

## 4. Riskler ve açık sorular

**Riskler**
- **Godot başsız GPU:** `--headless` RenderingDevice'ı kapatıyor. Gizli pencereli render macOS'ta App Nap veya pencere görünmezken kare kısma ile yavaşlayabilir [doğrulanmadı]. Prototiple ölçülmeli.
- **Unreal Mac kırılganlığı:** Resmi dokümanda Xcode 26.4 uyumsuzluğu; HWRT deneysel. Akademik UE simülatörlerinin hiçbiri Mac desteklemiyor.
- **RT soyutlaması yok:** Mac ve Windows için ortak C++ RT yolu bulunmadı (MoltenVK yok, Slang→Metal RT yok, WebGPU C API yok). RT gerektiren her özellik iki kez yazılır.
- **Nicel doğrulama eksikliği:** Literatürde gerçek veriyle nicel doğrulanmış gerçek zamanlı sonar simülatörü az. "Gerçekçi" iddiası kendi ölçümlerimize bağlı.
- **Bilinmeyen parametreler** (bkz. 2.10): En pahalı kademeler ölçüm olmadan yanlış hassasiyet izlenimi verebilir.
- **Sim'den gerçeğe aktarım:** Sentetik veriyle eğitilen algılama ağları gerçek veride düşüş gösteriyor (OceanSim 2026, SimWave). Simülatör ağ kabulünde tek ölçüt olmamalı.
- **03 raporuyla etkileşim:** Rust dışlandığı için Tauri tabanlı laboratuvar kabuğu (Tauri çekirdeği Rust'tır) yeniden değerlendirilmeli. Bu rapor kapsamı dışında ama motor seçimiyle birlikte karar verilmeli [çıkarım].
- **Lisans:** UE EULA ile GPL kodun birleştirilmesi ve dağıtım koşulları ayrıca incelenmeli (dogrulama/02 #21).

**Açık sorular**
1. Yarışma havuzunun boyutları, duvar malzemesi ve su durumu biliniyor mu? Ölçüm ve kayıt alınabilir mi?
2. Gerçek araçta hangi sonar(lar) olacak: yalnız Ping360 mı, FLS de var mı? FLS yoksa S3 ertelenebilir.
3. Kamera modeli ve ISP ayarları (otomatik pozlama sabitlenebilir mi?) ve S_c(λ) eğrisi temin edilebilir mi?
4. Görüntü işleme yardımcı bilgisayarda mı, PC'de mi? Kare taşıma protokolü (RTSP/UDP/shm) ne olacak?
5. Ekipte Unreal veya Godot deneyimi var mı?
6. Geliştirme Mac'leri hangi çip (M1/M2'de donanım RT birimi yok)?
7. Firmware dış tick ile adım eşlemeyi kabul ediyor mu (02 raporu açık sorusu)?

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **HoloOcean 2.4.0 (2026-11-03):** imaging, profiling, sidescan ve singlebeam sonarlar için Raycast (CPU) ve GPU ("rendered scene, image-based") uygulamaları; malzeme bağımlılığı (`materials.csv`); FFT dalgaları ve mesh tabanlı yüzerlik ("an estimation and may not reflect real world buoyancy"). Raycast/GPU örnek yapılandırmalarında `MultiPath: False`. Platform: 64-bit Linux/Windows, Vulkan GPU. Kaynaklar: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/changelog/changelog.html , https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/sensors/sensors/imaging-sonar-sensor.html , https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/env_appearance/waves/waves.html , https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/usage/installation.html
2. **Metal RT API'si Apple6+ ailesinde (M1 = Apple7 dahil) compute ve render pipeline'larında destekli. Hareket enterpolasyonlu RT Apple9.** Kaynak: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf . Donanım RT biriminin M3 ile geldiği: https://developer.apple.com/videos/play/tech-talks/111375
3. **MoltenVK'da Vulkan ray tracing yok** (1.4.2, 2026-07-20 dahil changelog'da RT girdisi yok). **Godot RD'de RT ve ray query "yalnız Vulkan; macOS/iOS'ta MoltenVK sınırları nedeniyle desteklenmiyor"**. Ayrıca "RenderingDevice headless kipte kullanılamaz". Kaynaklar: https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/Whats_New.md , https://docs.godotengine.org/en/stable/classes/class_renderingdevice.html
4. **Unreal (5.8/5.9 dokümanı) macOS:** Sonoma 14.5+; Lumen HWRT + MegaLights M2+ "Experimental"; Nanite/VSM M2+ "Beta"; "Xcode 26.4 is not compatible". Kaynak: https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine
5. **Embree ARM CPU'ları Linux ve macOS'ta (Apple M1) destekliyor; Windows ARM deneysel; Apache-2.0; v4.4.1.** Slang'in Metal hedefi RT desteklemiyor. Kaynaklar: https://github.com/RenderKit/embree (README), https://github.com/shader-slang/slang/blob/master/docs/target-compatibility.md
6. **Sea-thru/Akkaynak–Treibitz:** I_c = J_c·e^(−β^D(v_D)·z) + B^∞(1−e^(−β^B(v_B)·z)); v_D = {z, ρ, E, S_c, β}, v_B = {E, S_c, b, β}; β^D en çok z'den etkileniyor. OceanSim ise kanal başına sabit katsayıları elle ayarlıyor ve kamera için yalnız renk kartı açısal hatası (7.88°/13.78°) raporluyor. Kaynaklar: https://openaccess.thecvf.com/content_CVPR_2019/papers/Akkaynak_Sea-Thru_A_Method_for_Removing_Water_From_Underwater_Images_CVPR_2019_paper.pdf , https://arxiv.org/html/2503.01074v2
7. **Gerçek Ping360 havuz deneyi:** 3×6×5 m havuz, 7 m menzil, 1200 örnek. Akustik gölge en belirgin etki; seramik karo duvar ve yüzey yansımaları sınırların ötesinde görünüyor; ilk ~0.75 m'de dönüş kaynaklı gürültü; veri GitHub'da. Kaynak: https://arxiv.org/html/2411.05863v1
8. **APL-UW yüksek frekans deniz tabanı geri saçılım modeli yalnız 10–100 kHz için geçerli** (Ping360'ın 750 kHz'i aralık dışında). Kaynaklar: https://arxiv.org/pdf/physics/0607146 , https://apps.dtic.mil/sti/pdfs/ADB199453.pdf
9. **ACOUSIM (2026):** Sentetik ve gerçek SSS (KLSG-II, SCTD) arasında yoğunluk KL değerleri uçak sınıfı için 0.18–0.25, gemi sınıfı için 0.54–0.92. Bu bir dağılım-düzeyi doğrulamadır, fiziksel doğrulama değildir. Kaynak: https://arxiv.org/html/2605.19712v1
10. **Diligent Engine'in Metal arka ucu ticari lisanslı; Filament'te D3D arka ucu yok (Windows'ta Vulkan/GL/WebGPU); O3DE macOS desteği "deneysel".** Kaynaklar: https://github.com/DiligentGraphics/DiligentEngine (README), https://github.com/google/filament (README), https://www.docs.o3de.org/docs/welcome-guide/supported-platforms

---

## 6. Kaynaklar

**Projedeki önceki belgeler**
- 02-mevcut-simulatorler.md ve dogrulama/02-codex.md
- 04-hidrodinamik.md ve dogrulama/04-codex.md
- 05-sensor-modelleme.md ve dogrulama/05-codex.md

**HoloOcean**
- Changelog 2.4.0: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/changelog/changelog.html
- Sonarlar: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/sensors/sensors/imaging-sonar-sensor.html , …/sidescan-sonar-sensor.html , …/profiling-sonar-sensor.html , …/singlebeam-sonar-sensor.html
- Malzeme bağımlılığı: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/sensors/docs/material-dependency.html
- FFT dalgalar: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/env_appearance/waves/waves.html
- Performans: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/usage/performance.html
- İstemci–motor senkronizasyonu: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/usage/semaphores.html
- Kurulum: https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/usage/installation.html , https://raw.githubusercontent.com/byu-holoocean/holoocean-docs/main/v2.4.0/develop/develop.html

**Motorlar ve grafik API'leri**
- Unreal macOS gereksinimleri: https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine
- UE Water API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/Water/UWaterBodyComponent/GetWaveInfoAtPosition , https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/WaterBody/GetWaterSurfaceInfoatLocation
- UE `-RenderOffscreen` (forum, ikincil): https://forums.unrealengine.com/t/rendering-in-headless-mode-possible/475619
- UE lisans (ikincil): https://www.unrealengine.com/en-US/faq , https://pcgamer.com/unreal-engine-games-no-longer-owe-royalties-on-their-first-dollar1m-in-revenue
- AirSim harici fizik (ikincil): https://microsoft.github.io/AirSim/settings/
- Godot RenderingDevice: https://docs.godotengine.org/en/stable/classes/class_renderingdevice.html
- Godot renderer'lar: https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html
- Godot komut satırı: https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html
- Godot 4.6/4.7 (ikincil): https://godotengine.org/releases/4.6/ , https://godotengine.org/releases/4.7/ , https://phoronix.com/news/godot-4.7-dev-1-vulkan-rt
- O3DE platformlar: https://www.docs.o3de.org/docs/welcome-guide/supported-platforms
- MoltenVK: https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/Whats_New.md
- Metal Feature Set Tables: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf ; Apple M3 GPU tech talk: https://developer.apple.com/videos/play/tech-talks/111375
- wgpu-native: https://github.com/gfx-rs/wgpu-native (ffi/wgpu.h) ; Dawn: https://dawn.googlesource.com/dawn
- bgfx: https://github.com/bkaradzic/bgfx ; Filament: https://github.com/google/filament ; OGRE-Next: https://github.com/OGRECave/ogre-next ; Magnum: https://github.com/mosra/magnum
- Diligent Engine: https://github.com/DiligentGraphics/DiligentEngine ; The Forge: https://github.com/ConfettiFX/The-Forge
- Embree: https://github.com/RenderKit/embree ; Slang: https://github.com/shader-slang/slang/blob/master/docs/target-compatibility.md
- WebGPU uygulama durumu (ikincil): https://github.com/gpuweb/gpuweb/wiki/Implementation-Status , https://web.dev/blog/webgpu-supported-major-browsers

**Sonar**
- Hasan vd. 2024, Ping360 havuz deneyi: https://arxiv.org/html/2411.05863v1 ; veri: https://github.com/junayed/Sonar-Scan-Dataset-with-Ping-360
- ACOUSIM 2026: https://arxiv.org/html/2605.19712v1
- Ray-based multibeam (Sensors 2025): https://doi.org/10.3390/s25051516
- Choi vd. 2021: https://www.frontiersin.org/articles/10.3389/frobt.2021.706646/full
- OceanSim: https://arxiv.org/html/2503.01074v2
- SEE veri kümesi: https://arxiv.org/html/2505.15465v1 , https://github.com/guilhermecdo/SEE
- Cerqueira 2020: https://arxiv.org/pdf/2001.03539v1
- Xie, Bore, Folkesson 2022 (türevlenebilir SSS): https://arxiv.org/abs/2206.07821
- APL-UW el kitabı: https://apps.dtic.mil/sti/pdfs/ADB199453.pdf ; geçerlilik aralığı: https://arxiv.org/pdf/physics/0607146
- Hedef kuvveti (ikincil): https://www.fao.org/4/X5818E/x5818e06.htm
- Marine Debris FLS: https://arxiv.org/abs/2503.22880 , https://arxiv.org/abs/2108.06800
- Underwater Caves: https://cirs.udg.edu/?p=2373 , https://zenodo.org/record/7828404
- SimWave (ikincil): https://ceur-ws.org/Vol-4017/paper_32.pdf

**Kamera ve optik**
- Akkaynak & Treibitz 2018: https://openaccess.thecvf.com/content_cvpr_2018/html/Akkaynak_A_Revised_Underwater_CVPR_2018_paper.html
- Sea-thru 2019: https://openaccess.thecvf.com/content_CVPR_2019/papers/Akkaynak_Sea-Thru_A_Method_for_Removing_Water_From_Underwater_Images_CVPR_2019_paper.pdf
- Solonenko & Mobley 2015, Jerlov IOP: https://dx.doi.org/10.1364/ao.54.005392
- OceanSim sentetik veri ölçekleme 2026: https://arxiv.org/abs/2609.20680
- SQUID / Haifa veri kümeleri: https://www.viseaon.haifa.ac.il/datasets ; FLSea: https://arxiv.org/abs/2302.12772
- AQUALOC: https://w3.onera.fr/copernic/sites/default/files/2019_ijrr_aqualoc_underwater_dataset_visual_inertial_pressure_localization.pdf

**Dalga**
- Tessendorf, Simulating Ocean Water: https://jtessen.people.clemson.edu/reports/papers_files/coursenotes2004.pdf
- GodotOceanWaves (ikincil): https://github.com/sebbasim/GodotOceanWaves

**Dinamik veri kümeleri**
- BlueROV2 tether veri kümesi: https://www.ncbi.nlm.nih.gov/pmc/articles/PMC12808086/
- SOLAQUA: https://arxiv.org/html/2504.01790
- Gabl vd. 2021: https://ira.lib.polyu.edu.hk/bitstream/10397/92240/1/Gabl_Hydrodynamic_Loads_Restrained.pdf
