# Codex doğrulaması: 06-otopilot

## Hüküm tablosu

Raporun tamamı okundu. Bölüm 5’teki on iddia canlı aramayla araştırıldı ve birincil kaynaklar açılarak kontrol edildi. Hiçbir dosya değiştirilmedi, kurulum veya derleme yapılmadı. Aşağıdaki ilk on satır, raporun Bölüm 5 numaralarıyla eşleşir.

| # | İddia (kısa) | Hüküm | Kanıt (URL) | Not |
|---|---|---|---|---|
| 1 | ArduSub 400 Hz; rate controller ve motor çıkışı `FAST_TASK` | DOĞRU | [config.h](https://github.com/ArduPilot/ardupilot/blob/master/ArduSub/config.h), [Sub.cpp](https://github.com/ArduPilot/ardupilot/blob/master/ArduSub/Sub.cpp) | 400 Hz varsayılanı doğrulanıyor. Görevin çağrılması, MANUAL gibi kiplerde PID hesabının da çalışacağı anlamına gelmiyor. Ham IMU örnekleme frekansıyla karıştırılmamalı. |
| 2 | EKF3: 24 durum, 12 ms hedef adım, varsayılan `float` | DOĞRU | [EKF3 çekirdeği](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_NavEKF3/AP_NavEKF3_core.h), [ftype.h](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_Math/ftype.h) | `EKF_TARGET_DT=0.012f`; `HAL_WITH_EKF_DOUBLE` doğru değilse `ftype=float`. Hedef adım, her çağrının kesin süresi değildir. |
| 3 | PX4 EKF2 hata durumlu, Solà referanslı; tahmin varsayılanı 10000 µs | DOĞRU | [Türetme kodu](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/ekf2/EKF/python/ekf_derivation/derivation.py), [module.yaml](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/ekf2/module.yaml) | Kaynak, hata durumu ve tangent-space yaklaşımını açıkça kullanıyor. |
| 4 | PX4 UUV desteği deneysel | DOĞRU | [Resmî UUV belgesi](https://docs.px4.io/main/en/frames_sub/) | BlueROV2 Heavy için yardımcı kontrol kipleri var; otonom su altı görevleri sınırlı. Hibrit araç desteği anlamına gelmiyor. |
| 5 | PX4 SITL lockstep; HITL lockstep değil | DOĞRU | [Gazebo Classic lockstep](https://docs.px4.io/main/en/sim_gazebo_classic/#lockstep), [Gazebo bağlantı kodu](https://github.com/PX4/PX4-SITL_gazebo-classic/blob/main/src/gazebo_mavlink_interface.cpp) | Standart Gazebo Classic yolu için doğrulandı: lockstep yalnızca `!hil_mode_` koşulunda açılıyor. Bütün olası özel HITL uygulamalarına genellenemez. |
| 6 | `mavlink` 0.21 embedded/no_std ve Embassy 0.6 bağımlılık uyumu | DOĞRU | [mavlink kaynak](https://github.com/mavlink/rust-mavlink/blob/master/mavlink/src/lib.rs), [mavlink-core bağımlılıkları](https://docs.rs/crate/mavlink-core/latest), [Embassy bağımlılıkları](https://docs.rs/crate/embassy-stm32/latest) | `std`–`embedded` birlikte seçimi derleme hatasıdır; imzalama `std` ister. İki tarafta `embedded-io(-async)` 0.7 ailesi mevcut. Kart üzerinde derlenip çalıştırılmadı. |
| 7 | Özel DVL HIL mesajı yok; `VISION_POSITION_DELTA` ardupilotmega’da ve DVL entegrasyonunda kullanılıyor | DOĞRU | [common.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml), [ardupilotmega.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/ardupilotmega.xml), [Üreticinin entegrasyon belgesi](https://github.com/waterlinked/docs/blob/master/docs/dvl/bluerov-integration-dvl-ugps.md) | Mesajın tanımı ardupilotmega’dadır; onu içeren dialect’lerde de bulunabilir. Bu sonuç, common ile DVL hızının taşınamayacağı anlamına gelmez. |
| 8 | STM32U5A9’un FPU’su yalnızca tek duyarlıklı | DOĞRU | [ST: STM32U5A9NJ](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html) | Hedef çipin kendi ürün sayfasıyla doğrulandı; rapordaki farklı U5 modeline dayanmak gerekmiyor. |
| 9 | TinyMPC: STM32F405, N=15, 500 Hz | DOĞRU | [Makale, IV-B](https://arxiv.org/html/2310.16985v2) | Yörünge izleme ve tutum toparlama deneyleri için doğru. Engel kaçınmada 100 Hz ve N=20 kullanılmış. |
| 10 | PX4 tahsis doyumuyla integrali engelliyor | DOĞRU | [Allocator](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/control_allocator/ControlAllocator.cpp), [Rate controller bağlantısı](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/mc_rate_control/MulticopterRateControl.cpp), [İntegral hesabı](https://github.com/PX4/PX4-Autopilot/blob/main/src/lib/rate_control/rate_control.cpp) | Doyumu artıran yöndeki integrasyon engelleniyor; ters yönde integralin çözülmesine izin veriliyor. |
| 11 | MAVLink’in DVL, sonar, sızıntı ve batarya eksikleri özel protokolü gerektiriyor | KISMEN DOĞRU | [common.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml) | Tam sensör modellerinde eksikler var; fakat ilgili standart taşıma ve durum alanları da var. Özel protokol zorunlu sonuç değildir. |
| 12 | Ham sonar için yalnızca `TUNNEL` var | YANLIŞ | [Görüntü aktarım protokolü](https://mavlink.io/en/services/image_transmission.html) | Genel görüntü aktarımı için `DATA_TRANSMISSION_HANDSHAKE` ve `ENCAPSULATED_DATA` da var. Bunlar sonar için kapsamlı bir veri standardı değildir. |
| 13 | `HIL_SENSOR`, sensörleri ayrı zamanlamayla bağdaşmayan biçimde paketliyor | YANLIŞ | [Mesaj alanları](https://github.com/mavlink/c_library_v2/blob/master/common/mavlink_msg_hil_sensor.h), [PX4 alıcısı](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/mavlink/mavlink_receiver.cpp) | `fields_updated` farklı sensör gruplarının farklı hızlarda güncellenmesini sağlar. |
| 14 | 921600 baud bağlantının %74’ü HIL tarafından tüketilir | KISMEN DOĞRU | [Mesaj tanımları](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml) | Toplam 68 kB/s hesabı doğru; tam çift yönlü UART doluluğu yön başına hesaplanmalı. %74, belirli lockstep koşullarında seri aktarımın dönem içindeki payına karşılık gelebilir. |
| 15 | ESKF15 için toplam %2–4 CPU | KISMEN DOĞRU | [ST işlemci bilgisi](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html), [ESKF denklemleri](https://arxiv.org/pdf/1711.02508) | Optimize uygulama için makul hedef; bütün filtreyi kapsayan doğrulanmış bütçe değil. Bağımsız hesap aşağıda. |
| 16 | Tek duyarlıklı FPU nedeniyle `f32` zorunlu | YANLIŞ | [ST ürün bilgisi](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html) | Donanımsal `f64` aritmetiği yok; yazılımla `f64` kullanılabilir. `f32` güçlü bir performans tercihi, dilsel/donanımsal zorunluluk değil. |
| 17 | 10 km’de `f32` çözünürlüğü yaklaşık 0,6 mm | YANLIŞ | [Postcard’ın IEEE 754 gösterim açıklaması](https://postcard.jamesmunns.com/wire-format) | Bağımsız hesap: 10000 çevresindeki ardışık binary32 değerler arası mesafe **0,9765625 mm**. Mimari sonucu değiştirmeyen sayısal hata. |
| 18 | IMU–aktüatör el sıkışması, saat değiştirmeden PX4 ile aynı lockstep’i sağlar | KISMEN DOĞRU | [PX4 lockstep sırası](https://docs.px4.io/main/en/sim_gazebo_classic/#lockstep) | Kontrol adımlarını eşleyebilir; tüm zaman aşımı ve görev zamanlamasını simülasyon zamanına bağlamaz. PX4 SITL ayrıca saatini günceller. |
| 19 | `postcard + COBS`: no_std, kararlı format, paylaşılan tipler | DOĞRU | [Postcard](https://docs.rs/postcard/latest/postcard/), [COBS işlevi](https://docs.rs/postcard/latest/postcard/fn.to_slice_cobs.html), [Format şartnamesi](https://postcard.jamesmunns.com/wire-format) | Özellikler doğru. Kararlı format, değiştirilen uygulama şemalarının otomatik uyumluluğu değildir. |
| 20 | DVL eklentisi `VISION_SPEED_ESTIMATE` kullanmıyor | YANLIŞ | [DVL eklentisi](https://github.com/bluerobotics/BlueOS-Water-Linked-DVL/blob/master/dvl-a50/dvl.py) | Varsayılan `POSITION_DELTA`; seçilebilir `SPEED_ESTIMATE` yolu da mevcut. |
| 21 | no_std/std ayrımı için ayrı workspace önerisi | KISMEN DOĞRU | [Cargo resolver](https://doc.rust-lang.org/cargo/reference/resolver.html#features) | Birleştirme riski gerçek; ayrı workspace zorunlu değil. Aynı workspace içinde ayrı Cargo çağrıları kullanılabilir. |
| 22 | Klasik ArduPilot HIL kaldırılmasının nedeni USB’nin EKF’yi besleyememesi | DOĞRULANAMADI | [Raporun forum kaynağı](https://discuss.ardupilot.org/t/is-hil-mode-remove-form-v4-1-5/85213), [Güncel donanımda simülasyon belgesi](https://ardupilot.org/dev/docs/sim-on-hardware.html) | Forum 429 hatası verdi. Güncel belge bu tarihsel neden iddiasını kanıtlamıyor. |
| 23 | ArduSub sızıntıda doğrudan SURFACE’a geçer | KISMEN DOĞRU | [failsafe.cpp](https://github.com/ArduPilot/ardupilot/blob/master/ArduSub/failsafe.cpp) | SURFACE eylemi seçilmişse ve motorlar armed ise. Devre dışı ve uyarı seçenekleri ayrılmalı. |
| 24 | MSS’te 15 hata durumlu MEKF referansı var | DOĞRU | [ins_mekf.m](https://github.com/cybergalactic/MSS/blob/master/INS/functions/ins_mekf.m) | 15×15 hata kovaryansı açıkça tanımlanmış; nominal quaternion durumu farklı boyuttadır. |
| 25 | Embassy’de farklı executor öncelikleriyle kesmeli çalışma mümkün | DOĞRU | [Resmî executor belgesi](https://docs.embassy.dev/embassy-executor/git/cortex-m/index.html) | Tek executor içindeki önceliklendirme ile executor’lar arası kesme ayrımı korunmalı. |
| 26 | MAVLink kullanmak QGC uyumunu sağlar | KISMEN DOĞRU | [QGC eklenti mimarisi](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/firmware_plugin.html) | Temel haberleşmeyi sağlar; araç kurulumu, kipler ve parametre anlamları firmware’e özgüdür. Raporun prototiple doğrulama önerisi yerinde. |

## Yanlış ya da yanıltıcı bulunanlar

**1. MAVLink eksikleri olduğundan daha geniş gösterilmiş.**

Raporda özel protokolün gerekçesi, DVL/sonar/sızıntı/batarya karşılıklarının bulunmaması veya eksikliği. Doğru ayrım şudur: **mesajın varlığı, sensörün bütün anlamlarını taşıması ve firmware’in onu giriş olarak kabul etmesi üç ayrı konudur.**

- **DVL:** Common içinde özel bir DVL/HIL_DVL mesajı yok. Bununla birlikte `VISION_SPEED_ESTIMATE` hız taşıyabilir; `ODOMETRY` üç boyutlu hız, koordinat çerçevesi ve kovaryans sağlar. Bunlar ışın başına hız, bottom/water-track ayrımı ve bütün DVL kalite alanlarını standartlaştırmaz. [ODOMETRY tanımı](https://github.com/mavlink/c_library_v2/blob/master/common/mavlink_msg_odometry.h), [common.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml).
- **`OPTICAL_FLOW`:** Açısal hareketi giderilmiş iki yatay hız bileşeni ve taban mesafesi içerir. DVL’nin bazı çıktıları taşınabilir; ancak bunu tam bir DVL mesajı saymak da yanlıştır. `OPTICAL_FLOW_RAD` ve `HIL_OPTICAL_FLOW` esasen entegre açısal akış semantiğindedir. [OPTICAL_FLOW alanları](https://github.com/mavlink/c_library_v2/blob/master/common/mavlink_msg_optical_flow.h), [common.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml).
- **Sonar menzili:** `DISTANCE_SENSOR` ultrasonik menzil için uygundur; `OBSTACLE_DISTANCE` 72 mesafe örneği taşıyabilir. Bunlar yoğunluk görüntüsü veya ham akustik örnek dizisi değildir. [DISTANCE_SENSOR](https://github.com/mavlink/c_library_v2/blob/master/common/mavlink_msg_distance_sensor.h), [common.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml).
- **Sızıntı:** Ayrı bir ham sızıntı sensörü/HIL mesajı yok; fakat common’da `MAV_SYS_STATUS_SENSOR_LEAK` var. `SYS_STATUS` uzantıları üzerinden alt sistemin varlığı, etkinliği ve sağlığı bildirilebilir. Bu, zaman damgalı ham sensör enjeksiyonuyla eşdeğer değildir. [common.xml](https://github.com/mavlink/mavlink/blob/master/message_definitions/v1.0/common.xml).
- **Batarya:** `BATTERY_STATUS` gerilim, akım, tüketim ve kalan enerji gibi alanları zaten taşır. Özel `HIL_BATTERY` olmaması, batarya verisinin protokolde bulunmadığını göstermez. Mevcut firmware’in bu mesajı simülasyon girdisi olarak işlemesi ayrıca uygulanmalıdır. [BATTERY_STATUS](https://github.com/mavlink/c_library_v2/blob/master/common/mavlink_msg_battery_status.h).

Dolayısıyla özel protokol seçilebilir; ancak MAVLink’in bu verileri genel olarak taşıyamadığı gerekçesiyle seçilmemeli.

**2. “Ham sonar için yalnızca TUNNEL” yanlış.**

Raporda yalnızca `TUNNEL` bulunduğu yazıyor. Common’daki `DATA_TRANSMISSION_HANDSHAKE` ve `ENCAPSULATED_DATA`, genel görüntüleri parçalayarak taşıyabilir. Sonar görüntüsü uygun bir görüntü gösterimine dönüştürülerek bu kanaldan aktarılabilir.

Bunun sınırı da açık: resmî belge bu eski protokolü genel görüntü aktarımı için önermiyor. Ayrıca sonar geometrisi, menzil hücreleri ve akustik kalibrasyon için kapsamlı bir standart sağlamıyor. **Aktarım imkânı var; hazır sonar görüntüleme standardı yok.** [Resmî görüntü aktarım belgesi](https://mavlink.io/en/services/image_transmission.html).

**3. `HIL_SENSOR` ayrı sensör hızlarına engel değil.**

Raporda birleşik mesajın sensörlerin ayrı zamanlanmasıyla uyuşmadığı söyleniyor. `fields_updated` tam olarak hangi alanların yenilendiğini belirtir. Örneğin IMU 200 Hz, manyetometre 25 Hz, basınç 20 Hz güncellenebilir. PX4 alıcısı da ivmeölçer, gyro, manyetometre ve basıncı bu maskelerle ayrı işler.

Gerçek sınırlama, aynı paketteki güncellemelerin tek ortak zaman damgası kullanması ve değişmeyen alanların da paket içinde yer kaplamasıdır. Farklı örnekleme zamanları için ayrı mesajlar gönderilebilir. [HIL_SENSOR](https://github.com/mavlink/c_library_v2/blob/master/common/mavlink_msg_hil_sensor.h), [PX4 alıcı uygulaması](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/mavlink/mavlink_receiver.cpp).

**4. UART hesabında bant genişliği ile lockstep gecikmesi karışmış.**

Raporun paket boyutlarını kabul ederek, imzasız MAVLink 2 ve 8N1 için bağımsız hesap:

| Büyüklük | Sonuç |
|---|---:|
| PC → kart: 77 B × 400 Hz | 30.800 B/s |
| Kart → PC: 93 B × 400 Hz | 37.200 B/s |
| 921600 baud’da yön başına kapasite | 92.160 B/s |
| Yön başına doluluk | %33,4 ve %40,4 |
| Ardışık istek + cevap aktarım süresi | 1,845 ms |

Tam çift yönlü UART’ın doluluğunu iki yönü toplayarak %74 diye vermek yanıltıcıdır. **Ancak** her sensör paketinden sonra cevabı bekleyen, adımların örtüşmediği lockstep’te bu iki aktarım ardışıktır: 2,5 ms’lik dönemin yaklaşık %74’ü gerçekten yalnızca kablodaki aktarımda geçer. Hesaplama ve PC/USB gecikmesi için yaklaşık 0,655 ms kalır. Native USB önerisi bu gecikme gerekçesiyle hâlâ güçlüdür.

115200 baud’un yetersizliği doğru. Fakat kaynakta belirtilen 115200, **Zephyr konsolunun varsayılan ayarıdır**; kartın değiştirilemez VCP hız sınırı değildir. [Kartın seri port belgesi](https://docs.zephyrproject.org/latest/boards/st/stm32u5a9j_dk/doc/index.html#serial-port).

**5. ESKF için %2–4 CPU: makul hedef, eksik bütçe.**

Raporda yoğun kovaryans yayılımından toplam filtre yüküne hızlı geçiliyor. Aşağıdaki hesap benim bağımsız işlem sayısı hesabımdır; donanım ölçümü değildir.

`P⁻ = FPFᵀ + Qd` içinde iki yoğun 15×15 çarpım:

- Güncelleme başına **6750 çarpma ve 6300 toplama**, yaklaşık 6750 MAC eşdeğeri.
- 100 Hz’te yaklaşık **675.000 MAC/s**.
- Optimize skaler ölçüm güncellemesi için yaklaşık `4n² ≈ 900` MAC eşdeğeri alınırsa, saniyede 200 skaler güncelleme **180.000 MAC/s** ekler.
- Ara toplam **855.000 MAC/s** olur. Rapordaki 3–6 çevrim/MAC varsayımıyla yük **%1,6–3,2** çıkar.

Bu ara toplam; nominal durum entegrasyonu, Jacobian üretimi, süreç gürültüsü hesabı, quaternion işlemleri, hata enjeksiyonu/reset, kapılama ve bellek erişimlerinin bütününü kapsamıyor. Buna rağmen iyi optimize edilmiş uygulamada %2–4 hedefi şaşırtıcı değildir.

Tersine, Joseph formu her skaler ölçüm için iki genel yoğun matris çarpımıyla uygulanırsa, 200 ölçüm/s için toplam yaklaşık **2,025 milyon MAC/s** olur. Aynı çevrim varsayımıyla **%3,8–7,6**, diğer giderler hariçtir. Seyreklik kullanımı sonucu azaltabilir.

Hüküm: **ESKF15 bu işlemci için hesaplama açısından makul; %2–4’ü mimari kapasite garantisi saymak makul değil.** MAC başına 3–6 çevrim de kaynakla veya ölçümle doğrulanmamış varsayımdır. [ESKF ve Joseph denklemleri](https://arxiv.org/pdf/1711.02508), [Hedef işlemci](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html).

**6. `f32` zorunluluk değil; çözünürlük hesabı da hatalı.**

Rapordaki “`f32` zorunlu” ifadesinin doğrusu, “yoğun filtre aritmetiğinde `f32` tercih edilmeli”dir. `f64` kullanılabilir, fakat bu çipte temel çift duyarlıklı aritmetik donanım desteği almaz. Orijin ve seyrek coğrafi dönüşümlerde kullanılmasıyla bütün kovaryans hesabında kullanılması aynı maliyet değildir. [ST işlemci açıklaması](https://www.st.com/en/microcontrollers-microprocessors/stm32u5a9nj.html).

10.000 metre, `2¹³`–`2¹⁴` aralığındadır. Binary32 için komşu değer aralığı:

`ULP = 2^(13−23) m = 0,0009765625 m ≈ 0,977 mm`.

Rapordaki `10000 / 2²⁴` hesabı gerçek ULP değildir. Yerel NED seçiminin uygunluğu değişmiyor; temsil çözünürlüğü de kestirim doğruluğu anlamına gelmiyor.

**7. Saat değiştirmeden kurulan el sıkışması, bütün firmware için aynı lockstep değil.**

Rapordaki IMU sıra numarasıyla aktüatör eşleme deseni kullanılabilir. Fakat `dt`yi örnekten almak yalnızca o `dt`yi kullanan hesapları düzeltir. Embassy zamanlayıcıları, haberleşme zaman aşımı, görev süreleri ve watchdog gerçek zamanla ilerlemeye devam eder.

PX4 SITL belgesi, sensör mesajının **hem sensör durumunu hem PX4 zamanını** güncellediğini açıkça söylüyor. Dolayısıyla öneri, “gerçek zamanda ilerleyen HIL içinde kontrol adımlarının eşlenmesi” olarak adlandırılmalı. Durdurulabilir veya değişken hızlı tam simülasyon için ilgili yazılım zaman kaynakları ayrıca tasarlanmalı. [PX4 lockstep açıklaması](https://docs.px4.io/main/en/sim_gazebo_classic/#lockstep).

**8. DVL eklentisinin alternatif yolu gözden kaçmış.**

Raporda kullanılmadığı belirtilen `VISION_SPEED_ESTIMATE` yolu güncel kodda mevcut. `should_send` varsayılanı `POSITION_DELTA`, fakat `SPEED_ESTIMATE` seçilince ilgili gönderici çağrılıyor. “Varsayılan yol bu değil” denebilir; “eklentide kullanılmıyor” denemez. [Eklenti uygulaması](https://github.com/bluerobotics/BlueOS-Water-Linked-DVL/blob/master/dvl-a50/dvl.py).

**9. `postcard + COBS` seçiminin gerekçesi eksik.**

Seçim teknik olarak savunulabilir: iki uç Rust, sabit tamponlarla çalışma mümkün, `no_std` desteği var ve COBS bayt akışında çerçeve sınırlarını belirliyor. Ancak:

- Postcard’ın kararlı tel formatı, uygulama şemasının sürümler arasında uyumlu olmasını garanti etmez.
- Paylaşılan crate, farklı sürüm PC yazılımıyla firmware’in eşleşmesini tek başına doğrulamaz.
- `to_slice_cobs` yalnızca serileştirme ve COBS uygular; CRC eklemez. Postcard ayrı CRC işlevleri sunar.
- `postcard-rpc` şema/yol anahtarlarıyla yardımcı olabilir; bu özellik düz postcard kullanımına kendiliğinden gelmez.

Özel protokol seçiminin sağlam gerekçesi **tam sensör semantiği ve iki Rust uç arasında bakım kolaylığı** olabilir. MAVLink’in olanaksız veya ölçüm yapılmadan daha yavaş olduğu iddia edilmemeli. [Format şartnamesi](https://postcard.jamesmunns.com/wire-format), [Serileştirme ve CRC kaynak kodu](https://github.com/jamesmunns/postcard/blob/main/source/postcard/src/ser/mod.rs), [postcard-rpc](https://docs.rs/postcard-rpc/latest/postcard_rpc/).

**10. Bazı öneriler gereğinden kesin ifade edilmiş.**

Ayrı workspace, feature birleştirmesini önlemenin tek yolu değildir; ayrı derleme çağrıları da yeterli olabilir. Sızıntıda SURFACE, ArduSub’da koşulludur. QGC’de MAVLink haberleşmesi, bütün kurulum ve kip arayüzlerinin hazır olacağı anlamına gelmez. [Cargo](https://doc.rust-lang.org/cargo/reference/resolver.html#features), [ArduSub failsafe](https://github.com/ArduPilot/ardupilot/blob/master/ArduSub/failsafe.cpp), [QGC mimarisi](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/firmware_plugin.html).

## Rapor dışı ek bulgular

- **PX4 HITL’in bakım ve araç kapsamı önemli bir sınırlama.** Güncel resmî belge, HITL’in topluluk tarafından sürdürüldüğünü ve güncel PX4 sürümleriyle çalışmayabileceğini söylüyor. Belgelenen Gazebo Classic HITL araçları multicopter ve VTOL; UUV desteği, hazır UUV HITL desteği anlamına gelmiyor. [PX4 HITL](https://docs.px4.io/main/en/simulation/hitl).

- **Mevcut PX4 HITL alıcısı batarya modelinizi kendiliğinden kullanmaz.** `handle_message_hil_sensor`, batarya yayımlama işlevini çağırıyor; mevcut uygulama %70 kalan kapasite ve 10 A gibi sentetik değerler oluşturuyor. Gerçekçi düşük batarya senaryosu için mesaj tanımından bağımsız olarak alıcı davranışı da değiştirilmelidir. [PX4 alıcı kodu](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/mavlink/mavlink_receiver.cpp).

- **ArduPilot’ta dış fizik modeliyle donanımda simülasyon alternatifi belgelenmiş.** Güncel “Simulation on Hardware” belgesinin Simulink bölümünde haricî JSON fizik modeli bağlantısı var. Bu, eski MAVLink HIL yolunun kaldırılmadığını kanıtlamaz; fakat “ArduPilot’ta haricî simülatörlü donanım testi yapılamaz” sonucu da çıkarılamaz. STM32U5A9/ArduSub üzerinde hazır destek bu kaynakla doğrulanmış değildir. [Resmî belge](https://ardupilot.org/dev/docs/sim-on-hardware.html#using-simulink-for-physics-simulation).

- **Embassy’de U5 HS USB için kaynak kodu mevcut.** Güncel `otg.rs`, iç HS PHY için `new_hs` ve STM32U5’e özgü PHY başlatma kodu içeriyor. Bu, rapordaki belirsizliği azaltır; kartta kararlı enumerasyon, macOS/Windows gecikmesi ve 200/400 Hz tur süreleri hâlâ ölçülmüş değil. [Embassy USB sürücüsü](https://github.com/embassy-rs/embassy/blob/main/embassy-stm32/src/usb/otg.rs).

- **ESKF doğrulaması yalnızca referans çıktılarıyla karşılaştırmadan ibaret olmamalı.** Hata enjeksiyonundan sonra kovaryans reset dönüşümü tanımlanmalı. İnovasyon kapısı `P`den tek başına değil, `S = HPHᵀ + R` üzerinden hesaplanmalı. İlişkili ölçüm gürültüsünde eksenleri bağımsız skaler ölçümler gibi işlemek de ayrıca gerekçelendirilmelidir. Bunlar raporun önerisini tamamlayan matematiksel gerekliliklerdir. [Solà, Bölüm 6](https://arxiv.org/pdf/1711.02508).

- **Çıkarım: sensör sıralaması ve iki saat alanı protokolün parçası olmalı.** Aynı simülasyon zamanındaki DVL/basınç paketinin IMU’dan önce mi sonra mı işleneceği, adımın tamamlandığının nasıl anlaşılacağı ve yinelenen paketlerin davranışı belirlenmezse yalnızca IMU sıra numarası deterministik tekrar oynatmayı sağlamaz. Simülasyon zamanı ile bağlantı/watchdog için kullanılan gerçek zamanın sorumlulukları açıkça ayrılmalı.

- **Kaynak erişim sınırları:** [ST veri sayfası](https://www.st.com/resource/en/datasheet/stm32u5a9nj.pdf) zaman aşımına uğradı; FPU bilgisi erişilen ST ürün sayfasından doğrulandı. Raporun [ArduPilot forum kaynağı](https://discuss.ardupilot.org/t/is-hil-mode-remove-form-v4-1-5/85213) 429 döndürdü; kaldırılma gerekçesi doğrulanmadı. Denenen sürüme özgü docs.rs **Embassy USB Driver/Cargo.toml**, **mavlink-core Cargo.toml** ve docs.embassy.dev **U5A9 Driver** adresleri açılamadı; bağımlılıklar erişilen paket sayfaları ve depolardan kontrol edildi. Crates.io bağımlılık API’sine erişilemedi, paket sayfası yalnızca JavaScript uyarısı döndürdü. Eski `embassy-executor/src/arch/cortex_m.rs` yolu 404 verdi; executor özellikleri resmî belgeden doğrulandı. GitHub etiket listeleme çağrısı araç tarafından reddedildi; etiket üzerinden kaynak sabitlemesi doğrulanmadı.

## Raporlar arası çelişki değerlendirmesi

Bu görevde ayrı bir raporlar arası karşılaştırma sorulmadığından diğer araştırma raporları hakkında hüküm verilmedi. Buradaki sonuçlar `06-otopilot.md` ile açılarak incelenen birincil kaynakların karşılaştırmasına dayanır.

## Mimari için net sonuçlar

1. **Katmanlı Rust çekirdeği, ESKF15 ve kademeli PID başlangıcı savunulabilir.** Referans projeler bu seçimi destekliyor; hibrit araç için hazır çözüm oluşturmuyor.
2. **MAVLink’i sensörleri taşıyamadığı gerekçesiyle eleme.** Özel dialect ile `postcard + COBS` seçeneklerini sensör semantiği, bakım ve ölçülmüş gecikmeyle karşılaştır.
3. **Postcard seçilirse protokolü tamamla:** sürüm/şema doğrulaması, sınırlandırılmış çerçeveler, sıra/adım kimliği, hata davranışı ve bütünlük denetimi tanımla.
4. **Gerçek zamanlı HIL ile durdurulabilir SIL’in saat politikalarını açıkça belirle.** IMU–aktüatör el sıkışmasını bütün firmware’in zaman eşlemesi sayma.
5. **%2–4 EKF yükünü kapasite garantisi olarak kullanma.** Optimize `f32` uygulamasını kartta, ölçüm yığılmaları ve haberleşme kesmeleriyle birlikte ölç.
6. **Native USB güçlü adaydır.** 921600 baud UART’ta bant genişliği kadar 400 Hz lockstep’in yaklaşık 1,845 ms aktarım tabanı da kararı belirler.
7. **QGC uyumunu erken prototiple doğrula.** Parametreler, kipler, görev yükleme ve reset davranışı temel telemetriden ayrı kabul ölçütleri olmalı.
8. **Geometri ve sensör sözleşmesini önce kesinleştir.** İtici düzeni, DVL çerçevesi/kovaryansı, basınç modeli ve yüzey–dalış geçişleri kontrol ve kestirici tasarımını belirler.