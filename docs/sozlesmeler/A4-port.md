# A4 — Ortak çekirdek ↔ çevresi: `port` sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday, çapraz uyum turu (2026-11-20; `docs/gorevler/W1-uyum.md`) |
| Görev belgesi | `docs/gorevler/W1-A4.md`; ortak kurallar `docs/gorevler/W1-ortak.md` |
| Makinece okunur tanım | `core/include/ida/port.h` (bu belgenin parçasıdır; ikisi çelişemez) |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.8 (§2, §3, §3.1, §3.2, §4, §5, §8, §10); `docs/olcumler/MP4-dongu-yuku.md`, `MP1-arac-zinciri.md`, `MP3-vcp.md` (iskelet üzerinden); bölümler 01, 02, 03, 06 (taslak; iskeletle çeliştikleri yerde iskelet geçerlidir) |
| İnceleme | sol: `docs/sozlesmeler/inceleme/A4-sol.md`, `A4-sol-2.md`; yanıtlar: `A4-yanit.md`, `A4-yanit-2.md` (aynı dizin) |
| Durum | Yazan Opus; iki düzeltme turu mimar kararlarıyla yapıldı; çapraz uyum incelemesi ve mimar denetimi bekliyor; dondurulmadı |

Değerlerin niteliği: bu belgedeki her süre, hız ve kapasite ya bir ölçüme (rapor ve başlık atfıyla), ya bir birincil kaynağa, ya da **mimari seçimi (tahmin)** etiketine dayanır. Etiketli değerler ölçülmüş değildir; Bölüm 8'de hangi ölçümle kapanacakları yazılıdır.

Kural numaraları ilk sürümdekiyle aynı tutuldu; birinci düzeltme turunda eklenen kurallar A4-K102–K112, ikinci turda eklenenler A4-K113–K117, çapraz uyum turunda (W1-uyum) eklenenler A4-K118–K123'tür ve konularının bölümüne yerleştirildi. Çapraz uyum turunda kaldırılan ad ve alanlar (`IDA_FC_SELECT`, `IDA_MI_TARGET_ALIGN`, `ida_event_t.fc_ref`, `ida_fc_status_t.ext_len`/`ext[]`, `IDA_PORT_FC_EXT_MAX`) belgede ve başlıkta yoktur; kaldırılan kural yoktur.

---

## 1. Kapsam ve taraflar

### 1.1 Taraflar

| Taraf | Kim | Ne yapar |
|---|---|---|
| **Çekirdek** | Ortak çekirdek (`core/`; kestirim, güdüm, kontrol, görev, arıza güvenliği ve atış kontrol modülü), ISO C17 | Port işlevlerini uygular (`ida_core_*`); donanım, işletim sistemi ve simülasyon görmez |
| **Platform (kart)** | Kart yazılımı (`firmware/`) ve onun bağladığı A1↔A4 dönüştürücüsü (`protocol/ida_link`) | Çekirdeği kurar ve çağırır; girdileri doldurur, çıktıları A1'e verir; duvar saatini, kalıcı belleği, gözetimi sağlar |
| **Platform (SIL)** | SIL koşucusu (C17 + C++) ve aynı dönüştürücü | Kartın yerine çekirdeği PC'de çağırır; duvar saatini sim zamanından türetir |

"Platform" sözcüğü, ayrıca belirtilmedikçe iki platformu birden kapsar. Çekirdeği çağıran tek yer platformdur; çekirdek platformu hiçbir yoldan çağırmaz (geri çağırma yoktur; A4-K21).

### 1.2 Kapsam

Bu sözleşme şunları tanımlar: iki saat alanı ve zaman kuralları; çağrı modeli, birikmiş partiler ve bütçeler; sensör örneklerinin yapısı ve teslimi; komutlar ve daraltıcı mandalları; sürekli itici komutu ve tek seferlik olaylar; bağlantı, sistem ve sanal emniyet girdileri; kalıcı parametreler, açılış sayacı ve sıfırlama nedeni; komut sonuçları, durum, telemetri ve günlük çıktıları; arka plan işi ve yeniden oynatılması; bellek, sahiplik, dönüş kodları ve hata kuralları; sayı tipleri.

### 1.3 Kapsam dışı (başka yerde tanımlanır)

| Konu | Yer |
|---|---|
| A1 mesajları, tel biçimi, oturum el sıkışması, parti kurma, bariyer, `L_act`, sürekli komutun kullanım süresinin sayısal değeri, olayların yinelenme aralığı | A1 sözleşmesi |
| Algılama kaydının ve algılama partisi başlığının alanları ve doğrulama kuralları | A8 sözleşmesi, `core/include/ida/algilama_kaydi.h` (`ida_det_record_t`, `ida_det_batch_t`) |
| Araç ve otopilot parametrelerinin değerleri, itici sırası, montajlar | A5 sözleşmesi |
| Tekrar oynatma kaydının biçimi | A6 sözleşmesi |
| `ida_core_t`'nin içi, kestirim filtresinin iç denklemleri, parametre tablosunun tam listesi, atış kontrol modülünün çekirdek içindeki arayüzü (`ida_fc_*`) | Çekirdek uygulama işleri (W3; atış kontrol W4) |
| Kart içi kesme düzeni, flash sayfa yerleşimi, gözetim zamanlayıcısının ayarı | Kart yazılımı uygulama işi (W3) |
| Gerçek sensör sürücüleri | Bu aşamanın kapsamı dışında (K8). Sınır, gerçek bir sürücünün aynı örnek yapılarını dolduracağı yerde tutulur; bu bir tasarım hedefidir, güvence değildir |

Atış kontrol sınırı (W1-ortak.md): bu sözleşme atışla ilgili olarak yalnız olay ve durum yapılarını, izin ve engel kurallarını, sanal emniyet girdilerinin geri okunmasını ve algılama kaydının teslim biçimini içerir. Hedefleme, atış ölçütü ve torpidonun yetenekleri bu sözleşmenin konusu değildir.

---

## 2. Terimler ve kabuller

Birimler, çerçeveler, kuaterniyon kuralı ve iki zaman damgası iskelet §3.1'dedir ve burada yinelenmez. Bu sözleşmeye özgü terimler:

