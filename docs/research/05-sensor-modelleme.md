# 05 — Sensör Modelleme (sonar ağırlıklı)

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Ayrıca dil kararı C/C++ olmadan önce, Rust varsayımıyla yazıldı; Rust'a özgü öneriler geçersizdir. Doğrulaması: [dogrulama/05-codex.md](dogrulama/05-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

> Kapsam: tek hibrit (yüzey + dalış) insansız deniz aracı için HIL/SIL laboratuvarında sensör simülasyonu.
> Hedef donanım: STM32U5A9J-DK (geçici). Simülatör: Rust, macOS + Windows native.
> Tarih: 2026-11-10. Etiketler: **[doğrulandı]** = birincil kaynakta (datasheet, resmi doküman, kaynak kod, makale) okundu; **[ikincil]** = yalnızca bayi/özet kaynakta görüldü; **[doğrulanmadı]** = bilgi birikimime dayanıyor veya hesaplama/tahmin, kontrol edilmeli.

---

## 1. Özet

1. **Sonar sınıfları çok farklı veri hacmi üretir.** Tek ışınlı iskandil (Ping, 115 kHz, 25° ışın) ve mekanik taramalı sonar (Ping360, 750 kHz, 2°×25°) UART üzerinden "Ping Protocol" ile birkaç kB/s üretir. İleri bakan çok ışınlı görüntüleme sonarı (Oculus M750d: 512 ışın, en çok 40 Hz) Ethernet üzerinden MB/s düzeyinde veri üretir. Bu yüzden görüntüleme sonarı işleme işi MCU'ya değil yardımcı bilgisayara (companion) aittir.
2. **Sonar simülasyonunda 5 doğruluk kademesi var.** Kademeler sırasıyla şunlar:
   - (T0) geometrik "hedef tespiti" çıktısı,
   - (T1) ışın başına CPU ray-cast + histogram,
   - (T2) GPU rasterize/ray-trace + histogram + istatistiksel speckle (Stonefish, HoloOcean, OceanSim),
   - (T3) faz-uyumlu nokta saçıcı modeli (Choi vd. 2021, DAVE),
   - (T4) çok yollu yayılım ve kırılma.

   T2, 512 ışınlı FLS için GPU'da ~30 FPS gerçek zamana ulaşıyor (OceanSim). T3 aynı ışın sayısında RTX 2080 Ti'de 0.5–10 Hz arasında kalıyor.
3. **Ping360 ve Ping1D için T1 yeterli ve ucuz.** Ping360'ta her açı adımı ayrı bir ping'dir. 360°'lik tarama 50 m'de 33 s sürer. Bu nedenle tarama sırasındaki araç hareketinin (her ışının kendi poz ve zamanıyla) simüle edilmesi gerçekçilik açısından speckle'dan daha önemlidir.
4. **Havuz ortamı (yarışma) çok yollu yayılım için en kötü durumdur.** Beton duvarlar, su yüzeyi ve havuz tabanı güçlü yansıtıcıdır. T4 en azından basit bir "ayna yansıması" (yüzey/duvar tek sekme) olarak yol haritasına alınmalıdır.
5. **DVL'de (Water Linked A50) güncelleme hızı irtifaya göre değişir.** Hız 2–15 Hz arasındadır ve `velocity_valid` / `beam_valid` bayrakları ile `time_of_validity` damgası vardır. UART protokolü (115200 8N1, CRC-8) kart üzerinde birebir taklit edilmeye çok uygundur.
6. **IMU modeli standart forma oturtulmalı.** Model şu bileşenlerden oluşur:
   - beyaz gürültü + bias random walk / Gauss-Markov (Kalibr/PX4 formu),
   - açılışta rastgele bias, ölçek ve eksenler arası hata, sıcaklık katsayısı.

   ICM-42688-P datasheet değerleri: gyro 0.0028 °/s/√Hz, ZRO ±0.5 °/s, ZRO sıcaklık kayması ±0.005 °/s/°C, eksenler arası ±%1.25. Bias kararsızlığı datasheet'te **yok**.
7. **GNSS sinyali ~3 cm su altında kaybolur.** Model şu öğeleri içermeli:
   - anten derinliğine ve dalga taşmasına bağlı kayıp,
   - yüzeye çıkınca hot-start süresi (NEO-M9N: 2 s, cold: 24 s),
   - fix tipi geçişi,
   - Gauss-Markov konum hatası (PX4'teki gibi τ≈60 s) ve ~120 ms gecikme.
8. **Kamera için Jaffe-McGlamery modelinin sadeleştirilmiş, derinlik tamponuna dayalı hali yeterli.** Formül I = J·e^(−β_attn·d) + B∞(1−e^(−β_bs·d)) (Akkaynak-Treibitz 2018, OceanSim'de GPU'da) gerçek zamanlı çalışır. Aracın kendi lambasından gelen geri saçılım ve ileri saçılım bulanıklığı ikinci kademede eklenebilir.
9. **Arıza enjeksiyonu iki katmanda ayrı ayrı yapılmalı:**
   - ölçüm katmanında: bias, drift, ölçek, gürültü artışı, donma, sıçrama, kayıp (Balaban vd. 2009 taksonomisi),
   - iletim katmanında: bayt bozulması, CRC hatası, gecikme, paket kaybı.

   ArduPilot SITL'deki `SIM_*_FAIL`, `*_GLITCH`, `*_RND` parametreleri iyi bir referanstır.
10. **Karta veri aktarımı için karma yaklaşım öneriliyor:**
    - Varsayılan yol (b): USB HS üzerinden "ham ölçüm" HIL mesajları (hızlı, deterministik, ek donanım gerektirmez).
    - UART sensörleri (DVL, Ping1D/Ping360, GNSS UBX/NMEA) için seçmeli olarak (a): gerçek kablo protokolü PC'den USB-UART ile verilir.
    - I²C/SPI sensörleri için (a) ancak ayrı bir "sensör emülatörü" MCU ile mümkündür (sonraki faz).
    - STM32U5A9J-DK'de Ethernet yok, USB-C yalnızca device modunda. Bu yüzden Ethernet sensörleri (Oculus, DVL TCP) kablo seviyesinde emüle edilemez.

---

## 2. Bulgular

### 2.1 Sonar türleri ve küçük araçlarda kullanılan gerçek ürünler

| Tür | Ne ölçer | Örnek ürün ve ana parametreler | Arayüz / veri formatı | Etiket |
|---|---|---|---|---|
| Tek ışınlı iskandil / altimetre | Işın konisi içindeki en güçlü/ilk ekonun mesafesi + ham "profil" (menzil boyunca yoğunluk) | **Blue Robotics Ping Sonar (R2)**: 115 kHz, 25° (−3 dB) ışın, 0.3–100 m, çözünürlük menzilin %0.5'i, 5 V, 100 mA tipik | TTL UART, 115200 (varsayılan) / 9600 baud; Ping Protocol (`distance_simple` #1211: mm + %güven; `profile` #1300: u8[] yoğunluk dizisi + scan_start/length + gain) | [doğrulandı] BR ürün sayfası + ping1d mesaj dokümanı |
| Mekanik taramalı görüntüleme sonarı (MSIS) | Her açı adımında tek ışınlı yoğunluk profili. Adımlar dönerek 2B "fan" görüntüyü oluşturur. | **Blue Robotics Ping360**: 750 kHz; 2° yatay × 25° dikey ışın; 0.75–50 m; menzil çözünürlüğü %0.08 (50 m'de 4.1 cm); 360° tarama süresi 1 m'de 3.4–4.3 s, 50 m'de 33 s; 11–25 V, 5 W | USB, Ethernet (UDP), RS-485; Ping Protocol (`transducer` #2601 komutu, `device_data` #2300 yanıtı). Açı birimi **gradyan (0–399)**, `sample_period` 25 ns adımlı, `number_of_samples` 200–1200, frekans 500–1000 kHz (pratikte 650–850) | [doğrulandı] BR ürün sayfası + ping360 mesaj dokümanı |
| İleri bakan çok ışınlı görüntüleme sonarı (FLS) | Menzil × azimut yoğunluk görüntüsü. Dikey açı (elevation) bilgisi kaybolur; bu belirsizlik doğal olarak görüntüye yansır. | **Blueprint Oculus M750d**: 750 kHz / 1.2 MHz; 120 m / 40 m; en çok 512 ışın; açısal çözünürlük 1° / 0.6°; menzil çözünürlüğü 4 / 2.5 mm; en çok 40 Hz; yatay açıklık 130° (LF), dikey ~20° | Gigabit Ethernet + üretici SDK'sı. Veri formatı ayrıntıları burada doğrulanmadı. | [ikincil] (Blueye/VideoRay spec sheet'leri; HF açıklığında kaynaklar çelişiyor) |
| | | **Tritech Gemini 720is**: 720 kHz, 120° FOV, ~30 Hz, 8 mm menzil çözünürlüğü | Ethernet / VDSL | [ikincil]; ışın sayısı doğrulanmadı |
| | | **Teledyne BlueView P900-90** (Choi vd. simülasyon konfigürasyonu): 512 ışın, 90° FOV, 0.18° ışın aralığı | Ethernet | [doğrulandı] (Choi 2021, Tablo) |
| Çok ışınlı batimetrik (MBES) | Fan boyunca her ışın için tek "dip noktası" (menzil + açı). Su sütunu verisi isteğe bağlıdır. | Küçük araç sınıfı ürünler (Imagenex, Norbit vb.). Sayısal değerler bu raporda **doğrulanmadı**. | Genelde Ethernet | — |
| Yan taramalı (SSS) | İki yana dar (along-track) ve geniş (across-track) ışınlarla zaman-yoğunluk satırları. Görüntü aracın ilerlemesiyle "waterfall" olarak oluşur. | **Cerulean Omniscan 450 SS**: 450 kHz, along-track 0.5°, across-track 50°/taraf, ~150 m, across-track çözünürlük menzil/1200'e kadar, 10–30 V | Ethernet. Protokol doğrulanmadı. | [ikincil] |

**Mimari açıdan önemli çıkarımlar:**
- **Ping360 zamanlaması fiziğe bağlıdır** [doğrulanmadı — hesap].
  - Bir adımın süresi ≈ verici süresi + 2R/c + işlem süresidir. 50 m'de 2·50/1480 ≈ 68 ms eder; 400 gradyan × 68 ms ≈ 27 s. Bu değer üreticinin verdiği 33 s ile tutarlıdır.
  - Simülatör her adımı ayrı zaman damgası ve ayrı araç pozu ile üretmelidir. Aksi halde tarama sırasındaki hareket bozulması (motion smear) kaybolur.
- **Ping360 menzil/örnek ilişkisi:** menzil = c · n · sample_period · 25 ns / 2. 1200 örnek ve 50 m için örnek başı 4.17 cm çıkar; bu, üreticinin verdiği 4.1 cm çözünürlükle tutarlıdır. Simülatör `sample_period` ve `number_of_samples` alanlarından menzil kutularını aynı formülle türetmelidir.
- **Açısal çözünürlük:** 2° ışın 10 m'de ≈ 0.35 m yanal çözünürlük verir [hesap]. Küçük bir yarışma hedefi (ör. 0.3–0.5 m'lik hedef panosu/kapı) birkaç adıma yayılır. Hedef tespit algoritması bu bulanıklıkla test edilmelidir.
- **UART bant genişliği:** 115200 baud ≈ 11.5 kB/s. 1200 baytlık bir Ping360 profili ≈ 104 ms sürer. Kısa menzilde UART/RS-485 darboğaz olur [hesap]. Üreticinin RS-485 için USB/Ethernet'ten daha uzun tarama süresi vermesi (1 m'de 4.26 s'ye karşı 3.42 s) bununla uyumludur.
- **FLS verisi MCU'ya uygun değil:** 512 ışın × ~500 kutu × 8 bit × 40 Hz ≈ 10 MB/s [hesap]. Bu veri gerçek araçta Jetson/RPi sınıfı yardımcı bilgisayarda işlenmeli. STM32'ye yalnızca "tespit" (menzil, kerteriz, güven) gönderilmelidir.

