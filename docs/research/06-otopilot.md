# 06 - Otopilot Mimarisi: Seyrüsefer, Güdüm, Kontrol ve HIL Protokolü

> **Durum: ham araştırma raporu (yazıldığı haliyle korunuyor).** Bu rapor bağımsız doğrulamadan geçti ve bilinen hatalar içeriyor. Ayrıca dil kararı C/C++ olmadan önce, Rust varsayımıyla yazıldı; Rust'a özgü öneriler geçersizdir. Doğrulaması: [dogrulama/06-codex.md](dogrulama/06-codex.md). Düzeltmeler ve geçerli kararlar: [00-duzeltmeler-ve-kararlar.md](00-duzeltmeler-ve-kararlar.md). Çelişki halinde 00 numaralı belge geçerlidir.

Araştırma tarihi: 2026-11-10. Kapsam: tek bir hibrit (yüzey + dalış) insansız deniz aracının otopilot firmware'i. Hedef kart şimdilik STM32U5A9J-DK (Cortex-M33, 160 MHz). HIL ve SIL birlikte kullanılacak. Dil eğilimi Rust (Embassy + `no_std` çekirdek).

Okuma notu:
- Her önemli iddianın yanında kaynak etiketi [Sxx] vardır; URL'ler Bölüm 6'dadır.
- **"Doğrulanmadı"**: Bu oturumda birincil kaynaktan teyit edilemedi ya da yalnızca forum veya ikincil kaynağa dayanıyor.
- **"Kendi tahminim"**: Mühendislik hesabı veya çıkarımdır; ölçümle doğrulanmalıdır.
- Kaynak kodu alıntıları, ilgili dosyanın 2026-11-10 tarihindeki `master`/`main` dalından okundu.
- Kapsam dışı: Torpido atış kontrolünün otopilotla arayüzü bu raporda ele alınmadı. Bu konu ayrı araştırmada ve ilgili güvenlik gereksinimleriyle birlikte değerlendirilmeli.

---

## 1. Özet (10 madde)

1. **Referans mimarilerin hepsi aynı katmanları kullanıyor:** sürücü/HAL → kestirici (EKF) → kip mantığı ve güdüm → konum/derinlik kontrolü → tutum/hız kontrolü → itki tahsisi (mixer/allocator) → çıkış sürücüsü. ArduSub'ın ana döngüsü 400 Hz'tir. IMU, rate controller, motor çıkışı, AHRS ve kip güncellemesi her IMU örneğinde çalışır; diğer işler 1–50 Hz'lik bir zamanlayıcı tablosundadır [S1][S2]. PX4'ün UUV desteği resmi olarak "experimental" etiketlidir [S5]. Örnek alınacak mimari olarak ArduSub daha olgun, PX4'ün katman ayrımı daha temiz.
2. **Seyrüsefer için öneri: `f32` ile 15 hata durumlu (error-state / multiplicative) EKF.**
   - Hata durumu: konum, hız, tutum, gyro bias ve ivme bias.
   - Ölçümler sırayla, tek tek (skaler) füzyonlanır: derinlik, DVL hızı, manyetometre, yüzeyde GPS.
   - Benzer örnekler: PX4 EKF2 hata durumlu formülasyon kullanır (Solà türevi) [S7][S8]. ArduPilot EKF3 24 durumludur, hedef adımı 12 ms'dir ve derleme bayrağı verilmezse `float` kullanır [S3][S4]. MSS araç kutusunda 15 durumlu bir MEKF referansı var [S30].
   - Cortex-M33'te yalnızca tek duyarlıklı FPU olduğu için [S26] konum yerel NED çerçevesinde `f32` tutulmalı, yalnızca orijin `f64` olmalı.
3. **Yüzey ve dalış geçişini kestiricide ayrı bir "kaynak kipi" olarak modellemek gerekiyor.**
   - GPS füzyonu derinlik eşiği ve fix kalitesiyle kapılanır.
   - Yüzeye çıkıldığında ilk fix ki-kare kapısından geçerse normal güncelleme yapılır.
   - Geçmezse ama kalite sürekli iyiyse kontrollü bir "position reset" yapılır, sayaç artırılır ve kayda geçirilir. Bu yaklaşım PX4 ve ArduPilot'taki reset ve kaynak değiştirme mekanizmalarına benzer [S7][S9].
4. **Güdüm için yatay düzlemde LOS/ILOS, dikeyde referans modelli derinlik/irtifa setpoint'i önerilir.**
   - ILOS, akıntıya karşı integral etkisi sağlar ve kararlılık sonuçları hakemli literatürde var [S33][S34][S35].
   - Sonar taraması, biçerdöver (boustrophedon) deseninin rota noktası listesine çevrilmesiyle yapılır.
   - MSS'teki `ILOSpsi.m` ve `ALOS3D.m`, Rust'a taşınabilecek hazır referanslardır [S30].
5. **Kontrol için kademeli PID ile başlanmalı.**
   - Kademeler: açı → açısal hız → tork; derinlik → dikey hız → kuvvet; hız → itki.
   - Tahsis katmanı doyma bilgisini kontrolcüye geri vermeli. PX4 bunu koşullu integrasyonla yapıyor [S11][S12]. ArduSub'ın 6DOF mixer'ı ise kendi normalizasyonunu kodda "band aid" diye tanımlıyor [S13].
   - LQR ve SMC bu işlemcide hesaplama açısından ucuz.
   - MPC de uygulanabilir: TinyMPC, STM32F405 (M4, 168 MHz) üzerinde N=15 ile 500 Hz'te koştu [S37]. Ancak ilk sürüme alınmamalı.
6. **Görev yönetiminde iki katman olmalı.**
   - Güvenlik açısından kritik kip ve failsafe katmanı: Rust `enum` + `match` ile hiyerarşik durum makinesi (FSM).
   - Görev sıralaması: MAVLink mission item uyumlu basit bir yürütücü.
   - Davranış ağaçları (BT) modülerlik sağlıyor [S39][S40], ama `no_std` çekirdekte ilk sürüm için gerekli değil.
   - ArduSub'ın failsafe seti iyi bir kontrol listesi: sızıntı → SURFACE, EKF kötü 2 s, ana döngü kilitlenmesi 2 s, iç basınç ve sıcaklık, GCS kaybı, batarya, arazi [S2].
7. **Gerçek zamanlı yapı için öneri: öncelik seviyelerine ayrılmış `InterruptExecutor`'larla Embassy.**
   - Yüksek öncelik: kestirim ve kontrol hattı. Orta: sürücüler ve haberleşme. Düşük (thread executor): görev, telemetri ve kayıt.
   - Aynı executor içindeki Embassy görevleri birbirini kesemez (cooperative). Öncelikli kesme ancak ayrı executor'larla elde edilir [S17][S19].
   - Biçimsel zamanlama analizi istenirse alternatif RTIC'tir (SRP) [S20].
