# 06 — Protokol ve HIL köprüsü

Sürüm: 0.2 (2026-11-13). Durum: taslak; ikinci inceleme bekliyor. Sürüm 0.2: iskelet 0.6'ya uyum (0.5–0.6 ekleri: açılış sayacından türeyen olay kimliği ve olayın son geçerlilik süresi; sonar satırında gönderim anı; sanal emniyet girdileri ve fırlatıcı durumu; A8 üzerinden gelen kamera algılamaları) ve [inceleme/06-sol.md](inceleme/06-sol.md) bulgularının giderilmesi; madde madde yanıt [inceleme/06-yanit.md](inceleme/06-yanit.md). Başlıca değişiklikler: IMU'dan bağımsız zaman çerçevesi ve kimlikli parti üyeliği; sürekli komutun kullanım süresi ve güvenlik nesli; kimlikli, onaylı tek seferlik olaylar; bütün özel mesajlara bağlı oturum kimliği; 01 ile ortak uçtan uca zaman çizelgesi; tamamlanmış mesaj kümesiyle yeniden hesaplanan bütçeler.

Bu bölüm [00-iskelet.md](00-iskelet.md) Bölüm 9, satır 06'nın kapsamını yazar. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir ("00"). Atıf: `NN.k` = 00 Bölüm 4 düzeltme satırı, `Çn` çelişki çözümü, `Mn` ölçüm, `Kn`/`Ön` karar, `00/An` 00'daki açık soru, `DNN` = `docs/research/dogrulama/NN-codex.md`, `NN §x` = ham rapor bölümü, `0X §y` = bu dizindeki mimari bölüm.

**Bu bölümdeki hiçbir gecikme, bant genişliği ya da işlemci süresi ölçülmemiştir.** Bütçeler mesaj boyutlarından, komşu bölümlerin tahminlerinden ve nominal kapasitelerden türetilir; "tahmin" ya da "hedef" diye işaretlenir. Kod yoktur; alan listeleri ve imzalar sözleşme işinin girdisidir (iskelet §3).

---

## 1. Amaç ve sorumluluklar

1. **A1'in içeriği:** sim sunucusu ile kart (ya da SIL koşucusu) arasındaki bütün mesajların adı, yönü, hızı, alanları, zaman anlamı ve A4 port alanlarıyla eşlemesi (iskelet §3).
2. **Biçim:** MAVLink 2 + özel dialect `ida` (Ö2, Ç3); M9 yedeği olan açık serileştirmeli biçimin gerekleri.
3. **Zaman çerçevesi ve adım partisi:** partinin kuruluşu, üyelerinin doğrulanması, kapanışı (iskelet §4).
4. **Çıkışlar:** sürekli komutun kullanım süresi, eş zamansız güvenli çıktı, kimlikli tek seferlik olaylar (iskelet §3.2).
5. **Adım bariyeri:** son tarih, uçtan uca zaman çizelgesi, kaçırma politikasının uygulanması (iskelet §4, Ç1, Ö11).
6. **Oturum:** el sıkışma, şema doğrulaması, oturum kimliği, canlılık, kopma ve yeniden bağlanma.
7. **Sonar profilinin taşınması**, **bağlantı emülatörü** ve **yer istasyonu ağ geçidi**.
8. **Paylaşılan protokol modülü** `protocol/ida_link` (kart ve SIL koşucusu) ve sim tarafındaki HIL köprüsü `sim/hil/`.

**Yazmadıkları:** mesajları üreten modeller (04, 05), kartın USB/UART sürücüsü, kesme düzeni, gözetim zamanlayıcısı ve bellek yerleşimi (01), çekirdeğin girdileri nasıl kullandığı (02, 03), web panelleri ve kayıt dosyası biçimi (08).

**Temel ilke:** kartta protokolü yalnız bağlantı katmanı bilir; çekirdek bayt ya da MAVLink görmez (02 §3.5). Kart, HIL 1x ile bariyerli HIL arasında hiçbir davranış farkı göstermez: her zaman çerçevesine aynı sıra numaralı tek bir aktüatör çerçevesiyle yanıt verir; kipi yalnız sim'in sürücüsü belirler (04 §2.11).

---

## 2. İç yapı

### 2.1 Alt parçalar ve veri akışı

```
              ┌───────────────────────── Sim sunucusu (C++) ──────────────────────────────┐
 Web ─A3────▶ │ GCS ağ geçidi ─▶ Bağlantı emülatörü (yalnız GCS düzlemi, sim zamanı) ─┐    │
 QGC ─UDP───▶ │ (sysid 254/255)                                                        ▼    │
              │ Dünya (04) ─▶ Sensörler (05) ─▶ Parti kurucu (zarf, idx, TF) ─▶ G/Ç iş parçacığı ─┼▶ Hata enjektörü ─▶ ITransport
              │   ▲                                                  (kritik / toplu pencere)    │    USB CDC | VCP | TCP
              │   └─ LockstepDriver ◀─ StepBarrier ◀─ Oturum + ayrıştırıcı ◀────────────────────┼◀─
              └───────────────────────────────────────────────────────────────────────────────────┘
              ┌──────────── Kart yazılımı (01) / SIL koşucusu (02) ────────────┐
              │ bayt G/Ç + kesmede alım damgası ─▶ ida_link: ayrıştırıcı, oturum,
              │ parti kapanışı, A1↔A4 eşlemesi, olay tablosu, gönderim önceliği ─▶ ida_core_step / ida_core_tick
              └────────────────────────────────────────────────────────────────┘
```

| Parça | Yer | Görev |
|---|---|---|
| `protocol/ida.xml` | depo | `common.xml`'i içeren dialect; `mavgen` çıktısı depoda tutulur (08.10) |
| `protocol/ida_link/` (C17) | kart ve SIL koşucusu | ayrıştırma, kart tarafı oturum, parti kapanışı, A1 ↔ A4 eşlemesi, olay tablosu, gönderim önceliği |
| `sim/hil/` (C++20) | sim sunucusu | taşıma uyarlayıcıları, oturum, parti kurucu, toplu zamanlayıcı, bariyer, ön denetim, hata enjektörü, GCS ağ geçidi, bağlantı emülatörü |

**İki düzlem, tek akış.** HIL düzlemi (sensör, zaman çerçevesi, aktüatör, olay, durum, oturum) emülatörden geçmez (11 §A.5); GCS düzlemi (standart MAVLink yer istasyonu trafiği) emülatörden geçer. Sim → kart yönünde bütün trafik `ida` dialect'idir: GCS çerçeveleri `IDA_GCS_IN` zarfı içinde, baytları değiştirilmeden taşınır; zarf oturumu, partiyi ve üye indisini ekler (2.3). Kart, sim'den gelen zarfsız standart çerçeveyi atar ve sayar. Kart → sim yönünde GCS telemetrisi zarfsız standart MAVLink'tir; sim mesaj kimliğine göre ayırır. Kamera algılama bileşeni sim'e A8 ile bağlanır (iskelet §3); kayıtlarını sim alır ve A1'de `IDA_DETECTION` olarak karta iletir; bileşen A1'e doğrudan erişmez. Tek akış seçildi çünkü yedek taşıma (VCP) ve SIL soketi tek kanallıdır; ikinci CDC arayüzü yedektir (Bölüm 6). Tek akışta düşük öncelikli bir çerçevenin gönderimi başladıktan sonra kesilemez; düzlemlerin birbirini etkilememesi mantıksal ayrımdan değil, pencere bütçelerinden gelir (2.5).

### 2.2 Mesaj kümesi (A1)

**Ortak başlık `H`** (sim → kart sensör mesajları, 22 B): `sess` u16 (oturum, 2.9), `step_seq` u32 (teslim edildiği parti), `idx` u8 (parti içi üye indisi, 2.3), `inst` u8, `seq` u32 (sensörün kendi örnek sayacı; 05 `MeasurementRecord.seq`, 02 §4.1 kabul kuralı), `flags` u16 (02 §3.3 bitleri), `t_meas_ns` i64. `t_meas` ölçüm zamanıdır (iskelet §3.1): anlık ölçümde geçerlilik anı; aralık boyunca toplanan ölçümde aralığın sonu, süresi gövdedeki `window_ns`'tedir. **Alım zamanı A1'de taşınmaz:** alıcı kendi saatiyle damgalar (2.8). Çerçeve = yük + 12 B (imzasız MAVLink 2; 08.10). Boyutlar sondaki sıfır kırpması olmadan en kötü durumdur.

**Sim → kart:**

| Mesaj | Hız (05 §2.2) | Gövde (`H` dışında) | Zaman anlamı | Yük / çerçeve B | Pencere |
|---|---|---|---|---|---|
| `IDA_TIME_FRAME` | her kontrol periyodu | `sess`, `step_seq`, `t_sim_ns` i64, `n_members` u8, `tf_flags` u8 (`SEQ_RESET`, `IMU_VAR`); `H` yok | `t_n` | 16 / 28 | kritik, en son |
| `IDA_IMU` | kontrol hızı (400/200 Hz) | `gyro[3]` rad/s, `accel[3]` m/s² (özgül kuvvet) f32; `window_ns` u32; `temp_c` f32 | aralık sonu = `t_n`; `window_ns` = periyot | 54 / 66 | kritik |
| `IDA_MAG` | 100 Hz [T] | `field[3]` T | örnekleme anı | 34 / 46 | 2.3 kuralı |
| `IDA_BARO` | 25 Hz [T] | `p_abs` Pa, `temp_c` f32; `window_ns` u32 | D1 penceresinin sonu + süresi (05 §2.3.3) | 34 / 46 | 2.3 |
| `IDA_GNSS` | 5 Hz [T] | `lat_e7`, `lon_e7`, `alt_mm` i32; `vel_ned[3]`, `eph`, `epv` f32; `fix_type`, `n_sv` u8 | çözüm anı | 56 / 68 | 2.3 |
| `IDA_DVL` | 2–15 Hz | `t_report_ns` i64; `vel[3]`, `fom`, `cov[6]`, `beam_dist[4]`, `altitude` f32; `dvl_bits`, `status` u8 | yansıma anı; `t_report` cihazın rapor anıdır, alım zamanı değildir (05.3, Ç13) | 92 / 104 | 2.3 |
| `IDA_PING1D` | 20 Hz [T] | `distance` m f32; `confidence` u8 (%); `ping_number` u32; `scan_start`, `scan_length` m f32 | gönderim anı `t_tx` | 39 / 51 | 2.3 |
| `IDA_BATT` | 10 Hz [T] | `voltage` V, `current` A, `temp_c` f32; `window_ns` u32 | pencere sonu + süresi | 38 / 50 | 2.3 |
| `IDA_LEAK` | 10 Hz [T] | `mask` u8 | örnekleme anı | 23 / 35 | 2.3 |
| `IDA_SAFETY_IO` | 10 Hz ve her değişimde [T] | `safety_mask` u8 (acil durdurma, silahlandırma fişi, emniyet pimi); `tube_loaded` u8 (tüp başına bit) | örnekleme ya da değişim anı | 24 / 36 | 2.3 |
| `IDA_PING360_HEAD` | profil başına 1 | `profile_seq` u32; `angle_grad`, `n_samples`, `sample_period` (25 ns), `tx_duration` µs, `tx_freq_khz` u16; `gain`, `n_frag` u8 | `t_meas` = darbenin gönderim anı `t_tx` (cihazda damga yok, 00 Bölüm 6); kutu `i`'nin yansıma anı `t_tx + i · sample_period · 25 ns / 2` (gidiş süresi; iskelet §3.1) | 38 / 50 | toplu |
| `IDA_PING360_DATA` | ⌈n/246⌉ | `sess`, `step_seq`, `idx`, `inst`, `frag_idx`; `data[246]` u8; `H` yok | HEAD'den | 255 / 267 | toplu |
| `IDA_DETECTION` | kamera 30 Hz × ≤ 4 [T, 00/A24]; A8'den gelen, 03 biçimindeki kayıt | `det_seq` u32; `values[3]`, `cov[6]`, `confidence` f32; `producer_ver` u16; `source`, `frame`, `kind`, `class`, `producer_id` u8 | yakalama anı | 73 / 85 | toplu |
| `IDA_GCS_IN` | emülatör bıraktıkça | `sess`, `step_seq`, `idx`, `len` u8; `frame[≤ 247]` (değiştirilmemiş MAVLink çerçevesi) | emülatörün bırakma anı | 8 + iç / 20 + iç | toplu |
| `IDA_EVENT_ACK` | olay başına | `sess`, `event_id` u64, `result` u8 (2.4), `t_apply_ns` i64 | uygulandığı fizik adımı | 19 / 31 | toplu, üye değil |

**Kart → sim:**

