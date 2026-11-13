# 05 — Sensörler ve sonar

Sürüm: 0.1 (2026-11-13). Durum: taslak; mimar onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) (sürüm 0.3) Bölüm 9, satır 05'in kapsamını yazar: sensör eklenti arayüzü, her sensörün modeli ve hata kaynakları, zamanlama, sonar (ışın izleme, kademeler), arıza modelleri. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir (bundan sonra "00"). Atıf biçimi: `05.n` = 00 Bölüm 4'teki düzeltme satırı, `Çn` = çelişki çözümü, `Mn` = ölçüm konusu, `Kn`/`Ön` = karar günlüğü, `DNN #k / Yk / E:…` = `docs/research/dogrulama/NN-codex.md`, `NN §x` = ham rapor bölümü. Ham rapordan alınan her değer önce 00'daki düzeltmeyle karşılaştırıldı; 05 numaralı ham rapordaki Rust'a özgü öneriler kullanılmadı (K3).

**Değerlerin niteliği** dört etiketle verilir:

| Etiket | Anlamı |
|---|---|
| **[Ü]** üretici belgesi | Veri sayfası, ürün sayfası, protokol belgesi. Yanında doğrulama durumu yazar (ör. "D05 #4 DOĞRU"; "rapora göre, ayrıca doğrulanmadı") |
| **[Y]** yayımlanmış ölçüm | Hakemli ya da açık deney sonucu; ölçüldüğü koşul yanında |
| **[T]** tahmin | Kaynakta yok ya da kaynak simülasyon varsayılanı; bu bölümün seçimi, gerekçesi yanında. Mimari varsayımları da bu etiketi taşır |
| **[K]** kalibrasyon parametresi | Gerçek ölçüm olmadan bilinemez; varsayılan değeri vardır ama A5 dosyasından sensör ya da malzeme başına değiştirilir ve kayda (A6) yazılır |

Bu bölümdeki hiçbir hesap süresi, ışın hızı ya da işlemci yükü ölçülmemiştir; geçtikleri yerde "işlem bütçesi hesabı, ölçüm değil" diye işaretlenir (Ç9, D10 Y4). Bu bölüm kod içermez; imzalar sözleşme işinin girdisidir (iskelet §3).

---

## 1. Amaç ve sorumluluklar

Sensör katmanı sim sunucusunun içinde eklenti olarak çalışır ve fizik durumundan, kart yazılımının gerçek bir sürücüden alacağı biçimde ölçüm üretir (iskelet §2). Sorumlulukları:

1. **Eklenti arayüzü ve sensör merkezi (`SensorHub`):** sensörlerin A5'ten kurulması, zamanlayıcıya (04) kaydı, ölçüm kayıtlarının teslim sırasına konması ve 06'ya verilmesi.
2. **Sensör modelleri:** IMU, manyetometre, basınç, GNSS, DVL, Ping1D, Ping360, batarya gerilim/akım, sızıntı. Her biri için ölçülen büyüklük, çerçeve, hız, gecikme, gürültü ve sistematik hata.
3. **Sonar:** Embree ile işlemcide ışın izleme (Ö4); sahne geometrisinin A7'den kurulması; kademeler.
4. **Kamera sözleşmesinin sensör tarafı:** örnekleme zamanı, kare üst verisi, gecikme, kare düşürme. Görüntü 07'nindir.
5. **Arıza zinciri:** ölçüm modelinden bağımsız arıza uygulaması ve 08'in hata enjeksiyonuna verilen arayüz.
6. **Tekrar üretilebilirlik:** sensör başına tohumlu rastgele sayı akışları, sürümlü dağılımlar, yer gerçeğinin kayda eklenmesi.

**Yazmadıkları:** fizik ve çevre modeli, su basıncı ve dalga kinematiği (04); A1 mesaj biçimi, parçalama, adım partisi ve iletim katmanı hataları (06); kamera görüntüsünün çizimi ve su altı optiği (07); A5 dosya biçimi, hata enjeksiyonunun arayüzü ve kayıt biçimi (08); kestirim ve ölçümlerin nasıl kullanıldığı (02); sonar verisinde hedef algılama (03). Sensörlerin kablo protokolleri (UART, I2C, SPI, Ping Protocol, Water Linked `wrz`) bu aşamada taklit edilmez (iskelet §1); modeller gerçek cihazın alan anlamlarını korur, böylece ileride bir protokol kodlayıcısı eklenebilir (D05 N1).

**Ürün profilleri** (K10 kapsamında araç parametresidir; gerçek sensör tedariki değildir, K8): IMU TDK ICM-42688-P, basınç MS5837-30BA (Bar30), manyetometre PNI RM3100 sınıfı, GNSS u-blox NEO-M9N, DVL Water Linked A50, tek ışınlı sonar Blue Robotics Ping Sonar (Ping1D), mekanik taramalı sonar Ping360, güç ölçümü Blue Robotics Power Sense Module (PSM), sızıntı SOS Leak Sensor. Profil dosyaları her değeri `{değer, nitelik, kaynak}` üçlüsüyle tutar (05 §3.1 md.4).

---

## 2. İç yapı

### 2.1 Alt modüller ve veri akışı

```
sim/sensors/
  hub/       SensorHub, teslim kuyruğu, sayaçlar, 06'ya boşaltma
  common/    montaj dönüşümü, Gauss–Markov süreci, kuantalama/doyum, iç sıcaklık modeli, RNG akışları
  models/    imu, mag, baro, gnss, dvl, ping1d, ping360, camera_contract, battery, leak
  sonar/     sahne (Embree), ışın örnekleme, saçılım, biriktirme, kademeler, işçi işleri
  faults/    arıza zinciri ve model içi arıza kancaları
  profiles/  ürün profilleri (değer + nitelik + kaynak)
```

```
WorldSnapshot + IEnvironmentView (04)          A7 sahnesi (sonar geometrisi, akustik malzemeler)
          │ her fizik adımı: on_step (ortalama alanlar)                │
          ▼                                                           ▼
 Zamanlayıcı (04) ─► model.on_event ─► [ÖLÇÜM ZİNCİRİ] ─► [ARIZA ZİNCİRİ] ─► teslim kuyruğu ─► drain (06, A1)
                       │  yer gerçeği → çerçeve → hata modeli →      (ayrı RNG,         (t_teslim sırası)   └─► kayıt (08, A6)
                       │  kuantalama/doyum → sensörün kendi bayrakları  kancalar)                            └─► paneller (A3)
                       └─► sonar işi (işçi havuzu, değişmez poz kopyası) ─► sonuç sabit sırada tüketilir
```

Üç aşama birbirinden ayrıdır (D05 N1, 05 §2.9):
- **Ölçüm zinciri** yalnız sensörün kendi fiziğini ve kusurlarını üretir (gürültü, bias, ölçek, sıcaklık, kuantalama, doyum, cihazın kendi geçerlilik bayrakları). Arıza bilmez.
- **Arıza zinciri** ölçüm kaydını alır, etkin arızaları uygular. Ölçüm zincirinin rastgele sayı akışına dokunmaz (2.6).
- **Çıktı**: teslim kuyruğu kaydı modellenmiş teslim zamanında 06'ya verir; A1 kodlaması ve iletim hataları 06'nındır. Aynı kayıt yer gerçeğiyle birlikte kayda ve panellere gider.

### 2.2 Zamanlama