8. **HIL protokolü için iki ayrı düzlem önerilir.**
   - **Simülasyon bağlantısı:** `postcard` + COBS üzerine özel ikili protokol [S23][S24]. Şema paylaşılan bir Rust crate'inde durur. Her sensör için ayrı, zaman damgalı çerçeve gönderilir ve native USB kullanılır. Gerekçe: MAVLink HIL mesajlarında DVL, sonar, sızıntı ve batarya karşılığı yok ya da eksik [S14].
   - **Yer istasyonu ve telemetri:** `rust-mavlink`'in `embedded` özelliği [S21]. Bu özellik `embedded-io(-async)` 0.7 kullanır; `embassy-stm32` 0.6 da aynı sürümü kullanıyor [S22]. QGroundControl uyumu bu düzlemde sağlanır.
9. **Lockstep, firmware'e sahte saat vermeden kurulabilir.**
   - Kontrol hattı IMU örneği geldiğinde tetiklenir; bu, ArduPilot'taki `wait_for_sample` "metronom" desenidir [S6].
   - `dt` değeri örneğin kendi zaman damgasından hesaplanır.
   - Simülatör her IMU çerçevesinden sonra aynı sıra numaralı aktüatör çerçevesini bekler. Bu, PX4 SITL'deki lockstep sırasının aynısıdır [S10].
   - HIL'de 1x gerçek zaman yine de gereklidir. PX4 HITL'in lockstep olmadığı bilgisi forum düzeyindedir, doğrulanmadı [S16].
10. **SIL ve test yaklaşımı.**
    - Çekirdek, `no_std`, `alloc` ve `async` içermeyen saf `step()` fonksiyonlarından oluşmalı.
    - HAL sınırı yazmaç seviyesinden değil, "SI birimli ve zaman damgalı sensör örneği" seviyesinden geçmeli.
    - Test katmanları: host'ta `cargo test` ve `proptest`; MSS/Python referans çıktılarıyla altın testler; senaryo tabanlı SIL (ArduSub autotest'i örnek alınabilir [S15]); kartta `defmt-test`; CI'da self-hosted HIL regresyonu.
    - Kontrolcü ayarı SIL'de lockstep sayesinde gerçek zamandan hızlı, maliyet fonksiyonuna dayalı otomatik aramayla yapılmalı.

---

## 2. Bulgular

### 2.1 Referans mimariler

#### ArduSub (ArduPilot)

- **Katmanlar:** Kip `run()` → `AC_AttitudeControl` (açı P → açısal hız PID) / `AC_PosControl` → `AP_Motors` (mixer) → `AP_HAL` (PWM) [S1]. Açı kontrolcüsü açı hatasını karekök eğrili bir P ile hız setpoint'ine çevirir; hız döngüsü PID'dir [S1].
- **Döngü frekansları (kaynak):**
  - `MAIN_LOOP_RATE` = 400 [S2].
  - Her döngüde çalışan `FAST_TASK`'lar: `ins.update`, `run_rate_controller`, `motors_output`, `read_AHRS`, `read_inertia`, `update_flight_mode`, `update_surface_and_bottom_detector`.
  - Zamanlayıcı tablosundan örnekler: `fifty_hz_loop` 50 Hz, `rc_loop` 50 Hz, GPS 50 Hz, `update_batt_compass` 10 Hz, `read_rangefinder` 20 Hz, `update_altitude` 10 Hz, `three_hz_loop` 3 Hz, `one_hz_loop` 1 Hz, GCS alma/gönderme 400 Hz.
  - Her tablo girdisi bir "maksimum süre (µs)" bütçesi taşır [S2].
- **Zamanlayıcı davranışı:** Bütçeye sığmayan iş atlanır. Görevler bloklamamalı ve en kötü durum süreleri öngörülebilir olmalı [S6].
- **İş parçacıkları:** Ana döngü `ins.wait_for_sample()` ile IMU örneğini bekler; bu çağrı "metronom" işlevi görür. Bunun dışında 1 kHz'lik zamanlayıcı geri çağrısı ve yavaş işler için ayrı bir IO iş parçacığı vardır [S6].
- **Kipler:** MANUAL, ACRO, STABILIZE, ALT_HOLD (derinlik sensörü gerekir), AUTO/GUIDED/CIRCLE/POSHOLD (konum ve derinlik gerekir), SURFACE, MOTOR_DETECT ve SURFTRAK (mesafe ölçerle tabandan sabit yükseklik) [S9b]. GUIDED'da 3 s boyunca yeni hedef gelmezse araç son hedefte kalır [S9b].
- **Failsafe'ler (`ArduSub/failsafe.cpp`)** [S2]:
  - Ana döngü kilitlenmesi: 2 s.
  - EKF varyansı kötü: 2 s üst üste sürerse eylem (ör. disarm).
  - Pilot girişi zaman aşımı.
  - İç basınç ve iç sıcaklık: 2 s.
  - Sızıntı → `SURFACE`.
  - GCS kaybı → disarm, hold veya surface.
  - Çarpışma tespiti.
  - Arazi verisi kaybı → POSHOLD veya ALT_HOLD.
- **DVL entegrasyonu:** Yardımcı bilgisayardaki (BlueOS) eklenti `VISION_POSITION_DELTA` mesajını gönderir (ardupilotmega dialect, id 11011). EKF3 kaynakları "ExternalNav" yapılır (`EK3_SRC1_VELXY` = 6 vb.) [S27][S28]. `VISION_SPEED_ESTIMATE` yolunun eklentide kullanılmadığı forumda söyleniyor; doğrulanmadı [S28].
- **Klasik HIL'in kaldırılması:** ArduPilot, MAVLink üzerinden klasik HIL desteğini bıraktı. Forumda gösterilen gerekçe, USB üzerinden EKF'nin yeterince hızlı ve tutarlı beslenememesi; doğrulanmadı [S29]. **Ders:** HIL bağlantısının bant genişliği ve gecikme bütçesi baştan hesaplanmalı (bkz. 2.7).
- **Test altyapısı:** ArduSub SITL autotest dosyasında `AltitudeHold`, `PositionHold`, `GCSFailsafe`, `Surftrak`, `DiveMission`, `TerrainMission` gibi senaryolar var [S15].

#### PX4

- **UUV desteği:** "Experimental". Desteklenen platformlar BlueROV2 Heavy (Manual/Stabilized/Acro/Altitude/Position kipleri) ve HippoCampus. Otonom görev desteği sınırlı [S5].
- **UUV modülleri:** Kaynakta `uuv_att_control` ve `uuv_pos_control` var. `uuv_att_control`, dönme hatası üzerinden P ile tork setpoint'i üretir ve tork/itki değerlerini kırpar [S5b].
- **Tahsis:**
  - `control_allocator` modülü bu işi yapar. Yöntemlerden biri `ControlAllocationSequentialDesaturation`.
  - Tahsis edilemeyen tork ve itki `control_allocator_status` mesajıyla yayınlanır [S11].
  - Rate controller doyma bayraklarını alır ve doyma yönünde integrali dondurur [S12]. **Anti-windup için örnek alınacak arayüz deseni budur.**
- **EKF2:**
  - Formülasyon hata durumlu; türetme dosyası Solà'nın ESKF notunu referans gösterir ve tangent-space indekslemesi kullanır [S7][S8].
  - Durumlar: quaternion, NED hız, konum, gyro ve ivme bias'ları, manyetik alan ve bias, rüzgâr, arazi [S7].
  - Sensör gecikmesi için gecikmeli füzyon ufku, FIFO tamponlar ve tamamlayıcı filtreyle ileri yayılım kullanılır [S7].
  - `EKF2_PREDICT_US` varsayılanı 10000 µs, yani 100 Hz [S8b]. Güncellemelerde Joseph formu kullanılır [S7].
  - `EKF2_GPS_MODE=1` (dead-reckoning), GNSS'in uzun süre kaybolabileceğini varsayar ve başka kaynak varken GNSS'e reset yapmaktan kaçınır [S7].
  - Enlem/boylam `double`, filtre `float` [S8c].