### 2.2 Sonar simülasyon yöntemleri

#### 2.2.1 Fiziksel bileşenler ve formüller

- **Aktif sonar denklemi (kavramsal):** EL = SL − 2·TL + TS. Burada TL = 20·log10(r) + α·r (küresel yayılım + soğurma) ve TS hedef kuvvetidir. Görüntüleme sonarlarında alıcı TVG (time-varying gain) uygular. Simülatörler genelde ya fiziksel 1/r² ve e^(−2αr) uygulayıp ardından TVG ile normalize eder, ya da doğrudan menzile göre normalize eder. OceanSim "range-wise normalization" yapıyor [doğrulandı, OceanSim makalesi].
- **Yüzey yansıması:** Pratik simülatörlerin hepsi Lambert benzeri bir model kullanıyor: I ∝ A_r · cos(θ_inc), yani normal ile bakış vektörünün iç çarpımı.
  - Stonefish shader'ı: `clamp(dot(N, toEye), 0, 1) * restitution` [doğrulandı, kaynak kod].
  - OceanSim: I = A_r · (−v_in/|v_in| · v_n/|v_n|) · e^(−αd) [doğrulandı].
  - Choi vd.: Lambert yasası, μ yansıtıcılık parametresi [doğrulandı].
  - Malzemeye bağlı A_r (metal hedef ve beton farklı) gerçekçilik için ucuz ama etkili bir parametredir.
- **Akustik gölge:** Ray-cast veya z-buffer tabanlı her yöntemde doğal olarak oluşur. En yakın kesişim alındığı için arkadaki yüzeyler görünmez. HoloOcean'da gölge kenarı için `ShadowEpsilon` parametresi var [doğrulandı].
- **Benek (speckle) gürültüsü:**
  - Faz-uyumlu çok saçıcının toplamının genliği Rayleigh dağılımına uyar. Baskın (speküler) bir bileşen varsa Rician olur. Choi vd. bunu karmaşık Gauss genliklerle *fiziksel olarak* üretiyor [doğrulandı].
  - Az sayıda saçıcı veya geniş ışın durumunda dağılım Rayleigh'den sapar; K-dağılımı kullanılır (Abraham & Lyons 2002, IEEE JOE 27(4)) [künye doğrulandı, içerik özetten].
  - İstatistiksel kısayol (HoloOcean/OceanSim) şöyledir: Ĩ = I·(0.5 + w_m) + w̃_a, w_m ~ N(0, σ_m), w_a ~ Rayleigh(σ_a). Burada w̃_a menzile (r²/r_max²) ve azimuta (ışın deseni kazancı) göre ölçeklenir [doğrulandı, OceanSim denklem 3–6].
  - Stonefish'in yaklaşımı: ışın başına bir Gauss çarpımsal gürültü ve kutu başına Gauss toplamsal gürültü [doğrulandı, `flsOutput.comp`].
- **Yan loblar / ışın deseni:**
  - Choi vd. her ışını ideal dikdörtgen olarak hesaplıyor. Ardından tüm ışınların sinc (düzgün doğrusal dizi) ağırlıklı toplamıyla yan lobları geri ekliyor. Ağırlıklar önceden hesaplanıp GPU'da matris çarpımı olarak uygulanıyor [doğrulandı].
  - Stonefish dikey ışın içinde `smoothstep` ile kenar zayıflatması yapıyor ("lobe intensity correction") [doğrulandı].
  - DAVE'in eski görüntü-sonar eklentisinde dikey desen tamamen kare. Belgelenmiş zayıflıkları: sabit kodlu sonar sabitleri ve kare dikey desen [doğrulandı, DAVE dokümanı].
  - HoloOcean'da `AzimuthStreaks` parametresi gerçek sonarlarda görülen ışın şeritlerini taklit ediyor [doğrulandı].
- **Çok yollu yayılım (multipath):**
  - HoloOcean IROS 2022 "küme tabanlı multipath ray-tracing" algoritması öneriyor (`MultiPath`, `ClusterSize=5`) [özet doğrulandı; algoritma ayrıntısı okunamadı].
  - Cerqueira vd. 2020 hibrit yaklaşım kullanıyor: birincil kesişimler rasterize ediliyor, yalnızca yansıtıcı alanlar ray-trace ediliyor [ikincil].
  - Havuzda yüzey, duvar ve taban aynası "hayalet" hedefler üretir. Bu durum yarışmada yanlış pozitif kaynağıdır [doğrulanmadı — alan deneyimine dayalı beklenti].
- **Ses hızı:**
  - Mackenzie (1981): c = 1448.96 + 4.591T − 5.304e−2·T² + 2.374e−4·T³ + 1.340(S−35) + 1.630e−2·D + 1.675e−7·D² − 1.025e−2·T(S−35) − 7.139e−13·T·D³. Geçerlilik: T 2–30 °C, **S 25–40 ‰**, D 0–8000 m [doğrulandı, NPL].
  - **Tatlı su (havuz) için Mackenzie geçerli değildir.** UNESCO/Chen-Millero (Wong & Zhu 1995 katsayıları; S 0–40, T 0–40 °C) kullanılmalı [doğrulandı, NPL].
  - c'deki %1'lik hata menzilde %1 hata demektir.
  - <50 m menzilde ses hızı profilinden kaynaklanan ışın kırılması (refraction) ihmal edilebilir. Havuzda pratikte sabit c yeterlidir [doğrulanmadı — mühendislik yargısı].
- **Soğurma:**
  - Toplam soğurma üç terimin toplamıdır: borik asit + magnezyum sülfat + saf su (viskoz) terimi. Viskoz terim >1 MHz'de baskındır. Francois-Garrison (1982) ve Ainslie-McColm (1998) aynı veri setine dayanır; A&M daha sadedir [doğrulandı, NPL].
  - A&M formülü (dB/km, f kHz, z km) [**doğrulanmadı** — NPL sayfasında formül görsel olduğu için okunamadı, bellekten]:
    - α = 0.106·f₁f²/(f₁²+f²)·e^((pH−8)/0.56) + 0.52(1+T/43)(S/35)·f₂f²/(f₂²+f²)·e^(−z/6) + 0.00049·f²·e^(−(T/27+z/17))
    - f₁ = 0.78√(S/35)·e^(T/26), f₂ = 42·e^(T/17)
  - Bu formülle 750 kHz, 20 °C tatlı suda α ≈ 0.13 dB/m çıkıyor; 50 m'lik gidiş-dönüşte ≈ 13 dB [hesap, doğrulanmadı].

#### 2.2.2 Mevcut simülatörler nasıl yapıyor?

