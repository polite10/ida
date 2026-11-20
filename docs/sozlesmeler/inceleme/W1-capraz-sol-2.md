# sol incelemesi: sozlesmeler

## Hüküm

**Bu adaylar birlikte dondurulamaz.** A1 ↔ A4 ve A4 ↔ A8 arasında geçerli girdinin reddedilmesine, bilgi kaybına ve farklı emniyet davranışlarına yol açan çelişkiler var.

Güncel 22 sayaç, sekiz sağlık biti ve ortak enum değerleri eşleşiyor. Gerçek A8 başlığıyla C17/C++20 sözdizimi ve yapı yerleşimi denetimleri geçti. A8’in ikinci incelemesindeki dört bulgu güncel metinde kapanmış.

İnceleme belirtilen dar kapsamda yapıldı. Hiçbir dosya değiştirilmedi, kurulum yapılmadı.

## Düzeltilmesi gerekenler

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|
| 1 | `A1-kart-baglantisi.md` §3.6, §3.7, §8.2 A4 md.5/9 ↔ `A4-port.md` §3.4, §3.8, K91/K104; `core/include/ida/port.h` | A4 hâlâ `IDA_FC_SELECT=4`, `IDA_MI_TARGET_ALIGN=8`, anlamı uygulamaya bırakılmış görev `p[4]` ve FC `arg0/arg1`, olay `fc_ref` ve durum `ext_len/ext[64]` alanlarını içeriyor. Bunlar A1’in kapalı listesinin dışında; bazıları doğrudan hedef seçimi, diğerleri kapsam dışı içeriği taşıyabilecek opak kanallar. | A4’ü A1’in kapalı işlem/görev listesine bağla. Kapsam dışı türleri kaldır; FC argümanlarını bu sürümde sıfırla sınırla; `fc_ref` ve `ext[]` için A1’in istediği kaldırma/ayırma kararını uygula. | kritik |
| 2 | A1 §3.6 `CMD_SIMPLE`, K64 ↔ A4 §3.4, K46, §3.11(c) | A1 `SET_MODE.arg=0..7` kabul ediyor ve 0/1’i `NARROW_MODE` mandalına koyuyor. A4 mandalda yalnız 2–5’i kabul ediyor. Örneğin `SET_MODE(arg=1)` A1’den geçip bütün çağrıyı `BAD_INPUT` yapıyor. | A1’in komut aralığını A4’ün kabul ettiği hedeflerle eşitle; 0/1’i dönüştürücüde `LINK_BAD_FIELD` ile reddet. | kritik |
| 3 | A1 §3.6 `CMD_FC.tube`, K63 ↔ A4 K46; `port.h` `ida_narrow_t` | Daraltıcı `FC_DISARM/FC_ABORT` komutunda `tube` 0–1 anlamlı görünüyor; mandalda tüp alanı yok. Yalnız `tube` bakımından farklı iki komut aynı port girdisine dönüşüyor. `source` da mandalda taşınmıyor; bunun bilgi amaçlı kaybı ayrıca açıklanmamış. | Daraltıcı işlemlerin tüp kapsamını iki sözleşmede açıkça tekleştir. Tüp seçimi anlamlıysa mandal bunu kayıpsız temsil etmeli; bütün tüplere uygulanıyorsa A1 alanı bunu açıkça söylemeli. | kritik |
| 4 | A1 §3.7 `ACTUATOR.batch_seq`, K45/K72 ↔ A4 K57, §4.14 | A1 hata dönüşünü de mevcut partinin yanıtı olarak göndermek istiyor, fakat `batch_seq←out.seq`. A4 hata dönüşünde `out.seq` olarak son kabul edilen TF’yi veriyor. Son kabul edilen 10, reddedilen yeni parti 11 ise yanıt 10’a gider; 11 yanıtsız kalır. `NOT_INIT`’te 0 kullanılması da aynı sorunu oluşturuyor. | A1’de yanıtlanan parti kimliğini çağrının girdisinden al; A4’ün güvenli `out.t_ref/out.seq` geri dönüş anlamını ayrı tut. Olayların kendi `seq/t_decide` eşlemesini değiştirme. | kritik |
| 5 | A1 K121(iii), §8.2 A4 md.11 ↔ A4 K95 | A1 her step **ve tick** öncesinde tam `N_bg` çağrı istiyor; A4 canlı SIL’de step öncesinde `IDLE` dönene kadar çağırmayı emrediyor. `N_bg=0` ve bekleyen iş karşı örnektir. K94 nedeniyle sonuçların tüketilmesi/düşmesi de değişebilir. | A4-K95’i sabit `N_bg` takvimine bağla; step, tick ve oturum sonu çağrısının kapsamını açıkça belirt. | kritik |
| 6 | A1 K117, §8.2 A4 md.10 ↔ A4 K18/K19/K23 | A1 beklenen TF çevresinde tick başlatmayı yasaklıyor; A4 koşulsuz 10 ms mutlak ızgara istiyor. Izgara anı beklenen TF’ye denk geldiğinde iki kurala birlikte uyulamıyor. A1’in 9,733 ms hesabı da önceki çağrının bulunmamasına bağlı. | A4’te TF akarken A1-K117’nin takvimini kullan; 11 ms başlangıç aralığı güvencesini bu takvimle birlikte koru. | kritik |
| 7 | A1 K107, §8.2 A4 md.6 ↔ A4 K11, §2 “Izgara tick’i”; `port.h` `tf.present` açıklaması | A1 SIL’de `tf.present=0` oturum sonu tick’i istiyor. A4, bu biçimi ızgara tick’i diye tanımlıyor, SIL’de yasaklıyor ve her çağrının saatini o çağrının TF’sine bağlıyor. Oturum sonu için istisna yazılmamış. | A4’te oturum sonu çağrısını ızgara tick’inden ayır; `now_w=son kabul edilen TF zamanı` istisnasını ve çağrı türü kodlamasını belirt. | önemli |
| 8 | A1 K80, E8/E16 ↔ A4 K106/K68/K70 | A4-K106 son geçerlilik yanıtsız dolunca hemen `EXPIRED_NO_ACK` döndürülmesini istiyor. A1 yinelemeyi o anda durduruyor fakat son sonuç için **95 ms daha** bekliyor. Aynı olay için çekirdeğe farklı zamanda, hatta farklı sonuç döner. | A4’te yinelemeyi durdurma, açık olay yuvasını tutma ve son sonuç üretme anlarını ayrı tanımla; A1’in 95 ms payıyla tekleştir. | kritik |
| 9 | A1 K120/K80 ↔ A4 K67; §3.5 `DROPPED_NO_SLOT` | A1 izin açan olay için kova boşluğunu `DROPPED_NO_SLOT` sayıyor; A4 bu nedeni yalnız tablo doluluğuna bağlıyor. Ayrıca A1 kova boşken `SAFETY_CLOSE`’u tabloda bekletiyor, A4 ise tablo doluysa onu da düşürüyor. Sekiz açık olay varken yeni kapatma olayının korunacağı yol tanımlı değil. | A4’ün kapısını kova kuralıyla eşitle. Kapatma olayında yer bulunamamasını da açıkça çöz; örneğin oturumun kapatılmasıyla alıcıdaki izinlerin düşmesini zorunlu kılan ortak bir hata yolu tanımla. | kritik |
| 10 | A1 §3.9, Tablo 4.6 `EVENT`, §8.2 A4 md.3 ↔ A4 K66, `port.h` süre sabitleri | A4 `apply_by` ve `hold` için 1–1000 ms’ye izin veriyor; A1 sırasıyla 16–1000 ve 60–450 ms kabul ediyor. A4’e uygun `hold=1000 ms` olayı A1’de bütünüyle reddediliyor. | Ortak çalışma profilinin daha dar aralıklarını A4 parametre doğrulamasına bağla. `valid_for` için K58’in A1’e eşitleme kuralı zaten var; aynı bağ olay sürelerinde de gerekli. | kritik |
| 11 | A1 E10/K104 ↔ A4 K39/O4 | E10 yalnız varsayılan `T_imu_stale=20 ms` ile hesaplanmış; A4 bunu değiştirilebilir parametre yapıyor. A1-K104 bu parametreyi denetlemiyor. `T_imu_stale=50 ms`, `valid_for=60 ms` için `50+14,5=64,5>60`; “izin verilen her yapılandırma” iddiası sağlanmıyor. | E10’u gerçek çekirdek parametresiyle ön denetle veya ortak profilde bayatlık parametresini sınırla. | kritik |
| 12 | A4 §6 ↔ A8 K64/K67; A1 K58 | A4 kamera kapalıyken `n_det_batch=0` diyor. A8 ve A1 ise 1 Hz `NO_FRAME/CAMERA_OFF` başlığının çekirdeğe verilmesini istiyor. | A4 §6’yı düzelt: kayıt sayısı sıfırdır; durum başlığı gelen çağrıda parti başlığı sayısı sıfır değildir. | kritik |
| 13 | A1 K102/K121 ↔ A8 K60/K61, §6.3 | A1 aynı senaryoyla bütün SIL çerçevelerinin bayt eşitliğini garanti ediyor. A8 kamerayı beklemiyor ve hızlandırılmış SIL’de sonuçların bilgisayar zamanlamasına bağlı olduğunu açıkça söylüyor. Bir koşuda `OK`, diğerinde `LATE` başlığı üretilebilir; A1 kilitli adımı bunu önlemiyor. | A1’in eşitlik güvencesini kamera kapalı veya aynı kaydedilmiş algılama girdisinin kullanıldığı koşularla sınırla. | kritik |
| 14 | A4 K41, §3.7 “tek tek reddedilir” ↔ A8 K27/K29 | A8 bir kayıt bozuksa **bütün partiyi** reddediyor. A4 ise her kaydı/başlığı denetleyip geçeni kopyalamayı söylüyor. İki kayıtlı partide bir kayıt geçerliyse A4’ün doğrudan okuması kısmi kabul, A8’in kuralı sıfır kabul verir. | A4-K41’i açıkça parti atomikliğiyle yaz; bütün kayıtlar doğrulanmadan hiçbirini kabul etme. A8 atfını bu davranışla tamamla. | kritik |
| 15 | A8 K30, §8.2 A4 md.2 ↔ A4 K41/K88; `ida_core_counters_t`; A1 `CORE_COUNTERS` | A8 akış ve ret nedeni başına sayaçların A4 telemetrisinde bulunmasını bekliyor. A4/A1 yalnız tek toplam `det_rejected` alanı sunuyor. Örneğin iki akıştaki `CALIB` ve `TIME` retleri ayırt edilemiyor. | A4’te neden/akış sayaçlarının dışa açılmasını tanımla ve A1 taşımasını eşle veya A8’in beklentisini açıkça daralt. Mevcut 22 alanın birbirine eşlenmesi doğru, fakat bu beklentiyi karşılamıyor. | önemli |
| 16 | A8 §8.2 A4 md.3, O8 ↔ A4 K36/K39/K41 | A8 başlık gelmeyen algılama akışının ne zaman bayat sayılacağını A4’ten bekliyor. A4 yalnız IMU için eşik ve tepki tanımlıyor; algılama akışı için süre yok. | Algılama akışının bayatlık eşiğini ve yalnız arayüz düzeyindeki durum bildirimini tanımla; bu beklentiyi kapanmış sayma. | önemli |
| 17 | A8 §3.3, K27 `TIME` ↔ A4 K08, §3.1 | A8 zamanlarda yalnız negatifliği reddediyor; A4 üst sınırı `2⁶² ns`. Örneğin boş sonar başlığında `t_meas=2⁶²+1`, `window=0` A8’in zaman denetiminden geçer. Ayrıca K13’ün `başlık+1 s` ifadesi `INT64_MAX` yakınında taşmaya karşı tanımlı değil. | A8 zaman aralıklarını A4’ün üst sınırına bağla; sınır aritmetiğinin taşmadan değerlendirilmesini belirt. | önemli |
| 18 | `protocol/ida.xml` `IDA_MODE/IDA_ACK/IDA_REASON/...` ↔ `port.h` aynı adlı makrolar | **41 enum girdisi makrolarla çakışıyor.** `port.h` ardından üretecin `typedef enum IDA_MODE { IDA_MODE_INIT=0,... }` bildirimi gelince makro `0u`ya açılıyor ve derleme hata veriyor. Minimal karşı örnek Clang ile doğrulandı. | Tel enum adlarını ayrı önekle ayır veya zorunlu bir ortak başlık/kapsülleme düzeni tanımla. İki başlığın birlikte içerilmesini sınayan derleme denetimi ekle. Üretecin davranışı [pymavlink 2.4.50 kaynağında](https://raw.githubusercontent.com/ArduPilot/pymavlink/v2.4.50/generator/mavgen_c.py) doğrulandı. | önemli |
| 19 | A1 §8.2 A4 md.1; XML `IDA_REASON` ↔ A4 §3.8; `port.h` `IDA_REASON_*` | A1 bağlantı nedenleri için 128–191 aralığını ayırıyor; A4 bu ayırmayı kaydetmiyor ve yalnız kendi 0–15 kodlarıyla 256 üstü alanı açıklıyor. Güncel ortak 0–15 değerlerinde uyuşmazlık yok. | A4’e 128–191’in bağlantı katmanına ayrıldığını yaz; 128–131’in sahibini ve port sonucu olmadığını açıkla. | önemli |
| 20 | A1 K48 ↔ A4 K14 | A4 alım damgasını son baytın kesmede okunan zamanı diye tanımlıyor. A1 sonraki alım olayının damgasına, bulunamazsa önceki olayın damgasına izin veriyor. Bunlar aynı zaman değildir; A4’ün tazelik sürelerinin başlangıcı değişebilir. | A4’ü uygulanabilir A1 damgalama kuralına bağla; kaba damganın anlamını ve 34,5 ms sınırının hangi damgadan sayıldığını açıkça yaz. | önemli |
| 21 | A1 §3.7 `IDA_LOG`; XML 46066 ↔ A4 K90, §3.8 `ida_log_t` | `code` tablosu ve iki genel `arg0/arg1` alanının anlamı sonraki uygulamaya bırakılmış. Bu açık tanı kanalı üzerinden kapsam dışı hedefleme/atış ölçütü içeriğinin taşınamayacağı doğrulanamıyor. | İzin verilen tanı kodlarını ve argüman anlamlarını kapalı biçimde tanımla; kapsam dışı içeriği bu kanala bırakma. | önemli |

## Yeniden hesaplananlar

**Derleme ve makinece okunur tanımlar**

Gerçek `ida/algilama_kaydi.h` içerilerek Apple Clang 17 ile aşağıdaki denetimler başarılı oldu:

- C17: `-std=c17 -pedantic-errors -Wall -Wextra -fsyntax-only -I core/include`.
- C++20: aynı denetimin `-std=c++20` karşılığı.
- `ida_det_record_t` ve `struct ida_det_record`: **80 B**, hizalama **8 B**.
- `ida_det_batch_t` ve `struct ida_det_batch`: **32 B**, hizalama **8 B**.
- Başlıktaki bütün mevcut `offsetof` denetimleri geçti; A8 belge yerleşimiyle eşleşiyor.

İlk kabuk denemesi geçici dosya oluşturma kısıtına takıldı; gerçek derleyiciye stdin borusuyla verilen tekrarlar başarıyla tamamlandı. GNU/MSVC üretim derlemesi ve kart çalışması yapılmadı. `pymavlink` yerelde bulunmadığından tam MAVLink başlık üretimi yeniden çalıştırılmadı; kurulum yapılmadı.

XML bağımsız olarak ayrıştırıldı:

| Denetim | Sonuç |
|---|---|
| Mesaj / alan / enum girdisi | **37 / 334 / 186** |
| Kanonik satır toplamı | **557** |
| `dialect_hash` | **`0x104154A1E68773C8`**, belgeyle aynı |
| Ortak enum girdileri | **130 değer eşit** |
| Altın çerçeveler | **38/38 CRC ve boy denetimi geçti** |
| CRC-16 denetim girdisi `"123456789"` | **`0x6F91`** |

MAVLink alan sıralaması, little-endian kodlama, kırpma, 255 B yük sınırı ve imzasız 12 B ek yük canlı [resmî serileştirme belgesiyle](https://mavlink.io/en/guide/serialization.html) denetlendi.

**A1 ↔ port alan eşlemesi**

Aşağıdaki tablolarda `u8/u16/u32/u64` sırasıyla 8/16/32/64 bit işaretsiz, `i32/i64` işaretli tamsayıdır; genişlikleri 1/2/4/8 B’dır. `f32`, 4 B `float`tır. Başka aralık belirtilmeyen tamsayı tanı alanlarında tipin bütün aralığı geçerlidir. Dizilerde genişlik eleman başınadır. `P`, portta karşılığı bulunmayan **platform/protokol alanını** gösterir; bunların yerelde tutulması tek başına hata değildir.

Her `sess:u16` alanı güncel A1 oturumudur ve port yapısına kopyalanmaz. Her komut mesajındaki `cmd_id:u32` ve `source:u8`, daraltıcı olmayan yolda `ida_cmd_t`nin aynı adlı alanlarına gider; mandal yolundaki kayıplar bulgu 3’tedir.

| A1 alanı / uygulandığı mesajlar | Port karşılığı | Tip / genişlik | Birim; aralık |
|---|---|---|---|
| Sensörlerde `t_meas_ns` | `hdr.t_meas.ns` | i64 → i64 | ns; 0…2⁶² |
| Sensörlerde `seq` | `hdr.seq` | u32 → u32 | Sayaç; seri aritmetik |
| Sensörlerde `flags` | `hdr.flags` | u16 → u16 | `VALID=1`, `SATURATED=2`, `SENSOR_FAULT=4` |
| Sensörlerde `inst` | `hdr.inst` | u8 → u8 | 0…3 |
| IMU/BARO/BATT `window_ns` | `hdr.window_ns` | u32 → u32 | ns; >0; diğer sensörlerde port değeri 0 |
| Mesaj kimliği | `hdr.type` | türetilir → u8 | IMU=1, MAG=2, BARO=3, GNSS=4, DVL=5, PING1D=6, PING360=7, BATT=8, LEAK=9, SAFETY=10 |
| Tel alanı yok | `hdr.t_rx.ns` | platform → i64 | ns; 0…`now_w`; bulgu 20 |

Ortak sensör satırları IMU, MAG, BARO, GNSS, DVL, PING1D, BATT, LEAK, SAFETY ve PING360_HEAD için geçerlidir.

| A1 mesajı ve özel alanları | Port karşılığı | Tip / genişlik | Birim; aralık |
|---|---|---|---|
| `IMU.gyro[3]` | `in.imu.imu.gyro[3]` | f32 → f32 | rad/s; sonlu |
| `IMU.accel[3]` | `in.imu.imu.accel[3]` | f32 → f32 | m/s²; sonlu |
| `IMU.temp_c` | `in.imu.imu.temp_c` | f32 → f32 | °C; sonlu |
| `MAG.field[3]` | `samples[].u.mag.field[3]` | f32 → f32 | T; sonlu |
| `BARO.p_abs`, `temp_c` | `u.baro` aynı adlar | f32 → f32 | Pa, °C; basınç >0, değerler sonlu |
| `GNSS.lat_e7`, `lon_e7` | `u.gnss` aynı adlar | i32 → i32 | 10⁻⁷ derece; ±900000000 / ±1800000000 |
| `GNSS.alt_mm` | `u.gnss.alt_mm` | i32 → i32 | mm; bütün i32 |
| `GNSS.vel_ned[3]` | `u.gnss.vel_ned[3]` | f32 → f32 | m/s; sonlu |
| `GNSS.eph`, `epv` | `u.gnss` aynı adlar | f32 → f32 | m, 1σ; sonlu ve ≥0 |
| `GNSS.fix_type`, `n_sv` | `u.gnss` aynı adlar | u8 → u8 | fix 0…4; uydu sayısı 0…255 |
| `DVL.vel[3]`, `cov[6]`, `fom` | `u.dvl` aynı adlar | f32 → f32 | m/s, (m/s)², m/s; sonlu; kovaryans mevcutsa pozitif tanımlı, `fom≥0` |
| `DVL.beam_dist[4]`, `altitude` | `u.dvl` aynı adlar | f32 → f32 | m; ≥0 veya −1 |
| `DVL.dvl_flags`, `beam_valid` | `u.dvl` aynı adlar | u8 → u8 | bayraklar 1/2/4; ışın maskesi 0…15 |
| `PING1D.distance`, `scan_start`, `scan_length` | `u.ping1d` aynı adlar | f32 → f32 | m; sonlu ve ≥0 |
| `PING1D.ping_number`, `tx_offset_ns` | `u.ping1d` aynı adlar | u32 → u32 | sayaç; ns |
| `PING1D.confidence` | `u.ping1d.confidence` | u8 → u8 | %; 0…100 |
| `BATT.voltage`, `current`, `temp_c` | `u.batt` aynı adlar | f32 → f32 | V, A, °C; sonlu |
| `LEAK.mask` | `u.leak.mask` | u8 → u8 | bit maskesi; 0…255 |
| `SAFETY.estop`, `tube[2]` | `u.safety` aynı adlar | u8 → u8 | estop 0/1; tüp bitleri 1/2/4/8 |
| `SAFETY.safety_rev` | P: platformun sakladığı revizyon | u32 | süreç boyunca artan/doyan; 0 henüz görülmedi |
| `PING360_HEAD.angle_grad` | `ping360->angle_grad` | u16 → u16 | 0…399 |
| `PING360_HEAD.n_samples` | `ping360->n_samples` | u16 → u16 | A1 200…1200; A4 1…1200, bilinçli daha geniş port aralığı |
| `PING360_HEAD.sample_period` | `ping360->sample_period` | u16 → u16 | 25 ns; A1 80…40000, A4 ≥1 |
| `PING360_HEAD.tx_duration_us`, `tx_freq_khz` | aynı adlı alanlar | u16 → u16 | µs, kHz; port daraltmıyor |
| `PING360_HEAD.gain` | `ping360->gain` | u8 → u8 | cihaz kodu; port daraltmıyor |
| `PING360_HEAD.n_frag` | P: yeniden birleştirme | u8 | `ceil(n_samples/240)`, 1…5 |
| `PING360_DATA.seq`, `inst` | P: HEAD ile eşleme | u32, u8 | aynı satır kimliği/örneği |
| `PING360_DATA.frag_idx` | P: yeniden birleştirme | u8 | 0…`n_frag−1` |
| `PING360_DATA.data[240]` | `ping360->data[]` içindeki ilgili dilim | u8 → u8 | genlik baytı; toplam ≤1200 |

A1’in Ping360 aralıklarının A4’ten daha dar olması belgede açıkça ayrılmıştır; bu fark hata değildir. 200–1200 örnek, 80–40000 örnek periyodu ve 25 ns birimi [Blue Robotics birincil kaynağıyla](https://docs.bluerobotics.com/ping-protocol/pingmessage-ping360/) doğrulandı.

| A1 mesajı / alanları | Port karşılığı | Tip / genişlik | Birim; aralık |
|---|---|---|---|
| `TIME_FRAME.t_sim_ns`, `batch_seq` | `tf.t.ns`, `tf.seq` | i64, u32 | ns 0…2⁶²; seri aritmetik |
| `TIME_FRAME.n_members`, `tf_flags`, `run_mode` | P; IMU varlığından `imu_present` türetilir | u8 | 0…255; IMU biti 1; kip 1…4 |
| Tel alanı yok | `tf.present=1`, `tf.t_rx` | bool, i64 | alım damgası; oturum sonu istisnası bulgu 7 |
| `CMD_SIMPLE.type` | `cmd.type` veya mandal türü | u8 → u8 | {1,2,3,4,7,9,10,12,13,14,15} |
| `CMD_SIMPLE.arg` | `set_mode.mode`, `mission_begin.count`, `mission_ctl.action`, `param.id` | u16 → u8/u16 | kip: bulgu 2; öğe sayısı 1…128; eylem 1…4; parametre kimliği u16 |
| Diğer SIMPLE türlerinde `arg` | kullanılmaz | u16 | 0 |
| `CMD_MANUAL.axes[6]` | `u.manual.axes[6]`; tür 5 türetilir | f32 → f32 | birimsiz; [−1,1], sonlu |
| `CMD_GUIDED.mask` | `u.guided.mask`; tür 6 türetilir | u8 → u8 | bitler 1,2,4,8,16,32,64; sıfır maske reddedilir |
| `CMD_GUIDED.pos_ne[2]`, `depth`, `altitude` | `u.guided` aynı adlar | f32 → f32 | m; sonlu; derinlik/irtifa ≥0 |
| `CMD_GUIDED.vel_ned[3]` | `u.guided.vel_ned[3]` | f32 → f32 | m/s; sonlu |
| `CMD_GUIDED.yaw`, `yaw_rate` | `u.guided` aynı adlar | f32 → f32 | rad, rad/s; yaw (−π,π] |
| `CMD_MISSION_ITEM.index` | `u.mission_item.index`; tür 8 türetilir | u16 → u16 | 0…`count−1` |
| `CMD_MISSION_ITEM.item_type`, `frame` | aynı adlı alanlar | u8 → u8 | A1 tür 1…7; çerçeve 0…2; A4 farkı bulgu 1 |
| `CMD_MISSION_ITEM.p[4]` | aynı adlı alan | f32 → f32 | A1 Tablo 3.6a’nın tür başına birim/aralıkları; A4 anlamları eksik |
| `CMD_MISSION_ITEM.lat_e7`, `lon_e7` | aynı adlı alanlar | i32 → i32 | coğrafi sınırlar; kullanılmıyorsa 0 |
| `CMD_MISSION_ITEM.north_m`, `east_m`, `depth` | aynı adlı alanlar | f32 → f32 | m; yerel konum ±10000; derinlik tür başına 0…100 |
| `CMD_PARAM_SET.param_id`, `param_type`, `value_bits` | `u.param.id`, `.type`, `.v`; tür 11 türetilir | u16, u8, u32 → aynı genişlik | tür 1…3; değer i32/u32/f32 bitleri |
| `CMD_FC.op`, `tube`, `arg0`, `arg1` | `u.fc` veya mandal; tür 16 türetilir | u8, u8, u32, u32 | A1 op {1,2,3,5,6,7,8}; tüp 0/1; argümanlar 0; bulgu 1/3 |
| `EVENT_RESULT.event_id`, `result`, `t_apply_ns` | `evres[].event_id`, `.result`, `.t_apply.ns` | u64, u8, i64 | sim kodları 1…6; uygulama zamanı, diğer sonuçlarda 0 |

Görev parametrelerinin A1 Tablo 3.6a’daki anlamları kapalıdır; bunları A4’ün genel `p[]` alanına kopyalamak genişlik bakımından kayıpsız, anlam sözleşmesi bakımından eksiktir.

| A1 mesajı / alanları | Port karşılığı | Tip / genişlik | Birim; aralık |
|---|---|---|---|
| `ACTUATOR.batch_seq` | `out.seq` | u32 → u32 | hata dönüşü sorunu bulgu 4 |
| `ACTUATOR.valid_for_ns` | `out.valid_for_ns` | u32 → u32 | ns; ortak profil 60…450 ms; geçersiz komutta 0 |
| `ACTUATOR.pwm_us[8]` | `out.thr_cmd[8]` dönüşümü | u16 ↔ f32 | µs ↔ [−1,1]; A5 ESC eşlemesi; geçersiz komutta telde 0 |
| `ACTUATOR.act_flags` | `thr_valid/is_safe/armed/src`, sağlık ve platform bayrakları | u16 ↔ bool/u8 | tanımlı maske 0x03FF |
| `ACTUATOR.rx_to_tx_us` | P: `tf.t_rx`ten türetilir | u16 | µs; 65535’te doyar |
| `ACTUATOR.fs_level`, `core_rc` | `out.fs_level`, işlev dönüşü | u8 | 0…5; A1 yanıtında {0,3,4,8} |
| `ACTUATOR.n_events`, `n_merged`, `out_seq` | kapıdan geçen olay sayısı; P; P | u8, u8, u32 | 0…4; 0…255; üretim sayacı |
| `EVENT.event_id`, `t_decide_ns`, `batch_seq` | `event_id`, `t_decide.ns`, `seq` | u64, i64, u32 | kimlik; ns; parti kimliği |
| `EVENT.apply_by_ns`, `hold_ns`, `kind`, `tube` | aynı adlı alanlar | u32, u32, u8, u8 | süreler bulgu 10; tür 1…3; tüp 0/1, CLOSE için 255 |
| `EVENT.safety_rev`, `call_kind` | P: revizyon ve çağrı türü | u32, u8 | revizyon; STEP=1/TICK_TF=2/TICK_GRID=3 |
| Port `event.fc_ref` | tel karşılığı yok | u32 | bulgu 1 |
| `CMD_RESULT.cmd_id`, `kind`, `ack`, `reason` | `results[]` aynı adlar | u32, u8, u8, u16 | tür 1…3; ACK 1…5; neden alanı bulgu 19 |
| `CMD_RESULT.param_id`, `param_type`, `value_bits` | `value.id`, `.type`, `.v` | u16, u8, u32 | PARAM_VALUE; tür 1…3 |
| `CMD_RESULT.save_gen`, `save_len`, `save_crc` | `save.gen`, `.len`, blobun CRC’si | u32; u16 ← u32; u32 | boy ≤8000 B olduğundan daraltma kayıpsız; blob telde yok |
| `NAV.t_ns`, `batch_seq` | `nav.t.ns`, `out.seq` | i64, u32 | ns; parti |
| `NAV.p_ned[3]`, `v_ned[3]`, `omega_b[3]` | `nav` aynı adlar | f32 | m, m/s, rad/s |
| `NAV.q_nb[4]` | `nav.q_nb[4]` | f32 | birim kuaterniyon; w,x,y,z |
| `NAV.depth`, `altitude` | `nav` aynı adlar | f32 | m; bilinmeyen irtifa −1 |
| `NAV.p_sigma[3]`, `v_sigma[3]`, `att_sigma[3]` | `nav` aynı adlar | f32 | m, m/s, rad; 1σ |
| `NAV.origin_lat_e7`, `origin_lon_e7` | `nav` aynı adlar | i32 | 10⁻⁷ derece |
| `NAV.est_level`, `reset_counter` | `nav` aynı adlar | u8 | 0…4; mod 256 sayaç |
| `STATUS.boot_count`, `fs_reasons`, `mission_item` | `status` aynı adlar | u32, u32, u16 | açılış sayısı; bit maskesi; öğe indisi |
| `STATUS.mode`, `armed`, `fs_level`, `regime`, `phase`, `mission_state`, `param_ok`, `events_ok` | `status` aynı adlar | u8 → u8/bool | sırasıyla 0…7, 0/1, 0…5, 0…2, 0…3, 0…5, 0/1, 0/1 |
| `STATUS.param_gen`, `param_store`, `param_fail` | `plat` aynı adlar | u32, u8, u8 | kuşak; 0…3; 0…4 |
| `STATUS.param_blob_crc`, `batch_seq`, `link_state` | P; `out.seq`; A1 durumu | u32, u32, u8 | CRC; parti; A1 0/1/2, port link durumu ise NONE=0/RUNNING=1 olarak çevrilir |
| `STATUS.health` | `out.health` | u8 ↔ sekiz bool | aşağıdaki bit tablosu |
| `FC_STATUS.last_event_id`, `inhibit_mask`, `state`, `last_event_result` | `out.fc` aynı adlar | u64, u32, u8, u8 | son olay; engeller; SAFE=0; sonuç 0 veya tanımlı kod |
| Port `fc.ext_len`, `ext[64]` | tel karşılığı yok | u8, u8[64] | bulgu 1 |
| `LOG.t_w_ns`, `arg0`, `arg1`, `code`, `severity` | `logs[].t_w.ns`, aynı adlı diğer alanlar | i64, i32, i32, u16, u8 | ns; kod/argüman anlamı bulgu 21; önem 0…7 |

| A1 oturum/tanı mesajı ve alanları | Port karşılığı | Tip / genişlik; aralık |
|---|---|---|
| `SIM_HELLO.dialect_hash`, `nonce`, `run_id`, `ctrl_period_ns`, `proto_major/minor` | P | u64; u32≠0; u32; u32=5000000 ns; u8=1/0 |
| `BOARD_HELLO.param_schema_hash`, `core_version`, `port_major/minor` | `ida_core_info_t` aynı adlar | u64, u32, u16/u16 |
| `BOARD_HELLO.boot_count`, `reset_flags`, `reset_cause`, `consecutive_faults`, `boot_count_ok`, `param_src` | `ida_init_in_t` aynı adlar | u32, u32, u8, u8, u8→bool, u8; boot 1…0xFFFFFFFE, neden 0…8, bool 0/1, kaynak 0…2 |
| `BOARD_HELLO.init_rc` | `ida_core_init` dönüşü | u8; kurulu örnekte OK/PARAM_DEFAULTS; güvenli bekleme duyurusunda NOT_INIT |
| `BOARD_HELLO.det_schema_major/minor` | A8 başlık sabitleri | u16=1/0 |
| `BOARD_HELLO.dialect_hash`, `peer_nonce`, `nonce`, `fw_version`, `fw_git`, `ctrl_period_ns`, `proto_major/minor`, `role` | P | u64; u32; u32≠0; u32/u32; u32=5000000 ns; u8=1/0; u8=1/2 |
| `BOARD_HELLO.param_blob_crc`, `param_gen`, `param_blob_len`, `last_cmd_id`, `prev_sess`, `sessions_since_boot` | platform kalıcı durumu; port blob/kuşak/komut bilgisinden | u32, u32, u16≤8000, u32, u16, u16 |
| `SESSION_ACCEPT.run_id`, `nonce_sim`, `nonce_board`, `result`, `run_mode` | P; sonuçtan oturum geçişi türetilir | u32/u32/u32; u8=0…8; u8=1…4 |
| `KEEPALIVE.state` | P; port link durumu için açık çeviri | u8; A1 0…2 |
| `SESSION_CLOSE.reason` | P; `plat.link_event=SESSION_END` türetilir | u8; 1…6 |
| `CALL_TRACE.now_w_ns`, `batch_seq`, `bg_slices`, `prev_call_ns` | `in.now_w.ns`, `in.tf.seq`, `in.bg_slices`, `plat.prev_call_ns` | i64 ns; u32; u32; u32 ns |
| `CALL_TRACE.rx_age_ns[28]` | TF, IMU, 16 örnek, iki başlık, sekiz kaydın alım yaşı | u32 ns; 2³²−1’de doyar |
| `CALL_TRACE.n_rx`, `call_kind`, `core_rc`, `plat_flags`, `n_merged` | P; çağrı türü/dönüşü; platform bayrakları; P | u8; ≤28, 1…3, 0…8, maske 0x3F, 0…255 |
| `LINK_STATS.cnt[36]` | A1 Tablo 3.11b | u32[36]; açılıştan beri mod 2³² |
| `FAULT_RECORD.cfsr/hfsr/mmfar/bfar/pc/lr/xpsr/step_counter`, `fault_seq`, `stacking_valid` | P: kart hata kaydı | sekiz u32, u16, u8; son alan 0/1 |

`LINK_STATS` içinden port eşlemesi: `[11]→cnt_stale_session`, `[15]→cnt_batch_incomplete`, `[16]→cnt_batch_skipped`, `[17]→cnt_batch_old`, `[19]→cnt_batch_merged`, `[20]→cnt_member_overflow`, `[24]→cnt_cmd_deferred`, `[25]→cnt_cmd_dropped`, `[26]→cnt_narrow_coalesced`, `[28]→cnt_grid_missed`, `[29]→cnt_out_fixed`; `[2]+[4]+[5]+[8]→cnt_rx_error`. `[32]` artışının anlık karşılığı `tx_stalled`dır. Diğer indisler platform tanısıdır.

**CORE_COUNTERS: 22/22 alan eşit**

Her satırda XML alanı u32/4 B, port alanı `status.cnt.<aynı ad>` u32/4 B’dir; birim adet, aralık mod 2³²’dir.

| XML alanı | Port alanı | Sonuç |
|---|---|---|
| `sample_rejected` | `status.cnt.sample_rejected` | uyumlu |
| `sample_future` | `status.cnt.sample_future` | uyumlu |
| `sample_conflict` | `status.cnt.sample_conflict` | uyumlu |
| `sample_dup_dropped` | `status.cnt.sample_dup_dropped` | uyumlu |
| `narrow_from_rejected` | `status.cnt.narrow_from_rejected` | uyumlu |
| `bg_bad_input` | `status.cnt.bg_bad_input` | uyumlu |
| `imu_rejected` | `status.cnt.imu_rejected` | uyumlu |
| `imu_gap` | `status.cnt.imu_gap` | uyumlu |
| `bad_input` | `status.cnt.bad_input` | uyumlu |
| `dup_seq` | `status.cnt.dup_seq` | uyumlu |
| `old_seq` | `status.cnt.old_seq` | uyumlu |
| `wall_backstep` | `status.cnt.wall_backstep` | uyumlu |
| `cmd_rejected` | `status.cnt.cmd_rejected` | uyumlu |
| `widen_blocked` | `status.cnt.widen_blocked` | uyumlu |
| `evres_ignored` | `status.cnt.evres_ignored` | uyumlu |
| `event_unknown` | `status.cnt.event_unknown` | uyumlu |
| `det_rejected` | `status.cnt.det_rejected` | uyumlu |
| `ping360_rejected` | `status.cnt.ping360_rejected` | uyumlu |
| `bg_dropped` | `status.cnt.bg_dropped` | uyumlu |
| `bg_slice_mismatch` | `status.cnt.bg_slice_mismatch` | uyumlu |
| `log_dropped` | `status.cnt.log_dropped` | uyumlu |
| `step_miss` | `status.cnt.step_miss` | uyumlu |

**STATUS.health: 8/8 bit eşit**

| XML biti/değeri | Port bool alanı | Sonuç |
|---|---|---|
| 0 / 1 | `core_ok` | uyumlu |
| 1 / 2 | `data_ok` | uyumlu |
| 2 / 4 | `imu_rejected` | uyumlu |
| 3 / 8 | `imu_lag` | uyumlu |
| 4 / 16 | `input_dropped` | uyumlu |
| 5 / 32 | `narrowed` | uyumlu |
| 6 / 64 | `sample_conflict` | uyumlu |
| 7 / 128 | `bg_bad_input` | uyumlu |

**Algılama alanları: A1 ↔ gerçek A8 tipi ↔ A4**

A4 bunları `in.det[]`, `in.det_batch[]` ve `out.det_tx[]` içinde aynı tiplerle taşır.

| Tel alanları | Yapı alanları | Tip / genişlik | Birim; aralık |
|---|---|---|---|
| RECORD `t_meas_ns`, `det_seq`, `batch_seq` | aynı adlar | i64, u32, u32 | ns; kayıt/parti kimliği; zaman üst sınırı bulgu 17 |
| RECORD `window_ns`, `t_sigma_ns` | aynı adlar | u32, u32 | ns; kamera penceresi ≤1 s, sonar 0; zaman belirsizliği ≤1 s |
| RECORD `z[3]` | aynı ad | f32[3] | m/rad/rad; mesafe (0,1000], ölçülmeyen 0; açılar A8 sınırları |
| RECORD `chol[6]` | aynı ad | f32[6] | satıra göre m/rad; A8-K07 sınırları |
| RECORD `confidence` | aynı ad | f32 | [0,1] |
| RECORD `calib_ver`, `producer_ver`, `flags` | aynı adlar | u16 | kalibrasyon/üretici sürümü; A8 kayıt bayrakları |
| RECORD `source`, `inst`, `kind`, `frame`, `class_id`, `producer_id`, `batch_index`, `batch_count` | aynı adlar | sekiz u8 | kaynak 1/2; inst 0…254; tür 1…3; frame=1; sınıf u8; üretici≥1; sıra 0…3; sayı 1…4 |
| BATCH `t_meas_ns`, `batch_seq`, `window_ns` | aynı adlar | i64, u32, u32 | ns; artan parti kimliği, NO_FRAME’de 0; pencere ≤1 s/sonarda 0 |
| BATCH `calib_ver`, `producer_ver`, `flags` | aynı adlar | u16 | sürümler; yalnız NO_FRAME biti tanımlı |
| BATCH `source`, `inst`, `status`, `count`, `producer_id` | aynı adlar | beş u8 | 1/2; 0…254; 0…8; 0…4; durum başlığında üretici 0 olabilir |
| Tel alanı yok | RECORD `reserved[2]`, BATCH `reserved[5]` | u8 dizileri | alıcı 0 yazar |
| Tel alanı yok | A4 `det_t_rx[]`, `det_batch_t_rx[]` | i64 | ns; 0…`now_w` |

Algılama `inst` aralığının 0…254 olması, sensör başlığındaki `MAX_INST=4` ile çelişmiyor: farklı yapılara ait alanlardır.

**Enum ve dönüş kodları**

| Aile | Yeniden denetlenen değerler | Sonuç |
|---|---|---|
| `IDA_RC_*` ↔ `IDA_CORE_RC_*` | OK=0, DUP=1, OLD=2, BAD_INPUT=3, NOT_INIT=4, PARAM_DEFAULTS=5, VERSION=6, NOT_FOUND=7, IMU_REJECTED=8 | uyumlu |
| `IDA_BG_*` | IDLE=0, MORE=1, BAD_INPUT=2 | Tel enumu yok; yalnız ilk ikisinin `bg_slices`a sayılması uyumlu |
| `IDA_REASON_*` | ortak 0…15 | uyumlu; 128…191 ayırması bulgu 19 |
| Komut türleri | HEARTBEAT=1 … FC=16 | uyumlu |
| Olay türleri | OPEN=1, CLOSE=2, FIRE=3 | uyumlu |
| Olay sonuçları | 1…6 ve 16…19 | uyumlu |
| FC işlemleri / görev türleri | ortak girdilerin değerleri eşit | A4’teki fazladan SELECT=4 ve TARGET_ALIGN=8 bulgu 1 |
| Diğer ayna aileleri | örnek/DVL/tüp bayrakları, kip, setpoint, görev çerçevesi/denetimi, parametre, sonuç/ACK, arıza düzeyi, GNSS fix, önem | uyumlu |

**Kapasiteler ve zaman eşitsizlikleri**

| Madde | Bağımsız hesap / değerlendirme |
|---|---|
| Örnek kapasitesi | A1’in 520 B kritik sınırı, en küçük 28 B IMU-dışı sensörlerle bile TF+IMU sonrasında en çok `floor((520−88)/28)=15` örneğe izin verir. Tek geçerli pencere salt sayı kapasitesi yüzünden 16’yı aşmaz. |
| Komut/sonuç kapasitesi | Bir A1 partisinde en çok dört normal komut + dört mandal = **8 sonuç**. İki parti birleşince sekiz normal + dört mandal = **12 sonuç**; A4’e sığar. |
| Algılama kapasitesi | Tek parti **4 kayıt/1 başlık**; iki parti **8 kayıt/2 başlık**. A4’e sığar. Daha fazla birleşmede kapasite aşımı düşürme kurallarına gider. |
| Olay sonucu | Tek A1 partisinde ≤4; iki partide 8 olabilir. A4-K32/K68 fazlayı sonraki çağrıya taşır; doğrudan `n_evres=8` verilmemelidir. |
| Açık olay / sonuç kuyruğu | `N_olay=8`, `Q_sonuç=8`; yeni sonuçlar ve saklı sonuç taleplerinin ayrılması mevcut eski kuyruk karşı örneğini gideriyor. Zaman davranışı bulgu 8’e bağlı. |
| Son olay sayacı aralığı | `0xFFFFFFFF−0xFFFFFFF0+1 = 16` değer; yalnız kapatma olayları. |
| E1 | `5<10`: uyumlu. |
| E2 | `35<60`, `450<500`: uyumlu. |
| E3 | `60≥10+5+14,5=29,5`: sayısal olarak uyumlu; 14,5 ms gözlenen değerdir. |
| E4 | `4×100=400≤500`: uyumlu. |
| E5 | `500>14,5`, `500>35`: uyumlu. |
| E6 | `16≥10+5=15`: uyumlu; A4’ün daha geniş aralığı bulgu 10. |
| E7 | `15<16`: uyumlu; A4’ün izin verdiği bütün değerler için geçerli değil. |
| E8 | `2×35+5+14,5+0,86=90,36≤95<100`: aritmetik doğru; A4’ün sonuç üretme anı bulgu 8 nedeniyle uyumsuz. |
| E9 | `60/2=30≥29,5`, `450<500`: ortak profil için uyumlu. |
| E10 | Varsayılanda `20+14,5=34,5<60`: doğru; değiştirilebilir bayatlık parametresi bulgu 11. |
| E11 | Göndericide ≥2,5 ms TF aralığı, yarı açık 5 ms’de ≤2 TF verir. **Alıcıdaki** sıkışma üst sınırını vermez; A4 geliş varsayımı yalnız kısmen karşılanmış. |
| E12 | 1200 örnek için beş DATA+HEAD = altı çerçeve; sekiz kapanışlık yetim süresi pencere planına yeter: uyumlu. |
| E13 | `29+59+34+102+124=348`; `520−348=172≥89`; `520+399+258=1177≤1200`: uyumlu. |
| E14 | `260≥258`: uyumlu. |
| E15 | `520+59,85+60+260=899,85≤900 B/periyot`: uzun dönem hesabı uyumlu. |
| E16 | Yazılı süre toplamı `14,5+1,25+35+5+35=90,75<95 ms`: doğru; açık olayın tutulma süresi bulgu 8’le tekleştirilmeli. |
| E17 | `σ=36×39+7×49+8×49+15=2154 B`; `ρ=141,5333 B/ms`. Verilen yaklaşık hizmet modeliyle kuyruk `2154+2ρ=2437,0667 B`, gecikme `2+2154/200=12,77 ms`. Bunlar model hesabıdır; ölçülmüş sınır değildir. |
| E18 | Toplam **9,733333 ms**, pay **0,266667 ms**. Önceki tick ile **10,233333 ms**, azami başlangıç beklemesiyle **13,233333 ms**. Çakışmasız koşulda hesap doğru; A4 takvimi bulgu 6. |
| E19 | Kesin oranlarla `75+5×141,5333+500=1282,6667 B/periyot`, kapasitenin **%85,5111**’i; 1350 B altında. Metindeki 1282,5/1282,7 farkı yuvarlamadır. |

Beş A1 değişkeninin **32 uç köşe birleşiminde**, varsayılan 20 ms IMU bayatlığıyla E1–E10’un yazılı aritmetik ifadeleri sağlandı. Bu denetim geliş/hizmet varsayımlarının veya ölçülmemiş gecikmelerin doğruluğunu kanıtlamaz.

Ek hesaplar: kamera partisi `39+4×90=399 B`; 30 Hz’de **11970 B/s**, kontrol periyodu başına **59,85 B**. A8 soketindeki dört kayıtlı mesaj `16+40+4×80=376 B`. Kayıt sayacının 120 kayıt/s’de sarma süresi **414,252 gün**.

**Çelişki bulunmayan denetim maddeleri ve beklenti kapanışları**

| Denetim / beklenti | Sonuç ve dayanak |
|---|---|
| A1 ↔ A4 sensör tipleri, genişlikleri ve birimleri | uyumlu; yukarıdaki alan eşlemesi, aynı f32/tamsayı alanları ve ortak çerçeveler |
| Tek parti ve iki parti için sayısal kapasiteler | uyumlu; 16/8/8/2/4 sınırları ve 12 sonuç hesabı |
| `CORE_COUNTERS`, `STATUS.health` | uyumlu; 22 alan ve sekiz bit ayrı ayrı doğrulandı |
| İlk kabul ile taşıma yinelemesinin ayrılması | uyumlu; A4-K67/K106 ile A1-K80/K81; yineleme yeni olay değildir |
| `IMU_LAG` | uyumlu; A4 20 ms kurma, 10 ms/20 çağrı çözme ve genişletici yasağı A1’de karşıt bir kuralla değiştirilmemiş |
| A1 Tablo 4.6a/4.6b ↔ A4 §4.14/K64 | uyumlu; hata dönüşünde sürekli komut yok, tick/IMU_REJECTED yalnız CLOSE, FIRE yalnız uygun OK step çıktısında |
| A4’ün A1’den beklediği 2 | uyumlu; estop ve dört tüp biti SAFETY’de mevcut |
| A4’ün A1’den beklediği 4 | uyumlu; alıcı izni uygulama anından itibaren sim zamanında sayıyor |
| A4’ün A1’den beklediği 5 | A1 tarafı tanımlı; sürelerin A4’e bağlanması bulgu 10 |
| A4’ün A1’den beklediği 6 | uyumlu; kutu merkezi zamanı A1-K52’de |
| A4’ün A1’den beklediği 9 | uyumlu; eski birleşen partiye ayrı yanıt yok, `n_merged` ve kaçırma sayımı var |
| A4’ün A1’den beklediği 11 | uyumlu; platform sayaç kaynakları Tablo 3.11b’de |
| A4’ün A1’den beklediği 1/3/7/10/12 | kısmen; kapsam alanları, hata yanıtı kimliği, mandal ve sonuç zamanı bulgu 1/3/4/8 |
| A4’ün A1’den beklediği 8 | kısmen; gönderici sınırı var, alıcı sıkışması için sayısal üst sınır yok |
| A1’in A4’ten beklediği 2 | uyumlu; CLOSE da son geçerliliğe tabi, izin ömrü ayrı |
| A1’in A4’ten beklediği 4 | uyumlu; K58 değeri A1’e eşitlemeyi zorunlu kılıyor |
| A1’in A4’ten beklediği 7 | uyumlu; olay tablosunun sayısal sahibi A1, N=8 |
| A1’in A4’ten beklediği 8 | uyumlu; `safety_rev` platformda tutuluyor, port alanı beklenmiyor |
| A1’in A4’ten beklediği 1/3/5/6/9/10/11/12 | karşılanmamış; sırasıyla bulgu 19/10/1/7/1/6/5/9 |
| A4’ün A8’den beklediği 1/3 | uyumlu; gerçek tip/etiketler derleniyor, alım zamanı ayrı dizilerde |
| A4’ün A8’den beklediği 2 | A8 doğrulaması tanımlı; A4 kabul birimi bulgu 14 |
| A8’in A4’ten beklediği 1/4/5 | uyumlu; gerçek yapılar, i64 ns ve Cholesky alanları taşınıyor |
| A8’in A4’ten beklediği 2/3 | kısmen/karşılanmamış; bulgu 14–16 |

**A8 ikinci incelemesindeki dört bulgunun kapanışı**

| Önceki bulgu | Güncel metnin denetimi | Sonuç |
|---|---|---|
| K27/K28 | `FRAME_INVALID,count=0,calib_ver` yanlış örneği artık ilk başarısız CALIB satırından `CALIB_MISMATCH` alıyor | uyumlu; kapandı |
| §3.3 TIME | Boş sonar partisinde `window_ns=1` artık açık TIME denetiminde reddediliyor; negatif zaman ve pencere koşulları yazılmış | uyumlu; önceki bulgu kapandı; farklı üst sınır sorunu bulgu 17 |
| K46 | Yalnız açık karelere dokunuyor; nihai durum K63 önceliğiyle bir kez seçiliyor | uyumlu; kapandı |
| K39/K42 | `count=0,length=41` parti reddi; `length=361` çerçeve hatası; minor 0’da uzantı yok | uyumlu; kapandı |

## İskelete ya da diğer bölümlere notlar

**İskelet 0.9’a ait ayrı bulgular**

| # | Yer | Sorun | Doğrusu / önerilen düzeltme | Önem |
|---|---|---|---|---|---|
| İ1 | `docs/mimari/00-iskelet.md` §3.2 “Emniyet revizyonu” ↔ A1-K106; `W1-capraz.md` “tek sayaç” | İskelet emniyet yetkisini değiştiren **her olayın** revizyonu artırdığını söylüyor. A1 ve çapraz görev tanımı uygulanan `SAFETY_OPEN` açma/yenilemesini açıkça hariç tutuyor. İki tüpe aynı revizyonla üretilmiş açma olaylarında ilk olayın revizyonu artırıp artırmaması ikinci olayın sonucunu değiştirir. | İskeletteki genel ifadeyi kapalı artış listesine bağla. Öncelik gereği bu çelişki çözülmeden A1’in istisnası kendiliğinden bağlayıcı sayılamaz. | kritik |
| İ2 | İskelet §2 “Atış kontrol” ve §9 bölüm 03 kapsamı ↔ `W1-ortak.md` ve kullanıcının inceleme sınırı | İskelette hâlâ “atış çözümü” ve hizalanma/menzil koşuluna dayalı atış ölçütü kapsam ifadeleri bulunuyor. Bunlar bu incelemede verilen emniyet zinciri ve arayüz sınırını aşıyor. | İskelet kapsamını güncel dar sınıra çek; bu içeriği sözleşme veya uygulama gereği olarak aktarma. | önemli |

A4 §6’daki **sade 600 / tam 1200** ifadesi A1’in iki kademede 1200 üst sınırıyla ve `A5-senaryo.md` K78 ile uyumsuzdur. A4’ün bu satırı değiştirilmelidir; kademe farkı örnek üst sınırından türetilmemelidir. Önem: **önemli**.

İskeletin %60 olağan profil / %90 bağlayıcı yük ayrımı, A1’in güncel bant ölçütüyle uyumludur. Eski %84,3 açıklaması güncel **%85,51** hesabının gerisinde kalmıştır; sınırı değiştirmeyen açıklama güncellemesidir.

Eski 01–09 bölümlerindeki farklılıklar bulgu sayılmadı. Erişilen birincil web kaynakları doğrulandı; yerelde bulunmayan MAVLink üreteciyle tam üretim ve hedef kart/Windows çalışması doğrulanmış sayılmadı.