| Terim | Anlamı |
|---|---|
| **Ölçüm zamanı** (`ida_stime_t`) | Ölçümün geçerli olduğu an; HIL ve SIL'de sim zamanıdır (iskelet §4). Çekirdek simülasyonu bilmediği için bu adla görür |
| **Duvar saati** (`ida_wtime_t`) | Platformun gerçek zaman saati (kartta açılıştan beri geçen süre; SIL'de sim zamanından türetilmiş sanal saat). Zaman aşımı, gözetim, emniyet süreleri bunu kullanır |
| **Parti** | Bir kontrol periyoduna ait, zaman çerçevesiyle kapanan mesaj kümesi (iskelet §4). Partinin kurulması ve kapanması A1'indir |
| **Zaman çerçevesi** (`tf`) | Partiyi kapatan, partinin sıra numarasını ve sim zamanını taşıyan kayıt |
| **Çağrı** | `ida_core_step` ya da `ida_core_tick`'in bir kez çalışması; arka plan çağrısı ayrıca anılır |
| **Izgara tick'i** | Kartta parti olmadan, donanım zamanlayıcısıyla yapılan `ida_core_tick` çağrısı (`tf.present = 0`, `link_event ≠ SESSION_END`); takvimi A4-K18 |
| **Oturum sonu çağrısı** | Oturum bittiğinde yapılan, ızgara tick'inden ayrı türdeki `ida_core_tick` çağrısı (`tf.present = 0`, `link_event = SESSION_END`; A4-K122) |
| **Etkin emniyet değeri** | Çekirdeğin her emniyet alanı (acil durdurma, tüp başına fiş, tüp başına pim) için tuttuğu değer; A4-K116 |
| **Kısıtlayıcı içerik** | Bir emniyet örneğinin kısıtlayan yönde taşıdığı her değer (acil durdurma basılı, fiş çıkık, pim takılı); önceki değere göre geçiş aranmaz (iskelet §3.2) |
| **Bekleyen parti** | Kapanmış ama henüz çekirdeğe verilmemiş parti; platform en çok bir tane tutar (A4-K102) |
| **Daraltıcı girdi** | İzni azaltan girdi: daraltıcı mandal biti (A4-K44, A4-K46), `SESSION_END` (A4-K73) ya da emniyet girdisinin kapanması (A4-K111) |
| **Genişletici** | İzni artıran ya da atış üreten komut veya olay; kümesi A4-K104'tedir |
| **Hata dönüşü** | `IDA_RC_DUP_SEQ`, `IDA_RC_OLD_SEQ`, `IDA_RC_BAD_INPUT`, `IDA_RC_NOT_INIT`. `IDA_RC_IMU_REJECTED` hata dönüşü değildir (A4-K103) |
| **Nötr güvenli çıktı** | `thr_valid = 1`, `is_safe = 1`, sekiz `thr_cmd` tam olarak `0.0f` (A4-K55) |
| **Kullanılabilir çıktı anı** | Çağrının döndüğü an; platform çıktıyı ancak bu andan sonra kullanabilir |
| **Seri aritmetik** | İskelet §3.1 "Sıra numaraları": `uint32_t` `a`, `b`'den yenidir ⇔ `(a − b) mod 2³²` ∈ [1, 2³¹ − 1]; 0 yinelemedir; 2³¹ ya da büyükse `a` eskidir. Bu tanım `tf.seq`, örnek `seq`'i, `cmd_id` ve olay sayacı için aynıdır; boşluk bu pencereyle sınırlıdır |
| **Kart sayımı** | Kartta "açılıştan beri", SIL'de "son `ida_core_init`'ten beri" |

Kontrol periyodu 5 ms'dir (200 Hz; iskelet §4). Bu sözleşmenin hiçbir kuralı periyot sayısına değil süreye dayanır; kontrol hızı değişirse yalnız periyoda bağlı tahmini değerler (Bölüm 8) yeniden ele alınır.

---

## 3. Veri tanımları

Her yapı `port.h` içinde aynı adla ve aynı alanlarla bulunur. "Z" sütunu: **Z** her durumda anlamlı; **K** koşullu (koşul anlam sütununda); **B** bilgi (çekirdek davranışını değiştirmez). "Geçersizde" sütunu, alan geçersiz olduğunda alıcının ne yaptığını söyler.

### 3.1 Zaman

| Ad | Tip | Birim | Geçerli aralık | Z | Anlamı | Geçersizde |
|---|---|---|---|---|---|---|
| `ida_stime_t.ns` | `int64_t` | ns | 0 … `IDA_TIME_MAX_NS` (2⁶²) | Z | Ölçüm zamanı | Örnek reddedilir (A4-K08, A4-K33) |
| `ida_wtime_t.ns` | `int64_t` | ns | 0 … `IDA_TIME_MAX_NS` | Z | Duvar saati | `now_w`'de: `BAD_INPUT`; `t_rx`'te: örnek reddedilir |

`IDA_TIME_MAX_NS` = 2⁶² ns ≈ 146 yıl: mimari seçimi. Gerekçe: bu sınır altındaki bir zamana bir `uint32_t` süre (≤ 2³² ns) eklenince `int64_t` taşmaz.

**Zaman çerçevesi `ida_time_frame_t`**

| Ad | Tip | Birim | Geçerli aralık | Z | Anlamı | Geçersizde |
|---|---|---|---|---|---|---|
| `present` | `bool` | — | 0, 1 | Z | 0: ızgara tick'i | — |
| `seq` | `uint32_t` | — | tümü (seri aritmetik) | K (`present`) | Partinin sıra numarası; birleştirilmiş girdide en yeni partininki (A4-K102) | A4-K15, A4-K25 |
| `t` | `ida_stime_t` | ns | 3.1 | K | Partinin kapanış anı (sim zamanı; iskelet §4) | `BAD_INPUT` |
| `t_rx` | `ida_wtime_t` | ns | ≤ `now_w` | K | Zaman çerçevesinin alım zamanı | `BAD_INPUT` |

### 3.2 Ortak örnek başlığı `ida_hdr_t`

| Ad | Tip | Birim | Geçerli aralık | Z | Anlamı | Geçersizde |
|---|---|---|---|---|---|---|
| `t_meas` | `ida_stime_t` | ns | 3.1; `tf.present` ise ≤ `tf.t` | Z | Ölçüm zamanı (A4-K13) | Örnek reddedilir |
| `t_rx` | `ida_wtime_t` | ns | ≤ çağrının `now_w`'si | Z | Alım zamanı (A4-K14) | Örnek reddedilir |
| `seq` | `uint32_t` | — | seri aritmetikle öncekinden büyük | Z | Sensörün kendi örnek sayacı | Örnek reddedilir |
| `window_ns` | `uint32_t` | ns | Aralık ölçümünde > 0; anlık ölçümde 0 | Z | Aralık süresi; değerler `[t_meas − window_ns, t_meas]` aralığına aittir | Örnek reddedilir |
| `flags` | `uint16_t` | bit | `IDA_HF_*` | Z | `VALID` (0x0001), `SATURATED` (0x0002), `SENSOR_FAULT` (0x0004) | `VALID` = 0 ise örnek reddedilir |
| `type` | `uint8_t` | — | `IDA_SENS_*` (1–10) | Z | Sensör türü; birlik alanını seçer | Örnek reddedilir |
| `inst` | `uint8_t` | — | 0 … 3 | Z | Aynı türün örnek numarası; montaj parametresi bununla eşlenir | Örnek reddedilir |

Tür başına `window_ns` kuralı: IMU, basınç ve batarya aralık ölçümüdür (`window_ns` > 0); diğerleri anlıktır (`window_ns` = 0).

### 3.3 Sensör örnekleri

Bütün değerler `float` (IEEE-754 binary32), SI, sensör çerçevesinde (iskelet §3.1). "Sonlu" NaN ve sonsuz olmayan demektir; sonlu olmayan değer taşıyan örnek reddedilir (A4-K33). Fiziksel aralık denetimleri çekirdeğin parametresidir ve aralık dışı değer `SATURATED` bayrağıyla ya da çekirdeğin kapısıyla ele alınır; port yalnız sonluluğu şart koşar.

| Yapı ve alan | Tip | Birim | Geçerli aralık | Anlamı |
|---|---|---|---|---|
| **`ida_imu_t`** (`IDA_SENS_IMU`; `t_meas` = aralık sonu, `window_ns` = örnekleme aralığı) | | | | |
| `gyro[3]` | `float` | rad/s | sonlu | IMU çerçevesinde açısal hızın aralık ortalaması |
| `accel[3]` | `float` | m/s² | sonlu | IMU montaj noktasındaki özgül kuvvetin aralık ortalaması; dönme kolu etkileri dahildir (A4-K42) |
| `temp_c` | `float` | °C | sonlu | Sensör sıcaklığı |
| **`ida_mag_t`** (`IDA_SENS_MAG`; anlık) | | | | |
| `field[3]` | `float` | T | sonlu | Manyetik alan, sensör çerçevesi |
| **`ida_baro_t`** (`IDA_SENS_BARO`; `t_meas` = dönüşüm penceresinin sonu, `window_ns` = pencere süresi) | | | | |
| `p_abs` | `float` | Pa | sonlu, > 0 | Mutlak basınç |
| `temp_c` | `float` | °C | sonlu | Sıcaklık |
| **`ida_gnss_t`** (`IDA_SENS_GNSS`; anlık, `t_meas` = çözüm anı) | | | | |
| `lat_e7`, `lon_e7` | `int32_t` | 1e-7 ° | ±900 000 000; ±1 800 000 000 | WGS-84 enlem, boylam |
| `alt_mm` | `int32_t` | mm | tümü | Ortalama deniz seviyesine göre yükseklik |
| `vel_ned[3]` | `float` | m/s | sonlu | NED hız |
| `eph`, `epv` | `float` | m | sonlu, ≥ 0 | Yatay ve dikey konum belirsizliği, **1σ** (HDOP değil) |
| `fix_type` | `uint8_t` | — | `IDA_GNSS_FIX_*` (0–4) | 0 yok, 1 iki boyutlu, 2 üç boyutlu, 3 DGPS, 4 RTK. A1 dönüştürücüsü kendi kodlamasından bu değerlere çevirir |
| `n_sv` | `uint8_t` | — | tümü | Kullanılan uydu sayısı |
| **`ida_dvl_t`** (`IDA_SENS_DVL`; anlık, `t_meas` = geçerlilik anı = yansıma anı) | | | | |
| `vel[3]` | `float` | m/s | sonlu | DVL'nin dibe (ya da suya) göre hızı, DVL çerçevesi |
| `cov[6]` | `float` | (m/s)² | sonlu; `COV_PRESENT` ise pozitif tanımlı | `vel`'in kovaryansı, **DVL çerçevesinde**, üst üçgen sırası xx, xy, xz, yy, yz, zz; gövdeye dönüşüm çekirdeğindir |
| `fom` | `float` | m/s | sonlu, ≥ 0 | Cihazın başarım ölçüsü |
| `beam_dist[4]` | `float` | m | sonlu; ≥ 0 ya da −1 | Işın boyunca mesafe; çözülemeyen ışında −1 |
| `altitude` | `float` | m | sonlu; ≥ 0 ya da −1 | Dibe dik mesafe; bilinmiyorsa −1 |
| `dvl_flags` | `uint8_t` | bit | `IDA_DVL_*` | `VEL_VALID` (0x01), `COV_PRESENT` (0x02), `TRACK_BOTTOM` (0x04; 0 su izi) |
| `beam_valid` | `uint8_t` | bit | 0x00–0x0F | Bit `i`: ışın `i` geçerli |
| **`ida_ping1d_t`** (`IDA_SENS_PING1D`; anlık, `t_meas` = yansıma anı) | | | | |
| `distance` | `float` | m | sonlu, ≥ 0 | Dibe mesafe |
| `scan_start`, `scan_length` | `float` | m | sonlu, ≥ 0 | Tarama penceresi |
| `ping_number` | `uint32_t` | — | tümü | Cihaz sayacı |
| `tx_offset_ns` | `uint32_t` | ns | tümü | Gönderim anı = `t_meas − tx_offset_ns` |
| `confidence` | `uint8_t` | % | 0–100 | Cihazın güveni |
| **`ida_batt_t`** (`IDA_SENS_BATT`; `t_meas` = pencere sonu, `window_ns` = pencere) | | | | |
| `voltage` | `float` | V | sonlu | Paket gerilimi |
| `current` | `float` | A | sonlu | Akım; pozitif = boşalma |
| `temp_c` | `float` | °C | sonlu | Paket sıcaklığı |
| **`ida_leak_t`** (`IDA_SENS_LEAK`; anlık) | | | | |
| `mask` | `uint8_t` | bit | tümü | Bit `i`: sızıntı algılayıcısı `i` ıslak |
| **`ida_safety_t`** (`IDA_SENS_SAFETY`; anlık, örnekleme ya da değişim anı; A4-K34, A4-K75, A4-K111) | | | | |
| `estop` | `uint8_t` | — | 0, 1 | Sanal acil durdurma etkin |
| `tube[2]` | `uint8_t` | bit | `IDA_TUBE_*` | Tüp başına: `LOADED` (0x01) tüp dolu; `PLUG_IN` (0x02) sanal silahlandırma fişi takılı; `PIN_OUT` (0x04) sanal emniyet pimi çıkarılmış; `PERMIT_OPEN` (0x08) fırlatıcının süreli izni şu an açık |

**Kaplar.** IMU dışındaki ve Ping360 dışındaki örnekler `ida_sample_t` içinde taşınır: `hdr` ve `hdr.type`'ın seçtiği birlik alanı `u` (`u.mag`, `u.baro`, `u.gnss`, `u.dvl`, `u.ping1d`, `u.batt`, `u.leak`, `u.safety`). IMU örneği `ida_imu_sample_t` (`hdr` + `imu`) ile ayrı alanda gelir.

**Ping360 tarama satırı `ida_ping360_t`** (`IDA_SENS_PING360`; `t_meas` = darbenin gönderim anı, `window_ns` = 0; iskelet §3.1). Alanlar cihaz birimlerindedir (gerçek sürücünün alan anlamı korunur):

| Ad | Tip | Birim | Geçerli aralık | Aralığın niteliği | Anlamı |
|---|---|---|---|---|---|
| `hdr` | `ida_hdr_t` | — | 3.2 | — | Ortak başlık |
| `angle_grad` | `uint16_t` | gradyan | 0–399 | **Cihaz sınırı** | Tarama açısı (tam tur 400 gradyan) |
| `n_samples` | `uint16_t` | — | 1–1200 | **Cihaz sınırı** (üst); alt sınır port kuralı | Geçerli örnek sayısı |
| `sample_period` | `uint16_t` | 25 ns | ≥ 1 | Birim cihazındır; alt sınır **simülasyon tercihi** (cihazın kabul ettiği daha dar aralığı denetlemek A1'in ve sensör modelinin işidir) | Örnekleme aralığı |
| `tx_duration_us` | `uint16_t` | µs | tümü | Simülasyon tercihi (port daraltmaz) | Darbe süresi |
| `tx_freq_khz` | `uint16_t` | kHz | tümü | Simülasyon tercihi | Darbe frekansı |
| `gain` | `uint8_t` | kod | tümü | Simülasyon tercihi | Cihaz kazanç kodu |
| `data[1200]` | `uint8_t` | — | ilk `n_samples` anlamlı | — | Genlik profili |

Cihaz sınırları (400 gradyan, en çok 1200 örnek, 25 ns birimi) Blue Robotics Ping protokolü belgesindendir (<https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/>); sol incelemesinde bu birincil belgeden doğrulandı (`docs/sozlesmeler/inceleme/A4-sol.md`). Menzil kutusu `i`'nin nominal zamanının formülü A1 sözleşmesinindir (A4-K40).

**Algılama kaydı ve partisi.** Algılama kaydı `ida_det_record_t` ve algılama partisi başlığı `ida_det_batch_t` A8 sözleşmesinde, `core/include/ida/algilama_kaydi.h` başlığında tanımlanır; `port.h` bu başlığı içerir ve iki tipi değerle taşır. Kayıtların ve parti başlıklarının taşımadığı alım zamanı port girdisinde ayrı dizilerde (`det_t_rx`, `det_batch_t_rx`) verilir. Parti başlığı ile kayıtlar arasındaki ilişki (hangi kayıt hangi partiye ait, boş parti, durum başlığı) A8'in kuralıdır; kabul birimi **partidir** (A4-K41; A8-K27, K29).

### 3.4 Komutlar `ida_cmd_t` ve daraltıcı mandallar `ida_narrow_t`

Daraltıcı komutlar (`IDA_CMD_DISARM`; hedefi `GUIDED`/`AUTO` olmayan `IDA_CMD_SET_MODE`; `fc.op` = `FC_DISARM`/`FC_ABORT` olan `IDA_CMD_FC`) çekirdeğe `cmds[]` dizisiyle değil, platformun mandalları `ida_narrow_t` ile gelir (A4-K46). `cmds[]` yalnız daraltıcı olmayan komutları taşır.

| Ad | Tip | Geçerli aralık | Z | Anlamı |
|---|---|---|---|---|
| `cmd_id` | `uint32_t` | tümü | Z | Komut kimliği; sonuç aynı kimlikle (A4-K43, A4-K48) |
| `type` | `uint8_t` | `IDA_CMD_*` (1–16) | Z | Komut türü; birlik alanını seçer. Bilinmeyen tür `UNSUPPORTED` ile yanıtlanır |
| `source` | `uint8_t` | tümü | B | Gönderen sistem kimliği; yalnız kayıt içindir, yetki vermez |
| `u.*` | birlik | aşağıda | K | Türe özgü yük |

| `type` | Yük | Sınıfı (A4-K44, A4-K104) | Anlamı |
|---|---|---|---|
| `HEARTBEAT` (1) | — | nötr | Yer istasyonu canlılığı (A4-K52) |
| `ARM` (2) | — | **genişletici** | Silahlandırma (motorlar) isteği |
| `DISARM` (3) | — | **daraltıcı** (mandal `IDA_NARROW_DISARM`) | Devre dışı bırakma |
| `SET_MODE` (4) | `set_mode.mode` (`IDA_MODE_*`, 2–7) | hedef `GUIDED` ya da `AUTO` ise **genişletici**; değilse **daraltıcı** (mandal `IDA_NARROW_MODE`) | Kip değişimi |
| `MANUAL` (5) | `manual.axes[6]` | nötr | A4-K49 |
| `GUIDED_SP` (6) | `guided` (`ida_guided_sp_t`) | nötr | A4-K50 |
| `MISSION_BEGIN` (7) | `mission_begin.count` (1–128) | nötr | A4-K51 |
| `MISSION_ITEM` (8) | `mission_item` (`ida_mission_item_t`) | nötr | A4-K51 |
| `MISSION_END` (9) | — | nötr | A4-K51 |
| `MISSION_CTL` (10) | `mission_ctl.action` (`IDA_MCTL_START`, `PAUSE`, `RESUME`, `ABORT`) | nötr | Görev başlat, duraklat, sürdür, iptal |
| `PARAM_SET` (11) | `param` (id ve değer) | nötr | A4-K81 |
| `PARAM_GET` (12) | `param.id` | nötr | Değer komut sonucunda döner (A4-K43) |
| `PARAM_COMMIT` (13) | — | nötr | A4-K81 |
| `PARAM_SAVE` (14) | — | nötr | A4-K82, A4-K112 |
| `FAILSAFE_RESET` (15) | — | **genişletici** | Mandallı arıza eylemlerinin temizlenme isteği |
| `FC` (16) | `fc` (`ida_fc_cmd_t`) | `FC_DISARM`, `FC_ABORT`: **daraltıcı** (mandal; bütün tüplere); `FC_ARM`, `FC_FIRE`, `FC_RESET`, `FC_TEST_ENTER`: **genişletici**; `FC_TEST_EXIT`: nötr | Atış kontrol komutu; işlemler kapalı listedir ve A1 Tablo 3.6'daki gibidir; `arg0` = `arg1` = 0 zorunludur (A4-K119); hiçbir işlem hedef seçimi ya da hedef kimliği taşımaz |

**`ida_narrow_t`** (A4-K46):

| Ad | Tip | Aralık | Anlamı |
|---|---|---|---|
| `mask` | `uint8_t` | `IDA_NARROW_DISARM` (0x01), `IDA_NARROW_MODE` (0x02), `IDA_NARROW_FC_DISARM` (0x04), `IDA_NARROW_FC_ABORT` (0x08); tanımlı bitlerin birleşimi `IDA_NARROW_DEFINED` (0x0F), başka bit `BAD_INPUT` | Kurulu mandallar |
| `mode` | `uint8_t` | `MANUAL`, `STABILIZE`, `DEPTH_HOLD`, `STATION_KEEP`; başka değer `NARROW_MODE` kuruluyken `BAD_INPUT` (A4-K46) | `NARROW_MODE` kuruluysa en son gelen daraltıcı hedef kip |
| `cmd_id[4]` | `uint32_t` | tümü | Bit `i`'yi kuran en son komutun kimliği (dizin `i`: 0 DISARM, 1 MODE, 2 FC_DISARM, 3 FC_ABORT) |
| `coalesced[4]` | `uint8_t` | tümü (255'te doyar) | Bit `i`'de en son komuttan önce birleştirilen ek komut sayısı |

**`ida_guided_sp_t`** (A4-K50): `mask` (`IDA_SP_*`) hangi alanların geçerli olduğunu söyler; maskesiz alan yok sayılır.

| Alan | Tip | Birim, çerçeve | Aralık |
|---|---|---|---|
| `pos_ne[2]` | `float` | m, NED kuzey ve doğu, senaryo orijinine göre, gövde referans noktası | sonlu |
| `depth` | `float` | m, yüzeyden aşağı pozitif | sonlu, ≥ 0 |
| `altitude` | `float` | m, dipten yukarı pozitif | sonlu, ≥ 0 |
| `vel_ned[3]` | `float` | m/s, NED | sonlu |
| `yaw` | `float` | rad, kuzeyden doğuya, (−π, π] | sonlu |
| `yaw_rate` | `float` | rad/s, gövde z ekseni etrafında | sonlu |

**`ida_mission_item_t`** (A4-K51): `index` (0 … `count − 1`), `item_type` (`IDA_MI_*`: 1 `WAYPOINT`, 2 `DIVE_TO`, 3 `SURFACE`, 4 `HOLD`, 5 `SCAN_LINES`, 6 `SCAN_POINT`, 7 `WAIT_GNSS`; kapalı liste), `frame` (`NONE`, `LOCAL`, `GLOBAL`), `p[4]`, `lat_e7`, `lon_e7`, `north_m`, `east_m`, `depth`. Bütün alanların tür başına anlamı, birimi, aralığı, izin verilen `frame` değerleri ve kullanılmayan alanların 0 olma koşulu A1 Tablo 3.6a'dadır; bu sözleşme onu yinelemez ve çekirdek aynı denetimi yapar (A4-K51).

**`ida_param_value_t`**: `id` (`uint16_t`), `type` (`IDA_PT_I32` = 1, `IDA_PT_U32` = 2, `IDA_PT_F32` = 3), `v` (birlik: `i32`, `u32`, `f32`).

**`ida_fc_cmd_t`** (A4-K119): `op` (`IDA_FC_*`: 1 `ARM`, 2 `DISARM`, 3 `ABORT`, 5 `FIRE`, 6 `RESET`, 7 `TEST_ENTER`, 8 `TEST_EXIT`; kapalı liste, A1 Tablo 3.6), `tube` (0–1), `arg0`, `arg1` (`uint32_t`; ayrılmış, bu sürümde 0 olmak zorunda).

### 3.5 Olay sonucu `ida_event_result_t`

| Ad | Tip | Birim | Geçerli aralık | Anlamı |
|---|---|---|---|---|
| `event_id` | `uint64_t` | — | A4-K61 | Sonucun ait olduğu olay |
| `result` | `uint8_t` | — | `IDA_EVR_*` | Aşağıda |
| `t_apply` | `ida_stime_t` | ns | 3.1 | `APPLIED` ve `DUPLICATE`'te olayın uygulandığı an; diğerlerinde 0 |

| `result` | Kaynak | Anlamı |
|---|---|---|
| `APPLIED` (1) | Alıcı (sim) | Olay uygulandı |
| `DUPLICATE` (2) | Alıcı | Bu kimlik daha önce uygulanmıştı; yeniden uygulanmadı |
| `REJ_EXPIRED` (3) | Alıcı | Son geçerlilik geçmişti (A4-K66) |
| `REJ_SAFE` (4) | Alıcı | Sanal emniyet girdileri ya da fırlatıcı izni engelledi |
| `REJ_TUBE_EMPTY` (5) | Alıcı | Tüp boş |
| `REJ_SESSION` (6) | Alıcı | Oturum uyuşmadı |
| `DROPPED_GATE` (16) | Platform | İlk kabul kapısı olayı geçirmedi (A4-K67) |
| `DROPPED_NO_SLOT` (17) | Platform | Platformun olay tablosunda yer yoktu |
| `EXPIRED_NO_ACK` (18) | Platform | Olay son geçerliliğine kadar yanıtsız kaldı; platform yinelemeyi bıraktı (A4-K106) |
| `SESSION_END` (19) | Platform | Oturum sona erdi; olay iletilmeyecek (A4-K71) |

### 3.6 Platform durumu `ida_plat_t`

Sayaçlar kart sayımıyla birikir ve 2³²'de sarar; çekirdek farkı mod 2³² alır.

| Ad | Tip | Birim | Aralık | Anlamı | Çekirdeğin kullanımı |
|---|---|---|---|---|---|
| `link_state` | `uint8_t` | — | `IDA_LINK_NONE` (0), `IDA_LINK_RUNNING` (1) | A1 oturumunun durumu | `data_ok` (A4-K89) |
| `link_event` | `uint8_t` | — | `NONE`, `SESSION_START` (1), `SESSION_END` (2) | Oturum olayı; tam bir çağrıda görünür | A4-K72, A4-K73 |
| `param_store` | `uint8_t` | — | `IDA_PSTORE_IDLE/BUSY/OK/FAIL` | Kalıcı kaydın durumu | A4-K83 |
| `param_fail` | `uint8_t` | — | `IDA_PSFAIL_*` | `FAIL` nedeni (`FLASH`, `VERIFY`, `ABORTED`, `TOO_BIG`) | Günlük |
| `param_gen` | `uint32_t` | — | tümü | Son başarılı kaydın kuşağı | A4-K83 |
| `prev_overrun` | `bool` | — | 0, 1 | Önceki çekirdek çağrısı A4-K23 bütçesini aştı | Kaçırılan adım sayımı |
| `tx_stalled` | `bool` | — | 0, 1 | Kritik gönderim kuyruğu boşalmıyor | Günlük; arıza kararına girmez |
| `batch_merged` | `bool` | — | 0, 1 | Bu çağrının girdisi birden çok partiden birleşti (A4-K102) | Kaçırılan adım sayımı; günlük |
| `input_dropped` | `bool` | — | 0, 1 | Bu çağrının girdisinde kapasite yüzünden üye düştü | `health.input_dropped` |
| `prev_call_ns` | `uint32_t` | ns | tümü; 0 ölçülmedi | Önceki çağrının süresi | Günlük |
| `cnt_batch_skipped` | `uint32_t` | — | sayaç | Hiç üyesi gelmeden atlanan parti | Kaçırılan adım sayımı |
| `cnt_batch_incomplete` | `uint32_t` | — | sayaç | Eksik kapanan parti | Günlük |
| `cnt_batch_old` | `uint32_t` | — | sayaç | Atılan eski parti | Günlük |
| `cnt_batch_merged` | `uint32_t` | — | sayaç | Başka bir partiyle birleştirilen parti (A4-K102) | Kaçırılan adım sayımı |
| `cnt_stale_session` | `uint32_t` | — | sayaç | Eski oturum çerçevesi | Günlük |
| `cnt_member_overflow` | `uint32_t` | — | sayaç | Kapasiteyi aşıp düşen örnek, kayıt ya da Ping360 satırı (A4-K32, A4-K102) | Günlük |
| `cnt_cmd_deferred` | `uint32_t` | — | sayaç | Ertelenen daraltıcı olmayan komut (A4-K105) | Günlük |
| `cnt_cmd_dropped` | `uint32_t` | — | sayaç | Atılan daraltıcı olmayan komut (A4-K105) | Günlük |
| `cnt_narrow_coalesced` | `uint32_t` | — | sayaç | Mandalda birleştirilen daraltıcı komut (A4-K46) | Günlük |
| `cnt_grid_missed` | `uint32_t` | — | sayaç | Kaçırılan ızgara anı | Günlük |
| `cnt_rx_error` | `uint32_t` | — | sayaç | CRC ve çerçeveleme hatası | Günlük |
| `cnt_out_fixed` | `uint32_t` | — | sayaç | Platformun nötrle değiştirdiği çıktı (A4-K59) | Günlük |

### 3.7 Çağrı girdisi `ida_in_t`

`ida_core_step` ve `ida_core_tick` aynı girdi yapısını alır.

| Ad | Tip | Aralık | Z | Anlamı | Geçersizde |
|---|---|---|---|---|---|
| `now_w` | `ida_wtime_t` | 3.1 | Z | Çağrının duvar saati | `BAD_INPUT` (aralık dışı); A4-K10 (geri adım) |
| `tf` | `ida_time_frame_t` | 3.1 | Z | Partinin zaman çerçevesi; ızgara tick'inde `present = 0` | A4-K15 |
| `imu_present` | `bool` | step'te 1, tick'te 0 | Z | IMU örneği var mı | `BAD_INPUT` (A4-K17) |
| `imu` | `ida_imu_sample_t` | 3.3 | K (`imu_present`) | IMU örneği | `IDA_RC_IMU_REJECTED` (A4-K103) |
| `n_samples` | `uint8_t` | 0–16 | Z | `samples` dizisindeki örnek sayısı | `BAD_INPUT` |
| `samples[16]` | `ida_sample_t` | 3.3 | K | IMU ve Ping360 dışı örnekler | Tek tek reddedilir |
| `ping360` | `const ida_ping360_t *` | NULL ya da geçerli | K | En çok bir tarama satırı | Satır reddedilir |
| `n_det_batch` | `uint8_t` | 0–2 | Z | Algılama partisi başlığı sayısı | `BAD_INPUT` |
| `det_batch[2]` | `ida_det_batch_t` | A8 | K | Algılama partisi başlıkları (A8) | Başlık reddedilir |
| `det_batch_t_rx[2]` | `ida_wtime_t` | ≤ `now_w` | K | Başlık `i`'nin alım zamanı | Başlık reddedilir |
| `n_det` | `uint8_t` | 0–8 | Z | Algılama kaydı sayısı | `BAD_INPUT` |
| `det[8]` | `ida_det_record_t` | A8 | K | Algılama kayıtları (A8) | Kayıt reddedilir |
| `det_t_rx[8]` | `ida_wtime_t` | ≤ `now_w` | K | Kayıt `i`'nin alım zamanı | Kayıt reddedilir |
| `narrow` | `ida_narrow_t` | 3.4 | Z | Daraltıcı mandallar (A4-K46) | Tanımsız bit ya da geçersiz hedefli `NARROW_MODE`: `BAD_INPUT`; tanımlı bitler her durumda uygulanır (A4-K113 (b)) |
| `n_cmds` | `uint8_t` | 0–8 | Z | Daraltıcı olmayan komut sayısı | `BAD_INPUT` (A4-K31) |
| `cmds[8]` | `ida_cmd_t` | 3.4 | K | Daraltıcı olmayan komutlar, varış sırasıyla | Tek tek yanıtlanır |
| `n_evres` | `uint8_t` | 0–4 | Z | Olay sonucu sayısı | `BAD_INPUT` |
| `evres[4]` | `ida_event_result_t` | 3.5 | K | Olay sonuçları | A4-K69 |
| `bg_slices` | `uint32_t` | tümü | Z | Önceki step/tick çağrısından beri bu örnekte dönen `ida_core_background` çağrısı sayısı (A4-K109) | Sayılır (A4-K109) |
| `plat` | `ida_plat_t` | 3.6 | Z | Platform durumu | — |

### 3.8 Çağrı çıktısı `ida_out_t`

Hangi alanın hangi dönüş kodunda geçerli olduğu Bölüm 4.14'teki tablodadır.

| Ad | Tip | Birim | Aralık | Anlamı |
|---|---|---|---|---|
| `thr_valid` | `bool` | — | 0, 1 | Bu çıktı sürekli komut taşıyor (A4-K28, A4-K56) |
| `is_safe` | `bool` | — | 0, 1 | Güvenli sınıf (A4-K55) |
| `armed` | `bool` | — | 0, 1 | Çağrı sonundaki silahlandırma (motor) durumu |
| `src` | `uint8_t` | — | `IDA_SRC_STEP` (1), `IDA_SRC_TICK` (2) | Çıktıyı üreten işlev |
| `fs_level` | `uint8_t` | — | `IDA_FS_*` (0–5) | Etkin arıza eylemi: `NONE`, `WARN`, `HOLD`, `SURFACE`, `PASSIVE_SURFACE`, `DISARM` |
| `seq` | `uint32_t` | — | tümü | A4-K57 |
| `t_ref` | `ida_stime_t` | ns | 3.1 | A4-K57 |
| `valid_for_ns` | `uint32_t` | ns | `thr_valid = 1` ise 60–450 ms; değilse 0 | Sürekli komutun kullanım süresi (A4-K58) |
| `thr_cmd[8]` | `float` | birimsiz | [−1, 1], sonlu | Normalleştirilmiş ESC komutu, itici sırası A5'tedir (A4-K54) |
| `n_events`, `events[4]` | `uint8_t`, `ida_event_t` | — | 0–4 | Yeni tek seferlik olaylar (A4-K60–K65) |
| `n_results`, `results[12]` | `uint8_t`, `ida_cmd_result_t` | — | 0–12 | Komut kimliğiyle çoklanmış komut sonuçları (A4-K43) |
| `nav_valid`, `nav` | `bool`, `ida_nav_out_t` | — | — | Seyrüsefer çıkışı (A4-K87) |
| `status` | `ida_status_t` | — | — | Durum ve sayaçlar (A4-K88) |
| `health` | `ida_health_t` | — | — | Sağlık bitleri (A4-K89) |
| `n_logs`, `logs[4]` | `uint8_t`, `ida_log_t` | — | 0–4 | Günlük kayıtları (A4-K90) |
| `fc` | `ida_fc_status_t` | — | — | Atış kontrol durumu (A4-K91) |
| `n_det_tx`, `det_tx[4]` | `uint8_t`, `ida_det_record_t` | — | 0–4 | Kart içi sonar algılama kayıtları, değerle (A4-K92) |
| `bg` | `ida_bg_report_t` | — | — | Arka plan raporu (A4-K94) |

**`ida_event_t`** (A4-K60):

| Ad | Tip | Birim | Aralık | Anlamı |
|---|---|---|---|---|
| `event_id` | `uint64_t` | — | A4-K61 | `(boot_count << 32) \| sayaç` |
| `kind` | `uint8_t` | — | `IDA_EVT_SAFETY_OPEN` (1), `SAFETY_CLOSE` (2), `FIRE` (3) | Olay türü |
| `tube` | `uint8_t` | — | 0–1; `SAFETY_CLOSE`'ta ayrıca `IDA_TUBE_ALL` (0xFF) | Hedef tüp |
| `seq` | `uint32_t` | — | tümü | Kararın verildiği çıktının `seq`'i |
| `t_decide` | `ida_stime_t` | ns | 3.1 | Kararın verildiği çıktının `t_ref`'i |
| `apply_by_ns` | `uint32_t` | ns | 16–1000 ms | Son geçerlilik: `t_decide + apply_by_ns` (A4-K66) |
| `hold_ns` | `uint32_t` | ns | `SAFETY_OPEN`'da 60–450 ms; diğerlerinde 0 | İznin süresi (A4-K66, A4-K107) |

**`ida_cmd_result_t`** (A4-K43):

| Ad | Tip | Aralık | Anlamı |
|---|---|---|---|
| `cmd_id` | `uint32_t` | tümü | Sonucun ait olduğu komut ya da mandalın `cmd_id`'si |
| `kind` | `uint8_t` | `IDA_RES_ACK` (1), `IDA_RES_PARAM_VALUE` (2), `IDA_RES_PARAM_SAVE` (3) | Sonuç türü; `value` ve `save`'in hangisinin geçerli olduğunu seçer |
| `ack` | `uint8_t` | `IDA_ACK_ACCEPTED` 1, `REJECTED` 2, `IN_PROGRESS` 3, `DUPLICATE` 4, `UNSUPPORTED` 5 | Kabul ya da ret |
| `reason` | `uint16_t` | 0–15 (`IDA_REASON_*`); 16–127 ayrılmış; 128–191 A1 bağlantı katmanının, port sonucu değil; 192 ve üstü ayrılmış, kullanılmaz (A4-K120) | Ret nedeni |
| `value` | `ida_param_value_t` | — | `kind = PARAM_VALUE`: `PARAM_GET`'in değeri ya da `PARAM_SET`'te bekletilen değer |
| `save` | `ida_param_save_t` | — | `kind = PARAM_SAVE` ve `ack = ACCEPTED`: `gen`, `blob`, `len` (A4-K82) |

**`ida_nav_out_t`** (A4-K87): `t` (çıkış durumunun ölçüm zamanı); `p_ned[3]` m (senaryo orijinine göre, gövde referans noktası); `v_ned[3]` m/s; `q_nb[4]` (w, x, y, z; Hamilton; gövde → NED); `omega_b[3]` rad/s; `depth` m (aşağı pozitif); `altitude` m (dibe; bilinmiyorsa −1); `p_sigma[3]` m, `v_sigma[3]` m/s, `att_sigma[3]` rad (1σ); `origin_lat_e7`, `origin_lon_e7` (orijin, WGS-84); `est_level` (`IDA_EST_INIT/OK/DEGRADED/BAD/FAULT`); `reset_counter` (konum sıfırlamasında artar, 8 bit sarar).

**`ida_status_t`** (A4-K88): `mode` (`IDA_MODE_*`), `armed`, `fs_level`, `fs_reasons` (`IDA_FSR_*` bit maskesi: sızıntı, yer istasyonu kaybı, düşük ve kritik batarya, derinlik sınırı, en az irtifa, coğrafi sınır, kestirim bozulması ve kötülüğü, sayısal çökme, IMU bayat, IMU gecikmeli, adım kaçırma, parametre, oturum), `regime` (`IDA_REG_*`), `phase` (`IDA_PH_*`), `mission_state` (`IDA_MS_*`), `mission_item`, `param_ok`, `events_ok`, `boot_count`, `cnt`.

**`ida_core_counters_t`** (`status.cnt`; init'ten beri birikir, 2³²'de sarar): `sample_rejected` (A4-K33), `sample_future` (A4-K14, A4-K33), `sample_conflict` (çelişkili eşit anahtarlı çift başına bir; A4-K115), `sample_dup_dropped` (bayt bayt aynı yinelenme; A4-K115), `narrow_from_rejected` (reddedilen emniyet örneğinden uygulanan daraltma; A4-K116), `bg_bad_input` (A4-K117), `imu_rejected` (A4-K103), `imu_gap` (A4-K38), `bad_input`, `dup_seq`, `old_seq` (dönüş kodu sayıları), `wall_backstep` (A4-K10), `cmd_rejected` (`REJECTED` yanıtlanan komut), `widen_blocked` (A4-K45, A4-K108 ile engellenen genişletici komut ya da olay), `evres_ignored` (A4-K69), `event_unknown` (A4-K70), `det_rejected` (A4-K41), `ping360_rejected` (A4-K40), `bg_dropped` (A4-K94), `bg_slice_mismatch` (A4-K109), `log_dropped` (A4-K90), `step_miss` (`prev_overrun` gören çağrılar, `cnt_batch_skipped` artışları ve `cnt_batch_merged` artışlarının toplamı).

**`ida_health_t`** (A4-K89): `core_ok` (iç tutarlılık; gözetimi yalnız bu besler), `data_ok` (taze IMU ve `IDA_LINK_RUNNING`), `imu_rejected` (bu çağrının IMU'su reddedildi; A4-K103), `imu_lag` (`IMU_LAG` mandalı kurulu; A4-K37), `input_dropped` (bu çağrının girdisinde üye düştü; A4-K102), `narrowed` (bu çağrıda daraltma uygulandı; A4-K45, A4-K113, A4-K116), `sample_conflict` (bu çağrıda çelişkili eşit anahtarlı örnek çifti vardı; A4-K115), `bg_bad_input` (önceki step/tick çağrısından beri en az bir arka plan çağrısı `IDA_BG_BAD_INPUT` döndü; A4-K117).

**`ida_log_t`** (A4-K90): `t_w` (çağrının kullanılan `now_w`'si), `code` (`IDA_LOG_*`; kapalı tablo 3.15), `severity` (`IDA_SEV_*`, 0–7; MAVLink `MAV_SEVERITY` ile aynı sıra ve değerler: 0 `EMERGENCY` … 7 `DEBUG`; kaynak: `mavlink/mavlink` deposu `message_definitions/v1.0/common.xml`, `master` 3fbb88560a06, 2026-11-19'da okundu), `arg0`, `arg1` (`int32_t`; anlamları koda göre 3.15'te, kullanılmayan 0).

**`ida_fc_status_t`** (A4-K91): `state` (`IDA_FCS_*`; kapalı tablo 3.16), `inhibit_mask` (`IDA_FCI_*`; kapalı tablo 3.16), `last_event_id` (son çıkan olay; yoksa 0), `last_event_result` (`IDA_EVR_*`; sonuç yoksa 0). Opak alan yoktur.

**`ida_param_save_t`** (A4-K82): `gen`, `blob` (çekirdeğin belleğinde), `len` (≤ 8000).

**`ida_bg_report_t`** (A4-K94): `submitted` (bu çağrıda yeni arka plan işi gönderildi), `dropped` (bu çağrıda düşürülen iş sayısı).

### 3.9 Başlatma ve bilgi yapıları

**`ida_init_in_t`**:

| Ad | Tip | Aralık | Z | Anlamı | Geçersizde |
|---|---|---|---|---|---|
| `port_major`, `port_minor` | `uint16_t` | — | Z | Platformun derlendiği `IDA_PORT_VERSION_*` | `IDA_RC_VERSION` (A4-K02) |
| `now_w` | `ida_wtime_t` | 3.1 | Z | Kurulum anının duvar saati | `BAD_INPUT` |
| `boot_count` | `uint32_t` | 1 … 0xFFFFFFFE | K (`boot_count_ok`) | Açılış sayacı (A4-K86) | Olaysız çalışma (A4-K61) |
| `boot_count_ok` | `bool` | 0, 1 | Z | 0: sayaç ayrılamadı ya da tükendi | — |
| `reset_cause` | `uint8_t` | `IDA_RESET_*` (0–8) | B | Sıfırlama nedeni (yorumlanmış) | 0 sayılır |
| `reset_flags` | `uint32_t` | tümü | B | Platformun ham sıfırlama bayrakları | — |
| `consecutive_faults` | `uint8_t` | tümü | B | Platformun ardışık arıza sayacı | — |
| `param_src` | `uint8_t` | `IDA_PSRC_NONE/FLASH/SIL` | B | Bloğun kaynağı | — |
| `param_blob`, `param_len` | `const uint8_t *`, `uint32_t` | `param_blob` NULL ise `param_len` = 0; değilse `param_len` ≤ 8000 | K | Kodlanmış parametre bloğu (3.10) | NULL ile `param_len` > 0: `BAD_INPUT`; geçersiz blok: varsayılanlar (A4-K78) |

**`ida_core_info_t`** (A4-K98): `port_major`, `port_minor`, `core_size` (`sizeof(ida_core_t)`), `core_align`, `param_schema_hash` (A4-K79), `core_version`.

**`ida_param_desc_t`** (A4-K85): `id`, `type` (`IDA_PT_*`), `cls` (`IDA_PCLS_A` = çalışırken değişebilir, `IDA_PCLS_B` = yalnız devre dışıyken), `name[17]` (en çok 16 karakter + NUL; MAVLink `PARAM_VALUE.param_id` alanı `char[16]`'dır, kaynak: aynı `common.xml`), `min`, `max`, `def` (`ida_param_value_t`).

### 3.10 Kodlanmış parametre bloğu (bayt düzeni)

Blok, platformun kalıcı bellekte yorumlamadan sakladığı ve SIL koşucusunun A5'ten ürettiği bayt dizisidir. Bütün çok baytlı alanlar küçük uçludur.

| Konum (B) | Boy (B) | Alan | Değer |
|---|---|---|---|
| 0 | 4 | `magic` | `0x50414449` ("IDAP" baytları `49 44 41 50`) |
| 4 | 2 | `fmt_ver` | 1 |
| 6 | 2 | `rsv` | 0 |
| 8 | 8 | `schema_hash` | Çekirdeğin parametre şemasının özeti (`ida_core_info_t.param_schema_hash`) |
| 16 | 4 | `payload_len` | Yükün bayt sayısı; 8'in katı |
| 20 | 4 | `payload_crc` | Yükün CRC-32/ISO-HDLC değeri |
| 24 | `payload_len` | yük | 8 baytlık kayıtlar |

Kayıt (8 B): `param_id` (u16), `type` (u8, `IDA_PT_*`), `rsv` (u8 = 0), `value` (4 B; `I32` ikiye tümleyen, `U32`, `F32` IEEE-754 binary32 bitleri). Kayıtlar `param_id`'ye göre artan sıradadır.

Toplam blok boyu `24 + payload_len` ≤ `IDA_PARAM_BLOB_MAX` = 8000 B (mimari seçimi; kartın 8 KB'lık flash sayfasına platformun kendi başlığı ve tamamlanma kaydıyla sığması için; 01 §2.9 yerleşimi). Bu boyda en çok 997 kayıt bulunur.

CRC-32/ISO-HDLC: çokterimli 0x04C11DB7, yansıtılmış giriş ve çıkış, başlangıç 0xFFFFFFFF, son XOR 0xFFFFFFFF; "123456789" için denetim değeri 0xCBF43926 (kaynak: Greg Cook, CRC RevEng katalogu, <https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat.crc-32-iso-hdlc>; denetim değeri bu işte Python `zlib.crc32` ile, sol incelemesinde bağımsız bit hesabıyla doğrulandı).

`schema_hash`: çekirdeğin parametre tablosundan (her parametrenin `id`, `name`, `type` üçlüsü) kurulan kanonik metnin SHA-256 özetinin ilk 8 baytı (küçük uçlu `uint64_t`). Kanonik metnin tam biçimi çekirdek uygulama işinde sabitlenir; SIL koşucusu özeti `ida_core_info`'dan alır, kendisi hesaplamaz.

### 3.11 Dönüş kodları

Step, tick, init ve parametre işlevleri `ida_rc_t`, arka plan işlevi `ida_bg_rc_t` (`IDA_BG_IDLE` = 0, `IDA_BG_MORE` = 1, `IDA_BG_BAD_INPUT` = 2) döner. Bu iki tip yalnız dönüş değeridir, hiçbir yapı alanında kullanılmaz. İşlev başına hangi kodların dönebileceği Bölüm 4.15'tedir; çıktının içeriği Bölüm 4.14'tedir.

| Kod | Değer | Sınıf | Anlamı |
|---|---|---|---|
| `IDA_RC_OK` | 0 | kabul | Olağan |
| `IDA_RC_DUP_SEQ` | 1 | hata (ret) | `tf.present` ve `tf.seq` son kabul edilene eşit |
| `IDA_RC_OLD_SEQ` | 2 | hata (ret) | `tf.present` ve `tf.seq` seri aritmetikle son kabul edilenden küçük |
| `IDA_RC_BAD_INPUT` | 3 | hata (ret) | Yapısal girdi hatası (aşağıdaki (c) adımı) ya da 4.15'teki işaretçi önkoşulu |
| `IDA_RC_NOT_INIT` | 4 | hata | Örnek başarıyla kurulmamış |
| `IDA_RC_PARAM_DEFAULTS` | 5 | yalnız init | Örnek kuruldu ama blok geçersiz; varsayılanlar yüklü, arm engelli |
| `IDA_RC_VERSION` | 6 | yalnız init | Port sürümü uyuşmuyor; örnek kurulmadı |
| `IDA_RC_NOT_FOUND` | 7 | yalnız parametre işlevleri | İndis, ad ya da kimlik yok |
| `IDA_RC_IMU_REJECTED` | 8 | yalnız step; kabul (hata değil) | IMU örneği reddedildi; çağrı tick kurallarıyla sürdü (A4-K103) |

"Reddedilen çağrı" `BAD_INPUT`, `DUP_SEQ` ya da `OLD_SEQ` dönen step/tick çağrısıdır; "kabul edilen çağrı" `OK` ya da `IMU_REJECTED` dönendir.

**Step ve tick'in değerlendirme sırası (A4-K113).** Çekirdek her step ve tick çağrısını şu sırayla değerlendirir; ilk tutan adımın dönüş kodu çağrının dönüş kodudur:

| Adım | Denetim | Sonuç |
|---|---|---|
| (a1) | `c` ya da `in` NULL | `BAD_INPUT`; hiçbir şey okunmaz, yazılmaz, uygulanmaz |
| (a2) | Örnek kurulmamış | `NOT_INIT`; hiçbir daraltma uygulanmaz (uygulanacak durum yok); `out` NULL değilse 4.14 `NOT_INIT` satırı |
| (b) | Daraltma: `in.narrow`'un tanımlı bitleri (`IDA_NARROW_DEFINED` = 0x0F), `plat.link_event = SESSION_END` ve `samples[]`'in ilk `min(n_samples, 16)` girdisi arasındaki yapısal olarak geçerli her emniyet örneğinin (kabul edilecek ya da reddedilecek) A4-K116'daki kısıtlayıcı içeriği uygulanır | Dönüş kodu belirlemez; bu adım `in` okunabilen **her** çağrıda (`out` NULL olsa da, çağrı (c) ya da (d)'de reddedilse de) yapılır ve geri alınmaz |
| (b2) | `out` NULL | `BAD_INPUT`; (b) uygulanmış kalır, başka hiçbir şey yapılmaz ve yazılmaz |
| (c) | Yapısal doğrulama: `now_w` aralık dışı (A4-K08); `tf.present` iken `tf.t` aralık dışı ya da `tf.t_rx` > `now_w`; `tf.seq` arttığı halde `tf.t` artmamış (A4-K15); step'te `imu_present = 0` ya da tick'te `imu_present = 1` (A4-K17); bir sayı alanı kapasiteyi aşıyor (A4-K31); `narrow.mask`'te tanımsız bit ya da `NARROW_MODE` kuruluyken `narrow.mode` daraltıcı bir hedef değil (A4-K46) | `BAD_INPUT` |
| (d) | Sıra denetimi (`tf.present` iken; A4-K25) | `DUP_SEQ` ya da `OLD_SEQ` |
| (e) | Olağan işleniş; IMU doğrulaması yalnız burada (A4-K103) | `OK` ya da (yalnız step) `IMU_REJECTED` |

**Birlikte oluşabilen koşulların önceliği.** Tablo, aynı çağrıda iki koşul birlikte tutarsa dönüş kodunu verir (soldan sağa okunur; köşegen dışındaki her çift tek kez yazılmıştır):

| Birlikte | `NOT_INIT` | `BAD_INPUT` | `DUP_SEQ` | `OLD_SEQ` | `IMU_REJECTED` |
|---|---|---|---|---|---|
| `c`/`in` NULL | `BAD_INPUT` | `BAD_INPUT` | `BAD_INPUT` | `BAD_INPUT` | `BAD_INPUT` |
| `NOT_INIT` | — | `NOT_INIT` | `NOT_INIT` | `NOT_INIT` | `NOT_INIT` |
| `BAD_INPUT` ((b2) `out` NULL dahil) | | — | `BAD_INPUT` | `BAD_INPUT` | `BAD_INPUT` |
| `DUP_SEQ` | | | — | birlikte oluşmaz | `DUP_SEQ` |
| `OLD_SEQ` | | | | — | `OLD_SEQ` |

Daraltıcı girdinin bulunması dönüş kodunu değiştirmez; yalnız (b) adımında uygulanır ve kabul edilen çağrıda A4-K45'i tetikler.

**Init'in değerlendirme sırası.** (1) `c` NULL → `BAD_INPUT`, hiçbir şey yazılmaz; (2) `in` NULL → örnek kurulmamış işaretlenir, `BAD_INPUT`; (3) sürüm uyuşmazlığı → örnek kurulmamış işaretlenir, `VERSION`; (4) `now_w` aralık dışı ya da `param_blob`/`param_len` çelişkili → örnek kurulmamış işaretlenir, `BAD_INPUT`; (5) blok geçersiz ya da yok → örnek kurulur, `PARAM_DEFAULTS`; (6) `OK`. İlk tutan adım kazanır.

**Arka plan çağrısının değerlendirme sırası (A4-K117).** (1) `c` NULL → `IDA_BG_BAD_INPUT`; (2) `now_w` aralık dışı → `IDA_BG_BAD_INPUT`; (3) örnek kurulmamış → `IDA_BG_IDLE`; (4) dilim çalışır → `IDA_BG_IDLE` ya da `IDA_BG_MORE`.

### 3.12 Kapasiteler

| Sabit | Değer | Dayanak |
|---|---:|---|
| `IDA_PORT_N_THR` | 8 | İskelet §5 (sekiz itici) |
| `IDA_PORT_N_TUBE` | 2 | İskelet §5 (iki fırlatıcı) |
| `IDA_PORT_MAX_INST` | 4 | Mimari seçimi (tahmin) |
| `IDA_PORT_MAX_SAMPLES` | 16 | 02 §3.8 (tahmin) |
| `IDA_PORT_MAX_CMDS` | 8 | 02 §3.8 (tahmin); yalnız daraltıcı olmayan komutlar |
| `IDA_PORT_N_NARROW` | 4 | Daraltıcı tür sayısı (A4-K44) |
| `IDA_PORT_MAX_RESULTS` | 12 | `MAX_CMDS` + `N_NARROW`: bir çağrıda işlenebilen en çok komut sayısı (A4-K43) |
| `IDA_PORT_MAX_DET_IN` / `IDA_PORT_MAX_DET_OUT` | 8 / 4 | 03 §3.1 (tahmin) |
| `IDA_PORT_MAX_DET_BATCH` | 2 | Mimari seçimi (tahmin): birleştirilmiş girdide iki partinin başlıkları |
| `IDA_PORT_MAX_EVENTS` / `IDA_PORT_MAX_EVRES` | 4 / 4 | 02 §3.4 (tahmin) |
| `IDA_PORT_MAX_LOGS` | 4 | Mimari seçimi (tahmin) |
| `IDA_PORT_CMD_DEDUP` | 32 | Mimari seçimi (tahmin) |
| `IDA_PING360_MAX_SAMPLES` | 1200 | Cihaz sınırı (3.3) |
| `IDA_PARAM_BLOB_MAX` | 8000 | Mimari seçimi (3.10) |
| `IDA_PARAM_NAME_MAX` | 16 | MAVLink `PARAM_VALUE.param_id` `char[16]` (3.9) |

### 3.13 Diğer sabitler

| Sabit | Değer | Kural |
|---|---|---|
| `IDA_PORT_VERSION_MAJOR`, `IDA_PORT_VERSION_MINOR` | 1, 0 | A4-K02 |
| `IDA_TIME_MAX_NS` | 2⁶² | A4-K08 |
| `IDA_PORT_TICK_MAX_GAP_NS`, `IDA_PORT_DISPATCH_MAX_NS`, `IDA_PORT_START_MAX_NS` | 11 ms, 0,5 ms, 4 ms | A4-K18, A4-K19 |
| `IDA_PORT_STEP_BUDGET_NS`, `IDA_PORT_TICK_BUDGET_NS`, `IDA_PORT_BG_BUDGET_NS` | 3,5 ms, 0,5 ms, 0,3 ms | A4-K23 |
| `IDA_PORT_STALE_MARGIN_NS` | 14,5 ms | A4-K39 |
| `IDA_EVT_RESULT_MARGIN_NS` | 100 ms | A4-K70 |
| `IDA_VALID_FOR_MIN_NS`, `IDA_VALID_FOR_MAX_NS`, `IDA_VALID_FOR_DEFAULT_NS` | 60 ms, 450 ms, 100 ms | A4-K58 (sahibi A1-K104) |
| `IDA_APPLY_BY_MIN_NS`, `IDA_APPLY_BY_MAX_NS`, `IDA_HOLD_MIN_NS`, `IDA_HOLD_MAX_NS` | 16 ms, 1000 ms, 60 ms, 450 ms | A4-K66 (sahibi A1-K104) |
| `IDA_IMU_STALE_MIN_NS`, `IDA_IMU_STALE_MAX_NS`, `IDA_IMU_STALE_DEFAULT_NS` | 5 ms, 30 ms, 20 ms | A4-K39 |
| `IDA_DET_STALE_DEFAULT_NS` | 2 s (tasarım seçimi) | A4-K118 |
| `IDA_REASON_CORE_LAST`, `IDA_REASON_LINK_FIRST`, `IDA_REASON_LINK_LAST` | 15, 128, 191 | A4-K120 |
| `IDA_LOG_CODE_LAST` | 23 | A4-K90 (3.15) |
| `IDA_FCI_DEFINED` | 0x0003FFFF | A4-K91 (3.16) |
| `IDA_BOOT_COUNT_MIN`, `IDA_BOOT_COUNT_MAX` | 1, 0xFFFFFFFE | A4-K61 |
| `IDA_EVT_CTR_FIRST`, `IDA_EVT_CTR_CLOSE_ONLY`, `IDA_EVT_CTR_LAST` | 1, 0xFFFFFFF0, 0xFFFFFFFF | A4-K61, A4-K62 |
| `IDA_PING360_ANGLE_COUNT` | 400 | A4-K40 |
| `IDA_PBLOB_MAGIC`, `IDA_PBLOB_FMT_VER`, `IDA_PBLOB_HDR_SIZE`, `IDA_PBLOB_REC_SIZE` | 0x50414449, 1, 24, 8 | A4-K79 (3.10) |
| `IDA_LINK_NONE`, `IDA_LINK_RUNNING` | 0, 1 | A4-K72 |
| `IDA_LINKEV_NONE`, `IDA_LINKEV_SESSION_START`, `IDA_LINKEV_SESSION_END` | 0, 1, 2 | A4-K72, A4-K73 |
| `IDA_PSTORE_IDLE`, `IDA_PSTORE_BUSY`, `IDA_PSTORE_OK`, `IDA_PSTORE_FAIL` | 0–3 | A4-K83 |
| `IDA_PSFAIL_NONE`, `_FLASH`, `_VERIFY`, `_ABORTED`, `_TOO_BIG` | 0–4 | A4-K83 |
| `IDA_PSRC_NONE`, `IDA_PSRC_FLASH`, `IDA_PSRC_SIL` | 0–2 | A4-K78 |
| `IDA_EST_INIT`, `IDA_EST_OK`, `IDA_EST_DEGRADED`, `IDA_EST_BAD`, `IDA_EST_FAULT` | 0–4 | A4-K87 |

Diğer kod aileleri (`IDA_SENS_*`, `IDA_HF_*`, `IDA_GNSS_FIX_*`, `IDA_DVL_*`, `IDA_TUBE_*`, `IDA_CMD_*`, `IDA_MODE_*`, `IDA_SP_*`, `IDA_MI_*`, `IDA_MCTL_*`, `IDA_PT_*`, `IDA_FC_*`, `IDA_NARROW_*`, `IDA_EVR_*`, `IDA_SRC_*`, `IDA_FS_*`, `IDA_EVT_*`, `IDA_RES_*`, `IDA_ACK_*`, `IDA_REASON_*`, `IDA_FSR_*`, `IDA_REG_*`, `IDA_PH_*`, `IDA_MS_*`, `IDA_SEV_*`, `IDA_RESET_*`, `IDA_PCLS_*`, `IDA_LOG_*`, `IDA_FCS_*`, `IDA_FCI_*`) değerleriyle `port.h`'tadır ve yukarıdaki tablolarda anlamlarıyla anılır.

### 3.14 Birlikler ve seçiciler (A4-K121)

Port yapılarındaki her birliğin (ve koşullu alanın) etkin üyesini yalnız aşağıdaki seçici belirler. Etkin olmayan üyenin baytları anlamsızdır; A6 kodlaması yalnız etkin üyeyi yazar.

| Yapı ve birlik | Seçici alan | Seçici değeri → etkin üye |
|---|---|---|
| `ida_sample_t.u` | `hdr.type` | `IDA_SENS_MAG` → `mag`; `BARO` → `baro`; `GNSS` → `gnss`; `DVL` → `dvl`; `PING1D` → `ping1d`; `BATT` → `batt`; `LEAK` → `leak`; `SAFETY` → `safety`; başka değer: örnek reddedilir (A4-K30) |
| `ida_cmd_t.u` | `type` | `SET_MODE` → `set_mode`; `MANUAL` → `manual`; `GUIDED_SP` → `guided`; `MISSION_BEGIN` → `mission_begin`; `MISSION_ITEM` → `mission_item`; `MISSION_CTL` → `mission_ctl`; `PARAM_SET` ve `PARAM_GET` → `param`; `FC` → `fc`; `HEARTBEAT`, `ARM`, `MISSION_END`, `PARAM_COMMIT`, `PARAM_SAVE`, `FAILSAFE_RESET`: etkin üye yok |
| `ida_param_value_t.v` | `type` | `IDA_PT_I32` → `i32`; `U32` → `u32`; `F32` → `f32` |
| `ida_cmd_result_t.value` (yapı alanı) | `kind` | yalnız `IDA_RES_PARAM_VALUE`'da geçerli; değilse çekirdek sıfır yazar |
| `ida_cmd_result_t.save` (yapı alanı) | `kind`, `ack` | yalnız `IDA_RES_PARAM_SAVE` ve `IDA_ACK_ACCEPTED`'te geçerli; değilse çekirdek sıfır yazar (`blob` NULL) |
| `ida_in_t.imu` | `imu_present` | 1 → geçerli; 0 → anlamsız |
| `ida_in_t.samples[i]`, `det[i]`, `det_t_rx[i]`, `det_batch[i]`, `det_batch_t_rx[i]`, `cmds[i]`, `evres[i]` | ilgili `n_*` | `i` < `n_*` → geçerli; diğerleri anlamsız |
| `ida_out_t.events[i]`, `results[i]`, `logs[i]`, `det_tx[i]` | ilgili `n_*` | `i` < `n_*` → geçerli; diğerleri çekirdekçe sıfır yazılır |
| `ida_out_t.nav` | `nav_valid` | 1 → geçerli; 0 → çekirdekçe sıfır yazılır |
| `ida_narrow_t.cmd_id[i]`, `coalesced[i]`, `mode` | `mask` | bit `i` kurulu → geçerli; `mode` yalnız `NARROW_MODE` kuruluyken |

### 3.15 Günlük kodları (A4-K90; kapalı tablo)

Tabloda olmayan kod üretilmez; yeni kod sözleşme revizyonu ister. Hiçbir kod ve hiçbir argüman hedef, hedef geometrisi, atış çözümü ya da atış ölçütü değeri taşımaz. Kullanılmayan argüman 0'dır; "doyar" yazan argüman `int32_t` aralığında kırpılır.

| `code` | Ad | Ne zaman | `arg0` | `arg1` |
|---|---|---|---|---|
| 1 | `RESET_INFO` | Init'ten sonraki ilk step/tick | `reset_cause` | `consecutive_faults` |
| 2 | `PARAM_LOAD` | Init'ten sonraki ilk step/tick | init dönüş kodu (`IDA_RC_*`) | A4-K80'in ilk başarısız denetiminin sıra numarası (1–8; geçerliyse 0) |
| 3 | `PARAM_COMMIT` | Her `PARAM_COMMIT` sonucu | 1 uygulandı, 0 reddedildi | ret nedeni (`IDA_REASON_*`) |
| 4 | `PARAM_SAVE` | `plat.param_store` `OK` ya da `FAIL`'e geçtiğinde | `gen` | `param_store` |
| 5 | `MODE_CHANGE` | Kip değiştiğinde | yeni kip (`IDA_MODE_*`) | önceki kip |
| 6 | `ARM_CHANGE` | Silahlandırma değiştiğinde | yeni `armed` | neden (`IDA_REASON_*`; komutla değişimde 0) |
| 7 | `FAILSAFE_CHANGE` | `fs_level` değiştiğinde | yeni `fs_level` | `fs_reasons` (bit deseni) |
| 8 | `SESSION` | `link_event` ≠ `NONE` | `link_event` | 0 |
| 9 | `IMU_GAP` | A4-K38 | boşluğun mutlak değeri, µs (doyar) | 0 |
| 10 | `IMU_LAG` | `IMU_LAG` mandalı kurulduğunda ya da çözüldüğünde (A4-K37) | 1 kuruldu, 0 çözüldü | gecikme, µs (doyar) |
| 11 | `IMU_STALE` | IMU bayatlığı başladığında ya da bittiğinde (A4-K39) | 1 bayat, 0 taze | 0 |
| 12 | `WALL_BACKSTEP` | A4-K10 | geri adım, µs (doyar) | 0 |
| 13 | `BG_SLICE_MISMATCH` | A4-K109 | platformun `bg_slices`'ı | çekirdeğin sayımı |
| 14 | `BG_BAD_INPUT` | A4-K117 bayrağını taşıyan çağrı | önceki step/tick'ten beri `IDA_BG_BAD_INPUT` sayısı (doyar) | 0 |
| 15 | `SAMPLE_CONFLICT` | A4-K115 | örnek türü (`IDA_SENS_*`) | `inst` |
| 16 | `DET_STREAM` | Kamera algılama akışı bayatladığında ya da tazelendiğinde (A4-K118) | 1 bayat, 0 taze | `inst` |
| 17 | `EVENT_RESULT` | Bir olayın son sonucu kabul edildiğinde | olay türü (`IDA_EVT_*`) | sonuç (`IDA_EVR_*`) |
| 18 | `EVENT_UNKNOWN` | A4-K70 | olay türü | tüp (`IDA_TUBE_ALL` dahil) |
| 19 | `SAFETY_INPUT` | Bir etkin emniyet değeri değiştiğinde (A4-K116) | etkin `estop` | etkin tüp bitleri: bit 0 tüp 0 fiş takılı, bit 1 tüp 0 pim çıkık, bit 2 tüp 1 fiş takılı, bit 3 tüp 1 pim çıkık |
| 20 | `FC_STATE` | `fc.state` değiştiğinde | yeni `state` (`IDA_FCS_*`) | önceki `state` |
| 21 | `FC_INHIBIT` | `fc.inhibit_mask` değiştiğinde | yeni maske (bit deseni) | önceki maske |
| 22 | `NUMERIC_FAULT` | Sayısal çökme saptandığında | 1 kestirim, 2 kontrol | 0 |
| 23 | `CORE_INTEGRITY` | `core_ok` 0'a düştüğünde (A4-K89) | 1 koruma sözcüğü, 2 tamamlayıcı kopya, 3 geçersiz durum değeri | 0 |

### 3.16 Atış kontrol durumu ve emniyet engelleri (A4-K91; kapalı tablolar)

`fc.state` (`IDA_FCS_*`):

| Değer | Ad | Anlamı |
|---|---|---|
| 0 | `SAFE` | Emniyetli; açık izin yok |
| 1 | `ARMED` | Atış kontrol silahlı; açık izin yok |
| 2 | `PERMIT_OPEN` | En az bir tüpte izin açık (A4-K107) |
| 3 | `FIRE_PENDING` | Sonucu beklenen `FIRE` var |
| 4 | `TEST` | Sınama kipi (`FC_TEST_ENTER`) |
| 5 | `FAULT_LATCHED` | Mandallı arıza; yalnız `FC_RESET` çıkarır |

`fc.inhibit_mask` (`IDA_FCI_*`): yalnız emniyet zinciri engelleri; bit 18–31 ayrılmış ve 0.

| Bit | Ad | Kurulu olduğu durum |
|---|---|---|
| 0 | `NOT_ARMED` | Araç silahsız |
| 1 | `MODE` | Kip atış izni kümesinde değil |
| 2 | `EST` | Kestirim düzeyi `OK` değil |
| 3 | `FAILSAFE` | Arıza eylemi `HOLD` ya da üstü |
| 4 | `ESTOP` | Etkin acil durdurma basılı (A4-K116) |
| 5, 6 | `PLUG_T0`, `PLUG_T1` | Tüpün etkin fiş değeri çıkık |
| 7, 8 | `PIN_T0`, `PIN_T1` | Tüpün etkin pim değeri takılı |
| 9, 10 | `EMPTY_T0`, `EMPTY_T1` | Tüp boş (son kabul edilen `LOADED` = 0) |
| 11 | `EVENTS_OFF` | Olay kimliği kullanılamaz (A4-K61, K62) |
| 12 | `IMU_LAG` | A4-K37, K108 |
| 13 | `IMU_STALE` | A4-K39 |
| 14 | `SESSION` | A1 oturumu yok |
| 15 | `RESULT_PENDING` | Sonucu beklenen izin açan olay ya da `FIRE` var (A4-K123) |
| 16 | `FAULT_LATCHED` | Atış kontrol mandallı arızada |
| 17 | `DET_STALE` | Kamera algılama akışı bayat (A4-K118) |

---

## 4. Davranış kuralları

Her kural tek bir sınanabilir cümledir. "Çekirdek" ve "platform" Bölüm 1.1'deki taraflardır. Kurallar arasında öncelik: **A4-K113** (değerlendirme sırası) dönüş kodunu belirleyen her kuraldan, **A4-K28** (hata dönüşünde sürekli komut ve genişletici yok) çıktıyı belirleyen her kuraldan, **A4-K45** (kabul edilen çağrıda daraltma çağrıya hâkimdir) genişletmeye izin veren her kuraldan önce gelir.

### 4.1 Genel, sürüm, sayı tipleri

- **A4-K01.** `core/include/ida/port.h`, `ida/algilama_kaydi.h` (A8) bulunduğunda tek başına `-std=c17 -pedantic-errors -Wall -Wextra` ile uyarısız derlenir, bir C++ çeviri biriminden `extern "C"` bağlantısıyla içerilebilir ve işlev gövdesi, değişken tanımı ya da derleyici uzantısı içermez.
- **A4-K02.** `ida_core_init`, `in->port_major` ve `in->port_minor` çekirdeğin derlendiği `IDA_PORT_VERSION_MAJOR` ve `IDA_PORT_VERSION_MINOR`'a eşit değilse `IDA_RC_VERSION` döner ve örneği kurulmamış bırakır.
- **A4-K03.** Hiçbir taraf bir port yapısını bellekteki bayt düzeniyle taşımaya, kayda ya da kalıcı belleğe yazmaz; A1 ve A6 port yapılarını alan alan kodlar (tek istisna, bayt düzeni 3.10'da tanımlı parametre bloğudur ve o bir C yapısı değildir).
- **A4-K04.** Port yapılarında fiziksel büyüklükler `float` (IEEE-754 binary32), zamanlar `int64_t` ns, süreler `uint32_t` ns, coğrafi konumlar `int32_t` 1e-7 derece, GNSS yüksekliği `int32_t` mm'dir ve hiçbir port alanı `double` değildir.
- **A4-K05.** Port alanları iskelet §3.1'in birimlerini kullanır; istisnalar yalnız sıcaklık (°C), Ping360 cihaz birimleri (gradyan, 25 ns, µs, kHz), GNSS tamsayıları, sızıntı, emniyet ve mandal bit maskeleri ve birimsiz `thr_cmd`'dir.
- **A4-K06.** Çekirdek ve platform aynı `port.h` ile ve aynı derleyiciyle derlenip aynı ikiliye bağlanır; port yapılarının bayt düzeni için derleyiciler ya da hedefler arası ikili uyumluluk iddia edilmez ve `#pragma pack` ya da benzeri bir uzantı kullanılmaz.

### 4.2 Zaman

- **A4-K07.** Ölçüm zamanı (`ida_stime_t`) ile duvar saati (`ida_wtime_t`) ayrı yapı tipleridir; çekirdekte ve platformda hiçbir işlem bir ölçüm zamanını bir duvar saatinden çıkarmaz, onunla karşılaştırmaz ya da ona atamaz.
- **A4-K08.** Geçerli bir zaman değeri 0 ile `IDA_TIME_MAX_NS` (2⁶²) arasındadır; aralık dışı `now_w` ya da `tf.t` step ve tick'te `IDA_RC_BAD_INPUT`, init'te `IDA_RC_BAD_INPUT` (3.11 init sırası), `ida_core_background`'da `IDA_BG_BAD_INPUT` (A4-K117) üretir; aralık dışı `t_meas` ya da `t_rx` taşıyan örnek reddedilir.
- **A4-K09.** Platformun verdiği duvar saati bir çekirdek örneğinin ömrü boyunca (init, step, tick ve background çağrılarında) azalmaz, sarmaz ve 1 µs ya da daha ince çözünürlüklüdür.
- **A4-K10.** Çekirdek bir çağrıda önceki çağrınınkinden küçük `now_w` görürse o çağrıda önceki değeri kullanır ve `status.cnt.wall_backstep`'i bir artırır.
- **A4-K11.** Kartta duvar saati açılıştan beri geçen süredir; SIL'de her parti çağrısının `now_w`'si o çağrının partisini kapatan zaman çerçevesinin sim zamanına eşittir (`now_w.ns = tf.t.ns`); SIL'de ızgara tick'i yapılmaz, bunun tek istisnası oturum sonu çağrısıdır (A4-K122; A1-K107) ve onun `now_w`'si son kabul edilen zaman çerçevesinin sim zamanıdır; init'in `now_w`'si ilk zaman çerçevesinin sim zamanını aşmaz ve sim zamanı geri başlarsa SIL koşucusu çekirdeği yeni `boot_count` ile yeniden kurar.
- **A4-K12.** Çekirdek hiçbir saati, sayacı ya da rastgele sayı kaynağını okumaz; zamanı yalnız `now_w`, `tf` ve örnek damgalarından alır.
- **A4-K13.** Örneğin `t_meas`'ı iskelet §3.1'deki ölçüm zamanıdır: anlık ölçümde örnekleme anı; aralık ölçümünde (IMU, basınç, batarya) aralığın sonu, aralığın süresi `window_ns`'tedir; tek menzilli akustik ölçümde (Ping1D, DVL) yansıma anı; menzil profilinde (Ping360) darbenin gönderim anı; platform ve dönüştürücü bu değeri değiştirmeden aktarır.
- **A4-K14.** Örneğin, zaman çerçevesinin, algılama kaydının ve parti başlığının alım zamanı platformun damgasıdır: kartta A1-K48'in damgalama kuralıyla konan duvar saati (çerçevenin son baytından sonraki ilk alım olayının damgası; bağlanamazsa önceki en son olayınki), SIL'de çağrının `now_w`'si; damganın gerçek alım anından en büyük sapması A1'in tanımıdır (Bölüm 10, A1'den beklenen); bu sözleşmede alım zamanından sayılan bütün süreler (A4-K36, A4-K39, A4-K118) bu damgadan sayılır; her iki platformda alım zamanı ≤ çağrının `now_w`'si, aksi halde çekirdek örneği reddeder ve `status.cnt.sample_future`'ı artırır.
- **A4-K15.** `tf.present = 1` olan iki çağrı arasında (aynı oturumda, `SESSION_START` olmadan) `tf.seq` seri aritmetikle arttıysa `tf.t` kesin artmış olmalıdır; olmamışsa çekirdek `IDA_RC_BAD_INPUT` döner.

### 4.3 Çağrı modeli

- **A4-K16.** Platform kapanan her partiyi, `tf.seq` sırasıyla ve hiçbirini atlamadan, ya tek başına bir çekirdek çağrısıyla ya da A4-K102'ye göre birleştirilmiş tek bir çağrıyla verir: IMU örneği içeren girdi `ida_core_step`'e, IMU'suz girdi `ida_core_tick`'e gider ve partinin bütün örnekleri, komutları, algılama kayıtları ve Ping360 satırı (kapasite kuralları saklı) aynı çağrıya konur.
- **A4-K17.** `ida_core_step` `imu_present = 1`, `ida_core_tick` `imu_present = 0` ister; aksi halde çekirdek `IDA_RC_BAD_INPUT` döner.
- **A4-K18.** Kartta platform, oturum yokken ya da zaman çerçevesi akmıyorken çekirdeği 10 ms'lik mutlak ızgarada (`tf.present = 0`, örneksiz ve komutsuz) tick ile; oturum `RUNNING`'de ve zaman çerçevesi akarken A1-K117'nin takvimiyle (beklenen zaman çerçevesinin çevresinde tick başlatılmaz, zaman çerçevesi `T_tf_gec` gecikince 10 ms aralıkla tick yapılır) çağırır; bu takvimlerle ardışık iki çekirdek çağrısının başlangıçları arasındaki duvar saati aralığı, zaman çerçevesi akarken en çok `P + T_tf_gec + IDA_PORT_START_MAX_NS` = 5 + 1 + 4 = 10 ms (hesap; A1-K117 değerleriyle), akmıyorken en çok `IDA_PORT_TICK_MAX_GAP_NS` = 11 ms'dir (10 ms ızgara 01 §2.5'in tasarımı; 1 ms dağıtım payı mimari seçimi, tahmin).
- **A4-K19.** Kartta platform kapanan bir partiyi, kapandığında çekirdek çağrısı sürmüyorsa partiyi kapatan zaman çerçevesinin `tf.t_rx`'inden en geç `IDA_PORT_DISPATCH_MAX_NS` = 0,5 ms sonra, sürüyorsa o çağrının dönüşünden en geç 0,5 ms sonra başlatır; böylece her çağrıda `now_w − tf.t_rx` ≤ `IDA_PORT_START_MAX_NS` = 4 ms'dir (3,5 ms kesilemeyen step bütçesi + 0,5 ms dağıtım; 0,5 ms en uzun arka plan dilimini (0,3 ms) ve ayrıştırmayı kapsar; mimari seçimi, tahmin). Bu güvence A1'den beklenen geliş varsayımlarına dayanır (Bölüm 10).
- **A4-K20.** Bir çekirdek örneği üzerindeki hiçbir işlev (`init`, `step`, `tick`, `background`, `param_get`) aynı örnek üzerindeki başka bir işlev sürerken çağrılmaz; farklı örnekler birbirinden bağımsızdır ve aynı süreçte aynı anda kullanılabilir.
- **A4-K21.** Çekirdek işlevleri kesme bağlamından çağrılmaz; çekirdek kesme maskelemez, beklemez, bloklamaz ve hiçbir platform işlevini çağırmaz.
- **A4-K22.** Çekirdek değişebilir küresel ya da statik durum tutmaz; bütün durumu `ida_core_t` içindedir (çekirdek nesne dosyalarında yazılabilir veri bölümüne düşen sembol yoktur).
- **A4-K23.** Kartta (`fw-release` derlemesi, 160 MHz) çekirdeğin en kötü durum süresi `ida_core_step` için `IDA_PORT_STEP_BUDGET_NS` = 3,5 ms, `ida_core_tick` için `IDA_PORT_TICK_BUDGET_NS` = 0,5 ms, tek `ida_core_background` çağrısı için `IDA_PORT_BG_BUDGET_NS` = 0,3 ms'dir; bu bütçelere uymak çekirdek uygulamasının sorumluluğudur, platform her çağrının süresini ölçer ve sonraki çağrıda `plat.prev_call_ns` ve `plat.prev_overrun` ile bildirir (değerler mimari seçimi, tahmin; dayanak Bölüm 4.13).
- **A4-K24.** Aynı ikilide, aynı init girdisi ve (`bg_slices` dahil) aynı çağrı girdileri dizisi verilen ve aynı dizide aynı sayıda arka plan çağrısı yapılan iki çekirdek örneği bit düzeyinde aynı çıktı dizisini üretir (A4-K109, A4-K110).
- **A4-K25.** `ida_core_step` ve `ida_core_tick` 3.11'deki değerlendirme sırasıyla (A4-K113) ve 4.15'teki listeden tek bir kod döner; `tf.present = 0` olan çağrı `DUP_SEQ` ya da `OLD_SEQ` dönmez; ilk zaman çerçevesi (init ya da `SESSION_START`'tan sonra) her sıra numarasıyla kabul edilir; sonraki `tf.seq`, iskelet §3.1'in seri aritmetiğiyle son kabul edilenden yeniyse (fark 1 ile 2³¹ − 1 arasında; atlanan ya da birleştirilmiş partiler dahil) kabul edilir, fark 0 ise `DUP_SEQ`, fark 2³¹ ya da daha büyükse `OLD_SEQ` döner.
- **A4-K26.** Hata dönüşü veren çağrıda çekirdek kestirimi, güdümü, kontrolü ve atış kontrolün adım kısmını çalıştırmaz, o çağrının hiçbir örneğini, algılama kaydını ya da olay sonucunu kabul etmez, tazeliği yenilemez ve hiçbir komutu işlemez; daraltma yalnız A4-K113'ün (b) adımına göre uygulanır ve komut sonuçları A4-K114'e göre yazılır.
- **A4-K113.** Çekirdek her step ve tick çağrısını 3.11'deki sırayla değerlendirir: (a) `c`/`in` işaretçisi ve kurulmamış örnek denetimleri, (b) daraltmanın uygulanması ve ardından `out` NULL denetimi (b2), (c) yapısal doğrulama (`BAD_INPUT`), (d) sıra denetimi (`DUP_SEQ`, `OLD_SEQ`), (e) olağan işleniş; ilk tutan adımın kodu döner, (c) ve (d) birlikte tutarsa `BAD_INPUT` döner; (b) adımı `in` okunabilen ve örnek kurulu olan her çağrıda yapılır, tanımlı mandal bitlerini, `SESSION_END`'i ve A4-K116'daki emniyet daraltmasını uygular, çağrı sonradan reddedilse de geri alınmaz ve aynı daraltmanın yinelenen uygulanması ek etki üretmez.
- **A4-K114.** Reddedilen çağrıda (`BAD_INPUT`, `DUP_SEQ`, `OLD_SEQ`) çekirdek `cmds[]`'teki hiçbir komutu işlemez ve yinelenen kimlik penceresine (A4-K48) almaz; `n_cmds` ≤ 8 ise her komut için `IDA_ACK_REJECTED` / `IDA_REASON_CALL_REJECTED`, `n_cmds` > 8 ise `cmds[]` için hiç sonuç yazar; (b) adımında uygulanan her mandal biti için `IDA_ACK_ACCEPTED` yazar; `IDA_REASON_NARROWED` yalnız kabul edilen çağrıda verilir. `NOT_INIT`'te mandal bitleri ve `n_cmds` ≤ 8 ise komutlar `IDA_ACK_REJECTED` / `IDA_REASON_NOT_INIT` alır.
- **A4-K27.** Çekirdek, dönüş kodu ne olursa olsun (4.15'te "yazılmaz" denen NULL durumları hariç), `out`'un her alanını o çağrıda yazar; önceki çağrıdan kalmış bir değer çıktıda bulunmaz.
- **A4-K28.** Hata dönüşü veren her çağrının çıktısında `thr_valid = 0`, `valid_for_ns = 0`, sekiz `thr_cmd` = `0.0f`'tır ve genişletici olay (`SAFETY_OPEN`, `FIRE`) yoktur; bu kural bu sözleşmenin başka her kuralından önce gelir ve çıktının geri kalanı 4.14'teki tabloya göre yazılır.
- **A4-K29.** Bir çıktının A1 üzerinden gönderilip gönderilmeyeceği (`OLD_SEQ`, ızgara tick'i, hata dönüşleri) A1 sözleşmesinin kuralıdır; çekirdek her durumda A4-K27'ye göre tanımlı bir çıktı verir.
- **A4-K102.** Platform en çok bir bekleyen parti tutar: bir çekirdek çağrısı sürerken parti kapanırsa onu bekletir ve A4-K19'a göre başlatır; bekleyen parti varken bir parti daha kapanırsa ikisini tek girdide birleştirir: zaman çerçevesi en yeni partininkidir, IMU en yeni partininkidir (eskisi düşer), örnekler, algılama kayıtları, algılama partisi başlıkları ve komutlar varış sırasıyla kapasite sınırına kadar konur, Ping360 satırlarından en yenisi konur; sığmayan her üye düşer ve `plat.cnt_member_overflow`'u (daraltıcı olmayan komut için A4-K105'i) artırır, olay sonuçları düşmez (A4-K32), daraltıcılar mandaldadır (A4-K46); birleştirilmiş girdide `plat.batch_merged = 1` ve birleştirilen her eski parti için `plat.cnt_batch_merged` bir artar, üye düştüyse `plat.input_dropped = 1`'dir ve çekirdek bunu `health.input_dropped` ile bildirir.

### 4.4 Sensör örnekleri ve teslim

- **A4-K30.** Her örnek `ida_hdr_t` başlığını taşır; `ida_sample_t`'nin birlik alanı `hdr.type`'a göre okunur; `hdr.type` bilinmeyen ya da kabının türüyle uyuşmayan (IMU alanında `IDA_SENS_IMU` dışı, `samples[]`'ta IMU ya da Ping360) ya da `inst` ≥ 4 olan örnek reddedilir.
- **A4-K31.** Bir çağrıda `n_samples` ≤ 16, `n_cmds` ≤ 8, `n_det` ≤ 8, `n_det_batch` ≤ 2, `n_evres` ≤ 4 ve en çok bir IMU ve bir Ping360 satırı bulunur; sayı alanlarından biri sınırı aşarsa çekirdek `IDA_RC_BAD_INPUT` döner (daraltma A4-K113'ün (b) adımına göre yine uygulanır).
- **A4-K32.** Platform bir girdide kapasiteyi aşan örnekleri sonraki çağrıya taşır ya da atar ve her ikisinde `plat.cnt_member_overflow`'u artırır; kapasiteyi aşan olay sonuçlarını hiçbir durumda atmaz, sonraki çağrılara taşır.
- **A4-K33.** Çekirdek bir örneği ancak şunların hepsi tutarsa kabul eder: `flags`'te `IDA_HF_VALID`; bütün `float` alanlar sonlu; `t_meas` ve `t_rx` geçerli aralıkta; `t_rx` ≤ `now_w`; `tf.present` ise `t_meas` ≤ `tf.t`; `seq` aynı (`type`, `inst`) için son kabul edilenden seri aritmetikle büyük; `t_meas` aynı (`type`, `inst`) için son kabul edilenden kesin büyük (`IDA_SENS_SAFETY` için A4-K34); aynı çağrıda eşit anahtarlı başka bir örnekle çelişmiyor (A4-K115); reddedilen örnek kullanılmaz (yalnız emniyet örneğinin daraltıcı içeriği A4-K116'ya göre uygulanır), `status.cnt.sample_rejected` (ya da gelecek damgada `sample_future`) artar ve tazelik yenilenmez. Init'ten ya da `SESSION_START`'tan sonraki ilk örnek `seq` ve `t_meas` koşulundan muaftır.
- **A4-K34.** `IDA_SENS_SAFETY` örneğinde `t_meas` son kabul edilene eşitse ve `seq` büyükse örnek kabul edilir ve değeri günceller, ama o sensörün ölçüm zamanı tazeliğini yenilemez.
- **A4-K115.** Aynı çağrıda aynı (`t_meas`, `type`, `inst`, `seq`) anahtarını taşıyan iki ya da daha çok örnek bayt bayt aynıysa çekirdek birini A4-K33'e göre değerlendirir, diğerlerini düşürür ve her biri için `status.cnt.sample_dup_dropped`'u artırır; içerikleri farklıysa girdideki sıralarından bağımsız olarak hepsini reddeder, her çelişkili anahtar için `status.cnt.sample_conflict`'i bir artırır ve `health.sample_conflict = 1` verir.
- **A4-K116.** Bir `IDA_SENS_SAFETY` örneğinin kısıtlayıcı içeriği kısıtlayan yönde taşıdığı her değerdir: `estop = 1`, bir tüpte `PLUG_IN = 0` (fiş çıkık), bir tüpte `PIN_OUT = 0` (pim takılı); önceki kabul edilen değere göre geçiş aranmaz (iskelet §3.2). Çekirdek her emniyet alanı için bir etkin değer tutar (init'te ve `SESSION_START`'ta kısıtlayan). Bir çağrıda yapısal olarak geçerli (`type` ve `inst` doğru, alanlar okunabilir) herhangi bir emniyet örneği — kabul edilen, A4-K33 ya da A4-K115 ile reddedilen ya da reddedilen bir çağrıdaki (A4-K113 (b)) — bir alanı kısıtlayan değerle taşıyorsa çağrı sonunda o alanın etkin değeri kısıtlayandır; gevşeten değer ancak bir çağrıda kabul edilen bir örnek onu taşıyor ve o çağrıdaki hiçbir emniyet örneği o alanı kısıtlayan değerle taşımıyorsa etkin olur. Reddedilen örnek son kabul edilen değeri ve tazeliği değiştirmez. Etkin değer kısıtlayana döndüğünde `SAFETY_CLOSE` (A4-K111) ve `IDA_LOG_SAFETY_INPUT` bir kez üretilir; dönüşe yalnız reddedilen bir örneğin içeriği yol açtıysa `status.cnt.narrow_from_rejected` bir artar; etkin değerin değişmediği çağrıda olay ve sayaç üretilmez. Etkin değeri kısıtlayan bir alan varken çekirdek o alanın kapsadığı tüp(ler) (acil durdurmada iki tüp) için izin açan olay ve `FIRE` üretmez ve ilgili `IDA_FCI_*` bitini kurar. Çelişkili bir çiftte (A4-K115) alan o çağrı için kısıtlayandır; önceki çağrıda kabul edilmiş anahtarın farklı içerikle yeniden gelmesi de bu kurala tabidir. `LOADED` ve `PERMIT_OPEN` emniyet girdisi değildir; son kabul edilen örnekten alınır.
- **A4-K35.** Çekirdeğin çıktısı, aynı çağrıdaki `samples[]` dizisinin sırasından bağımsızdır: çekirdek örnekleri kendi sabit sırasıyla (`t_meas`, tür, `inst`, `seq` anahtarıyla) işler ve eşit anahtarlı örnekleri A4-K115'e göre sıradan bağımsız ele alır.
- **A4-K36.** Örnek tazeliği yalnız çekirdekte, iki ölçütle (son kabul edilen örneğin `t_rx`'inden bu yana geçen duvar süresi ve ölçüm zamanının ilerlemesi; iskelet §4) değerlendirilir; platform tazelik kararı vermez ve bayat örneği süzmez.
- **A4-K37.** `tf.present = 1` olan her çağrıda çekirdek gecikmeyi `lag = tf.t − t_meas(son kabul edilen IMU)` olarak hesaplar; `lag` > `T_lag_set` olduğunda `IMU_LAG` mandalını kurar; mandal, `lag` ≤ `T_lag_clear` olan art arda `N_lag_clear` zaman çerçeveli çağrıdan sonra çözülür; mandal kuruluyken `status.fs_reasons`'ta `IDA_FSR_IMU_LAG` ve `health.imu_lag = 1`'dir (`T_lag_set` = 20 ms, `T_lag_clear` = 10 ms, `N_lag_clear` = 20; hepsi mimari seçimi, tahmin; çekirdek parametresi).
- **A4-K108.** `IMU_LAG` mandalı kuruluyken çekirdek genişletici olay (`SAFETY_OPEN`, `FIRE`) üretmez, genişletici komutları (A4-K104) `IDA_REASON_IMU_LAG` ile reddeder ve `status.cnt.widen_blocked`'u artırır; kestirim, güdüm ve kontrol sürer ve mandal kendi başına arıza eylemi düzeyini değiştirmez.
- **A4-K38.** Çekirdek kabul ettiği her IMU örneği için boşluğu `(t_meas − window_ns) − t_meas(önceki kabul edilen IMU)` olarak hesaplar ve mutlak değeri `T_imu_gap_tol`'u (parametre; varsayılan `window_ns`'in onda biri, tahmin) aşarsa `status.cnt.imu_gap`'i artırır ve bir günlük kaydı üretir; boşluktaki filtre davranışı çekirdek uygulamasınındır.
- **A4-K103.** `ida_core_step`'in IMU örneği A4-K30 ya da A4-K33'ün bir koşulunu tutmazsa çekirdek çağrıyı `ida_core_tick` kurallarıyla sürdürür: o örnekle yayılım yapmaz, diğer örnekleri, kayıtları, komutları ve olay sonuçlarını tick'teki gibi kabul eder, seyrüsefer geçerliliğini bayatlık kuralıyla (A4-K39) belirler, genişletici olay üretmez, genişletici komutları `IDA_REASON_IMU_REJECTED` ile reddeder, `health.imu_rejected = 1` verir, `status.cnt.imu_rejected`'i artırır ve `IDA_RC_IMU_REJECTED` döner (çıktı 4.14'te).
- **A4-K39.** Son kabul edilen IMU örneğinin alım damgasından (A4-K14) `T_imu_stale` (parametre; 5–30 ms, varsayılan 20 ms; W1-uyum U7) geçtikten sonra **başlayan** ilk hata dönüşsüz step ya da tick çağrısı nötr güvenli çıktı üretir; kartta bu çıktı aynı damgadan en geç `T_imu_stale + IDA_PORT_STALE_MARGIN_NS` (11 ms başlangıç aralığı + 3,5 ms çağrı bütçesi = 14,5 ms; en kötü 30 + 14,5 = 44,5 ms, varsayılanla 34,5 ms) içinde **kullanılabilir** olur (çağrı döner); SIL'de sanal zamanda `T_imu_stale + 5 ms` içinde; çekirdek 5–30 ms dışındaki `T_imu_stale`'i parametre doğrulamasında (A4-K80) reddeder.
- **A4-K40.** `ping360` işaretçisi yalnız çağrı süresince geçerlidir ve çekirdek kullanacağı içeriği çağrı içinde kopyalar; `n_samples` 1–1200, `angle_grad` 0–399 ve `sample_period` ≥ 1 değilse ya da A4-K33'ün başlık koşulları tutmuyorsa satır reddedilir, `status.cnt.ping360_rejected` artar ve çağrı sürer; menzil kutusu zamanının formülü A1 sözleşmesinden alınır.
- **A4-K41.** Algılama partisi atomiktir: çekirdek her parti başlığını (`det_batch[i]`) A8'in başlık–kayıt eşlemesiyle ona ait kayıtlarla birlikte, A8-K29'un "Kart" satırlarıyla A8-K27'nin sırasında ve alım zamanlarının (`det_batch_t_rx[i]`, `det_t_rx[j]`) ≤ `now_w` koşuluyla denetler; başlık ya da ona ait herhangi bir kayıt bir denetimi geçmezse partinin başlığı ve kayıtlarının hiçbiri kullanılmaz (kısmi kabul yoktur) ve `status.cnt.det_rejected` bir artar; hiçbir başlığa ait olmayan kayıt kullanılmaz ve aynı sayaca sayılır; sensör çerçevesinden dünya çerçevesine dönüşümü yalnız çekirdek yapar.
- **A4-K118.** Çekirdek A8'in kamera algılama akışını (`source` = `IDA_DET_SRC_CAM_FWD`, `inst` = 0; tek kamera, iskelet §5) bekler; o akıştan kabul edilen son parti başlığının (A8-K64'ün durum başlıkları dahil) alım zamanından (A4-K14) `T_det_stale` (parametre; varsayılan 2 s; tasarım seçimi, tahmin) geçmişse ya da init, oturum sonu çağrısı (A4-K122) veya `SESSION_START`'tan beri `T_det_stale` boyunca hiç başlık kabul edilmemişse akışı bayat sayar: o akışın kayıtlarını kullanmaz, `fc.inhibit_mask`'te `IDA_FCI_DET_STALE`'i kurar ve geçişte `IDA_LOG_DET_STREAM` üretir; kabul edilen yeni bir başlık akışı tazeler (o çağrının kayıtları kullanılabilir).
- **A4-K42.** Bütün örnek değerleri sensör çerçevesindedir; sensör montajlarının konumu ve yönelimi, gövde referans noktasına (kütle merkezi değil; torpido ayrılınca yer değiştirmeyen sabit geometrik nokta; iskelet §3.1) göre verilmiş çekirdek parametreleridir; IMU'nun `accel`'i montaj noktasındaki özgül kuvvettir ve dönme kolu etkilerini çıkarmak çekirdeğin işidir.

### 4.5 Komutlar, daraltma ve genişletme

- **A4-K43.** Çekirdek kabul edilen her çağrıda (reddedilen çağrı ve `NOT_INIT` için A4-K114), `narrow.mask`'te kurulu her tanımlı bit için (dizin sırasıyla) ve `cmds[]`'teki her komut için (dizi sırasıyla) `results[]` dizisine aynı `cmd_id`'li tam bir sonuç yazar (`n_results` = kurulu bit sayısı + `min(n_cmds, 8)` ≤ 12); `PARAM_GET` ve `PARAM_SET` sonucu `IDA_RES_PARAM_VALUE`, `PARAM_SAVE` sonucu `IDA_RES_PARAM_SAVE`, diğerleri `IDA_RES_ACK` türündedir; tek değerli ayrı bir parametre değeri ya da kayıt yuvası yoktur.
- **A4-K44.** Daraltıcı girdiler şunlardır: `IDA_CMD_DISARM`; hedefi `IDA_MODE_GUIDED` ya da `IDA_MODE_AUTO` olmayan `IDA_CMD_SET_MODE`; `fc.op` = `IDA_FC_DISARM` ya da `IDA_FC_ABORT` olan `IDA_CMD_FC`; `plat.link_event = SESSION_END` (A4-K73); emniyet girdisinin kapanması, yani bir emniyet alanının etkin değerinin bu çağrıda kısıtlayana dönmesi (A4-K111, A4-K116); bunların dışında daraltıcı girdi yoktur. Etkin değeri zaten kısıtlayan olan bir alanın yinelenen kısıtlayıcı içeriği çağrı genelinde daraltıcı girdi sayılmaz; o alanın gevşetilmesine (A4-K116) ve o alanın kapsadığı tüplerin izin açan olaylarına üstün gelir.
- **A4-K104.** Genişleticiler şunlardır: `IDA_CMD_ARM`; hedefi `IDA_MODE_GUIDED` ya da `IDA_MODE_AUTO` olan `IDA_CMD_SET_MODE`; `IDA_CMD_FAILSAFE_RESET`; `fc.op` ∈ {`FC_ARM`, `FC_FIRE`, `FC_RESET`, `FC_TEST_ENTER`} olan `IDA_CMD_FC`; `SAFETY_OPEN` ve `FIRE` olayları; geri kalan komutlar nötrdür.
- **A4-K45.** Kabul edilen bir çağrının (`OK`, `IMU_REJECTED`) girdisinde daraltıcı bir girdi varsa (A4-K113'ün (b) adımında uygulanan: mandal biti, `SESSION_END` ya da A4-K111) çağrının son durumu daraltılmış durumdur; çekirdek aynı çağrıdaki bütün genişletici komutları girdideki sıralarından bağımsız olarak `IDA_REASON_NARROWED` ile reddeder, aynı çağrının çıktısında genişletici olay üretmez, `health.narrowed = 1` verir ve engellenen her genişletici için `status.cnt.widen_blocked`'u artırır; bu ret nedeni A4-K103 ve A4-K108'in ret nedenlerinden önce gelir; reddedilen çağrıda bu kural uygulanmaz (A4-K114).
- **A4-K46.** Platform her daraltıcı tür için (`IDA_NARROW_DISARM`, `_MODE`, `_FC_DISARM`, `_FC_ABORT`) tek bitlik bir mandal tutar, daraltıcı komutu `cmds[]`'e koymaz ve hiçbir koşulda atmaz: komut geldiğinde ilgili biti kurar, `cmd_id[i]`'yi en son komutun kimliği, `NARROW_MODE`'da `mode`'u en son hedef kip yapar, önceki aynı türden komutu `coalesced[i]`'ye ve `plat.cnt_narrow_coalesced`'a sayar, mandalları bir sonraki çekirdek çağrısının `narrow` alanında verir ve o çağrıdan sonra temizler; böylece bir daraltıcının çekirdeğe ulaşması en çok bir sonraki çağrı kadar gecikir. `FC_DISARM` ve `FC_ABORT` mandalları bütün tüplere uygulanır (mandal tüp taşımaz; A1 bu iki işlemde `tube` = 255 ister); mandal `source` taşımaz, `source` yalnız kayıt bilgisidir ve kaybı davranışı değiştirmez. `cmds[]`'te daraltıcı bir komut bulunursa (platform hatası) çekirdek onu ilgili mandal biti gibi uygular. `NARROW_MODE`'un geçerli hedefleri `MANUAL`, `STABILIZE`, `DEPTH_HOLD` ve `STATION_KEEP`'tir; çekirdek `narrow.mask`'te `IDA_NARROW_DEFINED` dışında bit ya da geçersiz hedefli `NARROW_MODE` görürse çağrıyı `BAD_INPUT` ile reddeder (A4-K113 (c)), tanımlı ve geçerli bitleri yine uygular ve geçersiz hedefli `NARROW_MODE` için kipi değiştirmeden yalnız atış iznini kip koşulundan düşürür.
- **A4-K105.** Platform bir çağrıya 8'den fazla daraltıcı olmayan komut düşerse fazlasını sonraki çağrıya erteler ve `plat.cnt_cmd_deferred`'i artırır; erteleme tamponu dolarsa gelen daraltıcı olmayan komutu atar ve `plat.cnt_cmd_dropped`'u artırır; platform bir daraltıcı komutu mandala aldığında erteleme tamponunda bekleyen bütün genişletici komutları atar ve her biri için ret sonucu üretir (iskelet §3.2 "yolda bekleyen gevşetme"; W1-uyum U4b; sahibi A1, Bölüm 10).
- **A4-K47.** Çekirdek bir çağrıdaki bütün mandal bitlerini ve sekiz komutun hepsini o çağrıda işler ve atış kontrol modülüne giden komutlar için iç kapasitesi bir çağrıda 12 komuttan az değildir.
- **A4-K48.** Son 32 komut içinde görülmüş bir `cmd_id` ile gelen komut yeni etki üretmez ve `IDA_ACK_DUPLICATE` ile yanıtlanır; pencereye yalnız kabul edilen çağrıda işlenen komutlar girer (A4-K114); mandal bitleri bu denetimden muaftır ve A4-K113'e göre her zaman uygulanır.
- **A4-K49.** `IDA_CMD_MANUAL`'ın `axes[6]` alanı gövde çerçevesinde (X, Y, Z, K, M, N sırası; iskelet §3.1) [−1, 1] aralığında normalleştirilmiş istektir: `MANUAL` kipinde `axes[j] × tau_max_manual[j]` genelleştirilmiş kuvvet isteğidir; `STABILIZE` kipinde ilk üç eksen aynı anlamdadır, son üçü `axes[j] × omega_max_manual[j]` açısal hız isteğidir; `DEPTH_HOLD` kipinde X, Y ve N `STABILIZE`'deki anlamdadır, Z `axes[2] × w_max_manual` derinlik değişim hızı isteğidir, K ve M yok sayılır; diğer kiplerde komut `IDA_REASON_BAD_MODE` ile reddedilir; sonlu olmayan ya da aralık dışı eksen `IDA_REASON_OUT_OF_RANGE` ile reddedilir (ölçek değerleri çekirdek parametresidir).
- **A4-K50.** `IDA_CMD_GUIDED_SP` yalnız `GUIDED` kipinde kabul edilir; alanların birimi ve çerçevesi 3.4'teki tablodadır, yalnız `mask`'teki alanlar kullanılır, `mask` = 0 ya da maskeli bir alan sonlu değilse komut `IDA_REASON_OUT_OF_RANGE` ile reddedilir.
- **A4-K51.** Görev yüklemesi atomiktir: çekirdek `MISSION_BEGIN(count)`, `count` adet sıra indisli `MISSION_ITEM` ve `MISSION_END`'i birlikte kabul eder ya da hepsini reddeder; görev ancak `IDA_MS_IDLE`, `LOADED`, `PAUSED`, `COMPLETE` ya da `ABORTED` durumunda ya da `HOLD` arıza eylemi sürerken değiştirilir; 128'den fazla öğe reddedilir. Çekirdek her `MISSION_ITEM`'ı A1 Tablo 3.6a'nın koşullarıyla denetler (kapalı tür listesi 1–7; türün izin verdiği `frame`; kullanılmayan konum alanlarının 0 olması; `depth` ve `p[i]` aralıkları; "0" yazan `p[i]`'nin tam 0 olması; sonluluk); tutmayan öğeyi `IDA_REASON_OUT_OF_RANGE` ile reddeder ve o yüklemeyi atomikliğe göre bütünüyle reddeder. Alanların anlamı, birimi ve aralığı yalnız A1 Tablo 3.6a'dadır.
- **A4-K52.** Çekirdek yer istasyonu bağlantısının canlılığını yalnız kabul ettiği `IDA_CMD_HEARTBEAT` komutlarının `now_w`'sinden izler; bağlantı kaybının süreleri duvar saatindedir ve değerleri çekirdek parametresidir.

### 4.6 Sürekli komut çıkışı

- **A4-K53.** Her step ve tick çıktısı `thr_valid`, `is_safe`, `armed`, `src`, `fs_level`, `seq`, `t_ref`, `valid_for_ns` ve `thr_cmd[8]` alanlarını taşır.
- **A4-K54.** `thr_cmd[i]`, A5'teki itici sırasında `i` numaralı iticinin normalleştirilmiş ESC komutudur ([−1, 1], 0 = dur); platform bunu A5'ten okunan üç parametreyle (merkez, kazanç, alt ve üst sınır) doğrusal olarak ESC darbe genişliğine çevirir ve başka bir dönüşüm yapmaz; dur ölü bandı ve itki eğrisinin tersi çekirdektedir.
- **A4-K55.** `is_safe = 1` ancak ve ancak çağrı sonunda araç silahsızsa (`armed = 0`), `fs_level` ≥ `IDA_FS_PASSIVE_SURFACE` ise ya da IMU bayatlığı veya sayısal çökme nedeniyle itki nötrse verilir; `thr_valid = 1` ve `is_safe = 1` olan her çıktıda sekiz `thr_cmd` tam olarak `0.0f`'tır.
- **A4-K56.** `IDA_RC_OK` dönen tick ve `IDA_RC_IMU_REJECTED` dönen step, nötr güvenli çıktı gerekiyorsa (A4-K39, A4-K55) nötr güvenli çıktıyı, gerekmiyorsa `thr_valid = 0` taşır (yürürlükteki sürekli komut son `OK` step'inkidir); `thr_valid = 0` olan çıktıda `thr_cmd` değerleri `0.0f`'tır ve platform onları kullanmaz.
- **A4-K57.** `out.seq` ve `out.t_ref`, çağrı `IDA_RC_OK` ya da `IDA_RC_IMU_REJECTED` döndüyse ve `tf.present` ise `tf.seq` ve `tf.t`'dir; diğer her durumda (ızgara tick'i ve hata dönüşleri) son kabul edilen zaman çerçevesinin değerleridir (hiç yoksa 0); girdideki geçersiz bir zaman hiçbir durumda `out.t_ref`'e yazılmaz; `out.src` çıktıyı üreten işlevi gösterir.
- **A4-K58.** `thr_valid = 1` olan çıktıda `valid_for_ns` 60–450 ms aralığındadır (`IDA_VALID_FOR_MIN_NS`–`IDA_VALID_FOR_MAX_NS`; sahibi A1-K104; W1-uyum U7), `thr_valid = 0` olanda 0'dır; değeri çekirdek parametresidir (varsayılan 100 ms) ve çekirdek aralık dışı değeri parametre doğrulamasında (A4-K80) reddeder.
- **A4-K59.** Platform `thr_valid = 1` olan bir çıktıda sonlu olmayan ya da [−1, 1] dışında bir `thr_cmd` görürse çıktıyı nötr güvenli çıktıyla değiştirir, o çıktının olaylarını ilk kabul kapısından geçirmez (her birine `DROPPED_GATE` döner) ve `plat.cnt_out_fixed`'i artırır.

### 4.7 Tek seferlik olaylar

- **A4-K60.** Olay türleri `SAFETY_OPEN` (fırlatıcıya süreli izin verir; genişletici), `SAFETY_CLOSE` (izni kaldırır, süresizdir) ve `FIRE`'dır (atış; genişletici); her olay 3.8'deki `ida_event_t` alanlarını taşır ve atış kontrolün iç durumunu taşıyan bir alanı yoktur.
- **A4-K61.** `event_id = ((uint64_t)boot_count << 32) | ctr`'dir; `ctr` her init'ten sonra 1'den başlar ve her yeni olayda bir artar; init'te `boot_count_ok = 0` ya da `boot_count` 1–0xFFFFFFFE dışındaysa çekirdek o örnekte hiç olay üretmez, `status.events_ok = 0` verir ve `IDA_CMD_ARM`'ı `IDA_REASON_EVENTS_OFF` ile reddeder.
- **A4-K62.** `ctr` 0xFFFFFFF0'a ulaştığında çekirdek yalnız `SAFETY_CLOSE` üretir, `status.events_ok = 0` verir ve arm'ı reddeder; 0xFFFFFFFF kullanıldıktan sonra o örnekte olay üretmez; `ctr` hiçbir koşulda sarmaz (açık kalmış izinler A4-K66'ya göre kendiliğinden düşer).
- **A4-K63.** Çekirdek her olayı tam bir kez, kararı verdiği çağrının `events[]` dizisinde çıkarır ve aynı `event_id`'yi bir daha çıkarmaz.
- **A4-K64.** `SAFETY_OPEN` ve `FIRE` yalnız `IDA_RC_OK` dönen `ida_core_step` çıktısında bulunabilir ve A4-K45, A4-K103 ve A4-K108'in yasakları bunlara da uygulanır; `FIRE` ayrıca yalnız aynı çıktıda `armed = 1` ve `is_safe = 0` iken bulunur; tick, `IMU_REJECTED` ve hata dönüşlerinin çıktısı yalnız `SAFETY_CLOSE` taşıyabilir.
- **A4-K65.** `events[]` dizisinde bütün `SAFETY_CLOSE` olayları diğer olaylardan önce gelir.
- **A4-K66.** Bir olay, `SAFETY_CLOSE` dahil, ölçüm zamanında `t_decide + apply_by_ns` anı geçtikten sonra uygulanmaz (`SAFETY_CLOSE` için bilinçli seçim: izin `hold_ns` ile zaten düşer; A1-K84, K87); `SAFETY_OPEN`'in verdiği izin iki saatte ayrı sayılır ve herhangi biri dolmuşsa kapalıdır: üreticide (çekirdek) olayı çıkaran çağrının `now_w`'sinden itibaren duvar saatinde `hold_ns`, alıcıda (sim sunucusu) uygulandığı andan itibaren sim zamanında `hold_ns`; `apply_by_ns` 16–1000 ms, `SAFETY_OPEN`'in `hold_ns`'i 60–450 ms aralığındadır (sahibi A1-K104; W1-uyum U7), `SAFETY_CLOSE` ve `FIRE`'da `hold_ns = 0`'dır; değerler atış kontrol parametresidir ve çekirdek aralık dışı değeri parametre doğrulamasında (A4-K80) reddeder. Bariyer beklemesinde sim zamanı durduğu için alıcıdaki süre uzamaz, üreticideki süre dolabilir; bu güvenli yöndür.
- **A4-K107.** Çekirdek bir izni ancak kendi duvar saatindeki `hold_ns` süresi dolmamışsa ve o izni veren `SAFETY_OPEN` için `IDA_EVR_APPLIED` sonucu almışsa açık sayar; izni açık tutmak istediği sürece yeni `SAFETY_OPEN`'leri, ardışık ikisinin çıkarıldığı çağrıların `now_w` farkı `hold_ns / 2`'yi aşmayacak aralıkla üretir.
- **A4-K67.** İlk kabul kapısı: platform çekirdeğin çıktısından aldığı her olayı, ilk kez kabul ederken yalnız şunların hepsi tutarsa olay tablosuna ve taşımaya alır, tutmazsa almaz ve sonuç döner: olay A4-K64'ün koşullarını taşıdığı çıktıyla birlikte geldi; A1 oturumu `IDA_LINK_RUNNING` (değilse sonuç `SESSION_END`); `event_id`'nin üst 32 biti init'te verilen `boot_count`'a eşit ve alt 32 biti aynı örnekte daha önce ilk kabulden geçen olaylarınkinden büyük (değilse `DROPPED_GATE`); izin açan olaylar (`SAFETY_OPEN`, `FIRE`) için olay tablosunda (A1-K80: `N_olay` = 8 yuva; bunlar en çok yedi yuva kullanır, bir yuva `SAFETY_CLOSE`'a ayrılmıştır) yer var ve A1-K120'nin izin açan olay kovası boş değil (değilse `DROPPED_NO_SLOT`); `SAFETY_CLOSE` yer ya da kova yokluğundan hiçbir zaman düşürülmez: kova boşken tabloda bekler (A1-K120), sekiz yuvanın hepsi doluyken yeni bir `SAFETY_CLOSE` gelirse platform oturumu kapatır (A1'in `EVENT_OVERFLOW` kapanış nedeni) ve alıcıdaki bütün izinler oturum sonuyla düşer (A4-K71, K73); platform olay üretmez ve olayın hiçbir alanını değiştirmez.
- **A4-K106.** Taşıma yinelemesi: ilk kabul kapısından geçmiş bir olayı platform ve A1, sonucu gelene, olayı çıkaran çağrının `now_w`'sinden `apply_by_ns` geçene ya da oturum bitene kadar aynı baytlarla yineleyebilir; yinelemeler ilk kabul kapısından yeniden geçmez, kimlik artışı denetimine tabi değildir ve yineleme yeni olay sayılmaz; son geçerlilik dolunca platform yinelemeyi durdurur, olayın yuvasını sonuç gelene ya da A1'in sonuç payı `T_sonuç_pay` (95 ms; A1-K80, E8) dolana kadar açık tutar ve `EXPIRED_NO_ACK`'i çekirdeğe ancak bu pay sonuçsuz dolunca verir.
- **A4-K123.** Çekirdeğin çıkardığı ve son sonucunu henüz almadığı bir `SAFETY_OPEN` ya da `FIRE` varken çekirdek o olayın açacağı iznin açık olduğunu varsaymaz, aynı tüp için yeni izin açan olay ya da `FIRE` üretmez ve `fc.inhibit_mask`'te `IDA_FCI_RESULT_PENDING`'i kurar; önceki `APPLIED` sonucuyla açılmış iznin süresi A4-K66 ve A4-K107'ye göre işlemeye devam eder.
- **A4-K68.** Platform çekirdeğin çıkardığı her olay için tam bir son sonuç (3.5 tablosundaki kodlardan biri) döndürür; sonucu aldığı ya da kendisi belirlediği andan sonraki ilk çağrının `evres[]` dizisine koyar, kapasite dolarsa sonraki çağrılara taşır ve atmaz.
- **A4-K69.** Çekirdek bir olay sonucunu yalnız `event_id` bekleyen olaylarından biriyse kabul eder; diğer sonuçları yok sayar ve `status.cnt.evres_ignored`'u artırır.
- **A4-K70.** Çekirdek bir olay için, olayı çıkardığı çağrının `now_w`'sinden `apply_by_ns + IDA_EVT_RESULT_MARGIN_NS` (100 ms; mimari seçimi, tahmin) duvar süresi içinde sonuç almazsa olayı sonucu bilinmeyen sayar, `status.cnt.event_unknown`'ı artırır ve atış kontrolü güvenli duruma alır.
- **A4-K71.** Oturum sona erdiğinde platform onaylanmamış olayları iletmeyi bırakır, yeni oturumda yeniden iletmez ve her birine `SESSION_END` sonucu döner.

### 4.8 Bağlantı, sistem ve emniyet girdileri

- **A4-K72.** Platform A1 oturumu kurulduğunda tam bir çağrıda `plat.link_event = SESSION_START`, sona erdiğinde tam bir çağrıda `SESSION_END` verir ve her çağrıda `plat.link_state`'i doldurur; `SESSION_START` gören çekirdek o çağrıda sensör kabul durumunu ve zaman çerçevesi izini sıfırlar ve kestirimi yeniden başlatır (ölçüm zamanının oturumlar arasında sürekli olduğu varsayılmaz).
- **A4-K73.** `SESSION_END` daraltıcıdır: çekirdek aynı çağrıda silahlandırmayı kaldırır, atış kontrolü güvenli duruma alır, atış kontrolün jeton ve ön izinlerini iptal eder ve bir `SAFETY_CLOSE` (`tube = IDA_TUBE_ALL`) üretir; yeni oturumda yeniden silahlandırma için yeni bir `IDA_CMD_ARM` gerekir.
- **A4-K74.** Oturum sona erdiğinde platform ertelenmiş komutları, bekleyen partiyi ve Ping360 yuvalarını siler (daraltıcı mandalları silmez, bir sonraki çağrıda verir) ve eski oturumdan gelen hiçbir başka girdiyi çekirdeğe vermez.
- **A4-K75.** Sanal emniyet girdileri (acil durdurma, silahlandırma fişi, emniyet pimi) ve fırlatıcı durumu çekirdeğe yalnız `IDA_SENS_SAFETY` örneği olarak gelir; A4'te bu girdileri değiştiren hiçbir çıkış alanı yoktur; `PERMIT_OPEN` biti fırlatıcının süreli izninin alıcıdaki durumudur ve yalnız olaylarla ve `hold_ns`'in dolmasıyla değişir; "bakım" girdisi yoktur (iskelet §10, öneri 8).
- **A4-K111.** Emniyet girdisinin kapanması, bir emniyet alanının (acil durdurma, tüp başına fiş, tüp başına pim) etkin değerinin A4-K116'ya göre bu çağrıda gevşetenden kısıtlayana dönmesidir; bu bir daraltıcı girdidir (A4-K44), A4-K113'ün (b) adımında uygulanır ve ilgili tüpte (acil durdurmada iki tüpte) çekirdek aynı çağrıda bir `SAFETY_CLOSE` üretir; etkin değeri zaten kısıtlayan olan alan için yeni `SAFETY_CLOSE` üretilmez.
- **A4-K122.** Oturum sonu çağrısı ızgara tick'inden ayrı bir çağrı türüdür: `ida_core_tick`, `tf.present = 0`, `plat.link_event = SESSION_END`, `n_samples = n_cmds = n_det = n_det_batch = 0`, `ping360` NULL (mandal bitleri ve olay sonuçları bulunabilir); platform onu A1-K107'nin zamanlamasıyla yapar; SIL'de ızgara tick'i yasağının tek istisnasıdır ve `now_w`'si son kabul edilen zaman çerçevesinin sim zamanıdır (A4-K11). Çağrı türleri girdiden şöyle ayrılır: step; `tf.present = 1` olan tick (zaman çerçeveli tick); `tf.present = 0` ve `link_event = SESSION_END` olan tick (oturum sonu çağrısı); `tf.present = 0` olan diğer tick (ızgara tick'i). Oturum sonu çağrısında çekirdek algılama akışlarının zaman ilerleme durumunu (akışta son kabul edilen partinin ölçüm zamanı ve A4-K118'in bayatlık süresi) sıfırlar; `batch_seq` ve `det_seq` kabul durumu sıfırlanmaz (A8-K17); sim'in çalıştırma nesli değişimi A1'de her zaman oturum kapanışı ve yeni oturum olduğundan karta ayrı bir nesil bildirimi gelmez.
- **A4-K76.** Batarya ve sızıntı bilgisi yalnız `IDA_SENS_BATT` ve `IDA_SENS_LEAK` örnekleriyle gelir; eşikleri ve arıza eylemleri çekirdeğin parametresi ve kuralıdır.
- **A4-K77.** Çekirdek her init'ten sonra `IDA_MODE_INIT`'te ve silahsız başlar; silahlandırma durumu hiçbir yoldan init'ler arasında taşınmaz ve `reset_cause`, `reset_flags` ve `consecutive_faults` yalnız günlüğe yazılır, çekirdeğin davranışını değiştirmez.

### 4.9 Kalıcı parametreler ve açılış sayacı

- **A4-K78.** Init'te platform kalıcı bloğu (kartta kalıcı bellekten, SIL'de A5'ten üretilmiş) `param_blob`, `param_len` ile verir; çekirdek bloğu A4-K80'e göre doğrular, geçersizse ya da blok yoksa derlenmiş varsayılanları yükler, `status.param_ok = 0` verir, `IDA_CMD_ARM`'ı `IDA_REASON_PARAM_INVALID` ile reddeder ve `IDA_RC_PARAM_DEFAULTS` döner; bu engel yalnız geçerli bir kümenin `PARAM_COMMIT` ile uygulanıp `PARAM_SAVE` ile `IDA_PSTORE_OK` olarak kaydedilmesiyle kalkar.
- **A4-K79.** Parametre bloğunun bayt düzeni 3.10'daki gibidir ve çekirdek bloğu yalnız bu düzenle okur ve yazar.
- **A4-K80.** Çekirdek bir bloğu ancak şunların hepsi tutarsa geçerli sayar: `magic`, `fmt_ver = 1` ve `rsv = 0`; `schema_hash` kendi şemasınınkine eşit; `24 + payload_len` = blok boyu ≤ 8000 ve `payload_len` 8'in katı; `payload_crc` doğru; kayıtlar `param_id`'ye göre kesin artan; şemadaki her parametre tam bir kez ve doğru `type` ile var; her değer kendi `min`–`max` aralığında; çekirdeğin ilişkili grup denetimleri geçiyor.
- **A4-K81.** `PARAM_SET` değeri bekletir ve hemen uygulamaz; `IDA_PCLS_B` parametresine silahlıyken `PARAM_SET` `IDA_REASON_ARMED` ile reddedilir; `PARAM_COMMIT` bekleyen değerleri ilişkili gruplarıyla doğrular ve geçerse adım sınırında birlikte uygular, geçmezse hepsini reddeder; bekleyen değerler arasında `IDA_PCLS_B` parametresi varken araç `PARAM_COMMIT` anında silahlıysa ya da aynı çağrıda (girdideki sırasından bağımsız) bir `IDA_CMD_ARM` varsa çekirdek commit'i `IDA_REASON_ARMED` ile reddeder ve bütün bekleyen değerleri atar.
- **A4-K82.** `PARAM_SAVE` yalnız silahsızken, `plat.param_store` `BUSY` değilken ve aynı çağrıda (sırasından bağımsız) `IDA_CMD_ARM` yokken kabul edilir (aksi halde `IDA_REASON_ARMED` ya da `IDA_REASON_PARAM_BUSY`); kabul edilince çekirdek o komutun sonucunu `IDA_RES_PARAM_SAVE` / `IDA_ACK_ACCEPTED` olarak, artan `gen`, `blob` ve `len` ile verir; `blob` aynı örnekte bir sonraki çekirdek çağrısına kadar geçerlidir ve platform kaydetmek için içeriği bu süre içinde kendi tamponuna kopyalar.
- **A4-K112.** Bir çağrıda en çok bir `PARAM_SAVE` kabul edilir ve aynı çağrıdaki diğer `PARAM_SAVE` komutları `IDA_REASON_ONE_PER_CALL` ile reddedilir; aynı çağrıda `IDA_CMD_ARM` ile `PARAM_SAVE` birlikte bulunursa `PARAM_SAVE` reddedilir (A4-K82) ve `IDA_CMD_ARM` kendi kurallarıyla değerlendirilir.
- **A4-K83.** Platform `plat.param_store`'u kayıt sürerken `BUSY`, bitince `OK` (ve `param_gen` = kaydedilen `gen`) ya da `FAIL` olarak bildirir; `BUSY` iken çekirdek `IDA_CMD_ARM`'ı `IDA_REASON_PARAM_BUSY` ile reddeder; oturum sonunda ya da platformun güvenli durumuna girişte platform süren kaydı iptal eder ve `FAIL` / `IDA_PSFAIL_ABORTED` bildirir.
- **A4-K84.** Platform bloğu yorumlamadan, bayt bayt aynı saklar ve açılışta aynen geri verir; kaydın kesintiye dayanıklılığı (eski geçerli bloğun yeni blok tamamlanana kadar korunması) platformun sorumluluğudur.
- **A4-K85.** `ida_core_param_count`, `ida_core_param_desc` ve `ida_core_param_find` örnek almaz, yalnız çekirdeğin sabit tablosunu okur ve her zaman çağrılabilir; `ida_core_param_get` örneği değiştirmez ve A4-K20'ye tabidir; `ida_core_param_find` yalnız en çok 16 karakterlik tam ad eşleşmesini kabul eder; önkoşullar ve dönüşler 4.15'tedir.
- **A4-K86.** Platform her init'te `boot_count` olarak, kartta önceki bütün açılışlarda, SIL'de aynı sim çalıştırmasındaki önceki bütün init'lerde verilenden kesin büyük bir değer verir ya da `boot_count_ok = 0` verir; aynı `boot_count` iki init'te verilmez.

### 4.10 Telemetri, sağlık, günlük

- **A4-K87.** `IDA_RC_OK` dönen her step çıktısında `nav_valid = 1` ve `nav` o adımın çıkış durumudur; tick, `IMU_REJECTED` ve hata dönüşlerinin çıktısında `nav_valid = 0`'dır.
- **A4-K88.** `status` her çıktıda (`NOT_INIT` dahil) o çağrı sonundaki durumu taşır; `status.cnt` sayaçları init'ten beri birikir ve 2³²'de sarar.
- **A4-K89.** `health.core_ok = 0` yalnız çekirdek kendi durumunun bozulduğunu (koruma sözcükleri, tamamlayıcı kopyalar, geçersiz durum değeri) saptadığında verilir; dış verinin yokluğu, bayat ya da reddedilen IMU, `IMU_LAG`, bağlantı kaybı ve kurtarılan sayısal çökme `core_ok`'u düşürmez; kartta platform `core_ok = 0` gördüğü sürece gözetim zamanlayıcısını beslemez; `health`'in diğer bitleri yalnız bildirimdir ve beslemeyi etkilemez.
- **A4-K90.** Çekirdek bir çağrıda en çok 4 günlük kaydı çıkarır ve yalnız 3.15'teki kapalı tablodaki kodları oradaki `arg0`/`arg1` anlamlarıyla üretir; tabloda olmayan kod üretilmez ve yeni kod sözleşme revizyonu ister; hiçbir günlük kaydı hedef, geometri, çözüm ya da atış ölçütü değeri taşımaz; fazla kayıtları atar ve `status.cnt.log_dropped`'u artırır.
- **A4-K91.** `fc` alanı her çıktıda atış kontrolün durumunu yalnız 3.16'daki kapalı tablolarla taşır: `state` `IDA_FCS_*` değerlerinden biridir, `inhibit_mask` yalnız `IDA_FCI_DEFINED` içindeki emniyet zinciri engellerini taşır (ayrılmış bitler 0), `last_event_id` ve `last_event_result` son çıkan olayınkilerdir; `fc`'de opak alan yoktur ve hiçbir alanı hedef, geometri, çözüm ya da ölçüt değeri taşımaz.
- **A4-K119.** Çekirdek `cmds[]`'teki bir `IDA_CMD_FC`'yi yalnız `op` A1 Tablo 3.6'nın kapalı listesindeyse (1, 5, 6, 7, 8; 2 ve 3 için A4-K46), `tube` 0–1 ise ve `arg0 = arg1 = 0` ise işler; aksi halde `IDA_ACK_REJECTED` / `IDA_REASON_OUT_OF_RANGE` ile reddeder; hiçbir FC işlemi hedef seçimi, hedef kimliği ya da atış ölçütü taşımaz.
- **A4-K120.** Çekirdek `results[].reason` alanına yalnız 0–15 tablosundaki (`IDA_REASON_*`) kodları yazar; atış kontrolün retleri de bu kodlarla bildirilir ve bu sürümde yeni neden kodu yoktur; 16–127 ayrılmıştır; 128–191 A1 bağlantı katmanının nedenleridir (A1-K64, K67) ve hiçbir port sonucu olarak üretilmez; 192 ve üstü (0x100 ve üstü dahil) ayrılmıştır ve kullanılmaz.
- **A4-K121.** Port yapılarındaki her birliğin ve koşullu alanın etkin üyesi yalnız 3.14'teki seçiciyle belirlenir; etkin olmayan üyenin baytları anlamsızdır ve okunmaz; çekirdek çıktısında etkin olmayan koşullu alanları ve dizi yuvalarını sıfırla yazar.
- **A4-K92.** `det_tx[]`, kart içi sonar algılamasının bu çağrıda ürettiği en çok 4 kaydı değerle taşır; tick, `IMU_REJECTED` ve hata dönüşlerinde `n_det_tx = 0`'dır.

### 4.11 Arka plan işi

- **A4-K93.** `ida_core_background(c, now_w)`, `c` ve `now_w` geçerliyse çekirdeğin sonar verisindeki arka plan işinden bir dilim çalıştırır, A4-K23'teki bütçeyi aşmaz ve bekleyen iş kaldıysa `IDA_BG_MORE`, kalmadıysa `IDA_BG_IDLE` döner; `now_w` A4-K09 ve A4-K10'a tabidir; geçersiz girdide A4-K117 uygulanır.
- **A4-K117.** `ida_core_background` `c` NULL ise ya da `now_w` A4-K08'in aralığı dışındaysa hiçbir arka plan işi yapmaz, işin ve çekirdeğin işlevsel durumunu değiştirmez ve `IDA_BG_BAD_INPUT` döner; bu çağrı dilim sayılmaz (çekirdeğin kendi dilim sayımına da platformun `bg_slices` sayımına da girmez); `c` NULL değilse çekirdek yalnız tanılama sayacı `status.cnt.bg_bad_input`'u artırır ve bir sonraki step ya da tick çıktısında `health.bg_bad_input = 1` verir.
- **A4-K110.** Bir arka plan çağrısının yaptığı iş miktarı ve sonucu yalnız çekirdeğin durumuna bağlıdır, süreye ya da `now_w`'ye bağlı değildir; dilim sınırı zamanla değil iş birimiyle belirlenir (iş biriminin büyüklüğü çekirdek uygulamasınındır ve A4-K23 bütçesine sığacak biçimde seçilir).
- **A4-K94.** Bir arka plan işinin sonucu, işin gönderildiği step'ten sabit `K_bg` step sonra (parametre) tüketilir; o step'te iş bitmemişse düşürülür, `out.bg.dropped` ve `status.cnt.bg_dropped` artar; erken biten iş erken tüketilmez.
- **A4-K109.** Platform her step ve tick çağrısında `bg_slices` alanına, aynı örnekte önceki step ya da tick çağrısından (ilk çağrıda init'ten) beri `IDA_BG_IDLE` ya da `IDA_BG_MORE` dönen `ida_core_background` çağrısı sayısını yazar (`IDA_BG_BAD_INPUT` dönenler sayılmaz; A4-K117); çekirdek bu sayıyı kendi saydığıyla karşılaştırır, eşit değilse `status.cnt.bg_slice_mismatch`'i artırır ve günlük kaydı üretir (davranışını değiştirmez); A6 bu sayıyı her çağrı için kaydeder; tekrarda `bg_slices` yalnız tutarlılık denetimidir: kayıtlı arka plan çağrısı sayısıyla uyuşmazsa tekrar hata verir ve tekrar oynatıcı ona bakarak arka plan çağrısı eklemez.
- **A4-K95.** Canlı SIL koşucusu her step ve tick çağrısından önce tam `N_bg` arka plan çağrısı yapar (A1-K121; `N_bg` çalıştırma parametresidir ve A6 üst verisinde `sil_n_bg` olarak saklanır), oturum sonu çağrısından (A4-K122) önce arka plan çağrısı yapmaz; tekrarda arka plan çağrıları yalnız kayıtlı çağrı dizisinden yürütülür (A6-K54, K55).
- **A4-K96.** Kartta platform, `ida_core_background` en son `IDA_BG_MORE` döndüyse her kontrol periyodunda (5 ms) en az bir arka plan çağrısı yapar.

### 4.12 Bellek, sahiplik ve hata kuralları

- **A4-K97.** Çekirdek dinamik bellek kullanmaz: çekirdek nesne dosyaları `malloc`, `calloc`, `realloc`, `free` ve özel havuz ayırıcısı sembolü içermez ve çekirdek özyineleme kullanmaz.
- **A4-K98.** Platform `ida_core_t`'yi statik (SIL'de süreç ömürlü) bir nesne olarak ayırır; `ida_core_info`'nun bildirdiği `core_size` ve `core_align` platformun derleme zamanındaki `sizeof` ve `_Alignof` değerlerine eşit değilse ya da `port_major`/`port_minor` uyuşmazsa platform çekirdeği çağırmaz ve kendi güvenli durumuna geçer.
- **A4-K99.** `ida_in_t` ve `ida_init_in_t` ile gösterdikleri her tampon çağıranındır ve yalnız çağrı süresince geçerlidir; çekirdek girdiyi değiştirmez ve çağrı döndükten sonra girdideki hiçbir işaretçiyi kullanmaz.
- **A4-K100.** `ida_out_t` çağıranındır; çıktıdaki tek işaretçi (`results[i].save.blob`) çekirdeğin belleğine işaret eder ve aynı örnekte bir sonraki çekirdek çağrısına (`init`, `step`, `tick`, `background`) kadar geçerlidir.
- **A4-K101.** Her işlev 4.15'teki önkoşulları denetler; önkoşul tutmazsa tablodaki dönüşü verir ve tablodaki çıktı kuralına uyar; hiçbir işlev NULL bir işaretçinin gösterdiği yere yazmaz ya da oradan okumaz.

### 4.13 Bütçe ve sınır değerlerinin dayanağı

**Çağrı bütçesi (A4-K23).** Kontrol periyodu 5 ms'dir (iskelet §4). MP4a'da bilerek pahalı tutulmuş yer tutucu adım, kartta `-O2` ve 200 Hz'te en kötü yükte (B) en çok 3321,96875 µs sürdü ve 121 000 adımda aşım olmadı; olağan yükte (A) 400 Hz'te en çok 1338,91875 µs'ydi (`docs/olcumler/MP4-dongu-yuku.md` §4.2, §5). Yer tutucu gerçek çekirdek değildir ve taşımayı içermez (aynı rapor §6). 3,5 ms'lik step bütçesi yer tutucunun en kötü değerini 178 µs payla karşılar ve 5 ms'lik periyottan platform için 1,5 ms ayırır; 01 §2.4'ün tahmini platform kalemleri (en uzun olağan dilim 0,3 ms, ayrıştırma 0,15 ms, çıktı kodlama 0,1 ms, kesmeler 0,125 ms) toplam 0,675 ms'dir. Bu **mimari seçimidir (tahmin)**; gerçek çekirdeğin karttaki ölçümüyle (W3 sonu) yeniden değerlendirilir (Bölüm 8). Bütçe `L_act` = 10 ms (iskelet §4) ile tek başına karşılaştırılmaz: uygulama gecikmesinin karşılanması A1 sözleşmesinin uçtan uca hesabıdır.

**Dağıtım (A4-K19, A4-K102).** Bir parti, çekirdek çağrısı yokken kapanırsa en uzun kesilemeyen platform işi (arka plan dilimi 0,3 ms, tahmin) ve ayrıştırmadan sonra başlar: ≤ 0,5 ms. Bir step sürerken kapanırsa step bitene kadar (≤ 3,5 ms) bekler, sonra ≤ 0,5 ms içinde başlar: ≤ 4,0 ms. Bekleyen parti tek olduğu ve fazlası birleştirildiği için birikme bu sınırı uzatmaz; birleştirme sıklığı A1'in geliş varsayımına bağlıdır (Bölüm 10). Bu sınırlar kartta ölçülmedi.

**IMU kesilmesi (A4-K39).** Son IMU'nun alımından `T_imu_stale` sonra bayatlık başlar. A4-K18 ardışık çağrı başlangıçlarını 11 ms ile sınırladığından, bu andan sonra başlayan ilk çağrı en geç 11 ms içinde başlar; çağrı step ise en çok 3,5 ms, tick ise en çok 0,5 ms sürer. Kullanılabilir çıktı sınırı en kötü durumda `T_imu_stale + 11 + 3,5` ms'dir; varsayılan `T_imu_stale` = 20 ms ile 34,5 ms (yalnız tick yolu kalırsa 31,5 ms), izin verilen en büyük `T_imu_stale` = 30 ms ile 44,5 ms (A1 E10: 44,5 < 60 ms en küçük `valid_for`). Süre IMU örneğinin A1-K48 damgasından sayılır; gerçek alım anına göre sınır, A1'in damga sapması kadar farklıdır. Hesap, arada hata dönüşü olmadığını varsayar (A4-K28 hata dönüşünde sürekli komut vermez; o durumda sim kullanım süresiyle nötrler, iskelet §4).

---

### 4.14 Dönüş koduna göre çıktı (A4-K26, A4-K28, A4-K103, A4-K113, A4-K114)

Dönüş kodunun hangisi olacağı 3.11'deki değerlendirme sırasıyla belirlenir; bu tablo yalnız seçilen kodun çıktısını verir. "Son geçerli" = son kabul edilen zaman çerçevesinin değeri (hiç yoksa 0). "(b) daraltması" = A4-K113'ün (b) adımında uygulanan daraltma (mandal bitleri, `SESSION_END`, A4-K116). Bütün satırlarda `status`, `health` (`core_ok` dahil) ve `fc` geçerlidir.

| Dönüş | `thr_valid` | `is_safe` | `thr_cmd` | `valid_for_ns` | `seq`, `t_ref` | Olaylar | Komut sonuçları | `nav_valid` | Örnek ve kayıt kabulü | `det_tx`, `bg.submitted` |
|---|---|---|---|---|---|---|---|---|---|---|
| step `OK` | 1 | A4-K55 | komut ya da 0 (güvenli) | 60–450 ms | `tf.seq`, `tf.t` | Hepsi (A4-K45, A4-K64, A4-K108 saklı) | Mandal bitleri ve her komut A4-K43'e göre; daraltma varsa genişleticiler `NARROWED` | 1 | Evet (A4-K33, A4-K115) | Geçerli |
| tick `OK` | A4-K56: 0 ya da 1 (yalnız nötr güvenli) | A4-K55 | 0 | 0 ya da 60–450 ms | `tf` varsa `tf.seq`, `tf.t`; ızgarada son geçerli | Yalnız `SAFETY_CLOSE` | step `OK` ile aynı; genişleticiler ayrıca kip ve izin kurallarına tabi | 0 | Evet | 0 |
| step `IMU_REJECTED` | A4-K56: 0 ya da 1 (yalnız nötr güvenli) | A4-K55 | 0 | 0 ya da 60–450 ms | `tf.seq`, `tf.t` | Yalnız `SAFETY_CLOSE` | Mandal bitleri `ACCEPTED`; genişleticiler `NARROWED` (daraltma varsa) ya da `IMU_REJECTED`; diğerleri olağan | 0 | IMU hariç evet | 0 |
| `BAD_INPUT` | **0** | Sınıf (bilgi) | 0 | 0 | Son geçerli (girdideki geçersiz zaman yazılmaz) | Yalnız (b) daraltmasından doğan `SAFETY_CLOSE` | Uygulanan mandal bitleri `ACCEPTED`; `n_cmds` ≤ 8 ise her komut `REJECTED` / `CALL_REJECTED`, `n_cmds` > 8 ise komutlar için sonuç yok (A4-K114) | 0 | Hayır (yalnız A4-K116'nın kısıtlayıcı içeriği) | 0 |
| `DUP_SEQ` | **0** | Sınıf (bilgi) | 0 | 0 | Son geçerli | `BAD_INPUT` ile aynı | Uygulanan mandal bitleri `ACCEPTED`; her komut `REJECTED` / `CALL_REJECTED` | 0 | `BAD_INPUT` ile aynı | 0 |
| `OLD_SEQ` | **0** | Sınıf (bilgi) | 0 | 0 | Son geçerli | `BAD_INPUT` ile aynı | `DUP_SEQ` ile aynı | 0 | `BAD_INPUT` ile aynı | 0 |
| `NOT_INIT` | **0** | 1 | 0 | 0 | 0, 0 | Yok | Mandal bitleri ve (`n_cmds` ≤ 8 ise) komutlar `REJECTED` / `NOT_INIT`; hiçbir daraltma uygulanmaz | 0 | Hayır | 0 |

Hata dönüşlerinde `is_safe`, çağrı sonundaki durumun A4-K55'e göre sınıfını bilgi olarak verir; sürekli komut taşımaz. Kabul edilen bir çağrıda bir genişleticinin ret nedeni önceliği: `NARROWED` (A4-K45) > `IMU_REJECTED` (A4-K103) > `IMU_LAG` (A4-K108) > komutun kendi kuralının nedeni. Örnek: `DISARM` mandalı + `ARM` + yinelenen `tf.seq` → dönüş `DUP_SEQ`, `DISARM` uygulanmış ve `ACCEPTED`, `ARM` `REJECTED` / `CALL_REJECTED`; aynı girdi `tf.seq` yeni olsaydı → dönüş `OK`, `ARM` `REJECTED` / `NARROWED`.

### 4.15 İşlevler: önkoşullar, dönüşler, hata halinde çıktı (A4-K101)

| İşlev | İşaretçi önkoşulları | Dönebilecek kodlar | Hata halinde çıktı |
|---|---|---|---|
| `void ida_core_info(ida_core_info_t *info)` | `info` NULL olabilir | — | `info` NULL ise hiçbir şey yapmaz; değilse her alanı yazar |
| `ida_rc_t ida_core_init(ida_core_t *c, const ida_init_in_t *in)` | `c`, `in` NULL değil; `in->param_blob` NULL ise `in->param_len` = 0 | `OK`, `PARAM_DEFAULTS`, `VERSION`, `BAD_INPUT` | `c` NULL: hiçbir yere yazmaz, `BAD_INPUT`. `in` NULL, `param_blob`/`param_len` çelişkili, `now_w` aralık dışı: örneği kurulmamış işaretler, `BAD_INPUT`. `VERSION`: örneği kurulmamış işaretler. `PARAM_DEFAULTS`: örnek kurulu |
| `ida_rc_t ida_core_step(ida_core_t *c, const ida_in_t *in, ida_out_t *out)` | `c`, `in`, `out` NULL değil; `in->ping360` NULL olabilir | `OK`, `IMU_REJECTED`, `DUP_SEQ`, `OLD_SEQ`, `BAD_INPUT`, `NOT_INIT` | Sıra ve öncelik 3.11 (A4-K113). `c` ya da `in` NULL: hiçbir yere yazmaz, durum değişmez, `BAD_INPUT`. Örnek kurulmamış: `NOT_INIT` (`out` NULL değilse 4.14). `out` NULL (c ve in geçerli, örnek kurulu): (b) daraltmasını örneğe uygular, başka hiçbir şey yapmaz ve yazmaz, `BAD_INPUT`. Diğer hatalar: 4.14 |
| `ida_rc_t ida_core_tick(ida_core_t *c, const ida_in_t *in, ida_out_t *out)` | step ile aynı | `OK`, `DUP_SEQ`, `OLD_SEQ`, `BAD_INPUT`, `NOT_INIT` | step ile aynı |
| `ida_bg_rc_t ida_core_background(ida_core_t *c, ida_wtime_t now_w)` | `c` NULL olabilir | `IDA_BG_IDLE`, `IDA_BG_MORE`, `IDA_BG_BAD_INPUT` | Sıra 3.11. `c` NULL ya da `now_w` aralık dışı: iş yapmaz, `IDA_BG_BAD_INPUT`; dilim sayılmaz; `c` NULL değilse yalnız `cnt.bg_bad_input` artar ve sonraki step/tick'te `health.bg_bad_input = 1` (A4-K117). Örnek kurulmamış: iş yapmaz, `IDA_BG_IDLE`; platformun `bg_slices` sayımına girer (sonraki init sayımı sıfırlar) |
| `uint16_t ida_core_param_count(void)` | — | parametre sayısı | — |
| `ida_rc_t ida_core_param_desc(uint16_t index, ida_param_desc_t *d)` | `d` NULL değil | `OK`, `NOT_FOUND`, `BAD_INPUT` | `d` NULL: `BAD_INPUT`. `index` ≥ sayı: `NOT_FOUND`. Hata halinde `*d` yazılmaz |
| `ida_rc_t ida_core_param_find(const char *name, uint16_t *id)` | `name` ve `id` NULL değil; `name` NUL ile biter | `OK`, `NOT_FOUND`, `BAD_INPUT` | NULL: `BAD_INPUT`. Ad 16 karakterden uzun ya da yok: `NOT_FOUND`. Hata halinde `*id` yazılmaz; çekirdek `name`'den en çok 17 bayt okur |
| `ida_rc_t ida_core_param_get(const ida_core_t *c, uint16_t id, ida_param_value_t *v)` | `c`, `v` NULL değil | `OK`, `NOT_FOUND`, `NOT_INIT`, `BAD_INPUT` | NULL: `BAD_INPUT`. Örnek kurulmamış: `NOT_INIT`. Bilinmeyen `id`: `NOT_FOUND`. Hata halinde `*v` yazılmaz |

---

## 5. Hata durumları

| # | Durum | Algılama | Çekirdeğin davranışı | Platformun davranışı |
|---|---|---|---|---|
| H1 | Port sürümü uyuşmuyor | init: `port_major/minor` (A4-K02); platform: `ida_core_info` (A4-K98) | `IDA_RC_VERSION`; örnek kurulmaz | Çekirdeği çağırmaz; kendi güvenli durumuna geçer |
| H2 | `ida_core_t` boyutu ya da hizalaması uyuşmuyor | `ida_core_info` (A4-K98) | — | Çekirdeği çağırmaz; güvenli durum |
| H3 | Kurulmamış örnek çağrıldı | Örnek kurulmamış | `IDA_RC_NOT_INIT`; 4.14 | Hatayı günlüğe yazar |
| H4 | NULL işaretçi | 4.15 | 4.15 | — |
| H5 | Yapısal girdi hatası (kapasite aşımı, step'te IMU yok, tick'te IMU var, geçersiz `now_w`/`tf`) | A4-K08, K15, K17, K31 | `BAD_INPUT`; 4.14; daraltıcılar uygulanır | Çıktıyı A1 kuralına göre kullanır |
| H6 | Yinelenen ya da eski zaman çerçevesi | A4-K25 | `DUP_SEQ` / `OLD_SEQ`; 4.14 | A1 kuralı (A4-K29) |
| H7 | Duvar saati geri adımı | A4-K10 | Önceki değeri kullanır; sayar | Platform hatasıdır; kartta K20 testiyle dışlanır |
| H8 | Geçersiz, yinelenen, eski ya da gelecek damgalı örnek | A4-K33 | Reddeder, sayar; tazelik yenilenmez | — |
| H9 | IMU örneği reddedildi | A4-K103 | `IMU_REJECTED`; tick kuralları; genişletici yok | Çıktıyı olağan tick çıktısı gibi kullanır |
| H10 | IMU düzenli geliyor ama ölçüm zamanı geride | A4-K37 | `IMU_LAG` mandalı; A4-K108: genişletici yok, arm reddi, kontrol sürer | — |
| H11 | IMU kesildi (zaman çerçevesi sürüyor ya da hiç veri yok) | A4-K39 | Nötr güvenli çıktı | Kartta ızgara tick'iyle çağırmayı sürdürür (A4-K18) |
| H12 | IMU aralığında boşluk | A4-K38 | Sayar, günlük | — |
| H13 | Bozuk Ping360 satırı | A4-K40 | Satırı reddeder; çağrı sürer | — |
| H14 | Geçersiz algılama kaydı ya da partisi | A4-K41 | Reddeder, sayar | — |
| H15 | Çağrı sürerken bir ya da iki parti daha kapandı | A4-K102 | Birleştirilmiş girdiyi işler; `health.input_dropped` | Bekletir, gerekirse birleştirir, sayar |
| H16 | Daraltıcı komut yağmuru | A4-K46 | Mandal bitlerini uygular | Mandal; birleştirir ve sayar; hiçbirini atmaz |
| H17 | Daraltıcı olmayan komut kapasitesi aşıldı | A4-K105 | Gelen sekiz komutu işler | Erteler; tampon doluysa atar ve sayar |
| H18 | Aynı çağrıda daraltıcı ve genişletici | A4-K45 | Daraltma hâkim; genişleticiler `NARROWED` | — |
| H19 | Sonlu olmayan çıktı (çekirdek hatası) | A4-K59 | — | Nötr güvenli çıktıyla değiştirir, olayları `DROPPED_GATE`, sayar |
| H20 | İlk kabul kapısı tutmuyor | A4-K67 | Sonucu alır; atış kontrolü kendi kuralıyla tepki verir (W4) | Olayı almaz; `DROPPED_GATE`, `DROPPED_NO_SLOT` ya da `SESSION_END` |
| H21 | Olaya yanıt yok | Platform: son geçerlilik (A4-K106); çekirdek: A4-K70 | Sonucu bilinmeyen sayar; atış kontrolü güvenli duruma | `EXPIRED_NO_ACK` döner |
| H22 | Yabancı ya da beklenmeyen olay sonucu | A4-K69 | Yok sayar, sayar | — |
| H23 | Süreli iznin süresi iki saatte farklı doldu | A4-K66, A4-K107 | Kendi duvar süresi dolunca izni kapalı sayar | Alıcı kendi sim süresiyle kapatır (A1) |
| H24 | Oturum sona erdi | `plat.link_event` | A4-K73 | A4-K71, A4-K74, A4-K83 |
| H25 | Açılış sayacı ayrılamadı ya da tükendi | `boot_count_ok = 0` ya da aralık dışı | Olaysız çalışma, arm reddi (A4-K61) | Açılış kaydında bildirir |
| H26 | Olay sayacı tükenmeye yaklaştı ya da tükendi | A4-K62 | Yalnız `SAFETY_CLOSE`, sonra hiç olay; arm reddi | — |
| H27 | Parametre bloğu geçersiz | A4-K80 | Varsayılanlar; `PARAM_DEFAULTS`; arm reddi | Açılış kaydında bildirir |
| H28 | Silahlıyken ya da `ARM` ile aynı çağrıda B sınıfı commit veya kayıt | A4-K81, A4-K82 | Reddeder; bekleyen değerleri atar | — |
| H29 | Parametre kaydı sürüyor ya da başarısız | `plat.param_store` | `BUSY` iken arm reddi; `FAIL`'de günlük | A4-K83 |
| H30 | Çağrı süresi bütçeyi aştı | Platform ölçümü (A4-K23) | `prev_overrun` ve atlanan ya da birleştirilen partiyi kaçırılan adım sayar; arıza eylemi çekirdeğin kuralına göre | Sonraki çağrıda bildirir |
| H31 | Arka plan işi zamanında bitmedi | A4-K94 | İşi düşürür, sayar | `bg_slices` ile A6'ya yazılır; tekrar oynatılabilir (A4-K109) |
| H32 | `bg_slices` çekirdeğin sayımıyla uyuşmuyor | A4-K109 | Sayar, günlük; davranış değişmez | Platform hatasıdır |
| H33 | Çekirdeğin iç durumu bozuk | A4-K89 | `core_ok = 0` | Gözetimi beslemez; kartta sıfırlama |
| H34 | Çağrılar arası aralık 11 ms'yi aştı (kart) | Çekirdek `now_w` farkından görür | Duvar saatine bağlı süreler gerçek aralıkla işler; günlük | Platform hatasıdır; kartlı testte ölçülür |
| H35 | Reddedilen çağrıda daraltıcı girdi (ör. `DISARM` mandalı + `ARM` + yinelenen `tf.seq`) | A4-K113 | Dönüş (c)/(d)'ye göre (`DUP_SEQ`); daraltma (b)'de uygulanır; komutlar `CALL_REJECTED`; `NARROWED` verilmez | Çıktıyı A1 kuralına göre kullanır; mandalı temizler |
| H36 | Aynı çağrıda yapısal hata ve yinelenen sıra | A4-K113 | `BAD_INPUT` | — |
| H37 | Mandal alanında tanımsız bit ya da geçersiz hedefli `NARROW_MODE` | A4-K46, A4-K113 (c) | Tanımlı bitler uygulanır; `BAD_INPUT` | Platform hatasıdır |
| H38 | Eşit anahtarlı örnekler: aynı içerik ya da çelişkili içerik | A4-K115 | Aynıysa biri değerlendirilir, diğerleri `sample_dup_dropped`; çelişkiliyse hepsi reddedilir, `sample_conflict`, `health.sample_conflict` | — |
| H39 | Reddedilen emniyet örneğinde daraltıcı içerik (çelişkili çift, eski ya da yinelenen anahtar, reddedilen çağrı) | A4-K116 | Daraltıcı içerik o çağrı için uygulanır, genişletici içerik uygulanmaz; `narrow_from_rejected` | — |
| H40 | Arka plan çağrısında `c` NULL ya da geçersiz `now_w` | A4-K117 | İş yok; `IDA_BG_BAD_INPUT`; `cnt.bg_bad_input`; sonraki step/tick'te `health.bg_bad_input` | Çağrıyı `bg_slices`'a saymaz |
| H41 | Kapalı liste dışı FC işlemi ya da sıfır olmayan `arg0`/`arg1` | A4-K119 | `REJECTED` / `OUT_OF_RANGE` | A1 zaten `LINK_BAD_FIELD` ile reddeder (A1-K64) |
| H42 | A1 Tablo 3.6a'yı tutmayan görev öğesi | A4-K51 | Öğe ve yükleme reddedilir | A1-K122 |
| H43 | Algılama partisinde bir kayıt geçersiz | A4-K41 | Parti bütünüyle reddedilir | — |
| H44 | Kamera algılama akışından `T_det_stale` boyunca başlık yok | A4-K118 | Akış bayat; kayıtları kullanılmaz; `IDA_FCI_DET_STALE` | — |
| H45 | Sonucu beklenen izin olayı ya da `FIRE` | A4-K123 | Aynı tüp için yeni izin olayı ya da `FIRE` yok; `IDA_FCI_RESULT_PENDING` | Yuvayı `apply_by + 95 ms`'ye kadar tutar (A4-K106) |
| H46 | Sekiz olay yuvası doluyken yeni `SAFETY_CLOSE` | A4-K67 | Oturum sonu çağrısını işler (A4-K73) | Oturumu `EVENT_OVERFLOW` ile kapatır |

---

## 6. Kalite kademelerinin etkisi

Çekirdekte ve portta kalite kademesi parametresi yoktur (iskelet §6). Etkiler yalnız girdinin içeriğindedir:

- Sonar kademesi Ping360 satırının içeriğini değiştirir, biçimini değiştirmez; örnek üst sınırı iki kademede de 1200'dür (`n_samples` 1–1200; A1 ve A5-K78 ile aynı) ve kademe farkı örnek üst sınırından türetilmez.
- Kamera kapalıyken algılama kaydı gelmez (`n_det` = 0); sim A8-K64'e göre 1 Hz'te `NO_FRAME` / `CAMERA_OFF` durum başlığı gönderir, bu başlığı taşıyan çağrıda `n_det_batch` 0 değildir ve başlık kamera akışını taze tutar (A4-K118); çekirdek bu durumda yalnız kendi sonar algılamasıyla çalışır.
- Emniyet zinciriyle ilgili bütün kurallar (A4-K28, A4-K44–K46, A4-K55, A4-K60–K75, A4-K103–K108, A4-K111, A4-K113–K116, A4-K119–K123) her kademede aynıdır ve aynı sınamalardan geçer (iskelet §6).

---

## 7. Uygunluk

Test kodu bu işte yazılmaz. Tür kısaltmaları: **D** derleme ya da sembol denetimi; **B** birim (çekirdek ya da platform tek başına, PC'de); **S** sözleşme testi (sahte platformla çekirdek ya da sahte çekirdekle platform; girdi ve çıktı yapıları üzerinden); **SIL** sim sunucusu ve SIL koşucusuyla; **K** kartlı (HIL).

| Kurallar | Tür | Sınama yöntemi ve gözlenecek büyüklük |
|---|---|---|
| K01 | D | `cc` ve `arm-none-eabi-gcc` ile `-std=c17 -pedantic-errors -Wall -Wextra -fsyntax-only`; `c++ -std=c++20 -fsyntax-only`; A8 başlığıyla birlikte; uyarı sayısı 0 |
| K02, K98 | S | Farklı sürüm sabitleriyle init → `IDA_RC_VERSION`, örnek kurulmamış (sonraki step `NOT_INIT`); platform tarafında sahte `ida_core_info` uyuşmazlığında çekirdek çağrısı sayısı 0 |
| K03 | D, inceleme | A1 ve A6 kodlayıcılarında port yapısının `memcpy`/`fwrite` ile bütün olarak yazılmadığının taraması |
| K04, K05, K06 | D | `port.h`'ta `double` ve `pragma` taraması; FLT_MANT_DIG denetimi |
| K07 | D | Ölçüm zamanını duvar saatine atayan/çıkaran örnek kodun derlenmemesi (derleme hatası testi); çekirdek kaynağında iki alanın birlikte kullanıldığı ifadelerin statik taraması |
| K08, K15, K17, K31 | S | Sınır değerleriyle girdiler; dönüş kodu ve `cnt.bad_input` |
| K09, K18, K122 | K, SIL | Duvar saatinin taşmaya yakın başlatılıp 10⁷ okunması (01 K20); IMU durdurulup yalnız ızgara tick'leriyle 10 dk: ardışık çağrı başlangıçları aralığının en büyüğü < 11 ms; zaman çerçevesi akarken (A1-K117 takvimi): en büyüğü ≤ 10 ms ve beklenen zaman çerçevesinin ±1 ms çevresinde başlayan tick sayısı 0; oturum sonu çağrısının alanları (A4-K122) ve SIL'de `now_w` = son kabul edilen zaman çerçevesinin zamanı; oturum sonu çağrısından sonra kamera akışının zaman ilerleme durumunun sıfırlanması (yeni oturumda, eski oturumun son başlığından daha küçük `t_meas_ns`'li başlığın kabulü) ve `batch_seq` kabul durumunun sıfırlanmaması (A8-K17) |
| K10 | S | Azalan `now_w` dizisi; `cnt.wall_backstep` ve çıktıdaki süre davranışı |
| K11 | SIL | Her çağrıda `now_w == tf.t`; ızgara tick'i sayısı 0 |
| K12, K22, K97 | D | Çekirdek nesnelerinde saat, `rand`, `malloc` ailesi ve yazılabilir veri bölümü sembolü taraması (`nm`; macOS ve kart); `-fstack-usage` ile özyineleme yokluğu |
| K13 | SIL | Sim tarafındaki yer gerçeği ile çekirdeğe ulaşan `t_meas`'ın tür başına eşitliği (aralık sonu, yansıma anı, gönderim anı) |
| K14 | K | Kartta `t_rx` ≤ `now_w` ihlali sayısı 0 |
| K19, K102 | K, B (platform) | Kartta `now_w − tf.t_rx` dağılımının en büyüğü ≤ 4 ms ve step sürmüyorken ≤ 0,5 ms; sahte çekirdekle (yapay olarak 3,5 ms süren step) iki ve üç partinin üst üste kapanması: bekleyen parti sayısı en çok 1, birleştirilmiş girdide `tf` en yeni, IMU en yeni, düşen üye sayacı ve `batch_merged` doğru |
| K16, K25, K26, K28, K29, 4.14 | S | Parti dizileri: olağan, DUP, OLD, GAP, birleştirilmiş, eksik, geçersiz `tf.t`; her dönüş kodu için 4.14 tablosunun her sütunu; geçersiz `tf.t`'li `BAD_INPUT`'ta `out.t_ref` = son geçerli; taze IMU'lu hatalı tick'te `thr_valid = 0` |
| K20, K21 | İnceleme, SIL | Kart çağrı noktalarının tek bağlamdan yapıldığının kod incelemesi; SIL'de aynı süreçte iki örneğin ayrı ayrı koşusu tek başına koşularıyla bit düzeyinde aynı |
| K23 | K | DWT ile step, tick ve background sürelerinin dağılımı ve en büyüğü (W3 sonu ölçümü); bütçe aşım sayısı |
| K24, K109, K95, K110 | SIL, S | SIL port kaydının (`bg_slices` ve arka plan çağrı dizisi dahil) iki tekrarının birbiriyle ve kayıttaki çıktılarla bit düzeyinde eşitliği (A6-K34, K56); canlı iki SIL koşusunun eşitliği yalnız kamera kapalıyken sınanır (iskelet §4); kayıtlı `bg_slices` ile kayıtlı arka plan çağrısı sayısı uyuşmazken tekrarın hata vermesi; yanlış `bg_slices`'ta `cnt.bg_slice_mismatch` artışı ve çıktının değişmemesi; arka plan çağrısına farklı `now_w` verilmesinin sonucu değiştirmemesi; canlı SIL'de her step ve tick öncesi arka plan çağrısı sayısının tam `N_bg`, oturum sonu çağrısı öncesinde 0 olması |
| K27 | S | Her çağrıdan önce `out`'u desenle doldurup çağrı sonrası desenin hiçbir alanda kalmadığını denetleme |
| K30, K33, K34 | S | Tür başına geçersiz bayrak, sonlu olmayan değer, yinelenen `seq`, eşit `t_meas`, gelecek damga; kabul/ret ve sayaçlar; SAFETY'de eşit `t_meas`'la değer güncellenir ama tazelik yenilenmez |
| K113, K114, 3.11 | S | (1) `DISARM` mandalı + `ARM` + `tf.seq` = son kabul: dönüş `DUP_SEQ`, çağrı sonunda `armed = 0`, `DISARM` sonucu `ACCEPTED`, `ARM` sonucu `REJECTED` / `CALL_REJECTED`, `NARROWED` hiç görülmez; aynı girdi yeni `tf.seq` ile: dönüş `OK`, `ARM` `NARROWED`. (2) `n_samples` = 17 ve `tf.seq` = son kabul: dönüş `BAD_INPUT`. (3) `n_cmds` = 9 ile `BAD_INPUT`: komutlar için sonuç yok, mandal sonuçları var. (4) Tanımsız mandal biti (0x10) + `DISARM` biti: `BAD_INPUT`, `armed = 0`. (5) `out` NULL + `DISARM` mandalı: `BAD_INPUT`, sonraki çağrıda `armed = 0`. (6) 3.11 öncelik tablosunun her dolu hücresi için bir girdi |
| K115, K116 | S | Son kabul edilen `estop = 0` ve fiş takılı iken aynı anahtarlı iki SAFETY örneği, `estop = 0` / `estop = 1`, iki sıralamada: her iki sıralamada sonuç aynı, ikisi de reddedilir, `sample_conflict` +1, `health.sample_conflict = 1`, çağrı daraltılmış (genişleticiler `NARROWED`, `SAFETY_CLOSE`); aynı sınama fiş biti 1/0 çiftiyle; bayt bayt aynı iki örnekte biri kabul, `sample_dup_dropped` +1; önceki çağrıda kabul edilmiş anahtarın `estop = 1` ile yeniden gelmesi: örnek reddedilir, daraltma uygulanır, son kabul edilen değer değişmez; `DUP_SEQ` dönen çağrıdaki `estop = 1` örneği: daraltma uygulanır |
| K32 | B (platform) | Kapasiteyi aşan parti: taşınan/atılan örnek sayısı ve sayaç; olay sonucu kaybı 0 |
| K35 | S | Aynı örneklerin (eşit ve çelişkili anahtarlılar dahil) rastgele sıralarıyla 1000 çağrı dizisi; çıktılar bit düzeyinde aynı |
| K36 | İnceleme | Platform kodunda tazelik kararı yokluğu |
| K37, K108 | S, SIL | IMU `t_meas`'ı tf'nin 25 ms gerisinde ama düzenli gelen akış: mandal kurulur, genişletici komutlar `IMU_LAG` ile reddedilir, `SAFETY_OPEN`/`FIRE` 0, kontrol çıktısı sürer; gecikme 5 ms'ye inince 20 çağrı sonra mandal çözülür, 19'da çözülmez |
| K38 | S | Bir IMU aralığı atlanmış akış; `cnt.imu_gap` artışı 1 |
| K103 | S | Step'te NaN'lı, `VALID` = 0 ve yinelenen `seq`'li IMU: dönüş `IMU_REJECTED`, `nav_valid = 0`, `health.imu_rejected = 1`, genişletici olay 0, aynı çağrıdaki diğer örnekler kabul, IMU bayatsa nötr güvenli çıktı |
| K39 | S, SIL, K | IMU kesilmesi: son IMU'nun A1-K48 alım damgasından nötr güvenli çıktıyı taşıyan çağrının **dönüşüne** kadar geçen süre (kartta çağrı dönüşünde platformun aldığı duvar saati damgasıyla); `T_imu_stale` = 5, 20 ve 30 ms ile; sınır SIL'de `T_imu_stale` + 5 ms (sanal), kartta `T_imu_stale` + 14,5 ms; yalnız ızgara yolunda ve parti akışı sürerken ayrı ayrı; `T_imu_stale` = 4 ve 31 ms içeren parametre kümesinin reddi |
| K40 | S | Sınır dışı Ping360 alanları; satır reddi ve çağrının `OK` dönmesi |
| K41, K118 | S | A8'in geçersiz kayıt ve parti örnekleri; iki kayıtlı partide bir kaydı bozuk: hiçbir kayıt kullanılmaz, `cnt.det_rejected` +1; başlıksız kayıt; kamera akışında 2 s başlık gelmemesi: `IDA_FCI_DET_STALE` kurulur ve o akışın sonraki kayıtları (yeni başlık gelene kadar) kullanılmaz, `CAMERA_OFF` başlıklarının 1 Hz gelmesiyle akış taze kalır |
| K90, K91, K119, K120, K121 | D, S | Başlıkta `IDA_FC_SELECT`, `IDA_MI_TARGET_ALIGN`, `fc_ref`, `ext` adlarının yokluğu (sembol taraması); `op` = 4 ve 9, `arg0` ≠ 0 ya da `arg1` ≠ 0 olan FC komutunun `OUT_OF_RANGE` ile reddi; `item_type` = 8 ve Tablo 3.6a'nın her koşulunun ihlaliyle yüklemenin bütünüyle reddi; çekirdeğin ürettiği bütün günlük kodlarının 1–23, `inhibit_mask`'in `IDA_FCI_DEFINED` içinde, `state`'in 0–5, `reason`'ın 0–15 içinde olması (uzun SIL koşusunda ve her hata yolu testinde gözlenir); etkin olmayan birlik üyelerine ve dizi yuvalarına çekirdeğin sıfır yazması |
| K116, K44, K111 | S | Son kabul edilen `estop` = 1 iken aynı çağrıda bayat `estop` = 1 örneği ile yeni `estop` = 0 örneği: etkin değer 1 kalır, yeni `SAFETY_CLOSE` ve sayaç yok; sonraki çağrıda yalnız `estop` = 0 örneği: etkin değer 0; bir tüpün pimi takılıyken (etkin) `IDA_CMD_ARM`'ın kabulü ve o tüp için `SAFETY_OPEN`/`FIRE` üretilmemesi, öteki tüp için üretilebilmesi; etkin değerin kısıtlayana döndüğü çağrıda bütün genişleticilerin `NARROWED` |
| K123, K67, K106 | B (platform), S | Sonucu bekleyen `SAFETY_OPEN` varken aynı tüp için yeni izin olayı ve `FIRE` üretilmemesi, `RESULT_PENDING` biti; izin açan olaylarla yedi yuva doluyken sekizinci izin olayının `DROPPED_NO_SLOT`, `SAFETY_CLOSE`'un kabulü; sekiz yuva doluyken yeni `SAFETY_CLOSE`'ta oturumun `EVENT_OVERFLOW` ile kapanması; kova boşken izin olayının `DROPPED_NO_SLOT`, `SAFETY_CLOSE`'un bekletilmesi; son geçerlilik dolduktan sonra yinelemenin durması ve `EXPIRED_NO_ACK`'in ancak `apply_by + 95 ms`'de verilmesi |
| K42 | SIL | Dönen araçta IMU montajı gövde referans noktasından uzakken kestirim hatasının montajsız duruma göre karşılaştırılması (dönme kolunun çıkarıldığının göstergesi) |
| K43, K48 | S | Her komut türü ve her mandal biti için sonuç sayısı ve `cmd_id` eşleşmesi; iki `PARAM_GET` ve iki `PARAM_SAVE` içeren çağrıda iki ayrı sonuç; yinelenen `cmd_id`'de `DUPLICATE` ve durum değişmezliği |
| K44, K45, K104, K111 | S | Her daraltıcı girdi (dört mandal biti, `SESSION_END`, `estop`, fişin çıkması, pimin takılması) ile her genişleticinin (`ARM`, `SET_MODE(GUIDED)`, `FAILSAFE_RESET`, `FC_ARM/SELECT/FIRE/RESET/TEST_ENTER`) aynı çağrıda, genişletici dizide önce ve sonra iken: kabul edilen çağrıda (`OK`, `IMU_REJECTED`) çağrı sonunda `armed = 0` (DISARM'da), genişletici sonucu `NARROWED`, `SAFETY_OPEN` ve `FIRE` 0, `health.narrowed = 1`; reddedilen çağrıda daraltma yine uygulanmış ve genişletici sonucu `CALL_REJECTED` |
| K46, K105, K47 | B (platform), S | Yalnız daraltıcılardan oluşan yük: bir çağrı aralığında 100 `DISARM`, 50 `FC_ABORT`, 20 daraltıcı `SET_MODE`: bir sonraki çağrıda dört bitin kurulu olması, `cmd_id` = son kimlik, `mode` = son hedef, `coalesced` ve `cnt_narrow_coalesced` doğru, daraltıcı kaybı 0; karışık yükte daraltıcı olmayan erteleme ve atma sayaçları |
| K49, K50, K51 | S | Kip başına manuel eksenlerin etkisi, geçersiz maske, görev yüklemesinin atomikliği (yarım yüklemede görev değişmez) |
| K52 | S | HEARTBEAT kesilmesinde arıza eyleminin duvar saati süresinde başlaması |
| K53, K55, K56, K57, K58 | S | Her çıktıda alanların tutarlılığı: `thr_valid ∧ is_safe` ⇒ sıfır komut; `thr_valid = 0` ⇒ `valid_for_ns = 0` ve sıfır komut; `seq`/`t_ref` kuralı |
| K54 | B (platform) | Bilinen `thr_cmd` değerleri için ESC darbe genişliği ve kırpma |
| K59 | B (platform) | NaN ve 1,5 değerli `thr_cmd`; nötrle değiştirme ve `DROPPED_GATE` |
| K60–K65 | S | Olay alanları; `event_id` biçimi; sayaç sınır değerleriyle başlatılmış örnek (çekirdek uygulamasının test kancasıyla); tick ve `IMU_REJECTED`'de genişletici olay 0; dizi sırası |
| K66, K107 | S, SIL | `APPLIED` gelmeden iznin açık sayılmaması; yenileme aralığının `hold_ns/2`'yi aşmaması; bariyer beklemesinde (sim zamanı durmuşken) çekirdeğin izni duvar süresinde kapalı sayması ve `FIRE` üretmemesi; alıcıda sim süresi dolunca `PERMIT_OPEN = 0` |
| K67, K106, K68, K71 | B (platform), SIL | İlk kabul kapısının her koşulunun tek başına olayı düşürmesi; kapıdan geçmiş olayın 5 kez yinelenmesinde hiçbir yinelemenin `DROPPED_GATE` almaması; olay başına son sonuç sayısı tam 1; oturum sonunda `SESSION_END` |
| K69, K70 | S | Yabancı `event_id`; yanıtsız olay için sürenin dolması ve sayaç |
| K72, K73, K74 | S, SIL, K | Silahlı ve izinli durumda oturum kopması: aynı çağrıda `armed = 0`, `SAFETY_CLOSE`; aynı çağrıdaki `ARM` reddi; yeni oturumda eski arm komutunun etkisizliği (01 K17) |
| K75, K76 | S, inceleme | Çıktı yapısında emniyet girdilerini değiştiren alan yokluğu (başlık incelemesi); `PERMIT_OPEN` değişiminin yalnız olaylarla görülmesi (SIL) |
| K77 | S | Her `reset_cause` değeriyle init; çıktı farkı yok, `armed = 0` |
| K78–K84, K112 | B, S, K | Bozuk magic, sürüm, özet, CRC, sıra, eksik ve aralık dışı değer içeren bloklar: `PARAM_DEFAULTS`; geçerli blok: `OK`; silahsızken bekletilen B sınıfı değerin silahlandıktan sonra commit'i: ret ve bekleyenlerin atılması; `ARM` + `PARAM_COMMIT` ve `PARAM_COMMIT` + `ARM` sıralarıyla aynı sonuç; iki `PARAM_SAVE`'de ikincisinin `ONE_PER_CALL`; kayıt `BUSY` iken arm reddi; kayıt sırasında güç kesintisi (kartta, 01 K19) |
| K85, 4.15 | S | `count`/`desc`/`find`/`get` tutarlılığı; 16 karakterlik ve 17 karakterlik adlar; bütün NULL ve sınır dışı girdilerde dönüş kodu ve çıktı nesnesinin değişmemesi |
| K86 | K, B | Açılış sayacının güç kesintili 100 tekrarda kesin artması (01 K15); SIL yeniden kurulumunda artış |
| K87, K88, K90, K91, K92 | S | Dönüş koduna göre `nav_valid`; durum alanları; günlük taşması sayacı; `det_tx` sayısı |
| K89 | S, K | Bozulmuş koruma sözcüğünde `core_ok = 0`; IMU kesilmesi, IMU reddi, `IMU_LAG` ve oturum kopmasında `core_ok = 1`; kartta 01 K8 senaryoları |
| K93, K94, K96 | S, K | Bitmeyen işte `bg.dropped`; kartta en hızlı Ping360 ile `bg_dropped` = 0 (01 K18) |
| K117, K08 | S | `now_w` = −1 ve 2⁶² + 1 ile arka plan çağrısı: dönüş `IDA_BG_BAD_INPUT`, iş durumu değişmez (sonraki geçerli arka plan çağrılarının dönüş dizisi ve step çıktıları, geçersiz çağrı hiç yapılmamış koşuyla bit düzeyinde aynı; `cnt.bg_bad_input` ve `health.bg_bad_input` hariç), `bg_slices` bu çağrıyı saymazken `cnt.bg_slice_mismatch` artmaz, sonraki step/tick'te `health.bg_bad_input = 1`, ondan sonrakinde 0; `c` NULL: `IDA_BG_BAD_INPUT` |
| K99, K100, K101 | S, B | Çağrıdan sonra girdi tamponunu bozup sonraki çağrının etkilenmediğini görme (adres temizleyicisiyle); 4.15'teki NULL durumları |

---

## 8. Açık kalanlar

| # | Konu | Etiket | Kapanacağı yer |
|---|---|---|---|
| O1 | Step, tick ve arka plan bütçeleri (3,5 / 0,5 / 0,3 ms) | Tahmin | Gerçek çekirdeğin karttaki DWT ölçümü (W3 sonu, M5); aşılırsa çekirdek iyileştirilir ya da mimar kararıyla bütçe değişir; bütçe değişirse `IDA_PORT_START_MAX_NS` ve `IDA_PORT_STALE_MARGIN_NS` birlikte değişir |
| O2 | Izgara aralığı payı (1 ms) ve dağıtım üst sınırı (0,5 ms) | Tahmin | Kart yazılımının ilk sürümüyle kartlı ölçüm (W3) |
| O3 | Kapasiteler (16 örnek, 8 komut, 8/4 algılama, 2 parti başlığı, 4 olay, 4 sonuç, 4 günlük, 32 yineleme penceresi) | Tahmin | A1'in periyot başına bayt bütçesi ve SIL senaryolarında gözlenen en büyük doluluklar (W4) |
| O4 | `T_imu_gap_tol` (`window`/10), `T_lag_set` (20 ms), `T_lag_clear` (10 ms), `N_lag_clear` (20), `T_det_stale` (2 s) varsayılanları; `T_imu_stale` aralığı 5–30 ms (W1-uyum U7) bağlayıcıdır, varsayılan 20 ms tahmindir | Tahmin | SIL arıza senaryoları ve kartlı K13 (W3/W4) |
| O5 | `IDA_EVT_RESULT_MARGIN_NS` (100 ms) | Tahmin | A1'in olay yineleme aralığı ve MP3 gidiş-dönüş dağılımıyla A1 sözleşmesinde; gerekirse bu sabit güncellenir |
| O6 | `valid_for_ns`, `apply_by_ns`, `hold_ns` | Kapandı (W1-uyum U7) | Aralıklar A1-K104'ündür; A4-K58, K66 ve başlık sabitleri aynı aralıkları kullanır |
| O7 | Ping360 kutu zamanı formülü | Kapandı | A1-K52 (kutu merkezi zamanı) |
| O8 | Algılama tiplerinin adları ve parti–kayıt ilişkisi | Kapandı (adlar); ilişki A8'in | `ida_det_record_t`, `ida_det_batch_t` gerçek A8 başlığıyla derlendi; parti atomikliği A4-K41 |
| O9 | Parametre kataloğu: kanonik şema metni ve tam parametre listesi | Açık; W3'ün | Çekirdek uygulama işi (W3). A5'in fizikten türetimi 1.0'dan çıkarıldı; A5 yalnız açık (kimlik, değer) çiftleri taşır |
| O10 | FreeRTOS yedeği (01 §2.4) devreye girerse A4-K20'nin "aynı örnekte eşzamanlı çağrı yok" kuralı, arka plan işinin ayrı görevde koşmasıyla çelişir | Koşullu | Yalnız yedek seçilirse ayrı bir sözleşme değişikliği işi; ilk sürüm RTOS'suzdur (iskelet §8) |
| O11 | GNSS yüksekliğinin datumu (ortalama deniz seviyesi) ile simdeki su yüzeyinin eşlenmesi | Mimari seçimi | A5 (orijin tanımı) ve sensör modelleri uygulaması |
| O12 | `ida_core_t` boyutu ve hizalama `_Static_assert` değerleri | Uygulama | Çekirdek uygulama işi (W3) |
| O13 | Sabit teslim gecikmesi A4-K37 ile ancak zaman çerçevesine göre ölçülebilir; zaman çerçevesinin kendisinin geride olması kartta ölçülemez | Bilinen sınır | A1'in gidiş-dönüş ve TIMESYNC ölçümü |
| O14 | Birleştirilen partilerin A1 yanıtı (eski partiye yanıt verilmemesi ve sim tarafında kaçırma sayılması) | A1'in kararı | A1 sözleşmesi |

---

## 9. Açık bulgu kapanış tablosu

Kaynak: `docs/mimari/acik-bulgular.md` "A4 — Ortak çekirdek ↔ çevresi" başlığı (26 bulgu). Bulgu metinleri değiştirilmedi. Düzeltme turu 1'de AB-060, AB-062, AB-064, AB-066, AB-078, AB-079, AB-080 ve AB-084'ün bağlandığı kurallar güncellendi.

| Bulgu | Önem | Durum | Sözleşmede nerede | Gerekçe |
|---|---|---|---|---|
| AB-059 | kritik | kapandı | A4-K89; H33 | İç sağlık (`core_ok`) ile veri kullanılabilirliği ayrıldı ve gözetimi yalnız iç sağlığın beslediği kural oldu. |
| AB-060 | kritik | kapandı | A4-K45, A4-K73, A4-K74, A4-K71, A4-K46, A4-K113 | Oturum sonu daraltıcı girdidir ve çağrıya hâkimdir: aynı çağrıda silahsızlandırma, atış kontrolün jeton ve izin iptali, `SAFETY_CLOSE`, aynı çağrıdaki her genişleticinin reddi ve bekleyen olaylara `SESSION_END`; mandallar oturum sonunda silinmez; daraltma reddedilen çağrıda da (b) adımında uygulanır. |
| AB-061 | önemli | kapandı | A4-K61, A4-K62, A4-K86 | Olay sayacının başlangıcı, sarmaması, son 16 değerin yalnız emniyet kapatmaya ayrılması ve açılış sayacının geçersizliğinde olaysız çalışma kural oldu; `ARM_CHANGE(0)` yerini `SAFETY_CLOSE` aldı. |
| AB-062 | kritik | kapandı | A4-K37, A4-K108; O13 | IMU ölçüm zamanı bağımsız zaman çerçevesine göre denetleniyor; `IMU_LAG` mandalının kurulma ve çözülme koşulları ve zorunlu emniyet tepkisi (genişletici yok, arm reddi, kontrol sürer) sözleşmede; yalnız zaman çerçevesinin kendisinin geride olması kart tarafında ölçülemez sınır olarak A1'e kaldı. |
| AB-063 | önemli | kapandı | A4-K83, A4-K82 | Parametre kaydı `BUSY` iken ve kayıtla aynı çağrıda arm/kayıt çakışması bağlayıcı kural oldu. |
| AB-064 | kritik | kapandı | A4-K26, A4-K28, A4-K45, A4-K57, A4-K113, A4-K114, Bölüm 3.11, 4.14 | Dönüş kodu tek değerlendirme sırasıyla ve birlikte oluşan koşullar için tek öncelik tablosuyla belirleniyor; hata dönüşlerinde sürekli komut ve genişletici yok (her kuraldan önce); daraltma reddedilen çağrıda da uygulanıyor ve komutlar `CALL_REJECTED` alıyor; her dönüş kodunun bütün çıktı alanları tek tabloda; geçersiz girdi zamanı çıktıya yazılmıyor; gönderme kararı A1'in. |
| AB-065 | önemli | kapandı | 3.6, 3.8, 4.14, 4.15; A4-K87–K92 | `plat` alanlarının tam listesi, durum, telemetri ve günlük yapıları ile her dönüş kodunun ve işlevin çıktı anlamı tanımlandı. |
| AB-066 | belirtilmemiş | kapandı | A4-K63, A4-K67, A4-K106, A4-K68 | Olayı çekirdek bir kez üretir; ilk kabul kapısı ile taşıma yinelemesi ayrı kurallar; yinelemenin sahibi platform ve A1; düşürülen ya da yanıtsız her olayın sonucu çekirdeğe döner. |
| AB-067 | belirtilmemiş | kapandı | 3.8 (`fs_level`, `is_safe`); A4-K55 | Çıktı arıza eylemi düzeyini ve güvenli sınıfı açıkça taşıyor. |
| AB-068 | belirtilmemiş | kapandı | A4-K18, A4-K39, 4.13 | IMU bayatlığından nötr çıktıya sınır, çağrının dönüşüne (kullanılabilir çıktıya) göre ve çağrı bütçesi dahil `T_imu_stale + 14,5 ms` olarak yazıldı. |
| AB-069 | önemli | kapandı | 3.2 (`window_ns`), 3.3; A4-K13 | Aralık sonu ve süresi ortak başlıkta; basınç ve batarya aralık ölçümü; Ping1D yansıma anı ve gönderim farkı ayrı alanda. |
| AB-070 | önemli | kapandı | A4-K38, A4-K103 | IMU aralık sürekliliğinin denetimi ve reddedilen IMU'da çağrının davranışı port kuralı oldu; boşluktaki filtre davranışı çekirdek uygulamasına (W3) bırakıldı. |
| AB-071 | önemli | kapandı | A4-K49, A4-K50, 3.4; A4-K26, 4.14 | Manuel eksenlerin çerçevesi ve kip başına anlamı, setpoint alanlarının çerçeveleri ve hata dönüşlerinde komutların yanıtı tanımlandı. |
| AB-072 | belirtilmemiş | uygulamaya devredildi | — | `Q_c`, `L`, `P₀` ve hizalama kabul ölçütü kestirim filtresinin içidir; ortak çekirdek kestirim uygulama işine (W3). |
| AB-073 | belirtilmemiş | uygulamaya devredildi | — | Manyetometre Jacobian'ı ve geçerlilik koşulları filtre içidir; W3 kestirim. |
| AB-074 | belirtilmemiş | uygulamaya devredildi | 3.3 (`eph`, `epv` 1σ) | `eph`'in anlamı (1σ, m) portta sabitlendi; Jacobian'lar filtre içidir, W3 kestirim. |
| AB-075 | belirtilmemiş | uygulamaya devredildi | 1.3 | `ida_fc_*` yapıları çekirdeğin iki iç modülü arasındadır, port değildir; atış kontrol uygulama işi (W4) ile çekirdek iskeleti (W3). Portta görünen yüzey (komut, mandal, olay, sonuç, geri okuma, algılama, arka plan) bu sözleşmede; çapraz uyum turunda bu yüzey kapalı listelere bağlandı (`IDA_FC_SELECT`, `IDA_MI_TARGET_ALIGN`, `fc_ref`, `ext[]` kaldırıldı; A4-K90, K91, K119, K120). |
| AB-076 | belirtilmemiş | kapandı | 3.12; O12 | Port kapasiteleri sabitlendi; port yapıları için boyut iddiası yapılmıyor (A4-K06); `ida_core_t` boyut denetimi A4-K98 ile çalışma zamanında. |
| AB-077 | belirtilmemiş | uygulamaya devredildi | 3.10; A4-K79, A4-K80 | Kodlanmış bloğun bayt düzeni ve doğrulaması bu sözleşmede kapandı; parametre tablosunun tam listesi ve sınıfları çekirdek uygulama işine (W3) ve A5'e. |
| AB-078 | kritik | kapandı | A4-K46, A4-K105, A4-K47, A4-K102 | Daraltıcılar kuyruğa değil tür başına tek bitlik mandala girer; sonlu tamponla hiçbiri düşmez ve en çok bir sonraki çağrıda çekirdeğe ulaşır; daraltıcı olmayan komutların erteleme ve atma politikası ayrı; çekirdeğin iç kapasitesi çağrı kapasitesinden az olamaz. |
| AB-079 | önemli | kapandı | A4-K60–K68, A4-K106, A4-K93, A4-K109 | Olay alanları, sonuç dönüşü, arka plan imzası, arka plan işinin yeniden oynatılması ve olay üretiminin sahipliği port düzeyinde tekleşti. |
| AB-080 | belirtilmemiş | kapandı | A4-K45, A4-K64, A4-K73, A4-K103 | IMU'suz ve IMU'su reddedilen çağrı da daraltıcıları atış kontrolden önce uygular ve emniyet kapatma olayı üretebilir; genişletici üretemez; iç çağrı biçimi W4'ün. |
| AB-081 | belirtilmemiş | kapandı | 3.3 (`ida_safety_t`); A4-K34, A4-K111, A4-K115, A4-K116 | Tüp başına fırlatıcı izni geri okuma alanında; eşit ölçüm zamanlı değişim örneği reddedilmeden değer güncelliyor; eşit anahtarlı çelişkili örnekler sıradan bağımsız reddediliyor ve daraltıcı içerikleri uygulanıyor; emniyet girdisinin kapanması daraltıcı. A1'den ilgili alanlar bekleniyor (Bölüm 10). |
| AB-082 | belirtilmemiş | kapandı | A4-K75 | "Bakım" girdisi yok (iskelet §10, öneri 8); "kart değiştiremez" yalnız sanal girdilere ait, fırlatıcı izni olaylarla değişiyor. |
| AB-083 | önemli | kapandı | A4-K61, A4-K62, A4-K86 | Açılış sayacının platformdan beklenen güvencesi (kesin artan ya da geçersiz) ve olay sayacının sarmaması kural oldu; flash'taki atomik artırımın gerçeklemesi kart yazılımı uygulamasınındır. |
| AB-084 | belirtilmemiş | kapandı | 3.3, 3.5, 3.8; A4-K61, A4-K66, A4-K107 | Emniyet ve tüp örneği, olay sonuç yolu, son geçerlilik ve iznin iki saatteki süresi tanımlandı; kimlik açılış sayacından kuruluyor. |

Özet: 26 bulgudan 21'i kapandı, 5'i uygulamaya devredildi; kapsam dışı ve açık bulgu yok. Kritik bulguların (AB-059, AB-060, AB-062, AB-064, AB-078) hepsi kapandı.

**Bölümler arası uyumsuzluk tablosunda A4'e değinen satırlar** (bilgi; tablo sahibi mimar):

| U | Bu sözleşmedeki karşılığı |
|---|---|
| U-01 | Olay alanları, kimliği, ilk kabul ve yineleme A4-K60–K68, A4-K106; A1 tarafı A1'de |
| U-02 | Tek girdi yapısı, zaman çerçevesi, IMU'suz tick, birleştirilmiş parti (A4-K15–K17, A4-K102); eski `ida_core_tick_wall` yok |
| U-03 | `event_id` açılış sayacından (A4-K61, A4-K86) |
| U-04 | `window_ns` ve tür başına `t_meas` (A4-K13) |
| U-07 | `core_ok` ve diğer sağlık bitleri (A4-K89) |
| U-08 | Komut kapasitesi, daraltıcı mandalları ve olay sahipliği (A4-K46, A4-K47, A4-K63, A4-K105) |
| U-10 | `ida_safety_t`; bakım girdisi yok (A4-K75); emniyet girdisinin kapanması daraltıcı (A4-K111) |
| U-13 | Montajlar gövde referans noktasına göre (A4-K42); tahsis matrisinin referansı çekirdek uygulamasında aynı noktaya bağlanır |
| U-14 | IMU ivmesi montaj noktasında; dönme kolu çekirdekte (A4-K42) |
| U-15 | DVL kovaryansı DVL çerçevesinde, `eph` 1σ (3.3) |
| U-16 | Çıktı normalleştirilmiş ESC komutu, eğri tersi çekirdekte (A4-K54); eğrinin aynı A5 sürümünden okunması A5'ten beklenir |

---

## 10. Başka sözleşmelerden beklenenler

### A1'den beklenen

1. Ek A'daki bütün alanların A1 mesajlarına eşlenmesi; zaman çerçevesine `t_rx` (TF'nin alım zamanı) dahil.
2. Emniyet geri okuma mesajında tüp başına `PERMIT_OPEN` biti ile `PLUG_IN`, `PIN_OUT`, `LOADED` ve `estop` (A4-K75, A4-K111; AB-081).
3. Olay mesajında `kind` (üç tür), `tube`, `seq`, `t_decide`, `apply_by_ns`, `hold_ns`; olay onayında 3.5'teki alıcı kodları (`fc_ref` kaldırıldı; W1-uyum U1).
4. **Alıcıda iznin süresi:** sim sunucusu `SAFETY_OPEN`'in iznini, uygulandığı andan itibaren **sim zamanında** `hold_ns` sonra yeni bir `SAFETY_OPEN` uygulanmamışsa kapatır; bariyer beklemesinde sim zamanı durduğu için süre uzamaz (A4-K66).
5. Süre aralıkları (karşılandı): `valid_for` 60–450 ms, `apply_by` 16–1000 ms, `hold` 60–450 ms A1-K104'ündür; A4 aynı aralıkları kullanır.
6. Ping360 menzil kutusu zamanı (karşılandı; A1-K52).
7. Hangi dönüş kodunda hangi yanıtın gönderildiği (A4-K29): `OLD_SEQ`, hata dönüşlerinde `thr_valid = 0` olan çıktının `CMD_VALID = 0` ile gönderilmesi, `IMU_REJECTED`'in olağan yanıt olması, ızgara tick'inin yalnız `is_safe = 1` iken gönderilmesi.
8. **Parti gelişi:** partiler karta nominal olarak bir kontrol periyodu (5 ms) arayla kapanır; A1, **alıcıda** art arda kapanan partiler için bir üst sınır tanımlar (gönderici tarafı E11 ile var); A4-K19'un 4 ms güvencesi ve A4-K102'nin birleştirmesi bu varsayımla hesaplandı.
9. **Birleştirilmiş partilerin yanıtı:** birleştirmede eski partiye ayrı yanıt verilmemesinin A1 tarafındaki anlamı (sim için kaçırılmış parti) ve sayımı (O14).
10. GCS MAVLink çerçevelerinden `ida_cmd_t`'ye ve daraltıcılar için mandal bitlerine çeviri; `cmd_id`'nin kurulması; mandalda birleştirilen eski daraltıcı komutların (`coalesced`) yer istasyonuna nasıl yanıtlanacağı (öneri: `ACCEPTED` / `IDA_REASON_COALESCED`); MAVLink parametre protokolünün `ida_core_param_*` işlevleriyle ve komut kimliğiyle çoklanmış sonuçlarla karşılanması; GNSS fix kodlarının 3.3'teki değerlere çevrilmesi.
11. `plat` sayaçlarının kaynağı olan parti ve oturum olaylarının (GAP, eksik parti, birleştirme, eski oturum) tanımı.
12. Olay yuvasının ömrü (A4-K106, U12c): yineleme `apply_by`'da durur, yuva `T_sonuç_pay` (95 ms) dolana kadar açık kalır, `EXPIRED_NO_ACK` ancak sonra verilir; olay tablosu `N_olay` = 8, bir yuva `SAFETY_CLOSE`'a ayrılmış; izin açan olay kovası boşsa `DROPPED_NO_SLOT`; sekiz yuva doluyken gelen `SAFETY_CLOSE` için yeni kapanış nedeni `EVENT_OVERFLOW` = 7 (A4-K67).
13. **Yolda bekleyen genişletici (U4b):** platform bir daraltıcı komutu mandala aldığında ertelenmiş kuyruğundaki bütün genişletici komutları atar ve her biri için ret sonucu üretir (A4-K105); tel biçimi değişmez.
14. **Alım damgası (P2-20):** A1-K48 damgasının gerçek alım anından en büyük sapmasının sayıyla verilmesi; A4-K14, K39 ve K118'in süreleri bu damgadan sayılır.
15. **Çağrı türü:** A1'in çağrı izi kodlamasında oturum sonu çağrısının (A4-K122) ızgara tick'inden ayrılabilmesi.
16. **Kısıtlayıcı emniyet örneği (U5b):** eski ya da yinelenen partinin CRC'si ve oturumu geçerli `IDA_SAFETY` üyesinin atılmaması ve sıradaki çekirdek çağrısına verilmesi; emniyet örneklerinin bir çağrının örnekleri arasında önce yerleştirilmesi ve kapasiteden düşürülmemesi (A4-K116 bu örneklerin kısıtlayıcı içeriğini uygular).
17. **Kapalı tablolar (U1):** `IDA_LOG.code`, `reason` ve `inhibit_mask` açıklamalarının A4 3.15, 3.16 ve A4-K120'ye atıf vermesi; FC `arg0`/`arg1`'in 0 olması ve `tube` = 255'in yalnız `FC_DISARM`/`FC_ABORT`'ta kullanılması.

### A5'ten beklenen

1. İtici sırası (`thr_cmd` indisi), ESC eşlemesinin üç parametresi (A4-K54) ve itki eğrisinin fizik çekirdeğiyle aynı sürümden okunması (U-16).
2. Sensör montajlarının gövde referans noktasına göre konum ve yönelimi (A4-K42); otopilotun inandığı montajların fizik gerçeğinden ayrı tutulması.
3. Otopilot parametrelerinin yalnız açık (kimlik, değer) çiftleri olarak taşınması (P3-14); parametre kataloğu W3'ündür (O9) ve fizikten türetim 1.0'da yoktur; SIL koşucusu bloğu (3.10) bu çiftlerden üretir.
4. Senaryo orijini ve GNSS yükseklik datumu (O11).
5. Görev öğelerinin A1 Tablo 3.6a'nın alanlarını bire bir taşıması (U1; `TARGET_ALIGN` yok).

### A6'dan beklenen

1. Tekrar oynatma için init girdisinin (parametre bloğu dahil) ve her çağrının tam girdisinin (Ping360 satırı, algılama kayıtları ve partileri, mandal alanı dahil) alan alan kaydı (A4-K03, A4-K24); çağrı türü ve dönüş kodu.
2. **Arka plan çağrı dizisi (U10):** her step ve tick çağrısının `bg_slices` değeri ve arka plan çağrılarının kendisi kaydedilir; tekrarda arka plan çağrıları yalnız kayıttaki diziden yürütülür ve `bg_slices` yalnız tutarlılık denetimidir (A4-K109); SIL'de `N_bg` üst veride `sil_n_bg` (u32) olarak saklanır (A4-K95); arka plan işi düşmeleri (`bg.dropped`) karşılaştırma için kaydedilir (A4-K94).
3. **Birlik kodlaması (P4-1):** birlikler ve koşullu alanlar 3.14'teki seçiciye göre yalnız etkin üyeyle yazılır; kaldırılan alanlar (`fc_ref`, `ext`) kayda girmez; `PARAM_SAVE` sonucunun blobu `len` (u32) + `len` bayt olarak kodlanır.

### A8'den beklenen

1. `ida_det_record_t` ve `ida_det_batch_t` (karşılandı; gerçek başlıkla derlendi).
2. Parti doğrulamasının kart satırları ve başlık–kayıt eşlemesi (A8-K27, K29); A4-K41 partiyi atomik kabul eder. Akış ve neden başına ret sayaçları çekirdekte yoktur (tek toplam `det_rejected`); ayrıntılı sayaçlar bileşende ve sim'in kabul kapısındadır (W1-uyum, A8 P2-15). Bayatlık eşiği A4-K118'dedir.
3. Kaydın ve parti başlığının alım zamanı taşımaması ya da taşırsa portun `det_t_rx`/`det_batch_t_rx`'iyle aynı anlamda olması.

---

## Ek A. A1 için dışa açılan adlar

A1 sözleşmesi bu adları kullanır. Tip ve anlamlar Bölüm 3'tedir. Düzeltme turu 1'de değişenler: `ida_cmd_ack_t`, `out.cmd_acks`, `out.param_value`, `out.param_save` ve `out.regen` kaldırıldı; `ida_cmd_result_t` / `out.results`, `ida_narrow_t` / `in.narrow`, `in.det_batch`, `in.bg_slices`, yeni `plat` ve `health` alanları ve `IDA_RC_IMU_REJECTED` eklendi; algılama tipleri `ida_det_record_t` ve `ida_det_batch_t` oldu. Düzeltme turu 2'de eklenenler: `IDA_BG_BAD_INPUT`, `IDA_NARROW_DEFINED`, `health.sample_conflict`, `health.bg_bad_input`, `cnt.sample_conflict`, `cnt.sample_dup_dropped`, `cnt.narrow_from_rejected`, `cnt.bg_bad_input`; dönüş kodunun değerlendirme sırası (3.11).

| Port yapısı | Alanlar |
|---|---|
| `ida_time_frame_t` | `present`, `seq`, `t`, `t_rx` |
| `ida_hdr_t` | `t_meas`, `t_rx`, `seq`, `window_ns`, `flags` (`IDA_HF_VALID`, `IDA_HF_SATURATED`, `IDA_HF_SENSOR_FAULT`), `type` (`IDA_SENS_*`), `inst` |
| `ida_imu_t` | `gyro[3]`, `accel[3]`, `temp_c` |
| `ida_mag_t` | `field[3]` |
| `ida_baro_t` | `p_abs`, `temp_c` |
| `ida_gnss_t` | `lat_e7`, `lon_e7`, `alt_mm`, `vel_ned[3]`, `eph`, `epv`, `fix_type` (`IDA_GNSS_FIX_*`), `n_sv` |
| `ida_dvl_t` | `vel[3]`, `cov[6]`, `fom`, `beam_dist[4]`, `altitude`, `dvl_flags` (`IDA_DVL_*`), `beam_valid` |
| `ida_ping1d_t` | `distance`, `scan_start`, `scan_length`, `ping_number`, `tx_offset_ns`, `confidence` |
| `ida_batt_t` | `voltage`, `current`, `temp_c` |
| `ida_leak_t` | `mask` |
| `ida_safety_t` | `estop`, `tube[2]` (`IDA_TUBE_LOADED`, `IDA_TUBE_PLUG_IN`, `IDA_TUBE_PIN_OUT`, `IDA_TUBE_PERMIT_OPEN`) |
| `ida_ping360_t` | `hdr`, `angle_grad`, `n_samples`, `sample_period`, `tx_duration_us`, `tx_freq_khz`, `gain`, `data[1200]` |
| `ida_det_record_t`, `ida_det_batch_t` (A8) | `in.det[]`, `in.det_t_rx[]`, `in.det_batch[]`, `in.det_batch_t_rx[]`, `out.det_tx[]` |
| `ida_cmd_t` | `cmd_id`, `type` (`IDA_CMD_*`), `source`, `u.set_mode.mode`, `u.manual.axes[6]`, `u.guided` (`mask`, `pos_ne[2]`, `depth`, `altitude`, `vel_ned[3]`, `yaw`, `yaw_rate`), `u.mission_begin.count`, `u.mission_item` (`index`, `item_type`, `frame`, `p[4]`, `lat_e7`, `lon_e7`, `north_m`, `east_m`, `depth`), `u.mission_ctl.action`, `u.param` (`id`, `type`, `v`), `u.fc` (`op`, `tube`, `arg0`, `arg1`) |
| `ida_narrow_t` | `mask` (`IDA_NARROW_DISARM`, `_MODE`, `_FC_DISARM`, `_FC_ABORT`), `mode`, `cmd_id[4]`, `coalesced[4]` |
| `ida_event_result_t` | `event_id`, `result` (`IDA_EVR_*`), `t_apply` |
| `ida_plat_t` | `link_state`, `link_event`, `param_store`, `param_fail`, `param_gen`, `prev_overrun`, `tx_stalled`, `batch_merged`, `input_dropped`, `prev_call_ns`, `cnt_batch_skipped`, `cnt_batch_incomplete`, `cnt_batch_old`, `cnt_batch_merged`, `cnt_stale_session`, `cnt_member_overflow`, `cnt_cmd_deferred`, `cnt_cmd_dropped`, `cnt_narrow_coalesced`, `cnt_grid_missed`, `cnt_rx_error`, `cnt_out_fixed` |
| `ida_in_t` (bunlara ek) | `bg_slices` |
| `ida_out_t` (sürekli komut) | `thr_valid`, `is_safe`, `armed`, `src`, `fs_level`, `seq`, `t_ref`, `valid_for_ns`, `thr_cmd[8]` |
| `ida_event_t` | `event_id`, `kind` (`IDA_EVT_*`), `tube`, `seq`, `t_decide`, `apply_by_ns`, `hold_ns` |
| `ida_cmd_result_t` | `cmd_id`, `kind` (`IDA_RES_*`), `ack` (`IDA_ACK_*`), `reason` (`IDA_REASON_*`), `value`, `save` (`gen`, `blob`, `len`) |
| `ida_nav_out_t` | `t`, `p_ned[3]`, `v_ned[3]`, `q_nb[4]`, `omega_b[3]`, `depth`, `altitude`, `p_sigma[3]`, `v_sigma[3]`, `att_sigma[3]`, `origin_lat_e7`, `origin_lon_e7`, `est_level`, `reset_counter` |
| `ida_status_t` | `mode`, `armed`, `fs_level`, `fs_reasons`, `regime`, `phase`, `mission_state`, `mission_item`, `param_ok`, `events_ok`, `boot_count`, `cnt.*` |
| `ida_health_t` | `core_ok`, `data_ok`, `imu_rejected`, `imu_lag`, `input_dropped`, `narrowed`, `sample_conflict`, `bg_bad_input` |
| `ida_log_t` | `t_w`, `code`, `severity`, `arg0`, `arg1` |
| `ida_fc_status_t` | `state` (`IDA_FCS_*`), `inhibit_mask` (`IDA_FCI_*`), `last_event_id`, `last_event_result` |
| `ida_log_t.code` | `IDA_LOG_*` (3.15) |
| `ida_cmd_result_t.reason` | `IDA_REASON_*` 0–15; aralıklar A4-K120 |
| `ida_init_in_t` (açılış kaydı için) | `boot_count`, `boot_count_ok`, `reset_cause`, `reset_flags`, `consecutive_faults`, `param_src` |
| `ida_core_info_t` (el sıkışma için) | `port_major`, `port_minor`, `param_schema_hash`, `core_version` |
| Dönüş kodları | `IDA_RC_OK`, `IDA_RC_IMU_REJECTED`, `IDA_RC_DUP_SEQ`, `IDA_RC_OLD_SEQ`, `IDA_RC_BAD_INPUT`, `IDA_RC_NOT_INIT`; arka plan: `IDA_BG_IDLE`, `IDA_BG_MORE`, `IDA_BG_BAD_INPUT` (yalnız ilk ikisi `bg_slices`'a sayılır) |
| İşlevler | `ida_core_step`, `ida_core_tick`, `ida_core_background`, `ida_core_param_count`, `ida_core_param_desc`, `ida_core_param_find`, `ida_core_param_get` |
