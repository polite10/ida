# A1 — Sim sunucusu ↔ kart (ve SIL koşucusu) bağlantı sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday (2026-11-19) |
| Görev belgesi | `docs/gorevler/W1-A1.md`; ortak kurallar `docs/gorevler/W1-ortak.md` |
| Makinece okunur tanım | `protocol/ida.xml` (MAVLink 2 dialect'i; bu belgenin parçasıdır, ikisi çelişemez) |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.8 (§3, §3.1, §3.2, §4, §5, §6, §8, §10); A4 sözleşmesi adayı (`docs/sozlesmeler/A4-port.md`, `core/include/ida/port.h`; düzeltme turu 1); A8 sözleşmesi adayı (`docs/sozlesmeler/A8-kamera-algilama.md`, `core/include/ida/algilama_kaydi.h`; düzeltme turu 1); ölçümler `docs/olcumler/MP3-vcp.md`, `MP4-dongu-yuku.md`, `MP1-arac-zinciri.md`; bölümler 01, 05, 06 (taslak; iskelet 0.8'den eski; çeliştikleri yerde iskelet geçerlidir) |
| Durum | Yazan Opus; sol incelemesi bekliyor; dondurulmadı |

**Değerlerin niteliği.** Her süre, hız, boyut ve kapasite ya bir ölçüme (rapor ve başlık atfıyla), ya bir birincil kaynağa (bağlantıyla), ya bir **hesaba** (aritmetik; ölçüm değil) ya da **mimari seçimi (tahmin)** etiketine dayanır. Etiketli değerler ölçülmüş değildir; Bölüm 8'de hangi ölçümle kapanacakları yazılıdır. Bu belgedeki uçtan uca gecikme bütçesi bir tahmindir; kartta ve PC'de birlikte ölçülmedi.

**Atış kontrol sınırı** (W1-ortak.md). Bu sözleşmede atışla ilgili içerik yalnız şunlardır: sanal emniyet girdilerinin ve fırlatıcı durumunun karta geri okunması (`IDA_SAFETY`), izin açma ve kapama olayları ile atış olayının taşınması (`IDA_EVENT`), sim'in olay sonucunu bildirmesi (`IDA_EVENT_RESULT`), atış kontrol komutlarının ve durumunun **opak** taşınması (`IDA_CMD_FC`, `IDA_FC_STATUS`) ve algılama kayıtlarının taşınması (`IDA_DET_*`). Hedef seçimi, atış ölçütü, menzil ya da önleme hesabına ilişkin alan, mesaj ya da kural yoktur; `IDA_CMD_FC.arg0/arg1` ve `IDA_FC_STATUS.ext[]` A1 tarafından yorumlanmaz.

---

## 1. Kapsam ve taraflar

### 1.1 Taraflar

| Taraf | Kim | A1'deki rolü |
|---|---|---|
| **Sim** | Sim sunucusu (C++20): HIL köprüsü, parti kurucu, bariyer, bağlantı emülatörü, yer istasyonu ağ geçidi | Oturumu açar; partileri kurar ve gönderir; sürekli komutları ve olayları uygular ya da reddeder; sayaçları tutar (A3'e bildirir, A6'ya yazar) |
| **Kart** | STM32U5A9J-DK üzerindeki kart yazılımı (`firmware/`) ve A1↔A4 dönüştürücüsü (`protocol/ida_link`) | Oturuma katılır; partileri kapatır ve çekirdeğe A4 girdisi olarak verir; çekirdeğin çıktısını A1 mesajlarına çevirir |
| **SIL koşucusu** | Ortak çekirdeği PC'de çağıran süreç (C17 + C++) ve aynı `protocol/ida_link` | Kartın yerine geçer; aynı mesajları yerel soketten konuşur (Bölüm 4.9) |

Bu belgede "kart", ayrıca belirtilmedikçe SIL koşucusunu da kapsar. "Platform" A4'teki anlamdadır (kart yazılımı ya da SIL koşucusu, `ida_link` dahil).

### 1.2 Kapsam

Bu sözleşme şunları tanımlar: seri bağlantı ayarları ve MAVLink 2 çerçevesi; çerçeve eşlemesi, bozuk ve eksik çerçevede davranış; oturum el sıkışması, kapanış, kopma ve yeniden bağlanma; parti, zaman çerçevesi, parti üyeliği ve atama kuralı; sensör, sonar ve algılama mesajları ve A4 eşlemesi; sürekli komut yanıtı, kabul ve ret kuralı; tek seferlik olaylar ve sim'in olay uygulaması; yer istasyonu komutları ve sınıfları; durum, telemetri, günlük ve çağrı izi; zamanlama değerleri ve eşitsizlikleri; bant bütçesi; sayaçlar; SIL taşıması.

### 1.3 Kapsam dışı (başka yerde)

| Konu | Yer |
|---|---|
| Çekirdeğin girdileri nasıl kullandığı, port yapıları ve dönüş kodları | A4 (`port.h`) |
| Algılama kaydı ve parti başlığının alanları ve doğrulaması | A8 (`algilama_kaydi.h`) |
| Panel ile sim arasındaki komut ve telemetri taşıması, gösterim | A3 |
| Araç ve otopilot parametrelerinin değerleri, sensör hızları ve sayıları, itici sırası ve ESC eşlemesi, senaryo zaman çizelgesi, bağlantı emülatörü profilleri | A5 |
| Kayıt dosyasının biçimi | A6 |
| Sensör modellerinin içi, `t_hazır` ve `t_teslim`'in üretimi, Ping360 tarama yapılandırması | Sensör modelleri ve sonar uygulaması (05; W3, W4); A5 |
| Kart içi kesme düzeni, DMA ve tampon boyutları, gözetim | Kart yazılımı uygulaması (01; W3) |
| Atış kontrolün iç kuralları, `fc` alanlarının anlamı | Atış kontrol uygulaması (W4) |
| Harici yer istasyonu yazılımına (ör. QGroundControl) açılan uç nokta | İlk sürümde yok (iskelet §3, §10 md. 17) |

---

## 2. Terimler ve kabuller

Birimler, çerçeveler, kuaterniyon kuralı ve iki zaman damgası iskelet §3.1'dedir; A4'ün terimleri (ölçüm zamanı, duvar saati, parti, zaman çerçevesi, daraltıcı, genişletici, seri aritmetik) A4 Bölüm 2'dedir. Burada yalnız bu sözleşmeye özgü olanlar:

| Terim | Anlamı |
|---|---|
| **Çerçeve** | Tek bir MAVLink 2 paketi (Bölüm 3.2) |
| **Akış** | Bir yöndeki bayt dizisi; seri bağlantıda ve TCP'de sıra korunur (FIFO) |
| **Oturum** | Bir el sıkışmayla başlayıp kapanışla biten, `sess` kimlikli bağlantı dönemi (Bölüm 4.2) |
| **Parti `n`** | `batch_seq = n` taşıyan `IDA_TIME_FRAME` (TF) ile kapanan üye kümesi |
| **Pencere `n`** | Sim'in akışında `TF(n−1)` ile `TF(n)` arasındaki aralık; parti `n`'in üyeleri bu aralıkta gönderilir |
| **Üye** | Bir partiye ait, TF dışındaki her sim → kart çerçevesi (sensör, sonar, algılama, komut, olay sonucu) |
| **`t_n`** | Parti `n`'in kapanış anı (sim zamanı) = `TF(n).t_sim_ns` |
| **`W(t)`** | Sim zamanı `t`'nin sim duvar saatindeki planlanan karşılığı (1x kiplerde çalıştırma başında bir kez kurulur) |
| **`P`** | Kontrol periyodu, 5 ms (200 Hz; iskelet §4) |
| **`h`** | Fizik adımı, 1,25 ms (iskelet §4) |
| **`L_act`** | Uygulama gecikmesi, iki periyot = 10 ms = 8 fizik adımı (iskelet §4) |
| **Uygulama anı `a_n`** | `t_n + L_act`; parti `n`'in yanıtının uygulandığı sim anı (iskelet §4) |
| **Yuva `n`** | Sim'in parti `n` için tuttuğu tek yanıt yuvası (iskelet §4) |
| **`t_hazır`** | Bir kaydın sim içinde kesin var olduğu sim anı (bir fizik adımı sınırı; sensör modelleri tanımlar, 05 §2.2) |
| **Kritik üye** | Penceresi `t_hazır`'a göre belirlenen ve hiçbir koşulda ertelenmeyen üye (A1-K35) |
| **Toplu üye** | Pencere bütçesine göre ertelenebilen üye (A1-K36) |
| **Atanmış üye** | Partisi başka bir sözleşmenin kuralıyla belirlenen üye: kamera algılama partisi, A8-K57'nin `P_k`'sına (A1-K38) |
| **Daraltıcı komut** | A4-K44'teki komutlar: `DISARM`, hedefi GUIDED/AUTO olmayan `SET_MODE`, `op` = FC_DISARM/FC_ABORT olan `FC` |
| **Eşzamansız güvenli çıktı** | Kartın ızgara tick'inden çıkan, `THR_VALID = 1` ve `IS_SAFE = 1` taşıyan `IDA_ACTUATOR` (`ASYNC` bayrağıyla; A1-K44) |
| **Çalıştırma geçerliliği** | Sim'in bir çalıştırmayı zamanlama bakımından geçerli sayıp saymadığı (iskelet §4; A1-K79) |

---

## 3. Veri tanımları

### 3.1 Sabitler