| Simülatör | Yöntem | Gürültü | Performans | Kaynak |
|---|---|---|---|---|
| **Stonefish** | OpenGL ile sahne sonar görüş hacminde render edilir. Fragment shader her pikselde (menzil, Lambert·restitution) yazar. Compute shader her ışın sütunu için dikey örnekleri menzil kutularına histogramlar. MSIS ve SSS için de ayrı compute shader'lar var (`msisUpdate*`, `sssLine*`). | Işın başına Gauss çarpımsal + kutu başına Gauss toplamsal. Belge örnek değerleri: FLS 0.01/0.02, MSIS 0.02/0.03, SSS 0.02/0.04. | Gerçek zamanlı (sayısal değer bulunamadı) | Kaynak kod, readthedocs [doğrulandı] |
| **HoloOcean** (UE4 → 2.0'da UE5.3) | Ortamın octree temsili. Görüş hacmindeki yaprakların menzil-azimut kutularına toplanması. 2.0 sürümü octree yerine doğrudan ray-cast'e geçiyor. | Rayleigh toplamsal, Gauss çarpımsal, üstel menzil gürültüsü (`RangeSigma`), multipath, azimut şeritleri | Octree önbelleği sahne başına 317–502 s; sonrasında 1.5–3.8 FPS (OceanSim'in ölçümü, RTX A6000) | Docs v2.4.0, Potokar 2022 ICRA/IROS, OceanSim Tablo II [doğrulandı] |
| **OceanSim** (Isaac Sim) | GPU ray tracing ile nokta bulutu (normal + semantik) → polar ızgarada toplama → HoloOcean gürültü modeli. Varsayılanlar Oculus M750d'ye göre ayarlı. | HoloOcean ile aynı | 28–32 FPS, önbellek gerekmez (RTX A6000) | arXiv 2503.01074 [doğrulandı] |
| **DAVE / nps_uw_multibeam_sonar** (Choi vd. 2021) | Gazebo + CUDA. Her ray bir saçıcıdır. Frekans alanında ışın spektrumu Σ a_n·S(f)·e^(i2kr_n)/r_n² biçiminde hesaplanır (k karmaşık, soğurma dahil) → IFFT ile zaman serisi → sinc ağırlıklı yan lob düzeltmesi. | Fiziksel: karmaşık Gauss genlik → Rayleigh/Rician | 512 ışın: 114 ray/ışın, 60 m'de 0.5 Hz; 11 ray, 60 m'de 3 Hz; 11 ray, 10 m'de 10 Hz (i9-9900K + RTX 2080 Ti) | Frontiers in Robotics & AI 2021 [doğrulandı] |
| **DAVE image sonar** (eski, Cerqueira 2017 kökenli) | Derinlik tamponu → Sobel ile normal → "SNR" görüntüsü → perspektif dilim → çarpımsal Gauss speckle | Sabit kodlu | — | DAVE dokümanı [doğrulandı] |
| **Cerqueira vd. 2017 / 2020** | GPU deferred shading ile derinlik + normal tamponu. 2020'de rasterize + seçmeli ray-trace hibriti. | — | "Gerçek zamanlı" | Computers & Graphics 68 (2017); arXiv 2001.03539 [künye doğrulandı] |
| **Bell & Linnett 1997** | SSS için optik ray tracing + dip pürüzlülüğü/backscatter istatistiği | — | Offline | IEE Proc. Radar Sonar Nav. 144(4) [künye doğrulandı] |

#### 2.2.3 Gerçekçilik / maliyet kademeleri (öneri sentezi)

| Kademe | İçerik | Maliyet | Ne için yeterli |
|---|---|---|---|
| **T0 — Semantik** | Görüş konisindeki hedeflerin (menzil, kerteriz) gürültülü listesi, tespit olasılığı P_d(r, açı) | İhmal edilebilir | Görev mantığı, durum makinesi, torpido atış zinciri |
| **T1 — CPU ray-cast, ışın başına histogram** | Işın konisi içinde N ray (ör. 25° koni için 200–2000). Her kesişim için Lambert·A_r·e^(−2αr)/r² → menzil kutusu. Gauss ışın ağırlığı. Speckle (Rayleigh). Gölge doğal olarak oluşur. | Ping1D/Ping360 için tek çekirdekte rahat (rakam doğrulanmadı) | Ping1D, Ping360, DVL ışınları. Firmware'in gerçek Ping Protocol verisini işlemesi. |
| **T2 — GPU raster/ray-trace + istatistiksel gürültü** | Stonefish/OceanSim tarzı. Menzil-azimut ızgarası, TVG/normalizasyon, Rayleigh + Gauss gürültü, azimut şeritleri | 512 ışında ~30 FPS (OceanSim, RTX A6000). macOS'ta Metal (wgpu ile) gerekir; donanımsal RT her platformda yok. | FLS, SSS, yardımcı bilgisayardaki tespit ağının eğitimi/testi |
| **T3 — Faz-uyumlu nokta saçıcı** | Choi vd.: karmaşık genlik, frekans alanı, yan loblar, fiziksel speckle | 0.5–10 Hz (RTX 2080 Ti) | Doğrulama, algoritma hassasiyet analizi (çevrimdışı) |
| **T4 — Multipath / kırılma** | Tek sekmeli ayna yansıması (yüzey/duvar). İleri aşamada küme tabanlı multipath (HoloOcean). | T2'nin 2–N katı (doğrulanmadı) | Havuz yanlış pozitifleri, yüzeye yakın çalışma |

### 2.3 Su altı seyrüsefer sensörleri

#### 2.3.1 DVL (Water Linked DVL A50 referans)

**Parametreler [doğrulandı, Water Linked dokümanları + Blueye spec sheet]:**
- 1 MHz, 4 ışınlı dışbükey Janus dizisi, ışın açısı 22.5°, yarı-güç ışın genişliği 4.4°.
- İrtifa 5 cm – 50 m (>35 m'de dip/tuzluluğa bağlı), en yüksek hız 3.75 m/s, hız çözünürlüğü 0.1 mm/s.
- Uzun dönem doğruluk ±%1.01 (standart) / ±%0.1 (Performance) [ikincil — Blueye spec sheet'i, WL kaynaklı].
- Ses hızı ayarlanabilir (1000–2000 m/s); örnek yapılandırmada 1475.

**Güncelleme hızı (range mode tablosu, A50/A125):**

| Mod | İrtifa | Hız |
|---|---|---|
| 0 | 0.05–0.6 m | 15 Hz |
| 1 | 0.3–3 m | 10 Hz |
| 2 | 1.5–14 m | 5–6 Hz |
| 3 | 7.7–36 m | 7–8 Hz |
| 4 | 15 m – max | 2–4 Hz |
| Su takibi (water tracking) | — | 2 Hz |

Dead-reckoning raporu 5 Hz'dir.

**Arayüz:**
- **TCP JSON (port 16171).** Rapor alanları:
  - hız ve doğruluk: `vx, vy, vz, fom, covariance(3×3), altitude`,
  - ışın başına: `transducers[{id, velocity, distance, rssi, nsd, beam_valid}]`,
  - durum: `velocity_valid, status`,
  - zaman: `time_of_validity` (ping merkezi, µs) ve `time_of_transmission` (µs).
  - Ölçülen TCP gecikmesi: ortalama ~4 ms, σ 2 ms, en çok 13 ms.
- **Seri port.** 115200 8-N-1. `wrz,vx,vy,vz,valid(y/n),altitude,fom,cov(9 değer ';' ile),tov,tot,time,status*crc`.
  - CRC-8: poly 0x07, init 0, refin/refout false.
  - Işın raporu: `wru,id,velocity,distance,rssi,nsd`.
  - Kilit yokken örnek: `wrx,...,0.000,0.000,0.000,2.707,-1.00,n,1` (irtifa −1, valid = n).

**Model önerisi:**
1. **Işın geometrisi:** 4 ışın Janus düzeninde. Her ışın için terrain ray-cast yapılır (Stonefish de böyle yapıyor) ve menzil r_i bulunur. Işın geçerlilik koşulu: r_i ∈ [r_min, r_max(mod)], geliş açısı sınırın altında ve kabarcık/türbülans maskesi yok.
2. **Işın hızı:** u_i = b_iᵀ·(v_DVL + ω × l) + n_i. Burada b_i ışın birim vektörü, l kol (lever-arm) uzunluğu, n_i ~ N(0, σ_b(r, rssi)).
3. **Çözüm:** 4 (veya ≥3) ışından en küçük kareler ile v̂ ve kovaryans; `fom` kovaryanstan türetilir. Gerçek A50'nin 3 ışınla çözüm verip vermediği **doğrulanmadı**.
4. **Sistematik hatalar:**
   - Ölçek: ses hızı uyuşmazlığı Δc/c. Klasik piston dönüştürücüde hız c ile orantılı ölçeklenir. Faz dizili DVL'lerde yatay bileşen c'den büyük ölçüde bağımsızdır. A50'nin dönüştürücü tipi **doğrulanmadı**.
   - Montaj hizalama hatası: özellikle yaw. Kat edilen yolla orantılı yanal sapma üretir.
   - Ölçek faktörü hatası: %0.1–1.
5. **Dip kilidi (bottom lock) kaybı ve geri kazanımı:**
   - Kaybın tetikleyicileri: irtifa aralık dışı, ışın engeli, kabarcık, itici türbülansı, akustik girişim, yumuşak dip. Bunlar WL SSS'de listelenmiş.
   - Kayıp sırasında `velocity_valid=false` olur ve hızlar 0 döner. **Firmware'in bu değeri kullanmaması test edilmeli.**
   - Geri kazanım bir arama gecikmesi ister: mod taraması, rastgele 0.5–3 s [doğrulanmadı — parametre].
   - "Periodic cycling" her 10 s'de daha yakın bir yansıma olup olmadığını kontrol eder [doğrulandı].
6. **Zaman:** Her rapor `time_of_validity = ping zamanı`. Teslim gecikmesi = 2r/c + işlem süresi + iletim süresi.
7. **Hibrit araca özel durum:** Yüzeyde seyirde DVL dalga kabarcıkları ve itici akışı nedeniyle sık kilit kaybeder. Su yüzeyine çok yakınken ışınlar havaya çıkabilir (araç yalpası). Bu senaryolar test kataloğuna eklenmeli [doğrulanmadı — beklenti].

#### 2.3.2 Basınç / derinlik sensörü (Blue Robotics Bar30 / MS5837-30BA)

**Parametreler:**
- 0–30 bar, derinlik çözünürlüğü 2 mm (tatlı su).
- Mutlak doğruluk ±200 mbar (0–45 °C, ≈ ±2 m) ve ±400 mbar (−20–85 °C).
- Sıcaklık doğruluğu ±4 °C. I²C adresi 0x76 [doğrulandı, BR ürün sayfası].

**Dönüşüm süresi:**
- Blue Robotics Python sürücüsü yorumu: "maks süre ≈ 2.2 µs × OSR". OSR 8192'de ≈ 18 ms eder; D1 + D2 için ≈ 36 ms → en çok ~25 Hz [doğrulandı — sürücü kaynak kodu; datasheet tablosu okunamadı].
- MS5837 datasheet'inde OSR 8192 için 18.08 ms ve 0.2 mbar RMS geçtiği söyleniyor [**doğrulanmadı**].

**Model:**
- P_ölçülen = P_atm + ρ·g·d + P_dalga + b(T) + n.
- Mutlak ofset yüzeyde sıfırlanır (firmware tarafında). Bu yüzden asıl önemli olanlar şunlardır: sıcaklık kayması, kuantizasyon, ρ seçimi (tatlı su 997 kg/m³'e karşı deniz ~1025 kg/m³; yanlış ρ ≈ %2.8 ölçek hatası) ve dalga/dinamik basınç (aracın hızıyla 0.5ρv²).
- MAVLink `HIL_SENSOR.abs_pressure` hPa cinsinden float'tır. Su altı değerleri (10 m ≈ 2000 hPa mutlak) sığar, ancak alan hava için tasarlanmıştır [doğrulandı, MAVLink].

#### 2.3.3 USBL / SBL ve akustik modem

- **Water Linked Underwater GPS G2:**
  - USBL değil, **SBL**'dir (Short Baseline): yüzeyde 4 alıcı ve bir locator.
  - Locator'dan her alıcıya uçuş süresiyle konum hesaplanır. Menzil 300 m'ye kadar (R300).
  - Topside GPS + pusula yönü gerekir; dinamik ortamda harici pusula önerilir [doğrulandı, WL dokümanı].
- **Blueprint SeaTrac X150:** USBL + veri modem, 1 km yarı-küresel menzil. "±0.1 m" değeri ses hızı doğruluğuna bağlı verilmiş (etiketi okunamadı). Kerteriz doğruluğu ve güncelleme hızı **doğrulanmadı** [ikincil].
- **Model önerisi** (genel USBL/SBL) [doğrulanmadı — literatür bilgisi]:
  - Konum = gerçek + menzil hatası (c hatası, zaman damgası jitter'i) + açı hatası (kerteriz σ_θ · r ile büyüyen yanal hata).
  - Yüzey biriminin yön/eğim hatası ve gecikme 2r/c + işlem süresidir.
  - Ayrıca çok yollu ölçüm sıçramaları (havuz duvarları!) ve kayıp eklenmeli.
  - Hata menzille orantılıdır; "menzilin %x'i" biçiminde parametrelenmeli.
- **Akustik modem:**
  - Water Linked M64: 64 bit/s yarı-çift yönlü, tipik ~500 ms gecikme, 200 m, 8 baytlık yük.
  - M16: 10 bit/s, 1000 m, paket süresi 1.6 s [doğrulandı, WL/BR].
  - Model: kuyruk + bant genişliği sınırı + gecikme + bit hata/paket kaybı (menzil ve gürültüye bağlı) + yarı-çift yönlü çakışma.

### 2.4 Atalet ve yön sensörleri

#### 2.4.1 IMU hata modeli

**Standart sürekli zaman modeli (Kalibr)** [doğrulandı]:
- ω̃ = ω + b + n, ḃ = σ_bg·w.
- Ayrık zamanda: σ_d = σ/√Δt (beyaz gürültü), σ_bd = σ_b·√Δt (bias random walk).
- Allan sapmasından okuma: σ değeri τ = 1 s'de, σ_b değeri τ = 3 s'deki +½ eğimli doğrudan.
- Düşük maliyetli MEMS'te parametrelerin **10× veya daha fazla şişirilmesi** öneriliyor; sebep sıcaklık ve ölçek etkilerinin modelde olmaması.
- Allan grafiğinde eğimler: ARW −½, bias kararsızlığı 0, rate random walk +½ (IEEE Std 952-1997) [ikincil özetlerle doğrulandı].
- Bias kararsızlığı yaygın olarak min(AD)/0.664 olarak raporlanır [ikincil, üretici sayfası].

**PX4 Gazebo IMU eklentisi** [doğrulandı, kaynak kod]:
- Bias birinci derece Gauss-Markov: b_k = φ·b_{k−1} + σ_bd·w, φ = e^(−Δt/τ), σ_bd = √(−σ_b²·τ/2·(e^(−2Δt/τ) − 1)).
- Varsayılanlar (IIM-42653 için): gyro ND 8.73e−4 rad/s/√Hz, accel ND 6.37e−3 m/s²/√Hz, τ_g = 1000 s, τ_a = 300 s, random walk 0.

**Tam model önerisi:**
- ỹ = S·M·(x + b_on + b_rw(t) + b_T·(T−T₀)) + n + kuantizasyon, ardından doygunluk (saturation).
- S: ölçek matrisi (köşegen = 1 + s_i).
- M: eksenler arası / hizalama hatası (köşegen dışı).
- b_on: açılıştaki bias (her açılışta rastgele).
- Gyro'da g-duyarlılığı ve titreşim düzeltmesi (vibration rectification) opsiyoneldir.

**Gerçek sayısal değerler:**

| Parametre | TDK ICM-42688-P (rev 1.6 datasheet) | ADI ADIS16470 (taktik altı sınıf) |
|---|---|---|
| Gyro gürültü yoğunluğu | 0.0028 °/s/√Hz (= 4.9e−5 rad/s/√Hz; ARW ≈ 0.17 °/√h [hesap]) | ARW 0.34 °/√h |
| Gyro bias kararsızlığı | **Datasheet'te yok** (TDK forumunda da yanıtsız) | 8 °/h (1σ) |
| Gyro ilk ZRO toleransı | ±0.5 °/s (kart seviyesi, 25 °C) | — |
| Gyro ZRO sıcaklık kayması | ±0.005 °/s/°C (0–70 °C) | — |
| Gyro ölçek toleransı / sıcaklık | ±%0.5 / ±%0.005/°C | — |
| Gyro doğrusalsızlık / eksenler arası | ±%0.1 / ±%1.25 | — |
| Accel gürültü yoğunluğu | X/Y 65, Z 70 µg/√Hz (VRW ≈ 0.04 m/s/√h [hesap]) | VRW 0.037 m/s/√h |
| Accel ilk ofset / sıcaklık | ±20 mg / ±0.15 mg/°C | Bias kararsızlığı 13 µg |
| Accel ölçek / eksenler arası | ±%0.5 / ±%1 | — |
| ODR | Gyro 12.5 Hz – 32 kHz; accel 1.5625 Hz – 32 kHz; 2 kB FIFO; SPI 24 MHz | — |

Kaynaklar: ICM-42688-P [doğrulandı, datasheet PDF metni]; ADIS16470 [ikincil — aggregator + datasheet alıntısı].

**Sıcaklık:** Araç içi sıcaklık su sıcaklığına ve elektronik ısısına bağlı olarak yavaş değişir. Bu yüzden b_T·ΔT terimi düzinelerce dakikalık görevlerde bias random walk'tan baskın olabilir [doğrulanmadı — beklenti]. Sıcaklık, ayrı bir termal model (bkz. 2.7) ile sürülmeli.

#### 2.4.2 Manyetometre

**Model (NXP AN4246, Ozyagcilar 2013):**
- B_ölçülen = W·R·B_dünya + V + n.
- W: yumuşak demir + ölçek + eksen dikliksizliği matrisi. V: sert demir (PCB ve sensör kaynaklı sabit ofset). R: araç yönelimi [künye doğrulandı; denklem biçimi NXP forum yanıtı ve AN4246 özetinden; **PDF'ten birebir okunamadı**].
- Kalibrasyon bunun tersidir: elipsoid uydurma.

**Motor/akım kaynaklı bozulma:**
- ArduPilot `compassmot`: girişim **akımla doğrusal** kabul edilir.
- Kabul ölçütü: <%30 iyi, %31–60 gri bölge, >%60 sensörü taşı [doğrulandı, ArduPilot dokümanı].
- Model: B_motor = Σ_k K_k·I_k (her itici/ESC akımı için 3B katsayı vektörü). Yön ters çevirmede işaret değişimi de modellenebilir.

**Sayısal değerler:**
- PNI RM3100: gürültü 15 nT (200 çevrim), 20 nT (100), 30 nT (50) [ikincil — PNI breakout datasheet].
- LIS3MDL ~3.2 mgauss (320 nT) **doğrulanmadı**.
- Dünya alanı WMM/IGRF modelinden alınmalı; Türkiye'de toplam alan ~45–50 µT civarı **doğrulanmadı**.

**Havuz riski:** Betonarme havuzlarda donatı demiri yerel manyetik anomali üretir. Simülatörde konuma bağlı "bozulma alanı" (ör. Gauss lekeleri) modellenebilir [doğrulanmadı — beklenti].

### 2.5 GNSS

- **Su altında kayıp:** L1 sinyali sıvı suya en fazla ~3 cm nüfuz ediyor. Deney teori ile uyumlu bulunmuş (J. Geodesy 2018, DOI 10.1007/s00190-018-1147-x) [özet doğrulandı]. Deniz suyunda deri derinliği L1'de ~6 mm mertebesinde [ikincil/hesap].
  - Pratik model: `gnss_available = anten_derinliği < ~0.02–0.03 m ve dalga taşması yok`.
  - Dalga taşması aralıklı kayıplara yol açar (JHU APL: "even mild antenna washover rapidly attenuates") [ikincil].
- **Yeniden kilitlenme:**
  - u-blox NEO-M9N datasheet (R02): cold start 24 s, hot/aided start 2 s, konum doğruluğu 2.0 m CEP (SBAS ile 1.5 m) [ikincil — birden çok bayi + datasheet alıntısı].
  - Model: dalış süresi < efemeris geçerliliği (~2–4 saat) ise hot start (≈2 s + rastgele), uzun ise warm start [efemeris süresi **doğrulanmadı**].
  - Fix tipi geçişi: NO_FIX → 2D → 3D. Uydu sayısı ve HDOP kademeli artar. İlk saniyelerde hata büyük, sonra sönümlenir.
- **Gürültü/sürüklenme (PX4 Gazebo GPS eklentisi)** [doğrulandı, kaynak kod]:
  - Konum = gerçek + beyaz gürültü + bias.
  - bias_{k+1} = bias_k + rw·√Δt·w·Δt − bias_k/τ, τ = 60 s.
  - Varsayılanlar: XY random walk 2.0, Z 4.0 (m/s)/√Hz; konum gürültü yoğunluğu 2e−4 / 4e−4 m/√Hz; hız 0.2 / 0.4 (m/s)/√Hz; 5 Hz; **gecikme 120 ms**.
  - Bias'ın dalıştan sonra yeniden başlatılması ve yüzeyde çok yollu yayılım (su yüzeyi yansıması, alçak anten) ek terim olarak önerilir [doğrulanmadı].
- **Protokol:** ArduPilot SITL GPS'i gerçek bayt akışı olarak üretebiliyor (UBX, NMEA, SBP, NOVA, MSP tipleri) [doğrulandı, `SIM_GPS.h`]. Bu, kablo seviyesi emülasyonun (yaklaşım a) yerleşik bir örneğidir.

### 2.6 Su altı kamera

- **Jaffe-McGlamery (Jaffe 1990, IEEE JOE 15:101–111):** Görüntü üç bileşenin toplamıdır: doğrudan + ileri saçılım + geri saçılım. Geri saçılım kamera ışını boyunca hacim integralidir ve ışık kaynağı-kamera geometrisine bağlıdır [künye ve yapı doğrulandı].
- **Revize model (Akkaynak & Treibitz, CVPR 2018):** Doğrudan sinyal ve geri saçılım **farklı** geniş-bant katsayılarla yönetilir. Eski "tek katsayılı" (atmosferik pus) model hatalıdır [doğrulandı].
- **Gerçek zamanlı uygulama:** OceanSim derinlik görüntüsü d ile kanal başına I_c = J·e^(−β_attn,c·d) + B∞,c·(1−e^(−β_bs,c·d)) hesaplıyor ve GPU'da çalıştırıyor [doğrulandı]. Rust/wgpu'da post-process shader olarak birkaç satırlık işlemdir; maliyet ihmal edilebilir.
- **Kademeler:**
  - K1: Yukarıdaki formül (ortam ışığı, sığ havuz). Gerçek zamanlı.
  - K2: Yapay aydınlatma (araç lambası). Geri saçılım için ekran uzayında ray-marching (ışın başına 16–64 adım, ışık konisi ve faz fonksiyonu) + ileri saçılım için menzile bağlı bulanıklık (PSF yarıçapı ∝ d). Düşük çözünürlükte gerçek zamanlı mümkün [doğrulanmadı — mühendislik tahmini].
  - K3: Partikül (marine snow), kaustikler, yüzey yansıması, hareket bulanıklığı, sensör gürültüsü (Poisson + okuma), rolling shutter, otomatik pozlama, JPEG/H.264 sıkıştırma artefaktları.
- Havuzda su çok berrak olsa da yüzey kaustikleri ve kloraminden gelen renk tonu belirgindir [doğrulanmadı].

### 2.7 Yardımcı sensörler

- **Batarya gerilim/akım:** Blue Robotics Power Sense Module analog çıkış verir: gerilim 11.0 V/V, akım 37.8788 A/V (0.330 V ofset), 100 A'e kadar [doğrulandı].
  - Model: Thevenin eşdeğer devresi (OCV(SoC, T) − I·R₀ − V_RC1 − V_RC2), Coulomb sayımı, sıcaklığa bağlı R₀ (ör. Chen & Rincón-Mora 2006, IEEE Trans. Energy Conv. 21(2)) [künye bellekten, **doğrulanmadı**].
  - Akım, simülatördeki itici modelinden gelir.
  - ADC kuantizasyonu, Hall sensörü ofset kayması ve itici PWM'inden kaynaklanan gürültü eklenmeli.
- **Sızıntı:** BR SOS Leak Sensor sızıntıda çıkışı VCC'ye çeker ve dijital GPIO'dur. Tepki süresi verilmemiş [doğrulandı]. Model: olay tabanlı (zamanında tetikleme), sahte pozitif sıçrama, gecikme.
- **Sıcaklık:** BR Celsius sensörü güncel sürümde TMP119 kullanıyor (önceden TSYS01). Doğruluk ±0.08 °C (0–45 °C), zaman sabiti tipik 0.5 s, I²C 0x48 [doğrulandı].
  - Gövde içi sıcaklık için birinci derece termal model: C·dT/dt = P_elek − k·(T − T_su). Bu model IMU ve basınç sensörünün sıcaklık terimlerini de sürer.

### 2.8 Zamanlama

**Gerçekçi örnekleme hızları ve gecikmeler:**

| Sensör | Örnekleme hızı | Gecikme / zaman damgası notu | Etiket |
|---|---|---|---|
| IMU (ICM-42688-P) | 1 kHz tipik seçim (12.5 Hz – 32 kHz arası mümkün) | FIFO; data-ready kesmesi; filtre gecikmesi ODR'ye bağlı | ODR doğrulandı; 1 kHz seçim önerisi |
| Manyetometre | 10–100 Hz | — | doğrulanmadı |
| Basınç (MS5837, OSR 8192) | ≤ ~25 Hz | Dönüşüm ~18 ms × 2 | sürücü kodundan |
| DVL A50 | 2–15 Hz (irtifaya bağlı) | `time_of_validity` ≠ teslim zamanı; TCP +4 ms ort. | doğrulandı |
| Ping1D | Yapılandırılabilir `ping_interval` (ms); en yüksek hız verilmemiş | Menzil 2R/c | doğrulandı / hız doğrulanmadı |
| Ping360 | Adım başına ≈ 2R/c + ek yük; 360° = 3.4–33 s | Her adım ayrı zaman ve poz | doğrulandı |
| Oculus M750d | ≤ 40 Hz | — | ikincil |
| GNSS | 1–10 Hz (PX4 simülasyon varsayılanı 5 Hz) | ~120 ms (PX4 varsayılanı) | PX4 kodu |
| USBL/SBL | ~0.5–2 Hz | 2r/c + işlem | doğrulanmadı |
| Akustik modem M64 | 64 bit/s | ~500 ms | doğrulandı |
| Kamera | 30 fps | Pozlama ortası damgası; rolling shutter | genel |

**Simülatörde çok hızlı sensörlerin zamanlanması** [öneri, doğrulanmadı — mühendislik]:
1. **Tamsayı zaman ekseni:** ns veya µs cinsinden `u64` sim saati. Fizik sabit adımla ilerler (ör. 1 kHz veya 2 kHz).
2. **Her sensör kendi periyoduna sahip bir zamanlayıcıdır.** Olay kuyruğu (min-heap) her olay için iki ayrı zaman tutar: `t_measure` (ölçüm anı; ölçüm burada fizik durumundan örneklenir) ve `t_deliver` (teslim anı = t_measure + gecikme + jitter).
3. **Fizik adımından hızlı sensörler** (ör. 4 kHz IMU ve 1 kHz fizik) için iki seçenek var:
   - ara değerleme: ivme/açısal hız adım içinde doğrusal veya spline ile ara değerlenir,
   - alt adımlama: fizik o sensör için alt adımlara bölünür.

   Gyro entegrasyon tutarlılığı için IMU ölçümü "anlık" değil, örnek aralığı boyunca **ortalama** olarak üretilmeli. Bu, anti-alias/LPF davranışına da yaklaşır.
4. **Her sensörün kendi saat kayması (clock drift, ppm) ve faz ofseti** olmalı. Gerçek sensörler senkron örneklemez.
5. **HIL taşımasında yüksek hızlı veriyi paketlemek:** IMU örnekleri 1 kHz'de tek tek değil, örneğin 8 örneklik paketlerle (ICM FIFO'suna benzer) gönderilebilir. Her örnek kendi zaman damgasını taşır. USB HS mikroçerçevesi 125 µs'dir [USB 2.0 genel bilgisi, doğrulanmadı].
6. **Lockstep vs gerçek zaman:**
   - PX4 SITL simülatörle lockstep çalışıyor [doğrulandı]. PX4 HITL sayfası lockstep'ten söz etmiyor; HITL'ye "community supported" deniyor [doğrulandı].
   - Gerçek kartta firmware kendi saatiyle çalıştığı için HIL'de pratik yöntem **gerçek zamanlı + sim zaman damgası**dır. Simülatör gerçek zamana kilitlenir; geride kalırsa bu ölçülür ve loglanır.
   - SIL'de lockstep (deterministik, hızlandırılabilir) kullanılmalı.

### 2.9 Arıza modelleri (hata enjeksiyonu)

**Taksonomi:**
- Balaban vd. (2009, IEEE Sensors J. 9(12):1907–1917, NASA Ames) şu sınıfları tanımlıyor [doğrulandı, özet]:
  - **bias**: sabit ofset δ,
  - **drift**: zamanla değişen δ(t); doğrusal ve doğrusal olmayan,
  - **scaling**: a(t)·x,
  - **noise**: gürültü artışı,
  - **hard/stuck**: donma.
- Bunlara eklenecekler: **sıçrama (outlier/spike)**, **kayıp (dropout)**, **aralıklı kayıp**, **kısmi bozulma**. Kısmi bozulma örnekleri: DVL'de tek ışın ölü, sonarda sektör kör, IMU'da tek eksen.
- ArduPilot SITL parametreleri iyi bir kontrol listesi [doğrulandı, `SITL.cpp`]: `GYR_FAIL_MSK`, `ACC_FAIL_MSK`, `ACCELn_FAIL`, `GYRn_BIAS`, `GYRn_SCALE`, `ACCn_RND`, `MAGn_FAIL`, `MAG_DELAY`, `SONAR_GLITCH`, `SONAR_RND`, `SONAR_SCALE`, `DRIFT_SPEED/TIME`, `TEMP_*`.

**Katmanlı enjeksiyon önerisi:**

```
gerçek durum → [fiziksel sensör modeli: gürültü/bias/ölçek/sıcaklık]
            → [ölçüm arızası: bias, drift, scale, stuck(freeze), spike, noise×k, NaN/aralık dışı]
            → [kuantizasyon + doygunluk + sensörün kendi geçerlilik bayrakları]
            → [kodlama: Ping/WL/UBX/HIL mesajı]
            → [iletim arızası: gecikme, jitter, paket kaybı, bayt bozulması, CRC hatası, sıra değişimi, baud kayması]
            → kart
```

- Her arıza parametreleri şunlar olmalı: `başlangıç zamanı/koşulu` (ör. "derinlik > 2 m olduğunda"), `süre`, `şiddet`, `rastgelelik tohumu`.
- Senaryolar betik/YAML olarak saklanmalı; aynı tohumla tekrar üretilebilir olmalı.
- **Sensörün kendi raporladığı durum ile gerçek durumun tutarsızlığı da bir arıza sınıfıdır.** Örnek: DVL `velocity_valid=true` derken hız yanlış. Firmware'in "sensöre körü körüne güvenme" davranışı ancak böyle test edilir.

### 2.10 Karta veri aktarma seviyesi

| | (a) Kablo protokolünü taklit (firmware gerçek sürücüyü çalıştırır) | (b) HIL mesajlarıyla ham ölçüm enjeksiyonu |
|---|---|---|
| Test edilen | Sürücü, ayrıştırıcı (parser), CRC, zaman aşımı, yeniden bağlanma, DMA/kesme yükü, baud | Kestirim, kontrol, görev mantığı |
| Gerçekçilik | Yüksek: bayt seviyesi hatalar, gerçek zamanlama | Orta: sürücü katmanı atlanır |
| Donanım ihtiyacı | UART: USB-UART dönüştürücü (ucuz). I²C/SPI: kartın master olduğu sensörler için **slave emülatör MCU** gerekir (PC slave olamaz). Ethernet: kartta yok. | Tek USB kablosu (USB HS device) |
| Determinizm / hız | Baud ile sınırlı; çok kanal; senkronizasyon zor | Yüksek; tek akış; zaman damgası kolay |
| "Yalnız sürücü değişsin" hedefine katkısı | Sürücünün kendisi doğrulanır | Sürücü-üstü arayüzün (HAL sınırı) doğru tasarlanmasını zorlar |
| Örnek | ArduPilot SITL UBX/NMEA GPS emülasyonu | PX4 HITL (MAVLink `HIL_SENSOR`/`HIL_GPS`/`HIL_ACTUATOR_CONTROLS`); ArduPilot SITL JSON (UDP 9002, IMU + konum + hız + attitude, opsiyonel `rng_1..6`) |

**STM32U5A9J-DK kısıtları** [ikincil/doğrulandı, ST ürün sayfası + bayiler]:
- MCU STM32U5A9NJH6Q (Cortex-M33), 2.5 MB SRAM, 4 MB flash.
- USB-C: **USB 2.0 HS, yalnızca device/sink**.
- ST-LINK V3E sanal COM portu, iki adet çift sıralı 2.54 mm genişleme konnektörü, **Ethernet yok**.
- Genişleme konnektörlerinde hangi UART/SPI/I²C'lerin açık olduğu bu araştırmada **doğrulanmadı**; UM2967 kontrol edilmeli.

**Karma yaklaşım:** Ayrıntılar bölüm 3'te.

---

## 3. Mimari için öneriler

### 3.1 Sensör modeli çekirdeği (Rust)
1. **Tek bir `SensorModel` trait'i olmalı.** Trait'in görevleri:
   - `sample(&World, t_measure) -> Measurement` (fiziksel değer + sensörün kendi bayrakları),
   - ayrı bir `FaultChain`,
   - ayrı bir `Encoder` (HIL mesajı, Ping Protocol, WL `wrz`, UBX).

   Gerekçe: Aynı ölçüm hem (a) hem (b) yolundan, hem de SIL'e akabilir. Model ile protokol birbirinden ayrılır.
2. **Deterministik rastgelelik:** Her sensöre ayrı ve tohumlanmış RNG (ör. ChaCha). Tohumlar log başlığına yazılmalı. Gerekçe: arıza senaryolarının tekrarlanabilirliği.
3. **Zaman:** `u64` ns sim saati, olay kuyruğu, `t_measure`/`t_deliver` ayrımı, sensör başına saat kayması (bölüm 2.8).
4. **Parametre dosyaları ürün bazlı olmalı** (`icm42688p.toml`, `dvl_a50.toml`, `ping360.toml`). Her değerin yanında kaynak ve "doğrulandı/tahmin" etiketi tutulmalı. Gerçek sensör alınınca yalnız bu dosya ve gerekirse Allan ölçümüyle bulunan değerler değişir.

### 3.2 Hangi sensör hangi kademeyle başlamalı?

| Sensör | Başlangıç kademesi (Faz 1) | Sonraki kademe | Gerekçe |
|---|---|---|---|
| IMU | Beyaz gürültü + Gauss-Markov bias + açılış bias'ı + ölçek/eksenler arası + sıcaklık katsayısı. ICM-42688-P datasheet değerleri, bias RW için Kalibr önerisiyle şişirme. | Gerçek sensör gelince Allan ölçümüyle yeniden parametreleme (15–24 saatlik kayıt) | Ucuz; kestirici hassasiyeti buna bağlı |
| Basınç | Hidrostatik + ρ + sıcaklık kayması + kuantizasyon + 25 Hz | Dalga/dinamik basınç | Derinlik kontrolünün temeli; GNSS kapısını da sürer |
| Manyetometre | Hard/soft iron (W, V) + itici akımına doğrusal girişim + WMM alanı | Havuz anomali alanı | Yüzey/dalış yön kestiriminde kritik |
| GNSS | Anten derinlik kapısı + hot-start gecikmesi + fix geçişi + PX4 tarzı Gauss-Markov + 120 ms gecikme | Washover olasılığı, çok yollu | Hibrit araç için yüzey/dalış geçişleri ana test senaryosudur |
| DVL | 4 ışın ray-cast + ışın gürültüsü + geçerlilik + mod tablosuna göre değişken hız + `time_of_validity` + ölçek/hizalama hatası; **seri `wrz` protokolü ile (a)** | Kabarcık/türbülans maskesi, su takibi modu | Dip kilidi kaybı, firmware için en sık gerçek dünya arızasıdır |
| Ping1D | T1 (ışın konisinde ray-cast, `profile` + `distance`, Ping Protocol ile (a)) | — | Ucuz; protokol küçük |
| Ping360 | T1 (adım başına koni ray-cast + Lambert + soğurma + Rayleigh speckle; adım başına poz/zaman); Ping Protocol | T4-lite (yüzey/duvar tek sekme) | Yarışma hedef tespitinin ana sensörü olma ihtimali yüksek; protokol ve hareket bozulması kritik |
| FLS (ileride alınırsa) | T2 (GPU raster, Stonefish yöntemi: menzil + Lambert tamponu → compute shader histogram), wgpu ile macOS/Windows | T3 çevrimdışı doğrulama | MCU'ya değil yardımcı bilgisayara gider; SIL kapsamında |
| SSS | T2 (satır bazlı; aracın ilerlemesiyle waterfall) | — | Yarışmada gerekli olup olmadığı belirsiz |
| USBL/SBL, modem | Olay tabanlı: gecikme, bant genişliği, menzille büyüyen hata, kayıp | Çok yollu sıçramalar | Yarışmada kullanılıp kullanılmayacağı belirsiz |
| Kamera | K1 (Akkaynak-Treibitz formülü, post-process) | K2 lamba geri saçılımı + bulanıklık | Görsel tespit varsa yardımcı bilgisayarda |
| Batarya / sızıntı / sıcaklık | Thevenin 1-RC + Coulomb; GPIO olayı; termal 1. derece | — | Güvenlik ve failsafe testleri için zorunlu |
| Hedef (torpido hedefi) | **T0 semantik tespit** (P_d, menzil/kerteriz gürültüsü), sonar ham verisiyle paralel | Ham sonardan gerçek tespit algoritması | Görev mantığını sonar algoritmasından bağımsız test etmeyi sağlar |

### 3.3 Karta aktarım: karma yaklaşım (somut)
1. **Varsayılan yol (b):** USB HS CDC (veya vendor bulk) üzerinden ikili, sürümlü bir HIL protokolü. Her mesaj `sensor_id`, `instance`, `t_measure_ns`, `seq`, yük ve CRC taşır.
   - MAVLink `HIL_SENSOR` hava aracına göre tasarlandığı için (diff_pressure, pressure_alt alanları) yalnız IMU/mag/baro için yeterlidir.
   - DVL, sonar, sızıntı ve batarya için özel mesajlar gerekir.
   - Gerekçe: Tek kablo, yüksek bant genişliği, zaman damgası kolaylığı.
2. **Firmware'de HAL sınırı:** Sürücüler `ImuSample`, `DvlReport`, `SonarPing`, `DepthSample` gibi normalize yapılar üretir. HIL modunda bu yapıları "HIL kaynak sürücüsü" doldurur. Böylece "ileride yalnızca sürücü değişsin" hedefi doğrudan bu sınırın kalitesine bağlanır.
3. **Seçmeli (a) — UART sensörleri, Faz 2:**
   - DVL `wrz`/`wru` (CRC-8), Ping1D/Ping360 (Ping Protocol), GNSS (UBX veya NMEA).
   - PC, USB-UART dönüştürücüyle kartın UART pinlerine bağlanır; simülatör "sensör gibi" bayt üretir ve komutlara yanıt verir.
   - Gerekçe: Bu sensörlerin ayrıştırıcıları ve zaman aşımı mantığı, en sık hata çıkan firmware kodlarıdır. Emülasyonun maliyeti de düşüktür.
4. **(a) — I²C/SPI sensörleri, Faz 3 (opsiyonel):**
   - Ayrı bir "sensör emülatörü" kart (ör. PIO'lu bir MCU) PC'den veri alır ve ICM-42688/MS5837 register haritasını slave olarak sunar.
   - Gerekçe: Gerçek register/FIFO/DRDY davranışı test edilir. Maliyet ve karmaşıklık yüksek olduğu için ancak sürücü riskliyse yapılmalı [doğrulanmadı — mühendislik önerisi].
5. **Ethernet sensörleri (FLS, DVL TCP):** Bu kartta kablo emülasyonu yapılamaz. Bu sensörler SIL'de yardımcı bilgisayar yazılımına (gerçek TCP/UDP protokolüyle) verilir.

---

## 4. Riskler ve açık sorular

1. **Hangi sensörler gerçekten alınacak?** Ping360 mi, FLS mi, SSS mi? Kademe yatırımı buna bağlı. FLS seçilirse GPU (wgpu/Metal) sonar boru hattı ciddi bir iş kalemidir.
2. **Yarışma kuralları ve ortamı:** Havuz mu açık su mu? Tatlı mı tuzlu mu? Hedefin malzemesi ve boyutu? Hedef akustik pinger taşıyor mu? Taşıyorsa hidrofon/TDOA modeli gerekir; bu raporun kapsamı dışında. Havuzda multipath ve manyetik anomali öne çıkar.
3. **Doğrulama verisi yok.** Gerçek sensör olmadan modellerin "gerçekçi" olduğunu kanıtlamak mümkün değil. Halka açık veri setleri (ör. Ping360/Oculus kayıtları) ile görsel/istatistiksel karşılaştırma planlanmalı. Gerçek sensör gelince Allan ve sonar kalibrasyonu yapılmalı.
4. **ICM-42688-P bias kararsızlığı datasheet'te yok.** Değer tahmine dayanacak; geniş aralıkla senaryo taraması (sensitivity sweep) yapılmalı.
5. **DVL A50 iç davranışı belgelenmemiş:** 3 ışınla çözüm, kilit geri kazanım süresi, dönüştürücü tipi (ses hızına duyarlılık) bilinmiyor.
6. **macOS GPU yolu:** Metal'de donanımsal ray tracing API'si wgpu üzerinden olgun mu? Değilse T2 rasterizasyon (Stonefish yöntemi) seçilmeli. Bu yöntem raster hattıyla her iki platformda da çalışır [doğrulanmadı].
7. **STM32U5A9J-DK pin erişimi:** Genişleme konnektörlerinde kaç UART/SPI/I²C açık, ST-LINK VCP hızı ne? UM2967'den doğrulanmalı. Kart geçici olduğu için HAL sınırı kart bağımsız tutulmalı.
8. **Gerçek zaman garantisi:** Windows/macOS'ta simülatörün 1 kHz IMU akışını jitter'siz üretmesi zor. Zaman damgası tabanlı tasarım bu riski azaltır ama firmware'in "teslim zamanı" gerçekçiliği sınırlı kalır.
9. **Ping Protocol'ün Ping360 Ethernet/UDP varyantı** ile UART varyantı arasındaki farklar ve RS-485 baud değerleri doğrulanmadı.
10. **Ainslie-McColm formülü** ve MS5837 OSR tablosu bellekten / ikincil kaynaktan; birincil kaynakla kontrol edilmeli.

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. **Ping360 parametreleri:** 750 kHz; 2° yatay × 25° dikey ışın; 0.75–50 m; menzil çözünürlüğü %0.08; 360° tarama 50 m'de 33 s, 1 m'de 3.42 s (Ethernet). Ping360 mesajlarında açı birimi gradyan (0–399), `sample_period` 25 ns adımlı (80–40000), `number_of_samples` 200–1200.
   Kaynaklar: https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/ , https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/
2. **Ping Protocol çerçevesi:** 'B','R' başlığı, u16 payload_length, u16 message_id, src/dst id, payload, u16 checksum = checksum dışındaki tüm baytların toplamı (little-endian). Ping1D `distance_simple` = #1211 (u32 mm, u8 %).
   Kaynaklar: https://docs.bluerobotics.com/ping-protocol/ , https://docs.bluerobotics.com/ping-protocol/pingmessage-ping1d/
3. **Water Linked DVL A50:**
   - Rapor hızı irtifaya bağlı 2–15 Hz; range mode tablosu (mod 0: 0.05–0.6 m @ 15 Hz … mod 4: 15 m+ @ 2–4 Hz).
   - `velocity_valid` alanı dip kilidi varlığını gösterir.
   - Seri port 115200 8N1; `wrz` CRC-8 (poly 0x07, init 0x00).
   - TCP JSON gecikmesi ort. ~4 ms (en çok 13 ms).

   Kaynak: https://github.com/waterlinked/docs/tree/master/docs/dvl (`dvl-json-protocol.md`, `dvl-serial-protocol.md`, `range-mode.md`)
4. **ICM-42688-P (DS-000347 rev 1.6):** gyro gürültü yoğunluğu 0.0028 °/s/√Hz; ilk ZRO ±0.5 °/s; ZRO sıcaklık kayması ±0.005 °/s/°C; eksenler arası ±%1.25; accel PSD 65 (X/Y) / 70 (Z) µg/√Hz; accel ilk ofset ±20 mg; ±0.15 mg/°C. Datasheet'te bias kararsızlığı değeri yok.
   Kaynak: TDK InvenSense ICM-42688-P datasheet, https://invensense.tdk.com/products/icm-42688-p/
5. **OceanSim karşılaştırması:** OceanSim sonarı RTX A6000'de 28–32 FPS üretirken HoloOcean 1.5–3.8 FPS üretiyor ve sahne başına 317–502 s octree önbelleği gerektiriyor. OceanSim gürültü modeli: Rayleigh toplamsal + Gauss çarpımsal (HoloOcean'dan).
   Kaynak: Song vd., "OceanSim", arXiv:2503.01074, Tablo II ve denklem 2–6.
6. **Choi vd. 2021 performansı:** 512 ışınlı (BlueView P900-90 konfigürasyonu) faz-uyumlu nokta saçıcı modeli i9-9900K + RTX 2080 Ti'de 114 ray/ışın ve 60 m'de 0.5 Hz; 11 ray ve 10 m'de 10 Hz. Speckle karmaşık Gauss genliklerle, yan loblar sinc ağırlıklı ışın toplamıyla üretiliyor.
   Kaynak: Choi vd., Frontiers in Robotics and AI 2021, doi:10.3389/frobt.2021.706646
7. **Stonefish FLS yöntemi:** Fragment shader (menzil, clamp(N·V)·restitution) yazar. Compute shader ışın başına dikey örnekleri menzil kutularına histogramlar. Işın başına Gauss çarpımsal ve kutu başına Gauss toplamsal gürültü eklenir.
   Kaynak: https://github.com/patrykcieslak/stonefish/blob/master/Library/shaders/flsOutput.comp ve `sonarInput.frag`
8. **GNSS su altı ve yeniden kilitlenme:**
   - GPS L1 sinyali sıvı suya en fazla ~3 cm nüfuz eder. Kaynak: J. Geodesy, doi:10.1007/s00190-018-1147-x
   - u-blox NEO-M9N: cold start 24 s, hot start 2 s, 2.0 m CEP. Kaynak: u-blox NEO-M9N datasheet (UBX-19014285).
9. **PX4 simülasyon varsayılanları:**
   - GPS: Gauss-Markov bias τ = 60 s, 120 ms gecikme, 5 Hz.
   - IMU: gyro ND 8.73e−4 rad/s/√Hz, accel ND 6.37e−3 m/s²/√Hz, bias τ 1000 s / 300 s.

   Kaynak: https://github.com/PX4/PX4-SITL_gazebo-classic (`include/gazebo_gps_plugin.h`, `include/gazebo_imu_plugin.h`)
10. **STM32U5A9J-DK donanımı:** Ethernet yok; USB-C portu USB 2.0 HS device/sink (host/OTG değil). Bu nedenle Ethernet tabanlı sensörler kablo seviyesinde emüle edilemez.
    Kaynaklar: https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html , UM2967.

---

## 6. Kaynaklar

**Sonar ürünleri ve protokoller**
- Blue Robotics Ping Sonar: https://bluerobotics.com/store/sonars/echosounders/ping-sonar-r2-rp/
- Blue Robotics Ping360: https://bluerobotics.com/store/sonars/imaging-sonars/ping360-sonar-r1-rp/
- Ping Protocol: https://docs.bluerobotics.com/ping-protocol/ ; Ping1D: https://docs.bluerobotics.com/ping-protocol/pingmessage-ping1d/ ; Ping360: https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/
- Oculus M750d spec sheet (Blueye): https://www.blueyerobotics.com/specsheet/BPS-OCU-M750D/pdf ; VideoRay: https://videoray.com/wp-content/uploads/Oculus_m750d_Specsheet.pdf
- Tritech Gemini 720is (manualslib): https://www.manualslib.com/manual/1083557/Gemini-720is.html?page=10
- Cerulean Omniscan 450 SS: https://www.blueyerobotics.com/specsheet/CER-OMNISCAN-450-SS/pdf ; https://shop.sphengineering.com/products/echonimbus-sss

**Sonar simülasyonu**
- Choi, W.-S. vd., "Physics-Based Modelling and Simulation of Multibeam Echosounder Perception for Autonomous Underwater Manipulation", Front. Robot. AI 2021: https://www.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2021.706646/full
- DAVE image sonar açıklaması: https://field-robotics-lab.github.io/dave.doc/contents/image_sonar_description
- DAVE ROS 2 sonar göçü (GSoC 2025): https://discourse.openrobotics.org/t/gsoc-2025-migrating-and-optimizing-dave-s-physics-based-sonar-plugin-for-ros-2-and-gazebo-harmonic/49835
- HoloOcean dokümanı v2.4.0 sensörler: https://byu-holoocean.github.io/holoocean-docs/v2.4.0/holoocean/sensors.html
- Potokar vd., "HoloOcean: An Underwater Robotics Simulator", ICRA 2022: https://www.ri.cmu.edu/app/uploads/2022/10/Potokar22icra.pdf
- Potokar vd., "HoloOcean: Realistic Sonar Simulation", IROS 2022: https://www.cs.cmu.edu/~kaess/pub/Potokar22iros.html
- HoloOcean 2.0 önizleme: https://arxiv.org/abs/2510.06160
- Song vd., "OceanSim: A GPU-Accelerated Underwater Robot Perception Simulation Framework": https://arxiv.org/abs/2503.01074
- Stonefish kaynak kodu: https://github.com/patrykcieslak/stonefish (shaders/`flsOutput.comp`, `sonarInput.frag`; `src/sensors/scalar/DVL.cpp`); sensör dokümanı: https://stonefish.readthedocs.io/en/latest/sensors.html ; "Stonefish: Supporting ML Research in Marine Robotics": https://arxiv.org/abs/2502.11887
- Cerqueira vd., "A novel GPU-based sonar simulator for real-time applications", Computers & Graphics 68 (2017), doi:10.1016/j.cag.2017.08.008 ; "A rasterized ray-tracer pipeline for real-time, multi-device sonar simulation": https://arxiv.org/abs/2001.03539
- Bell, J.M. & Linnett, L.M., "Simulation and analysis of synthetic sidescan sonar images", IEE Proc. Radar Sonar Navig. 144(4), 1997: https://researchportal.hw.ac.uk/en/publications/simulation-and-analysis-of-synthetic-sidescan-sonar-images/
- Abraham, D.A. & Lyons, A.P., "Novel physical interpretations of K-distributed reverberation", IEEE JOE 27(4), 2002, doi:10.1109/JOE.2002.804324
- NPL ses hızı denklemleri: http://resource.npl.co.uk/acoustics/techguides/soundseawater/underlying-phys.html
- NPL deniz suyu soğurması: http://resource.npl.co.uk/acoustics/techguides/seaabsorption/physics.html

**Seyrüsefer sensörleri**
- Water Linked dokümanları (DVL JSON/serial protokol, range mode, SSS, overview): https://github.com/waterlinked/docs/tree/master/docs/dvl ; https://waterlinked.github.io/
- DVL A50 spec sheet (Blueye): https://www.blueyerobotics.com/specsheet/WL-DVL-A50/pdf
- Water Linked Underwater GPS G2 / Modem M16 / M64: https://github.com/waterlinked/docs/tree/master/docs ; M64 datasheet: https://bluerobotics.com/wp-content/uploads/2019/02/W-MK-19009-1_Modem-M64_Datasheet.pdf
- SeaTrac USBL data sheet: https://videoray.com/wp-content/uploads/SeaTrac-USBL-Data-Sheet.pdf
- Blue Robotics Bar30: https://bluerobotics.com/store/sensors-cameras/sensors/bar30-sensor-r1/ ; MS5837 Python sürücüsü: https://github.com/bluerobotics/ms5837-python

**IMU / manyetometre / GNSS**
- TDK ICM-42688-P: https://invensense.tdk.com/products/icm-42688-p/ (datasheet DS-000347)
- ADIS16470 datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ADIS16470.pdf
- Kalibr IMU Noise Model: https://github.com/ethz-asl/kalibr/wiki/IMU-Noise-Model
- Allan sapması (Inertial Sense): https://docs.inertialsense.com/user-manual/imx/performance/allan_deviation/ ; MEMS Allan analizi (Sensors 2020): https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7506677/
- PX4 Gazebo Classic GPS/IMU eklentileri: https://github.com/PX4/PX4-SITL_gazebo-classic
- NXP AN4246 (Ozyagcilar): https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/sensors/2925/1/AN4246.pdf
- ArduPilot gelişmiş pusula kurulumu (compassmot): https://ardupilot.org/copter/docs/common-compass-setup-advanced.html
- PNI RM3100 breakout datasheet: https://www.digimax.it/media_import/COMPONENTI/PNI/SENSORI%20MAGNETICI/14190/14190_DS_001.pdf
- "Characteristics and limitations of GPS L1 observations from submerged antennas", J. Geodesy (2018): https://link.springer.com/content/pdf/10.1007/s00190-018-1147-x.pdf
- u-blox NEO-M9N datasheet (bayi kopyası) ve SparkFun özeti: https://learn.sparkfun.com/tutorials/sparkfun-gps-neo-m9n-hookup-guide/hardware-overview
- JHU APL, buoyant cable antenna: https://jhuapl.edu/sites/default/files/2024-09/30-4-Karim.pdf

**Kamera**
- Jaffe, J.S., "Computer modeling and the design of optimal underwater imaging systems", IEEE JOE 15(2):101–111, 1990
- Akkaynak, D. & Treibitz, T., "A Revised Underwater Image Formation Model", CVPR 2018: https://openaccess.thecvf.com/content_cvpr_2018/html/Akkaynak_A_Revised_Underwater_CVPR_2018_paper.html

**Yardımcı sensörler, arızalar, HIL**
- Blue Robotics Power Sense Module: https://bluerobotics.com/store/comm-control-power/elec-packages/psm-asm-r2-rp/ ; SOS Leak Sensor: https://bluerobotics.com/store/sensors-cameras/leak-sensor/sos-leak-sensor/ ; Celsius (TMP119): https://bluerobotics.com/store/sensors-cameras/sensors/celsius-sensor-r1/
- Balaban, E. vd., "Modeling, Detection, and Disambiguation of Sensor Faults for Aerospace Applications", IEEE Sensors J. 9(12), 2009: https://ntrs.nasa.gov/api/citations/20090033812/downloads/20090033812.pdf
- ArduPilot SITL parametreleri (`libraries/SITL/SITL.cpp`) ve GPS emülasyonu (`SIM_GPS.h`): https://github.com/ArduPilot/ardupilot/tree/master/libraries/SITL
- ArduPilot SITL JSON arayüzü: https://github.com/ArduPilot/ardupilot/blob/master/libraries/SITL/examples/JSON/readme.md
- MAVLink common mesajları (`HIL_SENSOR`, `HIL_ACTUATOR_CONTROLS`): https://mavlink.io/en/messages/common.html
- PX4 HITL: https://docs.px4.io/main/en/simulation/hitl.html ; PX4 simülasyon/lockstep: https://docs.px4.io/main/en/simulation/
- STM32U5A9J-DK: https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html ; Zephyr kart sayfası: https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html
