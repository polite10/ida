# 05 — Sensörler ve sonar

Sürüm: 0.2 (2026-11-14). Durum: taslak; ikinci inceleme bekliyor. Sürüm 0.2: iskelet 0.7'ye uyum (sabit gövde referans noktası; aralık ölçümlerinde aralık sonu ve süresi; akustik ölçümde yansıma anı, sonar satırında gönderim anı; en yakın adım sınırı; fırlatıcı ve emniyet geri okuması; A7/A5 malzeme kuralı; torpido itkisiz, sonar için yalnız bir hareketli gövde) ve [inceleme/05-sol.md](inceleme/05-sol.md) bulgularının giderilmesi; madde madde yanıt [inceleme/05-yanit.md](inceleme/05-yanit.md).

Bu bölüm [00-iskelet.md](00-iskelet.md) (sürüm 0.7) Bölüm 9, satır 05'in kapsamını yazar: sensör eklenti arayüzü, her sensörün modeli ve hata kaynakları, zamanlama, sonar (ışın izleme, kademeler), arıza modelleri. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir ("00"). Atıf: `05.n` = 00 Bölüm 4 düzeltme satırı, `Çn`, `Mn`, `Kn`/`Ön` = 00'daki çelişki, ölçüm, karar; `DNN #k / Yk / E:…` = `docs/research/dogrulama/NN-codex.md`; `NN §x` = ham rapor bölümü; `0X §y` = bu dizindeki mimari bölüm; `sol md.k` = [inceleme/05-sol.md](inceleme/05-sol.md) maddesi. Ham rapordan alınan her değer önce 00'daki düzeltmeyle karşılaştırıldı; 05 numaralı ham rapordaki Rust'a özgü öneriler kullanılmadı (K3).

**Değerlerin niteliği.** Her sayısal değer ya da model seçeneği bir etiket taşır:

| Etiket | Anlamı |
|---|---|
| **[Ü]** üretici belgesi | Veri sayfası, ürün sayfası, protokol belgesi; yanında doğrulama durumu (ör. "D05 #4 DOĞRU", "rapora göre, ayrıca doğrulanmadı") |
| **[Y]** yayımlanmış ölçüm | Hakemli ya da açık deney sonucu; ölçüldüğü koşulla |
| **[T]** tahmin | Bu bölümün seçtiği varsayılan ya da mimari varsayımı; kaynakta yok. Gerekçesi yanında |
| **[K]** kalibrasyon parametresi | Deneyle uydurulacak sürekli katsayı (yansıtıcılık, TVG eğrisi, sıcaklık katsayısı, gecikme dağılımı). Varsayılanı ayrıca [T]'dir. A5'ten değiştirilir, A6'ya yazılır |
| **[S]** yapısal bilinmez | Cihazın yeteneği ya da iç algoritması (ör. A50'nin üç ışınla çözüm verip vermediği). Seçenek olarak modellenir, uydurulmaz; varsayılan seçenek [T] |
| **[G]** çevresel girdi | Senaryo verisi (WMM alanı, su sıcaklığı, tuzluluk, pH) |
| **[C]** yapılandırma | Seçilmiş ayar (hız, ölçüm aralığı, ADC bit sayısı) |

Bu bölümdeki hiçbir hesap süresi, ışın hızı ya da işlemci yükü ölçülmemiştir; geçtikleri yerde "işlem bütçesi hesabı, ölçüm değil" diye işaretlenir (Ç9, D10 Y4). Bu bölüm kod içermez; imzalar sözleşme işinin girdisidir (iskelet §3).

---

## 1. Amaç ve sorumluluklar

Sensör katmanı sim sunucusunun içinde eklenti olarak çalışır; fizik durumundan, kart yazılımının gerçek bir sürücüden alacağı biçimde ölçüm üretir (iskelet §2). Sorumlulukları:

1. **Eklenti arayüzü ve sensör merkezi (`SensorHub`):** sensörlerin A5'ten kurulması, zamanlayıcıya (04) kaydı, kayıtların teslim kuyruğu ve 06'ya verilmesi.
2. **Sensör modelleri:** IMU, manyetometre, basınç, GNSS, DVL, Ping1D, Ping360, batarya gerilim/akım, sızıntı, fırlatıcı ve emniyet geri okuması (iskelet §5).
3. **Sonar:** Embree ile işlemcide ışın izleme (Ö4); sahnenin A7'den kurulması; kademeler.
4. **Kamera sözleşmesinin sensör tarafı:** zaman damgaları, `CameraRequest` üretimi, gecikme ve kare düşürme. Görüntü 07'nindir.
5. **Arıza zinciri:** ölçüm zincirinden bağımsız arıza uygulaması; 08'in hata enjeksiyonuna verilen arayüz.
6. **Tekrar üretilebilirlik:** sensör başına tohumlu akışlar, sürümlü dağılımlar, yer gerçeğinin kayda eklenmesi.

**Yazmadıkları:** fizik, çevre, su basıncı ve dalga kinematiği (04); A1 mesaj biçimi, adım partisi, parçalama ve iletim katmanı hataları (06); kamera görüntüsü ve su altı optiği (07); A5 biçimi, hata enjeksiyonu arayüzü ve sanal emniyet girdilerinin paneli (08); kestirim (02); sonar verisinde algılama (03). Kablo protokolleri (UART, I2C, SPI, Ping Protocol, Water Linked `wrz`) bu aşamada taklit edilmez (iskelet §1); modeller cihazların alan anlamlarını korur (D05 N1).

**Ürün profilleri** (K10 kapsamında araç parametresidir, gerçek sensör tedariki değildir, K8): IMU TDK ICM-42688-P, basınç MS5837-30BA (Bar30), manyetometre PNI RM3100 sınıfı, GNSS u-blox NEO-M9N, DVL Water Linked A50, tek ışınlı sonar Blue Robotics Ping Sonar (Ping1D), Ping360, güç ölçümü Blue Robotics PSM, sızıntı SOS Leak. Profil dosyaları her değeri `{değer, etiket, kaynak}` ile tutar (05 §3.1 md.4).

---

## 2. İç yapı

### 2.1 Alt modüller ve veri akışı

```
sim/sensors/
  hub/       SensorHub, teslim kuyruğu, sayaçlar, 06'ya boşaltma
  common/    montaj, Gauss–Markov, süzgeç, kuantalama/doyum, iç sıcaklık modeli, RNG anahtarları
  models/    imu, mag, baro, gnss, dvl, ping1d, ping360, camera_contract, battery, leak, safety_io
  sonar/     sahne (Embree), saçıcı hücreleri, yol üretimi, biriktirme, alıcı zinciri, işçi işleri
  faults/    arıza zinciri, durumlu cihaz arızaları
  profiles/  ürün profilleri
```

```
snapshot halkası + IEnvironmentView (04)        A7 sahnesi + A5 geçersiz kılmaları
        │ on_step (aralık biriktirme)                     │
        ▼                                                 ▼
 olay (04 zamanlayıcısı) ─► ÖLÇÜM ZİNCİRİ ─► temel kayıt + ham veri ─► ARIZA ZİNCİRİ ─► teslim kuyruğu ─► drain (06)
                              │ (ölçüm RNG akışı)                     (arıza RNG akışı, kopya üzerinde)       ├► kayıt (08, A6)
                              └► sonar işi (işçi; değişmez iş paketi) ─► sonuç t_teslim'de                   └► paneller (A3)
```

Üç aşama ayrıdır (D05 N1, 05 §2.9):
- **Ölçüm zinciri** sensörün kendi fiziğini ve kusurlarını üretir (gürültü, bias, ölçek, sıcaklık, süzgeç, doyum, kuantalama, cihazın kendi geçerlilik bayrakları). Arıza durumunu hiç okumaz ve her olayda koşulsuz çalışır. Çıktısı **temel kayıt** ile çözümden önceki **ham veridir** (DVL'de ışın başına hız/menzil/geçerlilik; sonarda alıcı öncesi kutu dizisi).
- **Arıza zinciri** temel kaydın ve ham verinin kopyası üzerinde çalışır. Kısmi arızalar (tek ışın, kör sektör, tek eksen) ham kopyaya uygulanır ve çözüm, ölçüm zincirindeki aynı saf işlevle (RNG kullanmaz) kopya üzerinde yeniden hesaplanır. Durumlu cihaz arızaları (motor takılması) ayrı sınıftır (2.7).
- **Çıktı:** teslim kuyruğu kaydı 06'ya verir; A1 kodlaması ve iletim hataları 06'nındır. Aynı kayıt, yer gerçeği ve temel kayıtla birlikte A6'ya gider.

### 2.2 Zaman sözleşmesi

**Adım ve bağlama.** Fizik adımı `h` = 1,25 ms (iskelet §4). Örnekleme olayları 04'ün rasyonel zamanlayıcısıyla **en yakın** adım sınırına bağlanır, sapma en çok 0,625 ms; rasyonel faz birikimli sürüklenme üretmez ama her örnek ideal zamanına eşit değildir (04 §2.11). **Tamamlanma olayları** (bir dönüşümün, dinlemenin ya da raporun fiziksel olarak bitmesi) fiziksel bitişe eşit ya da ondan **sonraki ilk** adım sınırında çalışır; hiçbir hesap fiziksel süresi dolmadan yapılmaz.

**Kayıttaki zaman alanları** (iskelet §3.1):

| Alan | Anlamı |
|---|---|
| `t_meas` | Ölçüm zamanı. Anlık ölçümde geçerlilik anı; aralık boyunca toplanan ölçümde **aralığın sonu**; akustik mesafe ölçümünde **yansıma anı**; menzil profili taşıyan sonar satırında **gönderim anı** |
| `window_ns` | Aralık süresi; anlık ölçümde 0. Tek geçerlilik anı gereken tüketici `t_meas − window_ns/2` kullanır |
| `t_tx` | Akustik gönderim anı (DVL, Ping1D, Ping360); diğerlerinde yok |
| `t_hazır` | Kaydın sim içinde kesin var olduğu an (06 §2.3): dünya iş parçacığında üretilen kayıtta üretildiği adım; işçi sonucunda `t_teslim` (deterministik garanti anı) |
| `t_teslim` | Modellenmiş alım anı (sim zamanı). Karta gönderilmez; kart kendi alım zamanını damgalar (06 §2.8) |

**Nedensellik kuralları:** `t_meas ≤ t_hazır ≤ t_teslim`; aralık ölçümünde `t_teslim ≥ t_meas` (aralık bitmeden teslim yok); akustikte `t_tx ≤ t_meas`. `t_hazır` her zaman bir adım sınırıdır ve `t_teslim`'i içeren kontrol partisinin sınırından sonra olamaz (06'nın atama kuralı bunu gerektirir; `t_hazır` = fiziksel bitişin üst adım sınırı ≤ `t_teslim`'in üst kontrol sınırı).

**Sensör başına zaman tanımları:**

| Sensör | Hız | `t_meas` | `window_ns` | `t_teslim` |
|---|---|---|---|---|
| IMU | kontrol hızı 400 Hz, yedek 200 Hz (iskelet §4, M5) [C] | aralık sonu = kontrol sınırı `t_n` | 2,5 ms / 5 ms | `t_n` (süzgeç gecikmesi değerin içindedir, 2.3.1) |
| Manyetometre | 100 Hz [C/T] | örnekleme anı (dönüşüm süresi bilinmediği için anlık [T]) | 0 | `t_meas` + 1 ms [T] |
| Basınç | 25 Hz [C/T]; üst sınır 2.3.3 | D1 dönüşüm penceresinin sonu | `T_D1` = 18,75 ms | `t_meas + T_D2 + T_okuma` |
| GNSS | 5 Hz [C/T] | çözüm anı | 0 | `t_meas` + 120 ms [T; PX4 simülasyon varsayılanı, D05 #9a, ölçüm değil] |
| DVL | moda göre 2–15 Hz [Ü, D05 #3a] | yansıma anı `t_tx + r̄/c` (05.3) | 0 | `t_tx + 2r̄/c + T_işlem + T_iletim` (2.3.5) |
| Ping1D | 2.4.6 | algılanan ekonun yansıma anı | 0 | `t_tx + T_dinleme + T_işlem + T_iletim` |
| Ping360 | açı başına (2.4.5) | gönderim anı `t_tx`; kutu `i`'nin yansıma anı `t_tx + (i + ½)·T_s/2` | 0 | `t_tx + T_dinleme + T_işlem + T_iletim` |
| Kamera | 07'nin kademesine göre (tam 30 Hz, sade 15 Hz [T]) | pozlama sonu | pozlama süresi `T_poz` | `t_meas + L_sonrası` (2.5) |
| Batarya | 10 Hz [C/T] | aralık sonu | 100 ms (örnek aralığı) | `t_meas` + 1 ms [T] |
| Sızıntı | 10 Hz [C/T] | örnekleme anı | 0 | `t_meas` |
| Fırlatıcı/emniyet geri okuması | 10 Hz ve her değişimde [T] (06 `IDA_SAFETY_IO`) | örnekleme anı | 0 | `t_meas` + 1 ms [T] |

`T_s = sample_period · 25 ns`. Kutu merkezi `(i + ½)` kuralı 03'ün menzil ekseniyle aynıdır (03 §2 md.1) [T; cihazın örnekleme anı belgelenmemiştir].

**Hız ızgarası:** 400 Hz 2 adım, 200 Hz 4, 100 Hz 8, 25 Hz 32, 10 Hz 80, 5 Hz 160 adım; 30 Hz 26⅔ adım (sınırlar 27, 53, 80, …); 15 Hz (sade kamera, DVL mod 0) 53⅓ adım (53/54).

**İş parçacığı:** Ping360 ışın izlemesi işçi havuzundadır (2.4.1). DVL (28 alt ışın) ve Ping1D (2.4.6) sorguları küçük olduğu için dünya iş parçacığında, aynı değişmez alt sahnelerle kurulup aynı adımda bırakılan bir üst sahneyle yapılır [T: sorgu sayısı küçük; süre ölçülmedi, M11]. Geometri gönderim adımında hesaplanır, kayıt tamamlanma olayında üretilir.

**Geçmiş durum:** 04'ün snapshot halkası (64 adım = 80 ms) ve `interpolate(t)` yeterlidir (04 §8'in sorusu): DVL `t_meas` anındaki hızı rapor anında halkadan ara değerler (`t_rapor − t_meas = r̄/c + T_işlem` ≤ 34 ms + `T_işlem`, `T_işlem` ≤ 40 ms [T] ile halka içinde); aralık ölçümleri `on_step` ile kendileri biriktirir; sonar işi kendi değişmez kopyasını taşır.

**Saat kayması:** sensör başına ppm ve faz [K, varsayılan 0]. IMU'da yoktur: kontrol IMU'su adım sınırına bağlıdır (iskelet §4); bu gerçek sensör zamanlamasından bilinçli bir sapmadır.

### 2.3 Seyrüsefer ve sistem sensörleri

**Ortak gösterim.** `O_b` gövde referans noktası: gövdeye sabit geometrik nokta, kütle merkezi değil, torpido ayrılınca yerinde kalır (iskelet §3.1, 04 §2.2). Sensör montajı `{l, R_bs}`: `l` sensörün `O_b`'ye göre konumu (gövde çerçevesi, m), `R_bs` sensörden gövdeye dönüş; ikisi de A5'in fizik gerçeği grubundan, sabittir. `R_nb` gövdeden NED'e dönüş, `ω` gövdenin NED'e göre açısal hızı, `v` `O_b`'nin gövde çerçevesindeki hızı.

**Gauss–Markov süreci** (05.1, D05 N6). Sürekli biçim `ḃ = −b/τ + w`, `w` yoğunluğu `q`; durağan varyans `σ_∞² = qτ/2`. Ayrık ve zaman adımından bağımsız:

`b(t+Δt) = φ b(t) + σ_∞ √(1 − φ²) ξ`,   `φ = e^{−Δt/τ}`,   `ξ ~ N(0,1)`, `Δt` iki güncelleme arasında gerçekten geçen süre.

Başlangıç `b(0) ~ N(0, σ_∞²)` (durağan başlangıç; geçiş evresi yok). Rastgele yürüyüş ayrı süreçtir: `b(t+Δt) = b(t) + √(q Δt) ξ`; sabit `σ_∞` ile `τ → ∞` limiti süreci dondurur, rastgele yürüyüş vermez (sol md.8). PX4 GPS bias kodu kullanılmaz (05.1).

**Gürültü sınıfları ve yoğunluk konvansiyonu** (05.8, D05 E:IMU gürültüsü; sol md.7, md.9):
- **Yoğunlukla tanımlı kanal** (IMU): üretici gürültü yoğunluğu `ASD` **tek taraflı** kabul edilir (TDK'nin 100 Hz bantta verdiği RMS değeri `ASD·√100` bu yorumu destekler; inceleyicinin TDK okumasıdır, bu oturumda bağımsız okunmadı). Dikdörtgen aralık ortalamasının eşdeğer gürültü bant genişliği `1/(2Δt)`'dir; ayrık σ = `ASD/√(2Δt)`. Kalibr/Allan katsayısı `N = ASD/√2`, Allan sapması `σ_A(τ) = N/√τ` [T: konvansiyon seçimi].
- **Sabit dönüşüm yapılandırmalı kanal** (basınç OSR, manyetometre çevrim sayısı, ADC): gürültü **örnek başına σ** olarak verilir ve çıktı hızı değişti diye ölçeklenmez.
- **"10 kat" şişirme uygulanmaz** (05.8, D05 Y9): sıcaklık ve ölçek hataları açıkça modellendiği için parametreler şişirilmez; şişirme gerekiyorsa 02'nin süreç ve ölçüm gürültüsü ayarıdır. Böylece aynı hata iki kez sayılmaz.

**Gövde içi sıcaklık** (IMU, basınç, manyetometre ve batarya ölçerinin sıcaklık terimlerini sürer; 05 §2.7): `C dT/dt = P_e − k (T − T_su)`, `C` J/K, `k` W/K, `P_e` W [T: 2000 J/K, 2 W/K, 20 W; 04'ün aviyonik yükü ile aynı güç]; fizik adımında üstel ayrık güncelleme `T ← T_∞ + (T − T_∞) e^{−h k/C}`, `T_∞ = T_su + P_e/k`. `T_su` 04'ten [G]. Kapatılabilir; o zaman `T` sabit A5 değeridir. Sıcaklık yasaları doğrusaldır: `b_T (T − T₀)`, `T₀` = 25 °C [T].

#### 2.3.1 IMU (ICM-42688-P profili)

**Yer gerçeği** (04 §3: `a_n` = `O_b`'nin NED eylemsiz ivmesi, `ω̇`, `g_n`):

`f_b = R_nbᵀ (a_n − g_n) + ω̇ × l + ω × (ω × l)`,   `f_s = R_bsᵀ f_b`,   `ω_s = R_bsᵀ (ω + R_nbᵀ ω_ie,n)`

Dünya dönüşü `ω_ie` (7,292115·10⁻⁵ rad/s, WGS-84 sabiti; NED'de enlem A5 başlangıcından) varsayılan açıktır [T: gerçekçilik önceliği]; kapatılabilir. Dünya dönüşünün ivmeölçerdeki Coriolis etkisi (`2ω_ie × v` ≈ 1,5·10⁻⁴ m/s², 1 m/s'de) ihmal edilir [T: ayrık gürültünün 1/50'sinden küçük]; `g_n` 04'ün yerçekimi ile aynıdır.

**Ölçüm zinciri** (fizik adımında, her eksen için; sıra sabittir):
1. `x' = S M x + b_on + b_gm(t) + b_T (T − T₀) + κ_nl x|x|/FS` (`x` = `ω_s` ya da `f_s`; `S` köşegen ölçek `1 + s_i`; `M` eksenler arası, köşegeni 1; `κ_nl` doğrusalsızlık; `FS` ölçüm aralığı).
2. Süzgeç kapalıysa (varsayılan): aralık sonunda `x'`'in fizik adımı sınırlarındaki yamuk ortalaması (400 Hz'te üç nokta) + `n ~ N(0, (ASD/√(2Δt))²)`. Süzgeç açıksa: her fizik adımında `x' + n_h`, `n_h ~ N(0, ASD²/(2h))`, birinci derece süzgeç `y ← y + (1 − e^{−h/τ_f})(x' + n_h − y)`, sonra aralık ortalaması; `τ_f` [K]. Süzgeç kapalıyken `ASD/√(2Δt)` kısayolu yalnız ideal aralık ortalaması için geçerlidir; dar süzgeçte gürültü fizik hızında üretilir ve aynı süzgeçten geçer.
3. Doyum (`|y| > FS` → kırp, `flags.doymuş`), sonra kuantalama.

| Parametre | Değer | Etiket |
|---|---|---|
| Jiroskop yoğunluğu | 0,0028 °/s/√Hz = 4,887·10⁻⁵ rad/s/√Hz | [Ü, D05 #4 DOĞRU] |
| İvmeölçer yoğunluğu | X/Y 65, Z 70 µg/√Hz = 6,374·10⁻⁴ / 6,865·10⁻⁴ m/s²/√Hz | [Ü, D05 #4] |
| Ayrık σ, 400 Hz | jiroskop 6,911·10⁻⁴ rad/s; ivme X/Y 9,015·10⁻³, Z 9,708·10⁻³ m/s² | türetilmiş |
| Ayrık σ, 200 Hz | jiroskop 4,887·10⁻⁴ rad/s; ivme 6,374·10⁻³ / 6,865·10⁻³ m/s² | türetilmiş |
| Açılış biası `b_on` | jiroskop ±0,5 °/s (±8,727·10⁻³ rad/s); ivme ±20 mg (±0,1961 m/s²) | [Ü, D05 #4]; dağılım N(0, (sınır/3)²) [T]; her koşuda bir kez, "sensör güç çevrimi" olayında yeniden |
| Sıcaklık katsayısı `b_T` | jiroskop ±0,005 °/s/°C (8,727·10⁻⁵ rad/s/°C); ivme ±0,15 mg/°C (1,471·10⁻³ m/s²/°C) | [Ü, D05 #4]; açılışta N(0, (sınır/3)²) [T] |
| Ölçek / eksenler arası | jiroskop ±%0,5 / ±%1,25; ivme ±%0,5 / ±%1 | [Ü, D05 #4]; aynı dağılım kuralı [T] |
| Doğrusalsızlık `κ_nl` | jiroskop ±%0,1 | [Ü, 05 §2.4.1]; ivmede 0 [T] |
| Bias kararsızlığı | veri sayfasında yok | [Ü: yok, D05 #4] |
| GM `τ` | jiroskop 1000 s, ivme 300 s | [T; PX4 IIM-42653 simülasyon varsayılanı, D05 #9c] |
| GM `σ_∞` | jiroskop 4,848·10⁻⁵ rad/s (10 °/h); ivme 1·10⁻³ m/s² | [K], varsayılan [T]; SIL'de geniş aralıkta taranır (05 §4 md.4) |
| 400 Hz GM yeniliği | jiroskop φ = 0,9999975, σ = 1,084·10⁻⁷ rad/s; ivme φ = 0,9999917, σ = 4,082·10⁻⁶ m/s² | türetilmiş (sol yeniden hesabı ile aynı) |
| Ölçüm aralığı, çözünürlük | ±500 °/s, ±4 g, 16 bit → 2,663·10⁻⁴ rad/s, 1,197·10⁻³ m/s² LSB | [C/T] |

#### 2.3.2 Manyetometre

`B̃ = W R_bsᵀ R_nbᵀ B_n + V + b_T,m (T − T₀) + Σ_k (K_k⁺ [s_k > 0] + K_k⁻ [s_k < 0]) I_k + n`

- İç birim tesla. `B_n` (NED) senaryo konumu için A5'te [G; WMM/IGRF ile önceden hesaplanır, entegre model yok]; A5'te nT verilirse `10⁻⁹` ile çevrilir.
- `W` yumuşak demir + ölçek + diklik, `V` sert demir [K; varsayılan `W` köşegeni 1 ± %2, `‖V‖` ≤ 5 µT, T]. `b_T,m` [K, varsayılan 0].
- **İtici girişimi** (ArduPilot `compassmot` akıma doğrusal kabulü, 05 §2.4.2): `I_k` 04'ün negatif olmayan besleme akımıdır (04 §2.7.4); motor yönü `s_k = sign(u_k)` 04'ün komutundan alınır ve yön bağımlılığı ayrı katsayılarla (`K_k⁺`, `K_k⁻`) tanımlanır. Fırçasız motorun dönen faz alanının ortalaması bu doğrusal yaklaşımla temsil edilir [T]. Varsayılan büyüklük: tam itkide girişim alanın %20'si [K/T; ArduPilot "< %30 iyi" ölçütünün içinde, Ü].
- Gürültü örnek başına σ = 20 nT (100 çevrim) [Ü, ikincil kaynak]; çıktı hızıyla ölçeklenmez. Havuz donatısı anomalisi isteğe bağlı konuma bağlı Gauss lekeleri [T, 05 §2.4.2 doğrulanmadı].

#### 2.3.3 Basınç / derinlik (MS5837-30BA profili)

- **Yer gerçeği:** sensör ağzının dünya konumunda `p_abs = p_atm + p_04(p_ağız, t) + C_p ½ρ|v_r|²`. `p_04` 04 §2.4'ün gövde integralinde kullandığı hidrostatik + birinci mertebe dalga basıncıdır ve 04'ten `water_pressure` sorgusu olarak beklenir; aynı formül tek yerden gelir, dalga basıncı iki kez eklenmez. Ağız su üstündeyse `p_04 = 0`. `C_p` ağız konumuna bağlı dinamik basınç katsayısı [K, varsayılan 0]; `C_p = 1`, 1 m/s'de 500 Pa ≈ 5,1 cm görünür derinlik eder.
- **Zamanlama:** OSR 8192'de tek dönüşüm en çok 18,08 ms [Ü, D05 E5 DOĞRU]. Pencere en büyük süreyi temsil edecek biçimde yukarı yuvarlanır: `T_D1 = T_D2 = 15 h = 18,75 ms` [T]. Değer D1 penceresi boyunca `on_step` ile alınan yamuk ortalamadır; `t_meas` = D1 sonu, `window_ns` = 18,75 ms; teslim D2 penceresi ve okuma (`T_okuma` = 1 ms [T]) sonrası. Bir örnek döngüsü 2 × 18,75 + 1 = 38,5 ms; 25 Hz (40 ms) sığar. Yapılandırılan hız `1/(T_D1 + T_D2 + T_okuma)` ≈ 25,97 Hz'i aşarsa senaryo reddedilir.
- **Hatalar:** örnek başına σ = 0,2 mbar = 20 Pa (≈ 2,04 mm su) [Ü, D05 E5]; mutlak doğruluk 0–6 bar ve 0–45 °C'de ±50 mbar (≈ ±0,51 m) [Ü, 00 Bölüm 6] → açılış ofseti N(0, (50 mbar/3)²) [T]; firmware yüzeyde sıfırlar (02 §2.3). Sıcaklık kayması `b_T,p (T − T₀)` [K, varsayılan 0]. Kuantalama 10 Pa (≈ 1,02 mm) [T; doğrulanmadı]. 2 mm çözünürlük 2 mm doğruluk değildir (D05 E). Sıcaklık çıkışı: gövde içi sıcaklık + N(0, (4 °C/3)²) açılış ofseti [Ü ±4 °C, 05 §2.3.2; dağılım T].

#### 2.3.4 GNSS (NEO-M9N profili)

- **Ölçülen:** anten konumu (`lat_e7`, `lon_e7`, `alt_mm`; NED → WGS-84 `double` dönüşümü A5 başlangıcından), NED hızı, `eph`, `epv`, `fix_type`, `n_sv`.
- **Uydu havuzu** [T]: koşu başında `N_sv` = 20 uydunun yükselme açısı `e_j` A5'ten ya da tohumdan çekilir (koşu boyunca sabit; uydu hareketi modellenmez). Anten üzerindeki su kalınlığı `w = max(0, ζ(x,y,t) − z_anten,yukarı)` her örnekte 04'ün yüzey yüksekliğinden; uydu `j` için su yolu `w_j = w / sin e_j`. İzleme durumu uydu başına iki durumlu bir süreçtir: izleniyorsa `w_j > w₅₀ + s` iken kaybedilir; kayıptaysa `w_j < w₅₀ − s` olduktan sonra yeniden edinim süresi `T_edin` [K] dolunca izlenir. Geçiş bandında (`|w_j − w₅₀| ≤ s`) örnek başına kayıp olasılığı `1/(1 + e^{−(w_j − w₅₀)/s})`. `w₅₀ = 30 mm` [Y; tatlı suda belirli jeodezik alıcı/antende 30–35 mm gözlem, D05 #8a; bu alıcıya aktarımı K], `s = 5 mm` [K]. Dalga taşması bu yolla kendiliğinden aralıklı kayıp üretir; "3 cm" fiziksel duvar değildir (05.4).
- **Fix ve kalite:** `n_sv ≥ 4` → 3D, `= 3` → 2D, `< 3` → NO_FIX. Konum σ uydu sayısıyla büyür: `σ_eff = σ_nom √(N_sv,ref / n_sv)` [T: DOP yerine kaba yaklaşım], `N_sv,ref` = 12 [T]. Fix kaybolmadan önce doğruluk böylece bozulur.
- **Yeniden kilitlenme bir durum makinesidir, garanti süre değildir** (05.5): bütün izleme kaybından sonra ilk fix süresi kesinti süresine göre seçilen log-normal dağılımdan çekilir: kesinti < `T_kısa` [K, 10 s] hızlı yeniden edinim (medyan 1 s [T]); ≤ `T_efemeris` [T; 2–4 saat, doğrulanmadı] sıcak başlangıç (medyan 2 s, [Ü] 2 s'nin komutla başlatılan, −130 dBm, oda sıcaklığı test koşulu tipik değeri olduğunu not ederek; dağılım K); daha uzun soğuk başlangıç (medyan 24 s, [Ü] dört takımyıldız yapılandırması tipik değeri). Bu değerler alt sınır değil medyandır. Edinim sürerken anten yeniden suya girerse edinim sayacı sıfırlanır [T]. Fix'ten sonra ek hata `e_yak(t) = e₀ e^{−t/τ_yak}` ile nominale iner [K].
- **Hata bütçesi:** 2,0 m CEP [Ü, D05 #8b, açık gökyüzü üretici koşulu] dairesel normalde `σ = CEP/√(2 ln 2)` = 1,699 m (eksen başına). Varyans paylaşımı: GM biası `σ_GM = √0,9 σ` = 1,611 m (`τ` = 60 s [T], ayrık biçimle), beyaz gürültü `σ_b = √0,1 σ` = 0,537 m, örnek başına bağımsız [T]. Düşey: aynı paylaşımla `1,5 σ` [T]. Hız: eksen başına beyaz gürültü 0,05 m/s [T]. Raporlanan `eph` = `σ_eff` (eksen başına 1σ yatay), `epv` = düşey 1σ, ikisi de × `k_rapor` [K, varsayılan 1]; `k_rapor ≠ 1` "raporladığı kalite ile gerçek tutarsız" sınıfını üretir. `eph`'in anlamı 02 ile bu tanımla eşlenir (02 §8.1 md.3).

#### 2.3.5 DVL (Water Linked A50 profili)

- **Geometri** [Ü, 05 §2.3.1; rapora göre, D05'te ayrıca sınanmadı]: 4 ışın, ışın açısı 22,5°, yarı güç genişliği 4,4°. Işın kimlikleri 0–3, DVL çerçevesinde azimutlar `ψ_i = 45° + 90° i` [T; A50'nin düzeni bu depoda doğrulanmadı]; birim vektör `b_i = [sin22,5° cos ψ_i, sin22,5° sin ψ_i, cos22,5°]` (z dibe doğru).
- **Işın başına menzil:** ışın konisi içinde 7 alt ışın [T] sonar sahnesinde (2.4.1) izlenir; alt ışın ağırlığı ışın deseninden. Işın menzili `r_i` = geçerli alt ışın kesişim mesafelerinin ağırlıklı ortalaması [T]; ışın geçerli sayılması için ağırlığın en az yarısının kesişmesi gerekir [T]. Hız izdüşümü nominal ışın ekseni `b_i` boyuncadır.
- **Işın geçerliliği:** `r_i cos22,5°` (ışının dikey karşılığı) etkin modun irtifa aralığında; geliş açısı < `θ_max` [K, 60° T]; dönüştürücü suda; kabarcık/türbülans düşürme olasılığı yüzeye yakınlık ve itici akımıyla [K, 05 §2.3.1 md.7 beklenti]. Aracın kendi geometrisi maskeyle dışlanır.
- **Işın hızı** (yansıtıcı yüzeye göre bağıl; sabit dipte yüzey hızı 0, hedef örneğine çarpan ışında o gövdenin noktasal hızı `PoseSet`'ten):

  `u_i = (c_ayar / c_gerçek)^κ · b_iᵀ R_bsᵀ (v + ω × l − R_nbᵀ v_yüzey,n) + n_i`

  Doppler kayması `Δf ≈ 2 f₀ v_ışın / c_gerçek`; cihaz hızı `c_ayar` ile çözer, bu yüzden piston dönüştürücüde çıktı `v · c_ayar/c_gerçek`'tir (1500 / 1475 m/s ve 1 m/s'de 0,98333 m/s; sol md.13, FFI/Kongsberg §5.2). `κ` dönüştürücü tipine bağlıdır: piston 1, faz dizili yatayda ≈ 0 [S; A50 tipi doğrulanmadı, varsayılan 1 T]. `n_i ~ N(0, σ_b²)`, `σ_b² = σ₀² + (k_r r_i)² + (k_u u_i)²`, varsayılan `σ₀` = 1 mm/s, `k_r` = 0,2 mm/s/m, `k_u` = 0,001 [K/T]. Uzun dönem ölçek hatası ±%1,01 [Ü, ikincil] açılışta N(0, (%1,01/3)²) [T]; montaj hatası `δq_montaj` [K].
- **Çözüm** (saf işlev, RNG yok): geçerli ışınlarla ağırlıksız en küçük kareler `v̂ = (HᵀH)⁻¹Hᵀu`, kovaryans `σ_b² (HᵀH)⁻¹` (ışın σ'ları farklıysa ağırlıklı). Simetrik dört ışında eşit σ ile `σ_x = σ_y = 1,848 σ_b`, `σ_z = 0,541 σ_b` (sol yeniden hesabı; test referansı). Üç ışınla çözüm [S; varsayılan kapalı, T]: kapalıyken 4 geçerli ışın gerekir. `fom` = kovaryans köşegeninin en büyüğünün karekökü [T; üretici tanımı okunmadı]. `altitude` = geçerli ışınlarda `r_i cos22,5°` ortalaması [T]. Hız çözünürlüğü 0,1 mm/s, en büyük hız 3,75 m/s [Ü].
- **Mod durum makinesi:** mod tablosu (0: 0,05–0,6 m 15 Hz; 1: 0,3–3 m 10 Hz; 2: 1,5–14 m 5–6 Hz; 3: 7,7–36 m 7–8 Hz; 4: 15 m+ 2–4 Hz) [Ü, D05 #3a]. Örtüşen aralıklar histerezis işlevi görür: etkin mod, irtifa aralığının dışına 3 ardışık pingte çıkınca komşu moda geçer [T]. Mod içindeki ping aralığı, tablodaki hız aralığından düzgün çekilir [T]. Her 10 s'de bir pingte daha yakın mod denenir [Ü, 05 §2.3.1 md.5].
- **Kilit:** geçerli ışın sayısı gerekenin altındaysa `vel_valid = false`, hız 0, irtifa −1, çözülemeyen ışında mesafe −1 (D05 E:DVL kodlayıcısı). 3 ardışık kilitsiz pingten sonra arama: modlar etkin moddan başlayarak dışa doğru sırayla denenir, her deneme o modun ping aralığıyla; geçerli çözüm bulunan modda kilit kurulur [T]. Ek rastgele arama gecikmesi yoktur; gecikme taramadan doğar.
- **Su takibi** (D05 E): yaklaşık 1,5–4,5 m aşağıdaki su kolonuna göre hız [Ü]; ışınlar boyunca `water_velocity` üç noktada örneklenip ortalanır [T]; 2 Hz [Ü]; `track` alanı. Kapalı varsayılan [C].
- **Nedensel zaman zinciri** (05.3, Ç13, D05 N4): gönderim `t_tx` en yakın adım sınırında; geometri `t_tx` pozuyla. Kilitliyken `t_meas = t_tx + r̄/c` (`r̄` geçerli ışın menzillerinin ortalaması); rapor `t_rapor = t_tx + 2r̄/c + T_işlem` (`T_işlem` [K, 5 ms T]); kayıt `t_rapor`'dan sonraki ilk adımda üretilir ve hız `interpolate(t_meas)` ile alınır; `t_teslim = t_rapor + T_iletim`. Işın menzilleri farklıysa her ışının gerçek yansıma anı `t_meas`'tan en çok `(r_maks − r_min)/c` kadar ayrılır (2 m farkta 1,33 ms); tek `t_meas` bu sınırla yaklaşıktır. Kilitsizken yansıma yoktur: `t_meas = t_tx`, rapor `t_tx + 2 r_maks,mod/c + T_işlem`'de [T]. `T_iletim`: seri profilde satır bayt sayısı × 10 / baud (115200 varsayılan [Ü, D05 #3c]); TCP profilinde ortalama 4 ms, σ 2 ms, [0,5; 13] ms aralığına kırpılmış normal [Ü üretici gözlemi, garanti değil, 05.12; kırpma T]. `t_rapor` 06'nın `t_report_ns`'idir.

#### 2.3.6 Batarya gerilim/akım (PSM profili)

- **Yer gerçeği:** 04'ün `V_bara`, `I_paket` değerleri; örnek aralığı (100 ms) boyunca `on_step` ile ortalanır; `t_meas` aralık sonu.
- **Ölçüm hattı güç hattından ayrıdır** (11.12, D11 E:Güç hattı): ters ölçek `V_a = V_bara / 11,0`, `I_a = 0,330 + I/37,8788` [Ü, D11 DOĞRU; 100 A'de ≈ 2,970 V]. Analog gerilimlere örnek başına beyaz gürültü (PWM kaynaklı, `σ_V,a`, `σ_I,a` [K, 2 mV T]; aralık ortalaması sonrası örnek başına σ, hızla ölçeklenmez), akım ofsetinin sıcaklık kayması `b_T,I (T − T₀)` [K, 0 T], ADC kuantalaması (bit sayısı ve referans [C]; 12 bit, 3,3 V T), ADC aralığı dışında doyum ve `flags.doymuş`; sonra aynı ölçeklerle volt ve ampere çevrilir. 100 A sürekli değer değildir.

#### 2.3.7 Sızıntı (SOS Leak profili)

Prob başına bit (`mask`). Yer gerçeği senaryonun ya da 08'in "su girişi" olayıdır (04 su girişini modellemez). Tepki süresi üretici belgesinde yok [Ü: yok] → algılama gecikmesi [K, 1 s T]. Sahte pozitif oranı [K, 0 T]. Anlık örnek.

#### 2.3.8 Fırlatıcı ve emniyet geri okuması

Gerçek bir aracın anahtar ve varlık algılayıcısı gibi bir sensördür (03 §2.8, iskelet §5).

- **Yetkili durum** sim tarafındadır ve geri okumadan ayrıdır: tüp `dolu` ve fırlatıcı `emniyet` durumu 04'ün snapshot'ında (04 §3); sanal emniyet girdileri (acil durdurma, silahlandırma fişi, emniyet pimi, bakım) sim sunucusunda durur, 08'in paneli ve A5 zaman çizelgesi değiştirir. Atışın uygulanıp uygulanmayacağına geri okuma değil yetkili durum karar verir (04). Kart yazılımı bu durumları değiştiremez.
- **Ölçülen** (`ida_fc_launcher_t` alanları, 03 §3.1): tüp başına `dolu`, `emniyet_açık`, `fiş_takılı`, `pim_çıkık`; araç başına `acil_durdurma`, `bakım`.
- **Zamanlama:** 10 Hz periyodik + yetkili durumdaki her değişimde ek örnek; değişimin algılanması anahtar gecikmesi `T_anahtar` kadar sonra olur [K, 10 ms T; sıçrama bastırma dahil]. `t_meas` örnekleme anı; `t_teslim = t_meas` + 1 ms [T]. `seq` her örnekte artar; tazelik kartta `seq` ve `t_meas` ilerlemesiyle denetlenir (iskelet §4).
- **Bozulabilirlik:** arıza zinciri bit çevirme, donma, kayıp ve geç teslim uygular (2.7); böylece kartın emniyet için geri okumaya güvenmediği sınanır (03 §2.7). Yer gerçeği yan verisi yetkili durumdur.

### 2.4 Sonar

#### 2.4.1 Sahne, malzeme ve değişmezlik

- **Tek kaynak:** sonar geometrisi A7'nin glTF + manifest kaynağından, 04 ve 07 ile aynı yükleyici ve glTF → gövde dönüşümüyle kurulur (D10 N8). Manifest özeti 04 ve 07'ninkiyle karşılaştırılır (07 §2). Akustik ağlar `SON_*` düğümleridir; yoksa `COL_*`'dan türetilir. Türetilmiş ağlar depoya girmez; derlemede sabitlenmiş araç sürümüyle üretilir ve manifest özetiyle doğrulanır (iskelet A7).
- **Malzeme:** her üçgen `malzeme_id` taşır. Varsayılan akustik değerler A7 malzeme kaydında, senaryoya özgü kalibrasyon A5'te aynı kimlikle durur; geçersiz kılmalar A6'ya yazılır (iskelet A7). Alanlar: `μ` geri saçılım katsayısı, geliş açısı yasası (Lambert `μ cos²θ` ya da `μ cos θ`), speküler basınç yansıma katsayısı `R_s`, pürüzlülük çarpanı `γ_p`. Hepsi [K]: 750 kHz'te hedef ve karo yansıtıcılığı bilinmiyor (10 §2.10); APL-UW modeli 750 kHz'e kalibrasyonsuz taşınmaz (10.10).
- **Statik sahne** (havuz, taban, batimetri, statik engeller) yüklemede bir kez kurulur ve commit edilir; sonra hiç değiştirilmez. Her hareketli gövde (hedef, torpido) kendi ağıyla ayrı, commit edilmiş ve değişmez bir alt sahnedir.
- **İşe özel üst sahne** (sol md.25): her sonar işi kendi üst sahnesini işçide kurar: statik sahnenin ve hareketli alt sahnelerin örnekleri (instance), dönüşümleri işin `PoseSet`'inden; tam kademede işe özel su yüzeyi yaması. İşin üst sahnesi yalnız o işe aittir; kurulduktan ve commit edildikten sonra değiştirilmez, iş bitince bırakılır. Böylece hiçbir sahne commit ya da sorgu sırasında değişmez (Embree `rtcCommitScene` sözleşmesi, sol md.25'in kaynağı). Paylaşılan alt sahnelerin aynı anda birden çok üst sahnede örneklenmesinin Embree'de izinli olduğu sözleşme işinde belgeden doğrulanır (bu depoda doğrulanmadı); doğrulanamazsa yedek: statik sahneyi ve alt sahneleri her işçiye ayrı kopyalamak (bellek bedeli).
- **İş paketi** değişmezdir ve paylaşımlı sahiplikle taşınır: `PoseSet` (04 snapshot'ından `t_tx` anının araç, hedef ve torpido pozları ve hızları), dalga listesinin değişmez kopyasına işaretçi (`WaveSpectrumSet`, sürümlü; 04 §3), cihaz yapılandırması, kademe, RNG anahtarı, `epoch` (çalıştırma sıfırlama sayacı).
- **Sınırlar:** sonar örneği başına bekleyen iş en çok 4 [T]; aşılırsa yeni ping düşürülür ve sayılır. `World::reset` ile `epoch` artar; eski `epoch`'lu sonuçlar atılır.
- **Su yüzeyi:** sade kademede ortalama yüzey düzlemi; tam kademede araç çevresinde dalga listesinden işçide üretilen yama (ızgara aralığı `Δ_y` [T, 5 cm]); yamada doğrusal ara değerlemenin yükseklik hatası ızgara aralığına bağlıdır ve 10.7'nin karşı örneğindeki gibi kayan nokta farkından büyüktür; `Δ_y` en kısa dalga boyunun 1/8'inden büyük seçilemez [T].
- **Ses hızı** [G girdileriyle]: tatlı suda UNESCO/Chen–Millero (Wong–Zhu katsayıları), deniz suyunda Mackenzie (05 §2.2.1; NPL sayfaları bağımsız doğrulamada açılmadı, D05 E:Erişim); A5'ten sabit `c` ile geçersiz kılınabilir. Tuzluluk 04'ten beklenir. Havuz ölçeğinde (< 50 m) kırılma ihmal edilir [T, 05 §2.2.1 mühendislik yargısı; ortamdan bağımsız eşik değildir].

#### 2.4.2 Akustik büyüklükler ve birimler

İç hesap fiziksel basınç ya da yoğunluk değil, **normalize** büyüklükler taşır (sol md.17):
- `p̃`: kaynak seviyesine göre normalize karmaşık basınç genliği (birimsiz; referans: dönüştürücü ekseninde 1 m'deki kaynak basıncı).
- `Ĩ = |p̃|²`: normalize yoğunluk (birimsiz). Fiziksel yoğunluk `p_rms²/(ρc)` gerekmez, çünkü alıcı zinciri (2.4.4) zaten [K] kazanç ve TVG ile eşlenir.
- dB: `20 log₁₀|p̃|` = `10 log₁₀ Ĩ` (re kaynak seviyesi, 1 m). Sıfır tabanı: `Ĩ < 10⁻¹⁵` iken `L_min`'e kırpılır.

| İlişki | Genlik `p̃` | Yoğunluk `Ĩ` |
|---|---|---|
| İki yönlü küresel yayılım, doğrudan yol, nokta saçıcı | `1/r²` | `1/r⁴` |
| Soğurma (α dB/m), toplam yol `L` | `10^{−αL/20}` | `10^{−αL/10}` |
| Işın deseni (tek yönlü genlik deseni `B`, aynı dönüştürücü gönderir ve alır) | `B_tx B_rx = B²` | `B⁴` |

Doğrudan yolda `L = 2r`; genlik soğurması `10^{−2αr/20}`, yoğunluk `10^{−2αr/10}` (05.11).

**Saçıcı.** Işın `j`'nin yüzeyle kesişimi alanı `dA_j = r² dΩ_j / cos θ_geliş` olan bir saçıcıdır (düzlemsel yerel yüzey yaklaşımı; sıyıran gelişte `cos θ` alt sınırı 0,05 [T]). Geri saçılım `σ_bs(θ)` malzemenin geliş açısı yasasından. Doğrudan yol katkısı:

`p̃_j = B(θ_j, φ_j)² · √(σ_bs dA_j) · g_c · e^{i 2 k r_c} · 10^{−2α r_j/20} / r_j²`

**Soğurma** `α(f, T, S, z, pH)`: varsayılan üretici Ainslie–McColm biçimidir (05 §2.2.1, ham rapordaki ifade):

`α [dB/km] = 0,106 f₁f²/(f₁² + f²) e^{(pH − 8)/0,56} + 0,52 (1 + T/43)(S/35) f₂f²/(f₂² + f²) e^{−z/6} + 0,00049 f² e^{−(T/27 + z/17)}`,   `f₁ = 0,78 √(S/35) e^{T/26}`, `f₂ = 42 e^{T/17}`

`f` kHz, `T` °C, `S` ‰, `z` km, `pH` [G, varsayılan 8 T]; sonuç 1000'e bölünerek dB/m. **Formül ve tatlı su uygulaması doğrulanmadı** (05.11; sol md.21: özgün makale ve NPL erişilemedi); geçerlilik alanı bu depoda bilinmiyor. Tatlı suda (`S = 0`) yalnız saf su terimi kalır [T]; aritmetiği 20 °C'de 750 kHz için 0,1314 dB/m, 115 kHz için 0,00309 dB/m verir. α sonar başına A5'ten geçersiz kılınabilir [K].

**Işın deseni** `B(θ, φ)` (tek yönlü genlik): yatay ve düşey −3 dB genişlikleri Ping360 için 2° × 25°, Ping1D için 25° [Ü, D05 #1a; Ping1D 05 §2.1]; bu genişliklerin tek yönlü olduğu ve desenin ayrılabilir sinc biçimi varsayılandır [T]; gerçek desen ve yan lob seviyesi yayımlanmamıştır [K, 10 §2.10].

#### 2.4.3 Saçıcı hücreleri ve eşevrelilik

Eşevreli toplamın fiziksel tutarlılığı için faz ışına değil yüzeye bağlıdır (sol md.18):
- Her gövdenin yüzeyi, üçgen yerel koordinatlarında kenarı `Δ_h` olan hücrelere bölünür [T, 1 cm]. Hücre `c` kimliği `(gövde_id, üçgen_id, hücre indisi)`; karmaşık katsayısı `g_c ~ CN(0, 1)` (`E|g_c|² = 1`, ortalama enerji korunur) bu kimlikten sayaç tabanlı RNG ile türetilir ve koşu boyunca sabittir.
- Işın hücreye düşerse katkısının fazı hücre merkezine olan mesafe `r_c` ile hesaplanır; ışın titreşimi fazı değiştirmez. Aynı hücreye düşen ışınlar kendi `dA_j`'lerini taşır; ışın sayısı artınca toplam, hücre alanıyla ağırlıklı aynı alana yakınsar.
- Sonuç: araç az hareket ettiğinde benek ardışık pinglerde ilintilidir, hücre boyutundan fazla kayınca ilinti kaybolur. Bu bir istatistiksel modeldir; mutlak faz doğruluğu iddia edilmez: 750 kHz'te dalga boyu 2 mm, 0,1 mm menzil farkı ≈ 36° faz farkı eder (00 Bölüm 6) ve dur-ve-atla varsayımı (2.4.5 md.1) yalnız kutu düzeyinde doğruluk sağlar.
- Sade kademe hücre fazı kullanmaz: yoğunluklar toplanır ve kutu başına ortalaması 1 olan üstel çarpanla benek uygulanır (Rayleigh genliğinin yoğunluk karşılığı); anahtar `(ping_no, kutu)`, ardışık pinglerde ilinti yoktur [T: bilinen sınır].

#### 2.4.4 Yollar, biriktirme ve alıcı zinciri

**Yol türleri** (tam kademe; sade kademede yalnız doğrudan yol). Bir yol dönüştürücüden `T` başlar, sıralı etkileşimlerden geçip `T`'ye döner. Etkileşim türleri: **difüz** (geri saçılım `σ_bs`, hücre fazı) ve **speküler** (ayna yansıması, katsayı `R_s γ_p(θ)`). Bir yolda tam bir difüz etkileşim ve en çok 2 speküler etkileşim bulunur [T]:

| Yol | Biçim | Sorgu |
|---|---|---|
| Doğrudan | `T → S → T` | 1 kesişim; dönüş aynı doğru |
| Gidişte sekme | `T → M → S → T` | kesişim `T→M`, ayna ışını `M→S`; dönüş `S→T` görünürlüğü |
| Dönüşte sekme | `T → S → M → T` | ayrı fiziksel yoldur; karşılıklılık gereği uzunluğu, desen çarpımı ve fazı gidişte sekmeyle aynıdır. Ayrı ışın atılmaz: gidişte sekmenin genlik katkısı 2 ile çarpılarak iki yol birlikte eklenir |
| Çift sekme | `T → M → S → M' → T` | `S`'den `T`'nin `M'` düzlemindeki görüntüsüne görünürlük (görüntü kaynağı, düzlemsel ayna) |

Speküler etkileşim yalnız ayna yansıma yasasını sağlayan doğrultuda yapılır (ayna ışını yansıyan doğrultuda atılır; dönüş bacağı görüntü kaynağı yöntemiyle kurulur). Dönüş bacağının görünür olması tek başına yeterli değildir: dönüş doğrultusu ışın deseni içinde olmalı ve yol speküler bacaklarda yansıma yasasını sağlamalıdır (00 Bölüm 6, D10 E). **Genlik:** gidiş açınık uzunluğu `L_g` (T'den S'ye, aynalar üzerinden düzleştirilmiş) ve dönüş `L_d` ile

`p̃ = B(gidiş)·B(dönüş) · Π R_s γ_p · √(σ_bs dA) · g_c · e^{i k (L_g + L_d)} · 10^{−α(L_g + L_d)/20} / (L_g L_d)`

Doğrudan yolda `L_g = L_d = r` ve `1/r²` geri gelir; çok yollu durum yarım toplam uzunluğa doğrudan yol yasası konarak genellenmez. Kutu: iki yönlü süre `(L_g + L_d)/c`. Hava–su sınırında `R_s` ≈ −1 (basınçta yumuşak sınır, fizik); `γ_p` pürüzlülük çarpanı [K].

**Biriktirme.** Kutu `i`, iki yönlü süre aralığı `[i T_s, (i+1) T_s)`'tir; fiziksel menzil `c_gerçek t/2`. Kart menzili kendi `c` parametresiyle hesaplar (03 §2 md.1); ses hızı farkı doğal menzil hatası üretir (05 §2.1). Tam kademede karmaşık katkılar kesirli gecikmeyle (doğrusal ara değerleme [T]) ince ızgaraya (`T_s/4`) eklenir. **Her iki kademede** menzil yanıtı darbe ve alıcı bant genişliğiyle korunur (sol md.20): tam kademede ince ızgara karmaşık dizisi, süresi `tx_duration` olan dikdörtgen zarf ile alıcı süzgecinin (bant genişliği `1/tx_duration` [T]) birleşik çekirdeğiyle evriştirilir; sade kademede yoğunluk dizisi aynı çekirdeğin `|·|²`'si ile evriştirilir (eşevresiz yaklaşık çekirdek). Sonra kutulara indirgenir.

**Alıcı zinciri** (sıra sabittir; iki kademede aynı):
1. Gürültü tabanı: tam kademede kutu başına `CN(0, Ĩ_n)` karmaşık gürültü eklenir; sade kademede `Ĩ_n` üstel dağılımla eklenir. `Ĩ_n` [K].
2. Yakın alan bandı: ilk 0,75–1 m'de ek gürültü [Y, Hasan 2024 Ping360 havuz gözlemi; nedeni makalenin açıklaması, D10 #7a; genliği K].
3. Zarf: `Ĩ = |p̃|²` (tam) ya da birikmiş `Ĩ` (sade). **Rayleigh ve benek testleri bu noktadaki doğrusal genlik `√Ĩ` üzerinde yapılır** (7. başlık).
4. Logaritmik dönüşüm `L = 10 log₁₀ Ĩ` [S: cihaz çıktısının logaritmik mi doğrusal mı olduğu belgelenmemiş; varsayılan logaritmik T].
5. TVG ve kazanç: `L + TVG(r) + G(gain)`, `TVG(r) = a log₁₀ r + 2 α_TVG r` [K; varsayılan `a` = 20, `α_TVG` = α; firmware eğrisi bilinmiyor, 10 §2.10]; `G` kazanç ayarı başına dB ofseti [K].
6. Eşleme ve kuantalama: `u8 = clamp(round((L' − L_min) · 255 / (L_maks − L_min)), 0, 255)`; `L_min`, `L_maks` [K]; 255 doyum bayrağı.

Yankılanma kuyruğu tam kademede isteğe bağlıdır [T, nicel kaynak yok].

#### 2.4.5 Ping360

- **Parametreler** [Ü, D05 #1a–#1c DOĞRU]: 750 kHz; 2° × 25°; 0,75–50 m; 400 açı konumu (0,9°); `sample_period` 80–40000 × 25 ns; 200–1200 örnek; frekans 500–1000 kHz. `Δr = c · sp · 25 ns / 2`, `T_dinleme = n · sp · 25 ns`, `R = n Δr`. Tamsayı `sp` gerçekleşen menzili ideal menzilden ayırır: 7 m ve 1200 örnek için ideal `sp` = 311,1, `sp` = 311 ile `R` = 6,9975 m; 50 m için `sp` = 2222 ile 49,995 m (sol yeniden hesabı). Gerçekleşen menzil HEAD alanlarından hesaplanır ve A6'ya yazılır. 1200 örnek en kısa `sp` = 80 ile 1,8 m'nin altına inemez; 1 m için daha az örnek gerekir (ör. 600 örnek, `sp` = 89, 1,00125 m).
- **Ping hattı** (2.4.4'teki sıra):
  1. **Poz:** ping `t_tx` adım sınırında başlar; geometri `t_tx` anının `PoseSet`'iyle (dur-ve-atla [T]; hata ≤ `|v| T_dinleme`, 50 m ve 1 m/s'de 6,7 cm ≈ 1,6 kutu). Her açı kendi pozuyla üretilir (05 §2.1, D05 N2).
  2. **Işın örnekleme:** koni (tam kademede yan lob payıyla genişletilmiş) katmanlı titreşimli örneklenir; titreşim sayaç tabanlı RNG'den, sonuç işçi sırasından bağımsız. HoloOcean açısal adım formülü kullanılmaz (`arccos` bağımsız değişkeni 1,000682; D10 #13).
  3. Kesişim (akustik gölge kendiliğinden, 10 §2.4.2), yollar, biriktirme, alıcı zinciri (2.4.4).
- **Komut ve durum** (Ping360 #2601 `transducer`, #2602 `auto_transmit`; D05 E; cihazın çakışma davranışı belgelenmediği için seçimler [T]): sim kartın `IDA_PING360_CONFIG`'ini bu anlamlarla yorumlar.

| Durum | Komut | Davranış |
|---|---|---|
| `BOŞTA` | `auto = 0` (tek ping: hedef açı, `transmit`) | Kafa mevcut açıdan hedef açıya kısa yönden döner (`T_motor`), `transmit = 1` ise tek ping atar, `BOŞTA`'ya döner |
| `BOŞTA` / `OTOMATİK` | `auto = 1` (`start`, `stop`, `num_steps`, `delay`) | `start`'a gider; her pingten sonra `num_steps` gradyan ilerler. `start = stop` ise tam tur ve 399 → 0 sarar; değilse sektörde uçlarda yön değiştirir [T]; pingler arası `delay` |
| Ping sürerken | herhangi yeni komut | Süren ping tamamlanır; yeni komut sonra geçerli olur. Bekleyen ve henüz başlamamış komutun yerini daha yenisi alır (son gelen geçerli) |
| Her durum | durdurma | Süren ping tamamlanır, `BOŞTA` (durdurma komutunun A1 karşılığı 06'dan beklenen) |
| Her durum | aralık dışı alan (ör. `sp < 80`, `n > 1200`) | Komut reddedilir, önceki yapılandırma sürer, olay [T: cihazın davranışı belgelenmemiş] |

  Yeni yapılandırmanın uygulandığı adım A6'ya yazılır.
- **Açı başına zamanlama; tarama süresi sabit değildir** (05.2, D05 N2):

  `T_ping = T_motor(Δaçı) + T_dinleme + T_işlem + T_iletim + T_delay`,   `T_motor = T_m0 + |Δaçı|·t_grad`,   `T_iletim = bayt · 10 / baud`

  `T_m0`, `t_grad`, `T_işlem` [K]. Zamanlama **kartın istediği** `n` ve `sp` ile hesaplanır; kalite kademesi yalnız içeriğin çözünürlüğünü değiştirir (2.4.7), zamanlamayı değiştirmez. İletimin sonraki pingle örtüşüp örtüşmediği [S; varsayılan ardışık T]. Bağlantı profili (USB, Ethernet, RS-485 ve baud) A5'te [C]; baud aday listesi biliniyor (115200–2.000.000), kayıpsız hız ölçülmedi (Ç19, M36). 115200 baud'da 1200 bayt yalnız iletimde 104,2 ms, 400 profil ≥ 41,7 s tutar; bu bağlantı 33 s taramayı üretemez (05.2).
- **Varsayılan katsayıların seçimi** [T]: üretici 50 m'de tam taramayı 33 s (açı başına 82,5 ms) verir [Ü, D05 #1b]. Varsayılan yapılandırma (1200 örnek, `sp` = 2222, 1 gradyan adım, USB) için `T_dinleme` 66,66 ms; kalan ≈ 15,8 ms `T_m0 + t_grad + T_işlem + T_iletim`'e paylaştırılır. Üreticinin 1 m süreleri (Ethernet 3,42 s, USB 4,17 s, RS-485 4,26 s) hangi örnek sayısı ve ayarlarla ölçüldüğü bilinmediği için uydurmada ve kabulde kullanılmaz (sol md.22).
- **Teslim:** `t_teslim = t_tx + T_dinleme + T_işlem + T_iletim`; `t_hazır = t_teslim`. Sonuç bu anda hazır değilse 4. başlıktaki politika.
- **Örnek hesap** (işlem bütçesi hesabı, ölçüm değil; D10 #13): 7 m, 1200 kutu, `c` = 1500 m/s → `Δr` = 5,83 mm, `T_dinleme` = 9,33 ms, yalnız akustik bekleme ≈ 107 ping/s üst sınır; 50 m'de 15 ping/s.

#### 2.4.6 Ping1D

- **Parametreler** [Ü, 05 §2.1; rapora göre, D05'te ayrıca sınanmadı]: 115 kHz; 25° koni; 0,3–100 m (tipik kullanılabilir 100 m, 00 Bölüm 6). "Menzilin %0,5'i" üreticinin profil (waterfall) çözünürlüğüdür, mesafe doğruluğu değildir (sol md.24).
- Doğrudan yol, eşevresiz hat her kademede aynıdır. İç profil `scan_start` ile `scan_start + scan_length` arasında örneklenir (200 kutu [T]); mesafe, eşiği aşan ilk güçlü ekonun kutu merkezidir; güven SNR'den eşlenir. Cihazın algoritması belgelenmemiştir: algılama kuralı [S], eşik ve güven eşlemesi [K]. Otomatik menzil `scan_length`'i son mesafenin 1,5 katına çeker [T].
- **Zaman:** dinleme en uzak dinlenen mesafeyi kapsar: `T_dinleme = 2 (scan_start + scan_length)/c`. `t_meas = t_tx + t_eko/2` (`t_eko` algılanan ekonun iki yönlü süresi); eko yoksa `t_meas = t_tx`, güven 0, `geçerli = false`. Ping aralığı `max(ping_interval, T_dinleme + T_işlem)`, `ping_interval` 50 ms [T].

#### 2.4.7 Kademe ve örnek sayısı

`n_eff = min(n_istenen, N_kademe)`; `sp_eff = round(sp_istenen · n_istenen / n_eff)`, [80, 40000] aralığına kırpılır; gerçekleşen menzil `c · n_eff · sp_eff · 25 ns / 2`; nicemlemenin bağıl menzil hatası ≤ `0,5/sp_eff` (06 §2.7 ile tek sözleşme). HEAD etkin değerleri taşır; istenen ve gerçekleşen değerler A6'ya. Kademe zamanlamayı ve `T_iletim`'i değiştirmez (modellenen cihaz istenen profili gönderir); A1 bant genişliğini değiştirir (06 §2.7).

### 2.5 Kamera: sensör tarafındaki sözleşme

Görüntü, optik ve geri okuma 07'nin; algılama A8 bileşeninindir (iskelet A8).

- **Zaman:** pozlama aralığının ortası `t_çizim` bir adım sınırıdır (07 aralarında ara değerleme yapmaz, 07 §2); `t_meas = t_çizim + T_poz/2` (pozlama sonu), `window_ns = T_poz` (iskelet §3.1). Tek geçerlilik anı gerektiren tüketiciler (03'ün yakalama anı, 07'nin çizimi) ortayı kullanır. Hız 07'nin kademesindedir: tam 30 Hz (26/27 adım), sade 15 Hz (53/54 adım) [T].
- **`CameraRequest`** (05 üretir, 07 çizer): `istek_id`, `kamera_id`, `kare_seq`, `t_çizim_ns`, `t_meas_ns`, `pozlama_ns`, `t_teslim_ns`, HIL'de `son_tarih_duvar_ns`, `gürültü_tohumu` (kameranın RNG anahtarından), `t_çizim` anındaki gövde durumları, montaj (`l`, `R_bs`), yapılandırma sürümü.
- **Gecikme:** `t_teslim = t_meas + L_sonrası` (pozlama ortasından `T_poz/2 + L_sonrası`); `L_sonrası` ISP, kodlama ve iletimi temsil eder [K; 10 §2.9'daki 15–40 ms tahmini kuyruğu kapsamaz, 10.8].
- **Düşürme:** SIL'de kare beklenir. HIL'de kare `t_teslim`'e karşılık gelen duvar saati son tarihine kadar hazır değilse düşürülür, sayılır, A6'ya yazılır; teslim zamanı kaydırılmaz (00 Bölüm 6, D09 E). Algılama kaydının teslimi `t_teslim + L_alg` 07'nindir (07 §2).

### 2.6 Rastgelelik ve tekrar üretilebilirlik

- **Akış anahtarı:** 04'ün Philox üreteci (09.4). Anahtar = özet`(senaryo_tohumu, ALT_SİSTEM_SENSÖR, sensor_id, inst, akış)`; akışlar `açılış`, `ölçüm`, `zamanlama`, `arıza`, `sonar_ışın`, `sonar_hücre`. Sayaç = `(örnek_no : 64 bit, kanal : 8, bileşen : 8, çekiliş : 16)`; sonar ışınında `örnek_no` = ping numarası, `bileşen` + `çekiliş` = ışın numarası; hücrede sayaç = hücre kimliği. Her çekilişin yeri sabit olduğu için bir sensörün, bir arızanın ya da işçi sırasının çekiliş sayısı başka bir çekilişi kaydırmaz.
- **Temel ölçüm koşulsuz üretilir:** kayıp ya da donmada model yine çalışır; temel ölçüm dizisi arıza açık ya da kapalı aynıdır.
- **Dağılımlar** kendi kodumuzdadır: normal, karmaşık normal, düzgün, üstel, log-normal, kırpılmış normal; her kayıt `model_version` taşır; dağılım ve model sürümleri, tohum, ürün profili özetleri, A7 manifest özeti ve A5 geçersiz kılmaları A6'ya yazılır (04 §2.13).
- **Hedef** (Ç7): aynı ikili ve platformda bit düzeyinde tekrar; macOS-arm64 ile Windows-x64 arasında toleranslı eşdeğerlik. Box–Muller'ın `log/sqrt/trig` çağrıları (09.4) ve Embree (10.9) platform farkı üretebilir. Embree yapıları tek iş parçacıklı ve sabit kalite bayraklarıyla kurulur; aynı platformda tekrar M11 kapsamında sınanır [T].
- **Sonar paralelliği:** ışın sonuçları ışın numarasıyla tutulur ve kutulara ışın numarası sırasıyla eklenir; paralel indirgeme yoktur.
- **Yer gerçeği** kayda yan veri olarak eklenir; A1'e gitmez, çekirdeğe girmez (02 §3.2 md.5).

### 2.7 Arıza zinciri

**Bileşim sırası** (birden çok arıza aynı kayda etkiyorsa sabit; kanal maskesine uyar):

1. Kısmi bozulma (ham kopyada: IMU tek eksen, DVL tek ışın, sonar kör sektör ya da kazanç düşüşü, sızıntı tek prob, geri okuma tek bit) → çözümün kopya üzerinde yeniden hesaplanması
2. Ofset `y + δ` → 3. Ölçek `a y` → 4. Sürüklenme `+ ρ(t − t₀)` ya da `+ δ_∞(1 − e^{−(t−t₀)/τ})` (sim zamanıyla)
5. Gürültü artışı `y + σ_nom √(k² − 1) ξ_arıza` (`k ≥ 1`; toplam σ = `k σ_nom`; yalnız gürültüsü yoğunluk ya da örnek σ'sıyla tanımlı kanalda)
6. Sıçrama (olasılık `p` ya da belirli anlar; genlik `±A`)
7. Donma: son arızalı çıktı tekrarlanır; iki tür: `t_meas` ilerler / `t_meas` de donar (ikincisi 02'nin "ölçüm zamanı ilerlemiyor" denetimini sınar). Bitince çıktı o anki zincir çıktısına sıçrar.
8. Yanlış geçerlilik (`valid` bozuk değerle `true`, ya da doğru değerle `false`; 05 §2.9)
9. NaN / aralık dışı → 10. Kendini bildiren arıza (`flags.sensör_arızası`)
11. Kayıp / aralıklı kayıp (kayıt teslim edilmez, `seq` boşluğu kalır) → 12. Geç teslim `t_teslim + Δ` ve titreşim; sensör içi sıra varsayılan korunur, `sıra_boz` seçeneğiyle bozulabilir

**Zaman deseni:** sürekli, Bernoulli(`p`) örnek başına, ya da zaman tabanlı Gilbert–Elliott: iki durum, ortalama kalma süreleri `T_iyi`, `T_kötü` (saniye), geçişler fizik adımında `1 − e^{−h/T}` olasılıkla (11.15'in zaman ölçeği dersi).

**Parametre denetimi** (`arm` anında): birim ve kanal sayısı sensör türüne uygun, `k ≥ 1`, `0 ≤ p ≤ 1`, süreler ≥ 0; aksi halde ret ve olay.

**Durumlu cihaz arızaları** (ayrı sınıf): Ping360 motor takılması (açı ilerlemez), DVL kalıcı ışın kaybı, sensör güç çevrimi (açılış hataları yeniden çekilir). Cihaz durumunu değiştirdikleri için arıza bittikten sonra temel diziyle bit eşitliği beklenmez. Toparlanma sözleşmesi: arıza bitince cihaz o anki durumundan olağan davranışına döner (motor kaldığı açıdan tarar); ölçüm RNG'si `örnek_no` ile anahtarlandığı için gürültü gerçekleşmeleri kaldığı yerden sürer.

---

## 3. Arayüzler

İmza ve alan listesi düzeyinde; sözleşmeler ayrı işte dondurulur (iskelet §3).

**Ölçüm kaydı** (05 → 06, 08):

```
struct MeasurementRecord {
  uint16_t sensor_id;  SensorKind kind;  uint8_t inst;  uint32_t seq;   // üretim sırası; kayıp boşluk bırakır
  int64_t  t_meas_ns;   int64_t window_ns;   int64_t t_tx_ns;          // 2.2; t_tx yoksa INT64_MIN
  int64_t  t_hazır_ns;  int64_t t_teslim_ns;
  uint16_t flags;        // bit0 geçerli, bit1 doymuş, bit2 sensör arızası (02 §3.3) + türe özgü
  float    quality;      // [0,1] normalize (fom, güven, eph'ten); yalnız A3/A6
  uint32_t fault_mask;   uint16_t model_version;                       // yalnız A3/A6
  Payload  payload;      // türe özgü etiketli birlik; alanlar 02 §3.3 ve 06 §2.2 ile
  Payload  base;         // arızasız temel kayıt; yalnız A6
  Truth    truth;        // yer gerçeği; yalnız A3/A6
};
```

A1'e giden alanlar (06 eşler): `seq`, `inst`, `flags`, `t_meas_ns`, `window_ns` (IMU, BARO, BATT, kamera üst verisi), `t_tx_ns` (Ping1D; Ping360'ta `t_meas` zaten gönderim anı; DVL'de A1 karşılığı yok, yalnız A6), `payload`. `t_teslim_ns` ve `t_hazır_ns` 06'nın parti atamasında kullanılır, karta gönderilmez.

**Eklenti arayüzü:**

```
class ISensorModel {
  SensorInfo   info() const;                                      // id, kind, inst, profil, model_version
  void         reset(const SensorConfig& cfg, const Mount& m, const RngKey& k);   // açılış hataları
  SchedulePlan plan() const;                                      // Periodic{pay_ns, payda, faz_ns} | EventDriven
  void         on_step(const StepView& v);                        // her fizik adımı; aralık biriktirme
  void         on_event(const SampleContext& c, RecordSink& out); // örnekleme ya da tamamlanma olayı
  void         on_command(const SensorCommand& cmd, int64_t t_apply_ns);   // Ping360 yapılandırması
  void         collect(int64_t t_ns, RecordSink& out);            // işçi sonuçları, sabit sırada
  RawView      raw() const;                                       // arıza zincirine ham veri (DVL ışınları, sonar kutuları)
  Payload      solve(const RawView& r) const;                     // saf çözüm işlevi; RNG yok
};
struct SampleContext {
  int64_t t_ns; uint64_t step_no;
  std::shared_ptr<const WorldSnapshot> snap;   const WorldSnapshotRing& ring;   // 04 §3
  const IEnvironmentView& env;   IWorkerPool& pool;   const SceneAssets& scene;
  std::shared_ptr<const WaveSpectrumSet> waves;   uint32_t epoch;
  IEventScheduler& sched;                       // tek seferlik olay: tamamlanma, sonraki ping
};
struct Mount { Vec3 l_b; Quat q_bs; };          // O_b'ye göre; A5 fizik gerçeği (iskelet §3.1)
```

`PoseSet` 04 snapshot'ından türetilen değişmez paylaşımlı kopyadır (2.4.1). `Mount` sim tarafında 05'e aittir; otopilotun inandığı montaj 02'nin parametresidir ve farklı olabilir (02 §3.6). Montaj hatası `δq_montaj` [K] ayrıca uygulanır.

**Sensör merkezi:**

```
class SensorHub {
  void reset(const ScenarioConfig& cfg, uint64_t seed, const SceneAssets& a7);   // epoch++
  void attach(Scheduler& s);
  void on_step(std::shared_ptr<const WorldSnapshot> s, const IEnvironmentView& env);
  void drain(int64_t t_until_ns, std::vector<MeasurementRecord>& out);   // t_teslim ≤ t_until olanlar
  void on_card_command(const SensorCommand& c);     // 06'dan; uygulandığı adım A6'ya
  IFaultControl& faults();                          // 08
  SensorCounters counters() const;   // sonar_geç, sonar_kuyruk_dolu, kamera_düşen, kuyruk_taşma, nan_reddi, model olayları
  PingTimingModel ping360_timing() const;           // 06'nın ön denetimine (06 §3)
};
```

`drain` kayıtları `(t_teslim, sensor_id, inst, seq)` ile sıralı verir (deterministik); parti içindeki iletim sırasının sahibi 06'dır (06 §2.3). Teslim kuyruğu sensör başına 64 kayıtla sınırlıdır [T].

**Arıza denetimi** (08 sürer):

```
class IFaultControl {
  FaultHandle arm(const SensorFaultSpec& f);   // bir sonraki adım sınırında etkin; parametre denetimi (2.7)
  void        disarm(FaultHandle h);
  std::span<const ActiveFault> active() const;
};
SensorFaultSpec: fault_id, sensor_id, inst, tür, t_başla_ns, süre_ns (0 = süresiz),
                 şiddet{ofset, ölçek, oran, τ, k, p, genlik, gecikme_ns, titreşim_ns}, kanal_maskesi,
                 zaman_deseni{sürekli | Bernoulli(p) | GE(T_iyi, T_kötü)}, durumlu (bool)
```

Koşula bağlı başlatma 08'in senaryo motorundadır; 05 zamanlı `arm` alır. Fiziksel arızalar 04'ün `apply_fault`'una, iletim hataları 06'nın `FaultInjector`'üne gider; üç katman ayrıdır (05 §1 md.9).

**04'ten beklenen** (04 §3'te olmayanlar): `IEnvironmentView::water_pressure(p_n, t)` (`p − p_atm`, 04 §2.4 formülü) ve `water_salinity()`; tek seferlik zamanlanmış olay (`IEventScheduler::at(t_ns, cb)`, en yakın sınır kuralına ya da tamamlanma için üst sınıra bağlanır); `IEnvironmentView`'un ve `WaveSpectrumSet`'in işçilerden eşzamanlı okunmaya güvenli olması; snapshot'ta itici dönüş yönü (`u` yeterli).

**08'den beklenen:** A5 `sensörler[]` `{sensor_id, kind, inst, ürün_profili, montaj (O_b'ye göre), hız, bağlantı profili, etkin, geçersiz kılmalar}`; akustik malzeme geçersiz kılmaları (A7 kimlikleriyle); sonar kademesi; `B_n`, pH, ses hızı ve soğurma geçersiz kılmaları; sanal emniyet girdilerinin sim tarafındaki yetkili durumu ve zaman çizelgesi (`ISafetyInputs`); arıza listesi.

---

## 4. Hata durumları ve davranışı

| Durum | Tespit | Davranış |
|---|---|---|
| Ürün profili ya da montaj eksik, birim hatalı | yüklemede | senaryo reddedilir, alan adıyla |
| Yapılandırılan hız fiziksel sınırı aşıyor (basınç > 25,97 Hz; DVL mod tablosu dışı) | yüklemede | reddedilir |
| A7 manifest özeti 04/07 ile uyuşmuyor; malzeme kimliği tabloda yok | yüklemede | sonar etkinleşmez, çalıştırma reddedilir; bilinmeyen malzemeye varsayılan verilmez |
| Ping360 komutu aralık dışı | kart komutu | reddedilir, önceki yapılandırma sürer, olay (2.4.5) |
| Dönüştürücü havada (yüzeyde yalpa) | her ping | eko yok: Ping360 yalnız gürültü tabanı, Ping1D güven 0, DVL ışını geçersiz |
| Model çıktısında NaN/Inf (arıza değilken) | her kayıt | `geçerli = false`, değer önceki geçerli değer, `nan_reddi` sayacı ve olay; NaN karta gitmez |
| Sonar işi `t_teslim`'de hazır değil | toplama anı | SIL: beklenir. HIL 1x: profil atılır, `sonar_geç`, A6. HIL + bariyer: aynı ve çalıştırma geçersiz (modellenen sensör akışı değişti; 06 §2.7 kuralıyla aynı). Sonraki pingin zamanı hesap süresinden bağımsızdır |
| Bekleyen sonar işi sınırda (4) | ping başlangıcı | ping düşürülür, `sonar_kuyruk_dolu`; HIL'de çalıştırma geçersiz |
| `epoch` değişti | sonuç toplama | eski sonuç atılır |
| Kamera karesi geç | teslim anı | 2.5 |
| Teslim kuyruğu dolu | kuyruk | en eski kayıt atılır, `kuyruk_taşma`, HIL'de çalıştırma geçersiz |
| Arıza parametresi geçersiz | `arm` | ret, olay |
| GNSS jeodezik başlangıç ya da `B_n` tanımsız | yüklemede | ilgili sensör etkinse ret |

---

## 5. Kalite kademelerinin etkisi

Kalite kademesi kullanıcının ayarıdır (iskelet §6). Sonar "sade" ve "tam" kademelidir, "kapalı" yoktur; kamera dışı diğer sensörler her kademede tam çalışır. Kademe profilin içeriğini ve çözünürlüğünü değiştirir; mesaj biçimini ve cihaz zamanlamasını değiştirmez (2.4.7); kart kademeyi bilmez, kademe A6'ya yazılır (06 §2.7).

| Etki | Sade | Tam |
|---|---|---|
| Ping360 örnek üst sınırı `N_kademe` | 600 (06 §2.7) | 1200 |
| Biriktirme | eşevresiz yoğunluk, eşevresiz darbe/alıcı çekirdeği | eşevreli karmaşık genlik, darbe/alıcı çekirdeği |
| Benek | kutu başına üstel çarpan, pingler arası ilintisiz | yüzeye bağlı saçıcı hücrelerinden |
| Işın deseni | ana lob | ana lob + yan loblar |
| Yollar | doğrudan | doğrudan + ≤ 2 speküler sekme |
| Su yüzeyi | ortalama düzlem | dalga yaması |
| Yayılım, soğurma, gölge, alıcı zinciri, yakın alan bandı | var | var |
| Ping başına başlangıç ışını | 1000 [T] | 5000 [T] (D10 #13'ün varsayımı) |
| Ping1D, DVL | iki kademede aynı | aynı |

**Hesap yükü** (işlem bütçesi hesabı, ölçüm değil):
- Ping başına kesişim sorgusu = `N_ışın · (1 + N_ayna)`, görünürlük sorgusu = `N_ışın · N_dönüş`; ikisi ayrı sayılır. Örnek: 5000 ışın, ışın başına üç kesişim bacağı (doğrudan, ayna, ikinci ayna) ve iki dönüş görünürlük sorgusu → 5 sorgu/ışın, 25.000 sorgu/ping, 100 Hz'te 2,5 milyon sorgu/s (sol yeniden hesabı). Görünürlük sorguları sayılmadığında D10 #13'ün değeri çıkar: 15.000 sorgu/ping, 1,5 milyon sorgu/s.
- Eşevreli birikimde her yolun 1200 frekans noktasına ayrı katkısı 18 milyon karmaşık katkı/ping (≥ 72 milyon gerçek işlem) eder (D10 #13); bu bölüm zaman alanında kesirli gecikmeli birikimi ve tek evrişim çekirdeğini seçer. Bunun maliyeti çekirdek uzunluğuna ve katkı sayısına bağlıdır; bir rakam verilmez (sol md.19 sonrası notu).
- Ping hızı `1/T_ping` ile sınırlıdır; en ağır durum kısa menzildir (7 m'de ≈ 107 ping/s akustik sınır).
- Menzilin en uzak ucunda yanal aralık kutu boyuna eşit seçilirse ≈ 22.008 ışın/ping gerekir (D10 #13); bu yakınsama garantisi değildir. Işın sayısı topluluk yakınsama testiyle seçilir (Bölüm 6).
- "Ping360 için tek ışın, ucuz, < 1 ms" desteklenmiyor (10.5, Ç9). İşçilerin dünya adım süresine etkisi ölçülmeden bilinmez (04 §2.12, M11).

**Kamera:** "kapalı" kademede `CameraRequest` üretilmez ve dışarıdan algılama gelmez; sade/tam hızı ve çözünürlüğü 07'nindir; sözleşme (2.5) aynıdır. **Kayıt** (08): temel kayıt ve yer gerçeği "telemetri" ve üstünde, sonar profilleri "tam"da önerilir.

---

## 6. Ölçüme bağlı kararlar

Bu tablodaki yedekler kalite kademesi değildir: ölçüm sonucunda tasarım yedeğe geçer ve kullanıcının seçtiği kademeden bağımsız uygulanır (iskelet §6).

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| Sonar ayrıntısı (iskelet §8) | iki kademe de HIL'de gerçek zamanlı | tam kademe yalnız SIL'de; HIL'de sade kademe ve azaltılmış ışın sayısı | M11 |
| Ping başına ışın sayısı | topluluk yakınsama testi: `N` iki katına çıkınca M = 32 gerçekleşimde kutu ortalama enerjisi, kutu varyansı ve ardışık kutu ilintisinin değişimi %5 güven aralığı içinde [T] | 5. başlıktaki başlangıç değerleri | M11 |
| Embree paylaşılan alt sahne | işe özel üst sahne, paylaşılan değişmez alt sahneler | işçi başına sahne kopyası | sözleşme işinde Embree belgesi; M11 |
| Platform eşdeğerliği | toleranslı istatistiksel eşdeğerlik (7. başlık) | platform başına altın profil | M11, M10 |
| Ping360 bağlantı zamanlaması | 50 m varsayılan yapılandırmaya uydurulmuş `T_m0`, `t_grad`, `T_işlem` | ölçülmüş profil | M36 |
| Akustik ve kamera kalibrasyonu | [K] varsayılanlar + açık veri kümesiyle uydurma | — (gerçek sensör kaydı K8 kapsamında değil) | M14 (K8 ile daralmış kapsamı) |
| Kamera teslim ve düşürme | `L_sonrası` [K]; HIL'de geç kare düşer | kamera yalnız görünür pencereyle | M12, M30, M13, M28 |
| IMU hızı | 400 Hz | 200 Hz | M5 |
| Sensör üretim süresi (06'nın `C_sim` payı) | dünya iş parçacığında | ağır modeller işçiye | M15 ve M11 |

---

## 7. Test yaklaşımı ve kabul ölçütleri

**İlkeler.** Eşikler [T]'dir ve 09'da kesinleşir; **kabulden önce dondurulur**. Kalibrasyon verisi ile kabul verisi ayrıdır. Etkiler tek tek açılarak sınanır (gürültü testinde açılış biası, sıcaklık ve kuantalama kapalı; ya da çıkarılmış artık üzerinde). İstatistiksel testler en az 20 tohumla, %95 güven aralığıyla, başlangıç geçişi atılarak koşar. Karşılaştırma toleransı `|a − b| ≤ ε_abs + ε_rel |b|`.

**Ortak testler**
- **Zaman:** her kayıtta `t_meas ≤ t_hazır ≤ t_teslim`, aralık ölçümünde `t_teslim ≥ t_meas`, akustikte `t_tx ≤ t_meas`; örnekleme olayı ideal zamandan ≤ 0,625 ms; tamamlanma olayı fiziksel bitişten önce değil; 10⁶ örnekte ideal zamanların birikimli sürüklenmesi 0 ns. Donmuş damga ve `sıra_boz` arızaları bu testlerin dışındadır ve ayrı sınanır.
- **GM süreci:** aynı `τ`, `σ_∞` ile 400, 200 ve 100 Hz'te durağan σ ve saniye cinsinden özilinti güven aralığında aynı (05.1). Rastgele yürüyüşte varyans `q t` ile büyür.
- **Arıza zinciri:** her tür istenen adımda başlar, biter; temel ölçüm dizisi arıza açık ya da kapalı bit düzeyinde aynı; durumsuz arızada arıza penceresi dışındaki çıktı arızasız koşuyla bit düzeyinde aynı; gürültü artışında varyans oranı `k²` güven aralığında; bileşim sırası belgedeki gibi; geçersiz parametre reddedilir.
- **Tekrar:** aynı platformda iki koşunun bütün kayıtları (sonar dahil) bit düzeyinde aynı. Platformlar arası: skaler kanallarda `ε_abs` = kanalın LSB'si, `ε_rel` = 10⁻⁶; sonarda topluluk düzeyinde kutu ortalama enerjisi ve histogramlar (JS ıraksaması < 0,01).

**Sensör başına**

| Sensör | Sınama |
|---|---|
| IMU | Durağan 4 saatlik SIL kaydının Allan eğrisi: `τ` = 1 s'de `N = ASD/√2`, güven aralığında; ayrık σ 400 ve 200 Hz'te tablodaki değerler; GM parametreleri özilintiden; **kimlik olmayan montajla** (`R_bs` 30° döndürülmüş, `l ≠ 0`): durağan araçta ivme ortalaması `R_bsᵀ R_nbᵀ(−g_n)`, sabit `ω` ile dönüşte kaldıraç kolu `R_bsᵀ ω × (ω × l)`, dünya dönüşü açıkken jiroskop ortalaması `R_bsᵀ R_nbᵀ ω_ie,n`; doyum bayrağı; süzgeç açıkken gürültü spektrumu süzgeç yanıtıyla uyumlu |
| Manyetometre | Tam dönüşte elipsoid uydurma `W`, `V`'yi; itici akım ve yön taramasında regresyon `K_k⁺`, `K_k⁻`'yi geri verir |
| Basınç | Sabit derinlikte ofset ve sıcaklık çıkarılmış artığın σ'sı 20 Pa; dalgada basınç genliğinin derinlikle sönümü 04'ün `F_p`'siyle; hız sınırı ihlali ret; `t_meas` D1 sonu, `t_teslim − t_meas` = 19,75 ms |
| GNSS | Su kalınlığı taramasında izleme eğrisi ve histerezis; fix kaybından önce `eph` büyür; yeniden kilitlenme süresi dağılımının medyanı parametreyle; edinimde suya giriş sayacı sıfırlar; 24 saatlik durağan kayıtta CEP 2,0 m güven aralığında |
| DVL | Düz tabanda irtifa `h` iken ışın menzili `1,0824 h`, `altitude` = `h`; eşit σ ile çözüm kovaryansı `1,848 σ_b` / `0,541 σ_b`; `c_ayar ≠ c_gerçek` ile ölçek `(c_ayar/c_gerçek)^κ`; nedensel zincir `t_tx < t_meas < t_rapor ≤ t_teslim` ve `t_teslim − t_meas = r̄/c + T_işlem + T_iletim`; hareketli hedefe çarpan ışında bağıl hız; mod histerezisi; kilit kaybında `vel_valid = false`, hız 0, irtifa −1; arama davranışı; su takibinde akıntı geri okunur |
| Ping1D | Düz tabanda mesafe hatası [T] eşiği (çözünürlük değil, sol md.24) + `Δc/c` etkisi; `T_dinleme` `scan_start + scan_length`'i kapsar; `t_meas = t_tx + t_eko/2`; eko yokken güven 0 |
| Ping360 | Bilinen mesafedeki duvarın tepe kutusu `2r/(c T_s) − ½` ± 1; gölge uzunluğu geometriyle ± 1 kutu; dönen araçta her açının kendi pozu; komut tablosunun her satırı; `T_ping` menzil, baud ve açı adımıyla değişir; 50 m varsayılan yapılandırmada tarama 33 s'ye ± %15 (uydurulan noktadır, bağımsız kabul değil; yalnız rapor); tam kademede homojen bölgede doğrusal genlik `√Ĩ` (log ve kuantalama öncesi) Rayleigh (KS, p > 0,05); sade kademede kutu yoğunluğu üstel; genlik ve yoğunluk soğurmasının ayrı birim testi; çok yollu katkı yalnız yansıma yasası ve dönüş görünürlüğü sağlanınca; doğrudan yolun `1/r²` (genlik) yasası |
| Kamera sözleşmesi | `t_çizim` adım sınırı, `t_meas = t_çizim + T_poz/2`; 26/27 ve 53/54 adım dizileri; HIL'de geç kare düşer, teslim zamanı değişmez |
| Batarya | Sabit akımda ölçek ve ofset; kuantalama basamağı; ADC aralığı dışında doyum; `t_meas` aralık sonu |
| Sızıntı | Algılama gecikmesi ve sahte pozitif oranı parametreyle |
| Fırlatıcı/emniyet | Yetkili durum değişince örnek `T_anahtar` sonra; bit çevirme arızasında yetkili durum ve fırlatma kararı değişmez (04 ile birlikte); kayıp ve donmada kartın tazelik denetimi tetiklenir (02 ile birlikte) |

**Açık veri kümeleriyle karşılaştırma** (sınadığı ve sınayamadığı birlikte):

| Veri kümesi | Kullanım | Sınar | Sınayamaz |
|---|---|---|---|
| Ping360 havuz kayıtları (Hasan vd. 2024; MIT lisanslı, 20 CSV, 10 senaryo, satır başına açı + 1200 adet 0–255 yoğunluk; 3 × 6 × 5 m havuz, 7 m menzil) [Y, D10 #7b] | Senaryolar ikiye ayrılır: yarısı kalibrasyon (malzeme, TVG, gürültü tabanı uydurması), yarısı kabul [T] | Havuz ikizinde gölge geometrisi, duvar ekosunun menzili, havuz sınırları ötesinde görünen yansımalar, yakın alan bandı, yoğunluk ve LBP doku histogramları (KL/JS/EMD, 10 §2.8) | Faz ve eşevreli modelin doğruluğu (faz sütunu yok); zamanlama ve hareket bozulması (damga yok); mutlak seviye (kazanç ve TVG bilinmiyor); malzeme yansıtıcılığının fiziksel değeri |
| Underwater Caves (Mallios 2017; iki MSIS + DVL + IMU + derinlik) [ikincil, 10 §2.8] | yalnız kabul | MSIS geometrisi ve tarama zamanlaması; DVL/IMU/derinlik gürültü mertebesi | Ping360'ın frekansı ve deseni; bizim ürün profillerimiz |
| SOLAQUA (BlueROV2, DVL + IMU), AQUALOC (kamera + IMU + basınç) [ikincil] | yalnız kabul | gürültü ve davranış mertebesi | profil değerleri, A50'nin iç davranışı |

Bu karşılaştırmalar modeli kalibre eder; gerçek sensörle doğrulamanın yerine geçmez (10 §2.4.4, M14).

**Kabul ölçütleri:** ortak ve sensör başına testler iki işletim sisteminde geçer; aynı platformda bit düzeyinde tekrar; Hasan 2024 kabul yarısında gölge ve duvar menzili ± 1 kutu, yoğunluk histogramı JS ıraksaması < 0,1 [T, uydurmadan önce dondurulur]; M11 raporu çıkmıştır.

---

## 8. Açık noktalar ve iskelete öneriler

**Komşu bölümlerle uyumsuzluklar** (bölüm iskelete uydu):

1. **06 `IDA_PING1D`** `t_meas`'ı gönderim anı tanımlıyor; iskelet §3.1 akustik mesafede yansıma anı istiyor. 06'ya `t_tx_ns` alanı eklenmesi ve `t_meas`'ın yansıma anına çekilmesi; 02 `ida_ping1d_t`'ye aynı alan.
2. **06 Ping360 kutu zamanı** `t_tx + i·T_s/2` yazıyor; 03 ve bu bölüm kutu merkezini `(i + ½)` alıyor. Tek kural seçilmeli (bu bölüm 03 ile aynı).
3. **02 `ida_baro_t`** hâlâ "dönüşüm ortası" diyor; iskelet §3.1 ve 06 aralık sonu + süre. 02'ye BARO ve BATT için `window_ns`.
4. **06 `IDA_SAFETY_IO`** yalnız `safety_mask` (acil durdurma, fiş, pim) ve `tube_loaded` taşıyor; 03'ün `ida_fc_launcher_t`'si tüp başına `emniyet_açık` ve araç başına `bakım` da istiyor. 06'nın mesajına bu alanların eklenmesi.
5. **06 `IDA_PING360_CONFIG`** tek ping için hedef açı ve `transmit` alanı, ayrıca durdurma komutu taşımıyor (2.4.5 tablosu).
6. **07 `CameraRequest.t_meas_ns`** pozlama ortası; iskelet §3.1 gereği bu bölüm `t_meas`'ı pozlama sonu, ortayı `t_çizim` alanı olarak tanımladı. 07'nin alan adlarının eşlenmesi.
7. **04 §3'te eksik:** `water_pressure`, `water_salinity`, tek seferlik zamanlanmış olay, işçilerden eşzamanlı okunabilirlik.

**İskelete öneri:** §3.1'e "tamamlanma olayları fiziksel bitişten sonraki ilk adım sınırında çalışır; örnekleme olayları en yakın sınıra bağlanır" ayrımının eklenmesi (bu bölüm 2.2'de uyguladı).

**Açık noktalar:**
- Embree'de commit edilmiş bir alt sahnenin birden çok üst sahnede eşzamanlı örneklenmesinin izinli olduğu belgeden doğrulanmalı (2.4.1; yedek tanımlı).
- Ainslie–McColm formülünün ve tatlı su uygulamasının birincil kaynaktan doğrulaması kapanmadı (05.11).
- İtici titreşiminin IMU'ya ve aracın kendi akustik gürültüsünün sonara etkisi: kaynak yok, ilk sürümde modellenmez.
- Anlamsal (T0) algılama üreteci (05 §3.2; 03 §8, 07 §2): SIL test aracı olarak kapsama alınıp alınmayacağı ve yer gerçeği kuralıyla (02 §3.2 md.5) ilişkisi 03 ve 07 ile birlikte kararlaştırılmalı.
- İç basınç ve iç sıcaklık sensörü iskeletin setinde yok (02 §8); 2.3'teki gövde içi sıcaklık modeli kaynak olabilir; mimarın kararı.
