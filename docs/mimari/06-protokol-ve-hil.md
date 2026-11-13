# 06 — Protokol ve HIL köprüsü

Sürüm: 0.1 (2026-11-12). Durum: taslak; mimar onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) Bölüm 9, satır 06'nın kapsamını yazar: A1'in mesaj kümesi ve gerekleri, sonar profilinin parçalanması, adım bariyeri, iki saat alanı, bağlantı yaşam döngüsü, SIL taşıması, bağlantı emülatörü ve yer istasyonu bağlantısı. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir (bundan sonra "00"). Atıf biçimi: `NN.k` = 00 Bölüm 4'teki düzeltme satırı, `Çn` = çelişki çözümü, `Mn` = ölçüm, `Kn`/`Ön` = karar günlüğü, `DNN` = `docs/research/dogrulama/NN-codex.md`, `NN §x` = ham rapor bölümü. Ham rapordan alınan her değer önce 00'daki düzeltmeyle karşılaştırıldı.

**Bu bölümdeki hiçbir gecikme, bant genişliği ya da işlemci süresi ölçülmemiştir.** Hesaplar mesaj boyutlarından ve varsayılan hızlardan türetilir; "tahmin" ya da "hedef" diye işaretlenir. Bu bölüm kod içermez; alan listeleri ve imzalar sözleşme işinin girdisidir (iskelet §3).

---

## 1. Amaç ve sorumluluklar

1. **A1'in içeriği:** sim sunucusu ile kart (ya da SIL koşucusu) arasındaki bütün mesajların adı, yönü, hızı, alanları ve taşıdığı zaman damgası.
2. **Çerçeveleme ve biçim:** MAVLink 2 + özel dialect `ida` (Ö2, Ç3); ölçüm M9'da yetersiz çıkarsa geçilecek açık serileştirmeli yedek biçimin gerekleri.
3. **Oturum:** el sıkışma, sürüm ve şema doğrulaması, kalp atışı, kopma ve yeniden bağlanma; iki tarafın her durumdaki davranışı.
4. **Adım bariyeri:** adım partisinin kuruluşu, sıra numarası eşlemesi, son tarih ve kaçırma politikası (iskelet §4, Ç1, A8).
5. **İki saat alanının protokoldeki karşılığı:** hangi alan ve hangi karar hangi saati kullanır.
6. **Sonar profilinin taşınması:** Ping360 tarama satırının parçalanması, yeniden birleştirilmesi ve adımlara yayılması.
7. **Bağlantı emülatörü ve yer istasyonu ağ geçidi:** yer istasyonu ile araç arasındaki telsiz ve kablo bağlantısının modeli.
8. **Paylaşılan protokol modülü:** kart yazılımı ile SIL koşucusunun ortak kullandığı C modülü (`protocol/`) ve sim sunucusundaki HIL köprüsü (`sim/hil/`).

**Yazmadıkları:** mesajları üreten sensör ve fizik modelleri (04, 05), kartın USB ve seri sürücüsü, kesme düzeni ve gözetim zamanlayıcısı (01), çekirdeğin girdileri nasıl kullandığı (02, 03), web panelleri ve kayıt biçimi (08).

**Temel ilke:** kart tarafında protokolü yalnız sürücü katmanı (bağlantı sürücüsü) bilir; çekirdek bayt ya da MAVLink görmez (02 §3.4). Kart, HIL 1x ile bariyerli HIL arasında **hiçbir davranış farkı göstermez**: her IMU adımına aynı sıra numaralı bir aktüatör çerçevesiyle yanıt verir. Kipi yalnız sim sunucusunun sürücüsü belirler (04 §2.11). Böylece kipin kendisi de karta sızmaz (iskelet §2).

---

## 2. İç yapı

### 2.1 Alt parçalar ve veri akışı

```
                    ┌──────────────── Sim sunucusu (C++) ─────────────────────────────┐
 Web panelleri ─A3─▶│ GCS ağ geçidi ──▶ Bağlantı emülatörü ──┐                         │
 QGC (MAVLink/UDP) ▶│ (sysid 254/255)   (yalnız GCS düzlemi) │                         │
                    │                                        ▼                         │
                    │ Dünya (04) ─▶ Sensörler (05) ─▶ Parti kurucu ─▶ Çoklayıcı ─▶ Hata│
                    │   ▲               Toplu zamanlayıcı ─┘   (adım n)     enjektörü  │
                    │   └── LockstepDriver ◀── Bariyer ◀── Ayrıştırıcı ◀────┘  │       │
                    └──────────────────────────────────────────────────────────┼───────┘
                                       ITransport: USB CDC | ST-LINK VCP | TCP loopback
                    ┌──────────── Kart yazılımı / SIL koşucusu (C) ────────────┼───────┐
                    │ bayt G/Ç (01 / SIL) ─▶ ida_link: ayrıştırıcı, oturum,           │
                    │   adım hazırlama, A1↔A4 dönüşümü ─▶ ida_core_step (02) ─▶ yanıt │
                    └─────────────────────────────────────────────────────────────────┘
```

| Parça | Yer | Görev |
|---|---|---|
| `protocol/ida.xml` | depo | `common.xml`'i içeren dialect; `mavgen` ile üretilen C başlıkları depoda tutulur, derlemede sürekli kod üretimi gerekmez (08.10) |
| `protocol/ida_link` (C17) | kart ve SIL koşucusu | ayrıştırıcı, kart tarafı oturum durum makinesi, adım hazırlama tamponu, A1 ↔ `ida_step_in_t`/`ida_step_out_t` dönüşümü, gönderim önceliği |
| `sim/hil/` (C++20) | sim sunucusu | `ITransport` uyarlayıcıları, sim tarafı oturum, parti kurucu, toplu veri zamanlayıcısı, bariyer, hata enjektörü, GCS ağ geçidi, bağlantı emülatörü |

Kanal: tek bayt akışı, iki mantıksal düzlem. **HIL düzlemi** (sensör, aktüatör, durum, kestirim, oturum) emülatörden geçmez; **GCS düzlemi** (yer istasyonu komutları, görev, telemetri) emülatörden geçer (11 §A.5 son madde: HIL yolunun emülatörden etkilenmesi HIL bütünlüğünü bozar). Kart gelen çerçeveyi gönderenin `sysid` değerine göre yönlendirir: 250 (sim HIL kaynağı) sensör portuna, 254/255 (yer istasyonları) bağlantı portuna (değerler öneri; sözleşmede sabitlenir). Sim, kartın gönderdiği çerçeveyi mesaj kimliğine göre ayırır. Tek akış seçildi çünkü yedek taşıma (VCP) ve SIL soketi tek kanallıdır; USB'de ikinci CDC arayüzü (08 §2.7) yalnız M1 kafa-kuyruk tıkanması gösterirse yedektir (Bölüm 6).

### 2.2 Mesaj kümesi (A1)

**Ortak başlık `H`** (HIL düzlemindeki her sensör mesajı): `t_meas_ns` int64 (sim zamanı, ns), `step_seq` uint32 (teslim edildiği kontrol adımı), `flags` uint16 (bit 0 geçerli, 1 doymuş, 2 sensör arızası; 02 §3.3), `inst` uint8 (sensör örneği). 15 B. Bütün zamanlar ns, çünkü sim zamanı tamsayı ns'dir (iskelet §4; 02 §8.1 md.3 ile uyumlu).

**Sim → kart, HIL düzlemi** (yük boyutu sondaki sıfır kırpması olmadan en kötü durumdur; çerçeve = yük + 12 B imzasız MAVLink 2 ek yükü, 08.10):