- **Fizik adımı 1,25 ms** (iskelet §4). Sensör olayları 04'ün rasyonel zamanlayıcısıyla adım sınırına bağlanır; birikimli kayma olmaz (04 §2.11).
- **Anlık örnekler** fizik durumunu olayın çalıştığı adım sınırında okur ve `t_meas` o adımın zamanıdır; durum ile damga tutarlıdır. İdeal zaman ile adım sınırı arasındaki fark (≤ 1 adım) gerçek sensörün örnekleme titreşimi gibi davranır ve bir sonraki ideal zamanı kaydırmaz.
- **Ortalama alan örnekler** (IMU, basınç, batarya) her fizik adımında `on_step` ile biriktirir; değer pencere ortalamasıdır, pencere uzunluğu `window_ns` alanındadır. Böylece 05, 04'ten geçmiş durum ya da ara değerleme istemez.
- **Olay sürümlü sensörler** (Ping360, Ping1D, DVL) bir sonraki olay zamanını kendi modelinden hesaplar (ping süresi menzile ve yapılandırmaya bağlıdır); bunun için 04'ten tek seferlik zamanlanmış olay beklenir (Bölüm 8).
- **İki zaman, ayrı tutulur** (iskelet §3.1): `t_meas` ölçümün geçerli olduğu an; `t_teslim` ölçümün alıcıya ulaştığı an (sim zamanı). 06 kaydı `t_teslim`'in düştüğü kontrol adımının partisine koyar (06 §2.4).
- **Saat kayması:** sensör başına ppm ve faz parametresi (05 §2.8 md.4) periyodun payına yansıtılır; varsayılan 0 ppm [T]. IMU'da kayma yoktur: kontrolü tetikleyen IMU örneği adım sınırına denk gelmek zorundadır (iskelet §4). Bu bir basitleştirmedir (Bölüm 8).
- **Fizik adımının tam katı olmayan periyotlar:** 15 Hz DVL = 53⅓ adım, 30 Hz kamera = 26⅔ adım (26/27 adım dönüşümü; 04'teki 33/34 değeri 1 ms adıma aittir, Bölüm 8).

**Sensör başına zaman tanımları** (02-sol md.14'ün istediği alan anlamları):

| Sensör | Hız (nitelik) | `t_meas` (ölçüm zamanı) | `window_ns` | Teslim gecikmesi `t_teslim − t_meas` |
|---|---|---|---|---|
| IMU | 400 Hz kontrol hızı, yedek 200 Hz (iskelet §4, M5) | aralık sonu = kontrol adımı sınırı (02 §3.3, 06 `IDA_IMU`) | 2,5 ms (aralık ortalaması) | 0; süzgeç gecikmesi değerin içindedir (2.3.1) |
| Manyetometre | 100 Hz [T] (05 §2.8'deki 10–100 Hz doğrulanmadı) | örnekleme anı | 0 | 1 ms [T] |
| Basınç | 25 Hz [T]; üst sınır ≈ 27,7 Hz [Ü, D05 E5] | basınç (D1) dönüşüm penceresinin ortası | D1 dönüşüm süresi | pencere sonundan D2 dönüşümü + okuma (2.3.3) |
| GNSS | 5 Hz [T] | çözüm anı | 0 | 120 ms [T; PX4 simülasyon varsayılanı, D05 #9a] |
| DVL | moda göre 2–15 Hz [Ü, D05 #3a] | `time_of_validity` = yansıma anı (05.3) | 0 | `r̄/c + T_işlem + T_iletim`; `2r/c` eklenmez (05.3, Ç13) |
| Ping1D | `ping_interval` ve menzile bağlı, varsayılan 20 Hz [T] | akustik gönderim anı `t_tx` | 0 | `T_dinleme + T_işlem + T_iletim` |
| Ping360 | açı başına; ping süresi modelden (2.4.4) | akustik gönderim anı `t_tx` (cihazda damga yok, 00 Bölüm 6) | 0 | `T_dinleme + T_işlem + T_iletim` |
| Kamera | 30 Hz [T, A24 açık] | pozlama ortası (05 §2.8) | pozlama süresi | `L_kam` [K] (2.5) |
| Batarya | 10 Hz [T] | pencere ortası | örnek aralığı | 1 ms [T] |
| Sızıntı | 10 Hz [T] | örnekleme anı | 0 | 0; algılama gecikmesi modeldedir |

### 2.3 Seyrüsefer ve sistem sensörlerinin modelleri

Ortak gösterim: `R_nb` gövdeden NED'e dönüş, `R_bs` sensörden gövdeye dönüş (montaj), `l` sensörün gövde referans noktasına göre konumu (gövde çerçevesi), `ω` gövde açısal hızı, `n` beyaz gürültü, `b` bias. Bütün Gauss–Markov (GM) süreçleri zaman adımından bağımsız ayrık biçimdedir (05.1, D05 N6):

`b(t+Δt) = e^{−Δt/τ} b(t) + σ_∞ √(1 − e^{−2Δt/τ}) ξ`,   `ξ ~ N(0,1)`, `Δt` iki örnek arasında gerçekten geçen süre.

`τ → ∞` sınırında rastgele yürüyüşe iner: `b += σ_rw √Δt ξ`. PX4 GPS bias kodu (`1 − 1/τ` adım başı sönüm, ek `Δt` çarpanı) kullanılmaz; davranışı adım süresine bağlıdır (05.1).

**Beyaz gürültü ve süzgeç koşulu** (05.8, D05 E:IMU gürültüsü). Gürültü yoğunluğu `N`, ideal aralık ortalaması için `σ_d = N/√Δt` ile ayrıklaştırılır (Kalibr biçimi, 05 §2.4.1; veri sayfasındaki yoğunluk Allan katsayısı N olarak yorumlanır [T]; iki taraflı ve tek taraflı yoğunluk tanımları arasındaki √2 farkı sözleşmede tek yerde sabitlenir, Bölüm 8). Bu kısayol yalnız çıktı aralık ortalamasıyla üretiliyorsa geçerlidir. Sensörün süzgeç bant genişliği `1/(2Δt)`'den darsa gürültü fizik hızında üretilir ve değerle aynı süzgeçten geçirilir; süzgeçlenmiş çıktıyı seyrek örneklemek `σ/√Δt` vermez.

**"10 kat" kuralı uygulanmaz** (05.8, D05 Y9). Kalibr'in düşük maliyetli MEMS için önerdiği şişirme, kestiricide modellenmeyen sıcaklık ve ölçek hatalarını telafi etmek içindir. Simülatör bu hataları ayrıca ürettiği için sensör parametreleri şişirilmez; şişirme gerekiyorsa 02'nin süreç ve ölçüm gürültüsü ayarıdır. Böylece aynı hata iki kez sayılmaz.

**Gövde içi sıcaklık** (IMU, basınç ve manyetometrenin sıcaklık terimlerini sürer; 05 §2.7): birinci derece model `C dT/dt = P_e − k (T − T_su)`; `C`, `k`, `P_e` [T], `T_su` 04'ten. Kapatılabilir; o zaman `T` sabit A5 değeridir.

#### 2.3.1 IMU (ICM-42688-P profili)

- **Ölçülen:** jiroskop `ω_s` (rad/s) ve özgül kuvvet `f_s` (m/s²), sensör çerçevesinde.
- **Yer gerçeği:** `f_b = v̇_b + ω × v_b + ω̇ × l + ω × (ω × l) − R_nbᵀ g_n`, `v_b` gövde referans noktasının gövde çerçevesindeki hızı, `v̇_b` gövde bileşenlerinin zaman türevi (04'teki `ν̇` bu anlamda beklenir, Bölüm 8); `ω_s,gerçek = R_bsᵀ (ω + R_nbᵀ ω_ie)`. Dünya dönüşü `ω_ie` (7,292115e-5 rad/s, WGS-84 sabiti; NED'de enlem A5 başlangıç noktasından) varsayılan açıktır ve kapatılabilir; 02 kestirimde ihmal eder, farkın etkisi SIL'de ölçülür (02 §2.3).
- **Model** (05 §2.4.1): `ỹ = sat(Q(S·M·(x + b_on + b_gm(t) + b_T (T − T₀)) + n))`; `S` ölçek (köşegen `1 + s_i`), `M` eksenler arası, `b_on` açılış biasi (her koşuda bir kez çekilir; "sensör güç çevrimi" olayı yeniden çeker), `b_gm` GM biası, `b_T` sıcaklık katsayısı, `Q` kuantalama, `sat` ölçüm aralığı doyumu (doyunca `flags.doymuş`).
- **Aralık ortalaması:** değer kontrol aralığı boyunca fizik adımı sınırlarındaki değerlerin yamuk ortalamasıdır (400 Hz'te üç nokta) [T: O(dt²) hata; 04 adım içi ağırlıklı ortalama verirse o kullanılır]. İsteğe bağlı birinci derece süzgeç (köşe frekansı ve grup gecikmesi [K]) cihazın kullanıcı arayüzü süzgecini temsil eder; varsayılan kapalı.

| Parametre | Değer | Nitelik |
|---|---|---|
| Jiroskop gürültü yoğunluğu | 0,0028 °/s/√Hz = 4,89e-5 rad/s/√Hz | [Ü, D05 #4 DOĞRU] |
| İvmeölçer gürültü yoğunluğu | X/Y 65, Z 70 µg/√Hz (6,37e-4 / 6,87e-4 m/s²/√Hz) | [Ü, D05 #4] |
| 400 Hz'te ayrık σ | jiroskop ≈ 9,8e-4 rad/s; ivmeölçer ≈ 0,013 m/s² | `N/√Δt` ile türetilmiş |
| Açılış biasi | jiroskop ±0,5 °/s; ivmeölçer ±20 mg | [Ü, D05 #4]; dağılım: sınır = 3σ normal [T] |
| Sıcaklık katsayısı | jiroskop ±0,005 °/s/°C; ivmeölçer ±0,15 mg/°C | [Ü, D05 #4] |
| Ölçek / eksenler arası | jiroskop ±%0,5 / ±%1,25; ivmeölçer ±%0,5 / ±%1 | [Ü, D05 #4] |
| Doğrusalsızlık | jiroskop ±%0,1 | [Ü, 05 §2.4.1] |
| Bias kararsızlığı | veri sayfasında yok | [Ü: yok, D05 #4] |
| GM biası `τ` | jiroskop 1000 s, ivmeölçer 300 s | [T; PX4 IIM-42653 simülasyon varsayılanı, D05 #9c, ölçüm değil] |
| GM biası `σ_∞` | jiroskop 4,8e-5 rad/s (10 °/h); ivmeölçer 1e-3 m/s² | [K]; kaynak yok, SIL'de geniş aralıkta taranır (05 §4 md.4) |
| Ölçüm aralığı, çözünürlük | ±500 °/s, ±4 g, 16 bit | [T]; A5'te seçilir |

#### 2.3.2 Manyetometre

- **Ölçülen:** manyetik alan (T), sensör çerçevesinde. **Model** (05 §2.4.2, NXP AN4246 biçimi; PDF'ten birebir okunmadı): `B̃ = W · R_bsᵀ R_nbᵀ B_n + V + Σ_k K_k I_k + n`; `W` yumuşak demir + ölçek + diklik hatası, `V` sert demir, `K_k` itici k'nin akımına doğrusal girişim vektörü (ArduPilot `compassmot` kabulü), `I_k` 04'ün itici akımları (işaretli; yön değişiminde işaret değişir).
- `B_n` (NED, nT) senaryo konumu için A5'te verilir [K; WMM/IGRF ile önceden hesaplanır, entegre model ilk sürümde yok]. Havuz donatısı anomalisi isteğe bağlı konuma bağlı Gauss lekeleri [T, 05 §2.4.2 doğrulanmadı].
- Gürültü 20 nT (100 çevrim) [Ü, ikincil kaynak, 05 §2.4.2]. `W`, `V` [K]; varsayılan `‖V‖` ≤ 5 µT, `W` köşegeni 1 ± %2 [T]. `K_k` [K]; varsayılan, tam itkide girişim alanın %20'si [T; ArduPilot "< %30 iyi" ölçütünün içinde, Ü].

#### 2.3.3 Basınç / derinlik (MS5837-30BA profili)

- **Ölçülen:** mutlak basınç `p_abs` (Pa) ve sıcaklık. **Yer gerçeği:** sensör ağzının dünya konumunda `p_atm + (p − p_atm)`; `p − p_atm`, 04'ün gövde integralinde kullandığı hidrostatik + birinci mertebe dalga basıncı formülünün aynısıdır (04 §2.4) ve 04'ten `water_pressure(p_n, t)` sorgusu olarak beklenir (Bölüm 8). Ağız su üstündeyse `p_atm`. Aracın hızından gelen dinamik basınç `C_p · ½ρ|v_r|²` eklenir; `C_p` ağız konumuna bağlıdır [K], varsayılan 0 (05 §2.3.2).
- **Zamanlama:** OSR 8192'de bir dönüşüm en çok 18,08 ms [Ü, D05 E5 DOĞRU]; D1 + D2 için salt süre sınırı ≈ 27,7 Hz, 25 Hz temkinli seçimdir. Basınç değeri D1 penceresi boyunca ortalamadır (pencere 14 fizik adımı = 17,5 ms [T]); `t_meas` pencere ortası; teslim D2 penceresi ve okuma (1 ms [T]) sonrasıdır. Yapılandırılan hız `1/(2·T_dönüşüm + t_okuma)`'yı aşarsa senaryo reddedilir (05.2'nin "dönüşüm süresinden gelen hız üst sınırı").
- **Hatalar:** RMS gürültü 0,2 mbar = 20 Pa (≈ 2 mm su) [Ü, D05 E5]; mutlak doğruluk 0–6 bar ve 0–45 °C'de ±50 mbar [Ü, 00 Bölüm 6, D05 E] → açılış ofseti, sınır = 3σ [T]; ofset firmware tarafında yüzeyde sıfırlanır (02 §2.3), asıl etkili olanlar sıcaklık kayması [K], kuantalama 10 Pa [T; doğrulanmadı] ve dinamik basınçtır. 2 mm çözünürlük 2 mm doğruluk değildir (D05 E).

#### 2.3.4 GNSS (NEO-M9N profili)

- **Ölçülen:** anten konumu (`lat_e7`, `lon_e7`, `alt_mm`), NED hızı, `eph`, `epv`, `fix_type`, `n_sv`. NED → jeodezik dönüşüm A5 başlangıç noktasından WGS-84 ile, `double` (04 §2.2).
- **Su altında kayıp ayarlanabilir bir geçiştir** (05.4, D05 N5): anten üzerindeki su kalınlığı `w = max(0, ζ(x,y,t) − z_anten,yukarı)` her örnekte 04'ün yüzey yüksekliğinden hesaplanır; dalga taşması bu yolla kendiliğinden aralıklı kayıp üretir. İzlenen uydu sayısı `w`'nin lojistik işleviyle düşer: `p_izle(w) = 1/(1 + e^{(w − w₅₀)/s})`. `w₅₀ = 30 mm` [Y; tatlı suda belirli jeodezik alıcı/antende 30–35 mm gözlem, D05 #8a; bu alıcıya aktarımı K], `s = 5 mm` [K]. `n_sv < 4` olunca fix düşer. "3 cm" fiziksel duvar değildir.
- **Yeniden kilitlenme bir durum makinesidir, garanti süre değildir** (05.5): `NO_FIX → 2D → 3D`; ilk fix süresi kesinti süresine göre seçilen dağılımdan çekilir: kısa kesinti (< `T_kısa` [K], varsayılan 10 s) hızlı yeniden edinim; uzun kesinti sıcak başlangıç, en az 2 s artı log-normal kuyruk [Ü 2 s komutla başlatılan, −130 dBm, oda sıcaklığı testine aittir; kuyruk K]; çok uzun kesinti (> `T_efemeris` [T; 2–4 saat, doğrulanmadı]) soğuk başlangıç 24 s ve üzeri [Ü, dört takımyıldız yapılandırması]. Fix'ten sonra hata, sönen ek bir terimle (`τ_yakınsama` [K]) büyükten nominale iner (05 §2.5).
- **Hatalar:** yatay konum = gerçek + GM biası (`τ` 60 s [T], doğru ayrık biçimle) + beyaz gürültü. 2,0 m CEP [Ü, D05 #8b, açık gökyüzü üretici koşulu] dairesel normalde eksen başına σ ≈ 1,70 m'ye karşılık gelir; bunun %90'ı GM'ye, %10'u beyaz gürültüye verilir [T]. Düşey σ yatayın 1,5 katı [T]. Hız gürültüsü σ 0,05 m/s [T]. Raporlanan `eph`/`epv` gerçek σ × `k_rapor` [K, varsayılan 1]; `k_rapor ≠ 1` "raporladığı kalite ile gerçek tutarsız" sınıfını üretir (05 §2.9).

#### 2.3.5 DVL (Water Linked A50 profili)

- **Geometri:** 4 ışın, Janus düzeni, ışın açısı 22,5°, yarı güç ışın genişliği 4,4° [Ü, 05 §2.3.1; rapora göre, D05'te ayrıca sınanmadı]. Her ışın, ışın konisi içinde 7 ışınla [T] sonar sahnesinde (2.4.1) izlenir; eğimli menzil `r_i` en yakın geçerli kesişimdir. Aracın kendi geometrisi ışın maskesiyle dışlanır.
- **Işın geçerliliği:** `r_i` modun aralığında; geliş açısı < `θ_max` [K, varsayılan 60°]; dönüştürücü suda (`w > 0`); yüzeye yakınlık ve itici akımına bağlı kabarcık/türbülans düşürme olasılığı [K, 05 §2.3.1 md.7 beklenti]. Çözülemeyen ışında mesafe −1, hız 0 (D05 E:DVL kodlayıcısı).
- **Hız:** ışın hızı `u_i = b_iᵀ R_bsᵀ (v_b + ω × l) · (c_gerçek / c_ayar)^{κ} + n_i`, `n_i ~ N(0, σ_b(r_i))`. `κ` ses hızı duyarlılığı [K; piston dönüştürücüde 1, faz dizilide yataya göre ~0; A50 tipi doğrulanmadı, varsayılan 1]. 4 ışından en küçük karelerle `v̂` ve kovaryans; 3 ışınla çözüm izni [K, A50 davranışı doğrulanmadı, varsayılan kapalı]. `fom` kovaryans köşegeninin en büyüğünün karekökü [T; üretici tanımı okunmadı]. Uzun dönem ölçek hatası ±%1,01 [Ü, ikincil]; montaj hizalama hatası (özellikle yaw) `δq_montaj` [K]. Hız çözünürlüğü 0,1 mm/s, en yüksek 3,75 m/s [Ü].
- **Mod ve hız:** mod tablosu (0: 0,05–0,6 m 15 Hz … 4: 15 m+ 2–4 Hz) [Ü, D05 #3a]; aralıklar örtüştüğü için hız irtifanın tek değerli işlevi değildir, mod geçişi histerezisli bir durum makinesidir; 10 s'de bir daha yakın yansıma denetimi [Ü, 05 §2.3.1 md.5]. Mod içindeki hız aralığından düzgün dağılımla [T].
- **Dip kilidi:** kayıpta `vel_valid = false`, hız 0, irtifa −1 (D05 E). Yeniden kazanım arama gecikmesi 0,5–3 s düzgün [T, 05 §2.3.1 doğrulanmadı].
- **Su takibi ayrı fiziksel ölçümdür** (D05 E): yaklaşık 1,5–4,5 m aşağıdaki su kolonuna göre hız [Ü]; ışınlar boyunca `IEnvironmentView::water_velocity` örneklenir; 2 Hz [Ü]. Sığ havuzda dip yansıması bozar [K olasılık]. `track` alanı dip/su ayrımını taşır.
- **Zaman** (05.3, Ç13, D05 N4): olay yansıma anında çalışır; `t_meas = t_yansıma` (o adımın zamanı); gönderim anı `t_tx = t_meas − r̄/c` yalnız kayda yazılır; rapor anı `t_rapor = t_meas + r̄/c + T_işlem` (`T_işlem` [K]); teslim `t_rapor + T_iletim`. `T_iletim` seçilen arayüz profilinden: seri satır uzunluğu × 10 / baud [Ü 115200 varsayılan, D05 #3c] ya da TCP 4 ms ortalama, σ 2 ms, gözlenen en büyük 13 ms [Ü üretici gözlemi, garanti değil, 05.12]. `t_rapor` 06'nın `t_report_ns` alanıdır.

#### 2.3.6 Batarya gerilim/akım (PSM profili)

- **Yer gerçeği:** 04'ün `V_bara` ve `I_paket` değerleri, örnek aralığı boyunca ortalama (itici akımı adım düzeyinde değiştiği için). Sıcaklık 04'ün batarya sıcaklığı.
- **Ölçüm hattı güç hattından ayrıdır** (11.12, D11 E:Güç hattı): PSM ölçekleri 11,0 V/V ve 37,8788 A/V, 0,330 V ofset [Ü, D11 DOĞRU]; ADC kuantalaması (bit sayısı ve referans [K]), akım ofsetinin sıcaklıkla kayması [K], PWM kaynaklı gürültü σ [K] eşdeğer gerilim üzerinde uygulanır, sonra ölçeklerle volt ve ampere çevrilir. 100 A sürekli değer değildir; geçerli ölçüm aralığı dışında `flags.doymuş` [Ü/K].

#### 2.3.7 Sızıntı (SOS Leak profili)

- Sayısal giriş, prob başına bit (`mask`). Tepki süresi üretici belgesinde yok [Ü: yok] → algılama gecikmesi [K], varsayılan 1 s [T]. Yer gerçeği, senaryonun ya da 08'in "su girişi" olayıdır (04 su girişini fiziksel modellemez). Sahte pozitif sıçrama oranı [K, varsayılan 0] (05 §2.7).

### 2.4 Sonar

#### 2.4.1 Sahne ve geometri (A7)

- **Kaynak tek:** sonar geometrisi A7'nin glTF + manifest kaynağından, 04 ve 07 ile aynı varlık yükleyicisi ve aynı glTF → gövde dönüşümüyle kurulur (04 §2.2, D10 N8). Manifest özeti 04 ve 07'ninkiyle karşılaştırılır; uyuşmazlıkta sonar etkinleşmez (Bölüm 4).
- **Akustik ağlar:** manifestte `SON_*` düğümleri varsa onlar, yoksa çarpışma ağından türetilen sadeleştirilmiş ağ kullanılır ve türetme ayarları manifeste yazılır (manifest içeriği 07'den beklenen). Havuz duvarı, taban ve batimetri statik sahnedir; hedefler, torpidolar ve diğer gövdeler Embree örnekleri (instance) olarak her pingte değişmez poz kopyasından konumlandırılır; yalnız üst düzey yapı yenilenir.
- **Malzeme kaydı:** her üçgen bir malzeme kimliği taşır; akustik özellikler (`μ` geri saçılım katsayısı, geliş açısı yasası, speküler yansıma katsayısı, pürüzlülük) A7 malzeme kaydında, görsel özelliklerle aynı kimlikle durur (D10 #1b; 10 §2.7). Bütün akustik malzeme değerleri **[K]**: 750 kHz'te hedef ve karo yansıtıcılığı bilinmiyor (10 §2.10), APL-UW modeli 750 kHz'e kalibrasyonsuz aktarılmaz (10.10).
- **Su yüzeyi:** sade kademede ortalama yüzey düzlemi; tam kademede aracın çevresinde, aynı dalga listesinden `surface_elevation` ile her pingte üretilen yama (12.14 tutarlılığı; ızgara aralığı [T]). Hava–su sınırı basınçta yumuşak sınırdır (yansıma katsayısı ≈ −1, fizik), pürüzlülük [K].
- **Ses hızı:** su sıcaklığı ve tuzluluktan; tatlı suda UNESCO/Chen–Millero (Wong–Zhu katsayıları), deniz suyunda Mackenzie (05 §2.2.1; NPL sayfaları bağımsız doğrulamada açılmadı, D05 E:Erişim). A5'ten sabit `c` ile geçersiz kılınabilir. Tuzluluk 04'ten beklenir (Bölüm 8). Havuz ölçeğinde (< 50 m) kırılma ihmal edilir [T, 05 §2.2.1 mühendislik yargısı].

#### 2.4.2 Akustik büyüklükler ve birimler

İç hesap iki büyüklükten birini taşır ve ikisi karıştırılmaz (05.11, D05 E:Akustik modelde birim):

| Büyüklük | Kullanıldığı kademe | Yayılım (iki yön, nokta saçıcı) | Soğurma (α dB/m) | dB dönüşümü |
|---|---|---|---|---|
| Karmaşık basınç genliği `p` | tam (eşevreli) | `1/r²` | `10^{−2αr/20}` | `20 log₁₀|p|` |
| Yoğunluk `I = |p|²` | sade (eşevresiz) | `1/r⁴` | `10^{−2αr/10}` | `10 log₁₀ I` |

- Işın `j`'nin kesişimi bir saçıcıdır; kapladığı yüzey alanı `dA_j = r² dΩ_j / cos θ_geliş`. Geri saçılım katsayısı `σ_bs(θ) = μ cos²θ` (Lambert, varsayılan [K]; Stonefish/OceanSim'in `cos θ` biçimi de seçilebilir, 05 §2.2.1). Yoğunluk katkısı `I_j = B(θ_j, φ_j)² σ_bs dA_j 10^{−2αr/10} / r⁴`; genlik katkısı `p_j = √(I_j) e^{i(2kr + ψ_j)}`, `ψ_j` saçıcı başına rastgele faz (tohumlu, 2.6).
- `α(f, T, S, z)` varsayılanı Ainslie–McColm formülüyle üretilir; formül ve tatlı su uygulaması **doğrulanmadı** (05.11); aritmetiği 750 kHz, 20 °C tatlı suda ≈ 0,131 dB/m, 115 kHz'te ≈ 0,003 dB/m verir. α her sonar için A5'ten geçersiz kılınabilir [K].
- `B(θ, φ)` ışın deseni (genlik): yatayda ve düşeyde −3 dB genişlikleri 2° × 25° (Ping360) ve 25° (Ping1D) [Ü, D05 #1a; Ping1D 05 §2.1] olan ayrılabilir piston (sinc) deseni varsayılandır [T]; gerçek desen ve yan lob seviyesi yayımlanmamıştır [K, 10 §2.10].

#### 2.4.3 Ping başına hesap hattı

1. **Poz:** ping `t_tx` adım sınırında başlar; aracın, hedeflerin ve torpidoların pozu o adımın değişmez kopyasından alınır (her açı adımı kendi pozuyla; 05 §2.1, D05 N2). Dinleme süresi boyunca araç hareketi ihmal edilir (dur-ve-atla varsayımı [T]; hata ≤ `|v| · T_dinleme`, 50 m ve 1 m/s'de ≈ 6,7 cm ≈ 1,6 kutu). Kutu başına poz sonraki kademedir.
2. **Işın örnekleme:** ışın konisi (tam kademede yan lob payıyla genişletilmiş) katmanlı titreşimli örneklenir; jitter sayaç tabanlı RNG'den gelir, böylece paralel işlemede sıra sonucu değiştirmez. HoloOcean'ın açısal adım formülü kullanılmaz: 2° ve `R_max/1200` ile `arccos` bağımsız değişkeni 1,000682 olur (D10 #13). Işın sayısı yakınsama testiyle seçilir (Bölüm 6, M11).
3. **Kesişim:** Embree en yakın kesişim; akustik gölge kendiliğinden oluşur (10 §2.4.2).
4. **Çok yollu (yalnız tam):** speküler katsayısı eşiği aşan yüzeyde ikincil ışın; dönüş yolu dönüştürücüye kapanıyorsa (görünürlük sorgusu ve ışın deseni içinde) katkı toplam yol uzunluğunun yarısına karşılık gelen kutuya, yansıma katsayıları çarpılarak eklenir; ileri yansıyan ışını iki kez sektirmek tek başına yeterli değildir (00 Bölüm 6, D10 E). En çok 2 sekme [T].
5. **Biriktirme:** kutu `i` iki yönlü zamana `i · sample_period · 25 ns` karşılık gelir; fiziksel menzil `c_gerçek · t / 2`. Kart menzili cihazdaki gibi kendi `c` varsayımıyla hesaplar; ses hızı farkı doğal menzil hatası üretir (05 §2.1). Tam kademede karmaşık katkılar kesirli gecikmeyle biriktirilir ve `tx_duration`'dan türetilen darbe zarfıyla evriştirilip zarfı alınır; sade kademede yoğunluklar kutuya eklenir ve benek çarpımsal üstel dağılımla (Rayleigh genliğinin yoğunluk karşılığı) uygulanır (05 §2.2.1).
6. **Gürültü ve alıcı:** ortam/ısıl gürültü tabanı [K]; ilk 0,75–1 m'de yakın alan gürültü bandı [Y, Hasan 2024 Ping360 havuz gözlemi; nedeni makalenin açıklaması, D10 #7a; genliği K]; TVG eğrisi ve `gain` ayarı dB ofsetleri [K, firmware eğrisi bilinmiyor, 10 §2.10]; doyum ve 8 bit kuantalama. Yankılanma kuyruğu (istatistiksel) tam kademede isteğe bağlıdır [T, nicel kaynak yok].
7. **Çıktı:** Ping360 için `ida_ping360_t` alanları (02 §3.3); Ping1D için mesafe ve güven.

#### 2.4.4 Ping360

- **Parametreler** [Ü, D05 #1a–#1c DOĞRU]: 750 kHz; 2° × 25°; 0,75–50 m; açı birimi gradyan (400 konum, 0,9°); `sample_period` 80–40000 × 25 ns; 200–1200 örnek; frekans 500–1000 kHz. Menzil `c · n · sample_period · 25 ns / 2`.
- **Yapılandırma** kartın `IDA_PING360_CONFIG` mesajıyla, gerçek cihazın `transducer` (#2601) ve `auto_transmit` (#2602) anlamıyla yorumlanır (D05 E; 06 §2.2): kip, kazanç, darbe süresi, örnekleme, örnek sayısı, başlangıç/bitiş açısı, adım sayısı, gecikme. Yeni yapılandırma bir sonraki pingte geçerli olur; uygulandığı adım kayda yazılır. Geçersiz değer gerçek cihazdaki gibi reddedilir ya da sınıra çekilir (davranış seçimi [T]) ve olay üretilir. Kademe sınırı `n_eff = min(n_istenen, N_kademe)` ve `sample_period` ölçeklemesi 06 §2.5'teki gibidir.
- **Açı başına zamanlama; tarama süresi sabit varsayılmaz** (05.2, D05 N2):

  `T_ping = T_motor(num_steps) + T_dinleme + T_işlem + T_iletim + T_gecikme`,   `T_dinleme = n · sample_period · 25 ns`,   `T_iletim = bayt_sayısı · 10 / baud` (seçilen bağlantı profili).

  `T_motor` ve `T_işlem` [K]; varsayılanları üreticinin tarama sürelerini (50 m'de 33 s; 1 m'de Ethernet 3,42 s, USB 4,17 s, RS-485 4,26 s) yeniden üretecek biçimde seçilir [Ü süreler D05 #1b; süreleri üreten örnek sayısı ve ayarlar bilinmediği için uydurma T]. 1200 baytlık profil 115200 baud'da yalnız iletimde ≈ 104 ms tutar; 400 profil ≥ 41,7 s eder, yani bu bağlantı 33 s'lik taramayı üretemez (05.2). İletimin bir sonraki pingle örtüşüp örtüşmediği bilinmiyor [K, varsayılan ardışık]. Baud aday listesi biliniyor (115200–2.000.000), kayıpsız hız ölçülmedi (Ç19, M36).
- **Örnek hesap** (işlem bütçesi hesabı, ölçüm değil; D10 #13): 7 m, 1200 kutu, `c = 1500 m/s` → kutu 5,83 mm, `T_dinleme = 9,33 ms`, yalnız akustik bekleme ≈ 107 ping/s üst sınır; motor ve iletim bunu düşürür. 50 m'de `T_dinleme` ≈ 66,7 ms.

#### 2.4.5 Ping1D

- **Parametreler** [Ü, 05 §2.1; rapora göre, D05'te ayrıca sınanmadı]: 115 kHz; 25° koni; 0,3–100 m (tipik kullanılabilir 100 m, 00 Bölüm 6); çözünürlük menzilin %0,5'i.
- Aynı hattın tek ışınlı, eşevresiz biçimi her kademede çalışır (kesişim sayısı küçüktür). İç profil `scan_start` ve `scan_length` ile örneklenir; mesafe, `scan_start` sonrasında eşiği aşan ilk güçlü ekodur; güven SNR'den eşlenir. Cihazın kendi algoritması belgelenmemiştir: algılama kuralı, eşik ve güven eşlemesi [K]. Otomatik menzil kipi `scan_length` değerini son mesafenin 1,5 katına çeker [T].
- Hız `min(1/ping_interval, 1/(T_dinleme + T_işlem))`; `ping_interval` varsayılan 50 ms [T]. Dönüştürücü havadaysa ya da eko yoksa güven 0.

### 2.5 Kamera: yalnız sensör tarafındaki sözleşme

Görüntünün çizimi, su altı optiği ve geri okuma 07'nindir; algılama 07'deki PC bileşeninde yapılır (iskelet §3).

- **Örnekleme:** 30 Hz [T, A24 açık], rasyonel zamanlayıcıyla 26/27 adım. `t_meas` = pozlama ortası, adım sınırında; pozlama süresi üst veridedir. Çizim isteği bu `t_meas` anının poz kopyasıyla 07'ye gider; operatör görünümünün zamanı bundan ayrıdır (00 Bölüm 6, D12 E:Tek sahnede iki zaman).
- **Kare üst verisi** (A2, 07 doldurur, 05 sözleşmeyi koyar): `kamera_id`, `kare_seq`, `t_meas_ns`, `pozlama_ns`, `t_teslim_ns`, çözünürlük ve piksel biçimi, iç parametreler (`fx, fy, cx, cy`, bozulma), montaj (`l`, `R_bs`), kalite kademesi, `istek_id`, `hazır_duvar_ns` (07'nin işi bitirdiği duvar saati), `düşürüldü` bayrağı ve nedeni.
- **Gecikme:** `t_teslim = t_meas + L_kam`; `L_kam` pozlama sonrası ISP, kodlama ve iletimi temsil eder [K; 10 §2.9'daki 15–40 ms tahmini kuyruk süresini kapsamıyor, 10.8]. Simülatör gerçek kameradan hızlı olmalıdır ki gecikme yapay olarak eklenebilsin (10 §2.9).
- **Kare düşürme:** SIL'de çekirdek kareyi bekler. Gerçek zamanlı HIL'de kare `t_teslim`'e kadar hazır değilse düşürülür, sayılır ve A6'ya yazılır; teslim zamanı geriye kaydırılmaz (00 Bölüm 6, D09 E; 10 §2.9). Hız "kapalı" kademede sıfırdır, mesaj kümesi değişmez (02 §5).

### 2.6 Rastgelelik ve tekrar üretilebilirlik

- **Akışlar:** 04'ün Philox üreteci (09.4); akış kimliği `(senaryo_tohumu, ALT_SİSTEM_SENSÖR, sensor_id, akış_türü)`, akış türleri `açılış`, `ölçüm`, `zamanlama`, `arıza`, `sonar_ışın`. Sayaç = örnek numarası (sonarda `(ping_no, ışın_no)`). Bir sensörün çektiği sayı başka sensörü, arıza zinciri ölçüm dizisini, sonar işçi sırası sonucu kaydırmaz.
- **Kayıp ve donmada model yine örnek üretir** ve sayacını ilerletir; arıza bitince ölçüm dizisi arızasız koşuyla bit düzeyinde aynıdır.
- **Dağılımlar** kendi kodumuzdadır (`<random>` dağılımları kullanılmaz): normal, düzgün, üstel, log-normal, Rayleigh (ters CDF). Her sensör kaydı `model_sürümü` taşır; dağılım ve model sürümleri, tohum, ürün profili özetleri ve A7 manifest özeti A6'ya yazılır (04 §2.13).
- **Hedef** (Ç7): aynı ikili ve platformda bit düzeyinde tekrar; macOS-arm64 ile Windows-x64 arasında toleranslı eşdeğerlik. Box–Muller'ın `log/sqrt/trig` çağrıları (09.4) ve Embree (platformlar arası bit eşitliği garantisi yok, 10.9) platform farkı üretebilir. Embree yapısı tek iş parçacıklı ve sabit ayarlarla kurulur; aynı platformda tekrarı M11 kapsamında sınanır [T].
- **Sonar paralelliği:** ışın sonuçları ışın numarasıyla tutulur, kutulara sabit sırada eklenir; paralel indirgeme yoktur (04 §2.12).
- **Yer gerçeği:** her kayıt, `t_meas` anındaki gerçek değeri yan veri olarak taşır; A1'e gitmez, çekirdeğe hiçbir yoldan girmez (02 §3.2 md.5).

---

## 3. Arayüzler

Biçimler C++ imza ve alan listesi düzeyindedir; sözleşmeler ayrı işte dondurulur (iskelet §3).

**Ölçüm kaydı** (05 → 06, 08):

```
struct MeasurementRecord {
  uint16_t sensor_id;  SensorKind kind;  uint8_t inst;  uint32_t seq;   // seq üretimde artar; kayıp arıza boşluk bırakır
  int64_t  t_meas_ns;       // ölçümün geçerli olduğu an (2.2 tablosu)
  int64_t  window_ns;       // ortalama penceresi; 0 = anlık
  int64_t  t_teslim_ns;     // modellenmiş alım anı (sim zamanı); 06 adım partisini buna göre kurar
  uint16_t flags;           // bit0 geçerli, bit1 doymuş, bit2 sensör arızası (02 §3.3) + türe özgü bitler
  float    quality;         // [0,1] normalize kalite (fom, güven, eph'ten); NaN = tanımsız; yalnız A3/A6
  uint32_t fault_mask;      // uygulanan arızalar; yalnız A3/A6
  uint16_t model_version;
  Payload  payload;         // türe özgü etiketli birlik; alanlar 02 §3.3 ile 06 §2.2'deki mesaj alanları
  Truth    truth;           // yer gerçeği; yalnız A3/A6
};
```

A1'e giden alanlar: `t_meas_ns`, `flags`, `inst`, `payload` (06 eşler). `t_teslim_ns` karta gönderilmez; kartın alım zamanı kendi saatidir.

**Eklenti arayüzü:**

```
class ISensorModel {
  SensorInfo   info() const;                                  // id, kind, inst, ürün profili, model_version
  void         reset(const SensorConfig& cfg, const MountTransform& mount, SensorRng& rng);  // açılış hataları
  SchedulePlan plan() const;                                  // Periodic{pay_ns, payda, faz_ns} | EventDriven
  int64_t      next_event_ns(int64_t t_now_ns) const;         // olay sürümlüler (DVL, Ping1D, Ping360)
  void         on_step(const StepView& v);                    // her fizik adımı; ortalama alan sensörler
  void         on_event(const SampleContext& c, MeasurementSink& out);
  void         on_command(const SensorCommand& cmd, int64_t t_apply_ns);  // kart yapılandırması (Ping360)
  void         collect(int64_t t_ns, MeasurementSink& out);   // işçi sonuçlarının sabit sırada tüketimi
};
struct SampleContext {
  int64_t t_ns; uint64_t step_no;
  const WorldSnapshot& snap;  const IEnvironmentView& env;    // 04 §3; yalnız bu çağrı süresince geçerli
  const ISceneQuery& scene;   IWorkerPool& pool;
  std::shared_ptr<const PoseSet> poses;                       // işçilere verilen değişmez kopya
  IFaultHooks& hooks;                                         // model içi arıza kancaları (2.7 / 3 sonu)
};
```

- **`PoseSet`** (değişmez, paylaşımlı sahiplik; 04-sol md.21'in istediği): `t_ns`, `step_no`, araç pozu (`p_n`, `q_nb`, `ν`), gövde referans noktasının araç datumuna göre konumu, hedef ve torpido pozları, dalga listesi sürümü. İşçi işi bitene kadar yaşar; dünya iş parçacığı onu değiştirmez.
- **`MountTransform`**: `l_datum` (araç datumuna göre konum, m), `q_bs` (sensör → gövde). Montaj dönüşümü sim tarafında 05'e aittir ve A5'in **fizik gerçeği** grubundan okunur; otopilotun inandığı montaj 02'nin `ida_params_t` sensör grubundadır ve farklı olabilir (02 §8.1 md.2). Montaj hatası `δq_montaj` [K] ayrıca uygulanır. Datum sabittir; dinamik referans noktası (İDA-0'da CG) torpido ayrılınca kayabileceği için `l = l_datum − r_ref,datum(t)` her örnekte `PoseSet`'ten hesaplanır.
- **`ISceneQuery`** (Embree sarmalayıcı): `intersect(span<Ray>, mask, span<Hit>)`, `occluded(span<Ray>, mask, span<bool>)`; `Hit`: `t`, `prim_id`, `instance_id`, `material_id`, `normal_n`. Statik sahne yüklemede, örnekler `PoseSet` ile güncellenir.

**Sensör merkezi** (04, 06, 08 ile sınır):

```
class SensorHub {
  void reset(const ScenarioConfig& cfg, uint64_t seed, const SceneAssets& a7);
  void attach(Scheduler& s);                                   // periyodikler Scheduler::add; olay sürümlüler tek seferlik olay
  void on_step(const WorldSnapshot& s, const IEnvironmentView& env);
  void drain(int64_t t_until_ns, std::vector<MeasurementRecord>& out);   // 06 parti kurucusu her kontrol adımında
  void on_card_command(const SensorCommand& c);               // 06'dan; uygulandığı adım A6'ya
  IFaultControl& faults();                                     // 08
  SensorCounters counters() const;   // sonar_geç, kamera_düşen, kuyruk_taşma, nan_reddi, kayıp, model olayları
};
```

`drain` teslim zamanı `t_until_ns`'e kadar olan kayıtları `(t_teslim, 06'nın sensör sırası, inst, seq)` anahtarıyla sıralı verir (06 §2.4 sıra kuralı). Teslim kuyruğu sınırlıdır (varsayılan sensör başına 64 kayıt [T]).

**04'ten beklenen** (04 §3 ile sınır; bazıları 04'te henüz yok, Bölüm 8): `WorldSnapshot` içinde `ν̇`'nin gövde bileşenlerinin zaman türevi olduğu tanımı; gövde referans noktasının araç datumuna göre konumu; itici akımları `I_k` (işaretli), `V_bara`, `I_paket`, batarya sıcaklığı; `g_n`; `IEnvironmentView::water_pressure(p_n, t)` (`p − p_atm`, 04 §2.4 formülü), `water_salinity()`; `Scheduler`'da tek seferlik olay (`add_once(t_ns, cb)`).

**A5'ten beklenen** (biçim 08'in): `sensörler[]` `{sensor_id, kind, inst, ürün_profili, montaj (fizik gerçeği), hız, etkin, parametre geçersiz kılmaları}`; ürün profili dosyaları; akustik malzeme tablosu (A7 ile aynı kimlikler); sonar kademesi; ses hızı ve soğurma geçersiz kılmaları; `B_n`; jeodezik başlangıç; arıza listesi.

**Arıza denetimi** (08'in hata enjeksiyonu bu arayüzü sürer):

```
class IFaultControl {
  FaultHandle arm(const SensorFaultSpec& f);     // bir sonraki adım sınırında etkin; uygulandığı adım A6'ya
  void        disarm(FaultHandle h);
  std::span<const ActiveFault> active() const;
};
SensorFaultSpec: fault_id, sensor_id, inst, tür, t_başla_ns, süre_ns (0 = süresiz),
                 şiddet {ofset, oran, ölçek, k, genlik, gecikme_ns, …}, kanal_maskesi,
                 zaman deseni {sürekli | Bernoulli(p) | Gilbert–Elliott(p, r, zaman tabanlı)}, alt_tohum
```

Koşula bağlı başlatma ("derinlik > 2 m olduğunda", 05 §2.9) 08'in senaryo motorunda değerlendirilir; 05 yalnız zamanlı `arm` alır. Gilbert–Elliott geçişleri örnek başına değil sim zamanında tanımlanır (11.15'in zaman ölçeği dersi). Fiziksel arızalar (itici, batarya) 04'ün `apply_fault`'una, iletim hataları 06'nın `FaultInjector`'üne gider; üç katman ayrıdır (05 §1 md.9).

---

## 4. Hata durumları ve davranışı

**Arıza modelleri** (Balaban 2009 sınıfları ve ekleri, 05 §2.9). Hepsi arıza zincirinde, ölçüm zincirinden sonra ve kendi RNG akışıyla uygulanır; aksi yazılmadıkça kanal maskesine uyar.

| Arıza | Uygulama | Not |
|---|---|---|
| Ofset (bias) | `y + δ` | |
| Sürüklenme | `y + ρ (t − t₀)` ya da `y + δ_∞ (1 − e^{−(t−t₀)/τ})` | sim zamanıyla; adım süresinden bağımsız |
| Ölçek | `a(t) · y` | DVL'de ses hızı ofsetiyle karıştırılmaz |
| Gürültü artışı | `y + σ_nom √(k² − 1) ξ_arıza` | varyanslar toplanır; ölçüm akışı değişmez |
| Donma | son değer tekrarlanır; iki tür: `t_meas` ilerler / `t_meas` da donar | ikinci tür 02'nin "ölçüm zamanı ilerlemiyor" tazelik denetimini sınar (iskelet §4) |
| Sıçrama | olasılık `p` ya da belirli anlarda `±A` | aykırı değer kapısını sınar |
| Kayıp / aralıklı kayıp | kayıt teslim edilmez; `seq` boşluğu kalır | model sayacı ilerler (2.6) |
| Kısmi bozulma | IMU tek eksen; DVL tek ışın; sonar kör sektör ya da kazanç düşüşü; Ping360 motor takılması (açı tekrarı); sızıntı tek prob | DVL ışını ve sonar sektörü çözümden **önce** etkili olmalıdır: model `IFaultHooks::beam_alive(i)`, `sector_gain(açı)` kancalarını sorar; kancalar ölçüm RNG'sini kullanmaz |
| Geç teslim | `t_teslim + Δ`, isteğe bağlı titreşim; sensör içi sıra varsayılan korunur, `sıra_boz` seçeneği | |
| Yanlış geçerlilik | `valid = true` iken değer bozuk ya da `valid = false` iken değer doğru | "sensöre körü körüne güvenme" testi (05 §2.9) |
| NaN / aralık dışı | değer NaN ya da sınır dışı | 02'nin girdi reddini sınar (02 §4) |
| Kendini bildiren arıza | `flags.sensör_arızası` | |

**Model ve altyapı hataları:**

| Durum | Tespit | Davranış |
|---|---|---|
| Ürün profili ya da montaj eksik, birim hatalı | yüklemede | senaryo reddedilir, alan adıyla |
| Yapılandırılan hız fiziksel sınırı aşıyor (basınç > `1/(2T_dönüşüm + t_okuma)`; DVL mod tablosu dışı) | yüklemede | reddedilir |
| A7 manifest özeti 04/07 ile uyuşmuyor; akustik malzeme kimliği tabloda yok | yüklemede | sonar etkinleşmez, çalıştırma reddedilir; bilinmeyen malzeme için varsayılan kullanılmaz |
| Ping360 yapılandırması aralık dışı | kart komutu | cihaz benzeri reddet ya da sınıra çek [T], olay |
| Dönüştürücü havada (yüzeyde yalpa) | her ping | eko yok: Ping360 yalnız gürültü tabanı, Ping1D güven 0, DVL ışını geçersiz |
| Model çıktısında NaN/Inf (arıza değilken) | her örnek | kayıt `geçerli = false`, değer önceki geçerli değer, `nan_reddi` sayacı ve olay; NaN karta gitmez |
| Sonar işi modellenen teslimde hazır değil | toplama anı | SIL: beklenir. HIL 1x: profil atılır, `sonar_geç` sayacı, A6; HIL + bariyer: aynı ve çalıştırma geçersiz (modellenen sensör akışı değişmiştir; 06 §2.5 `bulk_drop` ile aynı kural). Bir sonraki pingin zamanı hesap süresinden bağımsızdır |
| Kamera karesi geç | teslim anı | 2.5 |
| Teslim kuyruğu dolu | kuyruk | en eski kayıt atılır, `kuyruk_taşma`, HIL'de çalıştırma geçersiz |
| GNSS jeodezik dönüşüm ya da `B_n` tanımsız | yüklemede | ilgili sensör etkinse reddedilir |

---

## 5. Kalite kademelerinin etkisi

İskelet §6: sonar "sade" ve "tam" kademelidir, "kapalı" kademesi yoktur; kamera dışı diğer sensörler her kademede tam çalışır. Kademe yalnız profilin içeriğini değiştirir, biçimini değiştirmez (02 §5); kart kademeyi bilmez, kademe A6'ya yazılır (06 §2.5).

| Etki | Sade | Tam |
|---|---|---|
| Ping360 örnek üst sınırı `N_kademe` | 600 (06 §2.5 önerisi) | 1200 |
| Biriktirme | eşevresiz yoğunluk | eşevreli karmaşık genlik + darbe zarfı evrişimi |
| Benek | istatistiksel (üstel yoğunluk / Rayleigh genlik) | saçıcı fazlarından fiziksel |
| Işın deseni | ana lob ağırlığı | ana lob + yan loblar (koni genişletilmiş) |
| Menzil kutuları, iki yönlü yayılım, soğurma, akustik gölge, TVG/kazanç, yakın alan bandı, kuantalama | var | var |
| Çok yollu (yüzey, duvar; dönüş yolu kapanışlı) | yok | 1–2 sekme |
| Su yüzeyi | ortalama düzlem | dalga yaması |
| Yankılanma kuyruğu | yok | isteğe bağlı |
| Ping başına başlangıç ışını (başlangıç önerisi) | 1000 [T] | 5000 [T] (D10 #13'ün varsayımı) |
| Ping1D, DVL ışın izleme | her iki kademede aynı | aynı |

**Işın sayısı ve hesap yükü** (işlem bütçesi hesabı, ölçüm değil):
- Ping başına kesişim sorgusu ≈ `N_ışın · (1 + N_sekme + N_dönüş)`; saniyedeki yük, ping hızıyla çarpılır. Ping hızı üstten `1/(T_dinleme + T_ek)` ile sınırlıdır; en ağır durum kısa menzildir (7 m'de ≈ 107 ping/s akustik sınır). 5000 ışın + 2 ek yol parçası ≈ 15.000 sorgu/ping, 100 Hz'te 1,5 milyon sorgu/s (D10 #13).
- Eşevreli birikim, ortak darbe biçimi ve kesirli gecikmeyle uygulanırsa ping başına birkaç milyon basit işlem mertebesindedir; her yolun 1200 frekans noktasına katkısı ayrı hesaplanırsa 18 milyon karmaşık katkı/ping (≥ 72 milyon gerçek işlem) olur. Bu bölüm birinci yolu seçer.
- Menzilin en uzak ucunda yanal aralık kutu boyuna eşit seçilirse ≈ 22.008 ışın/ping gerekir; bu bir yakınsama garantisi değildir (D10 #13). Işın sayısı yakınsama testiyle belirlenir (Bölüm 6).
- "Ping360 için tek ışın, ucuz, < 1 ms" iddiası desteklenmiyor (10.5, Ç9); burada tekrarlanmaz. Gerçek zamanlılık M11 ile ölçülür. Ağır işler işçi havuzundadır; dünya iş parçacığının adım süresini etkilemeyeceği ölçülmüş bir güvence değildir (04-sol "Kurallar").

**Kamera:** "kapalı" kademede kare ve dışarıdan algılama yoktur; "sade"/"tam" çözünürlük ve hız 07'nindir; sözleşme (2.5) aynıdır.

**Kayıt kademesi** (08): yer gerçeği yan verisi "telemetri" ve üstünde kaydedilir; sonar profilleri "tam"da.

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| Sonar ayrıntısı (iskelet §8) | tam kademe gerçek zamanlı (eşevreli + çok yollu, 1200 örnek) | HIL'de sade kademe; tam kademe yalnız SIL'de | M11 |
| Ping başına ışın sayısı | yakınsama testiyle: `N` iki katına çıkınca kutu ortalama yoğunluğunun bağıl değişimi < %5 ve gölge kenarı ≤ 1 kutu kayıyorsa `N` yeterli [T] | tahmini başlangıç değerleri (5. başlık) | M11 |
| Embree platform eşdeğerliği | toleranslı istatistiksel eşdeğerlik (7. başlık) | platform başına altın profil | M11, M10 |
| Ping360 bağlantı zamanlaması | üretici tarama sürelerine uydurulmuş `T_motor`, `T_işlem`; baud profili | ölçülmüş profil | M36 |
| Sonar ve kamera gerçekçilik kalibrasyonu (yansıtıcılık, ışın deseni, TVG) | [K] varsayılanlar + açık veri kümesiyle uydurma (7. başlık) | — (gerçek sensör kaydı K8 kapsamında değil) | M14 |
| Kamera teslim gecikmesi ve kare düşürme | `L_kam` [K]; HIL'de geç kare düşürülür | kamera yalnız görünür pencereyle | M12, M30, M13, M28 |
| IMU hızı | 400 Hz | 200 Hz | M5 |
| IMU süzgeç ve gürültü konvansiyonu | aralık ortalaması, `N/√Δt` | fizik hızında üretim + süzgeç | 7. başlıktaki Allan testi |
| Sensör üretim süresi (06'nın `C_sim` bütçesindeki pay) | dünya iş parçacığında, adım başına | ağır modeller işçiye | M15 ve M11 ile birlikte (Bölüm 8, yeni ölçüm önerisi) |

---

## 7. Test yaklaşımı ve kabul ölçütleri

Bütün sayısal eşikler, aksi belirtilmedikçe [T]'dir ve 09'da kesinleşir. İstatistiksel testler sabit tohumla, en az iki tohumda koşar.

**Ortak testler**
- **Zaman:** her kayıtta `t_teslim ≥ t_meas`; aynı sensörde `t_meas` kesin artan; periyodik sensörlerde 10⁶ örnekte birikimli kayma 0 ns; ortalama alanlarda `window_ns` beklenen değer.
- **GM süreci zaman adımından bağımsızdır:** aynı `τ`, `σ_∞` ile 400, 200 ve 100 Hz'te üretilen dizilerde durağan σ ve saniye cinsinden özilinti (`e^{−Δt/τ}`) %5 içinde aynı (05.1'in doğrudan sınaması).
- **Arıza zinciri:** her arıza türü istenen adımda başlar ve biter; arıza penceresi dışında ve etkilenmeyen kanallarda ölçüm dizisi arızasız koşuyla bit düzeyinde aynı; kayıp sonrası dizi aynı; gürültü artışında varyans oranı `k²` (%5 içinde).
- **Tekrar:** aynı platformda iki koşunun bütün sensör kayıtları (sonar dahil) bit düzeyinde aynı. Platformlar arası: skaler sensörlerde bağıl 1e-6; sonar profillerinde kutu başına fark ≤ 1 LSB oranı %99'un üstünde ve yoğunluk histogramları arasında JS ıraksaması < 0,01.

**Sensör başına**

| Sensör | İstatistiksel ve geometrik sınama |
|---|---|
| IMU | Durağan 4 saatlik SIL kaydının Allan eğrisi: τ = 1 s'de `N` %10 içinde; GM parametreleri özilintiden geri okunur; durağan araçta ivmeölçer ortalaması `−R_bsᵀR_nbᵀ g_n`; sabit `ω` ile dönerken kaldıraç kolunda `ω × (ω × l)`; dünya dönüşü açıkken ortalama jiroskop `R_nbᵀ ω_ie`; doyum bayrağı |
| Manyetometre | Tam dönüşte elipsoid uydurma `W`, `V`'yi geri verir; itici akımı taramasında doğrusal regresyon `K_k`'yı geri verir |
| Basınç | Sabit derinlikte ortalama ve σ ≈ 20 Pa; dalgada basınç spektrumunun derinlikle sönümü 04'ün `F_p` işleviyle %2 içinde; hız sınırı ihlali reddedilir; `t_meas` pencere ortası |
| GNSS | Anten su kalınlığı taramasında izleme olasılığı eğrisi parametreyle uyumlu; yeniden kilitlenme süresi dağılımı tek değer değil (en küçüğü ≥ 2 s sıcak başlangıçta); 24 saatlik durağan kayıtta CEP %10 içinde; `eph` raporu `k_rapor`'a uygun |
| DVL | Düz tabanda bilinen irtifada `altitude` ve `beam_dist` geometrik değerlerde; bilinen hızda ortalama hata ölçek parametresiyle uyumlu; `t_teslim − t_meas = r̄/c + T_işlem + T_iletim` (2r/c değil); mod geçişinde histerezis; kilit kaybında `vel_valid = false`, hız 0, irtifa −1; yüzeyde yalpada havaya çıkan ışın geçersiz; su takibinde akıntı hızı geri okunur |
| Ping1D | Düz tabana bilinen mesafede hata ≤ menzilin %0,5'i + `Δc/c` etkisi; eğimli tabanda koni etkisi (en yakın nokta); güven 0 koşulları |
| Ping360 | Bilinen mesafedeki duvarın tepe kutusu `2r/(c · sample_period · 25 ns)` ± 1; silindir arkasındaki gölge uzunluğu geometriyle ± 1 kutu; tarama sırasında dönen araçta görüntünün her açıda kendi pozuyla bozulması; 50 m ve 1 m ayarlarında tarama süresi üretici değerleriyle %15 içinde; `T_ping` menzil ve baud değişince değişir (sabit değil); sade kademede homojen bölgede genlik dağılımı Rayleigh (KS testi, p > 0,05); genlik ve yoğunluk soğurmasının ayrı birim testi (`10^{−2αr/20}` ve `10^{−2αr/10}`); çok yollu katkı yalnız dönüş yolu kapanınca |
| Kamera sözleşmesi | `t_meas` pozlama ortası ve 26/27 adım dizisi; HIL'de geç kare düşürülür ve sayılır, teslim zamanı değişmez |
| Batarya | Bilinen sabit akımda ölçek ve ofset; ADC kuantalama basamağı; aralık dışında doyum bayrağı |
| Sızıntı | Olaydan sonra algılama gecikmesi parametreyle; sahte pozitif oranı parametreyle |

**Açık veri kümeleriyle karşılaştırma** (sınadığı ve sınayamadığı birlikte):

| Veri kümesi | Sınar | Sınayamaz |
|---|---|---|
| Ping360 havuz kayıtları (Hasan vd. 2024; MIT lisanslı, 20 CSV, satır başına açı + 1200 adet 0–255 yoğunluk; 3 × 6 × 5 m havuz, 7 m menzil) [Y, D10 #7b] | Havuz ikizinde (makaledeki ölçülerle) akustik gölge geometrisi, duvar ekosunun menzili, havuz sınırları ötesinde görünen yansımalar, yakın alan bandı, yoğunluk ve LBP doku histogramları (KL/JS/EMD, ACOUSIM yöntemi, 10 §2.8) | Faz ve eşevreli modelin doğruluğu (faz sütunu yok); zamanlama ve hareket bozulması (zaman damgası yok); mutlak seviye (kazanç ve TVG eğrisi bilinmiyor); malzeme yansıtıcılığının fiziksel değeri (yalnız uydurma) |
| Underwater Caves (Mallios 2017; iki MSIS + DVL + IMU + derinlik) [ikincil, 10 §2.8] | MSIS geometrisi ve tarama zamanlaması, DVL/IMU/derinlik gürültü düzeyleri | Ping360'ın frekansı ve ışın deseni (farklı cihazlar) |
| SOLAQUA (BlueROV2, DVL + IMU) ve AQUALOC (kamera + IMU + basınç) [ikincil] | DVL ve IMU gürültüsü ile basınç davranışının mertebesi | Bizim ürün profillerimizin değerleri (farklı sensörler); A50'nin iç davranışı |

Bu karşılaştırmalar modeli kalibre eder; "gerçekçi" iddiasını tek başına kanıtlamaz (10 §2.4.4). Gerçek veriyle nicel doğrulanmış, gerçek zamanlı ve havuzda çok yollu yayılım üreten açık kaynak sonar simülatörü bulunamadığı için ölçüt kendi metriklerimizdir.

**Kabul ölçütleri:** ortak ve sensör başına testler iki işletim sisteminde geçer; aynı platformda bit düzeyinde tekrar; Hasan 2024 havuz ikizinde gölge ve duvar menzili ± 1 kutu ve yoğunluk histogramı JS ıraksaması < 0,1 [T; ilk uydurmadan önce dondurulur, 02-sol md.24'ün dersi]; M11 raporu çıkmıştır.

---

## 8. Açık noktalar ve iskelete öneriler

**Komşu bölümlerle uyumsuzluklar** (bölüm iskelete uydu; aşağıdakiler ilgili bölümlere ve mimara bildirilir):

1. **Adım sınırına bağlama kuralı.** İskelet §4 "en yakın adım sınırı", 04 §2.11 "ideal zamana eşit ya da sonraki ilk adım" diyor. 05 her ikisiyle çalışır (damga o adımın zamanıdır), ama tek kural seçilmeli.
2. **04 hâlâ 1 ms adım kullanıyor** (02-sol son not). 1,25 ms'de 30 Hz kamera 26/27 adımdır, 33/34 değil.
3. **Basınç ölçüm zamanı:** 06 `IDA_BARO` "dönüşüm sonu" diyor; 05 D1 penceresinin ortasını kullanır (ölçümün geçerli olduğu an, iskelet §3.1). 06'nın tablosunun bu tanıma çekilmesi önerilir.
4. **02 §3.2 md.3 "veri hazır anındaki kart saati"** tanımı DVL'nin yansıma anıyla ve basıncın pencere ortasıyla çelişiyor (02-sol md.14). Öneri: "ölçümün geçerli olduğu an; sensör başına tanımı 05'teki tabloda".
5. **04'ün arayüzünde eksikler:** `ν̇`'nin anlamı (gövde bileşenlerinin türevi mi, eylemsiz ivme mi); dinamik referans noktasının (CG) torpido ayrılınca kayması ile iskelet §3.1'in "A5'te tanımlı gövde referans noktası" arasındaki fark; `water_pressure` ve `water_salinity` sorguları; tek seferlik zamanlanmış olay; işçilere verilen durumun sahipliği (05 burada `PoseSet` ile çözdü, 04'ün onaylaması gerekir).
6. **02 dünya dönüşünü ihmal ediyor;** 05'in IMU modeli varsayılan olarak içerir. Bu bilinçli bir model farkıdır; etkisi SIL'de ölçülür (02 §2.3).

**İskelete öneriler:**

1. §6 sonar satırının içeriği: "Sade: eşevresiz model, 600 örnek, çok yollu yok; Tam: eşevreli + 1–2 sekmeli çok yollu, 1200 örnek".
2. §4'e: "IMU örnekleri adım sınırına bağlı olduğu için IMU saat kayması modellenmez"; gerçek sensörün zamanlamasından bilinçli bir sapmadır.
3. §3.1'e: "ölçüm zamanı, ortalama alan ölçümlerde pencere ortasıdır; IMU'da aralık sonu ve aralık uzunluğu taşınır".
4. §8'e yeni satır önerisi: "Sensör üretim süresi (06'nın `C_sim` payı ≈ 0,40 ms tahmini içinde) ve HIL'de sonar geç sonuç oranı" ölçümü (M11 ile M15'in birleşimi; 00 Bölüm 7'ye eklenmesi önerilir).
5. M14 gerçek Ping360 kaydı ve 750 kHz malzeme ölçümü istiyor; K8 gerçek sensör tedarikini gündem dışı bırakıyor. Bu aşamada kalibrasyon yalnız açık veri kümesiyle yapılır ve akustik parametreler [K] olarak kalır; M14'ün kapsamının buna göre not edilmesi önerilir.

**Açık noktalar:**
- Gürültü yoğunluğu konvansiyonu (veri sayfasının tek taraflı yoğunluğu ile Allan `N` arasındaki √2 farkı) sözleşmede tek yerde sabitlenmeli.
- İç basınç ve iç sıcaklık sensörü iskeletin setinde yok (02 §8.1 md.5); 2.3'teki gövde içi sıcaklık modeli bunlara kaynak olabilir; eklenip eklenmeyeceği mimarın kararıdır.
- İtici kaynaklı titreşimin IMU'ya etkisi ve aracın kendi akustik gürültüsünün sonara etkisi için kaynak yok; ilk sürümde modellenmez.
- Torpido kabarcık izinin sonarda görünmesi kaynaksız (10 §2.7); ileri kademe.
- Ping1D'nin mesafe algoritması, A50'nin 3 ışınla çözümü, dönüştürücü tipi ve kilit kazanım süresi belgelenmemiş (05 §4 md.5); [K] olarak kaldı.
- 03'ün atış kontrol mantığını sonar algoritmasından bağımsız sınamak için sim tarafında anlamsal (T0) algılama üreteci önerilebilir (05 §3.2); kademe değil, yalnız SIL test aracı olur. Kapsama alınıp alınmayacağı 03 ile birlikte kararlaştırılmalı.
- Derinliğe bağlı akıntı profili gelirse DVL su takibi aynı arayüzle onu kullanır (04 §8).