| Ad | Değer | Dayanak |
|---|---|---|
| Seri hız | 3 000 000 baud, 8 veri biti, eşliksiz, 1 durdurma biti (8N1), akış denetimi yok | K17; iskelet §3, §8; MP3 (600 s, %80 yük, kayıpsız; `MP3-vcp.md` "Uzun kampanya") |
| Yön başına kapasite | 300 000 B/s; periyot başına 1 500 B | Hesap (8N1'de bayt başına 10 bit; iskelet §3) |
| Ortalama yük hedefi | Her yönde periyot başına ≤ 900 B (%60) | İskelet §3 |
| Pencere planı üst sınırı `B_pencere` | 1 200 B (%80) | Mimari seçimi (tahmin); MP3'te %80 sabit yük 600 s kayıpsız geçti |
| Ping360 pencere payı `B_p360` | 600 B | Mimari seçimi (tahmin); ortalama hedefi sağlar (3.10) |
| Sim kimliği (MAVLink `sysid`, `compid`) | 250, 25 | Mimari seçimi; 25, `common.xml`'de `MAV_COMP_ID_USER1`'in değeridir (pymavlink 2.4.50 ile gelen `common.xml`'den okundu) |
| Kart ve SIL koşucusu kimliği | 1, 1 | Mimari seçimi; 1, `common.xml`'de `MAV_COMP_ID_AUTOPILOT1`'dir (aynı kaynak) |
| Panel kaynak kimliği (`source`) | 255 | Mimari seçimi (yer istasyonu geleneği) |
| Sim laboratuvar kaynak kimliği (`source`) | 250 | Mimari seçimi |
| Mesaj kimliği aralığı | 46000–46099 | Mimari seçimi; Bölüm 3.2 |
| `proto_major`, `proto_minor` | 1, 0 | Bu sözleşme |
| `P`, `h`, `L_act` | 5 ms, 1,25 ms, 10 ms | İskelet §4 |
| Zamanlama değerleri | 3.9 | — |

### 3.2 Çerçeve, kimlikler ve dialect

**MAVLink 2 çerçevesi** (birincil kaynak: [MAVLink serileştirme](https://mavlink.io/en/guide/serialization.html), 2026-11-19'da okundu):

| Bayt | Alan | Boy | A1'deki değeri |
|---|---|---:|---|
| 0 | `magic` | 1 | `0xFD` |
| 1 | `len` | 1 | Yük boyu (kırpılmış), 1–255 |
| 2 | `incompat_flags` | 1 | 0 (imza yok) |
| 3 | `compat_flags` | 1 | 0 |
| 4 | `seq` | 1 | Gönderenin çerçeve sayacı, mod 256 |
| 5 | `sysid` | 1 | 3.1 |
| 6 | `compid` | 1 | 3.1 |
| 7–9 | `msgid` | 3 | 24 bit, küçük uçlu |
| 10 … `len`+9 | yük | `len` | Alanlar boy sırasıyla (aşağıda) |
| `len`+10, +11 | `checksum` | 2 | CRC-16/MCRF4XX, düşük bayt önce |

- İmzasız çerçevenin ek yükü 12 B'dır; en büyük yük 255 B, en büyük imzasız çerçeve 267 B'dır (aynı kaynak; imzalı çerçeve 13 B daha uzundur, 280 B).
- **Alan sırası:** yükteki alanlar tip boyuna göre büyükten küçüğe dizilir (8, 4, 2, 1 bayt); aynı boydaki alanlar XML sırasını korur; dizi alanları eleman tipinin boyuna göre sıralanır; çok baytlı alanlar küçük uçludur (aynı kaynak). A1 dialect'inde uzantı alanı (`<extensions/>`) yoktur.
- **Sağlama:** CRC-16/MCRF4XX (`poly 0x1021`, `init 0xFFFF`, yansıtılmış giriş ve çıkış, son XOR 0; "123456789" için 0x6F91; birincil kaynak: [CRC RevEng katalogu](https://reveng.sourceforge.io/crc-catalogue/16.htm), 2026-11-19'da okundu). `magic` hariç başlık ve yük baytları üzerinden hesaplanır, sonuna mesaja özgü `CRC_EXTRA` baytı eklenir; `CRC_EXTRA` iletilmez (MAVLink serileştirme). Bu işte denetim değeri üretilmiş C başlıklarının `crc_calculate` işleviyle 0x6F91 olarak elde edildi.
- **`CRC_EXTRA`:** mesaj adı ve boşluk, ardından tel sırasındaki her temel alan için tip adı ve boşluk, alan adı ve boşluk, dizi alanında dizi boyu baytı üzerinden aynı CRC; 16 bitlik sonucun iki baytı XOR'lanır (MAVLink serileştirme). Her mesajın değeri Ek B'dedir; üreteçle ve bağımsız hesapla aynı bulundu.
- **Yük kırpma:** gönderen yükün sonundaki sıfır baytları atar ve `len`'i buna göre yazar; yükün ilk baytı hiç kırpılmaz; alıcı eksik kısmı sıfırla doldurur ve kırpılmamış çerçeveyi de kabul eder (MAVLink serileştirme).
- **İmza kullanılmaz.** Gerekçe: A1 tek bir yerel kablo (ST-LINK sanal seri portu) ya da aynı makinedeki geri döngü soketidir; araya giren üçüncü bir taraf modellenmez; imza 13 B ek yük ve kartta SHA-256 hesabı getirir (MAVLink serileştirme: imza bloğu 13 B). Bütünlük CRC ile, oturum bağlılığı el sıkışma nonce'ları ve `sess` ile sağlanır (Bölüm 4.2).
- **MAVLink 1 kullanılmaz** (mesaj kimlikleri 255'ten büyüktür; MAVLink 1 yalnız 0–255 kimlikleri taşır; birincil kaynak: [MAVLink XML tanımı](https://mavlink.io/en/guide/define_xml_element.html), 2026-11-19'da okundu).

**Dialect.** `protocol/ida.xml` 37 özel mesaj ve 30 numaralandırma tanımlar. **Standart MAVLink mesajı kullanılmaz ve `common.xml` içerilmez.** Gerekçe:

1. A1'in her mesajı, standart mesajlarda bulunmayan bir alan ister: parti üyeliği ve oturum (`sess`, `batch_seq`), sensör başına örnek sayacı ve aralık süresi (`seq`, `window_ns`), komut kimliği (`cmd_id`; A4-K43, A4-K48), olay kimliği ve son geçerlilik (A4-K60–K66). Standart bir mesajın anlamını bükmeden bu alanlar eklenemez (Ö2; 00 belgesi 06.1–06.3, 08.10). Örnekler: `HIL_SENSOR` aynı paketteki güncellemelere tek zaman damgası verir (06.3); `HIL_ACTUATOR_CONTROLS` normalleştirilmiş değer taşır, kullanım süresi ve olay kimliği yoktur; `COMMAND_LONG`/`COMMAND_ACK` çifti komut kimliği taşımaz; görev protokolü (`MISSION_COUNT` → `MISSION_REQUEST_INT`) aracın istediği öğeleri tek tek çeker, A4'ün atomik `MISSION_BEGIN/ITEM/END` yüklemesiyle aynı anlamda değildir (A4-K51); `PARAM_SET` değeri yazar ve kalıcı belleğe kaydeder ("write new value to permanent storage"), A4'te ise bekletilir, `PARAM_COMMIT` ile uygulanır ve ayrı bir `PARAM_SAVE` ile kaydedilir (A4-K81, K82). Bu mesajların alan ve açıklamaları pymavlink 2.4.50 ile gelen `common.xml`'den okundu (ör. `COMMAND_ACK` alanları: `command`, `result`, `progress`, `result_param2`, `target_system`, `target_component`, `time_boot_ms`; komut kimliği yok).
2. Standart numaralandırmalar (`MAV_SEVERITY`, `GPS_FIX_TYPE`) için `common.xml`'i içermek denendi: pymavlink 2.4.50 ile üretilen `common/common.h`, ISO C17'de `int` aralığını aşan numaralandırıcılar (`MAV_SYS_STATUS_EXTENSION_USED = 2147483648`, `HIL_SENSOR_UPDATED_RESET = 2147483648`) içerdiği için `-std=c17 -pedantic-errors` ile derlenmedi (Bölüm 7.3). Bu yüzden önem düzeyi ve GNSS fix türü dialect'te A4 değerleriyle tanımlandı (`IDA_SEVERITY` değerleri `MAV_SEVERITY` ile aynı sıradadır; A4 3.8).
3. Harici yer istasyonu ilk sürümde yoktur (iskelet §3, §10 md. 17); ileride eklenirse sim'deki ağ geçidi standart mesajları A1 mesajlarına çevirir, A1 değişmez.

**Mesaj kimlikleri.** 46000–46099 aralığı kullanılır. Bu aralık, pymavlink 2.4.50 ile gelen 17 dialect dosyasının (`common`, `ardupilotmega`, `ASLUAV`, `cubepilot`, `storm32`, `icarous`, `uAvionix` ve diğerleri) hiçbirinde kullanılmıyor (bu işte dosyalardan tarandı). MAVLink belgesi özel dialect'ler için aralığı serbest bırakır ve 0–255'in kullanılmamasını ister (MAVLink XML tanımı). Kimlik tahsisi: 46000–46009 oturum, 46010 zaman, 46020–46030 sensör ve sonar, 46035–46036 algılama, 46040–46046 sim → kart komut ve olay sonucu, 46060–46070 kart → sim.

**`dialect_hash`.** `protocol/ida.xml`'den kurulan kanonik metnin SHA-256 özetinin ilk 8 baytı, küçük uçlu `uint64`. Kanonik metin ASCII'dir, her satır `\n` ile biter ve şu sırayla kurulur:

1. Her mesaj için, kimliğe göre artan sırada: `M <id> <ad> <CRC_EXTRA> <yük_azami>` satırı; ardından tel sırasındaki her alan için `F <tip> <ad> <dizi_boyu>` (dizi değilse 0); tip dizi işaretsiz yazılır (`float`, `uint8_t`).
2. Her numaralandırma için, ada göre artan sırada (bayt karşılaştırması), değere göre artan sırada her girdi: `E <numaralandırma> <girdi> <ondalık değer>`.

Bu adayın değeri `0xAEA95FC1D32CA393`'tür (533 satır; bu işte standart kitaplıkla hesaplandı). Değer, XML'deki her tel ya da numaralandırma değişikliğinde değişir; derleme betiği değeri üretir ve sim el sıkışmada iki tarafın derlenmiş değerini karşılaştırır (A1-K14).

### 3.3 Mesaj listesi

Boylar MAVLink kırpması olmadan en büyük değerlerdir (yük / çerçeve, bayt; üreteçten). "Sınıf" sütunu sim → kart üyelerinin pencere sınıfıdır (A1-K35, A1-K36).

| Kimlik | Ad | Yön | Hız | Yük / çerçeve | Sınıf |
|---|---|---|---|---:|---|
| 46000 | `IDA_SIM_HELLO` | sim → kart | El sıkışmada 100 ms'de bir | 22 / 34 | oturum |
| 46001 | `IDA_BOARD_HELLO` | kart → sim | Oturumsuz 200 ms'de bir; yanıt | 82 / 94 | oturum |
| 46002 | `IDA_SESSION_ACCEPT` | sim → kart | El sıkışma sonucu | 16 / 28 | oturum |
| 46003 | `IDA_KEEPALIVE` | iki yön | Başka çerçeve yoksa 100 ms'de bir | 3 / 15 | oturum |
| 46004 | `IDA_SESSION_CLOSE` | iki yön | Kapanışta | 3 / 15 | oturum |
| 46010 | `IDA_TIME_FRAME` | sim → kart | Periyot başına 1 | 17 / 29 | parti sonu |
| 46020 | `IDA_IMU` | sim → kart | Periyot başına 1 | 47 / 59 | kritik |
| 46021 | `IDA_MAG` | sim → kart | A5 (05: 100 Hz) | 27 / 39 | kritik |
| 46022 | `IDA_BARO` | sim → kart | A5 (05: 25 Hz) | 27 / 39 | kritik |
| 46023 | `IDA_GNSS` | sim → kart | A5 (05: 5 Hz) | 49 / 61 | kritik |
| 46024 | `IDA_DVL` | sim → kart | A5 (05: 2–15 Hz) | 77 / 89 | kritik |
| 46025 | `IDA_PING1D` | sim → kart | A5 (05: ≤ 20 Hz) | 36 / 48 | kritik |
| 46026 | `IDA_BATT` | sim → kart | A5 (05: 10 Hz) | 31 / 43 | kritik |
| 46027 | `IDA_LEAK` | sim → kart | A5 (05: 10 Hz) | 16 / 28 | kritik |
| 46028 | `IDA_SAFETY` | sim → kart | 10 Hz ve değişimde; partide ≤ 1 | 18 / 30 | kritik |
| 46029 | `IDA_PING360_HEAD` | sim → kart | Tarama satırı başına 1 | 27 / 39 | toplu |
| 46030 | `IDA_PING360_DATA` | sim → kart | Satır başına `⌈n/240⌉` | 246 / 258 | toplu |
| 46035 | `IDA_DET_BATCH` | sim → kart | Kamera karesi başına 1 (≤ 30 Hz); kamera kapalıyken 1 Hz | 27 / 39 | atanmış |
| 46036 | `IDA_DET_RECORD` | iki yön | Parti başına 0–4 | 78 / 90 | atanmış |
| 46040 | `IDA_CMD_SIMPLE` | sim → kart | Olay güdümlü | 10 / 22 | kritik (daraltıcı) ya da toplu |
| 46041 | `IDA_CMD_MANUAL` | sim → kart | Olay güdümlü | 31 / 43 | toplu |
| 46042 | `IDA_CMD_GUIDED` | sim → kart | Olay güdümlü | 44 / 56 | toplu |
| 46043 | `IDA_CMD_MISSION_ITEM` | sim → kart | Olay güdümlü | 47 / 59 | toplu |
| 46044 | `IDA_CMD_PARAM_SET` | sim → kart | Olay güdümlü | 14 / 26 | toplu |
| 46045 | `IDA_CMD_FC` | sim → kart | Olay güdümlü | 17 / 29 | kritik (daraltıcı) ya da toplu |
| 46046 | `IDA_EVENT_RESULT` | sim → kart | Olay sonucu başına 1 | 19 / 31 | kritik |
| 46060 | `IDA_ACTUATOR` | kart → sim | Çekirdek çağrısı başına ≤ 1 | 34 / 46 | — |
| 46061 | `IDA_EVENT` | kart → sim | Olay başına 1 + yinelemeler | 37 / 49 | — |
| 46062 | `IDA_CMD_RESULT` | kart → sim | Komut başına 1 | 27 / 39 | — |
| 46063 | `IDA_NAV` | kart → sim | 50 Hz | 118 / 130 | — |
| 46064 | `IDA_STATUS` | kart → sim | 10 Hz | 36 / 48 | — |
| 46065 | `IDA_FC_STATUS` | kart → sim | 10 Hz | 79 / 91 | — |
| 46066 | `IDA_LOG` | kart → sim | Olay güdümlü (çağrı başına ≤ 4) | 19 / 31 | — |
| 46067 | `IDA_CORE_COUNTERS` | kart → sim | 1 Hz | 72 / 84 | — |
| 46068 | `IDA_LINK_STATS` | kart → sim | 1 Hz | 144 / 156 | — |
| 46069 | `IDA_CALL_TRACE` | kart → sim | Çekirdek çağrısı başına 1 | 133 / 145 | — |
| 46070 | `IDA_FAULT_RECORD` | kart → sim | Oturum kurulunca, varsa 1 | 35 / 47 | — |

Alanların tamamı, tipleri, birimleri ve numaralandırmaları `protocol/ida.xml`'dedir. Aşağıdaki tablolar her alanın geçerli aralığını, A4 karşılığını ve geçersiz değerde davranışı verir. "Z" sütunu: **Z** her durumda anlamlı; **K** koşullu; **B** bilgi (alıcının davranışını değiştirmez). "Geçersizde" sütunundaki "örnek VALID = 0" kuralı A1-K47'dedir.

### 3.4 Sensör mesajları (sim → kart) ve A4 eşlemesi

**Ortak sensör alanları** (IMU, MAG, BARO, GNSS, DVL, PING1D, BATT, LEAK, SAFETY ve PING360_HEAD'de):

| Alan | Tip | Birim | Aralık | Z | A4 karşılığı | Geçersizde |
|---|---|---|---|---|---|---|
| `t_meas_ns` | `int64_t` | ns | 0 … 2⁶² | Z | `hdr.t_meas.ns` (aynen; A4-K13) | Çekirdek reddeder (A4-K33) |
| `seq` | `uint32_t` | — | Sensör akışında seri aritmetikle artan | Z | `hdr.seq` | Çekirdek reddeder |
| `window_ns` | `uint32_t` | ns | IMU, BARO, BATT'ta > 0 | Z | `hdr.window_ns`; diğer türlerde mesajda alan yoktur ve dönüştürücü 0 yazar | Çekirdek reddeder |
| `flags` | `uint16_t` | bit | `IDA_SAMPLE_FLAGS` (A4 `IDA_HF_*`) | Z | `hdr.flags` (aynen) | Bilinmeyen bitler aynen geçer; `VALID` = 0 ise çekirdek reddeder |
| `inst` | `uint8_t` | — | 0–3 | Z | `hdr.inst` | Çekirdek reddeder |
| (msgid) | — | — | — | Z | `hdr.type` (Tablo 3.4b) | — |
| — | — | — | — | Z | `hdr.t_rx` = kartın alım damgası (A1-K48) | — |

**Tablo 3.4b — Tür eşlemesi ve türe özgü alanlar.** Değerler A4 yapısına aynı ad ve birimle, dönüşümsüz kopyalanır (istisnalar belirtilmiştir).

| Mesaj | `hdr.type` | Türe özgü alanlar → A4 | Dönüşüm |
|---|---|---|---|
| `IDA_IMU` | `IDA_SENS_IMU` (1) | `gyro[3]`, `accel[3]`, `temp_c` → `ida_imu_t` | yok; `in.imu`'ya gider (A4-K17) |
| `IDA_MAG` | `IDA_SENS_MAG` (2) | `field[3]` (T) → `ida_mag_t.field` | yok |
| `IDA_BARO` | `IDA_SENS_BARO` (3) | `p_abs` (Pa), `temp_c` → `ida_baro_t` | yok |
| `IDA_GNSS` | `IDA_SENS_GNSS` (4) | `lat_e7`, `lon_e7`, `alt_mm`, `vel_ned[3]`, `eph`, `epv`, `fix_type`, `n_sv` → `ida_gnss_t` | yok (`IDA_GNSS_FIX` değerleri A4 `IDA_GNSS_FIX_*` ile aynıdır); `fix_type` > 4 ise örnek VALID = 0 |
| `IDA_DVL` | `IDA_SENS_DVL` (5) | `vel[3]`, `cov[6]`, `fom`, `beam_dist[4]`, `altitude`, `dvl_flags`, `beam_valid` → `ida_dvl_t` | yok |
| `IDA_PING1D` | `IDA_SENS_PING1D` (6) | `distance`, `scan_start`, `scan_length`, `ping_number`, `tx_offset_ns`, `confidence` → `ida_ping1d_t` | yok |
| `IDA_BATT` | `IDA_SENS_BATT` (8) | `voltage`, `current`, `temp_c` → `ida_batt_t` | yok |
| `IDA_LEAK` | `IDA_SENS_LEAK` (9) | `mask` → `ida_leak_t` | yok |
| `IDA_SAFETY` | `IDA_SENS_SAFETY` (10) | `estop`, `tube[2]` (`LOADED`, `PLUG_IN`, `PIN_OUT`, `PERMIT_OPEN`) → `ida_safety_t` | yok; `estop` > 1 ise örnek VALID = 0 |
| `IDA_PING360_HEAD` + `DATA` | `IDA_SENS_PING360` (7) | `angle_grad`, `n_samples`, `sample_period`, `tx_duration_us`, `tx_freq_khz`, `gain` ve birleştirilmiş `data[n_samples]` → `ida_ping360_t` | Birleştirme (A1-K49–K53); `window_ns` = 0 |

Sim'in alan aralıkları (gönderen kuralı; alıcı aralık dışı değeri çekirdeğe bırakır, A4-K33 sonluluğu ve bayrakları denetler): IMU `window_ns` = `P` (sabit periyotlu IMU; 05 §2.2); BARO `window_ns` > 0; BATT `window_ns` > 0; GNSS `eph`, `epv` ≥ 0; DVL `beam_valid` ≤ 0x0F; PING1D `confidence` ≤ 100; bütün `float` alanlar sonlu (sim sonlu olmayan değer üretmez; hata enjeksiyonu hariç, Bölüm 5).

**PING360_HEAD'e özgü aralıklar** (cihaz sınırları; 05 §2.4.5, D05 #1a–#1c "DOĞRU"; üst sınır 1200 iki kademede de aynıdır):

| Alan | Aralık | Kartın davranışı aralık dışında |
|---|---|---|
| `angle_grad` | 0–399 | Satırı atar, `ping360_bad` sayar |
| `n_samples` | 200–1200 | Aynı |
| `sample_period` (25 ns) | 80–40 000 | Aynı |
| `n_frag` | `⌈n_samples / 240⌉` (1–5) | Aynı |
| `tx_duration_us`, `tx_freq_khz`, `gain` | Her değer (port daraltmaz; A4 3.3) | — |

**PING360_DATA:** `seq` (satırın HEAD.`seq`'i), `inst`, `frag_idx` (0 … `n_frag − 1`), `data[240]`. Parça `k`, `[240k, 240k + 239]` örneklerini taşır; son parçada geçerli örnek sayısı `n_samples − 240(n_frag − 1)`'dir ve kullanılmayan kuyruk 0'dır.

### 3.5 Algılama mesajları ve A8 eşlemesi

`IDA_DET_BATCH` ve `IDA_DET_RECORD`, A8'deki `ida_det_batch_t` ve `ida_det_record_t`'nin `reserved` dışındaki bütün alanlarını aynı ad, tip ve bit değeriyle taşır (A8 3.2, 3.3). Tel sırası MAVLink kuralıyla belirlenir (3.2); A8 bunu serbest bırakır (A8 "A1'den beklenen" 1). Yük boyları A8'in kodlanmış üst sınırlarına eşittir: parti başlığı 27 B, kayıt 78 B (A8 `IDA_DET_BATCH_ENCODED_MAX`, `IDA_DET_RECORD_ENCODED_MAX`).

| Mesaj alanı | A8 alanı | Dönüşüm |
|---|---|---|
| Her alan | Aynı adlı alan | Yok (`float` IEEE-754 binary32 bitleri, tamsayılar küçük uçlu) |
| — | `reserved[]` | Taşınmaz; alıcı 0 yazar |
| — | Alım zamanı | Kartın alım damgası; port girdisinde `det_t_rx[]`, `det_batch_t_rx[]` (A4-K41; A8-K14) |

Kart algılama alanlarını doğrulamaz ve değiştirmez; doğrulama çekirdeğin A8-K29 kuralıdır.

### 3.6 Komutlar (sim → kart) ve A4 eşlemesi

**Ortak komut alanları:** `cmd_id` (`uint32_t`; A4 `ida_cmd_t.cmd_id`), `sess` (`uint16_t`; güncel oturum), `source` (`uint8_t`; A4 `ida_cmd_t.source`; panel 255, sim laboratuvarı 250).

| Mesaj | A4 `type` | Alanlar → A4 | Aralık (gönderen kuralı) | Kartın dönüşüm hatası (A1-K64) |
|---|---|---|---|---|
| `IDA_CMD_SIMPLE` | `type` alanı: 1, 2, 3, 4, 7, 9, 10, 12, 13, 14, 15 | `arg` → `SET_MODE`: `u.set_mode.mode`; `MISSION_BEGIN`: `u.mission_begin.count`; `MISSION_CTL`: `u.mission_ctl.action`; `PARAM_GET`: `u.param.id`; diğerlerinde kullanılmaz (0) | `SET_MODE`: 0–7; `MISSION_BEGIN`: 1–128; `MISSION_CTL`: 1–4 | `type` ∉ listedekiler ya da `SET_MODE`'da `arg` > 7: `LINK_BAD_FIELD` |
| `IDA_CMD_MANUAL` | `MANUAL` (5) | `axes[6]` → `u.manual.axes` | [−1, 1] | — (aralığı çekirdek denetler, A4-K49) |
| `IDA_CMD_GUIDED` | `GUIDED_SP` (6) | `mask`, `pos_ne[2]`, `depth`, `altitude`, `vel_ned[3]`, `yaw`, `yaw_rate` → `u.guided` | A4-K50 | — |
| `IDA_CMD_MISSION_ITEM` | `MISSION_ITEM` (8) | `index`, `item_type`, `frame`, `p[4]`, `lat_e7`, `lon_e7`, `north_m`, `east_m`, `depth` → `u.mission_item` | A4-K51 | — |
| `IDA_CMD_PARAM_SET` | `PARAM_SET` (11) | `param_id` → `u.param.id`; `param_type` → `u.param.type`; `value_bits` → `u.param.v` (32 bit aynen) | `param_type` 1–3 | `param_type` ∉ 1–3: `LINK_BAD_FIELD` |
| `IDA_CMD_FC` | `FC` (16) | `op`, `tube`, `arg0`, `arg1` → `u.fc` (opak; W4) | `op` 1–8, `tube` 0–1 | `op` ∉ 1–8: `LINK_BAD_FIELD` |

**Daraltıcılar mandala gider** (A4-K46): `CMD_SIMPLE` `type = DISARM` → `IDA_NARROW_DISARM`; `type = SET_MODE` ve `arg` ∉ {6, 7} → `IDA_NARROW_MODE` (`narrow.mode = arg`); `CMD_FC` `op` = 2 → `IDA_NARROW_FC_DISARM`; `op` = 3 → `IDA_NARROW_FC_ABORT`. Diğerleri `cmds[]`'e girer.

**Komut sınıfları** (A1-K60; A3'ün makinece okuyacağı liste `protocol/ida.xml`'deki `IDA_CMD_TYPE`, `IDA_FC_OP`, `IDA_EVENT_KIND` ve `IDA_SESSION_RESULT` girdilerinin açıklamasında `[sinif: …]` biçimindedir):

| Komut | A1 mesajı | Sınıf | Dayanak |
|---|---|---|---|
| HEARTBEAT | `CMD_SIMPLE` | nötr | A4 3.4 |
| ARM | `CMD_SIMPLE` | **genişletir** | A4-K104 |
| DISARM | `CMD_SIMPLE` | **daraltır** | A4-K44 |
| SET_MODE (hedef GUIDED ya da AUTO) | `CMD_SIMPLE` | **genişletir** | A4-K104 |
| SET_MODE (diğer hedef) | `CMD_SIMPLE` | **daraltır** | A4-K44 |
| MANUAL, GUIDED_SP | `CMD_MANUAL`, `CMD_GUIDED` | nötr | A4 3.4 |
| MISSION_BEGIN, MISSION_ITEM, MISSION_END, MISSION_CTL | `CMD_SIMPLE`, `CMD_MISSION_ITEM` | nötr | A4 3.4 |
| PARAM_SET, PARAM_GET, PARAM_COMMIT, PARAM_SAVE | `CMD_PARAM_SET`, `CMD_SIMPLE` | nötr | A4 3.4 |
| FAILSAFE_RESET | `CMD_SIMPLE` | **genişletir** | A4-K104 |
| FC: ARM, SELECT, FIRE, RESET, TEST_ENTER | `CMD_FC` | **genişletir** | A4-K104 |
| FC: DISARM, ABORT | `CMD_FC` | **daraltır** | A4-K44 |
| FC: TEST_EXIT | `CMD_FC` | nötr | A4 3.4 |
| Kartı planlı yeniden başlat (`SESSION_ACCEPT` sonucu `REBOOT`) | `IDA_SESSION_ACCEPT` | **daraltır** | A1-K17 |
| Oturumu kapat | `IDA_SESSION_CLOSE` | **daraltır** (kartta `SESSION_END`; A4-K73) | A1-K21 |
| Sanal acil durdurma etkin / kalktı | A1 mesajı değildir (sim durumu); etkisi `IDA_SAFETY` ile | daraltır / genişletir | İskelet §5; A4-K111 |
| Sanal silahlandırma fişi çıkarıldı / takıldı | Aynı | daraltır / genişletir | Aynı |
| Sanal emniyet pimi takıldı / çıkarıldı | Aynı | daraltır / genişletir | Aynı |

### 3.7 Kart → sim mesajları ve A4 eşlemesi

**`IDA_ACTUATOR`** (bir çekirdek çağrısının sürekli komutu):

| Alan | Tip | Birim | Aralık | Z | A4 kaynağı |
|---|---|---|---|---|---|
| `batch_seq` | `uint32_t` | — | tümü | Z | `out.seq` (A4-K57) |
| `valid_for_ns` | `uint32_t` | ns | `THR_VALID` = 1 ise 1 ms – 1 s; değilse 0 | Z | `out.valid_for_ns` (A4-K58) |
| `pwm_us[8]` | `uint16_t` | µs | `THR_VALID` = 1 ise A5'teki ESC alt–üst sınırı; değilse 0 | K | `out.thr_cmd[i]`'nin platformda ESC darbe genişliğine çevrilmesi (A4-K54); dizin A5 itici sırasıdır |
| `sess` | `uint16_t` | — | güncel oturum | Z | — |
| `act_flags` | `uint16_t` | bit | `IDA_ACT_FLAGS` | Z | `THR_VALID` ← `out.thr_valid`; `IS_SAFE` ← `out.is_safe`; `ARMED` ← `out.armed`; `SRC_TICK` ← `out.src = IDA_SRC_TICK`; `ASYNC` ← ızgara tick'i (A1-K44); `INPUT_DROPPED` ← `out.health.input_dropped`; `NARROWED` ← `out.health.narrowed`; `OUT_FIXED` ← platformun A4-K59 değişikliği; `BATCH_INCOMPLETE` ← A1-K30 |
| `rx_to_tx_us` | `uint16_t` | µs | 0–65 535 (doyar) | B | Platform: `tf.t_rx`'ten bu çerçevenin gönderim kuyruğuna verilmesine kadar geçen duvar süresi |
| `fs_level` | `uint8_t` | — | 0–5 | Z | `out.fs_level` |
| `core_rc` | `uint8_t` | — | `IDA_CORE_RC` | Z | Çağrının dönüş kodu |
| `n_events` | `uint8_t` | — | 0–4 | Z | `out.n_events` içinden ilk kabul kapısından geçen yeni olay sayısı (A1-K73) |
| `n_merged` | `uint8_t` | — | 0–255 | Z | Bu çağrıya birleştirilen önceki parti sayısı (A4-K102; `plat.batch_merged`) |

**`IDA_EVENT`** (A4 `ida_event_t`; A4-K60–K66): `event_id` (u64), `t_decide_ns` (`t_decide.ns`), `batch_seq` (`seq`), `apply_by_ns`, `hold_ns`, `fc_ref`, `kind` (`IDA_EVENT_KIND` = A4 `IDA_EVT_*`), `tube` (0–1; `SAFETY_CLOSE`'ta 255 = `IDA_TUBE_ALL`), `sess` ve olayı çıkaran çağrının türü `call_kind` (`IDA_CALL_KIND`; A4'te karşılığı yok, platform doldurur). A4 alanları dönüşümsüz kopyalanır. **Olay kimliğinin bit düzeni:** `event_id = (boot_count << 32) | ctr` (A4-K61); telde küçük uçlu 64 bit tamsayıdır: yükteki ilk 4 bayt `ctr`, sonraki 4 bayt `boot_count`'tur.

**`IDA_EVENT_RESULT`** (sim → kart; A4 `ida_event_result_t`): `event_id`, `result` (`IDA_EVENT_RESULT_CODE`; sim yalnız 1–6 gönderir), `t_apply_ns` (`APPLIED` ve `DUPLICATE`'te uygulandığı sim anı; diğerlerinde 0), `sess`. Kart bunu `in.evres[]`'e koyar (A4-K68).

**`IDA_CMD_RESULT`** (A4 `ida_cmd_result_t`): `cmd_id`, `kind` (A4 `IDA_RES_*`), `ack` (A4 `IDA_ACK_*`), `reason` (A4 `IDA_REASON_*` ya da A1 128–131), `param_id`, `param_type`, `value_bits` (`kind = PARAM_VALUE`'da `value`'nun 32 biti), `save_gen`, `save_len`, `save_crc` (`kind = PARAM_SAVE` ve `ACCEPTED`'te: `save.gen`, `save.len` ve platformun kaydettiği bloğun CRC-32/ISO-HDLC değeri; blok A1'de taşınmaz), `sess`.

**`IDA_NAV`** (A4 `ida_nav_out_t`; aynı ad ve birimler): `t_ns`, `batch_seq`, `p_ned[3]`, `v_ned[3]`, `q_nb[4]` (w, x, y, z; Hamilton; gövde → NED), `omega_b[3]`, `depth`, `altitude`, `p_sigma[3]`, `v_sigma[3]`, `att_sigma[3]`, `origin_lat_e7`, `origin_lon_e7`, `est_level`, `reset_counter`.

**`IDA_STATUS`**: `boot_count`, `fs_reasons`, `mission_item`, `mode`, `armed`, `fs_level`, `regime`, `phase`, `mission_state`, `param_ok`, `events_ok` (A4 `ida_status_t`); `health` (A4 `ida_health_t`'nin altı biti, `IDA_HEALTH`); `link_state` (kartın A1 durumu); `param_store`, `param_fail`, `param_gen` (A4 `ida_plat_t`); `param_blob_crc` (kalıcı bloğun CRC-32/ISO-HDLC değeri); `batch_seq` (son çıktının `seq`'i); `sess`.

**`IDA_FC_STATUS`** (A4 `ida_fc_status_t`; aynı adlar): `state`, `inhibit_mask`, `last_event_id`, `last_event_result`, `ext_len` (0–64), `ext[64]` (opak; biçimi W4'ün; A1 yorumlamaz; `ext_len`'den sonraki baytlar 0).

**`IDA_LOG`** (A4 `ida_log_t`): `t_w_ns`, `code`, `severity` (`IDA_SEVERITY` = A4 `IDA_SEV_*`), `arg0`, `arg1`.

**`IDA_CORE_COUNTERS`**: `cnt[18]` = A4 `ida_core_counters_t`'nin 18 alanı, yapıdaki sırayla (`sample_rejected` … `step_miss`).

**`IDA_LINK_STATS`**: `cnt[36]`, sıra ve tanımlar Tablo 3.11b'de.

**`IDA_CALL_TRACE`** (her çekirdek çağrısının kart tarafı girdileri; A1-K95):

| Alan | Tip | Birim | Anlam |
|---|---|---|---|
| `now_w_ns` | `int64_t` | ns | Çağrının `in.now_w` |
| `batch_seq` | `uint32_t` | — | `in.tf.seq`; ızgara tick'inde son kabul edilen |
| `bg_slices` | `uint32_t` | — | `in.bg_slices` (A4-K109) |
| `prev_call_ns` | `uint32_t` | ns | `plat.prev_call_ns` |
| `rx_age_ns[27]` | `uint32_t` | ns | `now_w − t_rx`, sırayla: `tf`, `imu`, `samples[0..15]`, `det_batch[0..1]`, `det[0..7]`; girdide olmayan öğe 0; 2³² − 1'de doyar |
| `n_rx` | `uint8_t` | — | `rx_age_ns`'te anlamlı öğe sayısı (son anlamlı dizinden bir fazlası) |
| `call_kind` | `uint8_t` | — | `IDA_CALL_KIND`: STEP, TICK_TF, TICK_GRID |
| `core_rc` | `uint8_t` | — | Dönüş kodu |
| `plat_flags` | `uint8_t` | bit | `IDA_PLAT_FLAGS` (`prev_overrun`, `tx_stalled`, `batch_merged`, `input_dropped`, `SESSION_START`, `SESSION_END`) |
| `n_merged` | `uint8_t` | — | Birleştirilen önceki parti sayısı |

**`IDA_FAULT_RECORD`** (01 §2.8): `cfsr`, `hfsr`, `mmfar`, `bfar`, `pc`, `lr`, `xpsr`, `step_counter`, `fault_seq`, `stacking_valid`. Yalnız bilgi.

### 3.8 Oturum mesajları

**`IDA_SIM_HELLO`**: `dialect_hash`, `nonce` (≠ 0), `run_id`, `ctrl_period_ns` (5 000 000), `proto_major` (1), `proto_minor` (0).

**`IDA_BOARD_HELLO`** (açılış kaydını taşır):

| Alan | Anlam (A4 kaynağı) |
|---|---|
| `dialect_hash`, `proto_major`, `proto_minor` | Kartın derlendiği A1 dialect'i |
| `ctrl_period_ns` | Kartın derlendiği kontrol periyodu (5 000 000) |
| `peer_nonce` | Yanıtlanan `SIM_HELLO.nonce`; duyuruda 0 |
| `nonce` | Kartın bu el sıkışma için ürettiği sayı (≠ 0) |
| `role` | `IDA_ROLE_BOARD` ya da `IDA_ROLE_SIL` |
| `boot_count`, `boot_count_ok`, `reset_cause`, `reset_flags`, `consecutive_faults`, `param_src` | `ida_init_in_t`'nin aynı adlı alanları |
| `init_rc` | `ida_core_init`'in dönüşü (`OK` ya da `PARAM_DEFAULTS`) |
| `port_major`, `port_minor`, `param_schema_hash`, `core_version` | `ida_core_info_t` |
| `det_schema_major`, `det_schema_minor` | A8 `IDA_DET_SCHEMA_*` |
| `fw_version`, `fw_git` | Kart yazılımı (ya da SIL koşucusu) sürümü ve commit özetinin ilk 32 biti |
| `param_blob_crc`, `param_blob_len`, `param_gen` | Kalıcı parametre bloğunun tamamının CRC-32/ISO-HDLC değeri, boyu (yoksa 0, 0) ve kuşağı |
| `last_cmd_id` | Bu açılışta çekirdeğe verilen son `cmd_id` (yoksa 0; A1-K66) |
| `prev_sess`, `sessions_since_boot` | Bu açılıştaki son oturum kimliği (yoksa 0) ve kurulmuş oturum sayısı |

**`IDA_SESSION_ACCEPT`**: `run_id`, `nonce_sim`, `nonce_board`, `sess` (OK'de ≠ 0), `result` (`IDA_SESSION_RESULT`), `run_mode` (`IDA_RUN_MODE`; bilgi).

**`IDA_KEEPALIVE`**: `sess` (oturum yoksa 0), `state` (`IDA_LINK_STATE`). **`IDA_SESSION_CLOSE`**: `sess`, `reason` (`IDA_CLOSE_REASON`).

**`IDA_TIME_FRAME`**:

| Alan | Tip | Birim | Aralık | Z | A4 karşılığı |
|---|---|---|---|---|---|
| `t_sim_ns` | `int64_t` | ns | 0 … 2⁶²; oturumda kesin artan | Z | `tf.t` |
| `batch_seq` | `uint32_t` | — | Oturumda seri aritmetikle bir artan | Z | `tf.seq` |
| `sess` | `uint16_t` | — | Güncel oturum | Z | — |
| `n_members` | `uint8_t` | — | 0–255 | Z | — (A1-K30) |
| `tf_flags` | `uint8_t` | bit | `IDA_TF_FLAGS` | Z | `IMU`: partide IMU var (A1-K30) |
| `run_mode` | `uint8_t` | — | `IDA_RUN_MODE` | B | — (A1-K24) |
| — | — | — | — | Z | `tf.present = 1`; `tf.t_rx` = TF'nin alım damgası (A1-K48) |

### 3.9 Zamanlama değerleri ve eşitsizlikler

| Ad | Değer | Saat | Dayanak | Kurallar |
|---|---|---|---|---|
| `P` | 5 ms | sim | İskelet §4 | A1-K29 |
| `L_act` | 10 ms (8 fizik adımı) | sim | İskelet §4 | A1-K74 |
| `T_bekle` (bariyerde bekleme sınırı) | 50 ms | sim duvarı | İskelet §4 başlangıç değeri; mimari seçimi (tahmin) | A1-K75, K84 |
| `T_kullanım` (sürekli komutun kullanım süresi; çekirdek parametresi `valid_for_ns`) | 100 ms | sim (uygulama anından) | Mimari seçimi (tahmin) | A1-K74, K77; A4-K58 |
| `T_bağ` (bağlantı zaman aşımı; iki taraf) | 500 ms | her tarafın kendi duvarı | Mimari seçimi (tahmin; 06 §2.8'deki eş canlılığı süresi) | A1-K22, K23 |
| `T_canlı` | 100 ms | duvar | Mimari seçimi | A1-K20, K90 |
| `T_hello` | 100 ms | sim duvarı | Mimari seçimi | A1-K12 |
| `T_duyuru` | 200 ms | kart duvarı | Mimari seçimi (06 §2.9) | A1-K11 |
| `T_hazır` | 1 s | sim duvarı | Mimari seçimi (06 §2.9) | A1-K27 |
| `T_reboot` | 5 s | sim duvarı | Mimari seçimi (06 §2.9) | A1-K17 |
| `T_SIL` | 5 s | sim duvarı | Mimari seçimi (06 §2.5) | A1-K76 |
| `T_emniyet` | 100 ms (10 Hz) | sim | 05 §2.3.8 (tahmin) | A1-K56 |
| TF yazımları arası alt sınır | `P/2` = 2,5 ms | sim duvarı | Mimari seçimi | A1-K39 |
| `T_olağan` | 2,5 ms | kart duvarı | Mimari seçimi | A1-K90 |
| `T_olay_yin` (olay yineleme aralığı) | 15 ms | kart duvarı | Mimari seçimi (tahmin) | A1-K80 |
| `apply_by_ns` (atış kontrol parametresi, W4) | Alt sınır 15 ms; varsayılan 20 ms | sim (uygulama) | Mimari seçimi (tahmin); A4-K66 aralığı 1 ms – 1 s | A1-K80, K84 |
| `hold_ns` (atış kontrol parametresi, W4) | Varsayılan 200 ms; yenileme ≤ `hold_ns / 2` (A4-K107) | sim (uygulama), kart duvarı (üretici) | Mimari seçimi (tahmin) | A1-K86 |
| `T_sonuç_pay` | 75 ms | kart duvarı | Mimari seçimi; E8 | A1-K80 |
| Ping360 yetim yuva süresi | 32 parti kapanışı | kart | Mimari seçimi; E12 | A1-K51 |

**Zorunlu eşitsizlikler** (her biri bu değerlerle sağlanır; değerlerden biri değişirse eşitsizlikler yeniden sınanır):

| # | Eşitsizlik | Bu değerlerle | Gerekçe |
|---|---|---|---|
| E1 | `P < L_act` | 5 < 10 | İskelet §4 |
| E2 | `T_bekle < T_kullanım < T_bağ` | 50 < 100 < 500 | Bariyerin tolere ettiği bir gecikme HIL 1x'te iticileri nötrlemez; bağlantı ölmeden önce komut kendiliğinden düşer (iskelet §4 son madde) |
| E3 | `T_kullanım ≥ L_act + P + RTT_maks` | 100 ≥ 10 + 5 + 14,5 = 29,5 | Gözlenen en uzun gidiş-dönüş (14,5 ms; MP3 "Echo profilleri") tek bir yanıtı kaçırtsa bile önceki komut geçerlidir; `T_kullanım / P − 1` = 19 ardışık kaçırılan partiden sonra nötr (hesap) |
| E4 | `4 · T_canlı ≤ T_bağ` | 400 ≤ 500 | Dört ardışık canlılık çerçevesinin kaybı oturumu bitirmez |
| E5 | `T_bağ > RTT_maks` ve `T_bağ > T_bekle` | 500 > 14,5; 500 > 50 | Gözlenen seyrek gecikmeler ve bariyer beklemesi oturumu bitirmez |
| E6 | `apply_by_ns ≥ L_act + P` | 20 ≥ 15 | Zamanında gelen bir olay (`t_apply = t_decide + L_act`) son geçerliliği geçmez; `P` kadar pay |
| E7 | `T_olay_yin < apply_by_ns` | 15 < 20 | Son geçerlilik dolmadan en az bir yineleme gider. Olağan durumda sonuç, kararın kart duvarındaki anından yaklaşık 3–9 ms sonra kartta olur (hesap, tahmini girdilerle: karar `W(t_n)`'den 2,5–7,5 ms sonra; sonuç `a_n = t_{n+2}` partisinin kritik üyesi, `W(t_n) + 10` ms civarında yazılır); 15 ms bundan büyüktür, gereksiz yineleme beklenmez |
| E8 | `T_bekle + P + RTT_maks ≤ T_sonuç_pay < IDA_EVT_RESULT_MARGIN` | 50 + 5 + 14,5 = 69,5 ≤ 75 < 100 | A1-K84'ün duvar sınırıyla uygulanabilen bir olayın sonucu, kart `EXPIRED_NO_ACK` vermeden önce ulaşır; kart da çekirdeğin kendi süresinden (A4-K70) önce sonuç verir |
| E9 | `hold_ns / 2 ≥ L_act + P + RTT_maks` ve `hold_ns < T_bağ` | 100 ≥ 29,5; 200 < 500 | Yenilenen izin süresi dolmadan alıcıya ulaşır; bağlantı kopunca izin oturum bitişinden önce kendiliğinden düşer |
| E10 | `T_imu_stale + IDA_PORT_STALE_MARGIN_NS < T_kullanım` | 20 + 14,5 = 34,5 < 100 | Bilgi: kartın kendi nötr çıktısı sim'deki kullanım süresinden önce gelir (A4-K39) |
| E11 | Gönderen tarafta herhangi bir yarı açık 5 ms'lik pencerede en çok 2 TF | A1-K39 | A4'ün geliş varsayımı (A4 "A1'den beklenen" 8); varış tarafında paketleme nedeniyle sıkışma olabilir (A1-K40) |
| E12 | Ping360 yetim yuva süresi ≥ bir satırın en kötü yayılımı | 32 ≥ 13 | En kötü pencerede Ping360'a kalan bayt `B_pencere − 1 090` = 110 B'dır (E13); 1 329 B'lık satır ≤ ⌈1 329 / 110⌉ = 13 pencereye yayılır (hesap) |
| E13 | `B_pencere ≥` kritik üyelerin en kötüsü + atanmış algılama partisi | 1 200 ≥ 691 + 399 = 1 090 | Kritik ve atanmış üyeler hiçbir koşulda ertelenmez (A1-K35, K38) |

**Uçtan uca bütçe** (HIL, bariyerli ya da 1x; `TF(n)`'nin `W(t_n)`'de yazılmasından sim'in yuva `n`'i `a_n`'de uygulamaya hazır olmasına kadar; **bütün kalemler tahmindir**, uçtan uca ölçülmedi):

| Kalem | En kötü (ms) | Olağan (ms) | Dayanak |
|---|---:|---:|---|
| Sim: TF(n) yazımının `W(t_n)`'den gecikmesi | 0,50 | 0,50 | Mimari seçimi (tahmin) |
| Aşağı yön tel süresi: `W(t_n)`'de üretilen kritik kuyruk (en kötü 691 B; olağan TF + IMU 88 B) | 2,30 | 0,29 | Hesap (bayt × 10 / 3 000 000) |
| Aşağı yön USB + ST-LINK ek gecikmesi | 0,40 | 0,20 | MP3'ten türetilmiş tahmin: 64 B echo p50 / p99,9 0,83 / 1,10 ms eksi iki yönün tel süresi 0,43 ms → iki yön için 0,40 / 0,67 ms |
| Kart: dağıtım | 0,50 | 0,50 | A4-K19 (tahmin) |
| Kart: `ida_core_step` | 3,50 | 3,50 | A4-K23 bütçesi (tahmin; MP4a yer tutucu en kötü 3,32 ms) |
| Kart: çıktı kodlama | 0,10 | 0,10 | 01 §2.4 (tahmin) |
| Yukarı yön tel süresi: 4 olay + ACTUATOR (242 B; olağan 46 B) ve önünde yazımı süren en büyük kritik olmayan çerçeve (156 B) | 1,33 | 0,15 | Hesap; A1-K90 |
| Yukarı yön USB + ST-LINK ek gecikmesi | 0,40 | 0,20 | Yukarıdaki gibi |
| Sim: uygulama adımına hazırlık `C_sim` | 0,40 | 0,40 | 06 §2.5 (tahmin) |
| **Toplam** | **9,43** | **5,84** | `L_act` = 10 ms'ye pay: 0,57 / 4,16 ms |

Bu bütçe MP3'teki seyrek 10–14,5 ms'lik gidiş-dönüşleri karşılamaz; o yanıtlar A1-K75'e göre kaçırılır ya da bariyerde beklenir ve çalıştırma geçersiz işaretlenir (iskelet §4 bunu olağan sayar). Bütçenin gerçek değerleri W3 (gerçek çekirdek, M5) ve W4 (HIL köprüsü; MP5) ölçümleriyle yeniden değerlendirilir (Bölüm 8, O1–O3).

### 3.10 Bant bütçesi

Hesap; boylar Tablo 3.3'teki en büyük çerçeve boylarıdır (kırpma kazancı sayılmaz), hızlar 05 §2.2'deki tahmini varsayılanlardır (A5 değiştirebilir; ön denetim A5 değerleriyle yeniden hesaplar, A1-K34). Tek örnekli (`inst = 0`) sensör seti, tek kamera.

**Sim → kart, periyot başına ortalama (200 Hz)**

| Kalem | Hesap | B / periyot |
|---|---|---:|
| `TIME_FRAME` | 29 × 1 | 29,0 |
| `IMU` | 59 × 1 | 59,0 |
| `MAG` 100 Hz | 39 × 0,5 | 19,5 |
| `BARO` 25 Hz | 39 × 0,125 | 4,9 |
| `GNSS` 5 Hz | 61 × 0,025 | 1,5 |
| `DVL` 15 Hz (en hızlı mod) | 89 × 0,075 | 6,7 |
| `PING1D` 20 Hz | 48 × 0,1 | 4,8 |
| `BATT` 10 Hz | 43 × 0,05 | 2,2 |
| `LEAK` 10 Hz | 28 × 0,05 | 1,4 |
| `SAFETY` 10 Hz + saniyede ≤ 10 değişim (tahmin) | 30 × 0,1 | 3,0 |
| **Kritik sensörler** | | **132,0** |
| Kamera algılaması, 30 Hz, karede 4 kayıt (A8 3.6) | 399 × 0,15 | 59,9 |
| Panel komutları: MANUAL 50 Hz (tahmin) + HEARTBEAT 1 Hz + diğer ≤ 5/s | (43 × 50 + 22 + 59 × 5) / 200 | 12,3 |
| Olay sonuçları, ≤ 20/s (tahmin) | 31 × 0,1 | 3,1 |
| **Ping360 dışı toplam** | | **207,3** (%13,8) |
| Ping360, pencere başına en çok `B_p360` | | ≤ 600,0 |
| **En kötü ortalama toplam** | | **≤ 807,3** (%53,8) |

%60 hedefi (900 B) her durumda sağlanır: Ping360 dışı kalemler 207,3 B, Ping360 payı A1-K34 ile 600 B'ı aşamaz.

**Ping360 çıktısı.** 1 200 örnekli satır 39 + 5 × 258 = 1 329 B, 600 örnekli satır 39 + 2 × 258 + 138 = 693 B'dır (hesap). `B_p360` ile en çok 120 000 B/s: 1 200 örnekte ≈ 90,3 satır/s, 600 örnekte ≈ 173 satır/s. 05'in varsayılanıyla (50 m, açı başına 82,5 ms) ≈ 12,1 satır/s → 80,4 B/periyot. 05'in 7 m örneğinde yalnız akustik bekleme ≈ 107 satır/s'lik üst sınır verir (05 §2.4.5; hesap); 1 200 örnekte bu hız payı aşar ve **seyreltilen Ping360 satırlarıdır** (A1-K37; yaklaşık %16, hesap). İki kademede de satır üst sınırı 1 200 örnektir; sade kademe daha kısa satır gönderirse (A5) pay kendiliğinden düşer.

**En kötü pencere** (A1-K34'ün sınırlarıyla, tek pencerede aynı anda): TF 29 + IMU 59 + bütün diğer sensörler 377 (39 + 39 + 61 + 89 + 48 + 43 + 28 + 30) + dört daraltıcı komut 102 (22 + 22 + 29 + 29) + dört olay sonucu 124 = **691 B kritik**; + bir kamera partisi 399 = **1 090 B**; kalan 110 B'a kadar toplu üye; pencere toplamı ≤ `B_pencere` = 1 200 B (kapasitenin %80'i; MP3'te bu yükte 600 s kayıpsız).

**Kart → sim, periyot başına ortalama**

| Kalem | Hesap | B / periyot |
|---|---|---:|
| `ACTUATOR` | 46 × 1 | 46,0 |
| `ACTUATOR` (`ASYNC`; yalnız arıza durumunda, ≤ 100 Hz) | 46 × 0,5 | ≤ 23,0 |
| `EVENT` ≤ 20/s, bir yinelemeyle (tahmin) | 49 × 0,2 | 9,8 |
| `CMD_RESULT` ≤ 56/s | 39 × 0,28 | 10,9 |
| `NAV` 50 Hz | 130 × 0,25 | 32,5 |
| `STATUS` 10 Hz | 48 × 0,05 | 2,4 |
| `FC_STATUS` 10 Hz | 91 × 0,05 | 4,6 |
| `CALL_TRACE` (200 step + 100 ızgara tick'i / s) | 145 × 1,5 | 217,5 |
| `CORE_COUNTERS` + `LINK_STATS` 1 Hz | (84 + 156) / 200 | 1,2 |
| **Olağan toplam** | | **347,9** (%23,2) |
| `LOG` (A4-K90: çağrı başına ≤ 4; üst sınır) | 31 × 4 × 1,5 | ≤ 186,0 |
| `DET_RECORD` (satır başına ≤ 4, ≤ 173 satır/s; üst sınır) | 90 × 4 × 173 / 200 | ≤ 311,4 |
| **En kötü toplam** | | **≤ 845,3** (%56,4) |

%60 hedefi en kötü durumda da sağlanır. Kritik olmayan çerçevelerin `T_olağan` penceresi periyot başına 750 B'a izin verir (hesap); en kötü durumda kritik olmayan talep ≈ 766 B olduğundan **seyreltilen önce `DET_RECORD`, sonra `LOG`'dur** (A1-K90'ın önceliği).

### 3.11 Sayaçlar

Sim'in sayaçları 64 bittir ve çalıştırma boyunca birikir; kartın sayaçları 32 bittir, açılıştan (SIL'de init'ten) beri birikir ve 2³²'de sarar. Sim bütün sayaçları A3'e yayımlar ve A6'ya yazar; her sayaç artışının nedeni olan karar (ret nedeni, parti numarası) A6'ya ayrıca yazılır.

**Tablo 3.11a — Sim sayaçları ve kararları**

| Ad | Tanım | Kural |
|---|---|---|
| `parti_gonderilen` | Gönderilen TF sayısı | A1-K28 |
| `parti_kacirildi` | Uygulama anında yuvası tamamlanmamış ve kaçırılan parti | A1-K75 |
| `parti_kacirildi_birlesik` | Bunların, sonradan gelen bir yanıtın `n_merged`'i kapsadığı kısmı | A1-K75 |
| `bariyer_bekleme`, `gercek_zaman_kaymasi_ns` (toplam ve en büyük) | Bariyerde beklenen yuva sayısı ve bekleme süreleri | A1-K75 |
| `tf_gec_yazim_ns` (en büyük) | TF yazım başlangıcının `W(t_n)`'den gecikmesi | A1-K43 |
| `yanit_ret_seq_disi`, `yanit_ret_yinelenen`, `yanit_ret_gec`, `yanit_ret_oturum` | Reddedilen yanıtlar, nedene göre | A1-K72, K27 |
| `yanit_iptal_esz`, `esz_guvenli_uygulanan` | Eşzamansız güvenli çıktının iptal ettiği yuvalar ve uygulanan eşzamansız çıktılar | A1-K78 |
| `komut_suresi_doldu` | Kullanım süresi dolup nötre geçiş | A1-K77 |
| `olay_alinan`, `olay_gec`, `olay_eksik` | Yeni olaylar; geç uygulamaya çalışılan; yuvası `n_events` tamamlanmadan kapanan | A1-K81, K83, K73 |
| `olay_sonuc_<kod>` | Her sonuç kodu için (APPLIED, DUPLICATE, REJ_EXPIRED, REJ_SAFE, REJ_TUBE_EMPTY, REJ_SESSION) | A1-K81–K88 |
| `komut_birlestirildi`, `komut_ertelendi`, `komut_sonucsuz` | Sim'de birleştirilen daraltıcılar; sonraki pencereye kalan toplu komutlar; oturum bittiği için sonucu gelmeyen komutlar | A1-K61, K36, K23 |
| `ping360_seyreltildi`, `ping360_gecikme_en_buyuk` | Atılan satırlar; `t_hazır`'dan teslim partisine en büyük gecikme (periyot) | A1-K37, K53 |
| `pencere_tasma` | Kritik ve atanmış üyelerin `B_pencere`'yi aştığı pencere (olağanda 0) | A1-K34 |
| `rx_crc_err`, `rx_resync_bytes`, `rx_unknown_msgid`, `rx_bad_len`, `rx_wrong_sys`, `rx_iflag`, `rx_mav1`, `rx_seq_gap` | Sim alıcısının çerçeve hataları | A1-K02–K08 |
| `oturum_kurulan`, `oturum_kopma`, `beklenmeyen_sifirlama` | Oturum olayları | A1-K16, K23 |
| `calistirma_gecersiz` ve neden bitleri | Zamanlama geçerliliği | A1-K79 |

**Tablo 3.11b — Kart sayaçları (`IDA_LINK_STATS.cnt[]` sırası)**

| # | Ad | Tanım | A4 `plat` karşılığı |
|---:|---|---|---|
| 0 | `rx_bytes` | Alınan bayt | — |
| 1 | `rx_frames` | Geçerli çerçeve | — |
| 2 | `rx_crc_err` | Sağlaması tutmayan çerçeve (bilinmeyen kimlik dahil) | `cnt_rx_error`'a girer |
| 3 | `rx_unknown_msgid` | Dialect'te olmayan ya da yönü yanlış kimlik | — |
| 4 | `rx_iflag` | `incompat_flags ≠ 0` | `cnt_rx_error`'a girer |
| 5 | `rx_bad_len` | `len` geçersiz | `cnt_rx_error`'a girer |
| 6 | `rx_wrong_sys` | `sysid`/`compid` sim'inki değil | — |
| 7 | `rx_mav1` | MAVLink 1 çerçevesi | — |
| 8 | `rx_overflow` | Alım tamponu taşması | `cnt_rx_error`'a girer |
| 9 | `rx_resync_bytes` | Çerçeve dışında atılan bayt | — |
| 10 | `rx_seq_gap` | Başlık `seq` boşluğu | — |
| 11 | `rx_stale_session` | `sess`'i eski çerçeve | `cnt_stale_session` |
| 12 | `rx_outside_session` | `RUNNING` dışında alınan üye ya da TF | — |
| 13 | `stamp_coarse` | Kaba alım damgası | — |
| 14 | `batch_closed` | Kapanan parti | — |
| 15 | `batch_incomplete` | Eksik parti | `cnt_batch_incomplete` |
| 16 | `batch_skipped` | Atlanan parti (GAP) | `cnt_batch_skipped` |
| 17 | `batch_old` | Eski TF | `cnt_batch_old` |
| 18 | `batch_dup` | Yinelenen TF | — |
| 19 | `batch_merged` | Birleştirilen parti | `cnt_batch_merged` |
| 20 | `member_overflow` | Kapasite yüzünden düşen üye | `cnt_member_overflow` |
| 21 | `ping360_incomplete` | Teslim partisinde eksik satır | — |
| 22 | `ping360_bad` | Geçersiz başlık ya da parça, çelişkili yineleme | — |
| 23 | `ping360_orphan` | HEAD'i gelmeyen ya da yer açmak için boşaltılan yuva | — |
| 24 | `cmd_deferred` | Ertelenen daraltıcı olmayan komut | `cnt_cmd_deferred` |
| 25 | `cmd_dropped` | Atılan daraltıcı olmayan komut | `cnt_cmd_dropped` |
| 26 | `narrow_coalesced` | Mandalda birleştirilen daraltıcı | `cnt_narrow_coalesced` |
| 27 | `cmd_bad_field` | Dönüşüm hatası | — |
| 28 | `grid_missed` | Kaçırılan ızgara anı | `cnt_grid_missed` |
| 29 | `out_fixed` | Nötrle değiştirilen çıktı | `cnt_out_fixed` |
| 30 | `tx_frames` | Gönderilen çerçeve | — |
| 31 | `tx_dropped` | Atılan kritik olmayan çerçeve | — |
| 32 | `tx_crit_stall` | Kritik çerçevenin taşımaya verilemediği periyot | `tx_stalled` bunun anlık karşılığıdır |
| 33 | `events_sent` | İlk kez gönderilen olay | — |
| 34 | `events_resent` | Olay yinelemesi | — |
| 35 | `sessions` | Kurulan oturum | — |

`plat.cnt_rx_error` = `rx_crc_err` + `rx_iflag` + `rx_bad_len` + `rx_overflow` (A4 3.6'nın "CRC ve çerçeveleme hatası" tanımının A1 karşılığı).

---

## 4. Davranış kuralları

Her kural tek bir sınanabilir cümledir. "Sim" ve "kart" Bölüm 1.1'deki taraflardır; "kart" SIL koşucusunu da kapsar (SIL'e özgü farklar 4.9'dadır). Zamanlama değerlerinin sayısal karşılıkları ve dayanakları 3.9'dadır; kurallarda adlarıyla anılır.

### 4.1 Fiziksel katman ve çerçeve

- **A1-K01.** Kartta A1 yalnız ST-LINK sanal seri portundan (USART1) geçer ve iki taraf portu 3 000 000 baud, 8N1, donanım ve yazılım akış denetimi kapalı kullanır; PC tarafı portu ham kipte (yankı, satır düzenleme ve karakter çevirisi kapalı) açar ve DTR/RTS hatlarının durumuna hiçbir davranış bağlamaz.
- **A1-K02.** Her iki taraf yalnız `magic = 0xFD`, `incompat_flags = 0` ve `compat_flags = 0` olan MAVLink 2 çerçevesi gönderir; alıcı `incompat_flags ≠ 0` olan çerçeveyi atar (`rx_iflag`), `compat_flags`'i yok sayar ve `0xFE` ile başlayan MAVLink 1 çerçevesini atar (`rx_mav1`).
- **A1-K03.** `protocol/ida.xml`'deki A4 ayna numaralandırmalarının (`IDA_CMD_TYPE`, `IDA_MODE`, `IDA_FC_OP`, `IDA_EVENT_KIND`, `IDA_EVENT_RESULT_CODE`, `IDA_ACK`, `IDA_REASON` 0–15, `IDA_CORE_RC`, `IDA_SAMPLE_FLAGS`, `IDA_DVL_FLAGS`, `IDA_TUBE_FLAGS`, `IDA_SP_MASK`, `IDA_MI_TYPE`, `IDA_MI_FRAME`, `IDA_MCTL`, `IDA_PARAM_TYPE`, `IDA_RES_KIND`, `IDA_FS_LEVEL`, `IDA_GNSS_FIX`, `IDA_SEVERITY`) her girdisi `core/include/ida/port.h`'taki karşılık sabitle aynı değerdedir.
- **A1-K04.** Sim her çerçeveyi `sysid = 250`, `compid = 25`; kart ve SIL koşucusu `sysid = 1`, `compid = 1` ile gönderir; her alıcı karşı tarafın kimliğini taşımayan çerçeveyi atar (`rx_wrong_sys`).
- **A1-K05.** Gönderen yükün sondaki sıfır baytlarını, ilk baytı koruyarak kırpar; alıcı kırpılmış ve kırpılmamış çerçeveyi kabul eder, eksik kısmı sıfırla doldurur ve `len` = 0 ya da `len` mesajın en büyük yük boyundan büyükse çerçeveyi atar (`rx_bad_len`).
- **A1-K06.** Alıcı, dialect'te olmayan ya da kendi yönünde alınmaması gereken (Tablo 3.3 "Yön" sütunu) mesaj kimliğini taşıyan çerçeveyi atar (`rx_unknown_msgid`).
- **A1-K07.** Alıcı sağlaması tutmayan çerçeveyi atar (`rx_crc_err`) ve ayrıştırmayı aday çerçevenin son baytından sonraki bayttan, `0xFD` arayarak sürdürür; bir çerçeveye ait olmayan her bayt `rx_resync_bytes`'a sayılır.
- **A1-K08.** Her gönderen çerçeve başlığındaki `seq`'i oturumdan bağımsız olarak her çerçevede bir artırır (mod 256); alıcı ardışık iki geçerli çerçeve arasındaki boşlukları `rx_seq_gap`'e sayar ve bu sayaca hiçbir davranış bağlamaz.
- **A1-K09.** Kart alım yolu 300 000 B/s'lik sürekli akışı bayt kaybetmeden işler; alım tamponu yine de taşarsa kart işlenmemiş baytları atar, `rx_overflow`'u artırır, ayrıştırmayı yeniden eşler ve açık partiyi eksik sayar (A1-K30).
- **A1-K10.** Her taraf taşımaya yalnız tam çerçeveyi sırasıyla verir, iki çerçeveyi bayt akışında iç içe geçirmez ve yazımına başladığı çerçeveyi, oturum sonu dahil, tamamlar ya da keser; kesilen çerçeve karşı tarafta A1-K07 ile atılır.

### 4.2 Oturum

Kartın durumları: `IDLE` (oturum yok), `HANDSHAKE` (bir `SIM_HELLO` yanıtlandı), `RUNNING` (oturum kuruldu; A4 `IDA_LINK_RUNNING`). Sim'in durumları: `KAPALI`, `EL_SIKISMA`, `HAZIR` (kabul gönderildi, kartın ilk oturum çerçevesi bekleniyor), `KOSUYOR`, `KOPUK`.

- **A1-K11.** Kart `IDLE`'da `T_duyuru` aralıkla `peer_nonce = 0` taşıyan `IDA_BOARD_HELLO` (duyuru) gönderir ve `IDA_BOARD_HELLO` dışında hiçbir çerçeve göndermez.
- **A1-K12.** Sim portu açtıktan hemen sonra işletim sisteminin alım ve gönderim tamponlarını boşaltır, yeni bir el sıkışma için sıfırdan farklı yeni bir `nonce` seçer, `peer_nonce`'u bu `nonce`'a eşit bir `IDA_BOARD_HELLO` alana kadar aynı `nonce`'lu `IDA_SIM_HELLO`'yu `T_hello` aralıkla gönderir ve o ana kadar aldığı bütün çerçeveleri atar.
- **A1-K13.** Kart bir `IDA_SIM_HELLO` aldığında: `nonce` yanıtladığı son `SIM_HELLO`'nunkiyle aynıysa ve durumu `HANDSHAKE` ise aynı `IDA_BOARD_HELLO`'yu yeniden gönderir, durumu `RUNNING` ise yok sayar; `nonce` farklıysa ve durumu `RUNNING` ise önce oturumu A1-K22'ye göre bitirir, sonra (her durumda) yeni bir sıfırdan farklı `nonce` seçer, `peer_nonce = SIM_HELLO.nonce` taşıyan `IDA_BOARD_HELLO` gönderir ve `HANDSHAKE`'e geçer; platform güvenli beklemesindeyse (01 §2.8) `SIM_HELLO`'yu yanıtlamaz ve duyurularında `init_rc = NOT_INIT` taşır.
- **A1-K14.** Sim yanıt `IDA_BOARD_HELLO`'yu şu sırayla denetler ve ilk tutmayan koşulun sonucunu `IDA_SESSION_ACCEPT` ile gönderir: `proto_major` ve `proto_minor` kendininkine eşit (değilse `REJECT_PROTO`); `dialect_hash` eşit (`REJECT_DIALECT`); `port_major` ve `port_minor` sim'in derlendiği `port.h` sürümüne eşit (`REJECT_PORT`); `det_schema_major` sim'in derlendiği A8 şema ana sürümüne eşit (`REJECT_DET_SCHEMA`); `ctrl_period_ns` 5 000 000 (`REJECT_RATE`); `param_schema_hash` sim'in beklediğine eşit (`REJECT_PARAM_SCHEMA`); bunlar tutarsa A1-K18'e göre `REBOOT` ya da `OK`.
- **A1-K15.** Sim `OK` sonucunda `sess` olarak sıfırdan, `BOARD_HELLO.prev_sess`'ten ve aynı sim sürecinde kullandığı son 16 oturum kimliğinden farklı bir değer seçer.
- **A1-K16.** Kart bir `IDA_SESSION_ACCEPT`'i yalnız `HANDSHAKE` durumunda ve `nonce_sim` yanıtladığı son `SIM_HELLO.nonce`'a, `nonce_board` kendi son `nonce`'una eşitse işler, diğerlerini atar; `OK`'de `RUNNING`'e geçer, oturumu `sess` ile kurar, hemen `IDA_KEEPALIVE(sess, RUNNING)` gönderir ve bir sonraki çekirdek çağrısında `plat.link_event = SESSION_START` verir (A4-K72); `REJECT_*`'te `IDLE`'a döner.
- **A1-K17.** Kart `REBOOT` sonucunu aldığında başka çerçeve göndermeden planlı yeniden başlatma yapar: kartta sistem sıfırlaması (`reset_cause = SOFTWARE`), SIL'de çekirdeğin yeni `boot_count` ve `reset_cause = SIL_REBOOT` ile yeniden kurulması (A4-K11, A4-K86); sim `REBOOT` gönderdikten sonra `T_reboot` içinde `boot_count`'u öncekinden büyük ve `sessions_since_boot = 0` olan bir duyuru bekler, gelmezse durumu A3'e hata olarak bildirir.
- **A1-K18.** Sim bir çalıştırmayı (`run_mode` ≠ `PREP`) yalnız `sessions_since_boot = 0`, `init_rc = OK`, `boot_count_ok = 1` ve `param_blob_crc` A5'ten kurulan bloğun (A4 3.10) CRC-32/ISO-HDLC değerine eşit olan bir karta başlatır; `sessions_since_boot > 0` ise önce `REBOOT` ister; blok uyuşmuyorsa A1-K19'u uygular.
- **A1-K19.** Parametre eşitleme `run_mode = PREP` olan bir oturumda yapılır: sim fizik yürütmeden yalnız zaman çerçevesi gönderir, A5'teki her otopilot parametresi için bir `IDA_CMD_PARAM_SET`, ardından `PARAM_COMMIT` ve `PARAM_SAVE` gönderir (`source = 250`), her birinin sonucunu bekler; hepsi `ACCEPTED`, `IDA_STATUS.param_store = OK` ve `param_blob_crc` beklenen değere eşit olursa `IDA_SESSION_CLOSE(RUN_END)` ve ardından yeni el sıkışmada `REBOOT` ister; aksi halde çalıştırmayı başlatmaz ve A3'e bildirir.
- **A1-K20.** `HANDSHAKE`, `HAZIR`, `KOSUYOR` ve `RUNNING` durumlarında her taraf, son `T_canlı` içinde başka bir çerçeve göndermediyse `IDA_KEEPALIVE` gönderir.
- **A1-K21.** `IDA_SESSION_CLOSE` alan taraf, `sess` güncel oturumunkiyse oturumu hemen bitirir (kart A1-K22, sim A1-K23); sim çalıştırma bittiğinde ve kapanırken, kart platform güvenli beklemesine girerken `IDA_SESSION_CLOSE` gönderir.
- **A1-K22.** Kart oturumu şu durumlardan biri olunca bitirir: `IDA_SESSION_CLOSE` alındı; sim'den `T_bağ` boyunca geçerli çerçeve alınmadı (kartta, duvar saatiyle); yeni `nonce`'lu `IDA_SIM_HELLO` alındı; platform güvenli beklemeye girdi; SIL'de TCP bağlantısı kapandı. Bitişte kart `IDLE`'a döner, bir sonraki çekirdek çağrısında `plat.link_event = SESSION_END` verir (A4-K72; daraltıcıdır, A4-K73), onaylanmamış olaylara `SESSION_END` sonucu döner ve yinelemelerini bırakır (A4-K71), ertelenmiş komutları, açık partiyi, Ping360 yuvalarını ve gönderim kuyruğunda yazımı başlamamış bütün çerçeveleri siler (A4-K74), mandalları silmez.
- **A1-K23.** Sim oturumu şu durumlardan biri olunca bitirir: `IDA_SESSION_CLOSE` alındı; karttan `T_bağ` boyunca geçerli çerçeve alınmadı (HIL); `KOSUYOR`'da bir `IDA_BOARD_HELLO` alındı (kart sıfırlandı); işletim sistemi aygıtın kalktığını bildirdi; SIL'de bağlantı kapandı ya da A1-K76'nın gözetimi doldu. Bitişte sim itici modelini bir sonraki fizik adımı sınırından itibaren nötr duruma (sıfır itki isteği; A1-K77) alır, iki tüpün süreli iznini kapatır, bütün yuvaları ve bekleyen olayları siler, sonucu gelmemiş komutları `LINK_SESSION_END` ile A3'e bildirir, planlanmamış bitişte çalıştırmayı geçersiz işaretler (A1-K79) ve fiziği durdurmadan A1-K12 ile yeni el sıkışmaya başlar (aygıt kalktıysa portu 1 s aralıkla yeniden açmayı dener).
- **A1-K24.** Kartın ve çekirdeğin hiçbir davranışı `run_mode` değerine bağlı değildir; kart bu alanı yalnız tanı için saklayabilir.
- **A1-K25.** Kart `RUNNING`'de `sess` alanı taşıyan bir sim çerçevesini (`TIME_FRAME`, komutlar, `EVENT_RESULT`, `KEEPALIVE`, `SESSION_CLOSE`) `sess` güncel oturumunki değilse atar (`rx_stale_session`); `sess` alanı taşımayan üyeler (sensör, sonar, algılama) akıştaki yerleriyle oturuma bağlanır (A1-K28).
- **A1-K26.** Kart `RUNNING` dışında aldığı bütün üyeleri, zaman çerçevelerini, komutları ve olay sonuçlarını atar (`rx_outside_session`).
- **A1-K27.** Sim `HAZIR`'da `sess`'i taşıyan ilk kart çerçevesini aldığında `KOSUYOR`'a geçer, `T_hazır` içinde almazsa A1-K12 ile yeni el sıkışmaya başlar; `KOSUYOR`'da `sess` alanı güncel oturumunki olmayan kart çerçevesini atar ve sayar (`yanit_ret_oturum`).

**Yeniden bağlanmada numaralar.** `event_id` açılış sayacından türediği için oturumlar arasında yinelenmez (A4-K61); yeni oturumun ilk TF'si her `batch_seq` ile kabul edilir (A4-K25) ve sim bir çalıştırma içinde `batch_seq`'i kesintisiz sürdürür; `cmd_id` A1-K66'ya göre sürer; MAVLink başlık `seq`'i oturumdan bağımsızdır (A1-K08).

### 4.3 Parti ve zaman

- **A1-K28.** Sim `KOSUYOR`'da (duraklatılmamışken) her kontrol periyodu için tam bir `IDA_TIME_FRAME(n)` gönderir ve parti `n`'in üyeleri, aynı oturumda akışta `TF(n−1)`'den (oturumun ilk partisinde `IDA_SESSION_ACCEPT`'ten) sonra ve `TF(n)`'den önce gönderilen üye çerçevelerin tamamıdır; kart bir üyeyi akışta aldığı yere göre bu partiye bağlar ve `TF(n)`'yi alınca partiyi kapatır.
- **A1-K29.** Sim bir oturumda `TF(n).t_sim_ns`'i bir öncekinden tam `P` büyük ve fizik adımının tam katı, `batch_seq`'i bir öncekinden seri aritmetikle tam 1 büyük gönderir; duraklatmadan sonra sayılar boşluksuz sürer.
- **A1-K30.** Sim `n_members`'a penceredeki üye çerçeve sayısını, `tf_flags.IMU`'ya pencerede `IDA_IMU` olup olmadığını yazar; kart bir partide aldığı geçerli üye sayısı `n_members`'tan farklıysa ya da `IMU` bayrağı kurulu olduğu halde IMU almadıysa partiyi eksik sayar (`plat.cnt_batch_incomplete`), aldığı üyeleri yine çekirdeğe verir ve o çağrının `IDA_ACTUATOR`'ında `BATCH_INCOMPLETE` bayrağını kurar.
- **A1-K31.** Kart bir oturumdaki ilk TF'yi her `batch_seq` ile kabul eder; sonrakinde `batch_seq` son kapanandan seri aritmetikle 1 büyükse olağan kapanıştır, 1'den fazla büyükse aradaki parti sayısını `plat.cnt_batch_skipped`'e ekleyip partiyi kapatır (boşluğun büyüklüğüne üst sınır yoktur), son kapanana eşitse (`DUP`) ya da küçükse (`OLD`) penceredeki üyeleri atar, `batch_dup` ya da `plat.cnt_batch_old`'u artırır, çekirdeği çağırmaz ve yanıt göndermez.
- **A1-K32.** Kart kapanan her partiyi A4-K16, A4-K19 ve A4-K102'ye göre çekirdeğe verir: partide IMU varsa `ida_core_step`, yoksa `ida_core_tick`; `tf.present = 1`, `tf.seq = batch_seq`, `tf.t = t_sim_ns`, `tf.t_rx` = TF'nin alım damgası.
- **A1-K33.** Sim her üyeyi tam bir partiye atar; atama yalnız sim zamanına, üyelerin `t_hazır`'ına ve kodlanmış boylarına bağlıdır ve taşımanın duvar saati zamanlamasına bağlı değildir.
- **A1-K34.** Sim bir partiye en çok şunları koyar: 1 `IDA_IMU`; IMU dışında 16 sensör örneği (A4-K31); 1 `IDA_SAFETY`; 1 `IDA_PING360_HEAD`; 1 kamera algılama partisi (1 `IDA_DET_BATCH` ve en çok 4 `IDA_DET_RECORD`; A8-K18); 4 `IDA_EVENT_RESULT` (A4-K31); her daraltıcı türden 1 komut (A1-K61); 4 daraltıcı olmayan komut; ve TF dahil pencerenin toplam kodlanmış boyunu `B_pencere`'nin, pencerenin `IDA_PING360_*` çerçevelerinin toplamını `B_p360`'ın altında tutar.
- **A1-K35.** Sim kritik üyeleri (`IDA_IMU`, diğer sensör mesajları, daraltıcı komutlar, `IDA_EVENT_RESULT`) `t_{n−1} < t_hazır ≤ t_n` olan partiye atar ve hiçbir koşulda ertelemez; komutun `t_hazır`'ı bağlantı emülatörünün bırakma anı (laboratuvar komutunda veriliş anı), olay sonucunun `t_hazır`'ı sonucun karara bağlandığı fizik adımıdır.
- **A1-K36.** Sim toplu üyeleri (daraltıcı olmayan komutlar ve `IDA_PING360_*`) `t_hazır ≤ t_{n−1}` olan bir partiye atar ve pencere `n`'i şu sırayla doldurur: kritik üyeler; A1-K38'e göre `P_k = n` olan algılama partisi; daraltıcı olmayan komutlar varış sırasıyla (A1-K34 sınırları içinde); Ping360 çerçeveleri sırasıyla (A1-K34 sınırları içinde); sığmayan toplu üye sırasını koruyarak sonraki pencereye kalır.
- **A1-K37.** Sim gönderimi başlamamış en çok bir Ping360 tarama satırını kuyrukta tutar; kuyruk doluyken yeni bir satır hazır olursa kuyruktaki gönderimi başlamamış eski satırı atar ve `ping360_seyreltildi`'yi artırır.
- **A1-K38.** Sim kamera karesi `k`'nın algılama partisini (başlık ve kayıtları) A8-K57'nin `P_k` partisine, başlık önce ve kayıtlar `batch_index` sırasıyla ardışık olarak koyar ve bir kamera partisini birden çok A1 partisine yaymaz; parti `n`'in oluşturulmaya başlandığı an `M_k` (A8) sim'in `TF(n−1)`'i taşımaya verdiği andır.
- **A1-K39.** HIL kiplerinde sim pencere `n`'in toplu ve atanmış üyelerini `TF(n−1)`'den hemen sonra, kritik üyeleri `t_hazır`'larına ulaşıldıkça yazar, `TF(n)`'yi `W(t_n)`'den önce yazmaz ve ardışık iki TF'nin yazım başlangıçları arasında en az `P/2` duvar süresi bırakır; sim geride kaldığında TF'leri bu alt sınırla yetişerek gönderir.
- **A1-K40.** Kart TF'lerin kendisine `P`'den kısa aralıklarla ulaşabileceğini olağan sayar (USB ve ST-LINK köprüsünün paketlemesi; MP3'te ölçülmedi) ve art arda kapanan partileri A4-K102'ye göre birleştirir; birleştirmede en yeni partinin yanıtı `n_merged`'i taşır ve birleştirilen eski partilere ayrı yanıt gönderilmez.
- **A1-K41.** HIL'de sim duraklatıldığında TF ve üye gönderimi durur, `IDA_KEEPALIVE` sürer, çalıştırma geçersiz işaretlenir (A1-K79); sürdürüldüğünde A1-K29 geçerlidir.
- **A1-K42.** Saat eşlemesi yapılmaz: A1'de iki tarafın duvar saatlerini karşılaştıran ya da birinden ötekine çeviren hiçbir alan ve kural yoktur; ölçüm zamanları sim zamanıdır (iskelet §4), kartın duvar saati yalnız kartın kendi sürelerinde ve tanı alanlarında (`rx_to_tx_us`, `IDA_CALL_TRACE`) kullanılır, sim taşıma gecikmesini yalnız kendi duvar saatiyle (TF yazımından yanıtın alımına) ölçer.
- **A1-K43.** Sim gönderdiği her TF'nin yazım başlangıcını ve bitişini kendi duvar saatiyle kaydeder (A6) ve gerçek zaman kaymasını (`W(t_n)` ile TF yazım başlangıcı arasındaki fark) sayaçlarda tutar (Tablo 3.11a).
- **A1-K44.** Kart bir ızgara tick'inin (A4-K18) çıktısını yalnız `thr_valid = 1` ise (A4-K56'ya göre bu durumda `is_safe = 1`'dir) `ASYNC` bayraklı bir `IDA_ACTUATOR` olarak, `batch_seq = out.seq` ile gönderir; ızgara tick'i başına en çok bir eşzamansız çıktı gider.
- **A1-K45.** Kart zaman çerçeveli her çekirdek çağrısı için (dönüş kodu `OK`, `IMU_REJECTED`, `BAD_INPUT`, `NOT_INIT` dahil) tam bir `IDA_ACTUATOR` gönderir; ızgara tick'inde A1-K44 geçerlidir; `DUP` ve `OLD` partiler için çekirdek çağrılmadığından yanıt yoktur (A1-K31).

### 4.4 Sensör, sonar ve algılama üyeleri

- **A1-K46.** Kart her sensör üyesini Tablo 3.4 ve 3.4b'ye göre A4 örneğine çevirir: `IDA_IMU`'yu `in.imu`'ya, diğer sensörleri partideki varış sırasıyla `in.samples[]`'e koyar, değerleri değiştirmez ve tazelik ya da aralık kararı vermez (A4-K36).
- **A1-K47.** Kart bir alanı A4 tipine çeviremediğinde (Tablo 3.4b'de belirtilen aralık dışı numaralandırma) örneği atmaz, `hdr.flags`'ten `VALID` bitini silerek çekirdeğe verir (çekirdek reddeder ve sayar; A4-K33).
- **A1-K48.** Kart her çerçevenin alım damgasını, çerçevenin son baytı belleğe yazıldıktan sonraki ilk alım olayının duvar saati olarak koyar; alım olayları en az her `0xFD` baytı, hat boşluğu ve DMA yarım/tam kesmeleridir; bir çerçeveye sonrasındaki bir olay bağlanamıyorsa çerçevenin son baytından önceki en son olayın damgası kullanılır ve `stamp_coarse` artar; damgalar akış sırasında azalmaz ve kart bunları A4'teki `hdr.t_rx`, `tf.t_rx`, `det_t_rx[]` ve `det_batch_t_rx[]` alanlarına koyar.
- **A1-K49.** Sim bir Ping360 tarama satırını `n_frag = ⌈n_samples / 240⌉` `IDA_PING360_DATA` ve ardından bir `IDA_PING360_HEAD` olarak, aynı oturumda ve akışta bu sırayla gönderir; DATA çerçeveleri birden çok pencereye yayılabilir, HEAD satırın son çerçevesidir.
- **A1-K50.** Ping360 satırının teslim edildiği parti, HEAD'inin üyesi olduğu partidir: kart o partiyi kapatırken aynı (`inst`, `seq`) için `frag_idx` 0 … `n_frag − 1` parçalarının hepsini almışsa satırı `in.ping360` ile çekirdeğe verir, almamışsa satırı atar ve `ping360_incomplete`'i artırır.
- **A1-K51.** Kart en az iki yeniden birleştirme yuvası tutar; `frag_idx ≥ n_frag` (HEAD gelmeden ≥ 5) olan ya da yükü 6 bayttan kısa DATA'yı atar (`ping360_bad`); aynı parçanın bayt bayt aynı yinelemesini yok sayar, farklı yinelemesinde satırı atar (`ping360_bad`); HEAD'i 32 parti kapanışı boyunca gelmeyen yuvayı boşaltır (`ping360_orphan`); iki yuva da doluyken yeni bir satırın parçası gelirse en eski yuvayı boşaltır (`ping360_orphan`).
- **A1-K52.** Menzil kutusu `i`'nin (0 … `n_samples − 1`) nominal yansıma anı `t_i = t_meas + (2i + 1) · sample_period · 25 ns / 4` ve nominal eğik menzili `r_i = c · (2i + 1) · sample_period · 25 ns / 4`'tür (`c` çekirdeğin ses hızı parametresi); bu, kutu merkezine giden doğrudan yolun zamanıdır, darbe süresi hesaba katılmaz ve çok yollu enerjiyi ayırmaz (iskelet §3.1).
- **A1-K53.** Ping360 satırının `t_meas_ns`'i gönderim anıdır ve teslimdeki gecikme (bir ya da daha çok periyot) bu değeri değiştirmez; sim satırın teslim edildiği partiyi A6'ya yazar.
- **A1-K54.** `IDA_PING1D.t_meas_ns` yansıma anıdır ve gönderim anı `t_meas − tx_offset_ns`'tir; köprü ve kart bu alanlar arasında dönüşüm yapmaz.
- **A1-K55.** DVL'nin rapor anı A1'de taşınmaz; `IDA_DVL.t_meas_ns` yansıma anıdır.
- **A1-K56.** Sim `IDA_SAFETY`'yi `T_emniyet` aralıkla ve sanal emniyet girdisi ya da tüp durumu değiştiğinde gönderir; bir partiye en çok bir tane koyar; aynı pencerede bir kapanma (`estop` 0→1 ya da bir tüpte `PLUG_IN` veya `PIN_OUT` 1→0; A4-K111) ve ardından açılma olduysa o partide kapanmış durumu, açılmayı sonraki partide gönderir; `t_meas_ns` örnekleme ya da değişim anıdır, `seq` her mesajda bir artar ve `PERMIT_OPEN` biti sim'deki iznin `t_meas` anındaki durumudur (A1-K89).
- **A1-K57.** Kart `IDA_DET_BATCH` ve `IDA_DET_RECORD`'u alanları değiştirmeden `in.det_batch[]` ve `in.det[]`'e varış sırasıyla koyar ve doğrulamaz (doğrulama A8-K29'a göre çekirdeğindir); partideki algılama öğesi A4 kapasitesini aşarsa A4-K102'nin düşürme kuralı uygulanır.
- **A1-K58.** Kamera kapalıyken sim A8-K64'ün `NO_FRAME` başlığını `t_meas_ns`'i konduğu partinin kapanış anı olacak biçimde o partiye koyar.

### 4.5 Komutlar

- **A1-K59.** Panelden gelen komutlar sim'de bağlantı emülatöründen (A5 profili; sim zamanı) geçer ve emülatörün bırakma anına göre bir partiye atanır; sim laboratuvarının komutları (A1-K19) emülatörden geçmez.
- **A1-K60.** Her komutun sınıfı (genişletir, daraltır, nötr) Tablo 3.6'dadır ve `protocol/ida.xml`'deki ilgili numaralandırma girdisinin açıklamasında `[sinif: genisletir|daraltir|notr|kosullu]` biçiminde de bulunur; "koşullu" girdinin sınıfı Tablo 3.6'daki koşuldan çıkar.
- **A1-K61.** Sim aynı partiye aynı daraltıcı türden (DISARM, daraltıcı SET_MODE, FC_DISARM, FC_ABORT) en çok bir komut koyar; aynı pencerede birden çok gelirse en sonuncuyu gönderir, öncekilerin her birini kendisi `ACCEPTED` / `IDA_REASON_COALESCED` sonucuyla A3'e bildirir ve sayar.
- **A1-K62.** Daraltıcı komutlar kritik, diğer komutlar toplu sınıftadır (A1-K35, A1-K36).
- **A1-K63.** Kart `sess`'i geçerli bir daraltıcı komutu ayrıştırdığı anda A4-K46'ya göre mandala alır ve tampon doluluğundan bağımsız olarak hiçbir koşulda atmaz; daraltıcı olmayan komutu partisinin çağrısının `cmds[]`'ine varış sırasıyla koyar ve kapasite aşımında A4-K105'i uygular.
- **A1-K64.** Kart Tablo 3.6'daki bir dönüşüm hatasında komutu çekirdeğe vermez ve `REJECTED` / `IDA_REASON_LINK_BAD_FIELD` sonucunu kendisi gönderir.
- **A1-K65.** Sim her yeni komuta, aynı sim çalıştırmasında ve kartın aynı açılışında başka hiçbir komutun taşımadığı bir `cmd_id` verir; aynı komutun emülatör ya da hata enjeksiyonu kaynaklı yinelemeleri aynı `cmd_id`'yi taşır.
- **A1-K66.** Sim her oturumda ilk yeni komuta, kendi sıradaki `cmd_id`'si ile `BOARD_HELLO.last_cmd_id + 1`'in seri aritmetikle büyük olanını verir, sonraki her yeni komutta bir artırır ve 0'ı atlar; kart `last_cmd_id`'yi çekirdeğe verdiği son komutun (mandal dahil) `cmd_id`'si olarak tutar.
- **A1-K67.** Kart `RUNNING`'de kabul ettiği her komut çerçevesi için en az bir `IDA_CMD_RESULT` gönderir: çekirdeğin sonucu (A4-K43; aynı çağrının çıktısından), mandalda üzerine yazılan önceki daraltıcı için platformun `ACCEPTED` / `IDA_REASON_COALESCED` sonucu (üzerine yazıldığı anda), A4-K105'te atılan komut için `REJECTED` / `IDA_REASON_LINK_DROPPED`, dönüşüm hatasında A1-K64; aynı `cmd_id`'li yinelemeye çekirdeğin `DUPLICATE` sonucu ayrıca gider ve sim bir `cmd_id` için ilk sonucu A3'e iletir, sonrakileri yalnız kaydeder.
- **A1-K68.** `PARAM_SAVE` kabul edildiğinde kart bloğu (A4-K82) kalıcı belleğe yazar, `IDA_CMD_RESULT`'ta `save_gen`, `save_len` ve bloğun CRC-32/ISO-HDLC değerini (`save_crc`) bildirir ve kaydın ilerleyişini `IDA_STATUS.param_store`, `param_fail`, `param_gen`, `param_blob_crc` ile bildirir; blok A1'de taşınmaz.
- **A1-K69.** Sim bir görev yüklemesinin `MISSION_BEGIN`, `MISSION_ITEM` ve `MISSION_END` komutlarını bu sırayla toplu sınıfta gönderir; komutlar birden çok partiye yayılabilir ve atomikliği çekirdek sağlar (A4-K51).
- **A1-K70.** Panel yer istasyonu canlılığını 1 Hz `IDA_CMD_SIMPLE(HEARTBEAT)` ile bildirir; bu komut emülatörden geçer ve kaybı çekirdeğin yer istasyonu kaybı kuralını işletir (A4-K52).

### 4.6 Sürekli komut yanıtı ve sim'in kabul kuralı

- **A1-K71.** Kart `IDA_ACTUATOR`'ı Tablo 3.7'ye göre doldurur; `THR_VALID = 0` iken `pwm_us[]` ve `valid_for_ns` 0'dır.
- **A1-K72.** Sim `ASYNC` bayraksız ve `sess`'i güncel bir `IDA_ACTUATOR`'ı şu sırayla değerlendirir: `batch_seq` bu oturumda gönderdiği bir parti değilse reddeder (`yanit_ret_seq_disi`); yuvası doluysa reddeder (`yanit_ret_yinelenen`); uygulama anı `a_n`'deki fizik adımı hesaplanmışsa reddeder (`yanit_ret_gec`); değilse yuvasına koyar. Reddedilen yanıt hiçbir partinin yerine uygulanmaz.
- **A1-K73.** Kart bir çağrının ilk kabul kapısından geçen yeni olaylarını (A4-K67) o çağrının `IDA_ACTUATOR`'ından önce, `call_kind`'ı çağrının türüyle doldurarak gönderir ve `n_events`'e bunların sayısını yazar; sim bir yuvayı ancak `IDA_ACTUATOR` ve `batch_seq`'i aynı, `call_kind`'ı `TICK_GRID` olmayan `n_events` farklı `event_id` alındığında tam sayar.
- **A1-K74.** Sim `a_n`'de başlayan fizik adımının başında yuva `n`'i uygular: `THR_VALID = 1` ise `pwm_us[]` yürürlükteki sürekli komut olur ve kullanım süresi `a_n + valid_for_ns`'te dolar; `THR_VALID = 0` ise yuva yanıtlanmış sayılır ve yürürlükteki komut değişmez.
- **A1-K75.** `a_n`'de yuva tam değilse: bariyer kipinde sim, sim zamanını ilerletmeden yalnız o yuvanın tamamlanmasını en çok `T_bekle` (duvar) bekler, bu sırada gelen eski numaralı yanıtları reddeder ve sonraki numaralıları yuvalarına koyar, yuva tamamlanırsa onu `a_n`'de uygular; `T_bekle` aşılırsa ya da HIL 1x'te parti "kaçırıldı" olur (`parti_kacirildi`; yanıtın `n_merged`'i bu partiyi kapsıyorsa `parti_kacirildi_birlesik`), komut değişmez, yuvanın sonradan gelen yanıtı A1-K72'ye göre reddedilir; bekleme süresi `gercek_zaman_kaymasi`'na eklenir.
- **A1-K76.** SIL'de sim `a_n`'de yuvanın tamamlanmasını sim zamanını ilerletmeden süre sınırı olmaksızın bekler; yalnız `T_SIL` (duvar) boyunca yanıt gelmezse oturumu bitirir ve A3'e hata bildirir.
- **A1-K77.** Yürürlükteki komutun kullanım süresi dolduğunda ve yeni bir komut uygulanmadıysa sim itici modelinin girdisini nötr duruma (sekiz itici için sıfır itki isteği) alır; nötr durum ESC modelinin "sinyal kaybı" davranışıyla (ör. son değeri tutma) temsil edilmez.
- **A1-K78.** Sim bir eşzamansız güvenli çıktıyı (`ASYNC`, `THR_VALID = 1`, `IS_SAFE = 1`) alımından sonraki ilk fizik adımı sınırında uygular, kullanım süresini o andan sayar ve ondan önce alınmış, henüz uygulanmamış, `IS_SAFE = 0` taşıyan bütün yuvaları iptal eder (`yanit_iptal_esz`); ondan sonra alınan yanıtlar A1-K72–K75'e göre işlenir; `IS_SAFE = 0` taşıyan `ASYNC` çerçeveyi reddeder.
- **A1-K79.** Sim bir çalıştırmayı şunlardan biri olduğunda zamanlama bakımından geçersiz işaretler ve nedenleri A3'e bildirir, A6'ya yazar: kaçırılan parti (birleştirilen dahil); sıfırdan büyük bariyer beklemesi; geç uygulanan olay (A1-K83); uygulanan eşzamansız güvenli çıktı; reddedilen geç yanıt; pencere planının aşılması (`pencere_tasma`); planlanmamış oturum bitişi ya da kart sıfırlanması; HIL'de duraklatma. Zamanlama kabulü yalnız bu sayaçların hepsi sıfır olan çalıştırmalarla yapılır (iskelet §4).

### 4.7 Tek seferlik olaylar

- **A1-K80.** Kart bir olayı ilk kabul kapısından (A4-K67) geçirdiği çağrıda `IDA_EVENT` olarak gönderir ve sonucu gelene ya da olayı çıkaran çağrının `now_w`'sinden `apply_by_ns` geçene kadar aynı yükle `T_olay_yin` aralıkla yineler (A4-K106); `now_w + apply_by_ns + T_sonuç_pay` içinde sonuç gelmezse çekirdeğe `EXPIRED_NO_ACK` verir.
- **A1-K81.** Sim bir çalıştırma boyunca karara bağladığı her `event_id`'nin son sonucunu ve `t_apply`'ını saklar; aynı `event_id` yeniden gelirse olayı yeniden işlemez ve saklı sonucu yeniden gönderir (`APPLIED` saklıysa `DUPLICATE` ve özgün `t_apply`, ret saklıysa aynı ret kodu); karara bağlanmamış (beklemede) bir olayın yinelemesini yok sayar.
- **A1-K82.** Sim üst 32 biti oturumun `BOARD_HELLO.boot_count`'una eşit olmayan `event_id`'yi `REJ_SESSION` ile reddeder.
- **A1-K83.** Sim `batch_seq = n` taşıyan ve `a_n`'deki fizik adımı hesaplanmadan alınan bir olayı `a_n`'de, yuvanın sürekli komutuyla aynı adımda uygulamaya çalışır; daha sonra alınan olayı ve `call_kind = TICK_GRID` taşıyan her olayı alımından sonraki ilk fizik adımı sınırında uygulamaya çalışır ve `olay_gec`'i artırır.
- **A1-K84.** Sim bir olayı uygulama anı `t_apply > t_decide + apply_by_ns` (sim zamanı) ise ya da `TF(batch_seq)`'in yazım başlangıcından bu yana kendi duvar saatinde `apply_by_ns + T_bekle`'den fazla geçmişse uygulamaz ve `REJ_EXPIRED` ile yanıtlar.
- **A1-K85.** Sim her fizik adımı sınırında şu sırayı uygular: (1) o adıma düşen sanal emniyet girdisi değişiklikleri (A3, A5 zaman çizelgesi); (2) süresi dolan izinlerin kapanması (A1-K86); (3) `SAFETY_CLOSE` olayları; (4) sürekli komut (yuva ya da eşzamansız çıktı); (5) `SAFETY_OPEN` olayları; (6) `FIRE` olayları; aynı türdeki olaylar `event_id` sırasıyla.
- **A1-K86.** Sim bir `SAFETY_OPEN`'ı ancak o adımda `estop = 0`, ilgili tüpte `PLUG_IN` ve `PIN_OUT` kurulu ve `event_id` aynı açılışta o tüpü kapsayan uygulanmış her `SAFETY_CLOSE`'unkinden büyükse uygular (`APPLIED`; tüpün izni `t_apply + hold_ns`'e kadar açık; daha yeni bir `SAFETY_OPEN` bu süreyi kendi `t_apply + hold_ns`'ine taşır), aksi halde `REJ_SAFE` döner.
- **A1-K87.** Sim süresi geçmemiş ve oturumu tutan bir `SAFETY_CLOSE`'u her durumda uygular (`APPLIED`) ve `tube`'un gösterdiği tüpün (255'te iki tüpün) iznini kapatır.
- **A1-K88.** Sim bir `FIRE`'ı şu denetimlerin ilk tutmayanının koduyla reddeder, hepsi tutarsa uygular: `REJ_SESSION` (A1-K82); `REJ_EXPIRED` (A1-K84); `REJ_SAFE` (o adımda `estop = 1`, ya da tüpte `PLUG_IN` veya `PIN_OUT` kurulu değil, ya da tüpün izni kapalı, ya da `event_id` aynı açılışta o tüpü kapsayan uygulanmış bir `SAFETY_CLOSE`'unkinden küçük); `REJ_TUBE_EMPTY` (tüpte `LOADED` kurulu değil); uygulanan `FIRE` o adımda 04'ün fırlatma modelini tetikler ve tüpün `LOADED` bitini siler.
- **A1-K89.** Sim bir tüpün iznini, A1-K86–K88'e ek olarak, o tüpü etkileyen bir emniyet girdisi kapandığında (`estop` 1 oldu, `PLUG_IN` ya da `PIN_OUT` 0 oldu) ve oturum bittiğinde aynı adımda kapatır; izin durumu her değiştiğinde A1-K56'ya göre `IDA_SAFETY` gönderir.
- **A1-K103.** Sim karara bağladığı her olayın sonucunu `IDA_EVENT_RESULT` ile, `t_hazır`'ı karar adımı olan kritik üye olarak gönderir.

### 4.8 Telemetri, sonuçlar ve izler (kart → sim)

- **A1-K90.** Kart bir çağrının çıktısını şu öncelikle gönderir: yeni `IDA_EVENT`'ler, `IDA_ACTUATOR`, olay yinelemeleri; ardından kritik olmayan çerçeveler bu sırayla: `IDA_CMD_RESULT`, `IDA_STATUS`, `IDA_FC_STATUS`, `IDA_CALL_TRACE`, `IDA_NAV`, `IDA_LOG`, `IDA_CORE_COUNTERS`, `IDA_LINK_STATS`, `IDA_DET_RECORD`; kritik olmayan çerçeveleri yalnız son `IDA_ACTUATOR`'ın gönderiminden sonraki `T_olağan` içinde ya da son `T_canlı` içinde hiç zaman çerçevesi almadıysa taşımaya verir; vaktinde gönderilemeyen `IDA_NAV`, `IDA_STATUS`, `IDA_FC_STATUS` ve sayaç çerçevesinin yerini aynı türün yenisi alır, diğerleri sırada bekler ve kuyruk dolarsa atılıp `tx_dropped`'e sayılır.
- **A1-K91.** Kart `IDA_NAV`'ı `IDA_RC_OK` dönen ve `batch_seq` 4'ün katı olan step çıktılarından, `IDA_STATUS` ve `IDA_FC_STATUS`'u kendi duvar saatiyle 100 ms'de bir son çıktıdan, `IDA_CORE_COUNTERS` ve `IDA_LINK_STATS`'ı 1 s'de bir gönderir.
- **A1-K92.** Kart her çağrının `out.results[]` öğelerinin her biri için bir `IDA_CMD_RESULT` gönderir (A1-K67).
- **A1-K93.** Kart her çağrının `out.logs[]` öğelerinin her biri için bir `IDA_LOG` gönderir.
- **A1-K94.** Kart her çağrının `out.det_tx[]` öğelerinin her biri için bir `IDA_DET_RECORD` gönderir (kart içi sonar algılamasının değerlendirme kaydı; parti başlığı gönderilmez).
- **A1-K95.** Kart her çekirdek çağrısı (step, zaman çerçeveli tick, ızgara tick'i) için tam bir `IDA_CALL_TRACE` gönderir; sim bunu A6'ya yazar ve gönderilemeyen iz (`tx_dropped`) o çalıştırmanın girdi tekrarını "tam değil" diye işaretletir.
- **A1-K96.** Kart oturum kurulduğunda önceki açılıştan bir hata kaydı varsa tam bir `IDA_FAULT_RECORD` gönderir.

### 4.9 SIL taşıması

- **A1-K97.** SIL'de A1 yalnız 127.0.0.1 üzerindeki bir TCP bağlantısıdır: sim bir bağlantı noktasında dinler, SIL koşucusunu `--a1-port <ondalık>` komut satırı argümanıyla başlatır, koşucu bağlanır ve iki taraf `TCP_NODELAY` kullanır.
- **A1-K98.** SIL'deki bayt akışı kartlı akışla aynıdır: aynı çerçeveler, aynı mesajlar, aynı `sysid`/`compid`, aynı oturum el sıkışması; koşucu `role = IDA_ROLE_SIL` bildirir.
- **A1-K99.** SIL'de her yeni TCP bağlantısı yeni bir akıştır (tampon boşaltma gerekmez) ve bağlantının kapanması iki tarafta oturumun bitişidir.
- **A1-K100.** SIL'de kartın duvar saati zaman çerçevesinden türetilir (A4-K11), ızgara tick'i yoktur (dolayısıyla `ASYNC` çıktı da yoktur), A1-K22'nin `T_bağ` koşulu uygulanmaz ve bariyer süre sınırı yerine A1-K76 geçerlidir.
- **A1-K101.** `protocol/ida_link` MAVLink ayrıştırma ve kodlama durumunu örneğin kendi yapısında tutar ve yalnız çağıranın durumuyla çalışan işlevleri (`mavlink_frame_char_buffer`, `mavlink_finalize_message_buffer`) kullanır; kitaplığın kanal dizilerini kullanan işlevleri kullanmaz; böylece bir süreçte birden çok SIL örneği koşabilir.
- **A1-K102.** Aynı senaryo, aynı tohum ve aynı ikililerle iki SIL koşusu, sim'in duvar saatiyle zamanlanan `IDA_KEEPALIVE` çerçeveleri ve onların kaydırdığı MAVLink başlık `seq` değerleri dışında, her iki yönde bayt bayt aynı çerçeve dizisini üretir (SIL'de kartın duvar saati sim zamanından türediği için kart tarafındaki zamana bağlı alanlar da belirlenimcidir).

---

## 5. Hata durumları

| # | Durum | Algılama | Sim'in davranışı | Kartın davranışı |
|---|---|---|---|---|
| H1 | Bozuk çerçeve (CRC) | Sağlama | Atar, sayar, yeniden eşler (A1-K07) | Aynı |
| H2 | Yanlış başlık: `incompat_flags ≠ 0`, MAVLink 1, yabancı `sysid`/`compid`, bilinmeyen ya da yönü yanlış kimlik, geçersiz `len` | Başlık alanları | Atar, sayar (A1-K02–K06) | Aynı |
| H3 | Kartta alım tamponu taşması | DMA yazma konumu okuma konumunu geçti | — | İşlenmemiş baytları atar, yeniden eşler, partiyi eksik sayar (A1-K09) |
| H4 | Parti üyesi kayıp ya da yinelenmiş | `n_members` ya da `IMU` bayrağı tutmuyor | `BATCH_INCOMPLETE` bayrağını sayar; çalıştırmayı geçersiz saymaz (eksik veriyi çekirdek kendi kurallarıyla ele alır) | Partiyi eksik kapatır, alınanları verir (A1-K30); yinelenen örneği çekirdek `seq` ile reddeder (A4-K33) |
| H5 | TF kaybı | Sonraki TF'de `batch_seq` boşluğu | Kayıp partinin yuvası boş kalır → kaçırıldı (A1-K75) | Kayıp partinin üyeleri sonraki partiye karışır; parti eksik ve atlanan sayılır (A1-K30, K31) |
| H6 | Yinelenen ya da eski TF | `batch_seq` ≤ son kapanan | — | Pencereyi atar, çekirdeği çağırmaz, yanıt yok (A1-K31) |
| H7 | TF'lerin sıkışık varışı | Bir çağrı sürerken parti kapanması | Birleştirilen partileri `parti_kacirildi_birlesik` sayar | A4-K102 birleştirmesi; `n_merged` (A1-K40) |
| H8 | Yanıt geç, yinelenmiş, sıra dışı ya da eski oturumdan | A1-K72, K27 | Reddeder, sayar; hiçbir partinin yerine uygulamaz | — |
| H9 | Yanıt uygulama anına yetişmedi | `a_n`'de yuva boş | Bariyerde `T_bekle`'ye kadar bekler; sonra ya da 1x'te kaçırıldı; çalıştırma geçersiz (A1-K75, K79) | — |
| H10 | Kullanım süresi doldu | Sim zamanı ≥ son kullanım anı | İticiler nötr (A1-K77) | — |
| H11 | Kartta IMU kesildi | Çekirdeğin bayatlık kuralı (A4-K39) | Eşzamansız güvenli çıktıyı uygular, eski olağan yuvaları iptal eder (A1-K78) | Izgara tick'inin nötr güvenli çıktısını `ASYNC` ile gönderir (A1-K44) |
| H12 | Olay çerçevesi kayboldu | Yuva `n_events` tamamlanmadı | Bariyerde bekler; olay yinelemesi gelirse geç uygulamaya çalışır (A1-K73, K83) | `T_olay_yin` aralıkla yineler (A1-K80) |
| H13 | Olay sonucu kayboldu | Kartta sonuç gelmedi | Yinelemeye saklı sonucu yeniden gönderir (A1-K81) | Yineler; süre dolarsa `EXPIRED_NO_ACK` (A1-K80) |
| H14 | Olayın son geçerliliği geçti | A1-K84 | `REJ_EXPIRED` | Sonucu çekirdeğe verir |
| H15 | Olay eski açılıştan ya da eski oturumdan | `event_id` üst 32 biti; `sess` | `REJ_SESSION` (A1-K82); `sess` eskiyse çerçeveyi atar (A1-K27) | Oturum sonunda yinelemeyi bırakır, `SESSION_END` (A1-K22) |
| H16 | Emniyet girdileri ya da izin atışa izin vermiyor; tüp boş | A1-K88 | `REJ_SAFE` / `REJ_TUBE_EMPTY`; atış uygulanmaz | Sonucu çekirdeğe verir (atış kontrolü kendi kuralıyla tepki verir; W4) |
| H17 | Eski bir `SAFETY_OPEN` daha yeni bir `SAFETY_CLOSE`'tan sonra geldi | `event_id` karşılaştırması | `REJ_SAFE` (A1-K86) | — |
| H18 | Ping360 satırı eksik, bozuk ya da yetim | A1-K50, K51 | — | Satırı atar, sayar |
| H19 | Ping360 satırları pay hızından hızlı | Kuyruk dolu | Eski satırı atar (A1-K37) | — |
| H20 | Kritik ve atanmış üyeler `B_pencere`'yi aştı | Pencere planı | Üyeleri yine gönderir, `pencere_tasma` sayar, çalıştırma geçersiz (A1-K79); ön denetim A5 değerleriyle bunu önceden reddeder | — |
| H21 | Komut alanı A4'e çevrilemiyor | Tablo 3.6 | Sonucu A3'e iletir | `LINK_BAD_FIELD` (A1-K64) |
| H22 | Daraltıcı komut yağmuru | Aynı türden birden çok | Partide türe bir tane; birleştirir (A1-K61) | Mandal; üzerine yazılan için `COALESCED` (A1-K63, K67) |
| H23 | Daraltıcı olmayan komut kapasitesi aşıldı | A4-K105 | Sonucu A3'e iletir | Erteler ya da atar; atılana `LINK_DROPPED` (A1-K67) |
| H24 | Komut eski oturumdan | `sess` | — | Atar (A1-K25) |
| H25 | El sıkışmada uyumsuzluk | A1-K14 | `REJECT_*`; çalıştırma başlamaz; A3'e hata | `IDLE` |
| H26 | Kart yanıt vermiyor | `T_hello` yinelemesine yanıt yok | Yinelemeyi sürdürür, A3'e bildirir | — |
| H27 | Planlı yeniden başlatma tamamlanmadı | `T_reboot` | A3'e hata (A1-K17) | — |
| H28 | Kart çalışırken sıfırlandı | `KOSUYOR`'da `BOARD_HELLO` | Oturum biter, iticiler nötr, izinler kapalı, çalıştırma geçersiz (A1-K23) | Açılıştan başlar; olay kimlikleri yeni açılış sayacıyla |
| H29 | Kablo çekildi ya da aygıt kalktı | İşletim sistemi olayı ya da `T_bağ` | A1-K23; portu 1 s aralıkla yeniden açar | `T_bağ` sonra oturum sonu, daraltma (A1-K22) |
| H30 | Sim çöktü ya da yeniden başladı | Kartta `T_bağ` ya da yeni `nonce`'lu `SIM_HELLO` | Yeni süreç A1-K12 ile başlar | A1-K22, K13 |
| H31 | PC portu kapalıyken kartın gönderdikleri (MP1 bulgusu) | — | Port açılışında tamponu boşaltır, eşleşen `BOARD_HELLO`'ya kadar her şeyi atar (A1-K12) | Oturumsuzken yalnız duyuru gönderir (A1-K11) |
| H32 | HIL'de duraklatma | Kullanıcı | TF durur, canlılık sürer, çalıştırma geçersiz (A1-K41) | IMU bayatlar; çekirdek nötr güvenli çıktı üretir |
| H33 | Kart platform güvenli beklemesinde | `init_rc = NOT_INIT` duyurusu ya da `SESSION_CLOSE(SAFE_STATE)` | Oturum açmaz; A3'e bildirir | A1-K13, K21 |
| H34 | Alım damgası çerçeveye bağlanamıyor | Damga halkası | — | Önceki olayın damgası, `stamp_coarse` (A1-K48) |
| H35 | SIL koşucusu asıldı ya da bağlantı kapandı | `T_SIL` ya da TCP | Oturum biter, A3'e hata (A1-K76) | — |
| H36 | Kartın parametre bloğu beklenenden farklı | `param_blob_crc` | Hazırlık oturumu ve eşitleme (A1-K19) | — |
| H37 | Yukarı yönde kritik olmayan talep pencereyi aşıyor | Gönderim kuyruğu | — | Önce `DET_RECORD`, sonra `LOG` seyreltilir; `tx_dropped` (A1-K90) |
| H38 | Sim gerçek zamanın gerisinde | TF yazım gecikmesi | `tf_gec_yazim_ns`; geride kalınca A1-K39 ile yetişir; yanıt kaçırılırsa geçersiz | Sıkışık varışı birleştirir (A1-K40) |

### 5.1 Hata enjeksiyonu (sim)

Sim'in hata enjektörü A1 çerçeveleri üzerinde, her yönde ayrı ve mesaj adına (`protocol/ida.xml`'deki `IDA_*` adları) göre seçilerek çalışır; işlemler: çerçeve düşürme, çoğaltma, komşu çerçevelerin yer değiştirmesi, bayt bozma, kesme, gecikme sıçraması, çöp bayt ekleme (06 §2.12). Her işlem tohumludur ve A6'ya yazılır. Beklenen birincil belirtiler: bozma ve kesmede `rx_crc_err`; düşürmede `batch_incomplete` (üye) ya da `batch_skipped` ve `parti_kacirildi` (TF), `olay_eksik` (olay); çoğaltmada `batch_dup` (TF), çekirdeğin `dup_seq`'i değil `sample_rejected`'i (üye), `yanit_ret_yinelenen` (yanıt), `DUPLICATE` (komut ve olay); çöp baytta `rx_resync_bytes`. Bir enjeksiyon birden çok belirti üretebilir. Fiziksel bağlantıda enjeksiyon yapılmaz; enjektör taşıma ile ayrıştırıcı arasında, sim'dedir.

---

## 6. Kalite kademelerinin etkisi

- A1'in mesaj kümesi, tel biçimi, oturum ve zamanlama kuralları her kademede aynıdır (iskelet §6).
- Sonar kademesi Ping360 satırının içeriğini ve `n_samples`'ı değiştirebilir; iki kademede de üst sınır 1 200 örnektir ve bant bütçesi bu sınırla hesaplanmıştır (3.10). Kademe karta bildirilmez.
- Kamera kapalı kademede algılama partisi gelmez; sim A8-K64'ün `NO_FRAME` başlığını 1 Hz'te gönderir (A1-K58).
- Kayıt kademesi "yalnız olaylar" iken A1 trafiği tam kaydedilmez ve girdi tekrarı güvencesi yoktur (iskelet A6).
- Emniyet zinciriyle ilgili bütün kurallar (A1-K22, K23, K56, K61, K63, K77, K78, K80–K89, K103) her kademede aynıdır ve aynı sınamalardan geçer.

---

## 7. Uygunluk

Test kodu bu işte yazılmaz. Tür kısaltmaları: **D** derleme, şema ya da statik denetim; **B** birim (`ida_link` ya da sim'in HIL köprüsü tek başına); **S** sözleşme testi (bir taraf gerçek, karşı taraf baytlarla konuşan sahte uç); **SIL** sim ve SIL koşucusu; **K** kartlı (HIL).

### 7.1 Kural başına sınama

| Kurallar | Tür | Sınama yöntemi ve gözlenecek büyüklük |
|---|---|---|
| K01 | K | Kartlı oturumda iki yönde 600 s akış (MP3 yönteminin ürün çerçeveleriyle yinelenmesi): kayıp ve `rx_crc_err` 0 |
| K02, K04–K07 | B, S | Her başlık hatası türüyle üretilmiş çerçeve dizileri; atılan çerçeve sayısı ve ilgili sayaç; ardından gelen geçerli çerçevenin kabulü |
| K03 | D | Üretilmiş başlıktaki her ayna numaralandırma değeri ile `port.h` sabitinin derleme zamanı eşitlik denetimi (`_Static_assert`) |
| K05 | B, S | Ek B dizilerinin kırpılmış ve kırpılmamış biçimleri; `len` = 0 ve `len` > azami: ret |
| K07 | B | Rastgele bayt akışına gömülü geçerli çerçeveler: bozulan adayın kapladığı bölgeden sonraki ilk geçerli çerçevenin yakalanması |
| K08 | B | `seq` boşluklu dizi: `rx_seq_gap` artışı, davranış değişmez |
| K09 | K | Kart alım işini yapay olarak durdurup taşma üretme: `rx_overflow`, yeniden eşleme, eksik parti |
| K10 | B, S | Gönderim yolunda kısmi yazım: bayt akışında iç içe çerçeve yokluğu |
| K11–K17 | S, SIL, K | El sıkışma senaryoları: başarılı; her `REJECT_*`; aynı `nonce`'un yinelenmesi (oturum bitmez); yeni `nonce` (oturum biter, `SESSION_END` çağrısı); `SESSION_ACCEPT` kaybı (sim `T_hazır` sonra yeniden dener); `REBOOT` (kartta `reset_cause = SOFTWARE`, `boot_count` artar) |
| K18, K19 | SIL, K | Bloğu farklı kart: hazırlık oturumu, `PARAM_SET` × N, `COMMIT`, `SAVE`, `REBOOT`; sonunda `param_blob_crc` beklenene eşit |
| K20, K21 | S | Trafiksiz bekleyen iki uçta 100 ms'de bir `KEEPALIVE`; `SESSION_CLOSE`'un iki yönde hemen oturum sonu üretmesi |
| K22, K23 | S, SIL, K | Silahlı ve izinli durumda: kablo çekme (kartlı, elle), sim sürecini öldürme, `T_bağ` aşımı: kartta aynı çağrıda `armed = 0` ve `SAFETY_CLOSE` (A4-K73), sim'de iticiler nötr ve izinler kapalı; yeni oturumda eski komutların ve olayların etkisizliği |
| K25–K27 | S | Eski `sess`'li TF, komut ve olay sonucu: atılma ve sayaç; `RUNNING` dışı üyeler atılır |
| K28–K31 | B, S, SIL | Parti dizileri: olağan, üye kaybı, TF kaybı, `DUP`, `OLD`, büyük `GAP`; kapanış, sayaçlar, çekirdek çağrısı sayısı, yanıt sayısı |
| K32 | S | Sahte çekirdekle: IMU'lu parti → step, IMU'suz → tick; `tf` alanları |
| K33–K38 | B (sim) | Pencere planlayıcısına sentetik `t_hazır` ve boy dizileri: atama tablosunun aynı girdiyle iki koşuda bit düzeyinde aynılığı; sınırların hiçbirinin aşılmaması; kritik üyelerin ertelenmemesi; Ping360 seyreltmesi; kamera partisinin `P_k`'da ve bölünmeden |
| K39, K40 | K | Kartta TF varış aralıklarının dağılımı ve `n_merged` sayısı (W4); sim tarafında ardışık TF yazımları arası en küçük aralık ≥ 2,5 ms |
| K41 | SIL, K | Duraklatma ve sürdürme: `batch_seq` ve `t_sim_ns` sürekliliği; HIL'de geçersizlik bayrağı |
| K42, K43 | D, inceleme | Mesaj alanlarında karşı tarafın duvar saatini taşıyan ya da ona çeviren alan olmadığının incelemesi |
| K44, K45, K78 | S, SIL, K | IMU kesilmesi: ızgara tick'inin `ASYNC` çıktısı (en çok 100 Hz), sim'de bir sonraki fizik adımında uygulanması ve önceki olağan yuvaların iptali |
| K46–K48 | S, K | Her sensör mesajının A4 alanlarına eşlemesi (Tablo 3.4b) için alan alan karşılaştırma; çevrilemeyen değerde `VALID = 0`; kartta damgaların akış sırasında azalmaması ve `t_rx ≤ now_w` (A4-K14) |
| K49–K53 | B, S | `n_samples` ∈ {200, 239, 240, 241, 600, 1199, 1200}; sonu sıfır olan son parça; eksik, yinelenmiş, çelişkili parça; HEAD'siz parça; iki yuva doluyken üçüncü satır; birden çok pencereye yayılan satır; teslim partisinin HEAD'in partisi olması; kutu zamanı formülünün sayısal referansla karşılaştırılması |
| K54–K58 | S | Ping1D, DVL, emniyet ve algılama mesajlarının alan alan eşlemesi; aynı pencerede kapanma ve açılmanın iki partiye bölünmesi; algılama alanlarının bit düzeyinde aynı taşınması (A8 altın kayıtlarıyla) |
| K59–K64 | S, SIL | Her komut türü ve her daraltıcı için mesaj → A4 eşlemesi; dönüşüm hataları; daraltıcı yağmurunda mandal bitleri (A4-K46 testiyle birlikte) |
| K65–K67 | S, SIL | `cmd_id` tekliği; oturum değişiminde `last_cmd_id` üstünden devam; her komut çerçevesine en az bir sonuç; çoğaltılmış komutta ikinci sonucun `DUPLICATE` |
| K68, K69 | SIL, K | `PARAM_SAVE` sonucunun `save_crc`'si ile kartın sonraki `param_blob_crc`'sinin eşitliği; çok partiye yayılan görev yüklemesinin atomikliği |
| K71–K77 | B (sim), SIL | Bariyer ve kabul kuralları: gecikme dağılımı taramasında kaçırmanın yalnız ve tam olarak `a_n`'de yuva boşken sayılması; uygulama anlarının her koşulda `a_n`; `THR_VALID = 0` yanıtında komutun değişmemesi; kullanım süresi dolunca nötr; geç ve yinelenen yanıtın reddi |
| K79 | SIL, K | Her geçersizlik nedeninin tek başına üretilmesi ve bayrağın kalkması; nedensiz koşuda bayrağın kalkmaması |
| K80–K89, K103 | B (sim), S, SIL | Olay kaybı, sonuç kaybı, yineleme, süre aşımı, eski açılış, eski `SAFETY_OPEN`; her `REJ_*` kodunun tek başına üretilmesi; aynı adımda `SAFETY_CLOSE`, emniyet girdisi değişimi ve `FIRE` sırası; `DUPLICATE`'te özgün `t_apply` |
| K90–K96 | S, K | Yukarı yön önceliği: kritik çerçevenin önünde yazımı süren en çok bir kritik olmayan çerçeve; hızların ölçümü; her çağrı için tam bir `IDA_CALL_TRACE` |
| K97–K102 | SIL | Aynı senaryonun iki SIL koşusunda iki yönün bayt dizilerinin (A1-K102'nin istisnaları dışında) eşitliği; aynı süreçte iki SIL örneği |
| 3.9 (E1–E13) | D, inceleme | Değerler değiştiğinde eşitsizliklerin otomatik denetimi (yapılandırma doğrulayıcısı) |
| 3.10 | B (sim), K | Ön denetimin bant hesabı; kartlı 600 s koşuda iki yönün ölçülen ortalama ve en kötü pencere baytı |

### 7.2 Uçtan uca zamanlama ölçümü

Bütçenin (3.9) kalemleri kartlı koşuda ayrı ayrı ölçülür: sim'de `W(t_n)`'den TF yazım başlangıcına (`tf_gec_yazim_ns`), TF yazım başlangıcından `IDA_ACTUATOR`'ın alımına (sim duvarı; gidiş-dönüş), kartta `rx_to_tx_us`; gidiş-dönüşten `rx_to_tx_us` çıkarılınca taşıma payı kalır (iki saat karşılaştırılmaz, A1-K42). Ölçütler: kaçırılan parti sayısı ve en büyük gidiş-dönüş, 30 dakikalık bariyerli koşuda (W4).

### 7.3 Bu işte yapılan doğrulama (makinece okunur tanım)

- **Şema:** `protocol/ida.xml`, pymavlink 2.4.50'nin `mavgen` aracıyla `lxml` yüklüyken üretildi; araç dosyayı kendi `mavschema.xsd` şemasına göre doğruladı ("Validating … ida.xml") ve 37 mesaj buldu.
- **C başlıkları:** `--lang=C --wire-protocol=2.0` ile üretilen başlıklar `cc -std=c17 -pedantic-errors -Wall -Wextra -fsyntax-only` (Apple clang 17.0.0, macOS) ile uyarısız derlendi; `c++ -std=c++20 -Wall -Wextra -fsyntax-only` ile de uyarısız. Üretilmiş başlıklar depoya girmez.
- **`common.xml` denemesi:** dialect `common.xml`'i içerdiğinde üretilen `common/common.h` aynı bayraklarla dört hata verdi (ISO C, `int` aralığını aşan numaralandırıcıları kısıtlar: `MAV_SYS_STATUS_EXTENSION_USED`, `MAV_SYS_STATUS_SENSOR_ENUM_END`, `HIL_SENSOR_UPDATED_RESET`, `HIL_SENSOR_UPDATED_FLAGS_ENUM_END`); bu yüzden dialect standart dosyaları içermez (3.2).
- **Altın diziler (Ek B):** 37 mesajın her biri için üç bağımsız yolla: (1) bu işte yazılan, yalnız MAVLink serileştirme belgesindeki kurallara (alan sırası, `CRC_EXTRA`, CRC-16/MCRF4XX, kırpma) dayanan standart kitaplık kodlayıcısı; (2) pymavlink'in ürettiği Python dialect'i; ikisi 37 mesajın 37'sinde bayt bayt aynı. (3) Üretilmiş C başlıklarının `*_encode_status` işlevleriyle 46010, 46030 (kırpılmış son Ping360 parçası) ve 46060 kodlandı; üçü de aynı. CRC-16/MCRF4XX denetim değeri ("123456789" → 0x6F91) üretilmiş C işleviyle elde edildi ve RevEng katalogundaki değerle aynı.
- **Kitaplığın örnek başına durum işlevleri:** üretilmiş `mavlink_helpers.h`'te `mavlink_frame_char_buffer(rxmsg, status, c, r_message, r_status)` ve `mavlink_finalize_message_buffer(msg, sysid, compid, status, min_len, len, crc_extra)` işlevlerinin durumu çağırandan aldığı kaynaktan okundu; ayrıştırıcının bozuk CRC'de aday çerçeveyi bütünüyle tükettiği, `incompat_flags`'te bilinmeyen bit görünce hemen boşta durumuna döndüğü ve `MAVLINK_CHECK_MESSAGE_LENGTH` tanımlıyken kırpılmış (kısa) yükleri de reddettiği görüldü (bu yüzden A1-K05'in uzunluk denetimi `ida_link`'te yapılır, bu makro tanımlanmaz). Bu işlevlerle birden çok örneğin aynı süreçte çalışması koşturularak **doğrulanmadı**.
- **`dialect_hash`:** 3.2'deki tanımla standart kitaplıkla hesaplandı.
- **Doğrulanmadı:** `arm-none-eabi-gcc` ile derleme (makinede yok); kartta kodlama ve ayrıştırma maliyeti (MP5); MAVLink belgesindeki bir mesajın en çok 64 alan sınırı (MAVLink XML tanımı) bu dialect'te sağlanıyor (en çok 28 alan, `IDA_BOARD_HELLO`) ama araçla ayrıca sınanmadı.

---

## 8. Açık kalanlar

### 8.1 Ölçüm ya da karar bekleyenler

| # | Konu | Etiket | Kapanacağı yer |
|---|---|---|---|
| O1 | Uçtan uca bütçe (3.9): sim yazım gecikmesi, `C_sim`, USB/ST-LINK ek gecikmesi, kartta dağıtım ve kodlama | Tahmin | W4 HIL köprüsü ve kartlı 30 dakikalık bariyerli koşu (7.2); W3 sonunda gerçek çekirdeğin adım süresi (M5) |
| O2 | Kartta MAVLink ayrıştırma ve kodlama maliyeti ve 1 200 B'lık pencerenin kart alım yoluna yükü | Ölçülecek | MP5 ya da protokol kütüphanesinin ilk sürümü (W2); sonuç adım bütçesini aşarsa yedek biçim (iskelet §8, Ö2) ayrı karardır |
| O3 | MP3'teki 10–14,5 ms'lik seyrek gecikmelerin nedeni ve temiz koşulda sıklığı | Ölçülecek | MP3 temiz koşul yinelemesi |
| O4 | `T_bekle`, `T_kullanım`, `T_bağ`, `T_canlı`, `T_olay_yin`, `T_sonuç_pay`, `apply_by_ns` ve `hold_ns` varsayılanları | Tahmin | SIL arıza senaryoları ve kartlı koşu (W4); E1–E13 korunarak |
| O5 | `B_pencere` (%80) ve `B_p360` | Tahmin | Kartlı 600 s koşu (7.1, 3.10 satırı); MP3'te yalnız sabit 256 B çerçeveli %80 yük sınandı |
| O6 | SIL TCP geri döngüsünün gidiş-dönüşü ve koşu hızına etkisi (AB-215) | Ölçülecek | W4 SIL koşucusu |
| O7 | İşletim sisteminin uyku ve USB güç yönetiminin gidiş-dönüşe etkisi; ST-LINK köprüsünün kendi gecikmesi (AB-215) | Ölçülecek | W4 kartlı koşu; MP8 Windows |
| O8 | Kartta TF varışlarının sıkışma sıklığı ve birleştirme oranı (A1-K40) | Ölçülecek | W4 |
| O9 | Ping360 satır hızının gerçekçi üst sınırı (motor ve işlem süreleri 05'te parametre) | A5 / sensör modeli | Sonar modeli uygulaması (W4) ve A5 |
| O10 | DTR/RTS hatlarının ST-LINK V3 sanal seri portunda kartı etkileyip etkilemediği | Doğrulanmadı | Kart yazılımının ilk sürümü (W3); A1-K01 davranış bağlamaz |
| O11 | `IDA_CALL_TRACE`'in girdi tekrarı için yeterliliği (birleştirme ve kapasite düşmelerinde hangi üyenin hangi çağrıya girdiği izde yoktur; sim bunu partinin içeriği ve `n_merged` ile yeniden kurar) | Tahmin | A6 tekrar oynatıcısının ilk sürümü (W5) |

### 8.2 Başka sözleşmelerden beklenenler

**A4'ten beklenen**

1. `IDA_REASON_*` değer alanında 128–191 aralığının A1 bağlantı katmanı nedenlerine ayrılması (`LINK_DROPPED`, `LINK_SESSION_END`, `LINK_BAD_FIELD`, `LINK_STALE_SESSION`; A1-K64, K67).
2. A4-K66'nın "süresi geçmiş olay uygulanmaz" kuralının `SAFETY_CLOSE` için de geçerli olduğunun bilinçli bir seçim olarak kaydı (A1-K87 bunu izler; izin `hold_ns` ile zaten düştüğü için güvenli yöndedir); değiştirilmek istenirse A1-K84 ve K87 birlikte değişir.
3. `apply_by_ns` için alt sınır `L_act + P` = 15 ms'nin (E6) atış kontrol parametresinin (W4) doğrulamasına eklenmesi.
4. `valid_for_ns` çekirdek parametresinin varsayılanının 100 ms olması (A4-K58; E2, E3).

**A8'den beklenen**

1. Kart → sim yönünde kart içi sonar kayıtlarının parti başlığı olmadan, yalnız kayıtlarla (`IDA_DET_RECORD`) taşınmasının kabulü (A4 `out.det_tx[]` başlık taşımaz; A1-K94).

**A3'ten beklenen**

1. Tablo 3.11a sayaçlarının ve çalıştırma geçerliliği nedenlerinin gösterimi; `IDA_STATUS`, `IDA_FC_STATUS`, `IDA_NAV` alanlarının panelde gösterimi.
2. Panel komutlarının sim ağ geçidine Tablo 3.6'daki türlerle ve sınıflarla verilmesi; sınıfların `protocol/ida.xml`'den okunması (A1-K60).
3. Sanal emniyet girdilerinin (acil durdurma, fiş, pim) denetimi; bunların A1 mesajı olmadığı, etkilerinin `IDA_SAFETY` ile karta gittiği.

**A5'ten beklenen**

1. Otopilot parametresi `valid_for_ns` = 100 ms; atış kontrol parametreleri `apply_by_ns` ≥ 15 ms (varsayılan 20 ms) ve `hold_ns` (varsayılan 200 ms) (3.9).
2. İtici modelinin nötr durumunun "sıfır itki isteği" olarak tanımlanması; ESC "sinyal kaybı" seçeneğinin (son değeri tutma) A1 nötrü için kullanılmaması (A1-K77; AB-001).
3. Sensör hızları ve örnek sayıları, Ping360 tarama yapılandırması (`n_samples` 200–1 200, `sample_period` 80–40 000), bağlantı emülatörü profilleri; ön denetimin 3.10 hesabını bu değerlerle yapabilmesi.
4. Otopilot parametre kümesinden A4 3.10 bloğunun belirlenimci olarak üretilebilmesi (A1-K18, K19).

**A6'dan beklenen**

1. İki yöndeki bütün A1 çerçevelerinin sim duvar saatiyle alım ve gönderim damgaları, ilgili sim zamanı ve parti atamasıyla kaydı; her bariyer kararının (zamanında, bekleme, kaçırıldı, iptal) ve uygulanan fizik adımının kaydı; Ping360 seyreltme, komut erteleme ve pencere planı kararlarının kaydı.
2. `IDA_CALL_TRACE`'lerin kaydı ve girdi tekrarında kullanılması (`bg_slices` dahil; A4-K109).

**Uygulama işlerinden beklenen**

1. Kart yazılımı (W3): A1-K09 ve K48'in alım yolu ve damga halkası; A1-K90'ın gönderim önceliği; A1-K17'nin planlı yeniden başlatması; `last_cmd_id`'nin tutulması.
2. HIL köprüsü (W4): pencere planlayıcısı (A1-K33–K39), bariyer (A1-K71–K79), olay uygulayıcısı (A1-K80–K89), sayaçlar (3.11).
3. Fizik çekirdeği (04): A1-K85'teki sıranın fizik adımı sınırına bağlanması; A1-K88'deki fırlatma tetiğinin 04'ün darbe modeline bağlanması.

### 8.3 İskelete öneriler

İskeletin tanımsız bıraktığı ya da yalnız sürekli komut için yazdığı durumlar; bu sözleşme en yalın tutarlı kuralı yazdı, iskelete işlenmesi önerilir:

1. **Kullanım süresinin başlangıcı** (iskelet §4 "Komutun ömrü"): süre, yanıtın sabit uygulama anından (`a_n`) sayılır (A1-K74); geç yanıt yeni ömür kazanmaz çünkü reddedilir.
2. **`THR_VALID = 0` yanıtı:** yuvayı doldurur (parti kaçırılmış sayılmaz) ama yürürlükteki komutu değiştirmez (A1-K74).
3. **Eşzamansız güvenli çıktı:** iskelet §4'ün sabit uygulama anı kuralının tek istisnasıdır; alımdan sonraki ilk fizik adımında uygulanır ve önceki olağan yuvaları iptal eder (A1-K78).
4. **Tek seferlik olayların zamanlaması:** iskelet §4 yalnız sürekli komutun geç kalmasını tanımlar; olaylar için: zamanında gelen olay yuvasıyla aynı adımda, geç gelen olay son geçerliliği içinde alımdan sonraki ilk adımda uygulanır ve çalıştırmayı geçersiz kılar (A1-K83, K79).
5. **Sim'de aynı fizik adımındaki sıra:** emniyet girdisi değişimleri ve izin kapatmaları sürekli komuttan ve izin açan ve atış olaylarından önce (A1-K85); iskelet §3.2 yalnız çekirdek içindeki sırayı yazar.
6. **Parti üyeliği:** üyelik akıştaki konumla belirlenir, üyeler parti numarası taşımaz (A1-K28); iskelet §4 "partinin bittiğini işaretler" ifadesiyle uyumludur.

---

## 9. Açık bulgu kapanış tablosu

Kaynak: `docs/mimari/acik-bulgular.md` "A1 — Sim sunucusu ↔ kart" başlığı (49 bulgu: AB-001…AB-047, AB-234, AB-235) ve iskelet §10'un son paragrafında anılan AB-003, AB-211, AB-215, AB-224. Bulgu metinleri değiştirilmedi.

| Bulgu | Önem | Durum | Sözleşmede nerede | Gerekçe |
|---|---|---|---|---|
| AB-001 | kritik | kapandı | A1-K77, K23; 8.2 A5 md. 2 | Nötr durum "sıfır itki isteği" olarak tanımlandı ve ESC sinyal kaybı modeliyle temsil edilmesi yasaklandı; kart sıfırlanması ve oturum kaybı sim'de hemen nötr üretir. |
| AB-002 | kritik | kapandı | A1-K10, K12, K13, K22, K25–K28 | Oturum sonunda kart başlamamış gönderimleri siler, başlamışı tamamlar ya da keser; dönem bilgisi el sıkışma nonce'ları, `sess` ve FIFO akıştaki konumla sağlanır; sim port açılışında tamponları boşaltır. |
| AB-003 | önemli | kapandı | A1-K48 | Alım damgası kaybında hiçbir zaman daha yeni bir zaman verilmez, önceki olayın damgası kullanılır ve sayılır; USB'ye özgü kısımlar K17 ile düştü. |
| AB-004 | önemli | kapandı | A1-K63, K61; A4-K46 | Daraltıcılar ayrıştırıldığı anda mandala gider ve tampon doluluğundan bağımsızdır; sim türe bir tane gönderir. |
| AB-005 | belirtilmemiş | kapandı | A1-K28, K32 | Tetik TF ile kapanan partidir; IMU bir üyedir (iskelet §4). |
| AB-006 | belirtilmemiş | kapandı | 3.9, 3.10; A1-K34, K56 | Kritik pencere ve bant 200 Hz ve yeni mesaj boylarıyla yeniden türetildi; emniyet geri okuması partide birdir. |
| AB-007 | belirtilmemiş | kapandı | 3.8; A1-K12–K16 | Ayrı `boot_id` yok; açılış kimliği `boot_count`, el sıkışma kimliği nonce'lardır. |
| AB-008 | belirtilmemiş | kapandı | A1-K28, K29, K32 | Bağımsız `IDA_TIME_FRAME` her periyotta gider; IMU'suz parti tick ile işlenir. |
| AB-009 | belirtilmemiş | kapandı | 3.7 `IDA_EVENT`; A1-K80–K89 | Seviye/kenar biçimi kaldırıldı; olaylar kimlikli, süreli ve onaylıdır. |
| AB-010 | belirtilmemiş | kapandı | A1-K31, K45 | Yinelenen ve eski TF'de çekirdek çağrılmaz ve yanıt gönderilmez; önbellekten çıktı yeniden gönderimi yoktur. |
| AB-011 | belirtilmemiş | kapandı | 3.4 (`seq`) | Her sensör mesajı sensörün kendi `seq`'ini taşır. |
| AB-012 | belirtilmemiş | kapandı | 3.4, A1-K46 | Basınç ölçüm zamanı pencere sonudur; bayatlık eşiği yalnız çekirdeğin parametresidir (A4-K39), A1 kendi eşiğini koymaz. |
| AB-013 | belirtilmemiş | kapandı | 3.8 TF; A1-K44, K78, K81, K74, K77 | Zaman çerçevesi alanları, eşzamansız güvenli çıktı, olay tekilleştirmesi ve kullanım süresinin alıcıda uygulanması tanımlandı. |
| AB-014 | kritik | kapandı | A1-K22, K82, K85–K89 | İzin kaldırıldığında bekleyen atış olayları: aynı adımda kapatma atıştan önce uygulanır, izin kapalıyken ya da daha yeni bir kapatmadan eski atış `REJ_SAFE` alır, oturum sonunda olaylar `SESSION_END` ile biter. |
| AB-015 | kritik | kapandı | A1-K85, K86, K88 | Aynı adımdaki daraltma ve genişletici olaylar için alıcı sırası ve `event_id` karşılaştırması yazıldı. |
| AB-016 | kritik | kapandı | A1-K81, K82; A4-K61, K86 | Açılış sayacının sahibi platformdur (A4); sim çalıştırma boyunca her `event_id`'nin sonucunu saklar, yeniden bağlantıda eski açılışın olayı `REJ_SESSION` alır. |
| AB-017 | kritik | kapandı | A1-K84, K86; 3.9 (`apply_by_ns`, `hold_ns`) | Bütün olaylar son geçerlilik taşır; iznin süresi alıcıda sim zamanında uygulama anından, üreticide duvar saatinde sayılır (A4-K66). |
| AB-018 | önemli | kapandı | A1-K52 | Kutu merkezi `(i + ½)` kuralı tek formül olarak yazıldı. |
| AB-019 | belirtilmemiş | uygulamaya devredildi | A1-K80–K89, K56 (A1 kısmı kapandı) | A1'e düşen kısım (olay önceliği, geçerlilik, oturum, onay, geri okuma) kapandı; sonar genlik baytının anlamı ve gönderim/yansıma pozunun kullanımı sonar modeli ve atış kontrol uygulamasınındır (W4). |
| AB-020 | belirtilmemiş | kapandı | A1-K72, K74; 2 (`a_n`) | Yanıt `batch_seq` taşır; planlanan uygulama anı `t_n + L_act`'tır; uygulanan adım A6'ya yazılır; 04'teki adlandırma uygulama işidir. |
| AB-021 | belirtilmemiş | kapandı | 2 (`L_act`), A1-K74, K75 | `L_act` sekiz fizik adımıdır ve yuva her parti için kurulur; komut ömrü `valid_for_ns` ile tek parametredir. |
| AB-022 | önemli | kapandı | A1-K52 | Kutu zamanı nominal doğrudan yol zamanıdır (iskelet §3.1). |
| AB-023 | önemli | uygulamaya devredildi | A1-K33, K35 | A1'in atama kuralı yalnız sim'in verdiği `t_hazır`'a dayanır; `t_hazır` ile `t_teslim`'in tutarlı üretimi sensör modelleri işinindir (W3). |
| AB-024 | önemli | kapandı | 1.3; 3.4 (PING360_HEAD aralıkları) | A1'de Ping360 yapılandırma komutu yoktur (A4 çıktısında karşılığı yok); tarama yapılandırması A5'tedir, böylece yinelenen komutun ek ping üretmesi olanaksızdır; cihaz aralıkları HEAD denetimine bağlandı. |
| AB-025 | önemli | uygulamaya devredildi | 3.4 | Etkin `n_samples` ve `sample_period` HEAD'de taşınır; kırpmadaki menzil hatası sonar modeli ve A5'in işidir. |
| AB-026 | belirtilmemiş | kapandı | A1-K54, K46 | Köprüde zaman dönüşümü yoktur; Ping1D `t_meas` yansıma anıdır. |
| AB-027 | kritik | kapandı | A1-K73; 3.7 (`n_events`) | Yanıt, çağrının yeni olay sayısını taşır ve olaylar yanıttan önce gelir. |
| AB-028 | kritik | kapandı | A1-K81 | Sim reddedilenler dahil her `event_id`'nin son sonucunu saklar ve yinelemeye aynı sonucu döner. |
| AB-029 | kritik | kapandı | A1-K85, K86, K87, K89 | İzin olayları `event_id` sırasıyla nesillenir; eski açma daha yeni kapatmadan sonra uygulanmaz; izin `hold_ns` ile düşer. |
| AB-030 | kritik | kapandı | A1-K49–K51 | Satırın teslim partisi HEAD'in partisidir ve HEAD satırın son çerçevesidir; kapanmış partiye düşme olanaksızdır. |
| AB-031 | önemli | kapandı | A1-K28, K30 | Üye indisi ve bit eşlemi kaldırıldı; üyelik akıştaki konumla, eksiklik `n_members` ile, yineleme çekirdeğin `seq` denetimiyle ele alınır. |
| AB-032 | önemli | kapandı | A1-K31, K99 | Büyük boşluklar `GAP` olarak kabul edilir (üst sınır yok); SIL'de TCP TF kaybetmez. |
| AB-033 | önemli | kapandı | A1-K72, K74 | Ömür sabit uygulama anından sayılır, geç yanıt reddedilir; `THR_VALID = 0`'ın anlamı yazıldı. |
| AB-034 | önemli | kapandı | 3.9 (bütçe ilk kalemi), 7.2 | Bütçe `W(t_n)`'den başlar; sim yazım gecikmesi ayrı kalem ve ayrı sayaçtır. |
| AB-035 | önemli | kapandı | 3.9 (bütçe), O1 | 200 Hz ve `L_act` = 10 ms ile en kötü kritik kuyruk dahil tahmini toplam 9,43 ms'dir; ölçüm O1'de. |
| AB-036 | önemli | kapandı | A1-K73, K44, K56, K90; 3.9 | Yukarı yön kritik içeriği (en çok dört olay + yanıt) ve eşzamansız çıktı hızı sınırlandı; emniyet geri okuması partide birdir. |
| AB-037 | önemli | kapandı | A1-K33–K39, K90; 3.10 | Pencere planı tam çerçeve boylarıyla yapılır; kritik olmayan yukarı yön gönderimi zaman penceresine bağlandı. |
| AB-038 | önemli | kapandı | A1-K11, K13, K27 | Aynı nonce'un yinelenmesi oturumu bitirmez; ilk kart çerçevesi gelmezse sim yeniden dener; kart sıfırlandıktan sonra duyuru gönderir. |
| AB-039 | önemli | kapandı | A1-K12, K15, K25, K27, K28 | Oturum kimliği son oturumlardan ve kartın son oturumundan farklı seçilir; veri mesajlarının oturuma bağlılığı nonce'lu el sıkışma ve FIFO akıştaki konumla sağlanır; karar taşıyan mesajlar ayrıca `sess` taşır. |
| AB-040 | kritik | kapandı | 3.4 (`IDA_SAFETY`), 3.7 (`IDA_EVENT_RESULT`); A1-K56 | Tüp başına izin durumu (`PERMIT_OPEN`) eklendi; bakım girdisi yoktur (iskelet §10 md. 8); olay sonucu `in.evres[]` ile çekirdeğe döner. |
| AB-041 | önemli | kapandı | 3.5; A1-K54 | Algılama mesajları A8'in bütün alanlarını (`t_sigma_ns`, `calib_ver` dahil) taşır; Ping1D yansıma anı. |
| AB-042 | önemli | kapandı | A1-K42; 3.7 | Mutlak kart saati damgası ve saat eşlemesi yoktur; tanı alanları süre olarak taşınır. |
| AB-043 | önemli | kapandı | 3.2 (`dialect_hash`), A1-K14 | Uzantı alanı yoktur; sürüm ve özet tam eşitlik ister. |
| AB-044 | kritik | kapandı | A1-K65–K67 | `cmd_id` sim'de komut başına tektir, yinelemelerde aynıdır, oturumlar arasında kartın son kimliğinden sürer. |
| AB-045 | önemli | kapandı | 3.7 (`IDA_FC_STATUS`, `IDA_CORE_COUNTERS`, `IDA_LINK_STATS`), 3.6 (`IDA_CMD_FC`) | Bütün alanlar ve tipleri tanımlı; `BAD_INPUT` `core_rc` ile taşınır. |
| AB-046 | belirtilmemiş | kapandı | 3.1, 3.2, A1-K101; 7.3 | Kimlikler, aralık, kanonik metin ve altı eksenli elle kumanda (`IDA_CMD_MANUAL`) tanımlı; örnek başına durum işlevlerinin varlığı kaynaktan doğrulandı, çok örnekli koşu sınanmadı (7.3). |
| AB-047 | belirtilmemiş | kapandı | 3.5, 3.10 | Partide en çok 4 kayıt (A8); bütçe 399 B/kare ile hesaplandı. |
| AB-234 | önemli | kapandı | A1-K11, K12; H31 | Port açılışında tampon boşaltma, nonce eşleşmesine kadar atma, oturumsuz kartın yalnız duyuru göndermesi. |
| AB-235 | kritik | uygulamaya devredildi | 3.9 (bütçe), O1 | A1 200 Hz'e ve A4'ün 3,5 ms'lik adım bütçesine göre yazıldı; gerçek çekirdeğin ölçümü W3 sonunun işidir (M5). |
| AB-211 | önemli | kapandı | 3.9 | USB kısmı K17 ile düştü; sanal seri port için tam kritik yük dahil bütçe `L_act` = 10 ms ile yeniden kuruldu (tahmin; O1). |
| AB-215 | belirtilmemiş | açık | O6, O7 | 400 Hz ve USB önerileri K17 ve iskelet 0.8 ile düştü; SIL geri döngü, işletim sistemi güç yönetimi ve ST-LINK köprüsü gecikmesi ölçümleri bekliyor. |
| AB-224 | belirtilmemiş | uygulamaya devredildi | 7.1 (K22, K23 satırı) | Kablo çekme sınaması kartlı testte elle yapılır; yazılımla kesilebilir USB çoklayıcı seçimi W4 HIL test işinindir. |

Özet (49 + 3 ek bulgu): 52 bulgudan 46'sı kapandı, 5'i uygulamaya devredildi (AB-019, AB-023, AB-025, AB-224, AB-235), 1'i açık (AB-215; belirtilmemiş önem). Kapsam dışı yok. Açık kalan kritik bulgu yoktur; AB-235 (kritik) uygulamaya (W3 ölçümü) devredildi.

**Uyumsuzluk tablosunda A1'e değinen satırlar** (bilgi; tablo sahibi mimar):

| U | Bu sözleşmedeki karşılığı |
|---|---|
| U-01 | Olay taşıması A1-K73, K80–K89 |
| U-02 | Zaman çerçevesi ve parti kapanışı A1-K28–K32 |
| U-03 | Olay kimliği yalnız açılış sayacından; `boot_id` yok (3.8) |
| U-04 | Aralık ölçümlerinde ölçüm zamanı aynen aktarılır (3.4); Ping1D A1-K54 |
| U-06 | Kutu zamanı A1-K52 |
| U-08 | Komut kapasitesi ve olay sahipliği A1-K34, K63, K67 |
| U-09 | Algılama alanları ve hızı 3.5, 3.10 |
| U-10 | `IDA_SAFETY` ve `PERMIT_OPEN`; bakım yok |
| U-11 | Ömrün başlangıcı ve nötr durum A1-K74, K77 |
| U-12 | Aynı adımdaki sıra A1-K85 |
| U-29 | A8 kayıt eşlemesi 3.5 |

---

## Ek A. A1 ↔ A4 alan eşleme tablosu

A4 sözleşmesinin "Ek A. A1 için dışa açılan adlar" listesindeki her ad için A1 karşılığı:

| A4 adı | A1 karşılığı |
|---|---|
| `ida_time_frame_t.present` | 1 (zaman çerçeveli çağrı); ızgara tick'inde 0 (kart) |
| `ida_time_frame_t.seq`, `.t` | `IDA_TIME_FRAME.batch_seq`, `.t_sim_ns` |
| `ida_time_frame_t.t_rx` | TF çerçevesinin alım damgası (A1-K48) |
| `ida_hdr_t.t_meas`, `.seq`, `.window_ns`, `.flags`, `.inst` | Sensör mesajlarının `t_meas_ns`, `seq`, `window_ns` (yalnız IMU, BARO, BATT; diğerlerinde 0), `flags`, `inst` |
| `ida_hdr_t.t_rx` | Çerçevenin alım damgası (A1-K48) |
| `ida_hdr_t.type` | Mesaj kimliğinden (Tablo 3.4b) |
| `ida_imu_t` (`gyro`, `accel`, `temp_c`) | `IDA_IMU` |
| `ida_mag_t.field` | `IDA_MAG.field` |
| `ida_baro_t` (`p_abs`, `temp_c`) | `IDA_BARO` |
| `ida_gnss_t` (`lat_e7`, `lon_e7`, `alt_mm`, `vel_ned`, `eph`, `epv`, `fix_type`, `n_sv`) | `IDA_GNSS` (aynı adlar; `fix_type` `IDA_GNSS_FIX`) |
| `ida_dvl_t` (`vel`, `cov`, `fom`, `beam_dist`, `altitude`, `dvl_flags`, `beam_valid`) | `IDA_DVL` |
| `ida_ping1d_t` (`distance`, `scan_start`, `scan_length`, `ping_number`, `tx_offset_ns`, `confidence`) | `IDA_PING1D` |
| `ida_batt_t` (`voltage`, `current`, `temp_c`) | `IDA_BATT` |
| `ida_leak_t.mask` | `IDA_LEAK.mask` |
| `ida_safety_t` (`estop`, `tube[2]`: `LOADED`, `PLUG_IN`, `PIN_OUT`, `PERMIT_OPEN`) | `IDA_SAFETY` |
| `ida_ping360_t` (`hdr`, `angle_grad`, `n_samples`, `sample_period`, `tx_duration_us`, `tx_freq_khz`, `gain`, `data[1200]`) | `IDA_PING360_HEAD` + `IDA_PING360_DATA` (A1-K49–K51) |
| `in.det[]`, `in.det_t_rx[]`, `in.det_batch[]`, `in.det_batch_t_rx[]` | `IDA_DET_RECORD`, `IDA_DET_BATCH` ve alım damgaları (A1-K57) |
| `out.det_tx[]` | Kart → sim `IDA_DET_RECORD` (A1-K94) |
| `ida_cmd_t` (`cmd_id`, `type`, `source`, `u.*`) | `IDA_CMD_*` (Tablo 3.6) |
| `ida_narrow_t.mask`, `.mode`, `.cmd_id[4]` | Daraltıcı komutlardan platformun kurduğu mandal (3.6; A1-K63) |
| `ida_narrow_t.coalesced[4]` | Platform sayar; üzerine yazılan komuta `COALESCED` sonucu (A1-K67); `narrow_coalesced` sayacı |
| `ida_event_result_t` (`event_id`, `result`, `t_apply`) | `IDA_EVENT_RESULT` (sim kodları); platform kodları (16–19) karttan üretilir, telde yoktur |
| `ida_plat_t.link_state`, `.link_event` | Kartın A1 durumu (A1-K16, K22) |
| `ida_plat_t.param_store`, `.param_fail`, `.param_gen` | Platformun kalıcı kaydı; `IDA_STATUS`'ta bildirilir |
| `ida_plat_t.prev_overrun`, `.tx_stalled`, `.batch_merged`, `.input_dropped` | Platform; `IDA_CALL_TRACE.plat_flags` |
| `ida_plat_t.prev_call_ns` | Platform; `IDA_CALL_TRACE.prev_call_ns` |
| `ida_plat_t.cnt_*` (12 sayaç) | Tablo 3.11b eşlemesi; `IDA_LINK_STATS` |
| `ida_in_t.bg_slices` | `IDA_CALL_TRACE.bg_slices` |
| `ida_out_t` sürekli komut alanları (`thr_valid`, `is_safe`, `armed`, `src`, `fs_level`, `seq`, `t_ref`, `valid_for_ns`, `thr_cmd[8]`) | `IDA_ACTUATOR` (3.7); `t_ref` telde yoktur (sim `batch_seq`'ten bilir); `thr_cmd` → `pwm_us` |
| `ida_event_t` (`event_id`, `kind`, `tube`, `seq`, `t_decide`, `apply_by_ns`, `hold_ns`, `fc_ref`) | `IDA_EVENT` (`seq` → `batch_seq`, `t_decide` → `t_decide_ns`) |
| `ida_cmd_result_t` (`cmd_id`, `kind`, `ack`, `reason`, `value`, `save.gen`, `save.len`) | `IDA_CMD_RESULT`; `save.blob` telde yoktur (`save_crc` ile özetlenir; A1-K68) |
| `ida_nav_out_t` (14 alan) | `IDA_NAV` |
| `ida_status_t` (`mode`, `armed`, `fs_level`, `fs_reasons`, `regime`, `phase`, `mission_state`, `mission_item`, `param_ok`, `events_ok`, `boot_count`) | `IDA_STATUS` |
| `ida_status_t.cnt.*` | `IDA_CORE_COUNTERS.cnt[18]` |
| `ida_health_t` (`core_ok`, `data_ok`, `imu_rejected`, `imu_lag`, `input_dropped`, `narrowed`) | `IDA_STATUS.health`; ayrıca `IDA_ACTUATOR` `INPUT_DROPPED`, `NARROWED` |
| `ida_log_t` (`t_w`, `code`, `severity`, `arg0`, `arg1`) | `IDA_LOG` |
| `ida_fc_status_t` (`state`, `inhibit_mask`, `last_event_id`, `last_event_result`, `ext_len`, `ext[64]`) | `IDA_FC_STATUS` |
| `ida_init_in_t` (`boot_count`, `boot_count_ok`, `reset_cause`, `reset_flags`, `consecutive_faults`, `param_src`) | `IDA_BOARD_HELLO` |
| `ida_core_info_t` (`port_major`, `port_minor`, `param_schema_hash`, `core_version`) | `IDA_BOARD_HELLO` |
| Dönüş kodları (`OK`, `IMU_REJECTED`, `DUP_SEQ`, `OLD_SEQ`, `BAD_INPUT`, `NOT_INIT`) | `IDA_ACTUATOR.core_rc`, `IDA_CALL_TRACE.core_rc` (`DUP_SEQ`/`OLD_SEQ` olağanda görülmez; A1-K31) |
| `ida_core_step`, `ida_core_tick` | A1-K32, K44 |
| `ida_core_background` | Çağrı sayısı `bg_slices` ile izlenir |
| `ida_core_param_count`, `_desc`, `_find` | A1'de karşılığı yoktur: parametre tablosu A1'de taşınmaz; sim aynı kaynaktan derlenen tabloyu kullanır ve `param_schema_hash`'le doğrular (A1-K14) |
| `ida_core_param_get` | A1'de doğrudan karşılığı yoktur; değer `IDA_CMD_SIMPLE(PARAM_GET)` komutunun sonucuyla gelir (A4-K43) |

## Ek B. Altın bayt dizileri

Her özel mesaj için bir çerçeve. Alan değerleri belirlenimci bir desenle seçilmiştir (birkaç mesajda anlamlı değerler); sim'den çıkan çerçevelerde `sysid = 250`, `compid = 25`, karttan çıkanlarda `1`, `1`. Bütün diziler 7.3'teki bağımsız kodlayıcı ve pymavlink üreteciyle bayt bayt aynıdır; 46010, 46030 ve 46060 ayrıca üretilmiş C başlıklarıyla doğrulandı. Diziler `protocol/ida.xml`'in bu adayına aittir; XML değişirse yeniden üretilir.

### 46000 IDA_SIM_HELLO

- Başlık: seq = 0, sysid = 250, compid = 25; CRC_EXTRA = 159; yük azami 22 B, çerçeve azami 34 B; bu dizide yük 21 B, çerçeve 33 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: dialect_hash = 81985529216486895; nonce = 3237998081; run_id = 7; ctrl_period_ns = 5000000; proto_major = 1; proto_minor = 0

```
fd 15 00 00 00 fa 19 b0 b3 00 ef cd ab 89 67 45 23 01 01 ee ff c0 07 00 00 00 40 4b 4c 00 01 dd
ba
```

### 46001 IDA_BOARD_HELLO

- Başlık: seq = 1, sysid = 1, compid = 1; CRC_EXTRA = 242; yük azami 82 B, çerçeve azami 94 B; bu dizide yük 82 B, çerçeve 94 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: dialect_hash = 30064771072; param_schema_hash = 30064771073; peer_nonce = 100022; nonce = 100033; boot_count = 100044; reset_flags = 100055; core_version = 100066; fw_version = 100077; fw_git = 100088; param_blob_crc = 100099; param_gen = 100110; last_cmd_id = 100121; ctrl_period_ns = 100132; param_blob_len = 1039; port_major = 1042; port_minor = 1045; det_schema_major = 1048; det_schema_minor = 1051; prev_sess = 1054; sessions_since_boot = 1057; proto_major = 101; proto_minor = 106; role = 111; reset_cause = 116; consecutive_faults = 121; boot_count_ok = 126; param_src = 131; init_rc = 136

```
fd 52 00 00 01 01 01 b1 b3 00 00 00 00 00 07 00 00 00 01 00 00 00 07 00 00 00 b6 86 01 00 c1 86
01 00 cc 86 01 00 d7 86 01 00 e2 86 01 00 ed 86 01 00 f8 86 01 00 03 87 01 00 0e 87 01 00 19 87
01 00 24 87 01 00 0f 04 12 04 15 04 18 04 1b 04 1e 04 21 04 65 6a 6f 74 79 7e 83 88 96 a9
```

### 46002 IDA_SESSION_ACCEPT

- Başlık: seq = 2, sysid = 250, compid = 25; CRC_EXTRA = 201; yük azami 16 B, çerçeve azami 28 B; bu dizide yük 16 B, çerçeve 28 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: run_id = 100000; nonce_sim = 100011; nonce_board = 100022; sess = 1009; result = 21; run_mode = 26

```
fd 10 00 00 02 fa 19 b2 b3 00 a0 86 01 00 ab 86 01 00 b6 86 01 00 f1 03 15 1a 4b fe
```

### 46003 IDA_KEEPALIVE

- Başlık: seq = 3, sysid = 250, compid = 25; CRC_EXTRA = 116; yük azami 3 B, çerçeve azami 15 B; bu dizide yük 3 B, çerçeve 15 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: sess = 1000; state = 6

```
fd 03 00 00 03 fa 19 b3 b3 00 e8 03 06 ab 9a
```

### 46004 IDA_SESSION_CLOSE

- Başlık: seq = 4, sysid = 250, compid = 25; CRC_EXTRA = 82; yük azami 3 B, çerçeve azami 15 B; bu dizide yük 3 B, çerçeve 15 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: sess = 1000; reason = 6

```
fd 03 00 00 04 fa 19 b4 b3 00 e8 03 06 72 dd
```

### 46010 IDA_TIME_FRAME

- Başlık: seq = 5, sysid = 250, compid = 25; CRC_EXTRA = 190; yük azami 17 B, çerçeve azami 29 B; bu dizide yük 17 B, çerçeve 29 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_sim_ns = 12345000000; batch_seq = 2469; sess = 6699; n_members = 4; tf_flags = 1; run_mode = 3

```
fd 11 00 00 05 fa 19 ba b3 00 40 c0 d1 df 02 00 00 00 a5 09 00 00 2b 1a 04 01 03 18 c1
```

### 46020 IDA_IMU

- Başlık: seq = 6, sysid = 250, compid = 25; CRC_EXTRA = 5; yük azami 47 B, çerçeve azami 59 B; bu dizide yük 47 B, çerçeve 59 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; window_ns = 100022; gyro = [2.0, 2.25, 2.5]; accel = [2.5, 2.75, 3.0]; temp_c = 3.0; flags = 1018; inst = 36

```
fd 2f 00 00 06 fa 19 c4 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 b6 86 01 00 00 00 00 40 00 00
10 40 00 00 20 40 00 00 20 40 00 00 30 40 00 00 40 40 00 00 40 40 fa 03 24 05 8c
```

### 46021 IDA_MAG

- Başlık: seq = 7, sysid = 250, compid = 25; CRC_EXTRA = 9; yük azami 27 B, çerçeve azami 39 B; bu dizide yük 27 B, çerçeve 39 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; field = [1.5, 1.75, 2.0]; flags = 1009; inst = 21

```
fd 1b 00 00 07 fa 19 c5 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 00 00 c0 3f 00 00 e0 3f 00 00
00 40 f1 03 15 a4 af
```

### 46022 IDA_BARO

- Başlık: seq = 8, sysid = 250, compid = 25; CRC_EXTRA = 206; yük azami 27 B, çerçeve azami 39 B; bu dizide yük 27 B, çerçeve 39 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; window_ns = 100022; p_abs = 2.0; temp_c = 2.5; flags = 1015; inst = 31

```
fd 1b 00 00 08 fa 19 c6 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 b6 86 01 00 00 00 00 40 00 00
20 40 f7 03 1f 91 79
```

### 46023 IDA_GNSS

- Başlık: seq = 9, sysid = 250, compid = 25; CRC_EXTRA = 0; yük azami 49 B, çerçeve azami 61 B; bu dizide yük 49 B, çerçeve 61 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; lat_e7 = -1014; lon_e7 = -1021; alt_mm = -1028; vel_ned = [3.0, 3.25, 3.5]; eph = 3.5; epv = 4.0; flags = 1024; inst = 46; fix_type = 51; n_sv = 56

```
fd 31 00 00 09 fa 19 c7 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 0a fc ff ff 03 fc ff ff fc fb
ff ff 00 00 40 40 00 00 50 40 00 00 60 40 00 00 60 40 00 00 80 40 00 04 2e 33 38 d4 dd
```

### 46024 IDA_DVL

- Başlık: seq = 10, sysid = 250, compid = 25; CRC_EXTRA = 162; yük azami 77 B, çerçeve azami 89 B; bu dizide yük 77 B, çerçeve 89 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; vel = [1.5, 1.75, 2.0]; cov = [2.0, 2.25, 2.5, 2.75, 3.0, 3.25]; fom = 2.5; beam_dist = [3.0, 3.25, 3.5, 3.75]; altitude = 3.5; flags = 1021; inst = 41; dvl_flags = 46; beam_valid = 51

```
fd 4d 00 00 0a fa 19 c8 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 00 00 c0 3f 00 00 e0 3f 00 00
00 40 00 00 00 40 00 00 10 40 00 00 20 40 00 00 30 40 00 00 40 40 00 00 50 40 00 00 20 40 00 00
40 40 00 00 50 40 00 00 60 40 00 00 70 40 00 00 60 40 fd 03 29 2e 33 58 f4
```

### 46025 IDA_PING1D

- Başlık: seq = 11, sysid = 250, compid = 25; CRC_EXTRA = 92; yük azami 36 B, çerçeve azami 48 B; bu dizide yük 36 B, çerçeve 48 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; distance = 1.5; scan_start = 2.0; scan_length = 2.5; ping_number = 100055; tx_offset_ns = 100066; flags = 1021; inst = 41; confidence = 46

```
fd 24 00 00 0b fa 19 c9 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 00 00 c0 3f 00 00 00 40 00 00
20 40 d7 86 01 00 e2 86 01 00 fd 03 29 2e bd 96
```

### 46026 IDA_BATT

- Başlık: seq = 12, sysid = 250, compid = 25; CRC_EXTRA = 157; yük azami 31 B, çerçeve azami 43 B; bu dizide yük 31 B, çerçeve 43 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; window_ns = 100022; voltage = 2.0; current = 2.5; temp_c = 3.0; flags = 1018; inst = 36

```
fd 1f 00 00 0c fa 19 ca b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 b6 86 01 00 00 00 00 40 00 00
20 40 00 00 40 40 fa 03 24 c2 cd
```

### 46027 IDA_LEAK

- Başlık: seq = 13, sysid = 250, compid = 25; CRC_EXTRA = 70; yük azami 16 B, çerçeve azami 28 B; bu dizide yük 16 B, çerçeve 28 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; flags = 1006; inst = 16; mask = 21

```
fd 10 00 00 0d fa 19 cb b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 ee 03 10 15 3c 1d
```

### 46028 IDA_SAFETY

- Başlık: seq = 14, sysid = 250, compid = 25; CRC_EXTRA = 86; yük azami 18 B, çerçeve azami 30 B; bu dizide yük 18 B, çerçeve 30 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; flags = 1006; inst = 16; estop = 21; tube = [26, 27]

```
fd 12 00 00 0e fa 19 cc b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 ee 03 10 15 1a 1b ac 6a
```

### 46029 IDA_PING360_HEAD

- Başlık: seq = 15, sysid = 250, compid = 25; CRC_EXTRA = 27; yük azami 27 B, çerçeve azami 39 B; bu dizide yük 27 B, çerçeve 39 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; angle_grad = 1006; n_samples = 1009; sample_period = 1012; tx_duration_us = 1015; tx_freq_khz = 1018; flags = 1021; inst = 41; gain = 46; n_frag = 51

```
fd 1b 00 00 0f fa 19 cd b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 ee 03 f1 03 f4 03 f7 03 fa 03
fd 03 29 2e 33 71 9f
```

### 46030 IDA_PING360_DATA

- Başlık: seq = 16, sysid = 250, compid = 25; CRC_EXTRA = 174; yük azami 246 B, çerçeve azami 258 B; bu dizide yük 126 B, çerçeve 138 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: seq = 4242; inst = 0; frag_idx = 4; data = [1, 2, 3, 4, 5, 6, … (240 öğe)]

```
fd 7e 00 00 10 fa 19 ce b3 00 92 10 00 00 00 04 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10
11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30
31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f 50
51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f 60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f 70
71 72 73 74 75 76 77 78 b6 e5
```

### 46035 IDA_DET_BATCH

- Başlık: seq = 17, sysid = 250, compid = 25; CRC_EXTRA = 87; yük azami 27 B, çerçeve azami 39 B; bu dizide yük 27 B, çerçeve 39 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; batch_seq = 100011; window_ns = 100022; calib_ver = 1009; producer_ver = 1012; flags = 1015; source = 31; inst = 36; status = 41; count = 46; producer_id = 51

```
fd 1b 00 00 11 fa 19 d3 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 b6 86 01 00 f1 03 f4 03 f7 03
1f 24 29 2e 33 40 e0
```

### 46036 IDA_DET_RECORD

- Başlık: seq = 18, sysid = 250, compid = 25; CRC_EXTRA = 221; yük azami 78 B, çerçeve azami 90 B; bu dizide yük 78 B, çerçeve 90 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; det_seq = 100011; batch_seq = 100022; window_ns = 100033; t_sigma_ns = 100044; z = [3.0, 3.25, 3.5]; chol = [3.5, 3.75, 4.0, 4.25, 4.5, 4.75]; confidence = 4.0; calib_ver = 1024; producer_ver = 1027; flags = 1030; source = 56; inst = 61; kind = 66; frame = 71; class_id = 76; producer_id = 81; batch_index = 86; batch_count = 91

```
fd 4e 00 00 12 fa 19 d4 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 cc 86
01 00 00 00 40 40 00 00 50 40 00 00 60 40 00 00 60 40 00 00 70 40 00 00 80 40 00 00 88 40 00 00
90 40 00 00 98 40 00 00 80 40 00 04 03 04 06 04 38 3d 42 47 4c 51 56 5b a6 f8
```

### 46040 IDA_CMD_SIMPLE

- Başlık: seq = 19, sysid = 250, compid = 25; CRC_EXTRA = 73; yük azami 10 B, çerçeve azami 22 B; bu dizide yük 10 B, çerçeve 22 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cmd_id = 1001; sess = 6699; arg = 0; type = 3; source = 255

```
fd 0a 00 00 13 fa 19 d8 b3 00 e9 03 00 00 2b 1a 00 00 03 ff 1b b4
```

### 46041 IDA_CMD_MANUAL

- Başlık: seq = 20, sysid = 250, compid = 25; CRC_EXTRA = 215; yük azami 31 B, çerçeve azami 43 B; bu dizide yük 31 B, çerçeve 43 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cmd_id = 100000; axes = [1.0, 1.25, 1.5, 1.75, 2.0, 2.25]; sess = 1006; source = 16

```
fd 1f 00 00 14 fa 19 d9 b3 00 a0 86 01 00 00 00 80 3f 00 00 a0 3f 00 00 c0 3f 00 00 e0 3f 00 00
00 40 00 00 10 40 ee 03 10 4e d2
```

### 46042 IDA_CMD_GUIDED

- Başlık: seq = 21, sysid = 250, compid = 25; CRC_EXTRA = 50; yük azami 44 B, çerçeve azami 56 B; bu dizide yük 44 B, çerçeve 56 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cmd_id = 100000; pos_ne = [1.0, 1.25]; depth = 1.5; altitude = 2.0; vel_ned = [2.5, 2.75, 3.0]; yaw = 3.0; yaw_rate = 3.5; sess = 1021; mask = 41; source = 46

```
fd 2c 00 00 15 fa 19 da b3 00 a0 86 01 00 00 00 80 3f 00 00 a0 3f 00 00 c0 3f 00 00 00 40 00 00
20 40 00 00 30 40 00 00 40 40 00 00 40 40 00 00 60 40 fd 03 29 2e 8b 39
```

### 46043 IDA_CMD_MISSION_ITEM

- Başlık: seq = 22, sysid = 250, compid = 25; CRC_EXTRA = 188; yük azami 47 B, çerçeve azami 59 B; bu dizide yük 47 B, çerçeve 59 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cmd_id = 100000; p = [1.0, 1.25, 1.5, 1.75]; lat_e7 = -1014; lon_e7 = -1021; north_m = 2.5; east_m = 3.0; depth = 3.5; sess = 1021; index = 1024; item_type = 46; frame = 51; source = 56

```
fd 2f 00 00 16 fa 19 db b3 00 a0 86 01 00 00 00 80 3f 00 00 a0 3f 00 00 c0 3f 00 00 e0 3f 0a fc
ff ff 03 fc ff ff 00 00 20 40 00 00 40 40 00 00 60 40 fd 03 00 04 2e 33 38 a7 8b
```

### 46044 IDA_CMD_PARAM_SET

- Başlık: seq = 23, sysid = 250, compid = 25; CRC_EXTRA = 202; yük azami 14 B, çerçeve azami 26 B; bu dizide yük 14 B, çerçeve 26 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cmd_id = 100000; value_bits = 100011; sess = 1006; param_id = 1009; param_type = 21; source = 26

```
fd 0e 00 00 17 fa 19 dc b3 00 a0 86 01 00 ab 86 01 00 ee 03 f1 03 15 1a c2 32
```

### 46045 IDA_CMD_FC

- Başlık: seq = 24, sysid = 250, compid = 25; CRC_EXTRA = 160; yük azami 17 B, çerçeve azami 29 B; bu dizide yük 17 B, çerçeve 29 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cmd_id = 100000; arg0 = 100011; arg1 = 100022; sess = 1009; op = 21; tube = 26; source = 31

```
fd 11 00 00 18 fa 19 dd b3 00 a0 86 01 00 ab 86 01 00 b6 86 01 00 f1 03 15 1a 1f 51 b9
```

### 46046 IDA_EVENT_RESULT

- Başlık: seq = 25, sysid = 250, compid = 25; CRC_EXTRA = 144; yük azami 19 B, çerçeve azami 31 B; bu dizide yük 19 B, çerçeve 31 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: event_id = 30064771072; t_apply_ns = 1005000000; sess = 1006; result = 16

```
fd 13 00 00 19 fa 19 de b3 00 00 00 00 00 07 00 00 00 40 15 e7 3b 00 00 00 00 ee 03 10 de a1
```

### 46060 IDA_ACTUATOR

- Başlık: seq = 26, sysid = 1, compid = 1; CRC_EXTRA = 139; yük azami 34 B, çerçeve azami 46 B; bu dizide yük 30 B, çerçeve 42 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: batch_seq = 2469; valid_for_ns = 100000000; pwm_us = [1500, 1520, 1480, 1500, 1550, 1450, 1500, 1500]; sess = 6699; act_flags = 5; rx_to_tx_us = 3210; fs_level = 0; core_rc = 0; n_events = 0; n_merged = 0

```
fd 1e 00 00 1a 01 01 ec b3 00 a5 09 00 00 00 e1 f5 05 dc 05 f0 05 c8 05 dc 05 0e 06 aa 05 dc 05
dc 05 2b 1a 05 00 8a 0c 4c 17
```

### 46061 IDA_EVENT

- Başlık: seq = 27, sysid = 1, compid = 1; CRC_EXTRA = 74; yük azami 37 B, çerçeve azami 49 B; bu dizide yük 37 B, çerçeve 49 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: event_id = 21474836483; t_decide_ns = 12345000000; batch_seq = 2469; apply_by_ns = 20000000; hold_ns = 200000000; fc_ref = 9; sess = 6699; kind = 1; tube = 0; call_kind = 46

```
fd 25 00 00 1b 01 01 ed b3 00 03 00 00 00 05 00 00 00 40 c0 d1 df 02 00 00 00 a5 09 00 00 00 2d
31 01 00 c2 eb 0b 09 00 00 00 2b 1a 01 00 2e 38 c8
```

### 46062 IDA_CMD_RESULT

- Başlık: seq = 28, sysid = 1, compid = 1; CRC_EXTRA = 111; yük azami 27 B, çerçeve azami 39 B; bu dizide yük 27 B, çerçeve 39 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cmd_id = 100000; value_bits = 100011; save_gen = 100022; save_crc = 100033; save_len = 1012; reason = 1015; param_id = 1018; sess = 1021; kind = 41; ack = 46; param_type = 51

```
fd 1b 00 00 1c 01 01 ee b3 00 a0 86 01 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 f4 03 f7 03 fa 03
fd 03 29 2e 33 1d 98
```

### 46063 IDA_NAV

- Başlık: seq = 29, sysid = 1, compid = 1; CRC_EXTRA = 110; yük azami 118 B, çerçeve azami 130 B; bu dizide yük 118 B, çerçeve 130 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_ns = 1000000000; batch_seq = 100011; p_ned = [1.5, 1.75, 2.0]; v_ned = [2.0, 2.25, 2.5]; q_nb = [2.5, 2.75, 3.0, 3.25]; omega_b = [3.0, 3.25, 3.5]; depth = 3.5; altitude = 4.0; p_sigma = [4.5, 4.75, 5.0]; v_sigma = [5.0, 5.25, 5.5]; att_sigma = [5.5, 5.75, 6.0]; origin_lat_e7 = -1077; origin_lon_e7 = -1084; est_level = 66; reset_counter = 71

```
fd 76 00 00 1d 01 01 ef b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 00 00 c0 3f 00 00 e0 3f 00 00
00 40 00 00 00 40 00 00 10 40 00 00 20 40 00 00 20 40 00 00 30 40 00 00 40 40 00 00 50 40 00 00
40 40 00 00 50 40 00 00 60 40 00 00 60 40 00 00 80 40 00 00 90 40 00 00 98 40 00 00 a0 40 00 00
a0 40 00 00 a8 40 00 00 b0 40 00 00 b0 40 00 00 b8 40 00 00 c0 40 cb fb ff ff c4 fb ff ff 42 47
e0 d7
```

### 46064 IDA_STATUS

- Başlık: seq = 30, sysid = 1, compid = 1; CRC_EXTRA = 130; yük azami 36 B, çerçeve azami 48 B; bu dizide yük 36 B, çerçeve 48 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: boot_count = 100000; fs_reasons = 100011; param_gen = 100022; param_blob_crc = 100033; batch_seq = 100044; mission_item = 1015; sess = 1018; mode = 36; armed = 41; fs_level = 46; regime = 51; phase = 56; mission_state = 61; param_ok = 66; events_ok = 71; health = 76; link_state = 81; param_store = 86; param_fail = 91

```
fd 24 00 00 1e 01 01 f0 b3 00 a0 86 01 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 cc 86 01 00 f7 03
fa 03 24 29 2e 33 38 3d 42 47 4c 51 56 5b d8 ef
```

### 46065 IDA_FC_STATUS

- Başlık: seq = 31, sysid = 1, compid = 1; CRC_EXTRA = 57; yük azami 79 B, çerçeve azami 91 B; bu dizide yük 79 B, çerçeve 91 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: last_event_id = 30064771072; inhibit_mask = 100011; state = 11; last_event_result = 16; ext_len = 21; ext = [26, 27, 28, 29, 30, 31, … (64 öğe)]

```
fd 4f 00 00 1f 01 01 f1 b3 00 00 00 00 00 07 00 00 00 ab 86 01 00 0b 10 15 1a 1b 1c 1d 1e 1f 20
21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f 40
41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f 50 51 52 53 54 55 56 57 58 59 c2 8e
```

### 46066 IDA_LOG

- Başlık: seq = 32, sysid = 1, compid = 1; CRC_EXTRA = 56; yük azami 19 B, çerçeve azami 31 B; bu dizide yük 19 B, çerçeve 31 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_w_ns = 1000000000; arg0 = -1007; arg1 = -1014; code = 1009; severity = 21

```
fd 13 00 00 20 01 01 f2 b3 00 00 ca 9a 3b 00 00 00 00 11 fc ff ff 0a fc ff ff f1 03 15 9d 6b
```

### 46067 IDA_CORE_COUNTERS

- Başlık: seq = 33, sysid = 1, compid = 1; CRC_EXTRA = 2; yük azami 72 B, çerçeve azami 84 B; bu dizide yük 71 B, çerçeve 83 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cnt = [100000, 100001, 100002, 100003, 100004, 100005, … (18 öğe)]

```
fd 47 00 00 21 01 01 f3 b3 00 a0 86 01 00 a1 86 01 00 a2 86 01 00 a3 86 01 00 a4 86 01 00 a5 86
01 00 a6 86 01 00 a7 86 01 00 a8 86 01 00 a9 86 01 00 aa 86 01 00 ab 86 01 00 ac 86 01 00 ad 86
01 00 ae 86 01 00 af 86 01 00 b0 86 01 00 b1 86 01 23 96
```

### 46068 IDA_LINK_STATS

- Başlık: seq = 34, sysid = 1, compid = 1; CRC_EXTRA = 209; yük azami 144 B, çerçeve azami 156 B; bu dizide yük 143 B, çerçeve 155 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cnt = [100000, 100001, 100002, 100003, 100004, 100005, … (36 öğe)]

```
fd 8f 00 00 22 01 01 f4 b3 00 a0 86 01 00 a1 86 01 00 a2 86 01 00 a3 86 01 00 a4 86 01 00 a5 86
01 00 a6 86 01 00 a7 86 01 00 a8 86 01 00 a9 86 01 00 aa 86 01 00 ab 86 01 00 ac 86 01 00 ad 86
01 00 ae 86 01 00 af 86 01 00 b0 86 01 00 b1 86 01 00 b2 86 01 00 b3 86 01 00 b4 86 01 00 b5 86
01 00 b6 86 01 00 b7 86 01 00 b8 86 01 00 b9 86 01 00 ba 86 01 00 bb 86 01 00 bc 86 01 00 bd 86
01 00 be 86 01 00 bf 86 01 00 c0 86 01 00 c1 86 01 00 c2 86 01 00 c3 86 01 9c 6e
```

### 46069 IDA_CALL_TRACE

- Başlık: seq = 35, sysid = 1, compid = 1; CRC_EXTRA = 77; yük azami 133 B, çerçeve azami 145 B; bu dizide yük 133 B, çerçeve 145 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: now_w_ns = 1000000000; batch_seq = 100011; bg_slices = 100022; prev_call_ns = 100033; rx_age_ns = [100044, 100045, 100046, 100047, 100048, 100049, … (27 öğe)]; n_rx = 26; call_kind = 31; core_rc = 36; plat_flags = 41; n_merged = 46

```
fd 85 00 00 23 01 01 f5 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 cc 86
01 00 cd 86 01 00 ce 86 01 00 cf 86 01 00 d0 86 01 00 d1 86 01 00 d2 86 01 00 d3 86 01 00 d4 86
01 00 d5 86 01 00 d6 86 01 00 d7 86 01 00 d8 86 01 00 d9 86 01 00 da 86 01 00 db 86 01 00 dc 86
01 00 dd 86 01 00 de 86 01 00 df 86 01 00 e0 86 01 00 e1 86 01 00 e2 86 01 00 e3 86 01 00 e4 86
01 00 e5 86 01 00 e6 86 01 00 1a 1f 24 29 2e d9 64
```

### 46070 IDA_FAULT_RECORD

- Başlık: seq = 36, sysid = 1, compid = 1; CRC_EXTRA = 89; yük azami 35 B, çerçeve azami 47 B; bu dizide yük 35 B, çerçeve 47 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cfsr = 100000; hfsr = 100011; mmfar = 100022; bfar = 100033; pc = 100044; lr = 100055; xpsr = 100066; step_counter = 100077; fault_seq = 1024; stacking_valid = 46

```
fd 23 00 00 24 01 01 f6 b3 00 a0 86 01 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 cc 86 01 00 d7 86
01 00 e2 86 01 00 ed 86 01 00 00 04 2e 5d 4c
```

## Ek C. Başka sözleşmelerin "A1'den beklenen" listelerinin karşılanması

**A4 "A1'den beklenen"**

| # | Beklenen | Karşılığı |
|---|---|---|
| 1 | Ek A'daki alanların eşlenmesi; TF'nin `t_rx`'i | Ek A; A1-K48 |
| 2 | Tüp başına `PERMIT_OPEN`, `PLUG_IN`, `PIN_OUT`, `LOADED`, `estop` | `IDA_SAFETY`; A1-K56, K89 |
| 3 | Olay alanları ve alıcı kodları | `IDA_EVENT`, `IDA_EVENT_RESULT`; A1-K81–K88 |
| 4 | Alıcıda iznin sim zamanında `hold_ns` | A1-K86, K89 |
| 5 | `valid_for_ns`, `apply_by_ns`, `hold_ns` değerleri | 3.9; E2–E9 |
| 6 | Ping360 kutu zamanı | A1-K52 |
| 7 | Hangi dönüş kodunda hangi yanıt | A1-K31, K44, K45 (`OLD`/`DUP`'ta çağrı ve yanıt yok; hata dönüşleri `THR_VALID = 0` ile gider; `IMU_REJECTED` olağan yanıt; ızgara tick'i yalnız `thr_valid = 1` iken) |
| 8 | Art arda kapanan partilere üst sınır | A1-K39 (gönderen tarafta yarı açık 5 ms'de ≤ 2 TF; E11); varış tarafında sıkışma A1-K40 |
| 9 | Birleştirilen partilerin yanıtı | A1-K40, K75 (`n_merged`; `parti_kacirildi_birlesik`) |
| 10 | Komut çevirisi, `cmd_id`, birleştirilen daraltıcıların yanıtı, parametre protokolü, GNSS fix | 3.6; A1-K63–K68; `COALESCED` sonucu; parametre işlemleri A4 komutlarıyla; fix türü A4 değerleriyle |
| 11 | `plat` sayaçlarının kaynağı | Tablo 3.11b; A1-K30, K31 |
| 12 | Olay yineleme aralığı ve `EXPIRED_NO_ACK` anı | A1-K80; 3.9 (`T_olay_yin`, `T_sonuç_pay`; E7, E8) |

**A8 "A1'den beklenen"**

| # | Beklenen | Karşılığı |
|---|---|---|
| 1 | Alanların bit düzeyinde aynı taşınması | 3.5; Ek B (46035, 46036) |
| 2 | Kare `k`'nın `P_k`'da, bölünmeden taşınması | A1-K38 |
| 3 | Bütçede yer | 3.10 (59,9 B/periyot; tepe 399 B; E13) |
| 4 | Kart → sim sonar partileri | A1-K94 (yalnız kayıtlar; 8.2 A8 md. 1) |
| 5 | Kaybolan başlığın kartta görünümü | Partinin eksik kapanması (A1-K30) ve A8'in `batch_seq` boşluğu; tazelik kuralı A4/çekirdeğin (A8'in A4'ten beklenen 3) |

**A3'ün beklediği (görev mesajındaki liste)**

| Beklenen | Karşılığı |
|---|---|
| Kart ve atış durum büyüklüklerinin telemetride taşınması | `IDA_STATUS`, `IDA_FC_STATUS`, `IDA_NAV`, `IDA_CORE_COUNTERS`, `IDA_LINK_STATS` (A1-K91) |
| Makinece okunur komut listesi ve sınıfları | Tablo 3.6; `protocol/ida.xml` `[sinif: …]` (A1-K60) |
| Bariyer kararlarının ve sayaçların tanımı | A1-K72–K79; Tablo 3.11a |
| Olay kimliğinin bit düzeni | 3.7 (`IDA_EVENT`) |
| Bekleme sınırı ve komut kullanım süresi | 3.9 (`T_bekle` 50 ms, `T_kullanım` 100 ms) |
| `bg_slices`'ın taşınması | `IDA_CALL_TRACE` (A1-K95) |

**A5'in beklediği (görev mesajındaki liste)**

| Beklenen | Karşılığı |
|---|---|
| Sürekli komutun kullanım süresi | 100 ms (3.9) |
| Kartlı kipte otopilot parametre bloğunun yüklenmesi ya da karşılaştırılması | A1-K18, K19 (CRC karşılaştırması; hazırlık oturumunda eşitleme) |
| Hata enjeksiyonu ve yer istasyonu komutlarında kullanılacak mesaj adları | 5.1 (`IDA_*` mesaj adları); Tablo 3.6 |