| Mesaj | Hız (dayanak) | Alanlar (birim, tip) | Zaman damgası | Yük / çerçeve B |
|---|---|---|---|---|
| `IDA_IMU` | 400 Hz (iskelet §4) | `H`; `gyro[3]` rad/s f32; `accel[3]` m/s² f32 (özgül kuvvet); `interval_ns` u32; `temp_c` f32; `n_aux` u16 (partide kendisinden önce gelen çerçeve sayısı) | `t_meas` = adım sınırı = adımın sim zamanı; aralık ortalaması (02 §3.3) | 49 / 61 |
| `IDA_MAG` | 10–100 Hz (02 §2.2; 05 §2.8 doğrulanmadı) | `H`; `field[3]` T f32 | örnekleme anı | 27 / 39 |
| `IDA_BARO` | ≤ 25 Hz (D05 E5) | `H`; `p_abs` Pa f32; `temp_c` f32 | dönüşüm sonu | 23 / 35 |
| `IDA_GNSS` | 5 Hz (02 §2.2) | `H`; `lat_e7`, `lon_e7` i32; `alt_mm` i32; `vel_ned[3]` m/s f32; `eph`, `epv` m f32; `fix_type`, `n_sv` u8 | çözüm anı | 49 / 61 |
| `IDA_DVL` | 2–15 Hz (D05 #3a) | `H`; `t_report_ns` i64; `vel[3]` m/s f32 (DVL çerçevesi); `fom` m/s f32; `cov[6]` f32 (simetrik üst üçgen; 02'nin `cov[9]`'una açılır); `beam_dist[4]` m f32 (çözülemeyen ışın −1); `altitude` m f32; `dvl_bits` u8 (`vel_valid`, `beam_valid`×4, `track` dip/su, `cov_valid`); `status` u8 | `t_meas` = `time_of_validity` = yansıma anı; `t_report` = rapor gönderim anı. Teslim = `t_report` + iletim; `2r/c` eklenmez (05.3, Ç13) | 85 / 97 |
| `IDA_PING1D` | 20 Hz (tahmin) | `H`; `distance` m f32; `confidence` % u8; `ping_number` u32; `scan_start`, `scan_length` m f32 | ping anı (05'in tanımı) | 32 / 44 |
| `IDA_PING360_HEAD` | profil başına 1 | `H`; `profile_seq` u16; `angle_grad` u16 (0–399); `n_samples` u16 (≤ 1200); `sample_period` u16 (25 ns birimi); `tx_duration` u16 µs; `tx_freq_khz` u16; `gain` u8; `last_step_seq` u32 | 05'in ürettiği ping zamanı; cihazda yakalama damgası yoktur (00 Bölüm 6), 06 yalnız taşır | 32 / 44 |
| `IDA_PING360_DATA` | profil başına ⌈n/251⌉ | `profile_seq` u16; `inst` u8; `frag_idx` u8; `data[251]` u8 | HEAD'den | 255 / 267 |
| `IDA_BATT` | 10 Hz (tahmin) | `H`; `voltage` V f32; `current` A f32; `temp_c` f32 | örnekleme anı | 27 / 39 |
| `IDA_LEAK` | 10 Hz (tahmin) | `H`; `mask` u8 (prob başına bit) | örnekleme anı | 16 / 28 |
| `IDA_DETECTION` | kameraya bağlı; 30 Hz × ≤ 4 (tahmin, A24 açık) | `H` (`inst` = kaynak sensör); `det_seq` u32; `source`, `frame`, `kind`, `class` u8; `values[6]` f32 (türe göre yön, yükseliş, menzil, konum); `cov[6]` f32 (ilk üç bileşenin 3×3 üst üçgeni); `confidence` f32; `producer_id` u8; `producer_ver` u16 | `t_meas` = yakalama anı (07 §3.2 "Detection v1"; ns) | 78 / 90 |

**Kart → sim, HIL düzlemi:**

| Mesaj | Hız | Alanlar | Zaman | Yük / çerçeve B |
|---|---|---|---|---|
| `IDA_ACTUATOR` | her IMU adımında 1 | `step_seq` u32 (IMU'nunki); `t_meas_ns` i64 (IMU'nunkinin yankısı); `pwm_us[8]` u16 (0 = darbe yok); `launcher[2]` u8 (bit 0 emniyet, 1 hazır, 2 ateşle; seviye sinyali); `act_flags` u16 (armed, girdi eksik, yineleme yanıtı, sıra boşluğu, IMU'suz adım, önceki adım süre aşımı); `step_us` u16 (DWT ile adım süresi); `rx_to_tx_us` u16 (IMU alındı → yanıt gönderildi, kart duvar saati) | sim zamanı yankısı + kart duvar saati süreleri | 36 / 48 |
| `IDA_NAV_STATE` | 50 Hz (öneri; en kötü 100 Hz) | `step_seq`; `t_ns`; `p_ned[3]`, `v_ned[3]`, `q_nb[4]`, `omega_b[3]`; `depth`, `altitude`; `p_sigma[3]`, `v_sigma[3]`, `att_sigma[3]` (f32); `est_level`, `src`, `reset_counter` u8 (02 §3.6) | ölçüm zamanı (sim) | 111 / 123 |
| `IDA_BOARD_STATUS` | 10 Hz | `board_wall_us` u64; `boot_id` u32; `mode` u8; `failsafe` u16; sayaçlar u32 (CRC hatası, düşürülen çerçeve, sıra boşluğu, eksik girdi, yineleme, süre aşımı); `cpu_load` u8; `max_step_us` u16; `tx_queue_peak` u16 | kart duvar saati | 44 / 56 |
| `IDA_FC_STATUS` | 10 Hz | atış kontrol durumu; alanlar 03'ten beklenen | ölçüm zamanı | ~40 / ~52 (tahmin) |
| `IDA_DETECTION` | sonar algılaması geldikçe (≤ 40/s, tahmin) | yukarıdakiyle aynı biçim; kart içi algılamanın değerlendirme kaydı | yakalama anı | 78 / 90 |
| `IDA_PING360_CONFIG` | değiştikçe | `mode`, `gain` u8; `tx_duration`, `sample_period`, `tx_freq_khz`, `n_samples`, `start_angle`, `stop_angle` u16; `num_steps`, `delay` u8; `auto` u8 (Ping360 #2601 ve #2602 `auto_transmit` karşılığı; D05 E) | — | 17 / 29 |

`launcher` alanında ateşleme bir darbe değil, 03'ün belirlediği en kısa süre boyunca tutulan bir seviyedir; sim yükselen kenarı fiziksel fırlatma olarak işler. Tek bir çerçevenin kaybı ateşlemeyi kaybettirmez. Emniyet zinciri ve darbe süresi 03'ün, fırlatma fiziği 04'ün işidir.

`pwm_us`: A1 itici komutunu ESC'nin gördüğü darbe genişliği olarak taşır; sim ESC modelini buna uygular (04 §3 `ActuatorFrame`). Çekirdeğin normalleştirilmiş `thr_cmd[8]` çıktısını (02 §3.1) µs'ye çevirmek kartın platform katmanının işidir (01'den beklenen; ölü bant ve uç değerler parametre). "Darbe yok" (0) değeri sinyal kaybını ifade eder.

**Oturum ve ölçüm mesajları (iki yön):**

| Mesaj | Kaynak | Hız | İçerik |
|---|---|---|---|
| `HEARTBEAT` | common | 1 Hz | HIL düzleminde sim canlılığı (sysid 250); GCS düzleminde `MAV_TYPE_SUBMARINE` (12), `MAV_AUTOPILOT_GENERIC` (0) (06 §2.7) |
| `TIMESYNC` | common | 10 Hz | duvar saatleri arası gidiş-dönüş ve ofset kestirimi (alan listesi sözleşmede `common.xml`'den doğrulanır) |
| `IDA_HELLO` | ida | el sıkışmada, 200 ms'de bir yinelenir | `proto_major`, `proto_minor` u8; `dialect_hash` u64; `role` u8 (SIM, BOARD, SIL); `sw_version`, `git_short` u32; `build_flags` u16 (FP politikası vb.); `boot_id` u32 (her açılışta rastgele); `reset_cause` u8 (POR, pin, yazılım, IWDG, BOR; ST'nin ayrı bayrakları, 11.19); `session_nonce` u32; `ctrl_rate_hz` u16; `caps` u32; `max_ping360_samples` u16 |
| `IDA_HELLO_ACK` | ida | yanıt | `result` u8 (OK, MAJOR, HASH, RATE, BUSY); `session_nonce` yankısı; `run_id` u32 |
| `IDA_LOOPBACK` | ida | ölçümde | `seq` u32, `t_send_wall_ns` i64, `pad[≤ 243]`; kartın bağlantı sürücüsü çekirdeğe vermeden geri yollar (M1'in 64/256 B gidiş-dönüş testi) |

`dialect_hash`: derlemede `(msgid, CRC_EXTRA, ad)` üçlülerinin sıralı listesinden hesaplanan 64 bit özet. MAVLink'in ileti başına `CRC_EXTRA` denetimi tek bir mesajın düzen farkını CRC hatasına çevirir ama eksik ya da fazla mesajı ve dialect sürümünü göstermez; özet bunu el sıkışmada açıkça yakalar.

**GCS düzlemi** (yalnız standart `common` mesajları ve iki özel atış mesajı; 06 §3.3, 06.12):
- Yer istasyonu → araç: `HEARTBEAT` (GCS), `COMMAND_LONG` (arm/disarm, kip, `MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN`, görev başlat/duraklat), `MANUAL_CONTROL`, `SET_POSITION_TARGET_LOCAL_NED` (GUIDED), görev protokolü `MISSION_COUNT` → `MISSION_REQUEST_INT` → `MISSION_ITEM_INT` → `MISSION_ACK` (06 §2.7; özel öğeler `MAV_CMD_USER_*`, 02 §2.6), `PARAM_REQUEST_LIST`/`PARAM_REQUEST_READ`/`PARAM_SET`, `IDA_FC_COMMAND` (atış komutları; jeton ve süre alanları 03'ten beklenen).
- Araç → yer istasyonu: `HEARTBEAT`, `SYS_STATUS` (sızıntı için `MAV_SYS_STATUS_SENSOR_LEAK`, 06.1), `ATTITUDE`, `GLOBAL_POSITION_INT`, `LOCAL_POSITION_NED`, `BATTERY_STATUS`, `MISSION_CURRENT`, `STATUSTEXT`, `COMMAND_ACK`, `PARAM_VALUE`, `AUTOPILOT_VERSION`, `IDA_FC_STATUS`, `IDA_FC_COMMAND_ACK`.
- 6 eksenli elle kumanda `MANUAL_CONTROL`'ün alanlarına sığmazsa özel `IDA_MANUAL_6DOF` eklenir (sözleşme işinde karar; açık nokta).

**Standart ile özel ayrımının gerekçesi.** Gerekçe "MAVLink bu verileri taşıyamaz" değildir; özel mesajların kendisi MAVLink'tir (06.1, 08.10, Ç3).

| Standart aday | Karar | Neden |
|---|---|---|
| `HEARTBEAT`, `TIMESYNC`, GCS düzlemindeki bütün mesajlar | yeniden kullanılır | anlamları birebir; hazır yer istasyonu yazılımı bunları anlar (06.12) |
| `HIL_SENSOR`, `HIL_GPS` | kullanılmaz | adım kimliği taşımaz; `HIL_SENSOR` aynı paketteki güncellemeler için tek ortak zaman damgası kullanır (06.3), biz her sensörü kendi damgasıyla ayrı gönderiyoruz; alıcı davranışını zaten kendimiz yazıyoruz (00 Bölüm 6, PX4 HITL dersi) |
| `HIL_ACTUATOR_CONTROLS` | kullanılmaz | normalleştirilmiş `float[16]`; fırlatıcı seviyeleri ve adım yankısı yok |
| `ODOMETRY`, `VISION_SPEED_ESTIMATE`, `OPTICAL_FLOW` (DVL) | kullanılmaz | ışın geçerliliği, dip/su izi ayrımı ve DVL kalite alanları yok; zaman anlamı yansıma anı değil (06.1, 05.3) |
| `DISTANCE_SENSOR` (Ping1D) | kullanılmaz | ms çözünürlüklü zaman, adım kimliği yok |
| `ENCAPSULATED_DATA` (Ping360) | kullanılmaz | genel görüntü aktarımı; sonar geometrisi ve menzil hücresi anlamı yok (06.2) |
| `BATTERY_STATUS` (HIL düzleminde) | kullanılmaz | zaman damgası yok; GCS düzleminde kullanılır |

### 2.3 Bant genişliği bütçesi

Hesap yön başınadır (06.4: çift yönlü bağlantıda iki yön toplanmaz). Hızlar 2.2'deki tablodan; GCS düzlemi A1 içinde yön başına 16 kB/s ile sınırlanır (öneri; emülatörün telsiz profili zaten daha dar).

| Sim → kart | B/s |
|---|---:|
| IMU 61 × 400 | 24.400 |
| MAG 39 × 100 + BARO 35 × 25 + GNSS 61 × 5 + DVL 97 × 15 + PING1D 44 × 20 + BATT 39 × 10 + LEAK 28 × 10 | 8.085 |
| Dışarıdan algılama 90 × 120 | 10.800 |
| Oturum (HEARTBEAT 21 + TIMESYNC 30 × 10) | 321 |
| **Ara toplam** | **43.606** |
| Ping360, 50 m menzil: 400 açı / 33 s ≈ 12,1 profil/s × 1.324 B (D05 #1b) | 16.050 |
| GCS düzlemi üst sınırı | 16.000 |
| **Toplam, 50 m** | **75.656** |
| Ping360 en kısa menzil: 400 / 3,42 s ≈ 117 profil/s × 1.324 B (üreticinin Ethernet süresi; modelimiz için üst sınır tahmini) | 154.900 |
| **Toplam, en kötü** | **214.506** |

1200 örneklik profil: HEAD 44 + 4 × 267 + son parça (4 + 196 + 12 = 212) = **1.324 B** (%10,3 ek yük). 600 örnek: 44 + 2 × 267 + 114 = 692 B.

| Kart → sim | B/s |
|---|---:|
| ACTUATOR 48 × 400 | 19.200 |
| NAV_STATE 123 × 100 (en kötü) | 12.300 |
| BOARD_STATUS 56 × 10 + FC_STATUS ~52 × 10 | 1.080 |
| Kart içi algılama kaydı 90 × 40 | 3.600 |
| Oturum | 321 |
| GCS düzlemi üst sınırı | 16.000 |
| **Toplam** | **52.501** |

| Taşıma (yön başına kapasite) | Sim → kart, 50 m | Sim → kart, en kötü | Kart → sim |
|---|---:|---:|---:|
| USB 2.0 HS bulk: kuramsal ≈ 53 MB/s (13 × 512 B / 125 µs mikroçerçeve; USB 2.0 belirtimi, bu depoda kaynakla doğrulanmadı) | < %0,2 | ≈ %0,4 | < %0,1 |
| VCP 921.600 baud = 92.160 B/s (01.2) | %82 | **%233, sığmaz** | %57 |
| VCP 2 Mbaud = 200.000 B/s | %38 | **%107, sığmaz** | %26 |
| VCP 3 Mbaud = 300.000 B/s | %25 | %72 | %18 |

Sonuç: USB HS'te bant genişliği sorun değildir; belirleyici olan gecikme ve titreşimdir ve USB bulk aktarımı teslim süresi garantisi vermez (05.6, 00 Bölüm 6). VCP'de en kötü Ping360 akışı sığmaz; yedek taşımada profil hızı sınırlanır (2.5). VCP'nin DK'da sürdürülebilir hızı ölçülmemiştir; ST'nin V3E için yayımladığı 15 MHz bir kart garantisi değildir (01.3, M3).

### 2.4 Adım bariyeri

**Adım partisi.** Kontrol adımı `n`, sim zamanı `t_n = n · 2,5 ms` (400 Hz; iki fizik adımı, iskelet §4). Teslim zamanı `(t_{n−1}, t_n]` aralığına düşen her sensör örneği (teslim zamanı 05'in modelidir) adım `n`'in partisine girer ve `step_seq = n` taşır. Kart zaten yalnız adım sınırlarında işler; teslimin 2,5 ms ızgarasına oturtulması kartın göreceği davranışı değiştirmez. Parti iki kısımdır:

1. **Toplu kısım:** IMU(n−1) gönderildikten hemen sonra yollanır; kartın hesap yaptığı ve aşağı yönün boş olduğu pencereyi kullanır. İçeriği: Ping360 parçaları ve dışarıdan gelen algılamalar. Bu sınıftaki veri bu yüzden bir kontrol adımı (2,5 ms) gecikmeyle ve belirlenimci olarak teslim edilir; ek gecikme A6 kaydına yazılır.
2. **Kritik kısım:** 1x kiplerde duvar saati `T₀ + t_n`'den önce gönderilmez, böylece kartın gördüğü IMU aralığı gerçek zamanlıdır. Sıra: GCS düzlemi çerçeveleri (emülatörün `t_n`'e kadar serbest bıraktıkları), sensör örnekleri, en sonda `IDA_IMU`.

**Sıra kuralı.** Sensör örnekleri `(t_meas, sıra, inst, seq)` anahtarıyla sıralanır. Sıra, 02 §2.2'deki işleme sırasıdır: IMU, basınç, DVL, manyetometre, GNSS, Ping1D, batarya, sızıntı, algılama, Ping360. IMU iletimde en sona konur, çünkü **adımın tamamlandığını gösteren sonlandırıcı** odur. Çekirdek ise aynı zaman damgalı örnekler arasında IMU'yu ilk işler (02); iletim sırası ile işleme sırası ayrı tanımlanmıştır ve ikisi de sabittir (00 Bölüm 6: sensör sıralaması protokolün parçasıdır).

**Adımın tamamlandığının anlaşılması.** Kart `IDA_IMU(step_seq = n)` alınca partinin bittiğini bilir. `n_aux` ile aldığı `n` etiketli çerçeve sayısını karşılaştırır; tutmazsa `act_flags.girdi_eksik` bayrağını kaldırır. GCS çerçevelerinde `step_seq` alanı yoktur; bunlar geliş sırasına göre bir sonraki IMU'nun adımına atanır. Kritik kısımda gönderildikleri için atama belirlenimcidir. Sim partiyi 02'nin girdi sınırlarına göre böler (`IDA_IN_MAX` = 16 örnek, `cmds[8]`, adım başına tek Ping360 profili; 02 §3.1). Fazlası sonraki adıma kayar ve kaydedilir; kartın hazırlık tamponu bu yüzden taşmaz.

**Kartın yanıtı.** IMU(n) için:
- `n = son + 1` ise adım koşar.
- `n = son` ise **DUP**: adım koşmaz, önceki `IDA_ACTUATOR` yeniden gönderilir (02 `DUP_SEQ`).
- `n < son` ise **OLD**: çerçeve atılır ve sayılır.
- `n > son + 1` ise **GAP**: `n − son − 1` adım kaçırılmış sayılır, adım koşar.

`IDA_ACTUATOR(n)` adımdan hemen sonra gönderilir ve gönderim kuyruğunda her şeyin önündedir. Diğer çıktılar (NAV_STATE, durum, GCS telemetrisi) adım başına bayt bütçesiyle arkasından gelir (öneri: 256 B/adım, tahmin). IMU akışı kesilirse kartın `ida_core_tick_wall` ile ürettiği çıktı `IMU'suz adım` bayrağıyla gönderilir; sim bunu bariyer dışında uygular.

**Aktüatör uygulama gecikmesi `L_act`.** Sim, `IDA_ACTUATOR(n)`'i sim zamanında **sabit** `t_n + L_act` anında uygular (04 `ActuatorFrame.t_sim_ns`). Birincil değer 1 fizik adımıdır (1,25 ms); yedek değer 2 fizik adımıdır (Bölüm 6). Fizik adımı `[t_n, t_n + L_act]` önceki komutla, yanıt beklenirken hesaplanır. Uygulama anı sim zamanında sabit olduğu için taşıma titreşimi yörüngeye girmez. Gerçek kartta da hesap ve PWM güncellemesi kadar bir gecikme vardır; `L_act` değeri kayda yazılır.

**Son tarih ve kaçırma politikası** (iskelet §4, Ö11; A8 için bu bölümün kararı). 1x kiplerde `IDA_ACTUATOR(n)` için duvar saati son tarihi:

`D_n = W_n + L_act + dt_fizik − C_sim`

Burada `W_n` kritik kısmın gönderildiği an, `C_sim` sim tarafında bir fizik adımı ile sensör üretimi ve kodlamanın süresidir (ölçülmedi; M15). Yanıt `D_n`'e kadar gelirse `t_n + L_act` anında uygulanır. Gelmezse sim durmaz: son geçerli komutla ilerler, `kaçırılan_adım` sayacını artırır ve çalıştırmayı geçersiz işaretler. Son tarihten sonra gelen `IDA_ACTUATOR(n)` geç yanıt sayılır, kaydedilir ve **uygulanmaz**, böylece uygulama anları sabit kalır. Sim tarafında `seq` beklenenden küçükse "geç", eşitse ve zaten alınmışsa "yineleme", büyükse "protokol hatası" sayılır. SIL'de son tarih yoktur: sim süresiz bekler; yalnız 5 s'lik gerçek duvar saati gözetimi asılmış koşucuyu yakalar. "Bekle" ve "güvenli duruma geç" seçenekleri bariyerin değil kartın arıza güvenliği katmanının işidir (02 §4); bariyer yalnız "son komutla ilerle, say, geçersiz işaretle" politikasını uygular.

**Gecikme bütçesi (400 Hz, `L_act` = 1,25 ms; bütün terimler tahmin):**

| Terim | Değer | Dayanak |
|---|---:|---|
| `L_act + dt_fizik` | 2,50 ms | iskelet §4 |
| − `C_sim` (fizik adımı + sensör + kodlama) | ≈ 0,40 ms | tahmin; M15, M11 |
| − kartta adım en kötü süresi | ≤ 1,00 ms | 02 §2.2 hedefi; M5 |
| = iki yönlü taşıma ve iki işletim sisteminin G/Ç payı | **≈ 1,10 ms** | M1 bunu ölçer |

USB HS'te kablo üzerindeki aktarım süresi ihmal edilebilir; pay, iki taraftaki yazılım yığınlarının ve bulk zamanlamasının p99,9 gidiş-dönüşüne kalır. M1'in kabul eşiği buradan çıkar: 64 B gidiş + 48 B dönüş için p99,9 ≤ 1,1 ms (hedef, ölçülmedi). Tutmazsa `L_act` = 2,5 ms ile pay ≈ 2,35 ms'ye çıkar. Bu durumda IMU(n+1), yanıt(n) beklenirken gönderilir ve aynı anda iki istek yolda olabilir; kart sırayla işler.

VCP'de kablo süreleri payı doğrudan tüketir. IMU + ACTUATOR = 109 B, 921.600 baud'da 1,18 ms, 2 Mbaud'da 0,55 ms tutar (06.4'teki `HIL_SENSOR` + `HIL_ACTUATOR_CONTROLS` için 1,845 ms ile karşılaştırılabilir). Kritik kısmın en kötü hali, bütün yavaş sensörlerin aynı adıma düşmesi ve GCS payının dolu olmasıdır; bu yaklaşık 444 B eder ve 2 Mbaud'da 2,2 ms, 3 Mbaud'da 1,5 ms sürer. Kartın 1,0 ms'si eklenince 400 Hz payı aşılır. **Sonuç: yedek taşıma, yedek kontrol hızıyla (200 Hz) birlikte gelir.** 200 Hz'te `L_act` = 2,5 ms ile pay ≈ 3,45 ms − 1,0 ms ≈ 2,45 ms olur (iskelet §8'in iki satırı arasındaki bağ; Bölüm 8).

**Bariyerin garanti ettikleri:**
- Her adımın girdi kümesi yalnız sim zamanıyla belirlenir; taşıma zamanlamasından bağımsızdır.
- Her aktüatör çerçevesi tam `t_n + L_act` anında uygulanır, uygulanamadıysa sayılır.
- Kaçırma, eksik girdi ve yineleme sayaçları sıfır olan bir çalıştırmada kapalı çevrim yörüngesi, kartın adım işlevinin girdilerine bağlı kısmı ölçüsünde tekrarlanabilir.

**Garanti etmedikleri:**
- Kartın duvar saatine bağlı davranışı: bağlantı ve IMU zaman aşımları, gözetim zamanlayıcısı, GCS failsafe süreleri, kalp atışı (Ç1, D06 Y7).
- 03'ün arka plan algılama işinin hangi adımda biteceği. 03'ten beklenen: sonucun, profilin geldiği adımdan sabit K adım sonra tüketilmesi; aksi halde tekrarlanabilirlik bozulur.
- İnsan girdisinin zamanlaması: yalnız A6 kaydından yeniden oynatılabilir.
- Platformlar arası bit eşitliği (Ç7).
- USB teslim süresi (05.6).
- Duraklatılabilirlik: HIL'de duraklatma kartın zamanını durdurmaz (2.6).

**HIL 1x (bariyersiz).** Mesajlar aynıdır; sim beklemez ve yanıtı geldiği andan sonraki ilk fizik adımı sınırında uygular. Geliş gecikmesi kaydedilir; geç yanıt çalıştırmayı geçersiz kılmaz. Geçersizlik yalnız sim'in kendi gerçek zamanı kaçırmasından gelir (04 §2.11).

### 2.5 Sonar profilinin taşınması

- **Parçalama.** Profil bir `IDA_PING360_HEAD` ve `⌈n_samples / 251⌉` adet `IDA_PING360_DATA` ile gönderilir. Parça `k` örnekleri `[251k, 251k + 250]` aralığını taşır; son parçanın geçerli uzunluğu `n_samples − 251(⌈n/251⌉ − 1)` hesabından çıkar. Ayrı bir uzunluk alanı gerekmez. MAVLink 2'nin sondaki sıfır baytları kırpması (06 §2.7) son parçayı kısaltır; alıcı kırpılan baytları sıfırla tamamlar (MAVLink kuralı; sözleşmede doğrulanır). Bu yüzden değeri sıfır olan son örnekler doğru geri gelir. `mavgen` alanları tip boyutuna göre yeniden sıraladığı için `data` dizisi tel düzeninde sona düşer; tel düzeni elle değil, üretilen başlıktan alınır.
- **Adımlara yayma.** Toplu zamanlayıcı her adımın toplu kısmına en fazla `B_bulk` bayt koyar ve profili bir ya da birkaç adıma yayar. HEAD'deki `last_step_seq` son parçanın gideceği adımı bildirir. Profil o adımda teslim edilmiş sayılır ve `t_meas` yine ping zamanıdır. `B_bulk` taşıma profiline bağlıdır: USB HS'te 1.600 B (bütün profil tek adımda; tahmin, M1 doğrular); VCP'de `hız × (dt_kontrol − kritik kısmın süresi − pay)` ile hesaplanır, 2 Mbaud ve 200 Hz'te yaklaşık bir DATA parçası. Adım başına en çok bir profil tamamlanır (02'nin tek profil girdisi).
- **Yeniden birleştirme (kart).** Örnek (`inst`) başına iki tampon tutulur: biri birleştirilirken diğeri 03'e verilir. Her tampon `max_ping360_samples` bayttır (1200; bellek yerleşimi 01'den beklenen). Parça, HEAD'deki `profile_seq` ile eşleşmezse atılır.
- **Kayıp parça.** Profil ya hep birlikte teslim edilir ya hiç edilmez. `last_step_seq` adımının IMU'su geldiğinde eksik parça varsa profil atılır ve `ping360_eksik` sayacı artar. Aynı örnek için yeni HEAD gelince yarım profil atılır. Kısmi profil çekirdeğe verilmez; gerçek cihazda bozuk bir mesajın tümüyle kaybolmasıyla aynı sonuçtur. Kaybın nedeni çerçeve bozulmasıysa `n_aux` uyuşmazlığı zaten çalıştırmayı işaretler.
- **Örnek sayısı ve kalite kademesi.** Kart Ping360'ı gerçek cihazdaki gibi `IDA_PING360_CONFIG` ile yapılandırır. Sim etkin örnek sayısını `n_eff = min(n_istenen, N_kademe)` alır ve menzili korumak için `sample_period`'u `n_istenen / n_eff` oranında büyütür. HEAD etkin değerleri taşır; kart menzili gerçek cihazdaki gibi gelen alanlardan hesaplar. Kademe bilgisi karta gönderilmez, A6'ya yazılır. Önerilen `N_kademe`: tam 1200, sade 600; değerler 05'in sonar kademesiyle kesinleşir.
- **Taşıma doygunluğu.** Toplu kuyruk iki profili aşarsa en eski profil atılır, `bulk_drop` sayacı artar ve bariyerli çalıştırma geçersiz olur, çünkü modellenen sensör akışı değişmiştir. Modellenen cihaz bağlantısının zamanlaması (Ping360'ın kendi baud hızı, açı adımı, ping süresi; 05.2, Ç19) 05'in modelidir; 06 yalnız A1 kapasitesinin bunu taşıyıp taşımadığını denetler.

### 2.6 İki saat alanı

Sim zamanı karta **yalnız örnek damgalarıyla** taşınır. IMU'nun `t_meas` alanı adımın sim zamanıdır; kart saatini bundan kurmaz ve ayrı bir sim saati eşleme mesajı yoktur (Ç1: bariyer kartın bütün zamanını sim zamanına bağlamaz). Kartta bu alan "ölçüm zamanı"dır (02 §3.2 md.3).

| Karar ya da alan | Saat | Taraf |
|---|---|---|
| Örnek damgaları, adım partisinin içeriği, `L_act`, aktüatör uygulama anı | sim | sim |
| Emülatörün gecikme, kayıp ve kesinti olayları | sim (11 §A.5; hızlandırılmış SIL'de de doğru kalır) | sim |
| Bariyer son tarihi `D_n`, 1x kiplerde gönderim temposu | duvar | sim |
| IMU kesilmesi (> 10 ms) | duvar | kart (02 §4) |
| Oturum kalp atışı (1 Hz), bağlantı kaybı (500 ms hiç bayt yok; tahmin), el sıkışma yinelemesi | duvar | iki taraf |
| GCS failsafe süreleri (3 s / 60 s) | duvar (kart); mesajların karta varışı emülatörde sim zamanıyla | kart (02 §4) |
| Gözetim zamanlayıcısı | duvar | kart (01) |
| `TIMESYNC`, `rx_to_tx_us`, `step_us`, `board_wall_us` | duvar | iki taraf |
| SIL'de kartın "duvar saati" | sim zamanından türetilen sanal saat (02 §2.8); koşucunun asılmasının gözetimi gerçek duvar saatiyle | SIL |

Sim, `TIMESYNC` ile kart duvar saati ile kendi duvar saati arasındaki ofseti kestirir. Gönderim temposu `(T₀, t₀)` eşlemesiyle birlikte kullanılarak kart olayları A6'da sim zaman çizgisine yerleştirilir.

### 2.7 Bağlantı yaşam döngüsü

Durumlar (iki tarafta): `KAPALI → EL_SIKIŞMA → HAZIR → KOŞUYOR → KOPUK`.

| Olay | Sim | Kart (bağlantı sürücüsü) |
|---|---|---|
| Bağlantı açıldı | `IDA_HELLO` gönderir, 200 ms'de bir yineler | `IDA_HELLO` ile yanıtlar |
| Sürüm, şema ya da hız uyuşmazlığı (`proto_major`, `dialect_hash`, `ctrl_rate_hz`) | `IDA_HELLO_ACK(result ≠ OK)`; çalıştırma başlamaz; hata A3'e | `HAZIR`'a geçmez; çekirdek adım almaz, çıkışlar platformun açılış durumunda kalır |
| `proto_minor` farkı | kabul; yalnız ortak alanlar kullanılır (MAVLink 2 uzantı alanları) | aynı |
| Çalıştırma başlangıcı | kartı `COMMAND_LONG(MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN)` ile sıfırlar ve yeni `boot_id`'yi bekler. Her çalıştırma kartın açılış durumundan başlar (tekrarlanabilirlik) | standart yeniden başlatma komutu; gerçek otopilotlar da destekler, simülasyon bilgisi gerektirmez |
| İlk IMU | `flags.SEQ_RESET` taşır | `son = step_seq − 1` alır |
| Kart beklenmedik biçimde sıfırlandı (yeni `boot_id`, `reset_cause`) | olay; senaryoda planlı bir reset değilse çalıştırma geçersiz. Fizik sürer, iticilere "darbe yok" (0 µs) uygulanır ve davranışı ESC modeli belirler (04, M23). Yeniden el sıkışır ve `SEQ_RESET` ile sürdürür | açılıştan başlar |
| Kablo çıktı (OS aygıtın kalktığını bildirir ya da 500 ms bayt yok) | `KOPUK`; çalıştırma geçersiz; iticilere "darbe yok"; portu 1 s'de bir yeniden açmayı dener; bağlanınca el sıkışma ve yeni bölüt (A6'da işaretli) | 10 ms'de IMU kesilmesi → çekirdeğin arıza güvenliği (02 §4: `PASSIVE_SURFACE`); 500 ms'de `KOPUK`, hazırlık tamponu boşaltılır, `IDA_HELLO` beklenir |
| Sim duraklatıldı (HIL) | Kalp atışı ve `TIMESYNC` sürer (G/Ç iş parçacığı), sensör akışı durur. Çalıştırma geçersiz işaretlenir; 08 kullanıcıyı önceden uyarır | IMU kesilmesi → arıza güvenliği; bağlantı kopuk sayılmaz |
| Sim sürdürüldü | `step_seq` ve sim zamanı kesintisiz devam eder | büyük duvar boşluğunu görür; mandallı arıza eylemleri açık sıfırlama ister (02 §2.6) |
| Sim çöktü ya da yeniden başladı | yeni süreç yeni `run_id` ile kartı sıfırlayarak başlar | bağlantı kaybı → arıza güvenliği |
| Kısa kaçırma (bariyer son tarihi) | son komutla ilerler, sayar (2.4) | etkilenmez; gecikmiş yanıtı gönderir |

"Son komutla ilerleme" yalnız bağlantı canlıyken kaçırılan adımlar içindir; bağlantı kaybı ve kart sıfırlanmasında "darbe yok" uygulanır. Gerçek donanımda PWM zamanlayıcısı kısa bir aksamada son değeri üretmeyi sürdürür; sıfırlamada ise çıkış durur (Bölüm 8, iskeletle ilişkisi).

### 2.8 SIL taşıması

- SIL koşucusu aynı mesajları **TCP loopback** (`127.0.0.1`, `TCP_NODELAY`) üzerinden alır ve gönderir. Seçim iki işletim sisteminde aynı API ile çalıştığı için yapıldı. Yerel soketin gecikmesi ölçülmedi (03.9); SIL beklediği için yalnız koşu hızını etkiler.
- **Tek kod yolu, sim tarafı:** oturum, parti kurucu, toplu zamanlayıcı, bariyer, emülatör ve hata enjektörü kipten bağımsızdır. Yalnız `ITransport` uygulaması (`UsbCdcTransport`, `SerialTransport`, `TcpTransport`) ve `LockstepDriver`'ın son tarih politikası (SIL'de yok, HIL'de `D_n`) değişir.
- **Tek kod yolu, kart tarafı:** kart yazılımı ve SIL koşucusu aynı `protocol/ida_link` modülünü bağlar. SIL baytları gerçekten kodlar ve çözer, yapıları süreç içinde doğrudan aktarmaz; böylece kodlayıcı her SIL koşusunda sınanır.
- SIL'de taşıma gecikmesi, kayıp ve bozulma hata enjektörüyle eklenebilir (2.10); bu, HIL zamanlamasının provasıdır.

### 2.9 Bağlantı emülatörü ve yer istasyonu bağlantısı

**Yeri.** Sim sunucusunun içinde, GCS ağ geçidi ile A1 çoklayıcısı arasında, yalnız GCS düzleminde (2.1 çizimi). İki yön ayrı modellenir. Emülatör sim zamanıyla sürülür ve kendi Philox akışını kullanır (04 §2.13: akış kimliği bağlantı ve yön başına). Dünya iş parçacığı her kontrol adımında emülatöre bir `LinkGeometry` yayınlar: anten fazı merkezinin yerel yüzeye göre yüksekliği (`IEnvironmentView::surface_elevation`, 04 §3), yer istasyonu antenine uzaklık, kullanılan kablo boyu. 11'deki "HIL'de gerçek bağlantının taban gecikmesini çıkar" önerisine gerek kalmaz: emülatör kartın USB yolunun önünde durur ve modellenen gecikme sim zamanında uygulanır. USB'nin kendi gecikmesi HIL düzlemindeki son tarih denetimine aittir.

**Model parametreleri** (yön ve bağlantı başına; 11 §A.3 ve düzeltmeleri; değerler A5 senaryosunda):

| Parametre | Anlam |
|---|---|
| `latency_base`, `jitter` | Dağılımlı gecikme. Seri telsiz ve kablo profillerinde serbest bırakma zamanı azalmayan tutulur, yani sıra korunur; yalnız datagram profilinde sıra bozulmasına izin verilir |
| `bandwidth` | Serileştirme süresi: `bayt × 8 / hız`. Kuyruk `queue_limit` bayt; dolunca sondan düşürme |
| Kayıp | Bernoulli (L1) ya da **zamana bağlı Gilbert–Elliott** (L2). Sabit `Δ_e = 10 ms` sim zamanı ızgarasında `P(G→B) = 1 − e^{−λ_GB·Δ_e}`, `P(B→G) = 1 − e^{−λ_BG·Δ_e}`. Ortalama kötü durum süresi `1/λ_BG` saniyedir ve telemetri hızından bağımsızdır (11.15). Çerçeve, gönderim anındaki durumun başarı olasılığıyla (`k`, `h`) geçer |
| `outage` | zamanlanmış ya da olaya bağlı kesinti (11 §B.10 md.1–7; senaryo biçimi 08) |
| `quality(geometri)` | telsizde marj modeli (aşağıda); kabloda uzunluk ve kopma; akustikte ayrı profil |

**Telsiz kalitesi** ("anten battı ⇒ kopuk" kuralının yerine; 11.2):

`M(t) = P_tx + G_tx + G_rx − FSPL(d, f) − L_su(f, T, S | σ) · d_su(t) − L_uyumsuz(t) − S_rx`

- `FSPL = 20·log₁₀(d) + 20·log₁₀(f) − 147,55` (standart serbest uzay; yüzey yansıması ve engel ilk sürümde yok).
- `d_su = max(0, −h_anten)`: anten üstündeki su yolu, dalga dahil.
- `L_su` dB/m, ITU-R P.527-6 karmaşık geçirgenlik modelinden ve tam iletken-dielektrik formülünden senaryo yüklenirken hesaplanır: `α = ω·√(με'/2)·√(√(1 + (σ_eff/(ωε'))²) − 1)`, `L = 8,686·α` (11.1). İyi iletken yaklaşımı kullanılmaz.
- Kabul referansı (Tablo 11.1b): deniz suyu 20 °C, 35 ppt için 433 / 915 / 2400 MHz'te 663 / 826 / 1084 dB/m; 0,01 S/m tatlı su örneğinde 10,2 / 39,3 / 256,9 dB/m.
- `L_uyumsuz`: batmış antenin uyumsuzluk ve hava-su sınırı kaybı. Parametredir, değeri bilinmiyor (M25).
- Paket başarısı `k(M) = 1 / (1 + e^{−(M − M₅₀)/w})` (öneri), G durumunun başarı olasılığına bağlanır.
- Marj eşiğin üstüne döndükten sonra bağlantı `reacquire_time` boyunca kapalı kalır (11 §A.2; 1–3 s tahmin, doğrulanmadı; M25).
- Hassasiyet ve hızlar varyant ve firmware'e bağlıdır (11.14). 0,5–1 km engelli görüş hattı değeridir.
- İkili kopma kuralı yalnız "kaba" profil olarak seçilebilir (11.2). Örnek büyüklükler: tatlı suda (0,01 S/m) 433 MHz'te 10 cm su yaklaşık 1 dB, deniz suyunda yaklaşık 66 dB.

**Profil türleri** (uygulama mesajı kaybı ile taşıma bozulması ayrı; 11.13, D11 N3):

| Profil | Fiziksel karşılık | Davranış |
|---|---|---|
| `datagram` | SiK/RFD telsiz, UDP | MAVLink çerçevesi bütün olarak düşer (yarım çerçeve kaybı senkron testine dönüşmesin diye; 11 §A.5) |
| `stream` | kablo üzerinde TCP (Fathom-X Ethernet) | çerçeve düşmez. Kayıp kesimi akışı `RTO` kadar durdurur ve arkasındaki bütün baytlar gecikir (kafa-kuyruk tıkanması). `RTO` parametredir: RFC 6298 alt sınırı ile işletim sistemi uygulamaları farklıdır (doğrulanmadı; M25). Uzun kesintide bağlantı kopar ve yeniden kurulur. Kablo: 80 Mbit/s pratik, 300 m test edilmiş (11.5) |
| `corrupt` | bit hatası | bit hata oranıyla bayt bozma → CRC reddi → ayrıştırıcının yeniden senkronu; ayrıştırıcıyı sınar |
| `acoustic` | M16 sınıfı modem | ilk sürümde yok. MAVLink `HEARTBEAT` en az 21 B'tır ve 1 Hz'te 168 bit/s ister (11.9); M16'nın 10 bit/s hızına sığmaz, ayrı sıkıştırılmış ileti kümesi gerekir. Yayılma gecikmesi koşulsuz `r/c` ile eklenmez (Ç13); hedef modem A18'e bağlı. M64 satışta değil (11.16) |

Kademeler 11 §A.3'teki gibidir: L0 ideal (SIL başlangıcı), L1 gecikme, bant ve Bernoulli, L2 zamana bağlı GE ve kalite. L3 akustiktir ve açık kalır.

**Yer istasyonu ağ geçidi.**
- Laboratuvar uygulaması araçla MAVLink konuşmaz. Web panelinin komutları A3 üzerinden sim sunucusuna gelir; ağ geçidi bunları `sysid 254` ile MAVLink'e kodlar ve emülatöre verir. Gelen telemetriyi çözüp A3'e yayınlar (panelin kendisi 08'in işidir).
- Hazır yer istasyonu için ağ geçidi MAVLink/UDP uç noktası açar (QGC varsayılanı 14550). QGC'nin çerçeveleri (`sysid 255`) aynı emülatörden geçer; iki istemci aynı telsiz kanalını paylaşır.
- **QGC uyumunun kapsamı:** bağlantı ve `HEARTBEAT`, tutum ve konum gösterimi, parametre listesi indirme ve yazma, standart yol noktalarıyla görev yükleme.
- **Sınırı:** genel otopilot eklentisinde mod değiştirme ve guided işlemler desteklenmez (`isCapable()` `false` döner); hibrit yüzey/dalış kipleri, özel görev öğeleri ve atış komutları QGC'de ifade edilmez (06.12, 09.15). Bunlar laboratuvar uygulamasının işidir. QGC'nin hedef mi prova mı olduğu kullanıcı kararıdır (00 A6). Kabul ölçütü ayrıdır (M18).
- Değerlendirme için sim, GCS düzlemi telemetrisini emülatöre girmeden önce de A6'ya yazar; emülatör kaybı ile kartın davranışı ayrışır.

### 2.10 Hata enjektörü

Taşıma ile ayrıştırıcı arasında, sim tarafında, iki yön için ayrı çalışır; HIL ve SIL'de aynıdır. İşlemler: çerçeve düşürme, çoğaltma, komşu çerçeveleri yer değiştirme, bayt bozma, kesme, gecikme sıçraması, çöp bayt ekleme. Hangi düzlemde ve hangi mesaj türünde uygulanacağı seçilir; her işlem tohumlu ve sayaçlıdır. Kartın aldığı baytlar yazımdan önce, kartın gönderdikleri sim tarafındaki alımda bozulur. Fiziksel USB'de enjeksiyon yapılmaz. Bu katman bağlantı emülatöründen ayrıdır: emülatör yer istasyonu bağlantısının **modelidir**, enjektör protokolün **sınanmasıdır**.

---

## 3. Arayüzler

**A1** (iskelet §3): içerik 2.2, taşıma 2.1 ve 2.8. Sözleşme işinde dondurulacaklar: `ida.xml`, mesaj kimlikleri (0–255 kullanılmaz, 06 §2.7; `common` ve `ardupilotmega` ile çakışmazlık `mavgen` ile denetlenir), `sysid`/`compid` değerleri, `dialect_hash` algoritması, `flags` bit tabloları.

**Kart tarafı C modülü** (`protocol/ida_link`; 01 entegre eder, SIL koşucusu da bağlar):

```c
void        ida_link_init(ida_link_t *l, const ida_link_cfg_t *cfg, ida_wtime_t now_w);
void        ida_link_rx(ida_link_t *l, const uint8_t *b, size_t n, ida_wtime_t now_w); /* ISR dışında */
bool        ida_link_step_ready(const ida_link_t *l);
ida_link_rc ida_link_take_step(ida_link_t *l, ida_step_in_t *in);       /* OK, DUP, OLD, GAP(k) */
size_t      ida_link_put_step_out(ida_link_t *l, const ida_step_out_t *o,
                                  uint8_t *tx, size_t cap);              /* ACTUATOR önce */
size_t      ida_link_poll_tx(ida_link_t *l, ida_wtime_t now_w, uint8_t *tx, size_t cap);
ida_link_st ida_link_state(const ida_link_t *l);                         /* KAPALI … KOPUK */
```

`ida_link_cfg_t`: `ctrl_rate_hz`, `tx_budget_per_step`, `max_ping360_samples`, `boot_id`, `reset_cause`, sürüm alanları, `us_from_thr` (normalleştirilmiş itici komutunu µs'ye çeviren platform işlevi; 01'den beklenen). Modül bellek ayırmaz; MAVLink kanal durumu ve alıcı tamponu statiktir (08.10; `MAVLINK_COMM_NUM_BUFFERS = 1`, 08 §2.7). `DUP` durumunda 02 adımı koşmaz, modül önceki çıktıyı yeniden kodlar.

**Sim tarafı (`sim/hil/`):**

```
class ITransport  { bool open(const Endpoint&); void close(); size_t write(span<const uint8_t>);
                    size_t read(span<uint8_t>, Duration timeout); LinkEvent poll_event(); };
class HilSession  { State state() const; void on_bytes(...); RunValidity validity() const; };
class StepBarrier { void emit_critical(StepId n, WallTime not_before);
                    optional<ActuatorFrame> await(StepId n, Deadline d); };      // 04 LockstepDriver çağırır
class BulkScheduler { void enqueue_ping360(const Ping360Profile&); size_t fill(StepId n, size_t budget); };
class LinkEmulator  { void submit(Dir, Frame, SimTime); vector<Frame> release(Dir, SimTime);
                      void set_geometry(const LinkGeometry&); };
class FaultInjector { void configure(const FaultSpec&); };
```

**Diğer bölümlerden beklenenler:**
- **01:** USB CDC ve VCP sürücüsü; bayt G/Ç ile `ida_link_rx` arasındaki tampon (kesme içinde ayrıştırma yok); `us_from_thr`; Ping360 tamponlarının bellek yeri; `reset_cause` bayrakları; `rx_to_tx_us` ölçümü.
- **02:** `ida_step_in_t` sınırları (bunlar adım partisinin sınırıdır); `DUP_SEQ` davranışı; IMU yokken `ida_core_tick_wall`.
- **03:** `IDA_FC_COMMAND`, `IDA_FC_COMMAND_ACK`, `IDA_FC_STATUS` alanları; ateşleme seviyesinin en kısa süresi; arka plan algılama sonucunun sabit adım ötelemesiyle tüketilmesi.
- **04:** `ActuatorFrame.t_sim_ns = t_n + L_act`; "darbe yok" girdisinde ESC davranışı; `LockstepDriver`'ın `StepBarrier` ile bağlanması; `LinkGeometry` için anten konumu.
- **05:** her sensörün ölçüm ve teslim zamanı; DVL `t_report`; Ping360 ping zamanlaması ve `IDA_PING360_CONFIG`'in yorumu; kademe başına `N_kademe`; IMU'nun adım aralığı ortalaması. Birden çok IMU örneği tek pakette biriktirilmez; biriktirme ilk örneğe bekleme ekler (05.6).
- **07:** kamera algılamasının modellenmiş teslim zamanında hazır olmaması durumunda düşürme kuralı (00 Bölüm 6).
- **08:** sayaçların ve geçerlilik bayrağının A3'te gösterimi; HIL'de duraklatma uyarısı; A1 trafiğinin A6 kaydı; emülatör ve hata profillerinin A5 biçimi.

---

## 4. Hata durumları ve davranışı

| Durum | Tespit | Davranış |
|---|---|---|
| CRC hatası, bozuk çerçeve | MAVLink ayrıştırıcısı | çerçeve atılır, sayaç; sonraki `0xFD` başlangıç baytında yeniden senkron; MAVLink başlık `seq` boşluğu kaynak başına sayılır |
| Partide eksik çerçeve | `n_aux` uyuşmazlığı | `act_flags.girdi_eksik`; bariyerli çalıştırma geçersiz, 1x'te yalnız sayaç |
| IMU yinelemesi, eski IMU, sıra boşluğu | `step_seq` | DUP / OLD / GAP (2.4) |
| Son tarih kaçırıldı | sim, `D_n` | son komut, `kaçırılan_adım`, geçersiz |
| Geç, yinelenmiş ya da beklenmeyen `IDA_ACTUATOR` | sim, `seq` | kaydedilir, uygulanmaz |
| Kart hazırlık tamponu doldu | kart | yeni örnek atılır, `girdi_eksik`. Sim 02'nin sınırlarına uyduğu için oluşması bir hatanın göstergesidir |
| Ping360 eksik ya da toplu kuyruk taşması | kart / sim | profil atılır; `ping360_eksik` / `bulk_drop`; bariyerde geçersiz |
| Sürüm, şema ya da hız uyuşmazlığı | `IDA_HELLO` | çalıştırma başlamaz (2.7) |
| Kart sıfırlanması, kablo çıkması, sim duraklaması ya da çökmesi | 2.7 | 2.7 |
| İşletim sisteminin sim sürecini ya da USB yığınını bekletmesi (uyku, güç tasarrufu, zamanlayıcı) | `D_n` kaçırmaları, `TIMESYNC` sıçraması | sayılır; HIL koşusu öncesinde uyku ve USB seçici askıya alma kapatılır. Etkileri ölçülmedi; M1'in senaryosuna girer |
| GCS düzlemi A1 payını aştı | ağ geçidi | emülatör kuyruğunda bekler ya da düşer (modelin parçası); HIL düzlemi etkilenmez |
| Mesaj alanı aralık dışı, NaN | `ida_link` çözücüsü | atılır, sayılır (02 §4 girdi hataları) |

Bariyerli bir çalıştırmanın **geçerli** sayılması için: `kaçırılan_adım = 0`, `girdi_eksik = 0`, `bulk_drop = 0`, planlanmamış sıfırlama yok, duraklatma yok. Sayaçlar A3'te canlı gösterilir ve A6'ya yazılır (Ö11).

---

## 5. Kalite kademelerinin etkisi

HIL ve protokol her kademede tam çalışır (iskelet §6). Dolaylı etkiler:
- **Sonar:** Ping360 örnek sayısının üst sınırı `N_kademe` (2.5); bant genişliği buna orantılıdır (1200 → 1.324 B, 600 → 692 B profil başına).
- **Kamera kapalı:** dışarıdan algılama gelmez; mesaj kümesi değişmez, çekirdek ve 03 sıfır algılamayla çalışır (02 §5).
- **Kayıt:** bariyerli bir HIL koşusunun yeniden oynatılabilmesi için A1'in her iki yönü kaydedilmelidir; tahmini yük yaklaşık 130–270 kB/s'dir (2.3). "Yalnızca olaylar" kademesinde koşu yeniden oynatılamaz; 08'e öneri olarak bildirilir.
- Emülatör kademeleri (L0–L3) kalite kademesi değil, senaryo seçimidir.

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| A1 taşıması | USB HS CDC (TinyUSB) | ST-LINK VCP ≥ 2 Mbaud, **200 Hz ile birlikte** | M1, M2, M3 |
| A1 biçimi | MAVLink 2 + `ida` dialect | açık serileştirmeli özel biçim (aşağıda) | M9 |
| `L_act` | 1 fizik adımı (1,25 ms) | 2 fizik adımı (2,5 ms) | M1, M17 |
| Bariyerli kontrol hızı | 400 Hz | 200 Hz | M17, M5 |
| Toplu veri bütçesi `B_bulk` | USB: 1.600 B/adım | VCP: baud'dan hesap | M1, M3 |
| Kanal ayrımı | tek akışta çoklama | USB'de ikinci CDC arayüzü (GCS düzlemi) | M1 (kafa-kuyruk tıkanması gözlenirse) |
| Geciken adım politikası (A8) | son komutla ilerle, say, geçersiz | — (bekleme 1x'te fiziksel zamanı durdurmaz, Ç1) | M17 |
| Emülatör parametreleri | üretici ve literatür değerleri | ölçülmüş değerler | M25 (telsiz, kablo, `reacquire_time`, `L_uyumsuz`, `RTO`), M24 (akustik) |
| SIL taşıması | TCP loopback | Unix alan soketi / adlandırılmış boru | yeni ölçüm önerisi: SIL soket gidiş-dönüşü ve koşu hızı (Bölüm 8) |

**Yedek biçimin gerekleri** (08.11, 06.14, D06 N3):
- **Çerçeve:** COBS ve `0x00` ayırıcı. COBS bütünlük denetimi eklemediği için her çerçevede CRC-32C (ya da CRC-16-CCITT) taşınır (06.14). En büyük çerçeve 1.400 B'tır; bu sınırla Ping360 profili tek çerçeveye sığar ve parçalama kalkar. Bu biçimin MAVLink'e karşı gerekçelerinden biri büyük paket ihtiyacıdır (08.10).
- **Başlık:** `proto_major`, `proto_minor` u8; `msg_type` u16; `length` u16; `step_seq` u32; kaynak başına çerçeve sırası u16.
- **Alan kodlaması:** ham yapı kopyalama yoktur; her alan açıkça kodlanır ve çözülür. Sabit genişlikli tamsayılar ve little-endian kullanılır. Kayan nokta IEEE-754 binary32/64'tür; bu koşul derlemede denetlenir, bit deseni tamsayı üzerinden aktarılır. `bool` u8'dir ve yalnız 0/1 kabul edilir. Enum'lar aralık denetimlidir. Diziler uzunluk önekli ve azami boyutludur. Padding, hizalama ve enum boyutu tel biçimine girmez (08.11).
- **Sürümleme:** el sıkışmada 64 bit şema özeti (kanonik şema metninden). Bilinmeyen `msg_type` atlanır; `minor` artışında yalnız sona alan eklenir; `major` uyuşmazlığında reddedilir.
- **Tek kaynak:** kodlayıcı ve çözücü (C ve C++) tek bir şema dosyasından üretilir; üretici seçimi açıktır (Rust tabanlı araç kullanılmaz, K3).
- **GCS düzlemi:** MAVLink kalır. Yedek biçimde GCS çerçeveleri `GCS_TUNNEL` türünde opak yük olarak taşınır.
- **Değişmeyenler:** oturum, bariyer, toplu zamanlayıcı ve emülatör biçimden bağımsızdır; yalnız kodlayıcı katmanı değişir.

M9'un karşılaştırması aynı anlamı taşıyan iki aday üzerinden yapılır: kartta ayrıştırma ve kodlama çevrimi (DWT), RAM ve flash (`mavlink_message_t` ve kanal durumu dahil; 08 §2.7'de sayı doğrulanmadı), Ping360 parçalama maliyeti, uçtan uca gecikme ve bakım yükü.

---

## 7. Test yaklaşımı ve kabul ölçütleri

**Protokol uygunluk testleri** (her iki işletim sisteminde ve kartta):
- Her mesaj için kodla-çöz gidiş-dönüşü, sınır değerler ve NaN reddi.
- C ile kodlanan baytların C++ ile çözülmesi ve tersi; depoda tutulan altın bayt dizileriyle karşılaştırma.
- `dialect_hash` tutarlılığı; bir mesajın düzeni bilerek değiştirildiğinde `CRC_EXTRA` reddi ve el sıkışma reddi.
- Ping360: `n_samples` ∈ {200, 250, 251, 252, 502, 1199, 1200}; sonu sıfır olan veri; son parçanın kırpılması; eksik parça, yanlış `profile_seq`, ikinci HEAD.

**Oturum testleri:** başarılı el sıkışma; `major`, şema ve hız uyuşmazlığı; çalıştırma başında yeniden başlatma; koşu ortasında yazılım ve pin reseti; kablo çıkarma (USB hattı yazılımla kesilebilen bir hub ile; araç seçimi açık); HIL'de duraklatma ve sürdürme; sim sürecinin öldürülüp yeniden başlatılması.

**Geri döngü (M1'in protokol tarafı):** `IDA_LOOPBACK` ile 64 ve 256 B yük, 200 / 400 / 1000 Hz, en az 30 dakika, macOS ve Windows; p50 / p99 / p99,9 / en büyük gidiş-dönüş histogramı. Aynı ölçüm SIL'de TCP loopback için de yapılır.

**Bariyer testleri** (önce SIL koşucusu ve hata enjektörüyle, sonra kartta):
- Gecikme dağılımı taraması: kaçırmalar yalnız ve tam olarak gecikme payı aştığında sayılır.
- Uygulama anları her koşulda `t_n + L_act`.
- DUP, OLD ve GAP davranışı; `n_aux` ile eksik girdi tespiti.
- Aynı senaryonun iki SIL koşusunda A6 kayıtları bit düzeyinde aynı.
- Kaçırma sayacı sıfır olan iki bariyerli HIL koşusunda, insan girdisi olmadan ve 03 arka planı sabit ötelemeyle çalışırken, `IDA_ACTUATOR` dizileri aynı.

**Hata enjeksiyonu:** düşürme, çoğaltma, sıra bozma, bozulma, kesme ve çöp bayt her yönde ayrı ayrı. Kabul ölçütü: enjekte edilen her olayın ilgili sayaçta tam olarak bir kez görünmesi; ayrıştırıcının bir sonraki geçerli çerçevede senkrona dönmesi; kartın hiçbir enjeksiyonda kilitlenmemesi ve gözetim zamanlayıcısını tetiklememesi.

**Emülatör testleri:**
- Bernoulli kayıp oranı güven aralığında.
- GE'de ortalama kötü durum süresi (saniye) iki farklı telemetri hızında aynı (11.15'in düzeltmesi).
- Sıra korunan profillerde sıra bozulması sıfır; ölçülen çıkış hızı `bandwidth`'i aşmıyor; kuyruk taşmasında düşürme.
- `L_su` birim testi Tablo 11.1b değerlerini (663 / 826 / 1084 ve 10,2 / 39,3 / 256,9 dB/m) %1 içinde üretiyor.
- `stream` profilinde kayıp çerçeve düşürmüyor, `RTO` kadar durma üretiyor.
- Anten batıp çıkınca `reacquire_time` uygulanıyor.

**QGC (M18):** bağlantı, araç türünün denizaltı görünmesi, parametre indirme ve yazma, standart görev yükleme. Kip değişimi ve hibrit kipler kabul ölçütü değildir (09.15).

**Kabul ölçütleri (hedef, ölçülmedi):**
- Uygunluk ve hata enjeksiyonu testlerinin %100'ü geçer.
- USB HS'te 400 Hz bariyerli 30 dakikalık koşu iki işletim sisteminde kaçırmasız tamamlanır. Tamamlanamazsa önce `L_act` = 2,5 ms, sonra 200 Hz denenir; karar M17'de kayda geçer.
- Ölçülen sürdürülebilir kapasitenin en çok %50'si kullanılır (2.3'teki en kötü toplamla).
- Kablo yeniden takıldıktan sonra 3 s içinde `HAZIR` durumuna geçilir.

---

## 8. Açık noktalar ve iskelete öneriler

**İskeletle çelişkiler ve öneriler** (bölüm iskelete uydu; aşağıdakiler mimarın kararına sunulur):

1. **"Son komutla ilerler" kuralının kapsamı.** İskelet §4 bariyer kaçırmasında sim'in son komutla ilerlemesini ister. Bölüm bunu kaçırmalar için uyguladı. Bağlantı kaybı ve kart sıfırlanması bir kaçırma değildir; gerçek donanımda PWM durur. Bu yüzden bu iki durumda "darbe yok" uygulandı (2.7). Öneri: iskelet §4'e "bağlantı kaybı ve kart sıfırlanmasında aktüatör girdisi sinyal kaybıdır" notu eklenmesi.
2. **Yedek taşıma ile yedek kontrol hızının bağı.** İskelet §8'de A1 taşıması ve kontrol döngüsü ayrı satırlardır. Hesap (2.4), VCP'de 400 Hz bariyerin en kötü partide sığmadığını gösteriyor. Öneri: §8'e "VCP yedeği 200 Hz yedeğini gerektirir" notu.
3. **Bariyerin tanımı.** İskelet "sim her sensör çerçevesinden sonra aynı sıra numaralı aktüatör yanıtını bekler" diyor. Öneri: "her kontrol adımı partisinden (IMU ile sonlanan) sonra" ve "yanıt sim zamanında sabit `L_act` gecikmesiyle uygulanır" biçiminde netleştirilmesi.
4. **Aktüatör gösterimi.** 02 normalleştirilmiş `thr_cmd`, 04 `pwm_us` kullanıyor. A1 `pwm_us` taşır; çeviri kartın platform katmanındadır (01). İskelet §3 A1 satırına "aktüatör komutları ESC darbe genişliği olarak" eklenmesi önerilir.
5. **Yeni dış arayüz.** Hazır yer istasyonu için MAVLink/UDP uç noktası A1–A7'de yok. Öneri: "A8: sim sunucusu ↔ harici yer istasyonu (MAVLink/UDP, emülatör arkasında)".
6. **Kalite kademesi tablosu.** İskelet §6'nın sonar satırına "Ping360 örnek sayısı üst sınırı" eklenmesi önerilir.
7. **HIL'de duraklatma.** İskelet §4 tablosuna "HIL kiplerinde duraklatma çalıştırmayı geçersiz kılar" eklenmesi önerilir (02 §4 de bunu 06'dan bekliyordu).
8. **A1 ↔ A4 dönüştürücüsünün sahibi** (02 §8.2 md.4): öneri 06'dır. `protocol/ida_link` modülünü 06 tanımlar, 01 ve SIL koşucusu bağlar.

**Açık noktalar:**
- QGC hedef mi prova mı (00 A6); yer istasyonu bağlantı türleri, kablo tipi ve uzunluğu (A20); akustik modem (A18).
- 6 eksenli elle kumanda için standart mesajın yeterliliği.
- `sysid` değerleri, mesaj kimliği aralığı ve `dialect_hash` algoritması sözleşme işinde sabitlenecek.
- Yeni ölçüm önerileri: (a) SIL TCP loopback gidiş-dönüşü ve koşu hızına etkisi; (b) M9'a ek olarak kartta 400 Hz'te ayrıştırmanın işlemci payı (USB alım kesmeleri adım süresine eklenir; M5 ile birlikte); (c) işletim sisteminin uyku ve USB güç yönetimi ayarlarının gidiş-dönüş kuyruğuna etkisi (M1 senaryosuna eklenebilir).
- Ping1D, batarya ve sızıntı hızları tahmindir; 05 kesinleştirir. Algılama hızı kamera kararına (A24) bağlıdır.
- Kayıt kademesinin bariyerli koşuların yeniden oynatılabilirliğine etkisi 08 ile birlikte karara bağlanmalı.