- **HIL/SITL:**
  - SITL'de lockstep vardır: simülatör zaman damgalı `HIL_SENSOR` gönderir, PX4 bir tur kestirim ve kontrol koşar, `HIL_ACTUATOR_CONTROLS` döner. Simülatör bu cevabı bekler, sonra fiziği bir adım ilerletir. Başlangıçta bir "freewheeling" dönemi vardır [S10].
  - HITL'de sensör verisi `mavlink_receiver.cpp` üzerinden gelir ve gerçek aktüatörler bloklanır [S10b].
  - HITL belgesi düşük gecikmeli bağlantı ister ve topluluk desteklidir [S10c]. Lockstep'ten bahsetmez [S10c][S16].

#### Akademik / açık kaynak

- **Fossen MSS (Matlab):**
  - Rota takibi: `GNC/ILOSpsi.m`, `ALOSpsi.m`, `ALOS3D.m`, `LOSchi.m`.
  - Seyrüsefer: `INS/functions/ins_mekf.m`. 15 boyutlu hata durumlu MEKF; quaternion ve Gibbs vektörü kullanır [S30].
- **PythonVehicleSimulator (MIT lisanslı):** `remus100.py` (L = 1,6 m, rudder + stern plane + pervane) ve `refModel3` referans modeli. ILOS/ALOS bu sürümde yok [S31].
- **KTH/SMaRC:** AUV için davranış ağacı tabanlı kontrol mimarisi [S39].

#### Dersler

- (a) "IMU örneği = metronom" deseni hem gerçek donanımda hem HIL'de doğal bir tetik sağlar.
- (b) Tahsis katmanından kontrolcüye doyma geri bildirimi zorunlu.
- (c) DVL gibi standart dışı sensörleri standart mesajlara sıkıştırmak kırılgan bir çözüm [S28].
- (d) Failsafe'ler zaman eşikli ve parametrik olmalı.
- (e) USB üzerinden HIL'de bant genişliği ve gecikme kritik.

### 2.2 Seyrüsefer (durum kestirimi)

#### Problem

- Su altında konum, DVL'nin bottom-track hızının tutumla dünya çerçevesine döndürülüp integre edilmesiyle bulunur (dead reckoning). Derinlik ise basınç sensöründen doğrudan ölçülür.
- Yön (heading) hatası doğrudan yanal kaymaya dönüşür. 1° yön hatası, kat edilen mesafenin yaklaşık %1,7'si kadar kayma demektir (sin 1° ≈ 0,0175, geometrik hesap).
- Bu yüzden manyetometre kalibrasyonu ve iticilerden gelen manyetik parazit, DVL kalitesi kadar önemlidir.
- AUV seyrüseferi üzerine kapsamlı bir derleme için: Paull vd. 2014 [S41].

#### Öneri: Error-state (multiplicative) EKF