| Mesaj | Hız | Alanlar | Yük / çerçeve B |
|---|---|---|---|
| `IDA_ACTUATOR` | her kapanan partiye 1; eş zamansız güvenli çıktıda ek (2.4) | `sess` u16; `step_seq` u32; `t_meas_ns` i64 (TF'nin `t_n`'i); `pwm_us[8]` u16 (0 = darbe yok); `valid_for_ns` u32; `rx_wall_us` u32 (TF'nin alım zamanı, kart duvar saati); `safety_gen` u16; `act_flags` u16 (`CMD_VALID`, `ARMED`, `GÜVENLİ`, `EŞ_ZAMANSIZ`, `YENİDEN`, `PARTİ_EKSİK`, `ATLANAN_ADIM`, `ÖNCEKİ_SÜRE_AŞIMI`, `SÜRE_DOYDU`); `step_us`, `rx_to_tx_us` u16; `src` u8 (`STEP`/`TICK`) | 47 / 59 |
| `IDA_EVENT` | olay başına; onaya ya da süresi dolana kadar yinelenir | `sess`; `event_id` u64 (açılış sayacı + olay sayacı); `kind` (`FIRE`, `ARM_CHANGE`), `tube`, `value` u8; `step_seq` u32; `t_meas_ns` i64; `valid_for_ns` u32 (son geçerlilik süresi); `shot_id` u32 (03) | 33 / 45 |
| `IDA_NAV_STATE` | 50 Hz (en kötü 100) | `sess`, `step_seq`, `t_ns`; `p_ned[3]`, `v_ned[3]`, `q_nb[4]` (w, x, y, z; Hamilton; gövde → NED), `omega_b[3]`, `depth`, `altitude`, `p_sigma[3]`, `v_sigma[3]`, `att_sigma[3]` f32; `est_level`, `src`, `reset_counter` u8 | 113 / 125 |
| `IDA_BOARD_STATUS` | 10 Hz | `board_wall_us` u64; `boot_id` u32; `sess`; `mode` u8; `failsafe` u16; sayaçlar u32 × 6 (CRC, düşen çerçeve, eski oturum, parti eksik, yineleme, süre aşımı); `cpu_load` u8; `max_step_us`, `tx_queue_peak` u16 | 46 / 58 |
| `IDA_BOOT_RECORD` | her oturum kuruluşunda | 01 §3.4 açılış kaydı: `sw_version`, `git_short`, `reset_flags` u32 (RCC ham bayrakları, birlikte bulunabilenlerin hepsi; 11.19), `reset_cause` u8 (yorum), `boot_count`, `sysclk_hz`, `fpscr` u32; `dev_id`, `rev_id`, `sess` u16; regülatör, PLL varyantı, `TZEN`, `BOR_LEV`, PHY varyantı, bağlantı yolu, ekran, platform durumu u8 | 39 / 51 |
| `IDA_FAULT_RECORD` | varsa, oturum kuruluşunda | `CFSR`, `HFSR`, `MMFAR`, `BFAR`, yığına itilmiş PC/LR/xPSR, adım sayacı u32; `stacking_valid` u8; `fault_seq`, `sess` u16 (01 §4) | 37 / 49 |
| `IDA_DIAG` | 1 Hz | 01 §2.8 sayaç grupları; alan listesi 01'den | ≤ 255 / ≤ 267 |
| `IDA_FC_STATUS` | 10 Hz | 03'ten beklenen; alanlar tanımlı olmadığı için boyut belirlenemiyor, bütçede üst sınır | ≤ 255 / ≤ 267 |
| `IDA_DETECTION` | ≤ 40/s [T] | kart içi sonar algılamasının değerlendirme kaydı; yukarıdaki biçim | 73 / 85 |
| `IDA_PING360_CONFIG` | değiştikçe | `sess`; `n_samples`, `sample_period`, `tx_duration`, `tx_freq_khz`, `start_angle`, `stop_angle` u16; `mode`, `gain`, `num_steps`, `delay`, `auto` u8 (Ping360 #2601, #2602; D05 E) | 19 / 31 |

**Oturum ve ölçüm mesajları:**

| Mesaj | Yön, hız | Alanlar | Yük / çerçeve B |
|---|---|---|---|
| `IDA_HELLO` | iki yön; el sıkışmada 200 ms'de bir | `proto_major`, `proto_minor` u8; `hash_base`, `hash_full` u64; `role` u8 (SIM, BOARD, SIL); `sw_version`, `git_short` u32; `build_flags` u16; `boot_id`, `boot_count`, `reset_flags` u32; `reset_cause` u8; `hello_nonce`, `peer_nonce` u32; `ctrl_rate_hz` u16; `caps` u32; `max_ping360_samples` u16 | 58 / 70 |
| `IDA_HELLO_ACK` | sim → kart | `result` u8 (`OK`, `REBOOT`, `MAJOR`, `HASH`, `RATE`, `BUSY`); `peer_nonce` yankısı u32; `sess` u16; `run_id`, `caps` u32 | 15 / 27 |
| `IDA_KEEPALIVE` | iki yön, 10 Hz (el sıkışmadan sonra) | `sess` u16; `state` u8 | 3 / 15 |
| `TIMESYNC` (common) | sim başlatır 10 Hz, kart yanıtlar | duvar saatleri arası ofset ve gidiş-dönüş | 18 / 30 |
| `IDA_LOOPBACK` | yalnız ölçüm derlemesi | `seq` u32, `t_send_wall_ns` i64, `pad[≤ 243]`; kartın bağlantı sürücüsü ayrıştırmadan geri yollar | ≤ 255 / ≤ 267 |

**Şema özeti.** `hash_base`: dialect'in kanonik metninden (mesaj kimliği sırasıyla; her mesajın adı, kimliği ve **temel** alanlarının XML sırasındaki adı, tipi, dizi boyu, birimi; kullanılan enum'ların adları ve değerleri) SHA-256'nın ilk 64 biti. `hash_full`: aynı metne uzantı alanları da eklenerek. Kural: `proto_major` ve `hash_base` eşit olmalı, değilse ret. `proto_minor` eşitse `hash_full` da eşit olmalı; minor farkında yüksek sürüm, karşı tarafın uzantı alanlarını görmeyeceğini bilir (MAVLink 2 uzantı kuralı) ve isteğe bağlı yetenekler `caps` kesişimiyle açılır. Gerekçe: MAVLink'in 8 bitlik `CRC_EXTRA`'sı yalnız temel alanları kapsar ve çakışabilir; tam şema özeti değildir.

**Sarma ve doygunluk.** `step_seq`, `seq`, `profile_seq` ve olay sayacı u32'dir; karşılaştırmalar işaretli fark üzerinden seri aritmetikle yapılır (`step_seq` 400 Hz'te 124 günde, `profile_seq` 117 profil/s'de 425 günde sarar). u16 süre alanları 65.535 µs'de doyar ve `SÜRE_DOYDU` bayrağını kaldırır; u32 sayaçlar sarar, alıcı farkı 2³² modunda alır.

**GCS düzlemi** (standart `common` mesajları ve iki özel atış mesajı; 06 §3.3, 06.12). Yer istasyonu → araç (izinli liste): `HEARTBEAT`, `COMMAND_LONG` (arm/disarm, kip, görev başlat/duraklat), `MANUAL_CONTROL`, `SET_POSITION_TARGET_LOCAL_NED`, görev protokolü (`MISSION_COUNT`, `MISSION_ITEM_INT`, `MISSION_ACK`), `PARAM_REQUEST_LIST`, `PARAM_REQUEST_READ`, `PARAM_SET`, `IDA_FC_COMMAND` (alanları 03'ten). Araç → yer istasyonu: `HEARTBEAT` (`MAV_TYPE_SUBMARINE`), `SYS_STATUS` (`MAV_SYS_STATUS_SENSOR_LEAK`, 06.1), `ATTITUDE`, `GLOBAL_POSITION_INT`, `LOCAL_POSITION_NED`, `BATTERY_STATUS`, `MISSION_CURRENT`, `MISSION_REQUEST_INT`, `STATUSTEXT`, `COMMAND_ACK`, `PARAM_VALUE`, `AUTOPILOT_VERSION`, `IDA_FC_STATUS`, `IDA_FC_COMMAND_ACK`. Altı eksenli elle kumanda `MANUAL_CONTROL`'e sığmazsa `IDA_MANUAL_6DOF` eklenir (8. başlık).

**Standart mesajların yerine özel mesaj kullanılmasının gerekçesi** "MAVLink bu verileri taşıyamaz" değildir; özel mesajlar da MAVLink'tir (06.1, 08.10, Ç3). `HIL_SENSOR`/`HIL_GPS`: adım, üye ve oturum kimliği yok; aynı paketteki güncellemeler tek zaman damgası paylaşır (06.3). `HIL_ACTUATOR_CONTROLS`: normalleştirilmiş `float[16]`, kullanım süresi ve olay kimliği yok. `ODOMETRY`/`VISION_SPEED_ESTIMATE`/`OPTICAL_FLOW`: ışın geçerliliği, dip/su izi ve yansıma anı yok (06.1, 05.3). `DISTANCE_SENSOR`: ms çözünürlük, adım kimliği yok. `ENCAPSULATED_DATA`: sonar geometrisi yok (06.2). `BATTERY_STATUS` HIL düzleminde zaman damgası taşımaz; GCS düzleminde kullanılır.

### 2.3 Zaman çerçevesi ve adım partisi

Kontrol adımı `n`'in sim zamanı `t_n = n·P`; `P` = 2,5 ms (400 Hz, iki fizik adımı) ya da 5 ms (200 Hz, dört) (iskelet §4). Sim her periyotta, IMU örneği üretilsin ya da üretilmesin, `IDA_TIME_FRAME(n)` gönderir (iskelet §4; 04 §2.11 "kontrol sınırı" görevi). Zaman çerçevesi (TF) partinin sonlandırıcısıdır; IMU değildir.

**Dört an ayrıdır:**

| An | Saat | Tanım |
|---|---|---|
| Hazır olma `t_hazır` | sim | Kaydın sim içinde kesin olarak var olduğu an. Anlık ve aralık sensörlerinde üretildiği fizik adımı; sonar ve kamera algılamasında `t_teslim` (işçi ya da A8 bileşeni sonucunun deterministik garanti anı, 05 §4, 07); GCS çerçevesinde emülatöre verildiği an |
| Teslim `t_teslim` | sim | 05'in modellenmiş alım anı; GCS'de emülatörün bırakma anı |
| İletim | sim duvar | Kaydın gönderildiği pencere (aşağıda) |
| Çekirdeğe teslim | — | Kaydın partisinin kapandığı çağrı |

**Pencereler.** *Kritik pencere* `K_n`: duvar saati `W(t_n)`'de başlar (2.5); içeriği yalnız `t_hazır > t_{n−1}` olan sensör kayıtları, `IDA_IMU(n)` ve en sonda `IDA_TIME_FRAME(n)`. *Toplu pencere* `B_n`: `TF(n)`'den hemen sonra; içeriği `t_hazır ≤ t_n` olup `n+1` ya da sonraki partiye ait kayıtlar, ayrıca parti üyesi olmayan oturum mesajları ve olay onayları. Ping360, kamera algılaması ve GCS zarfları *yalnız toplu* sınıftadır.

**Atama kuralı (deterministik).** Kaydın partisi, `t_n ≥ t_teslim` olan ve kaydın iletilebildiği en küçük `n`'dir: kayıt `B_{n−1}`'de iletilebilir (`t_hazır ≤ t_{n−1}`) ya da sensör kaydıysa `K_n`'de. Sonuç: hazır olma anı erken olan kayıtlar modellenen adımda teslim edilir; yalnız toplu sınıfta hazır olma anı geç olanlar (sonar, kamera algılaması, gecikmesi bir periyottan kısa GCS çerçevesi) bir periyot ek gecikmeyle teslim edilir. Ek gecikme sim zamanıyla belirlenir, taşıma zamanlamasına bağlı değildir ve A6'ya yazılır. `B_{n−1}`'in bayt bütçesi (2.6) dolarsa toplu kayıt sonraki pencereye kayar, sensör kaydı `K_n`'e geçer; kayma sayılır ve içerik sim zamanıyla belirlendiği için yine deterministiktir. Parti kurucu 02'nin çağrı sınırlarını (16 örnek, 8 komut, adım başına tek sonar dilimi; 02 §3.8) uygular; fazlası sonraki partiye kayar ve kaydedilir.

**Üyelik.** Her üye partide benzersiz `idx` taşır (0 … `n_members − 1`; IMU dahil, TF hariç); `TF(n)` `n_members`'ı bildirir. Kart parti başına 64 bitlik üye bit eşlemi ve indis başına anahtar tutar: `(msgid, inst, seq)`; Ping360 DATA'da `seq` yerine `frag_idx`; zarfta iç çerçevenin `(sysid, compid, MAVLink seq)`'i. Aynı `idx` ve aynı anahtar: **yineleme**, atılır, sayılır, tamamlanmaya katılmaz. Aynı `idx`, farklı anahtar: **çelişki**, parti bozuk sayılır. Böylece bir üyenin kaybı ile başka bir üyenin yinelenmesi birbirini örtemez.

**Gelecek partiler.** Kart, son kapanan partiden sonraki `1 + K_ileri` parti için aşamalı yuva tutar (`K_ileri` = 4; birkaç toplu pencereye yayılan sonar profili için). `step_seq` bu aralığın dışındaysa çerçeve protokol hatasıdır, atılır ve sayılır.

**Kapanış.** Parti `n` iki durumda kapanır: (a) `TF(n)` geldi; (b) `TF(n)` gelmeden aynı oturumda `step_seq > n` olan bir TF geldi (sonlandırıcı kaybı). Tam sayılması için bit eşleminin `n_members` üyenin hepsini içermesi gerekir; değilse `PARTİ_EKSİK` bayrağıyla işlenir. Kapanan partiler sırayla işlenir; hiçbir IMU örneği atlanmaz (02 §2.2). Gelen TF'nin `step_seq`'ine göre: `son + 1` olağan; `son` **DUP** (adım koşmaz; çekirdek `DUP_SEQ` ile sürekli komutu güncel güvenlik durumuyla yeniden üretir, 02 §3.4; `ACTUATOR(n)` `YENİDEN` bayrağıyla gider, olay üretilmez); `< son` **OLD** (atılır, sayılır, yanıt yok); `> son + 1` **GAP** (araya düşen partiler (b) kuralıyla kapanır, hiç üyesi gelmemiş adımlar `atlanan_adım` sayılır).

**Çekirdeğe teslim.** IMU içeren parti `ida_core_step`'e gider. IMU'suz parti, partinin örnekleri, komutları ve zaman çerçevesiyle **hemen** `ida_core_tick`'e gider ve sonuç `ACTUATOR(n)` olarak döner (`src = TICK`; çekirdek itki üretmediyse `CMD_VALID = 0`). Kural kartta ve SIL'de aynıdır: kart her TF'ye tek yanıt verir, IMU kesilse de bariyer kilitlenmez ve IMU'suz bir periyotta gelen `DISARM` bekletilmez (iskelet §4; 02 §2.2). Kartta ayrıca donanım zamanlayıcısı ≥ 100 Hz `tick` çağırır (01); bu çağrıların çıktısı yalnız güvenli çıktıysa gönderilir (2.4). SIL'de donanım zamanlayıcısı yoktur; sanal duvar saati TF'nin `t_sim`'inden türetilir (02 §2.8). Kullanıcı duraklatmasında sim TF göndermez: SIL'de sanal saat durur, arıza üretilmez; HIL'de kartın saati sürer ve çalıştırma geçersizdir (2.9).

**Sıra.** Kritik pencerede gönderim sırası: sensör kayıtları `(t_meas, işleme sırası, inst, seq)` ile, sonra IMU, sonra TF. İşleme sırası (aynı zaman damgalı örnekler arasında IMU önce) 02 §2.2'nin işidir; iletim sırası yalnız kapanışı belirler (00 Bölüm 6: sensör sıralaması protokolün parçasıdır).

### 2.4 Sürekli komut, güvenli çıktı ve tek seferlik olaylar

**Sürekli komut (`IDA_ACTUATOR`).** `pwm_us`: çekirdeğin normalleştirilmiş `thr_cmd[8]`'i (02 §3.4) µs'ye kartın platform katmanında çevrilir (01'den beklenen `us_from_thr`; ölü bant ve uçlar parametre); sim bunu ESC modeline verir (04 §2.7.1). `valid_for_ns`: 02 §3.4'teki kullanım süresi; 04 §2.7.1'deki `komut_ömrü` ile aynı parametredir (varsayılan 50 ms = 400 Hz'te 20, 200 Hz'te 10 periyot; mimari varsayımı). Sim bir sürekli komutu ancak şunların hepsi tutarsa kabul eder:

1. `sess` güncel oturumdur;
2. `step_seq` bekleyen ya da uçuştaki bir istektir ve o `step_seq` için daha önce kabul yoktur (değilse yineleme sayılır);
3. `safety_gen`, sim'in kabul ettiği son nesilden küçük değildir (seri aritmetik; küçükse "eski nesil" diye reddedilir);
4. bariyerli kipte varış son tarihten (`D_n`, 2.5) öncedir; güvenli çıktı bu koşuldan muaftır (aşağıda).

Uygulanan komut, uygulandığı sim anından itibaren `valid_for_ns` boyunca geçerlidir. Yeni kabul olmadan süre dolarsa sim "PWM sinyali yok" durumuna geçer ve davranışı ESC modeli belirler (04 §2.7.5). Bariyer kaçırmasında "son komutla ilerle" kuralı (iskelet §4) yalnız bu süre içinde geçerlidir.

**Güvenlik nesli ve eş zamansız güvenli çıktı.** `ida_link`, çekirdeğin her çıktısını *güvenli* (`armed = 0`; ya da arıza eylemi `PASSIVE_SURFACE` veya üstü; ya da `src = TICK` ile nötr; 02 §3.4, §4.2) ya da *olağan* diye sınıflar. Olağandan güvenliye her geçişte `safety_gen`'i bir artırır; güvenli çıktılar `GÜVENLİ` bayrağını taşır. Donanım zamanlayıcısının `tick`'i güvenli çıktı ürettiğinde kart bunu `EŞ_ZAMANSIZ` bayrağı ve son kapanan partinin `step_seq`'iyle hemen gönderir. Sim:

- `GÜVENLİ` bayraklı ve nesli artıran çıktıyı her kipte, son tarihten bağımsız olarak, bir sonraki fizik adımı sınırında uygular ve kaydeder. Güvenliği artıran çıktı hiçbir zaman atılmaz.
- Nesli eskimiş olağan çıktıyı reddeder: güvenli çıktıdan önce üretilip yolda kalmış, çoğaltılmış ya da önbellekten yinelenmiş bir çıktı onu geri alamaz.
- Kart güvenli durumdan çıktığında (ör. yeniden arm) sonraki olağan çıktılar yeni nesli taşır ve olağan biçimde kabul edilir. `DUP` yanıtı da güncel nesli taşır.

**Tek seferlik olaylar (`IDA_EVENT`)** (iskelet §3.2; 02 §3.4; 04 §3 `OneShotEvent`):

- **Kimlik:** `event_id = (boot_count << 32) | olay_sayacı` (iskelet §3.2): kartın açılış sayacı ile açılış içindeki olay sayacı. Açılış sayacı her açılışta artar ve güç kesintisinden sonra da geri gitmez (01'den beklenen); böylece kart yeniden başladığında ya da bağlantı yeniden kurulduğunda eski bir olay yeni sayılmaz. 02 §3.4'teki `boot_id` (rastgele) biçimi bununla değişir (8. başlık).
- **Son geçerlilik:** her olay `valid_for_ns` taşır (değeri 03'ten; öneri 100 ms, tahmin). Sim, olayı yalnız uygulama anı `t_meas + valid_for_ns`'i (sim zamanı) geçmiyorsa uygular; geçmişse `REJECTED_EXPIRED` ile onaylar. Kart, olayın üretildiği andan (kart duvar saati) bu süre geçince yinelemeyi bırakır ve olayı süresi dolmuş sayar.
- **Gönderim ve yineleme:** kartın olay tablosu (4 yuva; 02 `events[4]`) onaylanmamış olayları tutar. İlk gönderim aynı adımın `ACTUATOR`'ından hemen sonradır; onay gelmezse aynı baytlar `T_olay` = 10 ms (tahmin; 400 Hz'te 4 periyot) aralıkla, son geçerliliğe kadar yinelenir. Bu, olay üretmek değildir: çekirdek olayı bir kez üretir, bağlantı katmanı aynı kimliği taşır.
- **Tekilleştirme:** sim, çalıştırma boyunca uygulanan `event_id` kümesini tutar; yinelenen kimliği uygulamaz ve `DUPLICATE` ile yeniden onaylar (kartın yinelemesi durur).
- **Uygulama anı:** `step_seq = n` olup `D_n`'den önce gelen olaylar `t_n + L_act`'ta, aynı adımın sürekli komutuyla birlikte uygulanır; aynı adımda `ARM_CHANGE` (emniyet kapatma) `FIRE`'dan önce gelir (iskelet §3.2; 04 §2.1 md.1). Geç gelen olay bir sonraki fizik adımı sınırında uygulanır ve zamanlaması taşımaya bağlı kaldığı için bariyerli çalıştırmayı geçersiz kılar.
- **Sanal emniyet girdileri** (iskelet §5): acil durdurma, silahlandırma fişi ve emniyet pimi sim sunucusundadır; kart onları yalnız `IDA_SAFETY_IO` ile okur ve A1'de bunları değiştiren hiçbir mesaj yoktur (yalnız 08'in A3 denetimi ve senaryo değiştirir). `FIRE` olayı, uygulama anında bu girdiler izin veriyorsa uygulanır; vermiyorsa `REJECTED_SAFE`. Fırlatıcı durumu (tüp dolu/boş) aynı mesajla geri okunur.
- **Onay** (`IDA_EVENT_ACK`): `APPLIED`, `DUPLICATE`, `REJECTED_TUBE_EMPTY`, `REJECTED_SAFE`, `REJECTED_EXPIRED`, `REJECTED_SESSION` ve uygulama anı. Ayrılma 04'ün darbe modeliyle o adımda tek anda çözülür (iskelet §5); ayrı bir tetik darbesi süresi yoktur.
- **Oturum sonu:** kart onaylanmamış olayları siler ve sayar; yeni oturumda yeniden göndermez. Yeniden bağlanmadan sonra eski bir atış isteği uygulanamaz; gerekirse 03 yeni kimlikle yeni karar verir.
- Seviye biçimli eski `launcher[]` alanı kaldırıldı.

**İzni daraltan GCS komutları.** Dönüştürücü bir partinin bütün komutlarını o partinin çağrısına verir; komutların hangi adıma uygulandığını partinin `step_seq`'i belirler. 8 komut sınırı aşılırsa (sim aşmaz; aşılması protokol hatasıdır) izni daraltanlar (`DISARM`, izin dışı kipe geçiş, `FC_ABORT`) önce yerleştirilir, kalanlar sonraki çağrıya kalır ve sayılır. Aynı çağrıda daraltan komutun atış modülünden önce uygulanması 02 §2.2 md.4'tedir. Zarfın iç çerçevesi doğrulanır: CRC, kaynak `sysid` ∈ {254, 255}, hedef (1, 1) ya da yayın (0), `msgid` izinli yukarı yön listesinde; değilse atılır, sayılır.

### 2.5 Adım bariyeri ve uçtan uca zaman çizelgesi

**Eşleme.** 1x kiplerde duvar saati `W(t) = T₀ + (t − t₀)` çalıştırma başında bir kez kurulur ve A6'ya yazılır. Sim bir pencereyi geç gönderirse bütçe kaydırılmaz; `gönderim_gecikti` sayılır.

**Uygulama gecikmesi.** `L_act` ∈ {1, 2, 3} fizik adımı (`h` = 1,25 ms) ve `L_act ≤ P`. `ACTUATOR(n)` sim zamanında sabit `t_n + L_act`'ta uygulanır (04 `ActuatorFrame`); taşıma titreşimi yörüngeye girmez. Birincil: 400 Hz, `L_act = h`.

**Son tarih.** `D_n = W(t_n + L_act + h) − C_sim`. `C_sim`: komut uygulandıktan sonraki fizik adımının, o adımın sensörlerinin ve sonraki partinin kodlanmasının süresi; 04'ün `RealtimeDriver` son tarihi `T₀ + (k+1)h` ile aynı anlamdadır (ölçülmedi; M15, M11). Yanıt bütçesi `R_maks = D_n − W(t_n) = L_act + h − C_sim` kontrol periyodundan bağımsızdır (sürüm 0.1'in 200 Hz satırındaki 3,45 ms hatalıydı).

**Ölçüm noktaları (01 ile ortak çizelge):**

| Nokta | Saat | Damgalayan |
|---|---|---|
| `W(t_n)`: kritik pencerenin planlanan başlangıcı | sim duvar | plan |
| `e₁`: `TF(n)`'nin son baytı işletim sistemine verildi | sim duvar | sim G/Ç |
| `e₂`: `TF(n)`'nin son baytını taşıyan aktarımın tamamlanması (= alım zamanı) | kart duvar | kesme (01) |
| `e₃`: `ACTUATOR(n)` bağlantı sürücüsüne verildi | kart duvar | kart; `rx_to_tx_us = e₃ − e₂` |
| `e₄`: `ACTUATOR(n)` çözüldü | sim duvar | sim G/Ç |

`S_kart = e₃ − e₂` M5'in, `RTT_taşıma = (e₄ − e₁) − S_kart` M1'in, `C_sim` M15/M11'in konusudur; koşul `e₄ ≤ D_n`.

**Bütçe (bütün terimler tahmin):**

| Terim (ms) | `L_act = h` | `2h` | `3h` | Dayanak |
|---|---:|---:|---:|---|
| `R_maks = L_act + h − C_sim`, `C_sim` ≈ 0,40 | 2,10 | 3,35 | 4,60 | iskelet §4; 05 §8 md.4 |
| − `S_kart`: alımdan yazıma 1,57 + kesme payı 0,125 | 1,70 | 1,70 | 1,70 | 01 §2.4; kesme payı %5 × 2,5 ms, periyot başına üst sınır sayıldı |
| = `RTT_taşıma` payı (iki yön, iki işletim sistemi yığını) | **0,40** | **1,65** | **2,90** | M1, M3 |

01'in 1,57 ms'si, o an koşan arka plan diliminin bitmesini (≤ 300 µs) içerir; bu sınır 01'in dilimler arası kritik iş denetimi yapmasına bağlıdır (01'den beklenen). USB HS'te kablo süresi ihmal edilebilir; pay yazılım yığınlarının ve bulk zamanlamasınındır, teslim süresi garantisi yoktur (05.6). **M1 kabul eşiği:** gerçek alışveriş boyutlarıyla (aşağı 94 B = TF + IMU, olağan en kötü 261 B; yukarı 59 B) `RTT_taşıma` p99,9 ≤ 0,40 ms ve 30 dakikalık koşuda (720.000 adım) kaçırma sıfır. p99,9 tek başına kaçırmasızlığı kanıtlamaz. Tutmazsa `L_act = 2h` (1,65 ms), o da tutmazsa 200 Hz ve `L_act = 3h` (2,90 ms).

**Uçuştaki istekler.** Aynı anda bekleyen istek sayısı `⌈(L_act + h − C_sim)/P⌉`: 400 Hz'te `L_act = h` için 1, `2h` için 2. Her isteğin kendi son tarihi vardır; yanıtlar `step_seq` anahtarlı tamponda (pencere + 2 yuva) tutulur. Kart istekleri sırayla işler; sürekli durumda `S_kart < P` gerekir.

**Fizik ile G/Ç'nin sırası.** Dünya iş parçacığı her fizik adımı `k`'da: (1) `t_k = t_n + L_act` ise `ACTUATOR(n)`'i ve olaylarını `D_n`'e kadar bekler (`StepBarrier::await`), kabul, ret ya da kaçırma kararını kaydeder; (2) `[t_k, t_k + h]` adımını ve sensörlerini hesaplar; (3) `t_k + h` bir kontrol sınırıysa partiyi kurar ve G/Ç iş parçacığına verir (`post`). G/Ç iş parçacığı taşımanın tek yazarı ve okuyucusudur: kritik pencereyi `W(t_n)`'den önce yazmaz; toplu pencereyi `TF(n)`'den hemen sonra, bir sonraki kritik pencereden `g` = 0,2 ms (tahmin) önce boşalacak bayt kadar yazar ve işletim sistemine bundan fazlasını vermez (başlamış bir çerçeve kesilemez; 267 B'lık çerçeve 2 Mbaud'da 1,34 ms, 921.600 baud'da 2,90 ms sürer). Kart yönünde aynı kural geçerlidir: kritik olmayan çıktılar `ACTUATOR`'dan sonra ve bir sonraki `ACTUATOR`'dan önce boşalacak miktarla sınırlıdır (01'den beklenen; VCP'de `hız × (P − 0,5 ms)`, tahmin). Kısmi yazımda çerçevenin kalanı sırasını korur; bir çerçeve bölünmez.

**Kaçırma politikası** (iskelet §4, Ö11; 00/A8 sorusunun bu bölümdeki uygulaması). Yanıt `D_n`'e kadar gelmezse sim durmaz: son kabul edilen komutla, kullanım süresi içinde ilerler, `kaçırılan_adım`'ı artırır ve çalıştırmayı geçersiz işaretler. Geç olağan yanıt uygulanmaz (uygulama anları sabit kalır); geç güvenli yanıt uygulanır (2.4). SIL'de son tarih yoktur; yalnız 5 s'lik gerçek duvar saati gözetimi asılmış koşucuyu yakalar. "Bekle" ve "güvenli duruma geç" kartın arıza güvenliğinin işidir (02 §4).

**HIL 1x (bariyersiz).** Mesajlar ve kabul kuralları aynıdır; sim beklemez ve yanıtı gelişinden sonraki ilk fizik adımı sınırında uygular. Geliş gecikmesi kaydedilir; geç yanıt çalıştırmayı geçersiz kılmaz (geçersizlik sim'in gerçek zamanı kaçırmasından gelir; 04 §2.11).

**Sabit teslim gecikmesi** (02 §4.1 ve §8.2 md.5'in beklentisi). Kart ortak saat olmadan "akış doğru hızda ama hep geride" durumunu göremez. Sim, `ACTUATOR`'daki `rx_wall_us` ve TIMESYNC ofsetiyle her adımda aşağı yön teslim gecikmesini `d_aşağı(n) = (rx_wall − ofset) − W(t_n)` olarak kestirir; ortalaması bir periyottan büyükse uyarır ve A6'ya yazar. Bu bilgi çekirdeğe gönderilmez (simülasyondan habersizlik, 02 §3.2).

**Bariyerin garanti ettikleri:** her partinin içeriği yalnız sim zamanıyla belirlenir; her kabul edilmiş sürekli komut tam `t_n + L_act`'ta uygulanır ya da kaçırma sayılır; kaçırma, eksik parti, yineleme ve eş zamansız güvenli çıktı sayaçları sıfır olan bir koşuda kapalı çevrim, kartın adım işlevinin girdilerine bağlı kısmı ölçüsünde tekrarlanabilir. **Garanti etmedikleri:** kartın duvar saatine bağlı davranışı (gözetim, IMU ve bağlantı zaman aşımları, GCS failsafe; Ç1, 06.8); 03'ün arka plan işinin bitiş adımı (02 §3.7'deki `bg_result` jetonu kayda girer); insan girdisinin zamanlaması; platformlar arası bit eşitliği (Ç7); USB teslim süresi (05.6); HIL'de duraklatılabilirlik.

### 2.6 Bant genişliği ve taşıma profilleri

Hesap yön başınadır (06.4); boyutlar 2.2'den, hızlar 05 §2.2'den. Ping360: 400 açı / 33 s (50 m) ve 3,42 s (1 m, Ethernet; D05 #1b) ile 12,12 ve 116,96 profil/s; profil 1.355 B (1200 örnek) ve 713 B (600 örnek) (2.7). 3,42 s'nin 1200 örneklik yapılandırma için fiziksel üst sınır olduğu kanıtlanmamıştır; en kötü durum tahmini olarak kullanılır. GCS payı yön başına ≤ 16.000 B/s (öneri; aşağı yönde zarf dahil).

| Sim → kart (B/s) | 400 Hz | 200 Hz |
|---|---:|---:|
| TF 28 × f | 11.200 | 5.600 |
| IMU 66 × f | 26.400 | 13.200 |
| MAG 46 × 100 + BARO 46 × 25 + GNSS 68 × 5 + DVL 104 × 15 + PING1D 51 × 20 + BATT 50 × 10 + LEAK 35 × 10 + SAFETY_IO 36 × 10 | 9.880 | 9.880 |
| Kamera algılaması (A8'den) 85 × 120 | 10.200 | 10.200 |
| Olay onayı 31 × 10 [T] + KEEPALIVE 15 × 10 + TIMESYNC 30 × 10 | 760 | 760 |
| GCS zarfları (üst sınır) | 16.000 | 16.000 |
| **Sonarsız** | **74.440** | **55.640** |
| + Ping360 50 m (12,12 × 1.355) | 90.864 | 72.064 |
| + Ping360 en hızlı (116,96 × 1.355) | 232.920 | 214.120 |

| Kart → sim (B/s) | 400 Hz | 200 Hz |
|---|---:|---:|
| ACTUATOR 59 × f | 23.600 | 11.800 |
| IDA_EVENT 45 × 10 [T] | 450 | 450 |
| NAV_STATE 125 × 100 (en kötü) | 12.500 | 12.500 |
| BOARD_STATUS 58 × 10 + FC_STATUS ≤ 267 × 10 + DIAG ≤ 267 × 1 | 3.517 | 3.517 |
| Kart içi algılama 85 × 40 [T] | 3.400 | 3.400 |
| KEEPALIVE + TIMESYNC yanıtı | 450 | 450 |
| GCS telemetrisi (üst sınır) | 16.000 | 16.000 |
| **Toplam** | **59.917** | **48.117** |

| Taşıma (nominal kapasite) | Aşağı, 50 m (400 / 200 Hz) | Aşağı, en hızlı | Yukarı | %50 kuralıyla azami profil/s, 1200 / 600 örnek (400 Hz; 200 Hz) |
|---|---|---|---|---|
| USB 2.0 HS bulk; iki yön **ortak** veri yolu, kuramsal 53,25 MB/s (13 × 512 B / 125 µs; belirtim değeri, bu depoda doğrulanmadı) | iki yön toplamı %0,28 | iki yön toplamı %0,55 | (toplamda) | sınırlayıcı değil |
| VCP 921.600 baud (92.160 B/s) | %99 / %78 | %253 / %232 | %65 / %52 | yok |
| VCP 2 Mbaud (200.000 B/s) | %45 / %36 | %116 / %107 | %30 / %24 | 19 / 36; 33 / 62 |
| VCP 3 Mbaud (300.000 B/s) | %30 / %24 | %78 / %71 | %20 / %16 | 56 / 106; 70 / 132 |

USB kuramsal değeri ölçülmüş CDC kapasitesi değildir; USB'de belirleyici olan gecikme ve titreşimdir (2.5). VCP'nin DK üzerindeki sürdürülebilir hızı ölçülmemiştir; ST'nin V3E için yayımladığı 15 MHz bir garanti değildir (01.3, M3). %50 kuralı ölçülen kapasiteyle yeniden uygulanır (7. başlık).

**Seri yolda kritik pencere** (8N1, bayt × 10 / baud; yanıt 59 B dahil):

| Kritik içerik | 2 Mbaud | 3 Mbaud |
|---|---:|---:|
| En az: TF + IMU = 94 B | 0,77 ms | 0,51 ms |
| Olağan en kötü: + MAG, BATT, LEAK, SAFETY_IO (teslim gecikmesi bir periyottan kısa olanlar; 05 §2.2) = 261 B | 1,60 ms | 1,07 ms |
| Tam en kötü: bütün sensörler aynı kritik pencerede = 530 B | 2,95 ms | 1,96 ms |

ST-LINK köprüsünün ve sürücüsünün kendi gecikmesi ölçülmemiştir (M3); aşağıda köprüye 1,0 ms pay bırakılır (tahmin). 400 Hz, `L_act = 2h` (pay 1,65 ms): 3 Mbaud olağan durumda 0,58 ms kalır, tam en kötüde sığmaz. 200 Hz, `L_act = 3h` (pay 2,90 ms): 3 Mbaud tam en kötüde 0,94 ms kalır; 2 Mbaud olağanda 1,30 ms kalır, tam en kötüde sığmaz. **Sonuç:** VCP yedeği 200 Hz ve `L_act = 3h` ile gelir; 2 Mbaud yalnız ön denetimin bulduğu kritik en kötü ≤ 320 B ise kullanılır. En hızlı Ping360 akışı (1200 örnek) VCP'de %50 kuralına hiçbir aday baud'da sığmaz.

**Toplu pencere bütçesi** `B_bulk`: USB'de 4.096 B/periyot (tahmin; kritik RTT'ye etkisi M1'de ölçülür). VCP'de `hız × (P − kritik süre − g)`; olağan kritikle 2 Mbaud'da ≈ 199 B (400 Hz) / ≈ 699 B (200 Hz), 3 Mbaud'da ≈ 429 / ≈ 1.179 B.

**Ön denetim.** Her HIL çalıştırmasından önce sim, senaryo ve taşıma profiliyle şunları hesaplar ve biri tutmazsa çalıştırmayı gerekçesini yazarak başlatmaz: (1) kritik pencerenin en kötü baytı (sensör hızları ve en küçük teslim gecikmeleri 05'ten) ile `S_kart`, `C_sim` ve köprü gecikmesinin (ölçüldükten sonra M1/M3 değeri) toplamı `R_maks`'a sığar; (2) toplu talep (en kötü Ping360 profil hızı 05'in `T_ping` modelinden, Ping360 bağlantı profili ve `N_kademe` ile; kamera algılaması; GCS payı) `B_bulk × f`'ye ve toplam talep ölçülen kapasitenin %50'sine sığar; (3) emülatörün modellenen GCS bant genişliği zarf ek yüküyle GCS payına sığar; (4) parti sınırları 02'nin çağrı kapasitelerine sığar. Öneriler: Ping360 bağlantı profilini yavaşlatmak (05 §2.4.4), `N_kademe`'yi düşürmek, 200 Hz'e ya da USB'ye geçmek. Çalışma sırasında süren taşma olağan durum değildir: profil atılır, `bulk_drop` artar, bariyerli koşu geçersizdir.

**GCS patlama sınırı.** Bir toplu pencereye en çok 8 zarf ve en çok dört periyotluk ortalama pay (400 Hz'te 160 B, 200 Hz'te 320 B) konur; fazlası ağ geçidi kuyruğunda bir sonraki pencereyi bekler ve sayılır. GCS yalnız toplu pencerede olduğu için kritik pencereyi etkilemez.

### 2.7 Sonar profilinin taşınması

- **Parçalama.** Profil bir `HEAD` ve `F = ⌈n/246⌉` `DATA` ile gider; parça `k` örnekleri `[246k, 246k + 245]`; son parçadaki geçerli örnek `n − 246(F − 1)`. Gönderen son parçanın kullanılmayan kuyruğunu kodlamadan önce sıfırlar; MAVLink 2 sondaki sıfırları kırpar, alıcı sıfırla tamamlar (08.10; `mavlink_helpers.h`). Geçerli uzunluğu HEAD'deki `n_samples` belirler; değeri sıfır olan son örnekler doğru geri gelir. `data` dizisi XML'de son u8 alan olarak tanımlanır; `mavgen` alanları tip boyutuna göre ve boyut içinde XML sırasıyla dizdiği için tel düzeninde sona düşer. Tel düzeni elle hesaplanmaz, üretilen başlıktan alınır ve testle doğrulanır.
- **Boyut** `P(N) = 50 + 267(F − 1) + (9 + son + 12)`: 200 → 271 B; 246 → 317; 247 → 339; 600 → 713 (ek yük %18,8); 1199 → 1.354; 1200 → 1.355 (%12,9). Bant genişliği örnek sayısıyla tam orantılı değildir.
- **Atama.** Yalnız toplu sınıf; `t_hazır = t_teslim` olduğundan profil, `t_{n−1} ≥ t_teslim` olan ilk partide teslim edilir (bir periyot sabit ek gecikme, A6'da). `t_meas` darbenin gönderim anı olarak kalır. Profil birkaç toplu pencereye yayılırsa bütün parçaları aynı `step_seq`'i taşır (gelecek parti, 2.3); en çok `K_ileri` pencere.
- **Yeniden birleştirme (kart).** Yuva anahtarı `(sess, step_seq, inst)`; örnek başına iki yuva (biri birleşir, diğeri teslim edilmiştir). Yuva: HEAD alanları, 8 bitlik parça varlık bit eşlemi, 1.200 B veri (iki yuva 2.400 B veri ve üst veri; yerleşim 01'in). Denetimler: `1 ≤ n_samples ≤ max_ping360_samples` (el sıkışmada anlaşılan, ≤ 1.200); `n_frag = ⌈n_samples/246⌉`; `frag_idx < n_frag` (HEAD gelmeden `< ⌈max/246⌉`); gelen veri ≤ 246 B. Yinelenen parça bayt bayt aynıysa yok sayılır, farklıysa yuva çelişkili sayılır ve profil atılır; HEAD için de aynı kural.
- **Tamamlanma.** Parti kapanırken yuvada HEAD ve bütün parçalar varsa profil teslim edilir; yoksa atılır ve `ping360_eksik` artar. Kısmi profil çekirdeğe verilmez; gerçek cihazda bozuk mesajın tümüyle kaybolmasına karşılık gelir.
- **Tampon sahipliği.** Teslim edilen yuva `ida_core_step` dönene kadar değişmez; çekirdek dilimi kopyaladığı için (02 §3.8) 03'ün arka plan işi kartın yuvasına dokunmaz ve yuva adım dönüşünde serbest kalır.
- **Örnek sayısı ve kademe.** `n_eff = min(n_istenen, N_kademe)`; `sp_eff = round(sp_istenen · n_istenen / n_eff)`, [80, 40.000] aralığında. Gerçekleşen menzil `R = c · n_eff · sp_eff · 25 ns / 2`; nicemlemenin bağıl menzil hatası ≤ `0,5/sp_eff` ≤ %0,63. 0,75–50 m aralığında üst sınıra ulaşılmaz (50 m, 600 örnek, `c` = 1.500 m/s için `sp` ≈ 4.444). HEAD etkin değerleri taşır; istenen ve gerçekleşen değerler A6'ya yazılır; kademe karta gönderilmez. `N_kademe`: tam 1200, sade 600 (05 §5).

### 2.8 İki saat alanı ve tazelik

| Karar ya da alan | Saat | Taraf |
|---|---|---|
| Ölçüm zamanı, TF'nin `t_sim`'i, parti içeriği, `L_act`, uygulama anı, kullanım süresi | sim | sim |
| Emülatörün gecikme, kayıp ve kesinti olayları (hızlandırılmış SIL'de de doğru) | sim | sim |
| Alım zamanı `t_rx` | kart duvar (HIL); SIL'de TF(n)'nin sanal duvar karşılığı | kart / SIL |
| `W(t)`, `D_n`, pencereler | sim duvar | sim |
| IMU ve sensör tazeliği | `t_rx` ve ölçüm zamanının ilerlemesi | kart çekirdeği (02 §4.1) |
| Eş canlılığı (500 ms), el sıkışma yinelemesi (200 ms), olay yinelemesi (10 ms) | duvar | iki taraf |
| GCS failsafe süreleri (3 s / 60 s) | duvar (kart); mesajın varışı emülatörde sim zamanıyla | kart (02 §4.3) |
| Gözetim zamanlayıcısı | duvar | kart (01) |
| `TIMESYNC`, `rx_wall_us`, `rx_to_tx_us`, `step_us`, `board_wall_us` | duvar | iki taraf |

**Alım zamanı.** Kartta, çerçevenin son baytını taşıyan USB aktarımının ya da UART DMA/boşta hat olayının kesmede alınan duvar saati damgasıdır (01'den beklenen); ana döngünün bekleme süresi alım zamanına girmez. Her üye kendi damgasını taşır. SIL'de bütün parti üyelerinin alım zamanı `TF(n)`'nin sanal duvar karşılığıdır. Alım zamanı ile ölçüm zamanı hiçbir yerde birbirinden çıkarılmaz (02 §3.1).

**Üç ayrı canlılık, birbirini yenilemez:**
1. *Taşıma etkinliği:* herhangi bir bayt; yalnız tanı.
2. *Eş canlılığı:* güncel oturumun CRC'si doğru herhangi bir `ida` mesajı; `KEEPALIVE` 10 Hz'i garanti eder. 500 ms (5 periyot) gelmezse oturum `KOPUK`. El sıkışmada `HELLO` 200 ms'de bir gelir; zaman aşımı 1 s.
3. *Veri akışı:* kartta 02 §4.1'in iki ölçütü. Yalnız kabul edilmiş örnekler tazeliği yeniler: doğru oturum, kapanmış parti, `seq` ve `t_meas` ilerlemiş. Oturum dışı, yinelenen, çelişkili ya da CRC'si bozuk çerçeve çekirdeğe ulaşmaz. TF ilerlemesi çekirdeğe `time_frame` olarak gider (IMU'nun ölçüm zamanındaki bayatlığı, 02 §4.1). Simde veri akışının canlılığı `ACTUATOR` yanıtlarının ilerlemesidir.

`KEEPALIVE` ya da `TIMESYNC`'in gelmesi veri akışını canlı göstermez. Sim, `TIMESYNC` ile kart duvar saati ile kendi duvar saati arasındaki ofseti ve gidiş-dönüşü kestirir; `(T₀, t₀)` eşlemesiyle kart olayları A6'da sim zaman çizgisine yerleştirilir.

### 2.9 Oturum yaşam döngüsü

Durumlar: `KAPALI → EL_SIKIŞMA → HAZIR → KOŞUYOR`; herhangi birinden `KOPUK → EL_SIKIŞMA`.

| Olay | Sim | Kart (`ida_link`) |
|---|---|---|
| Taşıma açıldı | `EL_SIKIŞMA`; `HELLO(nonce_s)` 200 ms'de bir | dinler |
| Sim `HELLO`'su alındı | — | hangi durumda olursa olsun varsa oturumu bitirir (temizlik), `EL_SIKIŞMA`; `HELLO(boot_id, nonce_b, peer_nonce = nonce_s)` |
| Uyumsuzluk (`proto_major`, `hash_base`, `ctrl_rate_hz`, örnek sınırı) | `HELLO_ACK(MAJOR / HASH / RATE)`; çalıştırma başlamaz, hata A3'e | `HAZIR`'a geçmez; çekirdek adım almaz, çıkışlar açılış durumunda |
| Çalıştırma başı, kart bu çalıştırma için açılmamış | `HELLO_ACK(REBOOT)`; 5 s (tahmin) içinde yeni `boot_id`'li `HELLO` bekler; gelmezse başlatma başarısız | platformun yeniden başlatma kancası: kartta sistem sıfırlaması, SIL'de çekirdeğin yeni `boot_id` ile yeniden kurulması |
| Uyumlu `HELLO`, yeni açılış | `HELLO_ACK(OK, sess, run_id)`; `HAZIR`. `sess` sıfırdan ve son 16 oturumdan farklı | `peer_nonce` kendi `nonce_b`'si ise `HAZIR(sess)`; `BOOT_RECORD` ve varsa `FAULT_RECORD` gönderir |
| Kartın `sess`'li ilk mesajı geldi | ilk TF'yi `SEQ_RESET` ile gönderir; `KOŞUYOR` | — |
| İlk `TF(SEQ_RESET)` | — | `son = step_seq − 1`; `KOŞUYOR`. `SEQ_RESET` yalnız `HAZIR`'da kabul edilir; `KOŞUYOR`'da gelirse protokol hatası, yok sayılır |
| `HAZIR`'da 1 s kart mesajı yok | `EL_SIKIŞMA`'ya döner; 10 s'de kurulamazsa hata A3'e | — |
| Eş canlılığı kaybı (500 ms) ya da işletim sistemi aygıtın kalktığını bildirdi | `KOPUK`: çalıştırma geçersiz; iticilere hemen "darbe yok"; portu 1 s'de bir yeniden açar | `KOPUK`: temizlik; `HELLO` bekler; IMU kesilmesini çekirdek kendisi görür (02 §4) |
| Kart beklenmedik biçimde sıfırlandı (`KOŞUYOR`'da yeni `boot_id`'li `HELLO`) | olay; planlı değilse geçersiz; hemen "darbe yok"; yeniden el sıkışma (`REBOOT` istenmez), yeni `sess`; fizik sürer | açılıştan başlar |
| Sim duraklatıldı (HIL) | TF ve sensör akışı durur, `KEEPALIVE` ve `TIMESYNC` sürer; çalıştırma geçersiz, 08 kullanıcıyı önceden uyarır (02 §4.4'ün beklentisi) | IMU bayatlar → arıza güvenliği; oturum sürer |
| Sim sürdürüldü | `step_seq` ve sim zamanı kesintisiz sürer | mandallı arıza eylemleri açık sıfırlama ister (02 §2.6) |
| Sim çöktü ya da yeniden başladı | yeni süreç, yeni `run_id`; `HELLO` → `REBOOT` | `HELLO` gelince oturum biter |
| Eski oturumun çerçevesi | `sess` güncel değilse atılır, `eski_oturum` sayılır | aynı |

**Oturum sonunda temizlenenler.** Kartta: ayrıştırıcı durumu, aşamalı parti yuvaları, gelecek parti tamponları, Ping360 yuvaları, komut kuyruğu (eski bir arm komutu yeni oturumda uygulanamaz), gönderim kuyruğu, önbellekteki son çıktı ve onaylanmamış olaylar (silinir, yeniden gönderilmez). Simde: bariyer durumu, uçuştaki istekler, toplu kuyruk, ağ geçidinin A1 kuyruğu, kabul edilmiş son nesil. Simdeki uygulanmış `event_id` kümesi çalıştırma boyunca korunur. "Darbe yok" kart sıfırlanmasında gerçek donanımın davranışıdır (sıfırlama PWM'i durdurur); bağlantı kaybında kullanım süresi zaten en geç 50 ms'de doldurur, sim bunu beklemeden uygular (temkinli seçim, iskelet §3.2 ile uyumlu).

### 2.10 SIL taşıması

- SIL koşucusu aynı mesajları **TCP loopback** (`127.0.0.1`, `TCP_NODELAY`) üzerinden alır; seçim iki işletim sisteminde aynı API'nin bulunmasındandır. Gecikmesi ölçülmedi (03.9); SIL beklediği için yalnız koşu hızını etkiler.
- **Tek kod yolu:** sim tarafında yalnız `ITransport` uygulaması ve `LockstepDriver`'ın son tarih politikası (SIL'de yok) değişir. Kart ve SIL aynı `protocol/ida_link` modülünü bağlar; SIL baytları gerçekten kodlar ve çözer.
- SIL'de taşıma gecikmesi, kayıp ve bozulma hata enjektörüyle eklenebilir (2.12); HIL zamanlamasının provasıdır.

### 2.11 Bağlantı emülatörü ve yer istasyonu bağlantısı

**Yeri.** Sim sunucusunda, GCS ağ geçidi ile parti kurucu arasında, yalnız GCS düzleminde; iki yön ayrı modellenir. Sim zamanıyla sürülür ve kendi Philox akışını kullanır (04 §2.13). Dünya iş parçacığı her kontrol adımında `LinkGeometry` yayınlar: anten faz merkezinin yerel yüzeye göre yüksekliği (`IEnvironmentView::surface_elevation`, 04 §3), yer istasyonu antenine uzaklık, kablo boyu.

**İki gecikme ayrı raporlanır.** Modellenen bağlantı gecikmesi sim zamanındadır. HIL köprüsü buna sabit 0–1 periyotluk toplu pencere gecikmesini (2.3) ve gerçek taşıma ile kart servis süresini ekler; kartın duvar saatiyle çalışan failsafe süreleri toplamı görür. A6 ikisini ayrı yazar (köprü payı TIMESYNC ve `rx_wall_us` ile kestirilir). Modellenen gecikme köprünün en küçük gecikmesinden kısaysa (L0 ideal profil ya da bir periyottan kısa gecikme) çerçeve "model tutulamadı" diye işaretlenir ve sayılır. 3 s / 60 s ölçeğindeki failsafe sürelerine etkisi küçüktür; kısa gecikmeli profiller bu sınırla yorumlanır.

**Model parametreleri** (yön ve bağlantı başına; 11 §A.3 ve düzeltmeleri; değerler A5'te):

| Parametre | Anlam |
|---|---|
| `latency_base`, `jitter` | Dağılımlı gecikme; seri telsiz ve kablo profillerinde bırakma zamanı azalmayan tutulur (sıra korunur); yalnız datagram profilinde sıra bozulabilir |
| `bandwidth`, `queue_limit` | Serileştirme `bayt × 8 / hız`; kuyruk dolunca sondan düşürme |
| Kayıp | Bernoulli (L1) ya da zamana bağlı Gilbert–Elliott (L2): **olay zamanlı** iki durumlu süreç; G'de kalış `Üstel(λ_GB)`, B'de `Üstel(λ_BG)`, süreler Philox akışından ters dağılımla. Ortalama kötü durum süresi tam `1/λ_BG` saniyedir ve telemetri hızından bağımsızdır (11.15). Çerçeve, gönderim anındaki durumun başarı olasılığıyla (G'de `k`, B'de `h`) geçer. Sürüm 0.1'in sabit 10 ms ızgaralı biçimi kaldırıldı: o biçimde ortalama süre `Δ/(1 − e^{−λΔ})` olur (`λ_BG` = 100 s⁻¹ için 15,8 ms, `1/λ` = 10 ms değil) |
| `outage` | zamanlanmış ya da olaya bağlı kesinti (11 §B.10; biçim 08) |
| `quality(geometri)` | telsizde marj modeli; kabloda uzunluk ve kopma; akustikte ayrı profil |

**Telsiz kalitesi** ("anten battı ⇒ kopuk" yerine; 11.2): `M(t) = P_tx + G_tx + G_rx − FSPL(d, f) − L_su · d_su(t) − L_uyumsuz(t) − S_rx`.
- `FSPL = 20 log₁₀ d + 20 log₁₀ f − 147,55` (`d` m, `f` Hz; `20 log₁₀(4π/c)` = −147,552); `d ≥ 1 m`, yakın alan dışında geçerli; yüzey yansıması ve engel ilk sürümde yok.
- `d_su = max(0, −h_anten)`: anten üstündeki su yolu, dalga dahil.
- `L_su` (dB/m): ITU-R P.527-6 karmaşık geçirgenliğinden tam formülle, `α = ω √(με′/2) √(√(1 + (σ_eff/(ωε′))²) − 1)`, `L = 8,686 α` (11.1); `ε′` mutlak geçirgenliktir (`ε₀ ε_r`); ITU'nun toplam kayıp bileşeni kullanılırsa DC iletkenlik ikinci kez eklenmez. Kabul referansı (Tablo 11.1b): deniz suyu 20 °C, 35 ppt için 433 / 915 / 2400 MHz'te 663 / 826 / 1084 dB/m; 0,01 S/m tatlı su örneğinde 10,2 / 39,3 / 256,9 dB/m. 433 MHz'te 10 cm su: deniz suyunda ≈ 66 dB, tatlı su örneğinde ≈ 1 dB.
- `L_uyumsuz` parametredir, değeri bilinmiyor (M25). Paket başarısı `k(M) = 1/(1 + e^{−(M − M₅₀)/w})`, `w > 0`; `M₅₀`, `w`, `h` ölçülmüş değil, parametredir. Marj eşiğe döndükten sonra bağlantı `reacquire_time` boyunca kapalı (11 §A.2; 1–3 s tahmin, M25). Hassasiyet ve hızlar varyant ve firmware'e bağlıdır (11.14). İkili kopma kuralı yalnız "kaba" profil olarak seçilebilir (11.2).

**Profil türleri** (uygulama mesajı kaybı ile taşıma bozulması ayrıdır; 11.13):

| Profil | Fiziksel karşılık | Davranış ve doğruluk düzeyi |
|---|---|---|
| `datagram` | SiK/RFD telsiz, UDP | Uygulama çerçevesi bütün olarak düşer. İsteğe bağlı `hava_paketi_bayt` ile ardışık çerçeveler hava paketlerinde gruplanır ve kayıp paket başına uygulanır. Telsiz paketlemesinin yaklaşığıdır, fiziksel paket modeli değildir |
| `stream` | kablo üzerinde TCP (Fathom-X Ethernet; 80 Mbit/s pratik, 300 m test edilmiş, 11.5) | **Yaklaşık model.** Çerçeve düşmez, sıra korunur; kötü durumda akış durur. Yeniden iletim `RTO₀, 2RTO₀, 4RTO₀ … ≤ RTO_maks` geri çekilmesiyle denenir; kanal G'ye döndükten sonraki ilk deneme anında birikmiş baytlar sırayla teslim edilir (kafa-kuyruk tıkanması). Toplam durma `T_kopma`'yı aşarsa bağlantı kopar, kuyruktaki baytlar atılır, `T_yeniden` sonra yeniden kurulur. Paket düzeyinde TCP modeli değildir (RFC 9293, RFC 6298); RTO değerleri işletim sistemine göre farklıdır (doğrulanmadı; M25). Bit bozma bu profilde uygulanmaz |
| `corrupt` | bit hatası | Yalnız `datagram` ve seri profillerde: bayt bozma → CRC reddi → ayrıştırıcının yeniden senkronu |
| `acoustic` | M16 sınıfı modem | İlk sürümde yok. `HEARTBEAT` 21 B, 1 Hz'te 168 bit/s ister (11.9); 10 bit/s'ye sığmaz (tek çerçeve 16,8 s). Yayılma gecikmesi koşulsuz `r/c` ile eklenmez (Ç13); modem 00/A18'e bağlı; M64 satışta değil (11.16) |

Kademeler 11 §A.3'teki gibidir: L0 ideal, L1 gecikme + bant + Bernoulli, L2 zamana bağlı GE + kalite; L3 akustik açık kalır.

**Yer istasyonu ağ geçidi.** Laboratuvar uygulaması araçla MAVLink konuşmaz: web panelinin komutları A3 ile gelir, ağ geçidi bunları `sysid 254` ile MAVLink'e kodlar ve emülatöre verir; gelen telemetriyi çözüp A3'e yayınlar. Hazır yer istasyonu için MAVLink/UDP uç noktası açılır (QGC varsayılanı 14550); QGC çerçeveleri (`sysid 255`) aynı emülatörden geçer. QGC uyumunun kapsamı: bağlantı ve `HEARTBEAT`, tutum ve konum, parametre listesi okuma ve yazma, standart yol noktalarıyla görev yükleme. Genel otopilot eklentisinde kip değiştirme ve guided işlemler desteklenmez; hibrit kipler, özel görev öğeleri ve atış komutları laboratuvar uygulamasının işidir (06.12, 09.15). QGC'nin hedef mi prova mı olduğu kullanıcı kararıdır (00/A6); kabul ölçütü M18. Değerlendirme için sim GCS telemetrisini emülatöre girmeden önce de A6'ya yazar.

### 2.12 Hata enjektörü

Taşıma ile ayrıştırıcı arasında, sim tarafında, iki yönde ayrı çalışır; HIL ve SIL'de aynıdır. İşlemler: çerçeve düşürme, çoğaltma, komşu çerçeveleri yer değiştirme, bayt bozma, kesme, gecikme sıçraması, çöp bayt ekleme. Düzlem ve mesaj türü seçilir; her işlem tohumlu ve kayıtlıdır. Kartın aldığı baytlar yazımdan önce, kartın gönderdikleri simdeki alımda bozulur; fiziksel USB'de enjeksiyon yapılmaz. Enjektör protokolün **sınanmasıdır**; emülatör yer istasyonu bağlantısının **modelidir**. Bir enjeksiyon birden çok belirti üretebilir (ör. bozuk uzunluk alanı: CRC hatası + arkasındaki çerçevelerin kaybı + parti eksikliği + profil kaybı); belirti kümeleri 7. başlıktaki kabulde ayrı değerlendirilir.

### 2.13 Tekrar oynatma kaydı (A6)

İskelet A6 gereği tekrar oynatılabilir bir HIL ya da SIL kaydı şunları içerir (biçim 08'in): iki yöndeki bütün A1 çerçeveleri, sim duvar saatiyle gönderim/alım damgaları ve ilgili sim zamanı; her parti için üye listesi, `n_members`, atama ve kayma kararları, bir periyotluk ek gecikmeler; her bariyer kararı (zamanında, geç, kaçırıldı, eş zamansız güvenli çıktı) ve uygulanan `ActuatorFrame`'in fizik adımı; uygulanan, yinelenen ve reddedilen olaylar ile uygulandıkları fizik adımı; eski nesil, eski oturum ve çelişki retleri; düşürülen veriler (`bulk_drop`, `ping360_eksik`, `sonar_geç`, GCS kayması) ve nedenleri; oturum geçişleri (`HELLO`, `HELLO_ACK`, `sess`, `run_id`, `boot_id`, `BOOT_RECORD`, `FAULT_RECORD`); `(T₀, t₀)` eşlemesi ve TIMESYNC kestirimleri; emülatörün bırakma ve kayıp kararları; hata enjektörünün işlemleri; ön denetim raporu. Tahmini yük: ham iki yön trafiği 400 Hz'te ≈ 151 kB/s (50 m) ile ≈ 293 kB/s (en hızlı sonar) arası, üst veri hariç. "Yalnızca olaylar" kademesinde tekrar oynatma güvencesi yoktur (iskelet A6).

**Kanonik işlevsel iz.** Bit düzeyinde karşılaştırmalar ham kayıt üzerinde değil, bu iz üzerinde yapılır: parti içerikleri (duvar saati alanları hariç); `ACTUATOR`'ın işlevsel alanları (`step_seq`, `pwm_us`, `valid_for_ns`, `safety_gen`, `CMD_VALID`/`ARMED`/`GÜVENLİ` bitleri, `src`); olayların `event_id`, `kind`, `tube`, `value`, `step_seq` alanları; uygulama adımları. `step_us`, `rx_to_tx_us`, `rx_wall_us`, `BOARD_STATUS`, `DIAG` ve TIMESYNC tanı alanıdır, karşılaştırmaya girmez.

---

## 3. Arayüzler

**A1** (iskelet §3): içerik 2.2, taşıma 2.1 ve 2.10. Sözleşme işinde dondurulacaklar: `ida.xml`; mesaj kimlikleri (0–255 kullanılmaz; `common` ve `ardupilotmega` ile çakışmazlık `mavgen` ile denetlenir); `sysid`/`compid` değerleri (öneri: sim HIL 250, laboratuvar ağ geçidi 254, QGC 255, araç 1/1); şema özetinin kanonik metin kuralı; `flags`, `tf_flags`, `act_flags` bit tabloları; izinli GCS listesi.

**A1 ↔ A4 eşlemesi** (port alanları 02 §3.3–3.4; dönüştürücü kartta 01'in, PC'de 02'nin parçasıdır, eşlemeyi bu bölüm tanımlar; iskelet §3):

| A1 | Port | Dönüşüm |
|---|---|---|
| `H.t_meas_ns`, `seq`, `inst`, `flags` | `ida_hdr_t.t_meas`, `seq`, `inst`, `flags` | yok (ns; iskelet §3.1) |
| — | `ida_hdr_t.t_rx` | bağlantı katmanı damgalar (2.8) |
| `window_ns` (IMU, BARO, BATT) | `ida_imu_t.interval_ns`; BARO ve BATT için 02'de alan yok (8. başlık) | yok |
| IMU, MAG, BARO, BATT değerleri | aynı adlar; SI; sensör çerçevesi | yok. Fiziksel montaj 05'in, otopilotun inandığı montaj 02'nin parametresidir |
| GNSS `lat_e7`, `lon_e7` (1e-7°), `alt_mm` | aynı tamsayılar | yok (02 aynı biçimi tutar) |
| DVL `cov[6]` (üst üçgen, satır sırası xx, xy, xz, yy, yz, zz; (m/s)²; DVL çerçevesi) | `cov[9]` | simetrik açılım |
| DVL `dvl_bits` | `vel_valid`, `beam_valid`, `track`, kovaryans varlık biti | bit açma |
| DVL `t_report_ns` | karşılığı yok | atılır (8. başlık) |
| `PING360_HEAD` + `DATA` | `ida_ping360_t` (cihaz birimleri: gradyan, 25 ns, µs, kHz; 02 §3.3) | birleştirme (2.7); SI'ye çeviri 03'ün |
| `SAFETY_IO` | 02'de karşılığı yok; emniyet ve tüp durumu örneği (8. başlık) | yok; kart yalnız okur |
| `DETECTION` `values[3]`, `cov[6]` | `ida_detection_t` (alanlar 03'ün) | `kind`'a göre anlam ve birim 03'ün; altı boyutlu poz ilk sürümde yok |
| `GCS_IN` iç çerçevesi | `ida_cmd_t` | MAVLink → komut; `cmd_id` = `(step_seq, idx)`'ten 32 bit; yanıt aynı kimlikle `COMMAND_ACK` vb. |
| `TIME_FRAME` | `time_frame {t, seq}` | yok |
| `ida_out_t` sürekli komut | `ACTUATOR` | `thr_cmd` → `pwm_us` (01 `us_from_thr`); `thr_valid` → `CMD_VALID`; `seq`, `t_meas`, `valid_for_ns`, `armed`, `src` aynen; `safety_gen` ve `GÜVENLİ` bağlantı katmanında (2.4) |
| `ida_out_t.events[]` | `IDA_EVENT` | `FIRE` → 04 `ateşle`; `ARM_CHANGE` 0/1 → `emniyet_kapat` / `emniyet_aç` |
| `ida_telem_t` | `NAV_STATE`, GCS telemetrisi | `q_nb` (w, x, y, z), Hamilton, gövde → NED (iskelet §3.1) |

Tel istisnaları: `temp_c` °C (02 ile aynı), `pwm_us`, GNSS tamsayıları ve Ping360 cihaz birimleri; hepsi gerçek cihazın ve sürücünün alan anlamını korumak içindir (02 §3.2 md.2).

**Kart tarafı C modülü** (`protocol/ida_link`; 01 karta, 02 SIL koşucusuna bağlar):

```c
void        ida_link_init(ida_link_t *l, const ida_link_cfg_t *cfg, ida_wtime_t now_w);
void        ida_link_rx(ida_link_t *l, const uint8_t *b, size_t n, ida_wtime_t t_rx); /* ana döngüden; t_rx kesme damgası */
ida_link_ev ida_link_take(ida_link_t *l, ida_step_in_t *step, ida_tick_in_t *tick);   /* STEP, TICK, DUP, NONE */
size_t      ida_link_put_out(ida_link_t *l, const ida_out_t *o, bool async,
                             uint8_t *tx, size_t cap);   /* ACTUATOR ve yeni olaylar; kritik */
size_t      ida_link_poll_tx(ida_link_t *l, ida_wtime_t now_w, uint8_t *tx, size_t cap);
                                                          /* olay yinelemesi, oturum, kritik olmayanlar (bütçeli) */
ida_link_st ida_link_state(const ida_link_t *l);          /* KAPALI … KOPUK */
```

`ida_link_cfg_t`: `ctrl_rate_hz`, `max_ping360_samples`, `boot_id`, `boot_count`, `reset_flags`, sürüm alanları, `us_from_thr`, `reboot` kancası, `T_olay`, kritik olmayan gönderim bütçesi. OLD, GAP, eksik parti ve oturum sayaçları `plat` alanına (02 §3.1) aktarılır. **Durum örneğe aittir:** MAVLink ayrıştırma durumu ve tamponu, gönderim sıra sayacı, oturum, parti yuvaları ve olay tablosu `ida_link_t` içindedir. Çağıranın verdiği durumla çalışan MAVLink işlevleri (`mavlink_frame_char_buffer`, `mavlink_finalize_message_buffer`; c_library_v2 `mavlink_helpers.h`) kullanılır, kütüphanenin küresel kanal dizileri kullanılmaz; böylece bir süreçte birden çok SIL çekirdeği koşabilir (02 §2.8). Üretilmiş başlıkların bu işlevlerle kullanımı sözleşme işinde doğrulanır (doğrulanmadı); doğrulanamazsa her SIL koşucusu ayrı süreçte çalışır. Modül bellek ayırmaz.

**Sim tarafı (`sim/hil/`):**

```
class ITransport     { bool open(const Endpoint&); void close(); size_t write(span<const uint8_t>);
                       size_t read(span<uint8_t>, Duration timeout); size_t queued() const; LinkEvent poll_event(); };
class HilSession     { State state() const; uint16_t sess() const; void on_frame(const Frame&, WallTime rx);
                       RunValidity validity() const; };
class BatchBuilder   { Batch build(StepId n, SimTime t_n, span<const MeasurementRecord>, span<const GcsFrame>); };
class StepBarrier    { void post(StepId n, Batch&&, WallTime not_before, WallTime deadline);
                       AwaitResult await(StepId n);   // Accepted{ActuatorFrame, events} | Late | Missed; 04 LockstepDriver çağırır
                       size_t in_flight() const; };
class BulkScheduler  { void enqueue(BulkItem&&); size_t fill(StepId n, size_t budget); };
class PreflightCheck { Report evaluate(const ScenarioConfig&, const TransportProfile&); };
class LinkEmulator   { void submit(Dir, Frame, SimTime); vector<Frame> release(Dir, SimTime); void set_geometry(const LinkGeometry&); };
class FaultInjector  { void configure(const FaultSpec&); };
```

**Diğer bölümlerden beklenenler:**
- **01:** `ida_link`'in entegrasyonu (01'in `ida_conv_*`/`ida_acc_*` API'si bununla değiştirilir ya da biriktirici yalnız gerçek sürücü sınırı olarak kalır; HIL yolunda iki dönüştürücü olmaz); kesmede alım damgası; IMU'suz kapanan partide hemen `tick`; ≥ 100 Hz donanım `tick`'i ve yalnız güvenli çıktıda eş zamansız gönderim; `us_from_thr`; kritik olmayan gönderim bütçesi ve çerçeve bütünlüğü; dilimler arası kritik iş denetimi; `BOOT_RECORD`, `FAULT_RECORD`, `DIAG` içerikleri; güç kesintisinden sonra da geri gitmeyen açılış sayacı (olay kimliği için; `.noinit` yetmez); Ping360 yuvalarının yeri; K5 kabulünün 2.5'teki tam yol bütçesine çekilmesi.
- **02:** `ida_hdr_t`'ye ya da BARO/BATT yapılarına `window_ns` (iskelet §3.1); `time_frame` alanı (`t`, `seq`); `DUP_SEQ`'te kapılanmış yeniden üretim; güvenli/olağan sınıflamasında kullanılacak çıktı alanları; `event_id`'nin açılış sayacından kurulması ve olayın `valid_for_ns`'i; sanal emniyet ve tüp durumu için port örneği.
- **03:** `IDA_FC_COMMAND`, `IDA_FC_COMMAND_ACK`, `IDA_FC_STATUS` alanları; `shot_id`; olayın son geçerlilik süresi; algılama `kind` türleri, `values`/`cov` anlamları ve birimleri.
- **04:** `ActuatorFrame` ile `valid_for_ns`, `safety_gen` kabul kuralının bağlanması; `OneShotEvent`'in onay sonuçları, son geçerlilik denetimi ve sanal emniyet girdileriyle kapılanması; emniyet girdilerinin ve tüp durumunun `IDA_SAFETY_IO` için okunması; "darbe yok" girdisinde ESC davranışı; `LockstepDriver`'ın `StepBarrier` ile bağlanması; `LinkGeometry` için anten konumu.
- **05:** `MeasurementRecord`'da `t_meas`'ın aralık ölçümlerinde aralık sonu olması (iskelet §3.1; BARO şu an pencere ortası); her kaydın `t_hazır`'ı; Ping360 `T_ping` modelinin ön denetime verilmesi; `IDA_PING360_CONFIG`'in yorumu.
- **07:** A8 üzerinden gelen kaydın `t_meas` (yakalama anı) ve `t_teslim`'i; `t_teslim`'de hazır değilse düşürülmesi (00 Bölüm 6).
- **08:** sanal emniyet girdilerinin A3 denetimi ve senaryodaki zaman çizelgesi; sayaçların ve geçerlilik bayrağının A3'te gösterimi; HIL'de duraklatma uyarısı; 2.13'teki A6 içeriği; emülatör ve hata profillerinin A5 biçimi; ön denetim raporunun gösterimi.

---

## 4. Hata durumları ve davranışı

| Durum | Tespit | Davranış |
|---|---|---|
| CRC hatası, bozuk çerçeve | MAVLink ayrıştırıcısı | çerçeve atılır, sayılır. Bozuk uzunluk alanı arkasındaki geçerli çerçeveleri tüketebilir; toparlanma en çok bir azami çerçeve boyu (280 B, imzalı) içinde beklenir (sabitlenmiş ayrıştırıcıyla doğrulanacak) |
| Oturum dışı çerçeve | `sess` | atılır, `eski_oturum` |
| Parti üyesi eksik, yinelenmiş ya da çelişkili | bit eşlemi ve anahtar (2.3) | `PARTİ_EKSİK`; bariyerli koşu geçersiz, 1x'te sayaç |
| Sonlandırıcı (TF) kaybı | sonraki TF | parti eksik kapanır, adımlar sırayla koşar; yanıt geç kalır |
| TF yinelemesi, eski TF, sıra boşluğu | `step_seq` | DUP / OLD / GAP (2.3) |
| Son tarih kaçırıldı | sim, `D_n` | son komut (kullanım süresi içinde), `kaçırılan_adım`, geçersiz |
| Geç, yinelenmiş, eski nesilli `ACTUATOR` | sim | kaydedilir, uygulanmaz; güvenli çıktı her durumda uygulanır |
| Kullanım süresi doldu | sim | "PWM sinyali yok"; ESC modeli (04) |
| Olay onayı gelmedi | kart, `T_olay` | aynı kimlikle yineleme; oturum sonunda silinir |
| Yinelenen `event_id` | sim | uygulanmaz, `DUPLICATE` onayı |
| Süresi geçmiş olay; emniyet girdileri izin vermiyor | sim | uygulanmaz; `REJECTED_EXPIRED` / `REJECTED_SAFE` |
| Ping360 eksik, çelişkili ya da toplu kuyruk taşması | kart / sim | profil atılır; `ping360_eksik` / `bulk_drop`; bariyerde geçersiz |
| Ön denetim tutmadı | sim | çalıştırma başlamaz; gerekçe A3'e |
| Sürüm, şema, hız uyuşmazlığı; el sıkışma zaman aşımı | `HELLO` | çalıştırma başlamaz (2.9) |
| Kart sıfırlanması, kablo çıkması, sim duraklaması ya da çökmesi | 2.9 | 2.9 |
| İşletim sisteminin süreci ya da USB yığınını bekletmesi | `D_n` kaçırmaları, `d_aşağı` kayması, TIMESYNC sıçraması | sayılır; HIL öncesinde uyku ve USB seçici askıya alma kapatılır; etkisi M1 senaryosunda ölçülür |
| GCS payı aşıldı | ağ geçidi | sonraki pencereye kayar, sayılır; kritik pencere etkilenmez |
| İzinsiz GCS mesajı ya da yanlış hedef | kart | atılır, sayılır |
| Alan aralık dışı, NaN | `ida_link` çözücüsü | atılır, sayılır (02 §4.4) |

Bariyerli bir çalıştırmanın **geçerli** sayılması için: `kaçırılan_adım`, `parti_eksik`, `bulk_drop`, `ping360_eksik`, geç olay ve eş zamansız güvenli çıktı sayıları sıfır; planlanmamış sıfırlama ve duraklatma yok. Sayaçlar A3'te canlı gösterilir ve A6'ya yazılır (Ö11).

---

## 5. Kalite kademelerinin etkisi

HIL ve protokol her kademede tam çalışır (iskelet §6). Dolaylı etkiler: Ping360 örnek üst sınırı `N_kademe` profil boyutunu belirler (1200 → 1.355 B, 600 → 713 B); kamera kapalıyken dışarıdan algılama gelmez, mesaj kümesi değişmez (02 §5); kayıt kademesi tekrar oynatılabilirliği belirler (2.13). Emülatör kademeleri (L0–L3) kalite kademesi değil, senaryo seçimidir. Ölçüme bağlı yedekler kademe değildir (iskelet §6).

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| A1 taşıması | USB HS CDC (TinyUSB) | ST-LINK VCP ≥ 3 Mbaud (2 Mbaud yalnız kritik en kötü ≤ 320 B ise), **200 Hz ve `L_act = 3h` ile birlikte** | M1, M2, M3 |
| A1 biçimi | MAVLink 2 + `ida` dialect | açık serileştirmeli özel biçim (aşağıda) | M9 |
| `L_act` | 1 fizik adımı | 2 fizik adımı; 200 Hz'te 3 | M1, M17 |
| Bariyerli kontrol hızı | 400 Hz | 200 Hz | M17, M5 |
| Toplu pencere bütçesi | USB: 4.096 B/periyot | VCP: baud'dan (2.6) | M1, M3 |
| Kanal ayrımı | tek akış, pencere bütçeli | USB'de ikinci CDC arayüzü (GCS düzlemi) | M1 (kafa-kuyruk tıkanması gözlenirse) |
| Kullanım süresi, olay yineleme süresi | 50 ms, 10 ms | M5 ve M17 sonuçlarıyla ayar | M17 |
| Emülatör parametreleri | üretici ve literatür değerleri | ölçülmüş değerler | M25, M24 |
| SIL taşıması | TCP loopback | Unix alan soketi / adlandırılmış boru | yeni ölçüm önerisi (8. başlık) |

**Yedek biçimin gerekleri** (08.11, 06.14, D06 N3): çerçeve COBS + `0x00` ayırıcı ve her çerçevede CRC-32C (COBS bütünlük eklemez; 06.14); en büyük çerçeve 1.400 B (Ping360 profili tek çerçeveye sığar, parçalama kalkar; büyük paket ihtiyacı bu biçimin gerekçelerindendir, 08.10). Başlık: `proto_major`, `proto_minor` u8; `msg_type` u16; `length` u16; `sess` u16; `step_seq` u32; `idx` u8; kaynak başına çerçeve sırası u16. Alanlar açıkça kodlanır: sabit genişlikli tamsayılar, little-endian; kayan nokta IEEE-754 (derlemede denetlenir, bit deseni tamsayı üzerinden); `bool` yalnız 0/1; enum'lar aralık denetimli; diziler uzunluk önekli ve azami boyutlu; padding ve hizalama tele girmez. Sürümleme 2.2'deki iki özet kuralıyla. Kodlayıcı ve çözücü (C ve C++) tek şema dosyasından üretilir; Rust tabanlı araç kullanılmaz (K3). GCS çerçeveleri MAVLink kalır ve zarf türünde opak yük olarak taşınır. Oturum, parti, bariyer, olay, toplu zamanlayıcı ve emülatör biçimden bağımsızdır.

M9'un karşılaştırması aynı anlamı taşıyan iki aday üzerinden yapılır: kartta ayrıştırma ve kodlama çevrimi (DWT), RAM ve flash, Ping360 parçalama maliyeti, uçtan uca gecikme, bakım yükü.

---

## 7. Test yaklaşımı ve kabul ölçütleri

**Protokol uygunluk testleri** (iki işletim sisteminde ve kartta):
- Her mesaj için kodla-çöz gidiş-dönüşü, sınır değerler, NaN reddi; C ile kodlanan baytların C++ ile çözülmesi ve tersi; depodaki altın bayt dizileri; tel düzeninin üretilmiş başlıkla karşılaştırılması.
- Şema özeti: temel alan değişikliği `hash_base`'i ve ilgili mesajın `CRC_EXTRA`'sını değiştirir; uzantı alanı değişikliği yalnız `hash_full`'u değiştirir; el sıkışma kuralları (2.2) her iki durumda doğru sonuç verir. Her düzen değişikliğinin CRC hatası üreteceği varsayılmaz.
- Ping360: `n_samples` ∈ {200, 246, 247, 600, 1199, 1200}; sonu sıfır olan veri; kırpma; eksik, yinelenmiş ve çelişkili parça; sınır dışı `frag_idx` (ör. 255) ve `n_samples`; birkaç pencereye yayılan profil; HEAD'den önce gelen parça.
- Parti: üye kaybı ile başka üyenin yinelenmesinin birlikte olduğu durum eksik sayılır; sonlandırıcı kaybı; sıra bozulması; `K_ileri` dışı `step_seq`; IMU'suz parti `tick` ile yanıtlanır.
- Çıkışlar: güvenli çıktıdan sonra gelen eski nesilli olağan çıktı reddedilir; geç güvenli çıktı uygulanır; kullanım süresi dolunca "PWM sinyali yok"; olay kaybında yineleme ile tam bir kez uygulama; çoğaltılmış olay bir kez uygulanır; aynı adımda emniyet kapatma ateşlemeden önce; oturum değişiminde bekleyen olay ve eski arm komutu uygulanmaz; kart yeniden başladıktan sonra önceki açılışın kimliği yeni sayılmaz; süresi geçmiş olay ve emniyet pimi takılıyken ateşleme uygulanmaz.

**Oturum testleri:** başarılı el sıkışma; major, şema ve hız uyuşmazlığı; `REBOOT` ve yeniden başlatmanın gelmemesi; `KOŞUYOR`'da `SEQ_RESET`; eski oturum çerçevelerinin reddi; koşu ortasında yazılım ve pin reseti; kablo çıkarma (yazılımla kesilebilen USB hub ile; araç seçimi açık); HIL'de duraklatma ve sürdürme; sim sürecinin öldürülüp yeniden başlatılması; KEEPALIVE kesilirken TIMESYNC'in ya da çöp baytların oturumu canlı tutmaması.

**Ölçüm testleri (M1'in protokol tarafı):** (a) simetrik geri döngü: `IDA_LOOPBACK` ile 64 B ve 256 B **çerçeve** (yük 52 ve 244 B), 200 / 400 / 1000 Hz, ≥ 30 dakika, macOS ve Windows, p50 / p99 / p99,9 / en büyük; (b) kontrol alışverişi: gerçek boyutlarla (aşağı 94 ve 261 B, yukarı 59 B) kartın bağlantı katmanının yapay `ACTUATOR`'ı hemen döndürdüğü ölçüm derlemesi, `RTT_taşıma`'yı `S_kart`'tan ayırır. Aynı ölçümler SIL'de TCP loopback için de yapılır.

**Bariyer testleri** (önce SIL ve hata enjektörüyle, sonra kartta): gecikme dağılımı taramasında kaçırmalar yalnız ve tam olarak `D_n` aşıldığında sayılır; uygulama anları her koşulda `t_n + L_act`; DUP, OLD, GAP; uçuşta iki istekli `L_act = 2h` düzeni; ön denetimin yetersiz yapılandırmayı reddetmesi. Aynı senaryonun iki SIL koşusunda kanonik işlevsel iz (2.13) bit düzeyinde aynı. Kaçırma, eksik parti ve eş zamansız çıktı sayaçları sıfır olan iki bariyerli HIL koşusunda, insan girdisi olmadan, aynı ikili ve platformda kanonik işlevsel iz aynı.

**Hata enjeksiyonu:** düşürme, çoğaltma, sıra bozma, bozulma, kesme ve çöp bayt her yönde ayrı. Kabul: her enjeksiyon kaydedilir ve en az birincil belirtisini üretir (ör. bozulmada CRC sayacı); belirtisi olmayan sayaç artışı enjeksiyonsuz koşuda görülmez; toparlanma 4. başlıktaki sınır içinde; kart hiçbir enjeksiyonda kilitlenmez ve gözetim zamanlayıcısını tetiklemez.

**Emülatör testleri:** Bernoulli kayıp oranı güven aralığında; GE'de ortalama kötü durum süresi iki farklı telemetri hızında `1/λ_BG` ile güven aralığında aynı (11.15); sıra korunan profillerde sıra bozulması sıfır, çıkış hızı `bandwidth`'i aşmıyor, kuyruk taşmasında düşürme; `L_su` birim testi Tablo 11.1b değerlerini %1 içinde üretiyor; `stream` profilinde çerçeve düşmüyor, durma ve geri çekilme beklenen anlarda, `T_kopma` aşılınca yeniden bağlanma; anten batıp çıkınca `reacquire_time`; "model tutulamadı" işaretinin kısa gecikmeli profilde görünmesi.

**QGC (M18):** bağlantı, araç türünün denizaltı görünmesi, parametre okuma ve yazma, standart görev yükleme; kip değişimi ve hibrit kipler kabul ölçütü değildir (09.15).

**Kabul ölçütleri (hedef, ölçülmedi):**
- Uygunluk, oturum ve hata enjeksiyonu testlerinin %100'ü geçer.
- USB HS'te 400 Hz, `L_act = h` ile bariyerli 30 dakikalık koşu iki işletim sisteminde kaçırmasız tamamlanır. Tamamlanmazsa önce `L_act = 2h`, sonra 200 Hz ve `L_act = 3h` denenir; karar M17'de kayda geçer.
- Ölçülen sürdürülebilir kapasitenin en çok %50'si kullanılır; ön denetim bunu her çalıştırmada uygular.
- Kablo yeniden takıldıktan sonra 3 s içinde `HAZIR` durumuna geçilir.

---

## 8. Açık noktalar ve iskelete öneriler

**İskelete öneriler** (bölüm iskelete uydu; aşağıdakiler mimarın kararına sunulur):
1. **Yedek taşıma ile yedek kontrol hızının bağı.** İskelet §8'de A1 taşıması ve kontrol döngüsü ayrı satırlardır; 2.6'nın hesabına göre VCP yedeği 200 Hz ve `L_act = 3h` gerektirir. Öneri: §8'e bu bağın notu.
2. **Bariyerin tanımı.** İskelet §4 tablosu "sim her sensör çerçevesinden sonra aynı sıra numaralı aktüatör yanıtını bekler" diyor. Öneri: "her zaman çerçevesinden sonra; yanıt sim zamanında sabit `L_act` gecikmesiyle uygulanır".
3. **Aktüatör gösterimi.** 02 normalleştirilmiş `thr_cmd`, 04 `pwm_us` kullanıyor; A1 `pwm_us` taşır, çeviri kartın platform katmanındadır. İskelet A1 satırına "itici komutları ESC darbe genişliği olarak" eklenmesi önerilir.
4. **Yeni dış arayüz.** Hazır yer istasyonu için MAVLink/UDP uç noktası A1–A7'de yok. Öneri: emülatör arkasında "sim sunucusu ↔ harici yer istasyonu" arayüzü; A8 iskelet 0.6'da kamera algılama bileşenine verildiği için numara mimarın.
5. **Sıcaklık birimi.** İskelet §3.1 sıcaklık birimi saymıyor; 02 ve bu bölüm °C kullanıyor. Kabulün iskelete yazılması önerilir.

**Komşu bölümlerle açık uyumsuzluklar:**
- **05:** BARO ve batarya kaydında `t_meas` pencere ortası; iskelet 0.4 aralık sonu + süre istiyor. 05 güncellenene kadar köprü `t_son = t_orta + window/2` ile çevirir (tam sayı ns'de kesin).
- **02:** `ida_hdr_t`'de ya da BARO/BATT yapılarında `window_ns` yok; `ida_dvl_t`'de `t_report` karşılığı yok (bu bölüm atar); `event_id` hâlâ rastgele `boot_id`'den kuruluyor ve olayda son geçerlilik süresi yok (iskelet 0.5 §3.2 açılış sayacı ve süre istiyor); sanal emniyet ve tüp durumu için port örneği yok; A4 tarafındaki tick ve kapılama adları 01 ile tek sözleşmede birleşmeli.
- **01:** `ida_conv_*`/`ida_acc_*` ile `ida_link` arasındaki tek dönüştürücü kararı; `T_gap = 10 ms` sonrası 100 Hz duvar çağrısının yerine 2.3'teki kural (IMU'suz partide hemen `tick`, ayrıca ≥ 100 Hz donanım `tick`'i); DUP'ta `fire[]` sıfırlama yerine olay tablosu; K5'in tam yol bütçesine çekilmesi.
- **03:** FC mesajlarının alanları ve algılama türleri tanımlanmadan `IDA_FC_STATUS` boyutu ve algılama belirsizlik sözleşmesi tamamlanamaz.

**Açık noktalar:**
- QGC hedef mi prova mı (00/A6); yer istasyonu bağlantı türleri, kablo tipi ve uzunluğu (00/A20); akustik modem (00/A18).
- Altı eksenli elle kumandada `MANUAL_CONTROL`'ün yeterliliği (sözleşme işinde).
- `sysid` değerleri, mesaj kimliği aralığı ve şema özetinin kanonik metin kuralı sözleşme işinde sabitlenecek.
- c_library_v2'nin örnek başına durumla kullanımı (3. başlık) sözleşme işinde doğrulanacak.
- Yeni ölçüm önerileri: (a) SIL TCP loopback gidiş-dönüşü ve koşu hızına etkisi; (b) kartta 400 Hz'te ayrıştırmanın ve USB alım kesmelerinin işlemci payı (M5 ile); (c) işletim sisteminin uyku ve USB güç yönetiminin gidiş-dönüş kuyruğuna etkisi (M1 senaryosu); (d) ST-LINK köprüsünün kendi gecikmesi (M3'e ek).
- Ping1D, batarya ve sızıntı hızları 05'te tahmindir; algılama hızı kamera kararına (00/A24) bağlıdır.