- **Nominal durum:** p (NED), v (NED), q (gövde → NED), b_g, b_a.
- **Hata durumu:** δp, δv, δθ, δb_g, δb_a. Toplam **15**; MSS'teki `ins_mekf` ile aynı boyut [S30].
- **Opsiyonel durumlar:** basınç ofseti (1), su akıntısı (2), manyetometre bias'ı (3). Toplamda 15–21 durum. ESKF teorisi ve reset adımı için Solà [S8d].
- **Ölçüm modelleri:**
  - Derinlik: skaler ölçüm.
  - DVL: gövde çerçevesinde hız, `v_b = Rᵀv + ω × r_dvl` (DVL'nin montaj ofseti dahil). Eksen başına skaler güncelleme. Geçerlilik bayrağı ve FOM (figure of merit) ile kapılanır.
  - Manyetometre: yalnızca heading veya 3 eksen.
  - GPS: yalnızca yüzeyde, konum ve yatay hız.
- **Sayısal yöntem:** Sıralı skaler güncelleme matris tersi gerektirmez ve `f32` ile daha dayanıklıdır. Joseph formu, simetrikleştirme ve varyans alt sınırları kullanılmalı [S7].
- **Gecikme telafisi:** PX4 gecikmeli ufuk kullanıyor [S7]. Düşük hızlı bir araçta, ölçümü zaman damgasına en yakın tahminle eşleştirmek ilk sürüm için yeterli olabilir (kendi tahminim, SIL'de ölçülmeli).

#### Alternatifler

- **Invariant EKF:** Daha iyi tutarlılık sağlar (Barrau & Bonnabel, IEEE TAC 2017; künye bu oturumda kontrol edilmedi).
- **UKF-M:** Krauss & Stilwell, DVL + derinlik + akustik menzil + yüzeyde GPS kombinasyonunda UKF-M kullanıyor [S42]. Maliyeti EKF'nin birkaç katı.
- **Mahony/Madgwick + ayrı doğrusal KF:** Çok ucuz ve sağlam, ama bias ile tutum-konum korelasyonunu modellemez. Yedek kestirici olarak değerli. Rust'ta hazır crate'ler: `ahrs` 0.8.0 ve `dcmimu` (`no_std`) [S25].

#### Yüzey ↔ dalış geçişi

Öneri; EKF3'teki kaynak değiştirme [S9] ve PX4'teki `EKF2_GPS_MODE` [S7] fikirlerinden türetildi.

1. **Kaynak kipleri:** `SURFACE_GNSS`, `SUBMERGED_DR`, `TRANSITION`. Geçişler derinlik histerezisiyle yapılır; örneğin dalış > 0,5 m, yüzey < 0,2 m. Eşikler kendi tahminim, anten yüksekliğine göre ayarlanmalı.
2. **Dalışta:**
   - GPS füzyonu kapanır ve konum kovaryansının büyümesine izin verilir.
   - DVL bottom-lock da yoksa "konum güveni düşük" bayrağı kaldırılır; görev katmanı buna tepki verir (bkz. 2.5).
3. **Yüzeye çıkışta:**
   - Fix, ancak N ardışık iyi örnekten sonra kabul edilir: 3D fix, eph eşiğin altında, derinlik eşiğin altında. Dalgalar anteni kısa süreli batırabilir.
   - Ki-kare kapısı P'den hesaplanır. Dead reckoning sonrası P büyük olduğu için ilk fix genellikle kapıdan geçer.
   - Kapıdan geçmez ama kalite sürekli iyiyse konum ve ilgili P bloğu resetlenir. `reset_counter` artırılır (MAVLink `ODOMETRY.reset_counter` bu iş için var [S14]) ve güdüm/kontrol setpoint'leri yeniden başlatılır.
   - Dalış boyunca biriken DR hatası kaydedilir. Bu, DVL ve heading kalibrasyonu için değerli bir metrik.
4. **Yüzeyde DVL:** Kabarcık ve dalga nedeniyle güvenilmez olabilir. FOM ve derinlikle kapılanmalı (kendi tahminim).

#### Cortex-M33 üzerinde maliyet

Kendi tahminim; kartta ölçülmeli.

- **İşlem sayısı:** Yoğun 15×15 kovaryans yayılımı yaklaşık 2·15³ ≈ 6750 çarp-topla (MAC) tutar; seyreklik kullanılırsa daha az. 100 Hz'te bu yaklaşık 0,7 M MAC/s eder. Skaler güncellemeler O(n²) maliyetlidir.
- **CPU yükü:** Toplam ~1 M MAC/s, 160 MHz'te ve MAC başına 3–6 çevrimle **%2–4 CPU** demek.
- **`f32` zorunlu:** STM32U5'teki M33'te yalnızca tek duyarlıklı FPU var [S26]. `f64` işlemleri yazılım emülasyonuna düşer; çevrim maliyeti doğrulanmadı.
- **Diğer otopilotlar da `f32` kullanıyor:** ArduPilot EKF3, `HAL_WITH_EKF_DOUBLE` tanımlı değilse `float` kullanır [S4]. PX4 EKF2 de `float` kullanır, yalnızca enlem/boylam `double` [S8c].
- **Çözünürlük:** Yerel NED'de 10 km uzaklıkta `f32` çözünürlüğü ≈ 10000/2²⁴ ≈ 0,6 mm. Yarışma için fazlasıyla yeterli.

### 2.3 Güdüm

- **Rota noktası:** Kabul dairesi ile birlikte LOS. Orantılı LOS için USGES kararlılık sonucu: Fossen & Pettersen 2014 [S34].
- **ILOS:** Sabit akıntı altında kalıcı çapraz hatayı sıfırlar. Teori ve LAUV dahil deneyler: Caharija vd. 2016 [S33]. Adaptif ILOS: Fossen & Lekkas 2017 [S35]. Matlab referansları: `ILOSpsi.m`, `ALOSpsi.m` [S30].
- **3B güdüm:**
  - Referanslar: `ALOS3D.m` [S30] ve Fossen & Aguiar'ın 3B ALOS çalışması (Automatica 2024; yalnızca ikincil kaynakta görüldü, doğrulanmadı) [S34b].
  - Hibrit araçta yatay LOS ile dikey derinlik setpoint'ini ayırmak pratikte daha basit.
  - Araç dümen ve derinlik dümeniyle (stern plane) dalıyorsa dikey kanal "derinlik hatası → pitch setpoint'i" şeklinde çalışır.
- **Derinlik ve irtifa tutma:**
  - Derinlik basınç sensöründen alınır. ArduSub'ın "birkaç cm" hassasiyet iddiası arşiv belgesinde geçiyor; doğrulanmadı [S9c].
  - İrtifa için sonar ya da DVL menzil ölçümü gerekir; ArduSub'ın SURFTRAK kipi bunu yapar [S9b].
  - Setpoint'ler referans modelden geçirilmeli; örnek: `refModel3` [S31].
- **İstasyon tutma:**
  - Tam itkili araçta gövde çerçevesinde konum PID'i yeterli.
  - Az itkili araçta akıntıya burun tutma (weathervaning) gerekir.
  - USV'de transit, istasyon tutma ve geri gitme kontrolcüleri arasında denetleyici anahtarlama örneği: [S44].
- **Biçerdöver taraması:**
  - Hat aralığı = kapsama genişliği × (1 − örtüşme oranı).
  - Desen rota noktası listesine çevrilir ve her hat ILOS ile izlenir.
  - Dönüşler önceden hesaplanmış yaylarla yapılır.
  - Sonar verisi yalnızca düz ve sabit hız/derinlikli bacaklarda geçerli sayılır.
  - Bu öneri genel mühendislik bilgisine dayanıyor; özel bir standart bulamadım.

### 2.4 Kontrol

**Kademeli PID yapısı** [S1][S12]:

| Kanal | Dış döngü | İç döngü | Çıkış |
|---|---|---|---|
| Heading | ψ hatası → r_sp (P, doyumlu) | r hatası → PID | yaw torku |
| Pitch / roll | açı → açısal hız setpoint'i (P) | PID | tork |
| Derinlik (itkili) | z hatası → w_sp (P) | PI + yüzerlik ileri beslemesi | dikey kuvvet |
| Derinlik (stern plane'li) | z → θ_sp (PI, sınırlı) | pitch kanalı | derinlik dümeni |
| Hız | u_sp → PI + sürükleme eğrisi ileri beslemesi | — | ileri itki |

**Anti-windup:**
- (1) Her PID kendi çıkış sınırını bilir.
- (2) Tahsis katmanı eksen bazında pozitif ve negatif doyma bayrakları döndürür; integral doyma yönünde dondurulur. Bu PX4'ün deseni [S12].
- (3) Kip geçişlerinde bumpless transfer yapılır: integral, mevcut çıkışı koruyacak şekilde başlatılır.

**İtki tahsisi:**
- Temel ilişki τ = B·u. Önce sözde ters (pseudo-inverse) uygulanır.
- Doyma olursa öncelikli normalizasyon yapılır. ArduSub dikey (roll/pitch/throttle) ve yatay (yaw/forward/lateral) grupları ayrı normalize ediyor [S13].
- İtici eğrisi tersine çevrilmeli: T ∝ n|n| olduğundan komut ≈ sign·√|T|. İleri/geri asimetrisi ve ölü bölge telafi edilmeli.
- Konunun derlemesi: Fossen & Johansen 2006 [S36].

**İleri yöntemler:**
- **LQR:** Kazançlar SIL'de offline hesaplanır; firmware'de yalnızca K·x çarpımı kalır (6×12 ≈ 72 MAC). Hıza göre gain scheduling yapılabilir. Hesaplama açısından düşük riskli, ama model doğruluğuna bağımlı.
- **SMC:** Hesaplama açısından ucuz. Chattering'e karşı boundary layer gerekir. Klasik referans: Healey & Lienard 1993, AUV dalış ve yön kontrolü [S38].
- **MPC:**
  - TinyMPC, STM32F405 (M4, 168 MHz, 192 kB SRAM) üzerinde N=15 ufukla 500 Hz'te koştu [S37]. Aynı çipte DAQP'nin daha hızlı olduğunu gösteren 2026 tarihli bir ön baskı var; doğrulanmadı [S37b].
  - M33'te küçük bir doğrusal MPC 20–50 Hz'te rahat koşmalı (kendi tahminim).
  - Ayar ve doğrulama yükü yüksek olduğu için ikinci faza bırakılmalı.

**Yüzey ↔ su altı kontrolcü geçişi:**
- Rejim farkları: yüzeyde dalga bozucusu, itici havalanması (ventilation) ve farklı sönüm var; su altında 6DOF bağlaşımı ve yüzerlik baskın.
- Öneri:
  - Her rejim için ayrı bir kazanç seti tutulmalı.
  - Geçiş, kestiricinin kaynak kipiyle aynı derinlik histerezisine bağlanmalı.
  - Geçişte bumpless transfer uygulanmalı.
- Bu senaryoya özel bir makale bulamadım. Yakın örnekler: [S44] ve AUV için bumpless çok kipli tahsis (MDPI JMSE 2026, doğrulanmadı) [S45].

### 2.5 Görev yönetimi

**FSM ve davranış ağacı (BT) karşılaştırması:**
- **BT artıları:** Modülerdir; güvenlik koşulları her tick'te ağacın tepesinde kontrol edilir [S39]. Teorik çerçeve: Colledanchise & Ögren [S40]. Biçimsel doğrulama örnekleri var (BehaVerify) [S39b].
- **BT eksileri:** Tipik kütüphaneler dinamik bellek ve çalışma anında ağaç yükleme kullanır; bu `no_std` çekirdek için ağır.
- **FSM artıları:** Rust `enum` + `match` ile derleyici her durum/olay çiftinin ele alınmasını zorunlu kılar; test etmek kolay.

**Önerilen hiyerarşi:**
1. **Güvenlik/kip FSM'i (her zaman çalışır):** `Disarmed → Armed{Manual | Stabilize | DepthHold | StationKeep | Auto | Guided} → Failsafe{Surface | Hold | Disarm}`.
2. **Görev yürütücüsü:** MAVLink mission item listesini işler: `NAV_WAYPOINT`, derinlik değişimi, tarama deseni, loiter. Özel komutlar için `MAV_CMD_USER_*` aralığı (31010–) kullanılabilir [S14]. Yürütücü yalnızca setpoint üretir; güvenlik FSM'ini atlayamaz.
3. **İleride BT:** Yer istasyonunda veya yardımcı bilgisayarda üst düzey planlayıcı olarak.

**Failsafe tablosu.** ArduSub'dan uyarlandı [S2]. Eşikler kendi tahminim ve parametre olarak tutulmalı.

| Olay | Tespit | Eylem |
|---|---|---|
| Sızıntı | Sızıntı sensörü | SURFACE + yüzeyde konum bildirimi |
| Haberleşme kaybı | Heartbeat zaman aşımı | MANUAL/GUIDED'da HOLD → SURFACE. AUTO'da su altında haberleşme kaybı beklenen bir durumdur; bunun yerine görev süresi ve geofence denetlenir. |
| Düşük batarya | Gerilim, akım ve kalan mAh; iki seviye | Önce uyarı, sonra görev iptali ve yüzeye çıkış / eve dönüş |
| Derinlik sınırı | z > z_max − pay | İniş kesilir, yükselme setpoint'i verilir; tekrarlarsa SURFACE |
| Kestirim güveni düşük | İnovasyon oranı sürekli > 1; konum σ eşik üstü; DVL kilidi yok | AUTO'dan derinlik + heading tutmaya geç; süre aşılırsa SURFACE (ArduSub'da "EKF bad" süresi 2 s [S2]) |
| Ana döngü kilitlenmesi | Döngü sayacı (2 s [S2]) + donanım IWDG | Motorlar nötr / reset |
| İç basınç veya sıcaklık | Eşik + 2 s | Uyarı / SURFACE |
| Takılma | Komut var ama hız ≈ 0 | Dur, geri çık, SURFACE |

### 2.6 Gerçek zamanlı yapı

- **Embassy:**
  - Heap gerektirmez, görevleri statik ayırır ve entegre timer kuyruğu vardır [S17].
  - Cortex-M'de thread executor `WFE/SEV` ile, interrupt executor NVIC kesmeleriyle çalışır. Birden fazla executor farklı önceliklerde koşabilir ve yüksek öncelikli olan düşük öncelikliyi keser [S17][S18].
  - Opsiyonel `scheduler-deadline` (EDF, "soft-realtime") ve `scheduler-priority` özellikleri ek yük getirir [S18].
  - `platform-std` özelliği aynı executor'ı host üzerinde çalıştırabilir [S18]. `embassy-time` için `std` ve `mock-driver` time driver'ları var [S18b].
  - Aynı executor içindeki görevler birbirini kesemez [S19].
- **Ölçümler (Tweede Golf, 2022; blog, hakemli değil; STM32F446 @ 180 MHz, osiloskopla)** [S19]:

  | Ölçüm | Embassy | FreeRTOS | RTIC |
  |---|---|---|---|
  | Ortalama kesme gecikmesi | 3,74 µs | 4,97 µs | 1,18 µs |
  | Statik bellek | 872 B | 5480 B | 392 B |

  Eleştiri: en kötü durum değil, standart sapma ölçüldü [S19].
- **RTIC:** SRP tabanlı. Statik önceliklerle kesmeli zamanlama, yarış koşulu ve kilitlenme olmamasının garantisi, tek ve isimli bir kritik bölümle sınırlı öncelik terslenmesi sunar. Statik yanıt süresi analizine uygundur [S20].
- **FreeRTOS/ThreadX:** ThreadX 6.4.1 artık MIT lisanslı ve Eclipse'te [S46]. Rust bağlamaları olgun değil; `threadx-sys` README'si M0/M3 hedeflerini listeliyor [S46b]. Rust çekirdeğiyle kullanmak FFI ve iki dünyayı birden yönetme yükü getirir.
- **Çip desteği:** `embassy-stm32` kaynağında `stm32u5a9nj` özelliği mevcut [S22b].
- **Zamanlama garantisi:** Hiçbir seçenek WCET'i (en kötü durum yürütme süresi) kendiliğinden garanti etmez. DWT çevrim sayacıyla ölçüm, bütçe izleme (ArduPilot tarzı) ve donanım watchdog'u gerekir [S6].

### 2.7 HIL protokolü

**MAVLink HIL mesajları (common.xml):**

| Mesaj | Payload | İçerik |
|---|---|---|
| `HIL_SENSOR` (107) | 65 B | İvme, gyro, manyetometre (gauss), `abs_pressure` (hPa), `diff_pressure`, `pressure_alt`, sıcaklık, `fields_updated` bit maskesi, `id` |
| `HIL_GPS` (113) | 39 B | Hız alanları int16 cm/s |
| `HIL_ACTUATOR_CONTROLS` (93) | 81 B | `float[16]` (−1..1), `mode`, `flags`; flags'te `HIL_ACTUATOR_CONTROLS_FLAGS_LOCKSTEP` var |
| `HIL_STATE_QUATERNION` (115) | 64 B | Gerçek (ground-truth) durum |

Kaynak: [S14].

**Eksikler:**
- DVL için standart bir HIL mesajı yok.
- Yakın karşılıklar: `VISION_POSITION_DELTA` (yalnızca ardupilotmega dialect'inde), `VISION_SPEED_ESTIMATE` ve 233 B'lık `ODOMETRY` [S14][S27].
- Mesafe sensörü `DISTANCE_SENSOR`'dan gelir; tür olarak `ULTRASOUND` seçilebilir. Çok ışınlı veri `OBSTACLE_DISTANCE` (72 ışın) ile taşınabilir.
- Ham sonar verisi için yalnızca `TUNNEL` (128 B payload) var; bu da belgesiz ve uzantıya özgü bir taşıyıcı [S14].
- Sızıntı ve batarya için HIL mesajı yok.
- `HIL_SENSOR` IMU, manyetometre ve barometreyi tek bir mesajda paketliyor. Bu, sensörlerin ayrı ayrı zamanlanmasıyla uyuşmuyor.

**Paket boyutu:**
- MAVLink 2'de imzasız paket ek yükü 12 B'tır (10 B başlık + 2 B CRC). İmza 13 B ekler. Payload en fazla 255 B'tır ve sondaki sıfırlar kırpılır [S14b].
- Özel dialect'ler istediği ID aralığını kullanabilir, ancak 0–255 aralığı kullanılmamalı [S14c].

**Bant genişliği hesabı** (kendi hesabım):
- 400 Hz'te `HIL_SENSOR` (77 B) + `HIL_ACTUATOR_CONTROLS` (93 B) ≈ 68 kB/s.
- 921600 baud UART ≈ 92 kB/s eder; yani bağlantının yaklaşık %74'ü yalnızca HIL'e gider.
- DK kartın VCP'si varsayılan olarak USART1, 115200 baud [S47]; bu hızda HIL mümkün değil.
- Kartta USB 2.0 HS Type-C portu var [S47]. Native USB (CDC-ACM veya bulk) önerilir. `embassy-stm32` ile U5A9'da HS USB desteği bu oturumda doğrulanmadı.

**Rust desteği:**
- `mavlink` 0.21.0'ın `embedded` özelliği `embedded-io` 0.7 ve `embedded-io-async` 0.7 kullanır. `std` ile `embedded` birbirini dışlar. İmzalama özelliği `std` gerektirir [S21].
- `embassy-stm32` 0.6.0 da `embedded-io-async` 0.7'ye bağlı, yani sürümler uyumlu [S22].
- Özel dialect'ler `mavlink-bindgen` ile üretilebilir [S21].

**Özel protokol seçeneği:**
- `postcard` 1.x'in tel formatı kararlı ve belgeli; `no_std` için tasarlanmış. `to_slice_cobs` ve `from_bytes_cobs` fonksiyonları var [S23].
- `postcard-rpc` (0.12.1), PC ve MCU arasında istek/yanıt ve konu (topic) desteği sunar. Anahtar, yol ve şemanın 64 bit FNV1a özetidir; bu sayede şema uyuşmazlığı yakalanır. COBS ile dağıtım yapılabilir [S24].

**QGroundControl:**
- `MAV_AUTOPILOT_GENERIC` (0) ve `MAV_TYPE_SUBMARINE` (12) değerleri mevcut [S14d].
- QGC'nin genel (generic) bir otopilotta tam olarak neyi beklediği belgelenmemiş. Topluluk deneyimine göre heartbeat, `AUTOPILOT_VERSION` yanıtı, parametre protokolü ve görev protokolü gerekiyor; doğrulanmadı [S48].
- Görev protokolü: `MISSION_COUNT` → `MISSION_REQUEST_INT` → `MISSION_ITEM_INT` → `MISSION_ACK`. Zaman aşımı ve yeniden deneme kuralları belgede tanımlı [S49].

**Değerlendirme:** MAVLink'in değeri yer istasyonu tarafındadır: QGC haritası, görev planlama ve hazır araçlar. Simülasyon bağlantısında ise asıl gereken şey firmware sürücüleriyle birebir aynı tipte veridir; bunu en iyi tip güvenli, paylaşılan bir Rust şeması sağlar.

### 2.8 SIL yapısı

- **Kanıtlanmış desenler:**
  - ArduPilot: `AP_HAL` ile aynı kod SITL'de Linux/macOS üzerinde derlenir [S1][S15].
  - PX4: POSIX hedefi + simülatör modülü [S10].
- **Rust karşılığı:**
  - `nav-core`: `no_std` kütüphane; kestirim, güdüm, kontrol, tahsis ve FSM.
  - `proto`: paylaşılan serde tipleri.
  - `fw`: Embassy binary.
  - `sim-host`: std binary.
- **HAL sınırı:** "Zaman damgalı, SI birimli ve çerçevesi tanımlı sensör örneği" seviyesinde olmalı. Sürücüler bu tipi üretir; HIL'deki `SimLink` sürücüsü de aynı trait'leri uygular. Böylece firmware'in simülasyondan haberdar olan tek parçası sürücü katmanı olur.
- **Cargo dikkat noktası:** `no_std` ve `std` crate'lerini aynı derlemede birleştirmek feature unification sorunları çıkarır; ayrı workspace'ler önerilir (bkz. rapor 03).

### 2.9 Rust ekosistemi

| Crate | Sürüm / not | Kaynak |
|---|---|---|
| `nalgebra` | 0.35.0; `default-features=false` + `libm` ile `no_std`; statik `SMatrix` | [S25] |
| `micromath` | 2.1.0; hızlı ama yaklaşık fonksiyonlar | [S25] |
| `libm` | 0.2.16 | [S25] |
| `adskalman` | 0.18.0; `no_std` KF + RTS | [S25][S25b] |
| `kfilter` | 0.5.1; `no_std` KF/EKF | [S25] |
| `eskf` | 0.2.0; 2021'den beri güncellenmiyor | [S25] |
| `ahrs` | 0.8.0 | [S25] |
| `dcmimu` | 0.3.0 | [S25] |

**Açık kaynak Rust projeleri:**
- **Holsatus Flight:** Embassy tabanlı, RP2040 üzerinde 1 kHz kontrol döngüsü; MAVLink sunucusu ve interrupt executor'lar kullanıyor. Proje refactor aşamasında; kestirici türü belirtilmemiş [S32].
- **rust-gnc:** EKF henüz yol haritasında.
- Olgun bir Rust otopilot bulamadım. EKF'nin kendimiz yazılması ve MSS çıktılarıyla doğrulanması gerekecek.

### 2.10 Test stratejisi

- **Birim:** Host'ta `cargo test`. `proptest` ile özellik testleri: quaternion normunun korunması, tahsis çıktısının sınırlar içinde kalması, açı sarma fonksiyonu.
- **Altın testler:** MSS (`ins_mekf`, `ILOSpsi`) ve PythonVehicleSimulator çıktılarına karşı toleranslı karşılaştırma [S30][S31].
- **SIL senaryoları:** ArduSub autotest örnek alınır [S15]. Senaryolar: akıntı, DVL kesintisi, GPS gecikmesi, sızıntı, haberleşme kaybı. Monte Carlo koşuları ve geçti/kaldı metrikleri tanımlanır.
- **Kartta:** `defmt-test` 0.5.1 [S25]; DWT ile WCET ölçümleri.
- **HIL regresyonu:** Self-hosted runner, probe-rs ile yükleme, gerçek zamanlı senaryo koşusu. HIL kayıtlarının SIL'de yeniden oynatılması.
- **Ayar:**
  1. Önce SIL'de sistem tanılama (basamak cevapları).
  2. Sonra maliyet fonksiyonu (IAE, aşım, aktüatör eforu) üzerinde otomatik arama: grid, Nelder-Mead veya Bayes optimizasyonu.
  3. Sonuç HIL'de doğrulanır.
  - Sim-gerçek farkı nedeniyle kazanç marjı bırakılmalı (kendi tahminim).

---

## 3. Mimari için öneriler

### 3.1 Katman yapısı

```
nav-core (no_std, no alloc, no async; f32)
  types/      SensorSample<T>{t_us, frame, data}, Setpoint, ActuatorCmd
  estimator/  ESKF15 (+opsiyonel durumlar), kaynak kipi, sağlık metrikleri
  guidance/   LOS/ILOS, waypoint yöneticisi, biçerdöver üretici, referans modeli
  control/    PID (anti-windup + bumpless), kademeler, kazanç setleri (yüzey/su altı)
  alloc/      B matrisi, pseudo-inverse + öncelikli normalizasyon, itici eğrisi
  modes/      güvenlik FSM'i, failsafe değerlendiricisi, görev yürütücüsü
proto (no_std + std): postcard tipleri (sim bağlantısı), MAVLink dialect (GCS)
hal-traits:  ImuSource, PressureSource, DvlSource, GnssSource, ActuatorSink, Clock
fw (Embassy): gerçek sürücüler | SimLink sürücüleri (cargo feature)
sim-host (std): fizik + sensör modelleri + lockstep koordinatörü
```

Gerekçeler:
- Çekirdek saf fonksiyonlardan oluşursa SIL deterministik olur.
- Firmware'de simülasyondan haberdar olan tek yer sürücü katmanıdır (proje gereksinimi).
- Paylaşılan tipler sayesinde HIL ve SIL aynı veriyi kullanır.

### 3.2 Döngü frekansları

İlk sürüm için öneri; ArduSub [S2] ve PX4 [S8b] değerlerine göre ölçeklendi.

| İş | Frekans | Executor önceliği |
|---|---|---|
| IMU örneği (tetik) | 200 Hz (gerçek donanımda 400 Hz'e çıkılabilir) | Yüksek |
| ESKF tahmini | 100 Hz | Yüksek |
| Ölçüm güncellemeleri | Derinlik 20–50, DVL 4–15, manyetometre 10–50, GPS 1–10 Hz (geldikçe) | Yüksek |
| Açısal hız kontrolü + tahsis | 200 Hz | Yüksek |
| Açı, derinlik ve hız döngüleri | 50 Hz | Yüksek |
| Güdüm (LOS) | 10 Hz | Orta |
| Görev ve FSM | 10 Hz | Orta |
| Failsafe değerlendirmesi | 10–50 Hz; donanım watchdog'u ayrı | Orta |
| Telemetri (MAVLink) | 1–10 Hz (akış başına) | Düşük |
| Kayıt | Asenkron | Düşük |

Not: Su altı araçları yavaştır. 200 Hz'lik iç döngü, HIL bant genişliğini rahatlatır.

### 3.3 Protokol seçimi

- **Simülasyon bağlantısı:**
  - Teknoloji: `postcard` + COBS (`postcard-rpc` değerlendirilmeli), native USB üzerinden.
  - Mesajlar: her sensör için ayrı çerçeve (`ImuFrame`, `PressureFrame`, `DvlFrame`, `GnssFrame`, `MagFrame`, `SonarFrame`, `LeakFrame`, `BatteryFrame`), aktüatör tarafında `ActuatorFrame{seq, t_us, cmds}`, ayrıca zaman senkronizasyonu ve gecikme ölçümü.
  - Lockstep: simülatör, `ImuFrame` sıra numarasıyla eşleşen `ActuatorFrame`'i bekler (PX4 deseni [S10]).
- **GCS düzlemi:** MAVLink 2 (`rust-mavlink`, `embedded` özelliği):
  - Heartbeat: GENERIC + SUBMARINE.
  - Telemetri: `SYS_STATUS`, `ATTITUDE`, `GLOBAL_POSITION_INT`, `LOCAL_POSITION_NED`.
  - Protokoller: parametre, görev, `COMMAND_LONG`, `STATUSTEXT`.
  - Gerekirse özel dialect.
  - QGC uyumu prototiple test edilmeli.
- **Alternatif senaryo:** Simülatör ekibi PX4 uyumlu Gazebo eklentileri kullanmak isterse simülasyon bağlantısı için de MAVLink HIL seçilebilir. Bu durumda DVL/sonar için özel dialect mesajları gerekir (bkz. rapor 02).

### 3.4 Gerçek zamanlı çatı

- Embassy kullanılır: iki `InterruptExecutor` (yüksek ve orta öncelik) + bir thread executor.
- Çekirdek senkron çağrılır; `async` yalnızca I/O katmanında kalır.
- DWT ile her döngünün süresi ölçülür ve bütçe ihlalleri telemetriye yansıtılır.
- IWDG etkin olmalı.

### 3.5 Kestirici

- ESKF15 + yedek AHRS, `f32`. Yerel NED kullanılır; orijin `f64`.
- Kaynak kipi FSM'i ve sağlık metrikleri (inovasyon oranları, σ) görev katmanına yayınlanır.
- Geliştirme sırası: MSS ile doğrulanmış Rust portu → SIL → HIL.

---

## 4. Riskler ve açık sorular

1. **Araç geometrisi belirsiz.** İtici düzeni ve stern plane olup olmadığı belli değil. Bu bilgi kontrol ve tahsis tasarımını belirliyor.
2. **Sensör seti belirsiz.** DVL bütçesi var mı? DVL yoksa su altı konumu yalnızca IMU + model ile kestirilir ve hızla bozulur.
3. **HIL bağlantısı:** `embassy-usb` ile U5A9'da HS USB desteği ve USB gecikme jitter'ı ölçülmeli.
4. **QGC'nin genel otopilot davranışı belirsiz;** prototiple denenmeli [S48].
5. **Manyetik parazit:** İticiler ve batarya akımı heading hatası yaratır. Akıma bağlı kompanzasyon gerekebilir.
6. **Embassy WCET:** Garanti yok; ölçümle yönetilmeli. Gerekirse RTIC'e geçiş seçeneği açık tutulmalı.
7. **Lockstep ile gerçek zaman çatışması:** Simülatör gerçek zamanın gerisine düşerse firmware'deki zaman aşımları tetiklenir.
8. **Sim-gerçek farkı:** Hidrodinamik katsayılar bilinmiyor. Gerçek araçla sistem tanılama yapılmalı.
9. **Kart değişimi:** Gelecekteki kartta çift duyarlıklı FPU olup olmayacağı ve HAL taşınabilirliği belirsiz.
10. **Torpido arayüzü:** Bu rapor kapsamında değil. Ayrı araştırmanın güvenlik gereksinimleriyle birlikte belirlenmeli.

---

## 5. Bağımsız doğrulanması gereken kritik iddialar

1. ArduSub'ın ana döngüsü 400 Hz'tir ve rate controller ile motor çıkışı `FAST_TASK` olarak her döngüde çalışır. [S2]
2. ArduPilot EKF3 24 durumludur, `EKF_TARGET_DT` 12 ms'dir ve `HAL_WITH_EKF_DOUBLE` tanımlı değilse `float` kullanır. [S3][S4]
3. PX4 EKF2 hata durumlu bir formülasyondur (Solà türevi) ve `EKF2_PREDICT_US` varsayılanı 10000 µs'dir. [S7][S8][S8b]
4. PX4'ün UUV desteği resmi olarak "experimental" etiketlidir. [S5]
5. PX4 SITL lockstep'inde simülatör `HIL_SENSOR` gönderir ve `HIL_ACTUATOR_CONTROLS` gelene kadar bekler; HITL'de lockstep yoktur (ikinci kısım forum bilgisi). [S10][S16]
6. `mavlink` 0.21'in `embedded` özelliği `embedded-io` ve `embedded-io-async` 0.7 kullanır, `std` ile birbirini dışlar ve imzalama `std` gerektirir. `embassy-stm32` 0.6.0 da `embedded-io-async` 0.7'ye bağlıdır. [S21][S22]
7. MAVLink'te standart bir DVL HIL mesajı yoktur. `VISION_POSITION_DELTA` (11011) yalnızca ardupilotmega dialect'indedir ve ArduSub DVL entegrasyonu bunu kullanır. [S14][S27][S28]
8. STM32U5'teki Cortex-M33'te yalnızca tek duyarlıklı FPU vardır. [S26]
9. TinyMPC, STM32F405 üzerinde N=15 ufukla 500 Hz'te çalıştırılmıştır. [S37]
10. PX4 rate controller, tahsis katmanından gelen doyma bayraklarıyla integrali dondurur. [S11][S12]

---

## 6. Kaynaklar

- [S1] ArduPilot, Copter attitude control: https://ardupilot.org/dev/docs/apmcopter-programming-attitude-control-2.html
- [S2] ArduSub kaynak: https://github.com/ArduPilot/ardupilot/blob/master/ArduSub/Sub.cpp , .../ArduSub/config.h , .../ArduSub/failsafe.cpp
- [S3] EKF3 core: https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_NavEKF3/AP_NavEKF3_core.h
- [S4] ftype: https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_Math/ftype.h
- [S5] PX4 UUV: https://docs.px4.io/main/en/frames_sub/
- [S5b] https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/uuv_att_control/uuv_att_control.cpp
- [S6] ArduPilot threading: https://ardupilot.org/dev/docs/learning-ardupilot-threading.html
- [S7] PX4 EKF tuning: https://docs.px4.io/main/en/advanced_config/tuning_the_ecl_ekf
- [S8] EKF2 derivation: https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/ekf2/EKF/python/ekf_derivation/derivation.py
- [S8b] https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/ekf2/module.yaml
- [S8c] https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/ekf2/EKF/common.h
- [S8d] Solà, arXiv:1711.02508: https://arxiv.org/abs/1711.02508
- [S9] ArduPilot EKF overview: https://ardupilot.org/copter/docs/common-apm-navigation-extended-kalman-filter-overview.html
- [S9b] ArduSub modes: https://ardupilot.org/sub/docs/modes.html
- [S9c] https://www.ardusub.com/introduction/features.html
- [S10] PX4 simulation: https://docs.px4.io/main/en/simulation/ ; https://docs.px4.io/main/en/sim_gazebo_classic/
- [S10b] https://docs.px4.io/main/en/simulation/
- [S10c] https://docs.px4.io/main/en/simulation/hitl
- [S11] https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/control_allocator/ControlAllocator.cpp
- [S12] https://github.com/PX4/PX4-Autopilot/blob/main/src/lib/rate_control/rate_control.cpp
- [S13] https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_Motors/AP_Motors6DOF.cpp
- [S14] MAVLink common.xml: https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml ; https://mavlink.io/en/messages/common.html
- [S14b] https://mavlink.io/en/guide/serialization.html
- [S14c] https://mavlink.io/en/guide/define_xml_element.html
- [S14d] https://mavlink.io/en/messages/minimal.html
- [S15] https://github.com/ArduPilot/ardupilot/blob/master/Tools/autotest/ardusub.py
- [S16] https://discuss.px4.io/t/slow-simulator-does-not-work-in-sitl/47188 ; https://www.mathworks.com/matlabcentral/answers/2117081-px4-hitl-simulation-timing-synch-lockstep-issue
- [S17] https://docs.embassy.dev/embassy-executor/git/cortex-m/index.html
- [S18] https://github.com/embassy-rs/embassy/blob/main/embassy-executor/Cargo.toml
- [S18b] https://github.com/embassy-rs/embassy/blob/main/embassy-time/Cargo.toml
- [S19] https://tweedegolf.nl/en/blog/65/async-rust-vs-rtos-showdown/
- [S20] https://rtic.rs/2/book/en/
- [S21] https://github.com/mavlink/rust-mavlink ; https://docs.rs/crate/mavlink/latest/features ; mavlink-core/Cargo.toml
- [S22] https://crates.io/crates/embassy-stm32 (0.6.0 bağımlılıkları)
- [S22b] https://github.com/embassy-rs/embassy/blob/main/embassy-stm32/Cargo.toml
- [S23] https://docs.rs/postcard/latest/postcard/
- [S24] https://github.com/jamesmunns/postcard-rpc ; docs/overview.md
- [S25] crates.io API: nalgebra, micromath, libm, adskalman, kfilter, eskf, ahrs, dcmimu, defmt-test
- [S25b] https://github.com/strawlab/adskalman-rs
- [S26] https://www.st.com/en/microcontrollers-microprocessors/stm32u535ce.html
- [S27] https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/ardupilotmega.xml
- [S28] https://discuss.bluerobotics.com/t/pixhawk-not-sending-local-position-depite-feeded-by-vision-speed-estimate-message/17704 ; https://waterlinked.github.io/dvl/bluerov-integration-dvl-ugps
- [S29] https://discuss.ardupilot.org/t/is-hil-mode-remove-form-v4-1-5/85213
- [S30] Fossen MSS: https://github.com/cybergalactic/MSS
- [S31] https://github.com/cybergalactic/PythonVehicleSimulator
- [S32] https://github.com/holsatus/holsatus-flight
- [S33] Caharija vd. 2016, doi:10.1109/TCST.2015.2504838 ; https://ntnuopen.ntnu.no/ntnu-xmlui/handle/11250/2420689
- [S34] Fossen & Pettersen 2014, Automatica 50(11) ; https://ntnuopen.ntnu.no/ntnu-xmlui/handle/11250/280198
- [S34b] https://arxiv.org/html/2505.08344v1
- [S35] Fossen & Lekkas 2017, doi:10.1002/acs.2550
- [S36] Fossen & Johansen 2006, doi:10.1109/MED.2006.328749
- [S37] TinyMPC: https://arxiv.org/abs/2310.16985
- [S37b] https://arxiv.org/html/2603.09342
- [S38] Healey & Lienard 1993, doi:10.1109/JOE.1993.236372
- [S39] Sprague vd.: https://arxiv.org/abs/1811.00426
- [S39b] https://arxiv.org/abs/2402.04045
- [S40] Colledanchise & Ögren: https://arxiv.org/abs/1709.00084
- [S41] Paull vd. 2014, doi:10.1109/JOE.2013.2278891
- [S42] Krauss & Stilwell: https://arxiv.org/abs/2210.06510
- [S44] Bertaska & von Ellenrieder: https://arxiv.org/abs/1702.04940
- [S45] MDPI JMSE 2026, doi:10.3390/jmse14050516
- [S46] https://techcommunity.microsoft.com/blog/iotblog/azure-rtos-transition-to-open-source-is-now-complete/4105027
- [S46b] https://docs.rs/crate/threadx-sys/0.1.2/source/README.md
- [S47] https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html ; https://www.st.com/en/evaluation-tools/stm32u5a9j-dk.html
- [S48] https://discuss.px4.io/t/custome-autopilo-can-not-takeofft-with-qgroundcontrol-and-mavlink-v2/12105
- [S49] https://mavlink.io/en/services/mission.html
