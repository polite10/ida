# A1 — Sim sunucusu ↔ kart (ve SIL koşucusu) bağlantı sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday, çapraz uyum turu (2026-11-20) |
| Görev belgesi | `docs/gorevler/W1-A1.md`; ortak kurallar `docs/gorevler/W1-ortak.md`; çapraz uyum kararları `docs/gorevler/W1-uyum.md` (U1–U18) |
| Makinece okunur tanım | `protocol/ida.xml` (MAVLink 2 dialect'i; bu belgenin parçasıdır, ikisi çelişemez) |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.10 (§3, §3.1, §3.2, §4, §5, §6, §8, §10); A4 sözleşmesi adayı (`docs/sozlesmeler/A4-port.md`, `core/include/ida/port.h`; çapraz uyum turu, 129 kural); A8 sözleşmesi adayı (`docs/sozlesmeler/A8-kamera-algilama.md`, `core/include/ida/algilama_kaydi.h`; çapraz uyum turu); A5 sözleşmesi adayı (Ek D.7); ölçümler `docs/olcumler/MP3-vcp.md`, `MP4-dongu-yuku.md`, `MP1-arac-zinciri.md`; bölümler 01, 05, 06 (taslak; iskelet 0.8'den eski; çeliştikleri yerde iskelet geçerlidir) |
| İnceleme | sol: `docs/sozlesmeler/inceleme/A1-sol.md`, `A1-sol-2.md`; yanıtlar: `A1-yanit.md`, `A1-yanit-2.md` |
| Durum | Yazan Opus; düzeltme turu 1 ve 2 ile çapraz uyum turu (W1-uyum) mimar kararlarıyla yapıldı; doğrulama incelemesi bekliyor; dondurulmadı. Kural numaraları korundu; birinci turda eklenen kurallar A1-K104–K115, ikinci turda A1-K116–K122, çapraz uyum turunda A1-K123–K128, doğrulama incelemesinden sonra A1-K129 |

**Değerlerin niteliği.** Her süre, hız, boyut ve kapasite ya bir ölçüme (rapor ve başlık atfıyla), ya bir birincil kaynağa (bağlantıyla), ya bir **hesaba** (aritmetik; ölçüm değil) ya da **mimari seçimi (tahmin)** etiketine dayanır. Etiketli değerler ölçülmüş değildir; Bölüm 8'de hangi ölçümle kapanacakları yazılıdır. Bu belgedeki uçtan uca gecikme bütçesi bir tahmindir; kartta ve PC'de birlikte ölçülmedi.

**Atış kontrol sınırı** (W1-ortak.md). Bu sözleşmede atışla ilgili içerik yalnız şunlardır: sanal emniyet girdilerinin ve fırlatıcı durumunun karta geri okunması (`IDA_SAFETY`), izin açma ve kapama olayları ile atış olayının taşınması (`IDA_EVENT`), sim'in olay sonucunu bildirmesi (`IDA_EVENT_RESULT`), kapalı listedeki argümansız emniyet zinciri komutları (`IDA_CMD_FC`; 3.6) ve emniyet zinciri durumunun (`IDA_FC_STATUS`) taşınması ve algılama kayıtlarının taşınması (`IDA_DET_*`). Hedef seçimi, hedefleme çözümü, hedefe hizalanma, atış ölçütü, menzil ya da önleme hesabına ilişkin alan, mesaj, görev öğesi ya da kural yoktur; görev öğesi türleri kapalı listedir ve yalnız seyir ve dalış görevlerinden oluşur (3.6). Anlamı tanımsız genel alan yoktur: eski `IDA_FC_SELECT` işlemi, olayın `fc_ref`'i ve durumun `ext[]`'i A1'de yoktur (U1 ile A4'ten de kaldırıldı); `IDA_CMD_FC.arg0/arg1` ayrılmıştır ve sıfır olmak zorundadır (A1-K64). `IDA_LOG.code`/`arg0`/`arg1`, `IDA_CMD_RESULT.reason` ve `IDA_FC_STATUS.inhibit_mask` yalnız A4'ün kapalı tablolarındaki değerleri taşır (U1; 3.7). Panelin komut kataloğunda (`protocol/komutlar.json`; A1-K125) atışla ilgili tek öğe operatörün onaylı `FC_FIRE` isteğidir: istek karta gider, emniyet zincirini kart yürütür ve `FIRE` olayını yalnız çekirdek üretir; sim fırlatıcısını kartı atlayarak tetikleyen bir uç yoktur ve katalogda hedef, menzil, yön ya da ölçüt taşıyan parametre yoktur.

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

Birimler, çerçeveler, kuaterniyon kuralı ve iki zaman damgası iskelet §3.1'dedir; A4'ün terimleri (ölçüm zamanı, duvar saati, parti, zaman çerçevesi, daraltıcı, genişletici) A4 Bölüm 2'dedir; "seri aritmetikle büyük/yeni" her yerde iskelet §3.1'in sıra numarası kuralıdır (A1-K123). Burada yalnız bu sözleşmeye özgü olanlar:

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
| **Gönderim sınıfları (kart)** | (i) kritik sürekli çıktı, (ii) zorunlu-güvenilir, (iii) kaybedilebilir (A1-K110) |
| **`out_seq`** | Kartın sürekli çıktı üretim sayacı; sim sürekli çıktıları yalnız bu sırayla uygular (A1-K78, K105) |
| **`safety_rev` (emniyet revizyonu)** | Dünyanın tek emniyet sayacı (iskelet §3.2: `uint32`, sim süreci başında 1, sıfırlanmaz, doyan; yalnız altı işlem artırır); izin açan olaylar karar anındaki değerle damgalanır (A1-K106) |
| **Seri karşılaştırma** | 32 bitlik sıra numaraları ve kimlikler için iskelet §3.1 kuralı: `a`, `b`'den yenidir ⇔ `(a − b) mod 2³²` ∈ [1, 2³¹ − 1]; fark 0 ise yinelemedir; 2³¹ ya da büyükse `a` eskidir (A1-K123) |
| **`run_id` (A1)** | `IDA_SIM_HELLO` ve `IDA_SESSION_ACCEPT`'teki `uint32` çalıştırma kimliği; A3/A6'nın metin çalıştırma kimliğinden ayrıdır ve ondan türetilir (A1-K126) |
| **`D_n`** | Yuva `n`'in planlanan duvar son tarihi, `W(a_n) − C_sim` (A1-K108) |
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
| Alım damgasının sapma sınırı `Δ_rx` (kart) | Damga, çerçevenin son baytının hattan alındığı andan en çok 0,86 ms önce ya da en çok 0,1 ms sonradır; `|sapma|` ≤ 1 ms (A1-K48) | Hesap: 8N1'de bayt 3,33 µs; dialect'in en büyük çerçevesi 258 B = 0,86 ms (erken yön, kaba damga); geç yön 1 bayt süresi + kesme gecikmesi ≤ 96 µs (kesme gecikmesi **tahmin**, W3'te ölçülür) |
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

**Dialect.** `protocol/ida.xml` 37 özel mesaj ve 31 numaralandırma tanımlar. **Adlandırma (U15):** mesaj adları `IDA_` önekini, bütün numaralandırma adları ve girdileri `IDAW_` önekini taşır (değerler A4'teki karşılıklarıyla aynıdır); `port.h` sabitleri `IDA_` önekinde kalır. Böylece üretilmiş başlık ile `port.h` aynı çeviri biriminde ad çakışması olmadan içerilebilir (sol'un P2-18 bulgusu: eski adlarla 41 girdi `port.h` makrolarıyla çakışıyordu); iki başlığın birlikte derlenmesi W2'nin derleme denetimidir. Bu belgede `IDAW_` ile başlayan ad teldeki değeri, `IDA_` ile başlayan numaralandırma ya da sabit adı `port.h`'deki karşılığını gösterir. **Standart MAVLink mesajı kullanılmaz ve `common.xml` içerilmez.** Gerekçe:

1. A1'in her mesajı, standart mesajlarda bulunmayan bir alan ister: parti üyeliği ve oturum (`sess`, `batch_seq`), sensör başına örnek sayacı ve aralık süresi (`seq`, `window_ns`), komut kimliği (`cmd_id`; A4-K43, A4-K48), olay kimliği ve son geçerlilik (A4-K60–K66). Standart bir mesajın anlamını bükmeden bu alanlar eklenemez (Ö2; 00 belgesi 06.1–06.3, 08.10). Örnekler: `HIL_SENSOR` aynı paketteki güncellemelere tek zaman damgası verir (06.3); `HIL_ACTUATOR_CONTROLS` normalleştirilmiş değer taşır, kullanım süresi ve olay kimliği yoktur; `COMMAND_LONG`/`COMMAND_ACK` çifti komut kimliği taşımaz; görev protokolü (`MISSION_COUNT` → `MISSION_REQUEST_INT`) aracın istediği öğeleri tek tek çeker, A4'ün atomik `MISSION_BEGIN/ITEM/END` yüklemesiyle aynı anlamda değildir (A4-K51); `PARAM_SET` değeri yazar ve kalıcı belleğe kaydeder ("write new value to permanent storage"), A4'te ise bekletilir, `PARAM_COMMIT` ile uygulanır ve ayrı bir `PARAM_SAVE` ile kaydedilir (A4-K81, K82). Bu mesajların alan ve açıklamaları pymavlink 2.4.50 ile gelen `common.xml`'den okundu (ör. `COMMAND_ACK` alanları: `command`, `result`, `progress`, `result_param2`, `target_system`, `target_component`, `time_boot_ms`; komut kimliği yok).
2. Standart numaralandırmalar (`MAV_SEVERITY`, `GPS_FIX_TYPE`) için `common.xml`'i içermek denendi: pymavlink 2.4.50 ile üretilen `common/common.h`, ISO C17'de `int` aralığını aşan numaralandırıcılar (`MAV_SYS_STATUS_EXTENSION_USED = 2147483648`, `HIL_SENSOR_UPDATED_RESET = 2147483648`) içerdiği için `-std=c17 -pedantic-errors` ile derlenmedi (Bölüm 7.3). Bu yüzden önem düzeyi ve GNSS fix türü dialect'te A4 değerleriyle tanımlandı (`IDAW_SEVERITY` değerleri `MAV_SEVERITY` ile aynı sıradadır; A4 3.8).
3. Harici yer istasyonu ilk sürümde yoktur (iskelet §3, §10 md. 17); ileride eklenirse sim'deki ağ geçidi standart mesajları A1 mesajlarına çevirir, A1 değişmez.

**Mesaj kimlikleri.** 46000–46099 aralığı kullanılır. Bu aralık, pymavlink 2.4.50 ile gelen 17 dialect dosyasının (`common`, `ardupilotmega`, `ASLUAV`, `cubepilot`, `storm32`, `icarous`, `uAvionix` ve diğerleri) hiçbirinde kullanılmıyor (bu işte dosyalardan tarandı). MAVLink belgesi özel dialect'ler için aralığı serbest bırakır ve 0–255'in kullanılmamasını ister (MAVLink XML tanımı). Kimlik tahsisi: 46000–46009 oturum, 46010 zaman, 46020–46030 sensör ve sonar, 46035–46036 algılama, 46040–46046 sim → kart komut ve olay sonucu, 46060–46070 kart → sim.

**`dialect_hash`.** `protocol/ida.xml`'den kurulan kanonik metnin SHA-256 özetinin ilk 8 baytı, küçük uçlu `uint64`. Kanonik metin ASCII'dir, her satır `\n` ile biter ve şu sırayla kurulur:

1. Her mesaj için, kimliğe göre artan sırada: `M <id> <ad> <CRC_EXTRA> <yük_azami>` satırı; ardından tel sırasındaki her alan için `F <tip> <ad> <dizi_boyu>` (dizi değilse 0); tip dizi işaretsiz yazılır (`float`, `uint8_t`).
2. Her numaralandırma için, ada göre artan sırada (bayt karşılaştırması), değere göre artan sırada her girdi: `E <numaralandırma> <girdi> <ondalık değer>`.

Bu adayın değeri `0x3939788984A5FF80`'dir; kanonik metin **37 mesaj satırı + 334 alan satırı + 194 numaralandırma girdisi satırı = 565 satır**dır (çapraz uyum turunun bütün XML değişikliklerinden — `IDAW_` adları, `IDAW_CLOSE_REASON_EVENT_OVERFLOW`, `IDAW_CALL_KIND_SESSION_END`, `IDAW_FC_STATE`'in altı girdisi — sonra, en son adım olarak standart kitaplıkla hesaplandı; betik, XML'deki bütün alanların ve numaralandırma girdilerinin metne girdiğini ayrıca denetledi). Düzeltme turu 2'nin değeri `0x104154A1E68773C8` (557 satır) eski adlarla ve eski girdi kümesiyle geçerliydi. İlk turdaki `0x6481E5EBBF66F5F1` (533 satır) yanlıştı: son XML değişikliğinden (o zamanki adıyla `IDA_CLOSE_REASON_TX_OVERFLOW`) önce hesaplanmıştı; o XML'in doğru değeri sol'un bulduğu `0x4B9E97C1439CF7FE`'dir (534 satır) ve aynı betik bunu yeniden üretti. Değer, XML'deki her tel ya da numaralandırma değişikliğinde değişir; derleme betiği değeri üretir ve sim el sıkışmada iki tarafın derlenmiş değerini karşılaştırır (A1-K14).

### 3.3 Mesaj listesi

Boylar MAVLink kırpması olmadan en büyük değerlerdir (yük / çerçeve, bayt; üreteçten). "Sınıf" sütunu sim → kart üyelerinde pencere sınıfıdır (A1-K35, K36, K113), kart → sim çerçevelerinde gönderim sınıfıdır: (i) kritik, (ii) zorunlu-güvenilir, (iii) kaybedilebilir (A1-K110); kart → sim oturum çerçeveleri (ii) sınıfındadır.

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
| 46028 | `IDA_SAFETY` | sim → kart | 10 Hz ve değişimde; partide ≤ 1 | 22 / 34 | kritik |
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
| 46060 | `IDA_ACTUATOR` | kart → sim | Çekirdek çağrısı başına ≤ 1 | 38 / 50 | (i) kritik |
| 46061 | `IDA_EVENT` | kart → sim | Olay başına 1 + yinelemeler | 37 / 49 | (ii) zorunlu |
| 46062 | `IDA_CMD_RESULT` | kart → sim | Komut başına 1 | 27 / 39 | (ii) zorunlu |
| 46063 | `IDA_NAV` | kart → sim | 50 Hz | 118 / 130 | (iii) |
| 46064 | `IDA_STATUS` | kart → sim | 10 Hz | 36 / 48 | (iii) |
| 46065 | `IDA_FC_STATUS` | kart → sim | 10 Hz | 14 / 26 | (iii) |
| 46066 | `IDA_LOG` | kart → sim | Olay güdümlü (çağrı başına ≤ 4) | 19 / 31 | (iii) |
| 46067 | `IDA_CORE_COUNTERS` | kart → sim | 1 Hz | 88 / 100 | (iii) |
| 46068 | `IDA_LINK_STATS` | kart → sim | 1 Hz | 144 / 156 | (iii) |
| 46069 | `IDA_CALL_TRACE` | kart → sim | Çekirdek çağrısı başına 1 | 137 / 149 | (iii) |
| 46070 | `IDA_FAULT_RECORD` | kart → sim | Oturum kurulunca, varsa 1 | 35 / 47 | (iii) |

Alanların tamamı, tipleri, birimleri ve numaralandırmaları `protocol/ida.xml`'dedir. Aşağıdaki tablolar her alanın geçerli aralığını, A4 karşılığını ve geçersiz değerde davranışı verir. "Z" sütunu: **Z** her durumda anlamlı; **K** koşullu; **B** bilgi (alıcının davranışını değiştirmez). "Geçersizde" sütunundaki "örnek VALID = 0" kuralı A1-K47'dedir.

### 3.4 Sensör mesajları (sim → kart) ve A4 eşlemesi

**Ortak sensör alanları** (IMU, MAG, BARO, GNSS, DVL, PING1D, BATT, LEAK, SAFETY ve PING360_HEAD'de):

| Alan | Tip | Birim | Aralık | Z | A4 karşılığı | Geçersizde |
|---|---|---|---|---|---|---|
| `t_meas_ns` | `int64_t` | ns | 0 … 2⁶² | Z | `hdr.t_meas.ns` (aynen; A4-K13) | Çekirdek reddeder (A4-K33) |
| `seq` | `uint32_t` | — | Sensör akışında seri aritmetikle artan | Z | `hdr.seq` | Çekirdek reddeder |
| `window_ns` | `uint32_t` | ns | IMU, BARO, BATT'ta > 0 | Z | `hdr.window_ns`; diğer türlerde mesajda alan yoktur ve dönüştürücü 0 yazar | Çekirdek reddeder |
| `flags` | `uint16_t` | bit | `IDAW_SAMPLE_FLAGS` (A4 `IDA_HF_*`) | Z | `hdr.flags` (aynen) | Bilinmeyen bitler aynen geçer; `VALID` = 0 ise çekirdek reddeder |
| `inst` | `uint8_t` | — | 0–3 | Z | `hdr.inst` | Çekirdek reddeder |
| (msgid) | — | — | — | Z | `hdr.type` (Tablo 3.4b) | — |
| — | — | — | — | Z | `hdr.t_rx` = kartın alım damgası (A1-K48) | — |

**Tablo 3.4b — Tür eşlemesi ve türe özgü alanlar.** Değerler A4 yapısına aynı ad ve birimle, dönüşümsüz kopyalanır (istisnalar belirtilmiştir).

| Mesaj | `hdr.type` | Türe özgü alanlar → A4 | Dönüşüm |
|---|---|---|---|
| `IDA_IMU` | `IDA_SENS_IMU` (1) | `gyro[3]`, `accel[3]`, `temp_c` → `ida_imu_t` | yok; `in.imu`'ya gider (A4-K17) |
| `IDA_MAG` | `IDA_SENS_MAG` (2) | `field[3]` (T) → `ida_mag_t.field` | yok |
| `IDA_BARO` | `IDA_SENS_BARO` (3) | `p_abs` (Pa), `temp_c` → `ida_baro_t` | yok |
| `IDA_GNSS` | `IDA_SENS_GNSS` (4) | `lat_e7`, `lon_e7`, `alt_mm`, `vel_ned[3]`, `eph`, `epv`, `fix_type`, `n_sv` → `ida_gnss_t` | yok (`IDAW_GNSS_FIX` değerleri A4 `IDA_GNSS_FIX_*` ile aynıdır); `fix_type` > 4 ise örnek VALID = 0 |
| `IDA_DVL` | `IDA_SENS_DVL` (5) | `vel[3]`, `cov[6]`, `fom`, `beam_dist[4]`, `altitude`, `dvl_flags`, `beam_valid` → `ida_dvl_t` | yok |
| `IDA_PING1D` | `IDA_SENS_PING1D` (6) | `distance`, `scan_start`, `scan_length`, `ping_number`, `tx_offset_ns`, `confidence` → `ida_ping1d_t` | yok |
| `IDA_BATT` | `IDA_SENS_BATT` (8) | `voltage`, `current`, `temp_c` → `ida_batt_t` | yok |
| `IDA_LEAK` | `IDA_SENS_LEAK` (9) | `mask` → `ida_leak_t` | yok |
| `IDA_SAFETY` | `IDA_SENS_SAFETY` (10) | `estop`, `tube[2]` (`LOADED`, `PLUG_IN`, `PIN_OUT`, `PERMIT_OPEN`) → `ida_safety_t`; `safety_rev` A4'te yoktur, platform son verdiği değeri tutar (A1-K106) | yok; `estop` > 1 ise örnek VALID = 0 |
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
| `IDA_CMD_SIMPLE` | `type` alanı: 1, 2, 3, 4, 7, 9, 10, 12, 13, 14, 15 | `arg` → `SET_MODE`: `u.set_mode.mode`; `MISSION_BEGIN`: `u.mission_begin.count`; `MISSION_CTL`: `u.mission_ctl.action`; `PARAM_GET`: `u.param.id`; diğerlerinde kullanılmaz (0) | `SET_MODE`: 2–7 (A4'ün kabul ettiği hedefler: 2–5 daraltıcı, 6–7 genişletici; 0 `INIT` ve 1 `DISARMED` hedef değildir); `MISSION_BEGIN`: 1–128; `MISSION_CTL`: 1–4 | `type` ∉ listedekiler ya da `SET_MODE`'da `arg` ∉ 2–7: `LINK_BAD_FIELD` (P2-2) |
| `IDA_CMD_MANUAL` | `MANUAL` (5) | `axes[6]` → `u.manual.axes` | [−1, 1] | — (aralığı çekirdek denetler, A4-K49) |
| `IDA_CMD_GUIDED` | `GUIDED_SP` (6) | `mask`, `pos_ne[2]`, `depth`, `altitude`, `vel_ned[3]`, `yaw`, `yaw_rate` → `u.guided` | A4-K50 | — |
| `IDA_CMD_MISSION_ITEM` | `MISSION_ITEM` (8) | `index`, `item_type`, `frame`, `p[4]`, `lat_e7`, `lon_e7`, `north_m`, `east_m`, `depth` → `u.mission_item` | Tablo 3.6a; A4-K51 | Tablo 3.6a'nın herhangi bir koşulu (A1-K122): `LINK_BAD_FIELD` |
| `IDA_CMD_PARAM_SET` | `PARAM_SET` (11) | `param_id` → `u.param.id`; `param_type` → `u.param.type`; `value_bits` → `u.param.v` (32 bit aynen) | `param_type` 1–3 | `param_type` ∉ 1–3: `LINK_BAD_FIELD` |
| `IDA_CMD_FC` | `FC` (16) | `op` → `u.fc.op`; `tube` → `u.fc.tube`; `u.fc.arg0 = u.fc.arg1 = 0` | `op` ∈ {1, 2, 3, 5, 6, 7, 8}; `op` = 2, 3 (daraltıcılar) iken `tube` = 255 (bütün tüpler), diğer işlemlerde `tube` 0–1; `arg0 = arg1 = 0` | `op` kapalı listede değil, `tube` işlemin izin verdiği değer değil ya da `arg0`/`arg1` ≠ 0: `LINK_BAD_FIELD` |

**Tablo 3.6a — Görev öğesi türleri (kapalı liste).** Bu sürümde görev öğeleri yalnız seyir ve dalış görevleridir; eski `IDA_MI_TARGET_ALIGN` (8) A1'de yoktur, taşınamaz ve A4'ten de kaldırıldı (U1). Bu tablo görev öğesi türlerinin tek tanımıdır; A4 ve A5 ona atıf verir (U1). Her türün `p[]` parametrelerinin anlamı, birimi ve aralığı aşağıdadır; "0" yazan parametre tam olarak 0 olmak zorundadır; hiçbir parametrenin anlamı başka bir işe bırakılmaz. Konum alanları: `frame = NONE` iken `lat_e7`, `lon_e7`, `north_m`, `east_m` = 0; `LOCAL` iken `lat_e7 = lon_e7 = 0` ve `north_m`, `east_m` sonlu, mutlak değeri ≤ 10 000 m; `GLOBAL` iken `north_m = east_m = 0`, `lat_e7` ±900 000 000 ve `lon_e7` ±1 800 000 000 içinde. `depth` m, aşağı pozitif. Aralıklar mimari seçimidir (tahmin).

| `item_type` | Ad | `frame` | `depth` (m) | `p[0]` | `p[1]` | `p[2]` | `p[3]` |
|---|---|---|---|---|---|---|---|
| 1 | `WAYPOINT` (yol noktasına seyir) | LOCAL, GLOBAL | 0–100 | Kabul yarıçapı, m, 0,1–50 | Seyir hızı, m/s, 0,05–3 | 0 | 0 |
| 2 | `DIVE_TO` (bulunduğu yerde dalış) | NONE | > 0 – 100 | Dikey hız, m/s, 0,05–1 | Derinlik toleransı, m, 0,05–5 | 0 | 0 |
| 3 | `SURFACE` (yüzeye çıkış) | NONE | 0 | Çıkış hızı, m/s, 0,05–1 | 0 | 0 | 0 |
| 4 | `HOLD` (konum ve derinlikte bekleme) | NONE (bulunduğu yer), LOCAL, GLOBAL | 0–100 | Süre, s, > 0 – 3 600 | Konum toleransı, m, 0,1–50 | 0 | 0 |
| 5 | `SCAN_LINES` (paralel hatlarla tarama seyri) | LOCAL, GLOBAL (başlangıç köşesi) | 0–100 | Hat boyu, m, 1–500 | Hatlar arası, m, 0,5–100 | Hat sayısı, tamsayı değerli, 1–100 | İlk hattın rotası, rad, (−π, π], kuzeyden doğuya |
| 6 | `SCAN_POINT` (noktada konum tutarak tarama) | LOCAL, GLOBAL | 0–100 | Süre, s, > 0 – 3 600 | Konum toleransı, m, 0,1–50 | 0 | 0 |
| 7 | `WAIT_GNSS` (yüzeyde GNSS çözümü bekleme) | NONE | 0 | En çok bekleme, s, > 0 – 3 600 | Gereken en az fix türü, tamsayı değerli `IDAW_GNSS_FIX`, 1–4 | 0 | 0 |

Bütün `p[]` değerleri sonludur; "tamsayı değerli" alan, kesirli kısmı tam 0 olan `float`'tır. "Tarama" türleri yalnız aracın izleyeceği yolu ve bekleyeceği yeri tanımlar; sonar her türde aynı biçimde çalışır, hiçbir tür bir hedefe ya da algılamaya göre davranış taşımaz.

**Atış kontrol işlemleri kapalı listedir** ve yalnız emniyet zinciri işlemlerinden oluşur. Bu sürümde her işlem argümansızdır; `arg0` ve `arg1` ayrılmıştır, 0 olmak zorundadır ve kart sıfır olmayanı reddeder (A1-K64). Eski `IDA_FC_SELECT` (4; hedef seçimi) A1'de yoktur, taşınamaz ve A4'ten de kaldırıldı (U1). FC işlemlerinin tek tanımı bu tablodur. Kartın dönüştürücüsünün `LINK_BAD_FIELD` reddi (A1-K64), çekirdeğin aynı alanlar için yaptığı `OUT_OF_RANGE` denetiminden (A4-K119) önce gelir; ikisi aynı koşulu denetler ve çelişmez.

| `op` | Ad | Sınıf | `tube` | `arg0` | `arg1` | Panel kataloğunda (A1-K125) |
|---|---|---|---|---|---|---|
| 1 | `FC_ARM` | genişletir | 0–1 | 0 | 0 | var |
| 2 | `FC_DISARM` | daraltır (mandal) | 255 (bütün tüpler) | 0 | 0 | var |
| 3 | `FC_ABORT` | daraltır (mandal) | 255 (bütün tüpler) | 0 | 0 | var |
| 5 | `FC_FIRE` | genişletir | 0–1 | 0 | 0 | var (onaylı; parametresi yalnız `tube`) |
| 6 | `FC_RESET` | genişletir | 0–1 | 0 | 0 | var |
| 7 | `FC_TEST_ENTER` | genişletir | 0–1 | 0 | 0 | var |
| 8 | `FC_TEST_EXIT` | nötr | 0–1 | 0 | 0 | var |

**`FC_FIRE` (mimar kararı, W1-uyum sonrası).** Bu sürümde atışı başlatan tek kaynak operatörün onaylı isteğidir (katalogdaki `fc_fire`; `sinif` = `genisletici`, `onay` = true; A3'ün iki adımlı onay akışı). İstek bağlantı emülatöründen ve A1'den karta gider; emniyet zincirini kart yürütür ve `FIRE` olayını yalnız çekirdek üretir; sim olayı A1-K85, K88 ve K118'e göre doğrular. Böylece emniyet zinciri, sözleşmelerde tanımlanmayan bir atış ölçütüne gerek kalmadan uçtan uca sınanır. U16'nın "doğrudan atış ucu yoktur" ifadesi sim fırlatıcısını kartı atlayarak tetikleyen ucu kasteder; böyle bir uç yoktur (A3-K99).

**Daraltıcılar mandala gider** (A4-K46): `CMD_SIMPLE` `type = DISARM` → `IDA_NARROW_DISARM`; `type = SET_MODE` ve `arg` ∈ 2–5 → `IDA_NARROW_MODE` (`narrow.mode = arg`); `CMD_FC` `op` = 2 → `IDA_NARROW_FC_DISARM`; `op` = 3 → `IDA_NARROW_FC_ABORT`. Diğerleri `cmds[]`'e girer.

**Daraltıcıların kapsamı (P2-3).** `FC_DISARM` ve `FC_ABORT` her zaman **bütün tüplere** uygulanır; bu yüzden telde `tube` = 255 zorunludur ve mandal tüp taşımaz (A4 `ida_narrow_t`'de tüp alanı yoktur); tüp bakımından farklı iki geçerli daraltıcı komut olamaz, dönüşümde bilgi kaybolmaz. Mandal komutun `source` alanını da taşımaz: `source` yalnız bilgi amaçlıdır, çekirdeğin hiçbir kararına girmez; sim her komutun kaynağını `cmd_id` ile kendi kaydında (A6) tutar ve sonuç `cmd_id` ile döndüğü için kaynak sonuçla yeniden eşlenir; bu bir bilgi kaybı değildir. Mandalda yalnız en son daraltıcının `cmd_id`'si (ve `NARROW_MODE`'da hedef kip) kalır, üzerine yazılan önceki komutların her biri A1-K61/K67'ye göre kendi sonucunu alır.

**Komut sınıfları** (A1-K60; A3'ün makinece okuyacağı liste `protocol/komutlar.json`'dur (A1-K125); sınıflar ayrıca `protocol/ida.xml`'deki `IDAW_CMD_TYPE`, `IDAW_FC_OP`, `IDAW_EVENT_KIND` ve `IDAW_SESSION_RESULT` girdilerinin açıklamasında `[sinif: …]` biçimindedir):

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
| FC: ARM, FIRE, RESET, TEST_ENTER | `CMD_FC` | **genişletir** | A4-K104 |
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
| `batch_seq` | `uint32_t` | — | tümü | Z | Platform: **çağrıya verilen zaman çerçevesinin sıra numarası** (`in.tf.seq`; birleştirilmiş girdide en yeni partininki), dönüş kodundan bağımsız olarak (hata dönüşleri ve `NOT_INIT` dahil); ızgara tick'inde (`ASYNC`) bu oturumda kapanan son partinin `batch_seq`'i, yoksa 0. `out.seq` kullanılmaz (P2-4: hata dönüşünde `out.seq` son kabul edilen partiyi gösterir ve yanıt yanlış yuvaya giderdi); A4'ün `out.t_ref`/`out.seq` anlamı değişmez |
| `valid_for_ns` | `uint32_t` | ns | `THR_VALID` = 1 ise 3.9'daki aralık (A1-K104; 60–450 ms); değilse 0 | Z | `out.valid_for_ns` (A4-K58) |
| `pwm_us[8]` | `uint16_t` | µs | `THR_VALID` = 1 ise A5'teki ESC alt–üst sınırı; değilse 0 | K | `out.thr_cmd[i]`'nin platformda ESC darbe genişliğine çevrilmesi (A4-K54); dizin A5 itici sırasıdır |
| `sess` | `uint16_t` | — | güncel oturum | Z | — |
| `act_flags` | `uint16_t` | bit | `IDAW_ACT_FLAGS` | Z | `THR_VALID` ← `out.thr_valid`; `IS_SAFE` ← `out.is_safe`; `ARMED` ← `out.armed`; `SRC_TICK` ← `out.src = IDA_SRC_TICK`; `ASYNC` ← ızgara tick'i (A1-K44); `INPUT_DROPPED` ← `out.health.input_dropped`; `NARROWED` ← `out.health.narrowed`; `OUT_FIXED` ← platformun A4-K59 değişikliği; `BATCH_INCOMPLETE` ← A1-K30 |
| `rx_to_tx_us` | `uint16_t` | µs | 0–65 535 (doyar) | B | Platform: `tf.t_rx`'ten bu çerçevenin gönderim kuyruğuna verilmesine kadar geçen duvar süresi |
| `fs_level` | `uint8_t` | — | 0–5 | Z | `out.fs_level` |
| `core_rc` | `uint8_t` | — | `IDAW_CORE_RC` | Z | Çağrının dönüş kodu |
| `n_events` | `uint8_t` | — | 0–4 | Z | `out.n_events` içinden ilk kabul kapısından geçen yeni olay sayısı (A1-K73) |
| `n_merged` | `uint8_t` | — | 0–255 | Z | Bu çağrıya birleştirilen önceki parti sayısı (A4-K102; `plat.batch_merged`) |
| `out_seq` | `uint32_t` | — | açılıştan beri kesin artan | Z | — (platformun üretim sayacı; A1-K105) |

**`IDA_EVENT`** (A4 `ida_event_t`; A4-K60–K66): `event_id` (u64), `t_decide_ns` (`t_decide.ns`), `batch_seq` (`seq`), `apply_by_ns`, `hold_ns`, `kind` (`IDAW_EVENT_KIND` = A4 `IDA_EVT_*`), `tube` (0–1; `SAFETY_CLOSE`'ta 255 = `IDA_TUBE_ALL`), `sess`; A4'te karşılığı olmayan, platformun doldurduğu iki alan: olayı çıkaran çağrının türü `call_kind` (`IDAW_CALL_KIND`) ve emniyet revizyonu damgası `safety_rev` (A1-K106). A4 alanları dönüşümsüz kopyalanır. Eski `fc_ref` alanı U1 ile A4'ten kaldırıldı; A1'de karşılığı yoktur. **Olay kimliğinin bit düzeni:** `event_id = (boot_count << 32) | ctr` (A4-K61); telde küçük uçlu 64 bit tamsayıdır: yükteki ilk 4 bayt `ctr`, sonraki 4 bayt `boot_count`'tur.

**`IDA_EVENT_RESULT`** (sim → kart; A4 `ida_event_result_t`): `event_id`, `result` (`IDAW_EVENT_RESULT_CODE`; sim yalnız 1–6 gönderir), `t_apply_ns` (`APPLIED` ve `DUPLICATE`'te uygulandığı sim anı; diğerlerinde 0), `sess`. Kart bunu `in.evres[]`'e koyar (A4-K68).

**`IDA_CMD_RESULT`** (A4 `ida_cmd_result_t`): `cmd_id`, `kind` (`IDAW_RES_KIND` = A4 `IDA_RES_*`), `ack` (`IDAW_ACK` = A4 `IDA_ACK_*`), `reason` (`IDAW_REASON`: çekirdek yalnız A4'ün ortak 0–15 tablosundaki kodları üretir; 16–127 ayrılmış; 128–191 A1'in bağlantı katmanı nedenleridir (bu sürümde 128–131; A1-K64, K67); 192 ve üstü ayrılmış, kullanılmaz (A4-K120; U1); 0–15'in anlamlarının tek kaynağı A4'tür, A1 yinelemez), `param_id`, `param_type`, `value_bits` (`kind = PARAM_VALUE`'da `value`'nun 32 biti), `save_gen`, `save_len`, `save_crc` (`kind = PARAM_SAVE` ve `ACCEPTED`'te: `save.gen`, `save.len` ve platformun kaydettiği bloğun CRC-32/ISO-HDLC değeri; blok A1'de taşınmaz), `sess`.

**`IDA_NAV`** (A4 `ida_nav_out_t`; aynı ad ve birimler): `t_ns`, `batch_seq`, `p_ned[3]`, `v_ned[3]`, `q_nb[4]` (w, x, y, z; Hamilton; gövde → NED), `omega_b[3]`, `depth`, `altitude`, `p_sigma[3]`, `v_sigma[3]`, `att_sigma[3]`, `origin_lat_e7`, `origin_lon_e7`, `est_level`, `reset_counter`.

**`IDA_STATUS`**: `boot_count`, `fs_reasons`, `mission_item`, `mode`, `armed`, `fs_level`, `regime`, `phase`, `mission_state`, `param_ok`, `events_ok` (A4 `ida_status_t`); `health` (A4 `ida_health_t`'nin sekiz alanı, ada göre: `core_ok` → bit 0 (1), `data_ok` → bit 1 (2), `imu_rejected` → bit 2 (4), `imu_lag` → bit 3 (8), `input_dropped` → bit 4 (16), `narrowed` → bit 5 (32), `sample_conflict` → bit 6 (64), `bg_bad_input` → bit 7 (128); `IDAW_HEALTH`); `link_state` (kartın A1 durumu); `param_store`, `param_fail`, `param_gen` (A4 `ida_plat_t`); `param_blob_crc` (kalıcı bloğun CRC-32/ISO-HDLC değeri); `batch_seq` (son çıktının `seq`'i); `sess`.

**`IDA_FC_STATUS`** (atış kontrolün emniyet zinciri durumu; A4 `ida_fc_status_t`'nin aynı adlı dört alanı): `state` (`IDAW_FC_STATE` = A4 `IDA_FCS_*`, 0–5; değerler ve anlamlar A4 §3.16'nın kapalı tablosudur), `inhibit_mask` (etkin emniyet engeli bitleri `IDA_FCI_*`; anlamları A4 §3.16'nın kapalı tablosundadır (A4-K91; 18 bit, yalnız emniyet engelleri; bit 18–31 ayrılmış ve 0; U1); A1 tabloyu yinelemez), `last_event_id`, `last_event_result`. Eski `ext_len` ve `ext[64]` alanları U1 ile A4'ten kaldırıldı; hedefleme ya da atış ölçütü taşıyan alan yoktur.

**`IDA_LOG`** (A4 `ida_log_t`): `t_w_ns`, `code`, `severity` (`IDAW_SEVERITY` = A4 `IDA_SEV_*`), `arg0`, `arg1`. `code` yalnız A4'ün kapalı günlük kodu tablosundaki değerleri alır (A4 §3.15, A4-K90; 1–23); `arg0`/`arg1`'in anlamı o tablonun koda göre satırıdır (kullanılmayan argüman 0); tabloda olmayan kod üretilmez ve yeni kod sözleşme revizyonu ister (U1). A1 tabloyu yinelemez; kart alanları dönüşümsüz kopyalar. Bu kanal hedefleme ya da atış ölçütü içeriği taşımaz.

**`IDA_CORE_COUNTERS`**: A4 `ida_core_counters_t`'nin 22 alanı, her biri **aynı adlı** ayrı bir `uint32_t` alanda (dizi değil; eşleme sıraya değil ada dayanır): `sample_rejected`, `sample_future`, `sample_conflict`, `sample_dup_dropped`, `narrow_from_rejected`, `bg_bad_input`, `imu_rejected`, `imu_gap`, `bad_input`, `dup_seq`, `old_seq`, `wall_backstep`, `cmd_rejected`, `widen_blocked`, `evres_ignored`, `event_unknown`, `det_rejected`, `ping360_rejected`, `bg_dropped`, `bg_slice_mismatch`, `log_dropped`, `step_miss` (A4 düzeltme turu 2 adayı, `core/include/ida/port.h`). Telde MAVLink'in boy sıralaması uygulanır; alıcı alanları adıyla okur. A4'ün yeni `IDA_BG_BAD_INPUT` arka plan dönüşü telde taşınmaz: A4-K117'ye göre dilim sayılmaz (`IDA_CALL_TRACE.bg_slices`'a girmez) ve `bg_bad_input` sayacı ile `health.bg_bad_input` bayrağında görünür. A4'ün `IDA_RC_*` listesi (0–8) `IDAW_CORE_RC` ile aynıdır.

**`IDA_LINK_STATS`**: `cnt[36]`, sıra ve tanımlar Tablo 3.11b'de.

**`IDA_CALL_TRACE`** (her çekirdek çağrısının kart tarafı girdileri; A1-K95; kaybedilebilir sınıf, A1-K110):

| Alan | Tip | Birim | Anlam |
|---|---|---|---|
| `now_w_ns` | `int64_t` | ns | Çağrının `in.now_w` |
| `batch_seq` | `uint32_t` | — | `IDA_ACTUATOR.batch_seq` ile aynı kural: `in.tf.seq`; ızgara ve oturum sonu tick'inde bu oturumda kapanan son partininki, yoksa 0 |
| `bg_slices` | `uint32_t` | — | `in.bg_slices` (A4-K109) |
| `prev_call_ns` | `uint32_t` | ns | `plat.prev_call_ns` |
| `rx_age_ns[28]` | `uint32_t` | ns | `now_w − t_rx`; dizin 0 `tf`, 1 `imu`, 2–17 `samples[0..15]`, 18–19 `det_batch[0..1]`, 20–27 `det[0..7]` (1 + 1 + 16 + 2 + 8 = 28); girdide olmayan öğe 0; 2³² − 1'de doyar |
| `n_rx` | `uint8_t` | — | `rx_age_ns`'te anlamlı öğe sayısı (son anlamlı dizinden bir fazlası) |
| `call_kind` | `uint8_t` | — | `IDAW_CALL_KIND`: 1 STEP, 2 TICK_TF, 3 TICK_GRID, 4 SESSION_END (oturum sonu çağrısı, A4-K122, A1-K107; ızgara tick'inden ayrı çağrı türüdür) |
| `core_rc` | `uint8_t` | — | Dönüş kodu |
| `plat_flags` | `uint8_t` | bit | `IDAW_PLAT_FLAGS` (`prev_overrun`, `tx_stalled`, `batch_merged`, `input_dropped`, `SESSION_START`, `SESSION_END`) |
| `n_merged` | `uint8_t` | — | Birleştirilen önceki parti sayısı |

**`IDA_FAULT_RECORD`** (01 §2.8): `cfsr`, `hfsr`, `mmfar`, `bfar`, `pc`, `lr`, `xpsr`, `step_counter`, `fault_seq`, `stacking_valid`. Yalnız bilgi.

### 3.8 Oturum mesajları

**`IDA_SIM_HELLO`**: `dialect_hash`, `nonce` (≠ 0), `run_id` (A1'in sayısal çalıştırma kimliği, ≠ 0; türetimi A1-K126), `ctrl_period_ns` (5 000 000), `proto_major` (1), `proto_minor` (0).

**`IDA_BOARD_HELLO`** (açılış kaydını taşır):

| Alan | Anlam (A4 kaynağı) |
|---|---|
| `dialect_hash`, `proto_major`, `proto_minor` | Kartın derlendiği A1 dialect'i |
| `ctrl_period_ns` | Kartın derlendiği kontrol periyodu (5 000 000) |
| `peer_nonce` | Yanıtlanan `SIM_HELLO.nonce`; duyuruda 0 |
| `nonce` | Kartın bu el sıkışma için ürettiği sayı (≠ 0) |
| `role` | `IDAW_ROLE_BOARD` ya da `IDAW_ROLE_SIL` |
| `boot_count`, `boot_count_ok`, `reset_cause`, `reset_flags`, `consecutive_faults`, `param_src` | `ida_init_in_t`'nin aynı adlı alanları |
| `init_rc` | `ida_core_init`'in dönüşü (`OK` ya da `PARAM_DEFAULTS`); platform güvenli beklemedeyse `NOT_INIT` (H33; çekirdeğin `NOT_INIT` çıktısının sabit içeriği A4-K126'dadır) |
| `port_major`, `port_minor`, `param_schema_hash`, `core_version` | `ida_core_info_t` |
| `det_schema_major`, `det_schema_minor` | A8 `IDA_DET_SCHEMA_*` |
| `fw_version`, `fw_git` | Kart yazılımı (ya da SIL koşucusu) sürümü ve commit özetinin ilk 32 biti |
| `param_blob_crc`, `param_blob_len`, `param_gen` | Kalıcı parametre bloğunun tamamının CRC-32/ISO-HDLC değeri, boyu (yoksa 0, 0) ve kuşağı |
| `last_cmd_id` | Bu açılışta çekirdeğe verilen son `cmd_id` (yoksa 0; A1-K66) |
| `prev_sess`, `sessions_since_boot` | Bu açılıştaki son oturum kimliği (yoksa 0) ve kurulmuş oturum sayısı |

**`IDA_SESSION_ACCEPT`**: `run_id` (`SIM_HELLO`'nunkiyle aynı; A1-K126), `nonce_sim`, `nonce_board`, `sess` (OK'de ≠ 0), `result` (`IDAW_SESSION_RESULT`), `run_mode` (`IDAW_RUN_MODE`; bilgi).

**`IDA_KEEPALIVE`**: `sess` (oturum yoksa 0), `state` (`IDAW_LINK_STATE`). **`IDA_SESSION_CLOSE`**: `sess`, `reason` (`IDAW_CLOSE_REASON`: 1 `SHUTDOWN`, 2 `PEER_TIMEOUT`, 3 `PROTOCOL`, 4 `SAFE_STATE`, 5 `RUN_END`, 6 `TX_OVERFLOW` (A1-K110), 7 `EVENT_OVERFLOW` (A1-K80; U12)).

**`IDA_TIME_FRAME`**:

| Alan | Tip | Birim | Aralık | Z | A4 karşılığı |
|---|---|---|---|---|---|
| `t_sim_ns` | `int64_t` | ns | 0 … 2⁶²; oturumda kesin artan | Z | `tf.t` |
| `batch_seq` | `uint32_t` | — | Oturumda seri aritmetikle bir artan | Z | `tf.seq` |
| `sess` | `uint16_t` | — | Güncel oturum | Z | — |
| `n_members` | `uint8_t` | — | 0–255 | Z | — (A1-K30) |
| `tf_flags` | `uint8_t` | bit | `IDAW_TF_FLAGS` | Z | `IMU`: partide IMU var (A1-K30) |
| `run_mode` | `uint8_t` | — | `IDAW_RUN_MODE` | B | — (A1-K24) |
| — | — | — | — | Z | `tf.present = 1`; `tf.t_rx` = TF'nin alım damgası (A1-K48) |

### 3.9 Zamanlama ve kapasite değerleri, izin verilen aralıklar ve eşitsizlikler

"İzin verilen aralık" sütunu, A5'in değiştirebildiği parametrelerin aralığıdır; aralıklar, içlerindeki **her** bileşim aşağıdaki bütün eşitsizlikleri sağlayacak biçimde seçilmiştir (kutu biçimli aralıklar; her eşitsizlik en kötü köşede sınandı). Aralık dışı yapılandırma A1-K104 ile reddedilir. "Sabit" değerler bu sürümde değiştirilemez.

| Ad | Değer | İzin verilen aralık | Saat | Dayanak | Kurallar |
|---|---|---|---|---|---|
| `P` | 5 ms | sabit | sim | İskelet §4 | A1-K29 |
| `L_act` | 10 ms (8 fizik adımı) | sabit | sim | İskelet §4 | A1-K74 |
| `T_bekle` (bariyerde bekleme sınırı) | 30 ms | 10–35 ms | sim duvarı | İskelet §4'ün başlangıç değeri 50 ms'ydi; E8'in iki partilik teslim kanıtı için düşürüldü (mimari seçimi, tahmin) | A1-K75, K84 |
| `T_kullanım` (çekirdek parametresi `valid_for_ns`) | 100 ms | 60–450 ms | sim (uygulama anından) | Mimari seçimi (tahmin) | A1-K74, K77; A4-K58 |
| `T_bağ` (bağlantı zaman aşımı; iki taraf) | 500 ms | 500–2 000 ms | her tarafın kendi duvarı | Mimari seçimi (tahmin) | A1-K22, K23 |
| `apply_by_ns` (atış kontrol parametresi) | 20 ms | 16–1 000 ms | sim (uygulama) | Mimari seçimi (tahmin); aralığın tek kaynağı bu tablodur (A1-K104), A4 aynısını kullanır (A4-K66; U7) | A1-K80, K84 |
| `hold_ns` (atış kontrol parametresi) | 200 ms; yenileme ≤ `hold_ns / 2`, önceki sonuç zamanında alındıysa (A4-K107, E9) | 60–450 ms | sim (uygulama), kart duvarı (üretici) | Mimari seçimi (tahmin) | A1-K86 |
| `T_canlı` | 100 ms | sabit | duvar | Mimari seçimi | A1-K20 |
| `T_hello` | 100 ms | sabit | sim duvarı | Mimari seçimi | A1-K12 |
| `T_duyuru` | 200 ms | sabit | kart duvarı | Mimari seçimi (06 §2.9) | A1-K11 |
| `T_hazır` | 1 s | sabit | sim duvarı | Mimari seçimi (06 §2.9) | A1-K27 |
| `T_reboot` | 5 s | sabit | sim duvarı | Mimari seçimi (06 §2.9) | A1-K17 |
| `T_SIL` | 5 s | sabit | sim duvarı | Mimari seçimi (06 §2.5) | A1-K76 |
| `T_emniyet` | 100 ms (10 Hz) | sabit | sim | 05 §2.3.8 (tahmin) | A1-K56 |
| TF yazımları arası alt sınır | `P/2` = 2,5 ms | sabit | sim duvarı | Mimari seçimi | A1-K39 |
| `T_olay_yin` (olay yineleme aralığı) | 15 ms | sabit | kart duvarı | Mimari seçimi (tahmin) | A1-K80 |
| `T_sonuç_pay` | 95 ms | sabit | kart duvarı | E8 | A1-K80 |
| `T_yetim` (Ping360 yetim yuva süresi) | 8 parti kapanışı | sabit | kart | Mimari seçimi; E12 | A1-K51 |
| `B_pencere` | 1 200 B | sabit | — | Mimari seçimi (MP3'te %80 sabit yük kayıpsız) | A1-K34 |
| `B_krit` (parti başına kritik bayt) | 520 B | sabit | — | Uçtan uca bütçeden türetildi (E18) | A1-K113 |
| `S_ayr` (sonarın ayrılmış payı) | 258 B (bir tam `PING360_DATA`) | sabit | — | E13, E14 | A1-K36, K112 |
| `R_s`, `C_s,max` (sonar kredisi) | 260 B/pencere, 600 B | sabit | — | E14, E15 | A1-K112 |
| `R_c`, `C_c,max` (toplu komut kredisi) | 60 B/pencere, 236 B (4 × 59) | sabit | — | E15 | A1-K112 |
| `R_iii` (kart, kaybedilebilir sınıf) | 500 B/periyot | sabit | kart duvarı | Mimari seçimi; E19 | A1-K110 |
| `N_olay` (kartın açık olay tablosu) | 8: izin açan olaylar ve `FIRE` en çok 7 yuva, 1 yuva `SAFETY_CLOSE`'a ayrılmış (U12) | sabit | — | Mimari seçimi; E16 | A1-K80 |
| `Q_sonuç` (sim'in yeni olay sonucu kuyruğu) | 8 giriş | sabit | — | E16 | A1-K111 |
| `Q_ii` (kartın zorunlu-güvenilir kuyruğu) | 64 çerçeve | sabit | — | E17 (kanıtın sınırı 59) | A1-K110 |
| `S_ii` (zorunlu sınıfa ayrılmış hizmet) | 1 000 B/periyot (200 B/ms) | sabit | kart duvarı | E17 | A1-K110 |
| Sim komut kovası | Daraltıcı olmayan: patlama 8, hız 2 / 5 ms; daraltıcı: her tür için patlama 1, hız 1 / 5 ms | sabit | sim duvarı | E17 | A1-K119 |
| Platform olay kovaları | İzin açan: patlama 4, hız 4 / 5 ms; `SAFETY_CLOSE`: patlama 3, hız 3 / 5 ms | sabit | kart duvarı | E17 | A1-K120 |
| `T_tf_gec`, `T_tick_yasak` | 1 ms, 1 ms (en uzun tick 0,5 ms + koruma 0,5 ms) | sabit | kart duvarı | Mimari seçimi (tahmin) | A1-K117 |
| `N_bg` (SIL'de parti başına arka plan dilimi) | çalıştırma parametresi | ≥ 0 | — | A1-K121 | A1-K121 |
| `A_sensör` (ön denetim: TF, IMU, emniyet dışı kritik sensör baytının parti başına ortalaması) | ≤ 150 B | ön denetim sınırı | — | E13 | A1-K113 |

`RTT_maks` = 14,5 ms, MP3'te 600 s'lik koşularda **gözlenen** en uzun gidiş-dönüştür ("Echo profilleri"); güvenceli bir üst sınır değildir. Aşağıdaki eşitsizliklerde kullanıldığı yerde güvence bu gözleme dayanır.

**Zorunlu eşitsizlikler** (aralıkların en kötü köşesinde; ✓ her bileşimde sağlanır; E1–E19 yapılandırma eşitsizlikleridir ve düzeltme turu 2'de yeniden hesaplandı; E20–E21 çapraz uyum turunda eklenen kapasite hesaplarıdır, parametreye bağlı değildir):

| # | Eşitsizlik | En kötü köşe | Gerekçe |
|---|---|---|---|
| E1 | `P < L_act` | 5 < 10 ✓ | İskelet §4 |
| E2 | `T_bekle < T_kullanım < T_bağ` | 35 < 60; 450 < 500 ✓ | Bariyerin tolere ettiği bir gecikme HIL 1x'te iticileri nötrlemez; bağlantı ölmeden önce komut kendiliğinden düşer |
| E3 | `T_kullanım ≥ L_act + P + RTT_maks` | 60 ≥ 29,5 ✓ | Gözlenen en uzun gidiş-dönüş tek bir yanıtı kaçırtsa bile önceki komut geçerlidir (varsayılanla `T_kullanım / P − 1` = 19 ardışık kaçırmaya kadar) |
| E4 | `4 · T_canlı ≤ T_bağ` | 400 ≤ 500 ✓ | Dört ardışık canlılık çerçevesinin kaybı oturumu bitirmez |
| E5 | `T_bağ > RTT_maks` ve `T_bağ > T_bekle` | 500 > 14,5; 500 > 35 ✓ | Gözlenen seyrek gecikmeler ve bariyer beklemesi oturumu bitirmez |
| E6 | `apply_by_ns ≥ L_act + P` | 16 ≥ 15 ✓ | Zamanında gelen bir olay son geçerliliği geçmez |
| E7 | `T_olay_yin < apply_by_ns` (kesin) | 15 < 16 ✓ | Son geçerlilik dolmadan en az bir yineleme gider. (İlk adayda alt sınır 15 ms idi ve `15 < 15` sağlanmıyordu; düzeltildi.) Olağan durumda sonuç, kararın kart duvarındaki anından ≈ 3–9 ms sonra kartta olur (hesap, tahmini girdilerle); gereksiz yineleme beklenmez |
| E8 | `2·T_bekle + P + RTT_maks + b ≤ T_sonuç_pay < IDA_EVT_RESULT_MARGIN` (b = 0,86 ms aşağı yön engellemesi) | 70 + 5 + 14,5 + 0,86 = 90,36 ≤ 95 < 100 ✓ | **Uygulanan** bir olayın sonucu kartın `EXPIRED_NO_ACK`'inden önce ulaşır. Kanıt: kararın kart duvarındaki anı `t_d` ≥ `TF(batch_seq)`'in yazımı; A1-K84 gereği olay ancak TF yazımından en geç `apply_by + T_bekle` sonra uygulanabilir; sonuç ilk dört içindeyse hemen, değilse bir sonraki TF yazımında (≤ `P + T_bekle` sonra; E16) yazılır; önünde en çok bir çerçeve (b) ve taşıma (`RTT_maks`) vardır. Toplam ≤ `t_d + apply_by + 2·T_bekle + P + b + RTT_maks`. Reddedilen bir olayın sonucu geç kalabilir; o durumda çekirdek olayı "sonucu bilinmiyor" sayar ve güvenli duruma geçer (A4-K70), bu güvenli yöndedir. İlk aday ikinci sonuç partisini saymıyordu (sol: 79,5 > 75); `T_bekle` aralığı 10–35 ms'ye, `T_sonuç_pay` 95 ms'ye çekildi |
| E9 | `hold_ns / 2 ≥ L_act + P + RTT_maks` ve `hold_ns < T_bağ` | 30 ≥ 29,5; 450 < 500 ✓ | Olağan durumda önceki `SAFETY_OPEN`'in sonucu `hold_ns / 2` içinde alınır ve yenilenen izin süresi dolmadan alıcıya ulaşır; eşitsizlik bunun **yeterli koşuludur**, kesintisiz yenilemenin güvencesi değildir: sonuç gecikirse (bariyer beklemesi, sonuç kuyruğu, yeniden gönderim) A4-K123 yeni izin olayını yasaklar, yenileme yapılmaz ve izin alıcıda `hold_ns` dolunca düşer (A4-K107); bu tanımlı ve güvenli sonuçtur. Bağlantı kopunca izin oturum bitişinden önce kendiliğinden düşer |
| E10 | `T_imu_stale + IDA_PORT_STALE_MARGIN_NS < T_kullanım`, `T_imu_stale`'in izin verilen en büyük değeriyle | 30 + 14,5 = 44,5 < 60 ✓ (`T_imu_stale` A4 parametresidir, aralığı 5–30 ms, varsayılan 20 ms; U7. Varsayılanla 34,5 < 60) | Kartın kendi nötr çıktısı sim'deki kullanım süresinden önce üretilir (A4-K39); A4 aralığı çekirdeğin parametre doğrulamasında sınırladığı için `T_imu_stale` ile `T_kullanım`'ın (60–450 ms) her birleşiminde sağlanır (P2-11) |
| E11 | Gönderen tarafta herhangi bir yarı açık 5 ms'lik pencerede en çok 2 TF | A1-K39 ✓ | A4'ün geliş varsayımı (A4 "A1'den beklenen" 8); varışta sıkışma olabilir (A1-K40) |
| E12 | `T_yetim ≥` bir satırın ilk parçasıyla HEAD'i arasındaki en çok parti kapanışı | 8 ≥ 6 ✓ | 1 200 örnekli satır 6 çerçevedir (5 DATA + HEAD); E14'e göre kuyruk boş değilken her pencerede en az bir sonar çerçevesi gider; HEAD en geç ilk parçanın penceresinden 5 pencere sonra gelir |
| E13 | (1)–(5) ≤ `B_krit`; `B_krit` − (1)–(5) ≥ en büyük sensör çerçevesi; `B_krit` + `det_azami` + `S_ayr` ≤ `B_pencere` | 29 + 59 + 34 + 102 + 124 = 348 ≤ 520; 520 − 348 = 172 ≥ 89; 520 + 399 + 258 = 1 177 ≤ 1 200 ✓ | **İzin verilen her yapılandırmada** geçerlidir: kritik bayt A1-K113 ile, algılama partisi A8 ile (tek kamera, en çok 4 kayıt; A8-K18, K35) sınırlıdır; sensör sayısı ya da `inst` ne olursa olsun fazlası kayar, TF, IMU, emniyet girdisi, daraltıcı komutlar ve olay sonuçları kaymaz, sonarın ayrılmış payı her pencereye sığar. İlk adayın tek örneğe dayalı E13'ü (691 + 399) yerine geçer; sol'un karşı örneği (üç DVL ve üç kamera kaydıyla 1 178 B ve 10,023 ms) artık oluşamaz, çünkü kritik bayt 520 B'ı aşamaz ve algılama partisi kuyruğun sonuna değil pencere başına gider (A1-K36, K39). Sensörlerin kaymasız akması ön denetimle (`A_sensör` ≤ 150 B < 172 B) sağlanır; en kötü erteleme süresine güvence yoktur, sayılır (A1-K113) |
| E14 | `R_s ≥` en büyük Ping360 çerçevesi | 260 ≥ 258 ✓ | Kredi her pencerede en az bir tam çerçeveye yeter; ayrılmış pay (`S_ayr`) yer sağlar; sonar komutlarca çiğnenmez |
| E15 | Aşağı yön bağlayıcı ortalama ≤ %60 ve tepe ≤ %90 (A1-K116) | `B_krit` + kamera + `R_c` + `R_s` = 520 + 59,85 + 60 + 260 = 899,85 ≤ 900; tek periyotta ≤ `B_pencere` = 1 200 ≤ 1 350 ✓ | **İzin verilen her yapılandırmada** (3.10) |
| E16 | Yeni sonuç kuyruğunun (a) her girişi kartta açık bir olaya aittir; dolayısıyla doluluk ≤ `N_olay` = `Q_sonuç` = 8 | Karar ≤ `t_d + apply_by + T_bekle + RTT_maks + h` (geç gelen kopya dahil); kuyrukta ≤ `P + T_bekle`; toplam ≤ `t_d + apply_by` + 14,5 + 1,25 + 35 + 5 + 35 = `t_d + apply_by` + 90,75 < `t_d + apply_by + T_sonuç_pay` (95) ✓ | Kartta bir olay yeri ancak sonuç alınınca ya da `apply_by + T_sonuç_pay` dolunca boşalır; bu sınırın içinde kuyruktaki sonucun olayı açıktır; açık olay sayısı `N_olay` ile ve platform kovasıyla (A1-K120) sınırlıdır. Kuyruk 8 girişle partide 4 sonuç hızında en çok iki pencerede boşalır. Saklı sonuç talepleri (b) kuyruğa girmez (A1-K111); ilk adaydaki karşı örnek (dokuz sonuçlanmış olayın gecikmiş yinelemesi) bu yüzden kuyruğu doldurmaz |
| E17 | Zorunlu-güvenilir kuyruk (ii) için (σ, ρ) geliş eğrisi ve hizmet eğrisi aynı saatte (kart duvarı), bütün `τ` aralıkları için: kuyruk ≤ 59 çerçeve ≤ `Q_ii` = 64 | Geliş: komut sonuçları ≤ 36 + 1,2·τ çerçeve (sim kovası: daraltıcı olmayan 8 + 0,4/ms, daraltıcı 4 + 0,8/ms; taşıma titreşimi 14,5 ms ve platform ertelemesi 5 ms için 1,2 × 19,5 ≈ 24 eklenir); yeni olaylar ≤ 7 + 1,4·τ (iki platform kovası); yinelemeler ≤ 8 + 0,533·τ (`N_olay` / `T_olay_yin`); bir canlılık ya da oturum çerçevesi; bayt olarak α(τ) ≤ 2 154 + 141,5·τ B (τ ms). Hizmet: (ii)'ye ayrılmış 200 B/ms, gecikme payı `T` = (390 B (i) patlaması + 156 B engelleme) / 275 B/ms ≈ 2,0 ms; β(τ) = 200·(τ − 2)⁺. Kuyruk sınırı = α(T) ≤ 2 437 B ve ≤ 52 + 3,13·2 ≈ 59 çerçeve; gecikme sınırı ≤ T + σ/200 ≈ 12,8 ms; uzun dönem ρ = 141,5 < 200 ✓ | Birleştirme, mandal sonuçları (`COALESCED`, `LINK_DROPPED`, `LINK_BAD_FIELD`) ve `DUPLICATE`, her biri tek bir komut çerçevesine karşılık geldiği için komut gelişinin içindedir. İlk adayın parti sayımına dayalı kanıtının yerine geçer (sol #7). Kuyruk modeli (yanıt belgesi) en kötü patlamada 34 çerçeve gördü |
| E18 | **Çağrı çakışması olmayan partiler için** uçtan uca en kötü ≤ `L_act` | 9,733 ≤ 10 ✓ | Aşağıdaki bütçe; çakışma A1-K117 ile sayılır ve partiyi zamanlama bakımından geçersiz kılar. Çakışmalı partinin en kötüsü: önceki ızgara tick'i sürerken 10,233 ms; aşan bir step sürerken (A4-K19, başlangıç ≤ 4 ms) 13,233 ms; ikisi de `L_act`'ı aşar ve güvence dışıdır |
| E19 | Yukarı yön bağlayıcı uzun dönem ≤ %90 (A1-K116) | (i) 75 + (ii) 141,5 × 5 = 707,5 + (iii) `R_iii` 500 = 1 282,5 ≤ 1 350 (%85,5) ✓ | E17'nin geliş hızlarıyla; ilk turun daha gevşek varsayımlarıyla da %84,27 < %90 |
| E20 | Bir çekirdek çağrısına düşen emniyet örneği sayısı `S` ≤ 16 (A4-K31 örnek kapasitesi; A1-K124) | Her pencere en çok 1 `IDA_SAFETY` taşır (A1-K34, K56; `DUP`/`OLD` pencereleri dahil). Kart boştayken kapanan pencere hemen çağrılır (`S` ≤ 1); bir çağrı sürerken kapanan pencereler tek bekleyen girdide toplanır (A4-K102), yani `S` ≤ 1 + `w`, `w` = önceki çağrının süresi `D_c` içinde kapanan emniyet taşıyan pencere sayısı. Sim ardışık TF yazımları arasında ≥ `P/2` = 2,5 ms bırakır (A1-K39): taşıma bu aralığı korursa `w` ≤ ⌈`D_c` / 2,5 ms⌉ ve `S` ≤ 16 ⇔ `D_c` ≤ 37,5 ms; taşıma pencereleri hat hızında sıkıştırırsa emniyet taşıyan pencere en az `L_min` = TF'nin en küçük tel boyu + `IDA_SAFETY`'nin en küçük tel boyu = 29 + 25 = 54 B'dir (≥ 0,18 ms) ve `S` ≤ 16 ⇔ `D_c` ≤ 15 × 54 / 300 = 2,70 ms ✓ koşullu. **En küçük tel boyu** (A1-K05'in kırpmasıyla): bir çerçevenin, kurallar gereği sıfır olamayan son alanının ilk baytına kadarki yükü + 12 B; sıfır olmayan değer en kötü durumda yalnız en düşük baytındadır. TF: tel sırasının son alanı `run_mode` 1–4 → yük 17 B, çerçeve 29 B (kırpılmaz). `IDA_SAFETY`: tel sırası `t_meas_ns`, `seq`, `safety_rev`, `flags`, `inst`, `estop`, `tube[2]`; sıfır olamayan tek alan `safety_rev` ≥ 1 (A1-K106) → yük 13 B, çerçeve 25 B (`flags` hata enjeksiyonunda 0 olabildiği için ona güvenilmez; `VALID` kuruluyken 17 B / 29 B). İlk adaydaki 63 B azami boyla hesaplanmıştı; 58 B (W1 doğrulama incelemesi) `flags = VALID` varsayımıyla doğrudur | **Koşullu; zamanlamadan bağımsız bir kanıt verilemez**: A4 çağrı süresine sınır koymaz ve ST-LINK köprüsünün ne kadar veriyi tutup birden bırakabileceği ölçülmedi (O8). Gerçek çekirdeğin çağrı bütçesi 3,5 ms'dir (tahmin; E18) ve 2,70 ms'yi aşar: sınır yalnız ≥ 17 emniyet taşıyan pencerenin (sim tarafında ≥ 40 ms'lik yazımın) köprüde biriktirilip tek çağrı süresinde hat hızıyla bırakılmasıyla aşılabilir; bu durum çağrı çakışmasıdır (A1-K117) ve çalıştırmayı zaten geçersiz kılar. Aşılırsa A1-K124 hiçbir emniyet örneğini atmadan oturumu bitirir (oturum sonu daha kısıtlayıcıdır) |
| E21 | Alıcıda (kartta) herhangi bir `τ` ms'lik aralıkta kapanan parti sayısı (A4 "A1'den beklenen" md. 8) | (a) Taşıma gönderen aralığını koruduğunda (A1-K39: ardışık TF yazım başlangıçları ≥ 2,5 ms): ≤ `1 + ⌊τ / 2,5⌋`; 5 ms'de 3, 10 ms'de 5. (b) Her koşulda (köprü veriyi tutup birden bıraksa bile): bir pencere telde en az bir TF'dir (29 B) ve kart hattan 300 B/ms'den hızlı bayt alamaz; ≤ `1 + ⌊τ · 300 / 29⌋`; 5 ms'de 52 ✓ hesap | (b) fiziksel ve koşulsuz sınırdır; (a) olağan çalışmanın sınırıdır ve köprünün sıkıştırması ölçülmedi (O8; MP3'te 10 ms'yi aşan seyrek gecikmeler sıkışmaya yol açabilir). A4-K19 ve K102 (a)'ya dayanır; (b)'de A4-K102 birleştirmeyle tek bekleyen girdide kalır, sığmayan emniyet dışı üyeler düşer ve A1-K117 çağrı çakışmasını sayar |

**Uçtan uca bütçe** (HIL, bariyerli ya da 1x; `TF(n)`'nin `W(t_n)`'de yazılmasından sim'in yuva `n`'i `a_n`'de uygulamaya hazır olmasına kadar; **yalnız çağrı çakışması olmayan partiler için**, A1-K117). Uçtan uca ölçülmedi. "Nitelik": **ölçüm** (bir ölçüm raporunun doğrudan değeri), **hesap** (aritmetik), **ölçümden türetilmiş tahmin**, **tahmin**.

| Kalem | En kötü (ms) | Olağan (ms) | Nitelik | Dayanak |
|---|---:|---:|---|---|
| a. Sim: TF(n) yazımının `W(t_n)`'den gecikmesi | 0,500 | 0,500 | tahmin | Mimari seçimi |
| b. Aşağı yön engelleme: taşımada önceden yazılmış bir tam çerçeve (≤ 258 B; A1-K115) | 0,860 | 0 | hesap (anma hızından); köprü tamponu doğrulanmadı | O7 |
| c. Aşağı yön kritik kuyruk: en kötü `B_krit` = 520 B; olağan TF + IMU 88 B | 1,733 | 0,293 | hesap | bayt × 10 / 3 000 000 |
| d. Aşağı yön USB + ST-LINK ek gecikmesi | 0,400 | 0,200 | ölçümden türetilmiş tahmin | MP3 64 B echo p50 / p99,9 0,83 / 1,10 ms eksi iki yönün tel süresi; yön başına ayrı ölçülmedi |
| e. Kart: dağıtım (önceki çağrı yokken; A1-K117) | 0,500 | 0,500 | tahmin | A4-K19 |
| f. Kart: `ida_core_step` | 3,500 | 3,500 | tahmin (bütçe) | A4-K23; ölçülen değer yalnız yer tutucunundur: MP4a 200 Hz ağır yükte en çok 3,32196875 ms (ölçüm, gerçek çekirdek değil) |
| g. Kart: çıktı kodlama | 0,100 | 0,100 | tahmin | 01 §2.4 |
| h. Yukarı yön engelleme: sürücüdeki bir kritik olmayan çerçeve (≤ 156 B; A1-K110) | 0,520 | 0 | hesap | — |
| i. Yukarı yön kritik: 4 yeni olay + `ACTUATOR` (246 B); olağan 50 B | 0,820 | 0,167 | hesap | — |
| j. Yukarı yön USB + ST-LINK ek gecikmesi | 0,400 | 0,200 | ölçümden türetilmiş tahmin | d ile aynı |
| k. Sim: uygulama adımına hazırlık `C_sim` | 0,400 | 0,400 | tahmin | 06 §2.5 |
| **Toplam** | **9,733** | **5,860** | | `L_act` = 10 ms'ye pay **0,267 / 4,140 ms (tahmin)** |

Çağrı çakışmalı partiler (A1-K117) bu bütçenin dışındadır: önceki bir ızgara tick'i sürerken TF gelirse e kalemine en çok 0,5 ms eklenir (toplam 10,233 ms); aşan bir step sürerken gelirse dağıtım en çok 4 ms olur (A4-K19; toplam 13,233 ms); ikisi de `L_act`'ı aşar, `cagri_cakismasi` sayılır ve çalıştırma zamanlama bakımından geçersizdir. A1-K117'nin tick takvimi, TF'ler `P` aralıkla ve zamanında geldiği sürece çakışmayı olanaksız kılar (yanıt belgesindeki modelde titreşimsiz akışta 0 çakışma). `B_krit` bu toplamın 10 ms'yi aşmayacağı en büyük değerin (600 B) altında seçildi. MP3'teki seyrek 10–14,5 ms'lik gidiş-dönüşler bu bütçeyle karşılanmaz; o yanıtlar A1-K75'e göre kaçırılır ya da bariyerde beklenir ve çalıştırma geçersiz işaretlenir (A1-K79, K108). Gerçek değerler W3 (gerçek çekirdek, M5) ve W4 (HIL köprüsü; MP5) ölçümleriyle yeniden değerlendirilir (O1–O3).

### 3.10 Bant bütçesi

Hesap; boylar Tablo 3.3'teki en büyük çerçeve boylarıdır (kırpma kazancı sayılmaz). "Nitelik" sütunu: **B** bağlayıcı üst sınır (bir A1 kuralı ya da başka bir sözleşmenin sınırı her yapılandırmada sağlar); **V** varsayımsal profil (05 §2.2'nin tahmini varsayılanları; A5 değiştirebilir, ön denetim A5 değerleriyle yeniden hesaplar). %53,8 ve %56,4 gibi ilk adaydaki oranlar yalnız varsayımsal profil içindi (sol); aşağıda iki nitelik ayrı toplanır.

**Sim → kart, periyot başına ortalama (200 Hz)**

| Kalem | Nitelik | Hesap | B / periyot |
|---|---|---|---:|
| `TIME_FRAME` | B | 29 × 1 | 29,0 |
| `IMU` | B | 59 × 1 | 59,0 |
| `SAFETY`, 10 Hz + saniyede ≤ 10 değişim | V (bağlayıcı: partide ≤ 1, ≤ 34 B) | 34 × 0,1 | 3,4 |
| `MAG` 100 Hz, `BARO` 25 Hz, `GNSS` 5 Hz, `DVL` 15 Hz, `PING1D` 20 Hz, `BATT` 10 Hz, `LEAK` 10 Hz (tek örnek) | V | 19,5 + 4,875 + 1,525 + 6,675 + 4,8 + 2,15 + 1,4 | 40,9 |
| **Kritik sensörler, profil** | V | | **132,3** |
| **Bütün kritik üyeler, üst sınır** | B (A1-K113) | `B_krit` | **≤ 520** |
| Kamera algılaması, 30 Hz, karede 4 kayıt, tek kamera | B (A8 3.6, K18, K35) | 399 × 0,15 | 59,85 |
| Toplu komutlar: profil MANUAL 50 Hz + HEARTBEAT 1 Hz + diğer ≤ 5/s | V (bağlayıcı: `R_c` ≤ 60) | (43 × 50 + 22 + 59 × 5) / 200 | 12,3 |
| Olay sonuçları ≤ 20/s | V (bağlayıcı: `B_krit` içinde) | 31 × 0,1 | 3,1 |
| Ping360, 05'in 50 m varsayılanı (≈ 12,1 satır/s × 1 329 B) | V (bağlayıcı: `R_s` ≤ 260) | | 80,4 |

| Toplam | B / periyot | Kapasiteye oranı |
|---|---:|---:|
| Varsayımsal profil (Ping360 50 m) | 288,0 | %19,2 |
| Varsayımsal profil, Ping360 bağlayıcı payında | 467,6 | %31,2 |
| **Bağlayıcı üst sınır, izin verilen her yapılandırma** (`B_krit` + kamera + `R_c` + `R_s`; E15) | **899,85** | **%60,0** |
| Tek pencerede bağlayıcı üst sınır (`B_pencere`) | 1 200 | %80,0 |

**Ping360 çıktısı.** 1 200 örnekli satır 39 + 5 × 258 = 1 329 B, 600 örnekli satır 39 + 2 × 258 + 138 = 693 B (hesap). Sonar kredisi `R_s` = 260 B/pencere = 52 000 B/s: 1 200 örnekte en çok ≈ 39 satır/s, 600 örnekte ≈ 75 satır/s. Kuyruk boş değilken her pencerede en az bir tam çerçeve gider (E14); bu yüzden başlamış bir 1 200 örnekli satır en geç 6 pencerede (30 ms) teslim edilir ve bir satırın `t_hazır`'ından teslimine en kötü gecikme 1 + 6 + 6 = 13 penceredir (65 ms; uygunluk penceresi + önceki satırın kalanı + kendisi; A1-K37'nin tek bekleyen satırıyla). Bu güvence pencere planı içindir; sim'in gerçek zamanın gerisinde kalması (A1-K108) planı değiştirmez ama teslimi geciktirir. 05'in 50 m varsayılanı (≈ 12,1 satır/s) seyreltilmez; 7 m örneğindeki akustik üst sınır (≈ 107 satır/s, 05 §2.4.5) ≈ 39 satır/s'e seyreltilir (A1-K37; hesap). İki kademede de satır üst sınırı 1 200 örnektir.

**En kötü pencere** (A1-K34, K36, K113; izin verilen her yapılandırmada): kritik ≤ 520 B + algılama partisi ≤ 399 B + sonarın ayrılmış payı ≤ 258 B = 1 177 B; kalan ≥ 23 B ve kredilerin izin verdiği kadar toplu komut ve ek sonar çerçevesi; toplam ≤ `B_pencere` = 1 200 B (MP3'te %80 sabit yükte 600 s kayıpsız). Algılama partisi bulunmayan pencerelerde komutlara en az 1 200 − 520 − 258 = 422 B kalır; kamera 30 Hz'te olduğundan algılama partisi ardışık 6 pencerenin en çok birindedir.

**Kart → sim, periyot başına ortalama**

| Kalem | Nitelik | Hesap | B / periyot |
|---|---|---|---:|
| (i) `ACTUATOR` | B (TF başına 1) | 50 × 1 | 50,0 |
| (i) `ACTUATOR` `ASYNC` (yalnız arıza durumunda) | B (≤ 100 Hz) | 50 × 0,5 | ≤ 25,0 |
| (ii) `EVENT`: profil ≤ 20/s, bir yinelemeyle | V | 49 × 0,2 | 9,8 |
| (ii) `EVENT`: üst sınır (yeni: iki platform kovası 1,4/ms; yineleme ≤ `N_olay` / `T_olay_yin`) | B | 49 × (7 + 2,67) | ≤ 473,7 |
| (ii) `CMD_RESULT`: profil ≤ 56/s | V | 39 × 0,28 | 10,9 |
| (ii) `CMD_RESULT`: üst sınır (sim komut kovası: 1,2 komut/ms) | B | 39 × 6 | ≤ 234,0 |
| (iii) `CALL_TRACE` (200 step + 100 ızgara tick'i / s) | B (çağrı başına 1) | 149 × 1,5 | 223,5 |
| (iii) `NAV` 50 Hz, `STATUS` 10 Hz, `FC_STATUS` 10 Hz, sayaçlar 1 Hz | B (A1-K91) | 32,5 + 2,4 + 1,3 + 1,3 | 37,5 |
| (iii) `LOG` | V ≈ 0 (bağlayıcı: çağrı başına ≤ 4, ≤ 186) | | — |
| (iii) `DET_RECORD`: profil, 50 m satır hızında, satır başına 4 kayıt | V (bağlayıcı: ≤ 363,6, 200 örnekli satırlarla `R_s` hızında) | 90 × 4 × 12,1 / 200 | 21,8 |

| Toplam | B / periyot | Kapasiteye oranı |
|---|---:|---:|
| Varsayımsal profil (`ASYNC`, `LOG` yok) | 353,5 | %23,6 |
| İlk adayın varsayımlarıyla (profil + `ASYNC` + `LOG` üst sınırı + 173 satır/s ile algılama 311,4; bu turun çerçeve boylarıyla) | 854,1 | %56,9 |
| **Bağlayıcı üst sınır**: (i) 75 + (ii) 707,7 (E17 hızları) + (iii) `R_iii` 500 | **1 282,7** | **%85,5** |

Bant ölçütü A1-K116'dadır (mimar kararı 2026-11-20; iskelet §3, sürüm 0.9'da işlendi, 0.10'da aynı): %60 ortalama hedefi olağan çalışma profili içindir ve iki yönde de sağlanır (%19,2 ve %23,6); bağlayıcı üst sınır her yönde %90'ı (1 350 B/periyot) aşmaz: aşağı yönde uzun dönem %60,0 ve tek periyot %80 (E15), yukarı yönde uzun dönem %85,5 (E19; ilk turun varsayımlarıyla %84,27) ve bu değer E17'nin kuyruk kanıtındaki geliş hızlarıyla aynıdır (zorunlu sınıf 141,5 B/ms < ayrılmış 200 B/ms; kalan kapasite (iii)'e 300 − 15 − 141,5 ≈ 143,5 B/ms ≥ `R_iii` 100 B/ms bırakır). Kapasite (%100) bağlayıcı sınırla da aşılmaz; kaybedilebilir sınıf `R_iii` ile sınırlıdır ve önce `DET_RECORD`, sonra `LOG` atılır (A1-K90 sırası, A1-K110). Sol'un hesapladığı %56,751, ilk adayın çerçeve boylarıyla aynı varsayımın değeridir; bu turda `ACTUATOR` 4 B büyüdü, `FC_STATUS` 65 B küçüldü.

### 3.11 Sayaçlar

Sim'in sayaçları 64 bittir ve çalıştırma boyunca birikir; kartın sayaçları (Tablo 3.11b'nin tel katmanı sayaçları dahil) 32 bittir, açılıştan (SIL'de init'ten) beri birikir ve 2³²'de sarar; sim kart sayacının 64 bitlik birikimini ardışık iki okumanın farkıyla (`(yeni − eski) mod 2³²`) kurar, bu yüzden iki okuma arasında 2³²'den az artış varsayılır (1 s'de bir okuma ile olağan sayaçlar için sağlanır). Sim bütün sayaçları A3'e yayımlar ve A6'ya yazar; her sayaç artışının nedeni olan karar (ret nedeni, parti numarası) A6'ya ayrıca yazılır.

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
| `komut_bayat_yetki` | Sim'in gönderim kuyruğunda revizyonu değiştiği için gönderilmeden kapatılan genişletici komut (A3'e `bayat_yetki`) | A1-K129 |
| `calistirma_gecersiz` ve neden bitleri | Zamanlama geçerliliği | A1-K79 |
| `son_tarih_kacirildi`, `sim_gecikmesi` | Yuva sonucundan ayrı: yuvayı tamamlayan çerçevenin `D_n`'den sonra alınması (bariyerde kurtarılanlar dahil); sim'in fizik adımını planlanan duvar anından sonra hesaplaması | A1-K108 |
| `yanit_ret_eski_uretim` | `out_seq`'i artmayan ya da son uygulanandan büyük olmayan sürekli çıktı | A1-K78, K105, K109 |
| `alan_ret_<mesaj>`, `bag_ret_<mesaj>` | Tablo 4.6'da reddedilen kart çerçeveleri, mesaj ve denetim sırasına göre | A1-K109 |
| `olay_rev_ret` | `safety_rev` damgası güncel değil diye `REJ_SAFE` alan izin açan olaylar; `safety_rev` (güncel değer) | A1-K106 |
| `sonuc_kuyrugu_en_buyuk` | Olay sonucu kuyruğunun en büyük doluluğu (≤ `Q_sonuç`) | A1-K111 |
| `kritik_erteleme`, `kritik_erteleme_en_buyuk` | `B_krit` yüzünden kayan kritik üyeler ve en büyük kayma (parti) | A1-K113 |
| `cagri_izi_eksik` | Gelmeyen `IDA_CALL_TRACE`'ler; ilgili periyot "zamanlaması doğrulanmamış" | A1-K95 |
| `yapilandirma_ret` | A1-K104 ile reddedilen yapılandırma ve sağlanmayan eşitsizlik | A1-K104 |
| `cagri_cakismasi` | `CALL_CONFLICT` bayraklı partiler | A1-K117 |
| `sonuc_talebi_atildi` | Yer olmadığı için yanıtlanmayan saklı sonuç talepleri | A1-K111 |
| `komut_kovasi_bekleme` | Kova yüzünden bekleyen komutlar | A1-K119 |
| `emniyet_rev_doydu` | Sayaç doygunluğa ulaştı (bayrak) | A1-K106 |

**Tablo 3.11b — Kart sayaçları (`IDA_LINK_STATS.cnt[]` sırası)**

| # | Ad | Tanım | A4 `plat` karşılığı |
|---:|---|---|---|
| 0 | `rx_bytes` | Alınan bayt | — |
| 1 | `rx_frames` | Geçerli çerçeve | — |
| 2 | `rx_crc_err` | Sağlaması tutmayan çerçeve (dialect'teki kimliklerle; A1-K07) | `cnt_rx_error`'a girer |
| 3 | `rx_unknown_msgid` | Dialect'te olmayan (başlıkta) ya da yönü yanlış (sağlamadan sonra) kimlik (A1-K06) | `cnt_rx_error`'a girer |
| 4 | `rx_iflag` | `incompat_flags ≠ 0` | `cnt_rx_error`'a girer |
| 5 | `rx_bad_len` | `len` geçersiz | `cnt_rx_error`'a girer |
| 6 | `rx_wrong_sys` | `sysid`/`compid` sim'inki değil | — |
| 7 | `rx_mav1` | Aday dışında görülen `0xFE` baytı (A1-K02) | — |
| 8 | `rx_overflow` | Alım tamponu taşması | `cnt_rx_error`'a girer |
| 9 | `rx_resync_bytes` | Çerçeve dışında atılan bayt | — |
| 10 | `rx_seq_gap` | Kabul edilen çerçevede başlık `seq`'inin beklenen değerden farklı olduğu olay (A1-K08) | — |
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
| 31 | `tx_dropped` | Atılan kaybedilebilir (iii) çerçeve | — |
| 32 | `tx_crit_stall` | Kritik çerçevenin taşımaya verilemediği periyot; (ii) sınıfının taşması bu sayacı artırır ve oturumu bitirir (A1-K110) | `tx_stalled` bunun anlık karşılığıdır |
| 33 | `events_sent` | İlk kez gönderilen olay | — |
| 34 | `events_resent` | Olay yinelemesi | — |
| 35 | `sessions` | Kurulan oturum | — |

`plat.cnt_rx_error` = `rx_crc_err` + `rx_iflag` + `rx_bad_len` + `rx_overflow` + `rx_unknown_msgid` (A4 3.6'nın "CRC ve çerçeveleme hatası" tanımının A1 karşılığı). Gerekçe (mimar kararı): iki uç aynı `dialect_hash` ile el sıkıştığından dialect dışı kimlik yalnız hat bozulmasından doğar (çerçeveleme hatası); yönü yanlış ama sağlaması doğru çerçeve uyumlu bir karşı uçta oluşmaz, oluşursa hata sayılması zararsızdır. Her çerçeve bu beş sayaçtan en çok birine girer (A1-K07'nin tek ret nedeni), çifte sayım yoktur.

---

## 4. Davranış kuralları

Her kural tek bir sınanabilir cümledir. "Sim" ve "kart" Bölüm 1.1'deki taraflardır; "kart" SIL koşucusunu da kapsar (SIL'e özgü farklar 4.9'dadır). Zamanlama değerlerinin sayısal karşılıkları ve dayanakları 3.9'dadır; kurallarda adlarıyla anılır.

### 4.1 Fiziksel katman ve çerçeve

- **A1-K01.** Kartta A1 yalnız ST-LINK sanal seri portundan (USART1) geçer ve iki taraf portu 3 000 000 baud, 8N1, donanım ve yazılım akış denetimi kapalı kullanır; PC tarafı portu ham kipte (yankı, satır düzenleme ve karakter çevirisi kapalı) açar ve DTR/RTS hatlarının durumuna hiçbir davranış bağlamaz.
- **A1-K02.** Her iki taraf yalnız `magic = 0xFD`, `incompat_flags = 0` ve `compat_flags = 0` olan MAVLink 2 çerçevesi gönderir; alıcı `incompat_flags ≠ 0` olan adayı atar (`rx_iflag`) ve `compat_flags`'i yok sayar. MAVLink 1 çerçevesi doğrulanamaz ve bütün olarak atlanmaz: aday dışındayken görülen her `0xFE` baytı `rx_mav1`'i bir artırır (ve aday başlatmadığı için `rx_resync_bytes`'a da sayılır); ardından gelen baytlar olağan `0xFD` taramasına girer (`len`'e göre atlama yapılmaz).
- **A1-K03.** `protocol/ida.xml`'deki A4 ayna numaralandırmalarının (`IDAW_CMD_TYPE`, `IDAW_MODE`, `IDAW_FC_OP`, `IDAW_FC_STATE`, `IDAW_EVENT_KIND`, `IDAW_EVENT_RESULT_CODE`, `IDAW_ACK`, `IDAW_REASON` 0–15, `IDAW_CORE_RC`, `IDAW_SAMPLE_FLAGS`, `IDAW_DVL_FLAGS`, `IDAW_TUBE_FLAGS`, `IDAW_SP_MASK`, `IDAW_MI_TYPE`, `IDAW_MI_FRAME`, `IDAW_MCTL`, `IDAW_PARAM_TYPE`, `IDAW_RES_KIND`, `IDAW_FS_LEVEL`, `IDAW_GNSS_FIX`, `IDAW_SEVERITY`) her girdisi `core/include/ida/port.h`'taki karşılık `IDA_` sabitiyle aynı değerdedir; adlar önekle ayrıldığından (U15) iki başlık aynı çeviri biriminde içerilir ve eşitlik derleme zamanında denetlenir (7.1, K03 satırı). Girdi girdi eşleme `protocol/ayna.json`'dadır (21 numaralandırma, 136 eşitlik; `IDAW_REASON` yalnız 0–15); ad kuralı mekanik değildir (ör. `IDAW_SAMPLE_FLAGS_VALID` ↔ `IDA_HF_VALID`, `IDAW_CORE_RC_OK` ↔ `IDA_RC_OK`), bu yüzden eşleme bu dosyadan okunur. Dosya bu sözleşmenin parçasıdır ve `dialect_hash`'e girmez.
- **A1-K04.** Sim her çerçeveyi `sysid = 250`, `compid = 25`; kart ve SIL koşucusu `sysid = 1`, `compid = 1` ile gönderir; her alıcı karşı tarafın kimliğini taşımayan çerçeveyi atar (`rx_wrong_sys`).
- **A1-K05.** Gönderen yükün sondaki sıfır baytlarını, ilk baytı koruyarak kırpar; alıcı kırpılmış ve kırpılmamış çerçeveyi kabul eder, eksik kısmı sıfırla doldurur ve `len` = 0 ya da `len` mesajın en büyük yük boyundan büyükse çerçeveyi atar (`rx_bad_len`).
- **A1-K06.** Alıcı dialect'te olmayan mesaj kimliğini taşıyan adayı başlık denetiminde atar ve yalnız `rx_unknown_msgid`'e sayar (`CRC_EXTRA`'sı bilinmediği için sağlaması doğrulanamaz; `rx_crc_err`'e girmez, `rx_unknown_msgid` üzerinden `cnt_rx_error`'a girer, 3.11); dialect'te olan ama kendi yönünde alınmaması gereken (Tablo 3.3 "Yön" sütunu) kimliği taşıyan çerçeveyi sağlaması ve `sysid`/`compid` denetiminden sonra atar ve yine `rx_unknown_msgid`'e sayar.
- **A1-K07.** Alıcının denetim sırası şudur: başlıkta `incompat_flags` (A1-K02) → kimliğin dialect'te olması (A1-K06) → `len` (A1-K05); ardından sağlama (`rx_crc_err`); sağlama tuttuktan sonra `sysid`/`compid` (A1-K04) → yön (A1-K06); ilk tutmayan denetim tek ret nedenidir (bu yüzden sağlaması bozuk, yabancı `sysid`'li çerçeve `rx_crc_err`'e sayılır). Başlık denetiminde reddedilen adayda ayrıştırma adayın `0xFD`'sinden sonraki bayttan, `0xFD` arayarak sürer (adayın arkasındaki geçerli çerçeve kaçırılmaz); sağlaması tutmayan adayda ise adayın tamamı tüketilir ve ayrıştırma aday çerçevenin son baytından sonraki bayttan sürer; aday başlatmayan ve bir çerçeveye ait olmayan her bayt `rx_resync_bytes`'a sayılır.
- **A1-K08.** Her gönderen çerçeve başlığındaki `seq`'i oturumdan bağımsız olarak her çerçevede bir artırır (mod 256); alıcı `seq` geçmişini yalnız kabul ettiği çerçevelerle günceller ve kabul ettiği her çerçevede `seq` bir önceki kabul edilenin `seq`'i + 1 (mod 256) değilse `rx_seq_gap`'i bir artırır (sayaç kayıp çerçeve sayısını değil olay sayısını sayar; aynı `seq`'in yinelenmesi de bir olaydır; ilk kabul edilen çerçevede karşılaştırma yapılmaz); bu sayaca hiçbir davranış bağlanmaz.
- **A1-K09.** Kart alım yolu 300 000 B/s'lik sürekli akışı bayt kaybetmeden işler; alım tamponu yine de taşarsa kart işlenmemiş baytları atar, `rx_overflow`'u artırır, ayrıştırmayı yeniden eşler ve açık partiyi eksik sayar (A1-K30).
- **A1-K10.** Her taraf taşımaya yalnız tam çerçeveyi sırasıyla verir, iki çerçeveyi bayt akışında iç içe geçirmez ve yazımına başladığı çerçeveyi, oturum sonu dahil, tamamlar ya da keser; kesilen çerçeve karşı tarafta A1-K07 ile atılır.

### 4.2 Oturum

Kartın durumları: `IDLE` (oturum yok), `HANDSHAKE` (bir `SIM_HELLO` yanıtlandı), `RUNNING` (oturum kuruldu; A4 `IDA_LINK_RUNNING`). Sim'in durumları: `KAPALI`, `EL_SIKISMA`, `HAZIR` (kabul gönderildi, kartın ilk oturum çerçevesi bekleniyor), `KOSUYOR`, `KOPUK`.

- **A1-K11.** Kart `IDLE`'da `T_duyuru` aralıkla `peer_nonce = 0` taşıyan `IDA_BOARD_HELLO` (duyuru) gönderir ve `IDA_BOARD_HELLO` dışında hiçbir çerçeve göndermez; tek istisna, `IDLE`'a geçmeden önce taşımaya verilmiş oturum sonu çağrısının `IDA_CALL_TRACE`'inin yazımının tamamlanmasıdır (A1-K22, K10).
- **A1-K12.** Sim portu açtıktan hemen sonra işletim sisteminin alım ve gönderim tamponlarını boşaltır, yeni bir el sıkışma için sıfırdan farklı yeni bir `nonce` seçer (HIL'de gerçek rastgelelikten, SIL'de A1-K114'e göre tohumdan), `peer_nonce`'u bu `nonce`'a eşit bir `IDA_BOARD_HELLO` alana kadar aynı `nonce`'lu `IDA_SIM_HELLO`'yu `T_hello` aralıkla gönderir ve o ana kadar aldığı bütün çerçeveleri atar.
- **A1-K13.** Kart bir `IDA_SIM_HELLO` aldığında: `nonce` yanıtladığı son `SIM_HELLO`'nunkiyle aynıysa ve durumu `HANDSHAKE` ise aynı `IDA_BOARD_HELLO`'yu yeniden gönderir, durumu `RUNNING` ise yok sayar; `nonce` farklıysa ve durumu `RUNNING` ise önce oturumu A1-K22 ve A1-K107'ye göre bitirir ve oturum sonu çağrısı dönene kadar yanıt vermez; sonra (her durumda) yeni bir sıfırdan farklı `nonce` seçer (kartta gerçek rastgelelikten, SIL'de A1-K114'e göre), `peer_nonce = SIM_HELLO.nonce` taşıyan `IDA_BOARD_HELLO` gönderir ve `HANDSHAKE`'e geçer; platform güvenli beklemesindeyse (01 §2.8) `SIM_HELLO`'yu yanıtlamaz ve duyurularında `init_rc = NOT_INIT` taşır.
- **A1-K14.** Sim, el sıkışmadan önce kendi yapılandırmasını A1-K104'e göre denetler; yanıt `IDA_BOARD_HELLO`'yu şu sırayla denetler ve ilk tutmayan koşulun sonucunu `IDA_SESSION_ACCEPT` ile gönderir: yapılandırma A1-K104'ü sağlıyor (değilse `REJECT_CONFIG`); `proto_major` ve `proto_minor` kendininkine eşit (`REJECT_PROTO`); `dialect_hash` eşit (`REJECT_DIALECT`); `port_major` ve `port_minor` sim'in derlendiği `port.h` sürümüne eşit (`REJECT_PORT`); `det_schema_major` sim'in derlendiği A8 şema ana sürümüne eşit (`REJECT_DET_SCHEMA`); `ctrl_period_ns` 5 000 000 (`REJECT_RATE`); `param_schema_hash` sim'in beklediğine eşit (`REJECT_PARAM_SCHEMA`); bunlar tutarsa A1-K18'e göre `REBOOT` ya da `OK`.
- **A1-K15.** Sim `OK` sonucunda `sess` olarak sıfırdan, `BOARD_HELLO.prev_sess`'ten ve aynı sim sürecinde kullandığı son 16 oturum kimliğinden farklı bir değer seçer; seçim HIL'de gerçek rastgelelikten, SIL'de A1-K114'e göre tohumdan yapılır.
- **A1-K16.** Kart bir `IDA_SESSION_ACCEPT`'i yalnız `HANDSHAKE` durumunda ve `nonce_sim` yanıtladığı son `SIM_HELLO.nonce`'a, `nonce_board` kendi son `nonce`'una eşitse işler, diğerlerini atar; `OK`'de `RUNNING`'e geçer, oturumu `sess` ile kurar, hemen `IDA_KEEPALIVE(sess, RUNNING)` gönderir ve `plat.link_event = SESSION_START`'ı bundan sonraki ilk çekirdek çağrısında verir (A4-K72); bu çağrı, önceki oturumun A1-K107'deki oturum sonu çağrısından kesin sonradır. `REJECT_*`'te `IDLE`'a döner.
- **A1-K17.** Kart `REBOOT` sonucunu aldığında başka çerçeve göndermeden planlı yeniden başlatma yapar: kartta sistem sıfırlaması (`reset_cause = SOFTWARE`), SIL'de çekirdeğin yeni `boot_count` ve `reset_cause = SIL_REBOOT` ile yeniden kurulması (A4-K11, A4-K86); sim `REBOOT` gönderdikten sonra `T_reboot` içinde `boot_count`'u öncekinden büyük ve `sessions_since_boot = 0` olan bir duyuru bekler, gelmezse durumu A3'e hata olarak bildirir.
- **A1-K18.** Sim bir çalıştırmayı (`run_mode` ≠ `PREP`) yalnız `sessions_since_boot = 0`, `init_rc = OK`, `boot_count_ok = 1` ve `param_blob_crc` A5'ten kurulan bloğun (A4 3.10) CRC-32/ISO-HDLC değerine eşit olan bir karta başlatır; `sessions_since_boot > 0` ise önce `REBOOT` ister; blok uyuşmuyorsa A1-K19'u uygular.
- **A1-K19.** Parametre eşitleme `run_mode = PREP` olan bir oturumda yapılır: sim fizik yürütmeden yalnız zaman çerçevesi gönderir, A5'teki her otopilot parametresi için bir `IDA_CMD_PARAM_SET`, ardından `PARAM_COMMIT` ve `PARAM_SAVE` gönderir (`source = 250`), her birinin sonucunu bekler; hepsi `ACCEPTED`, `IDA_STATUS.param_store = OK` ve `param_blob_crc` beklenen değere eşit olursa `IDA_SESSION_CLOSE(RUN_END)` ve ardından yeni el sıkışmada `REBOOT` ister; aksi halde çalıştırmayı başlatmaz ve A3'e bildirir.
- **A1-K20.** `HANDSHAKE`, `HAZIR`, `KOSUYOR` ve `RUNNING` durumlarında her taraf, son `T_canlı` içinde başka bir çerçeve göndermediyse `IDA_KEEPALIVE` gönderir.
- **A1-K21.** `IDA_SESSION_CLOSE` alan taraf, `sess` güncel oturumunkiyse oturumu hemen bitirir (kart A1-K22, sim A1-K23); sim çalıştırma bittiğinde ve kapanırken, kart platform güvenli beklemesine girerken `IDA_SESSION_CLOSE` gönderir.
- **A1-K22.** Kart oturumu şu durumlardan biri olunca bitirir: `IDA_SESSION_CLOSE` alındı; sim'den `T_bağ` boyunca geçerli çerçeve alınmadı (kartta, duvar saatiyle); yeni `nonce`'lu `IDA_SIM_HELLO` alındı; platform güvenli beklemeye girdi; zorunlu-güvenilir gönderim kuyruğu taştı (A1-K110); olay tablosunun sekiz yuvası doluyken yeni `SAFETY_CLOSE` geldi (A1-K80; `EVENT_OVERFLOW`); bir çağrının emniyet örnekleri kapasiteyi aştı (A1-K124; `PROTOCOL`); SIL'de TCP bağlantısı kapandı. Bitişte kart sırasıyla: (1) gönderim kuyruğunda yazımı başlamamış bütün çerçeveleri, ertelenmiş komutları, açık partiyi ve Ping360 yuvalarını siler (A4-K74; mandalları silmez), onaylanmamış olaylara `SESSION_END` sonucu döner ve yinelemelerini bırakır (A4-K71); (2) A1-K107'ye göre `plat.link_event = SESSION_END` taşıyan oturum sonu çağrısını yapar (daraltıcıdır, A4-K73); (3) bu çağrının `IDA_CALL_TRACE`'ini (`call_kind` = `SESSION_END`) **en iyi çabayla** taşımaya verir; (4) `IDLE`'a döner. Bağlantı kopuksa iz kaybolur; bu kayıp `cagri_izi_eksik`'e sayılmaz (A1-K95) ve kayıttan yeniden kurma bu ize dayanmaz (A1-K127).
- **A1-K23.** Sim oturumu şu durumlardan biri olunca bitirir: `IDA_SESSION_CLOSE` alındı; karttan `T_bağ` boyunca geçerli çerçeve alınmadı (HIL); `KOSUYOR`'da bir `IDA_BOARD_HELLO` alındı (kart sıfırlandı); işletim sistemi aygıtın kalktığını bildirdi; SIL'de bağlantı kapandı ya da A1-K76'nın gözetimi doldu. Bitişte sim itici modelini bir sonraki fizik adımı sınırından itibaren nötr duruma (sıfır itki isteği; A1-K77) alır, iki tüpün süreli iznini kapatır, bütün yuvaları ve bekleyen olayları siler, sonucu gelmemiş komutları `LINK_SESSION_END` ile A3'e bildirir, planlanmamış bitişte çalıştırmayı geçersiz işaretler (A1-K79) ve fiziği durdurmadan A1-K12 ile yeni el sıkışmaya başlar (aygıt kalktıysa portu 1 s aralıkla yeniden açmayı dener).
- **A1-K24.** Kartın ve çekirdeğin hiçbir davranışı `run_mode` değerine bağlı değildir; kart bu alanı yalnız tanı için saklayabilir.
- **A1-K25.** Kart `RUNNING`'de `sess` alanı taşıyan bir sim çerçevesini (`TIME_FRAME`, komutlar, `EVENT_RESULT`, `KEEPALIVE`, `SESSION_CLOSE`) `sess` güncel oturumunki değilse atar (`rx_stale_session`); `sess` alanı taşımayan üyeler (sensör, sonar, algılama) akıştaki yerleriyle oturuma bağlanır (A1-K28).
- **A1-K26.** Kart `RUNNING` dışında aldığı bütün üyeleri, zaman çerçevelerini, komutları ve olay sonuçlarını atar (`rx_outside_session`).
- **A1-K27.** Sim `HAZIR`'da `sess`'i taşıyan ilk kart çerçevesini aldığında `KOSUYOR`'a geçer, `T_hazır` içinde almazsa A1-K12 ile yeni el sıkışmaya başlar; `KOSUYOR`'da `sess` alanı güncel oturumunki olmayan kart çerçevesini atar ve sayar (`yanit_ret_oturum`).

**Yeniden bağlanmada numaralar.** `event_id` açılış sayacından türediği için oturumlar arasında yinelenmez (A4-K61); yeni oturumun ilk TF'si her `batch_seq` ile kabul edilir (A4-K25) ve sim bir çalıştırma içinde `batch_seq`'i kesintisiz sürdürür; `cmd_id` A1-K66'ya göre sürer; MAVLink başlık `seq`'i oturumdan bağımsızdır (A1-K08).

- **A1-K104.** Sim bir çalıştırmayı başlatmadan önce A5'ten gelen zamanlama ve emniyet parametrelerini (`T_bekle`, `valid_for_ns`, `T_bağ`, `apply_by_ns`, `hold_ns`) 3.9'daki izin verilen aralıklarla denetler; biri aralık dışındaysa çalıştırmayı başlatmaz, el sıkışmada `REJECT_CONFIG` gönderir ve A3'e aralık dışı parametreyi ve sağlanmayan eşitsizliğin numarasını (E1–E19) bildirir.
- **A1-K107.** Kart oturum sonunu saptadığında (A1-K22), sürmekte olan bir çekirdek çağrısı varsa onun dönüşünden hemen sonra, yoksa hemen, bekleyen parti olmasa da `tf.present = 0`, örneksiz ve komutsuz, `plat.link_event = SESSION_END` taşıyan tek bir `ida_core_tick` çağrısı yapar; bu çağrı dönene kadar yeni bir `IDA_SIM_HELLO`'yu yanıtlamaz ve yeni oturumun hiçbir girdisini (üye, TF, komut, olay sonucu) çekirdeğe vermez; `link_event` bir çağrıda en çok bir olay taşır ve `SESSION_START` en erken bu çağrıdan sonraki çağrıdadır (A1-K16); SIL'de bu çağrının `now_w`'si son kabul edilen zaman çerçevesinin sim zamanıdır.

**Oturum sonundan yeni oturuma geçiş** (A1-K13, K16, K22, K107; zaman aşağı doğru akar):

```
Kart (platform + çekirdek)                               Sim
──────────────────────────                               ───
RUNNING, oturum s1
  çekirdek çağrısı sürüyor (step n)
  ◄── oturum sonu saptandı: SESSION_CLOSE(s1)            (ya da yeni SIM_HELLO(nonce2),
      ya da T_bağ aşıldı                                  ya da TCP kapandı)
  step n döner (çıktısı s1 oturumu bittiği için
    gönderilmez; olaylarına SESSION_END sonucu, A4-K71)
  tick(link_event = SESSION_END)  ← A1-K107; daraltıcı:
    armed = 0, SAFETY_CLOSE(ALL) üretilir (A4-K73),
    olay oturumsuz olduğu için iletilmez (SESSION_END)
  tick döner ─────────────────────────────────────────── bu ana kadar SIM_HELLO yanıtlanmaz
IDLE: duyuru ya da bekleyen SIM_HELLO(nonce2)'ye yanıt
  BOARD_HELLO(peer_nonce = nonce2, nonce = b2) ────────►
                                                     ◄── SESSION_ACCEPT(s2, nonce2, b2, OK)
RUNNING, oturum s2
  KEEPALIVE(s2) ───────────────────────────────────────► HAZIR → KOSUYOR (A1-K27)
  sonraki çekirdek çağrısı (ızgara tick'i ya da          ◄── s2'nin ilk partisinin üyeleri, TF
    s2'nin ilk partisi): link_event = SESSION_START
```

Hızlı yeniden bağlantı (yeni `SIM_HELLO` oturum sonu tick'inden önce gelir) uygunluk testindedir (7.1, A1-K107 satırı).

### 4.3 Parti ve zaman

- **A1-K28.** Sim `KOSUYOR`'da (duraklatılmamışken) her kontrol periyodu için tam bir `IDA_TIME_FRAME(n)` gönderir ve parti `n`'in üyeleri, aynı oturumda akışta `TF(n−1)`'den (oturumun ilk partisinde `IDA_SESSION_ACCEPT`'ten) sonra ve `TF(n)`'den önce gönderilen üye çerçevelerin tamamıdır; kart bir üyeyi akışta aldığı yere göre bu partiye bağlar ve `TF(n)`'yi alınca partiyi kapatır.
- **A1-K29.** Sim bir oturumda `TF(n).t_sim_ns`'i bir öncekinden tam `P` büyük ve fizik adımının tam katı, `batch_seq`'i bir öncekinden seri aritmetikle tam 1 büyük gönderir; duraklatmadan sonra sayılar boşluksuz sürer.
- **A1-K128.** (Çalıştırma nesli; mimar kararı, W1-uyum ek.) Bir oturum içinde zaman çerçevesinin sim zamanı geriye gitmez ve sıçramaz (A1-K29). Sim zamanının süreksiz olduğu her durum — yeni çalıştırma, sıfırlama, atlama; yani A2/A8 çalıştırma neslinin her değişimi — A1'de oturumun kapanması ve yeni oturumun kurulmasıdır: sim süreksizlikten önce `IDA_SESSION_CLOSE(RUN_END)` gönderir ve oturumu bitirir (A1-K23), ardından A1-K12 ile yeni el sıkışmaya başlar; yeni oturumun ilk TF'si yeni neslin zamanını taşır. Kartta algılama akışlarının zaman ilerleme durumunun ve diğer akış durumlarının sıfırlanması oturum sonu çağrısının (A1-K107; `SESSION_END`) sonucudur, ayrıntısı A4'tedir. Yeni ileti ya da neden kodu yoktur; tel biçimi değişmez.
- **A1-K30.** Sim `n_members`'a penceredeki üye çerçeve sayısını, `tf_flags.IMU`'ya pencerede `IDA_IMU` olup olmadığını yazar; kart bir partide aldığı geçerli üye sayısı `n_members`'tan farklıysa ya da `IMU` bayrağı kurulu olduğu halde IMU almadıysa partiyi eksik sayar (`plat.cnt_batch_incomplete`), aldığı üyeleri yine çekirdeğe verir ve o çağrının `IDA_ACTUATOR`'ında `BATCH_INCOMPLETE` bayrağını kurar.
- **A1-K31.** Kart bir oturumdaki ilk TF'yi her `batch_seq` ile kabul eder; sonrakinde `d = (batch_seq − son kapanan) mod 2³²` ile (A1-K123): `d = 1` olağan kapanıştır; 2 ≤ `d` ≤ 2³¹ − 1 ise aradaki `d − 1` parti sayısını `plat.cnt_batch_skipped`'e ekleyip partiyi kapatır; `d = 0` ise yinelemedir (`DUP`), `d` ≥ 2³¹ ise eskidir (`OLD`): kart penceredeki üyeleri atar, `batch_dup` ya da `plat.cnt_batch_old`'u artırır, çekirdeği çağırmaz ve yanıt göndermez; **ancak** penceredeki CRC'si ve `sess`'i geçerli `IDA_SAFETY` üyesini atmaz: onu A1-K124'e göre, öteki alanlarını değiştirmeden `hdr.flags`'te `VALID` = 0 yaparak sıradaki çekirdek çağrısına verir; çekirdek örneği A4-K33 ile reddeder ve yalnız kısıtlayıcı içeriğini A4-K116'ya göre uygular (A4-K130; U5b). Reddedilen pencerenin örneği gevşetemez. (Önceki metindeki "boşluğun büyüklüğüne üst sınır yoktur" ifadesi kaldırıldı (U8): 2³¹ ve üstü fark eski sayılır.)
- **A1-K123.** (Sıra numaraları; U8, iskelet §3.1.) A1'deki bütün 32 bitlik sıra numaraları ve kimlikler (`batch_seq`, `out_seq`, `cmd_id`, sensör `seq`'i, `event_id`'nin alt 32 biti) şu kuralla karşılaştırılır: `a`, `b`'den yenidir ancak ve ancak `(a − b) mod 2³²` ∈ [1, 2³¹ − 1]; fark 0 ise yinelemedir; fark ≥ 2³¹ ise `a` eskidir (tam yarım uzay farkı, 2³¹, de eski sayılır). "Seri aritmetikle büyük" bu belgede "yeni", "büyük değil" "yineleme ya da eski" demektir. Hiçbir kural boşluğun büyüklüğünü sınırsız saymaz: 2³¹ ya da daha büyük ileri sıçrama eskidir. Oturum (A1-K15, `sess` tekrar kullanılmaz) ve komut kimliği uzlaştırması (A1-K66) aynı kuralı kullanır; A4 aynı tanımı kullanır.
- **A1-K32.** Kart kapanan her partiyi A4-K16, A4-K19 ve A4-K102'ye göre çekirdeğe verir: partide IMU varsa `ida_core_step`, yoksa `ida_core_tick`; `tf.present = 1`, `tf.seq = batch_seq`, `tf.t = t_sim_ns`, `tf.t_rx` = TF'nin alım damgası.
- **A1-K33.** Sim her üyeyi tam bir partiye atar; atama yalnız sim zamanına, üyelerin `t_hazır`'ına ve kodlanmış boylarına bağlıdır ve taşımanın duvar saati zamanlamasına bağlı değildir.
- **A1-K34.** Sim bir partiye en çok şunları koyar: 1 `IDA_IMU`; IMU dışında, `IDA_SAFETY` dahil 16 sensör örneği (A4-K31'in örnek kapasitesi; `IDA_SAFETY` de `in.samples[]`'e girer), bunların en çok 1'i `IDA_SAFETY`; 1 `IDA_PING360_HEAD`; 1 kamera algılama partisi (1 `IDA_DET_BATCH` ve en çok 4 `IDA_DET_RECORD`; A8-K18, A8-K35); 4 `IDA_EVENT_RESULT` (A4-K31); her daraltıcı türden 1 komut (A1-K61); 4 daraltıcı olmayan komut; kritik üyelerin toplam kodlanmış boyunu `B_krit` ile (A1-K113), pencerenin TF dahil toplam boyunu `B_pencere` ile sınırlar; sonar ve toplu komut çerçevelerini A1-K112'nin kredileriyle gönderir.
- **A1-K35.** Sim kritik üyeleri (`IDA_IMU`, diğer sensör mesajları, `IDA_SAFETY`, daraltıcı komutlar, `IDA_EVENT_RESULT`) `t_{n−1} < t_hazır ≤ t_n` olan partiye atar; yalnız A1-K113'ün `B_krit` sınırı aşıldığında bu kuralın öncelik sırasıyla bir sonraki partiye kaydırır, daraltıcı komutlar ayrıca A1-K61'in tür başına kovasını bekleyebilir (≤ 5 ms); komutun `t_hazır`'ı bağlantı emülatörünün bırakma anı (laboratuvar komutunda veriliş anı), olay sonucunun `t_hazır`'ı sonucun karara bağlandığı fizik adımıdır.
- **A1-K36.** Sim toplu üyeleri (daraltıcı olmayan komutlar ve `IDA_PING360_*`) `t_hazır ≤ t_{n−1}` olan bir partiye atar ve pencere `n`'i şu sırayla doldurur: (1) kritik üyeler (A1-K113); (2) A1-K38'e göre `P_k = n` olan algılama partisi; (3) sonar kuyruğu boş değilse sonarın ayrılmış payı: kredisi yeterse sıradaki tam bir Ping360 çerçevesi (A1-K112); (4) daraltıcı olmayan komutlar varış sırasıyla, komut kredisi (`R_c`) ve A1-K34 sınırları içinde; (5) sonar kredisi ve `B_pencere` yettiği sürece sıradaki tam Ping360 çerçeveleri; çerçeveler bölünmez, sığmayan toplu üye sırasını koruyarak sonraki pencereye kalır.
- **A1-K37.** Sim gönderimi başlamamış en çok bir Ping360 tarama satırını kuyrukta tutar; kuyruk doluyken yeni bir satır hazır olursa kuyruktaki gönderimi başlamamış eski satırı atar ve `ping360_seyreltildi`'yi artırır (bant payını aşan satır hızında seyreltilen yalnız Ping360 satırlarıdır).
- **A1-K38.** Sim kamera karesi `k`'nın algılama partisini (başlık ve kayıtları) A8-K57'nin `P_k` partisine, başlık önce ve kayıtlar `batch_index` sırasıyla ardışık olarak koyar ve bir kamera partisini birden çok A1 partisine yaymaz; parti `n`'in oluşturulmaya başlandığı an `M_k` (A8) sim'in `TF(n−1)`'i taşımaya verdiği andır.
- **A1-K39.** HIL kiplerinde sim pencere `n`'in toplu ve atanmış üyelerini `TF(n−1)`'den hemen sonra, kritik üyeleri `t_hazır`'larına ulaşıldıkça yazar, `TF(n)`'yi `W(t_n)`'den önce yazmaz ve ardışık iki TF'nin yazım başlangıçları arasında en az `P/2` duvar süresi bırakır; sim geride kaldığında TF'leri bu alt sınırla yetişerek gönderir.
- **A1-K40.** Kart TF'lerin kendisine `P`'den kısa aralıklarla ulaşabileceğini olağan sayar (USB ve ST-LINK köprüsünün paketlemesi; MP3'te ölçülmedi) ve art arda kapanan partileri A4-K102'ye göre birleştirir; birleştirmede en yeni partinin yanıtı `n_merged`'i taşır ve birleştirilen eski partilere ayrı yanıt gönderilmez (sim için birleştirilen eski partiler `parti_kacirildi_birlesik`'e sayılan kaçırılmış partilerdir; A1-K75). Alıcıda art arda kapanabilecek parti sayısının üst sınırı E21'dedir: herhangi bir `τ` ms'lik aralıkta en çok `1 + ⌊τ / 2,5⌋` (taşıma gönderen aralığını koruduğunda; 5 ms'de 3) ve her koşulda en çok `1 + ⌊τ · 300 / 29⌋` (hat hızı sınırı; 5 ms'de 52).
- **A1-K41.** HIL'de sim duraklatıldığında TF ve üye gönderimi durur, `IDA_KEEPALIVE` sürer, çalıştırma geçersiz işaretlenir (A1-K79); sürdürüldüğünde A1-K29 geçerlidir.
- **A1-K42.** Saat eşlemesi yapılmaz: A1'de iki tarafın duvar saatlerini karşılaştıran ya da birinden ötekine çeviren hiçbir alan ve kural yoktur; ölçüm zamanları sim zamanıdır (iskelet §4), kartın duvar saati yalnız kartın kendi sürelerinde ve tanı alanlarında (`rx_to_tx_us`, `IDA_CALL_TRACE`) kullanılır, sim taşıma gecikmesini yalnız kendi duvar saatiyle (TF yazımından yanıtın alımına) ölçer.
- **A1-K43.** Sim gönderdiği her TF'nin gerçek yazım başlangıcını ve bitişini kendi duvar saatiyle ölçer ve her kayıt kademesinde bellekte kullanır: planlanan anlardan sapmaları A1-K108'e göre ayrı sayaçlarda tutar (Tablo 3.11a) ve zamanlama geçerliliği kararını bunlarla verir. Bu zamanları kayda (A6 `hil/tf_yazim`) yalnız kayıt kademesi `telemetri` ya da `tam` iken yazar; `olaylar` kademesinde bu kayıt tutulmaz ve o kademedeki kayıt zamanlamanın yeniden kurulmasına yetmez (A6-K28 aynı koşulu taşır; W1 doğrulama incelemesi parça 4 Y8).
- **A1-K44.** Kart bir ızgara tick'inin (A4-K18) çıktısını yalnız `thr_valid = 1` ise (A4-K56'ya göre bu durumda `is_safe = 1`'dir) `ASYNC` bayraklı bir `IDA_ACTUATOR` olarak, `batch_seq` = bu oturumda kapanan son partinin `batch_seq`'i (yoksa 0; Tablo 3.7) ile gönderir; ızgara tick'i başına en çok bir eşzamansız çıktı gider.
- **A1-K45.** Kart zaman çerçeveli her çekirdek çağrısı için (dönüş kodu `OK`, `IMU_REJECTED`, `BAD_INPUT`, `NOT_INIT` dahil) tam bir `IDA_ACTUATOR` gönderir ve onun `batch_seq`'ine dönüş kodundan bağımsız olarak çağrıya verdiği `tf.seq`'i yazar (birleştirilmiş girdide en yeni partininki; `out.seq` değil; P2-4); ızgara tick'inde A1-K44 geçerlidir; `DUP` ve `OLD` partiler için çekirdek çağrılmadığından yanıt yoktur (A1-K31).
- **A1-K112.** Sim sonar sınıfı için bir bayt kredisi `C_s` tutar: her pencere başında `C_s ← min(C_s + R_s, C_s,max)`; sıradaki Ping360 çerçevesi yalnız bütün olarak ve yalnız `C_s` ≥ çerçeve boyu iken gönderilir ve gönderilince `C_s` çerçeve boyu kadar azalır; pencerenin `S_ayr` baytlık bölümü sonarın ayrılmış payıdır ve komutlar bu bölümü kullanamaz (A1-K36 adım 3); daraltıcı olmayan komutlar için aynı biçimde `R_c` hızında biriken, en çok `C_c,max` olan bir komut kredisi tutulur.
- **A1-K113.** Sim bir partinin kritik üyelerinin toplam kodlanmış boyunu `B_krit` ile sınırlar; sınırı aşan üyeleri şu sıranın sonundan başlayarak bir sonraki partiye kaydırır (kayan üyeler sonraki partide yeni kritik üyelerden önce gelir ve kendi sıralarını korur): (1) `IDA_TIME_FRAME`; (2) `IDA_IMU`; (3) `IDA_SAFETY`; (4) daraltıcı komutlar; (5) `IDA_EVENT_RESULT` (A1-K111); (6) diğer sensör örnekleri, eski `t_hazır` önce, eşitlikte MAG, BARO, GNSS, DVL, PING1D, BATT, LEAK sırası, sonra `inst`; (1)–(5)'in toplamı en çok 348 B olduğundan (E13) bunlar hiçbir koşulda kaymaz; kayan her üye `kritik_erteleme`'ye sayılır ve en büyük erteleme (parti) kaydedilir.
- **A1-K115.** HIL'de sim bir çerçeveyi işletim sisteminin seri sürücüsüne, sürücünün çıkış kuyruğunda önceki çerçeveden kalan bayt yokken verir (kuyruk ölçülebildiği ölçüde); böylece `W(t_n)`'de kritik kuyruğun önünde taşımada en çok bir tam çerçeve (≤ 258 B) bulunur.

- **A1-K116.** Bant ölçütü (mimar kararı 2026-11-20; iskelet §3): her yönde (a) olağan çalışma profilinde (3.10 "V" satırları, A5 değerleriyle ön denetimde yeniden hesaplanır) periyot başına ortalama yük ≤ 900 B (%60); (b) bu sözleşmenin bağlayıcı sınırlarının izin verdiği en kötü uzun dönem yükü ≤ 1 350 B (%90) ve tek periyotluk aşağı yön yükü ≤ `B_pencere`; (c) çakışmasız partilerde kritik çıktının uçtan uca gecikme bütçesi (E18) sağlanır. Sim ön denetimde (a)'yı sağlamayan yapılandırmayı reddeder; (b) ve (c) E15, E19 ve E18'le sağlanır.
- **A1-K117.** Kart oturum `RUNNING`'de ve TF akarken ızgara tick'ini (A4-K18) yalnız beklenen bir sonraki TF gecikince başlatır: son TF alımından `E = son TF + P` hesaplanır; `[E − T_tick_yasak, E + T_tf_gec)` aralığında ızgara tick'i başlamaz; `E + T_tf_gec`'e kadar TF gelmezse o andan başlayarak ve TF gelene kadar 10 ms aralıkla ızgara tick'i yapılır; oturum yokken ya da TF akmıyorken A4-K18'in 10 ms ızgarası geçerlidir. Bir TF geldiğinde başka bir çekirdek çağrısı (tick ya da süren bir step) sürüyorsa kart o partinin `IDA_ACTUATOR`'ında `CALL_CONFLICT` bayrağını kurar ve `cagri_cakismasi`'nı artırır; sim böyle bir partiyi `cagri_cakismasi`'na sayar ve çalıştırmayı zamanlama bakımından geçersiz işaretler (A1-K79).
### 4.4 Sensör, sonar ve algılama üyeleri

- **A1-K46.** Kart her sensör üyesini Tablo 3.4 ve 3.4b'ye göre A4 örneğine çevirir: `IDA_IMU`'yu `in.imu`'ya, diğer sensörleri A1-K124'ün yerleştirme sırasıyla (emniyet örnekleri önce, sonra diğerleri partideki varış sırasıyla) `in.samples[]`'e koyar, değerleri değiştirmez ve tazelik ya da aralık kararı vermez (A4-K36).
- **A1-K124.** (Emniyet örneklerinin korunması; U5b.) Kart bir çekirdek çağrısının girdisini kurarken o çağrıya düşen bütün **bekleyen emniyet örneklerini** — kapanan partilerin (A4-K102 ile birleştirilenler dahil) ve A1-K31'e göre atılan `DUP`/`OLD` pencerelerinin CRC'si ve `sess`'i geçerli `IDA_SAFETY` üyeleri — varış sırasıyla `in.samples[]`'in başına koyar, diğer sensör örneklerini ardından koyar; bunlardan `DUP`/`OLD` pencerelerinden ve A4-K102 birleştirmesinde en yeni parti dışındaki partilerden gelenleri, öteki alanlarını değiştirmeden `hdr.flags`'te `VALID` = 0 yaparak teslim eder (A4-K130: çekirdek onları reddeder, yalnız kısıtlayıcı içeriklerini uygular; bu A1-K46'nın "değerleri değiştirmez" kuralının A1-K47 gibi tanımlı istisnasıdır); en yeni partinin örneği olduğu gibi gider; A4-K102'nin kapasite düşürmesi yalnız emniyet dışı örneklere uygulanır ve hiçbir emniyet örneği kapasite nedeniyle atılmaz. E20'ye göre bir çağrıya düşen emniyet örneği sayısı örnek kapasitesini (16) aşmaz; aşacak olursa (E20'nin varsayımı bozulmuştur: önceki çağrı sürerken 16'dan çok emniyet taşıyan pencere kapanmıştır) kart hiçbir emniyet örneğini atmadan bunu bağlantı hatası sayar, `IDA_SESSION_CLOSE(PROTOCOL)` göndermeyi dener ve oturumu bitirir (A1-K22): oturum sonu bütün izinleri ve silahlandırmayı kaldırır (A4-K73) ve yeni oturumda kabul edilmiş emniyet örneği olmadığından çekirdek emniyet girdilerini kapalı sayar (A4-K116), yani bekleyen örneklerin kısıtlayıcı içeriğinden daha kısıtlayıcıdır. Oturum sonunda (A1-K22) açık pencerenin üyeleri, emniyet örnekleri dahil, aynı gerekçeyle silinir. **Düşen gevşetmenin karta ulaşması:** birleştirilen eski bir partideki gevşeten değer (ör. `estop` 1→0) bu kuralla kabul edilmez; sim `IDA_SAFETY`'yi değişimde ve ayrıca `T_emniyet` = 100 ms aralıkla yeniden gönderdiğinden (A1-K56) güncel gevşek değer en geç bir `T_emniyet` sonra yeni bir partide `VALID` = 1 ile gelir; ek bir kural gerekmez.
- **A1-K47.** Kart bir alanı A4 tipine çeviremediğinde (Tablo 3.4b'de belirtilen aralık dışı numaralandırma) örneği atmaz, `hdr.flags`'ten `VALID` bitini silerek çekirdeğe verir (çekirdek reddeder ve sayar; A4-K33).
- **A1-K48.** Kart her çerçevenin alım damgasını, çerçevenin son baytı belleğe yazıldıktan sonraki ilk alım olayının duvar saati olarak koyar; alım olayları en az her `0xFD` baytı, hat boşluğu ve DMA yarım/tam kesmeleridir; bir çerçeveye sonrasındaki bir olay bağlanamıyorsa çerçevenin son baytından önceki en son olayın damgası kullanılır ve `stamp_coarse` artar; damgalar akış sırasında azalmaz ve kart bunları A4'teki `hdr.t_rx`, `tf.t_rx`, `det_t_rx[]` ve `det_batch_t_rx[]` alanlarına koyar. **Sapma sınırı (P2-20):** çerçeveyi hat boşluğu ya da bir `0xFD` baytı izlediğinde (sim yalnız bütün çerçeve yazdığı için hata enjeksiyonu dışında her zaman böyledir) damga gerçek alım anından (son baytın hattan alındığı an) en çok 1 bayt süresi + kesme gecikmesi kadar sonradır (≤ 0,1 ms; kesme gecikmesinin ≤ 96 µs olması kart yazılımının yükümlülüğüdür, W3'te ölçülür); kaba damga (`stamp_coarse`) gerçek alım anından en çok bir çerçeve süresi kadar öncedir (≤ 0,86 ms) ve hiçbir zaman sonra değildir; böylece `|t_rx − gerçek alım|` ≤ `Δ_rx` = 1 ms (3.1). Bayt bozma enjeksiyonunda çerçeveyi izleyen bozuk baytlar olayı geciktirebilir; bu çerçeveler için sınır verilmez. A4'ün tazelik süreleri bu damgadan sayılır ve bu sınıra atıf verir.
- **A1-K49.** Sim bir Ping360 tarama satırını `n_frag = ⌈n_samples / 240⌉` `IDA_PING360_DATA` ve ardından bir `IDA_PING360_HEAD` olarak, aynı oturumda ve akışta bu sırayla gönderir; DATA çerçeveleri birden çok pencereye yayılabilir, HEAD satırın son çerçevesidir.
- **A1-K50.** Ping360 satırının teslim edildiği parti, HEAD'inin üyesi olduğu partidir: kart o partiyi kapatırken aynı (`inst`, `seq`) için `frag_idx` 0 … `n_frag − 1` parçalarının hepsini almışsa satırı `in.ping360` ile çekirdeğe verir, almamışsa satırı atar ve `ping360_incomplete`'i artırır.
- **A1-K51.** Kart Ping360 parçalarının uzunluğunu tel yüküne değil, MAVLink'in sıfırla tamamlamasından (A1-K05) sonraki mantıksal alanlara uygular (1 baytlık tel yükü taşıyan sıfır parça geçerlidir; Ek B'deki ek vaka); en az iki yeniden birleştirme yuvası tutar; `frag_idx ≥ n_frag` (HEAD gelmeden ≥ 5) olan DATA'yı atar (`ping360_bad`); aynı parçanın tamamlanmış alanları bayt bayt aynı yinelemesini yok sayar, farklı yinelemesinde satırı atar (`ping360_bad`); ilk parçasından sonra `T_yetim` parti kapanışı boyunca HEAD'i gelmeyen yuvayı boşaltır (`ping360_orphan`); iki yuva da doluyken yeni bir satırın parçası gelirse en eski yuvayı boşaltır (`ping360_orphan`).
- **A1-K52.** Menzil kutusu `i`'nin (0 … `n_samples − 1`) nominal yansıma anı `t_i = t_meas + (2i + 1) · sample_period · 25 ns / 4` ve nominal eğik menzili `r_i = c · (2i + 1) · sample_period · 25 ns / 4`'tür (`c` çekirdeğin ses hızı parametresi); bu, kutu merkezine giden doğrudan yolun zamanıdır, darbe süresi hesaba katılmaz ve çok yollu enerjiyi ayırmaz (iskelet §3.1).
- **A1-K53.** Ping360 satırının `t_meas_ns`'i gönderim anıdır ve teslimdeki gecikme (bir ya da daha çok periyot) bu değeri değiştirmez; sim satırın teslim edildiği partiyi A6'ya yazar.
- **A1-K54.** `IDA_PING1D.t_meas_ns` yansıma anıdır ve gönderim anı `t_meas − tx_offset_ns`'tir; köprü ve kart bu alanlar arasında dönüşüm yapmaz.
- **A1-K55.** DVL'nin rapor anı A1'de taşınmaz; `IDA_DVL.t_meas_ns` yansıma anıdır.
- **A1-K56.** Sim `IDA_SAFETY`'yi `T_emniyet` aralıkla ve sanal emniyet girdisi, tüp durumu, izin durumu ya da `safety_rev` değiştiğinde gönderir; bir partiye en çok bir tane koyar ve bu üye sim'de hiçbir koşulda ertelenmez ve kartta hiçbir koşulda atılmaz (A1-K31, K124); aynı pencerede bir kapanma (`estop` 0→1 ya da bir tüpte `PLUG_IN` veya `PIN_OUT` 1→0; A4-K111) ve ardından açılma olduysa o partide kapanmış durumu, açılmayı sonraki partide gönderir; `t_meas_ns` örnekleme ya da değişim anıdır, `seq` her mesajda bir artar, `PERMIT_OPEN` biti sim'deki iznin ve `safety_rev` emniyet revizyonunun `t_meas` anındaki değeridir (A1-K89, K106).
- **A1-K57.** Kart `IDA_DET_BATCH` ve `IDA_DET_RECORD`'u alanları değiştirmeden `in.det_batch[]` ve `in.det[]`'e varış sırasıyla koyar ve doğrulamaz (doğrulama A8-K29'a göre çekirdeğindir); partideki algılama öğesi A4 kapasitesini aşarsa A4-K102'nin düşürme kuralı uygulanır.
- **A1-K58.** Kamera kapalıyken sim A8-K64'ün `NO_FRAME` başlığını `t_meas_ns`'i konduğu partinin kapanış anı olacak biçimde o partiye koyar.

### 4.5 Komutlar

- **A1-K59.** Panelden gelen komutlar sim'de bağlantı emülatöründen (A5 profili; sim zamanı) geçer ve emülatörün bırakma anına göre bir partiye atanır; sim laboratuvarının komutları (A1-K19) emülatörden geçmez.
- **A1-K60.** Her komutun sınıfı (genişletir, daraltır, nötr) Tablo 3.6'dadır ve `protocol/ida.xml`'deki ilgili numaralandırma girdisinin açıklamasında `[sinif: genisletir|daraltir|notr|kosullu]` biçiminde de bulunur; "koşullu" girdinin sınıfı Tablo 3.6'daki koşuldan çıkar.
- **A1-K61.** Sim aynı partiye aynı daraltıcı türden (DISARM, daraltıcı SET_MODE, FC_DISARM, FC_ABORT) en çok bir komut koyar ve her tür için A1-K119'un kovasına uyar (duvar saatinde 5 ms'de en çok bir); arada aynı türden birden çok gelirse en sonuncusunu gönderir, öncekilerin her birini kendisi `ACCEPTED` / `IDAW_REASON_COALESCED` sonucuyla A3'e bildirir ve sayar; bekleyen son daraltıcı kovanın izin verdiği ilk anda (en geç 5 ms içinde) gider.
- **A1-K62.** Daraltıcı komutlar kritik, diğer komutlar toplu sınıftadır (A1-K35, A1-K36).
- **A1-K63.** Kart `sess`'i geçerli bir daraltıcı komutu ayrıştırdığı anda A4-K46'ya göre mandala alır ve tampon doluluğundan bağımsız olarak hiçbir koşulda atmaz; daraltıcı olmayan komutu partisinin çağrısının `cmds[]`'ine varış sırasıyla koyar ve kapasite aşımında A4-K105'i uygular. **Bekleyen genişleticilerin düşmesi (U4b; iskelet §3.2):** kart bir daraltıcı komutu mandala aldığı anda A4-K105'in erteleme tamponunda bekleyen bütün **genişletici** komutları (Tablo 3.6'daki "genişletir" sınıfı) atar ve her biri için `REJECTED` / `IDAW_REASON_NARROWED` sonucunu kendisi gönderir (A1-K67; A4-K105); nötr komutlar etkilenmez. Açık ya da bekleyen (henüz çağrılmamış) partideki genişleticiler mandalla aynı çağrıya girer ve A4-K45 ile `NARROWED` alır. Böylece kendisinden önce gelmiş ama henüz uygulanmamış bir genişletici, sonradan kabul edilen daraltıcıdan sonra uygulanamaz. Tel biçimi değişmez.
- **A1-K64.** Kart Tablo 3.6'daki bir dönüşüm hatasında (aralık dışı tür ya da numaralandırma; `IDA_CMD_SIMPLE`'da `SET_MODE` hedefi 2–7 dışında (P2-2); `IDA_CMD_FC`'de `op` kapalı listede değil, `tube` işlemin izin verdiği değer değil (`FC_DISARM`/`FC_ABORT`'ta 255, diğerlerinde 0–1; P2-3) ya da `arg0`/`arg1` ≠ 0; `IDA_CMD_MISSION_ITEM`'da A1-K122) komutu çekirdeğe ve mandala vermez ve `REJECTED` / `IDAW_REASON_LINK_BAD_FIELD` sonucunu kendisi gönderir.
- **A1-K65.** Sim her yeni komuta, aynı sim çalıştırmasında ve kartın aynı açılışında başka hiçbir komutun taşımadığı bir `cmd_id` verir; aynı komutun emülatör ya da hata enjeksiyonu kaynaklı yinelemeleri aynı `cmd_id`'yi taşır.
- **A1-K66.** Sim her oturumda ilk yeni komutun kimliğini kartın son kimliğine göre seçer: `L = BOARD_HELLO.last_cmd_id`, `c` sim'in sıradaki kimliği; `(c − L) mod 2³²` ∈ [1, 2³¹ − 1] ise (`c`, `L`'den A1-K123 anlamında yeni) `x = c`, değilse `x = (L + 1) mod 2³²` ve bu 0 ise `x = 1`; böylece seçilen kimlik her durumda `L` karşısındaki pencerededir (`(x − L) mod 2³²` ∈ [1, 2³¹ − 1]: `c` seçilince tanım gereği, `L + 1` seçilince fark 1, `L = 0xFFFFFFFF` iken `x = 1` ve fark 2). Sonraki her yeni komutta sim kimliği bir artırır ve 0'ı atlar (0 yerine 1). Kart `last_cmd_id`'yi çekirdeğe verdiği son komutun (mandal dahil) `cmd_id`'si olarak tutar; A4 aynı karşılaştırmayı kullanır (U8). (İlk metindeki "`c`, `h = L + 1`'den yeniyse `c`" seçimi yanlıştı: `L = 10`, `c = 2³¹ + 10` örneğinde `c − h = 2³¹ − 1` iken `c − L = 2³¹`, yani `c` eskiydi; W1 doğrulama incelemesi parça 1 bulgu 2.)
- **A1-K67.** Kart `RUNNING`'de kabul ettiği her komut çerçevesi için en az bir `IDA_CMD_RESULT`'ı zorunlu-güvenilir sınıfta (A1-K110) gönderir: çekirdeğin sonucu (A4-K43; aynı çağrının çıktısından), mandalda üzerine yazılan önceki daraltıcı için platformun `ACCEPTED` / `IDAW_REASON_COALESCED` sonucu (üzerine yazıldığı anda), A4-K105'te atılan komut için `REJECTED` / `IDAW_REASON_LINK_DROPPED`, dönüşüm hatasında A1-K64; aynı `cmd_id`'li yinelemeye çekirdeğin `DUPLICATE` sonucu ayrıca gider; sim bir `cmd_id` için A1-K109'dan geçen ilk sonucu A3'e iletir, sonrakileri yalnız kaydeder; sonuç ancak oturum biterse (A1-K23) gelmeyebilir.
- **A1-K68.** `PARAM_SAVE` kabul edildiğinde kart bloğu (A4-K82) kalıcı belleğe yazar, `IDA_CMD_RESULT`'ta `save_gen`, `save_len` ve bloğun CRC-32/ISO-HDLC değerini (`save_crc`) bildirir ve kaydın ilerleyişini `IDA_STATUS.param_store`, `param_fail`, `param_gen`, `param_blob_crc` ile bildirir; blok A1'de taşınmaz.
- **A1-K69.** Sim bir görev yüklemesinin `MISSION_BEGIN`, `MISSION_ITEM` ve `MISSION_END` komutlarını bu sırayla toplu sınıfta gönderir; komutlar birden çok partiye yayılabilir ve atomikliği çekirdek sağlar (A4-K51).
- **A1-K70.** Panel yer istasyonu canlılığını 1 Hz `IDA_CMD_SIMPLE(HEARTBEAT)` ile bildirir; bu komut emülatörden geçer ve kaybı çekirdeğin yer istasyonu kaybı kuralını işletir (A4-K52).

- **A1-K119.** Sim komut gönderimini duvar saatinde, partilerden bağımsız kovalarla sınırlar: daraltıcı olmayan komut çerçeveleri için patlama 8, hız 5 ms'de 2; her daraltıcı tür için patlama 1, hız 5 ms'de 1 (A1-K61); emülatör ya da hata enjeksiyonu kaynaklı yinelemeler de çerçeve olarak sayılır; kova boşken daraltıcı olmayan komut bekler (A1-K36'nın komut kredisine ek olarak).
- **A1-K129.** (Yolda bekleyen genişletici, sim tarafı; iskelet §3.2; W1 doğrulama incelemesi parça 1 bulgu 1.) Sim, bağlantı emülatöründen çıkıp A1'e verilen ve gönderim kuyruğunda (toplu komut kuyruğu, komut kredisi ve kovası; A1-K36, K112, K119) bekleyen her genişletici komutu (Tablo 3.6'daki "genişletir" sınıfı) kabul edildiği emniyet revizyonuyla birlikte tutar ve **tele yazmadan hemen önce** yeniden denetler: revizyon o günden beri değiştiyse (araya kabul edilen her daraltıcı komut ve her kısıtlama revizyonu artırır; A1-K106) komutu göndermez, kuyruktan çıkarır, A3'e A3-K101/K102'nin `bayat_yetki` sonucuyla bildirir ve sayar (`komut_bayat_yetki`). Böylece yolda bekleyen bir genişletici, kendisinden sonra kabul edilmiş bir daraltıcıdan sonra uygulanamaz; bağ zincirin üç bekleme noktasında aynıdır: (1) bağlantı emülatöründen çıkış (A3-K101; sahibi A3), (2) sim'in A1 gönderim kuyruğu (bu kural), (3) kartın erteleme tamponu (A1-K63). Tele yazılmış bir genişletici ile sonraki daraltıcı aynı çağrıya düşerse A4-K45, ayrı çağrılara düşerse uygulama sırası kabul sırasıyla aynıdır (genişletici daraltıcıdan önce kabul edilmiştir). Tel biçimi değişmez.
- **A1-K122.** Kart `IDA_CMD_MISSION_ITEM`'ı Tablo 3.6a'ya göre denetler: `item_type` 1–7 değilse (eski `TARGET_ALIGN` = 8 dahil), `frame` türün izin verdiği değerlerden değilse, kullanılmayan konum alanları 0 değilse, `depth` ya da bir `p[i]` türün aralığı dışında ya da sonlu değilse, "0" yazan bir `p[i]` tam 0 değilse komutu A1-K64'e göre reddeder.
### 4.6 Sürekli komut yanıtı ve sim'in kabul kuralı

- **A1-K71.** Kart `IDA_ACTUATOR`'ı Tablo 3.7'ye göre doldurur; `THR_VALID = 0` iken `pwm_us[]` ve `valid_for_ns` 0'dır.
- **A1-K72.** Sim A1-K109'dan geçen, `ASYNC` bayraksız bir `IDA_ACTUATOR`'ı şu sırayla değerlendirir: yuvası doluysa reddeder (`yanit_ret_yinelenen`); uygulama anı `a_n`'deki fizik adımı hesaplanmışsa reddeder (`yanit_ret_gec`); değilse yuvasına koyar. Reddedilen yanıt hiçbir partinin yerine uygulanmaz; A1-K109'da reddedilen yanıt o parti için hiç gelmemiş sayılır.
- **A1-K73.** Kart bir çağrının ilk kabul kapısından geçen yeni olaylarını (A4-K67) o çağrının `IDA_ACTUATOR`'ından önce, `call_kind`'ı çağrının türüyle doldurarak gönderir ve `n_events`'e bunların sayısını yazar. Olayın `batch_seq`'i ve `t_decide_ns`'i çekirdeğin olaya verdiği `seq` ve `t_decide`'dır (A4-K57): `core_rc` ∈ {`OK`, `IMU_REJECTED`} olan zaman çerçeveli çağrıda bunlar çağrının `tf.seq`'i ve `tf.t`'sidir ve yanıtın `batch_seq`'iyle aynıdır; hata dönüşünde (`BAD_INPUT`; `NOT_INIT`'te olay yoktur) ve A4-K113 (b2) ile bekletilip bir hata dönüşünde çıkan olayda "son geçerli" zaman çerçevesininkidir (A4 4.14; yoksa 0 ve 0), yanıtın `batch_seq`'inden farklı olabilir. Sim yuva `n`'i şöyle tam sayar: `core_rc` ∈ {`OK`, `IMU_REJECTED`} ise `IDA_ACTUATOR(n)` ve A1-K109'dan geçmiş, `batch_seq`'i `n`, `call_kind`'ı `TICK_GRID` olmayan `n_events` farklı `event_id` alındığında; hata dönüşünde (`THR_VALID = 0`, komut değişmez) `IDA_ACTUATOR(n)` A1-K109'dan geçtiğinde, olayları beklemeden; hata dönüşünün olayları yuvaya bağlanmaz, kendi `batch_seq`'leriyle A1-K83, K84 ve K85'e göre uygulanır (W1 doğrulama incelemesi parça 2 Y1).
- **A1-K74.** Sim `a_n`'de başlayan fizik adımının başında yuva `n`'i uygular: `THR_VALID = 1` ise `pwm_us[]` yürürlükteki sürekli komut olur ve kullanım süresi `a_n + valid_for_ns`'te dolar; `THR_VALID = 0` ise yuva yanıtlanmış sayılır ve yürürlükteki komut değişmez.
- **A1-K75.** `a_n`'de yuva tam değilse: bariyer kipinde sim, sim zamanını ilerletmeden yalnız o yuvanın tamamlanmasını en çok `T_bekle` (duvar) bekler, bu sırada gelen eski numaralı yanıtları reddeder ve sonraki numaralıları yuvalarına koyar, yuva tamamlanırsa onu `a_n`'de uygular; `T_bekle` aşılırsa ya da HIL 1x'te parti "kaçırıldı" olur (`parti_kacirildi`; yanıtın `n_merged`'i bu partiyi kapsıyorsa `parti_kacirildi_birlesik`), komut değişmez, yuvanın sonradan gelen yanıtı A1-K72'ye göre reddedilir; bekleme süresi `gercek_zaman_kaymasi`'na eklenir ve bariyerde kurtarılan yuva da A1-K108'e göre son tarih kaçırması sayılır.
- **A1-K76.** SIL'de sim `a_n`'de yuvanın tamamlanmasını sim zamanını ilerletmeden süre sınırı olmaksızın bekler; yalnız `T_SIL` (duvar) boyunca yanıt gelmezse oturumu bitirir ve A3'e hata bildirir.
- **A1-K77.** Yürürlükteki komutun kullanım süresi dolduğunda ve yeni bir komut uygulanmadıysa sim itici modelinin girdisini nötr duruma (sekiz itici için sıfır itki isteği) alır; nötr durum ESC modelinin "sinyal kaybı" davranışıyla (ör. son değeri tutma) temsil edilmez.
- **A1-K78.** Sim sürekli çıktıları (olağan yanıtlar ve `ASYNC`) yalnız `out_seq` sırasıyla uygular (A1-K105): `out_seq`'i son uygulanan sürekli çıktınınkinden seri aritmetikle büyük olmayan çıktıyı uygulamaz (`yanit_ret_eski_uretim`); A1-K109'dan geçen eşzamansız güvenli çıktıyı (`ASYNC`, `THR_VALID = 1`, `IS_SAFE = 1`) alımından sonraki ilk fizik adımı sınırında uygular ve kullanım süresini o andan sayar; bir eşzamansız güvenli çıktı, `out_seq`'i kendisininkinden küçük olan, henüz uygulanmamış bütün sürekli çıktıları (yuvalardaki olağan yanıtlar dahil) iptal eder (`yanit_iptal_esz`); `out_seq`'i büyük olan olağan yanıt kendi yuvasında A1-K72–K75'e göre uygulanır; aynı fizik adımı sınırına birden çok sürekli çıktı düşerse hepsi `out_seq` sırasıyla uygulanır ve sonuncusu yürürlükte kalır.
- **A1-K79.** Sim bir çalıştırmayı şunlardan biri olduğunda zamanlama bakımından geçersiz işaretler ve nedenleri A3'e bildirir, A6'ya yazar: kaçırılan parti (birleştirilen dahil); A1-K108'e göre kaçırılan son tarih (bariyerde kurtarılanlar dahil) ya da sim gecikmesi; çağrı çakışmalı parti (A1-K117); sıfırdan büyük bariyer beklemesi; geç uygulanan olay (A1-K83); uygulanan eşzamansız güvenli çıktı; reddedilen geç yanıt; pencere planının aşılması (`pencere_tasma`); planlanmamış oturum bitişi ya da kart sıfırlanması; HIL'de duraklatma. Zamanlama kabulü yalnız bu sayaçların hepsi sıfır olan çalıştırmalarla yapılır (iskelet §4). Gönderilemeyen `IDA_CALL_TRACE`'in periyodu ayrıca "zamanlaması doğrulanmamış" sayılır (A1-K95).
- **A1-K105.** Kart, açılıştan (SIL'de init'ten) başlayan ve gönderdiği her `IDA_ACTUATOR`'da (olağan ya da `ASYNC`) bir artan `out_seq`'i tutar ve çerçeveye yazar; `out_seq` oturumlar arasında sıfırlanmaz ve akıştaki sıra üretim sırasıyla aynıdır; sim bir oturumda `out_seq`'i bir öncekinden seri aritmetikle büyük olmayan yanıtı A1-K109 (V5) ile reddeder.
- **A1-K108.** HIL kiplerinde sim her yuva `n` için planlanan duvar son tarihini `D_n = W(a_n) − C_sim` olarak tutar ve yuva sonucundan ayrı olarak şunları sayar: yuvayı tamamlayan son çerçeve `D_n`'den sonra alındıysa (bariyerde bekleyip kurtarılan ve kaçırılan yuvalar dahil) `son_tarih_kacirildi`; sim `a_n`'de başlayan fizik adımını `W(a_n)`'den sonra hesaplamaya başladıysa `sim_gecikmesi`; ikisi de çalıştırmayı zamanlama bakımından geçersiz kılar (A1-K79). Bariyer beklemesinden sonra sim `W`'yi bekleme süresi kadar kaydırır ve kaydırmayı `gercek_zaman_kaymasi`'na yazar.
- **A1-K109.** Sim CRC'si doğru her kart çerçevesini Tablo 4.6'daki denetimlerden sırayla geçirir; ilk tutmayan denetim mesajın tek ret nedenidir; reddedilen mesaj bütünüyle reddedilir ve hiçbir etkisi uygulanmaz (yuva doldurmaz, olay ya da sonuç kaydı açmaz, olay sonucu döndürmez, sayaç dışında durum değiştirmez); ret nedeni tablodaki sayaca sayılır ve ham çerçeve A6'ya yazılır; reddedilen `IDA_ACTUATOR` o parti için hiç gelmemiş sayılır (A1-K75), reddedilen `IDA_EVENT` hiç alınmamış sayılır.

**Tablo 4.6 — Kart çıktılarının alan doğrulaması (sim).** Denetimler satır sırasıyla yapılır.

| Sıra | Denetim | Mesaj | Sayaç |
|---|---|---|---|
| V1 | `sess` güncel oturumunki | `sess` taşıyan bütün kart mesajları | `yanit_ret_oturum` |
| V2 | Numaralandırma ve aralık (aşağıda) | Hepsi | `alan_ret_<mesaj>` |
| V3 | Bağ: bu oturumda gönderilmiş parti ya da komut (aşağıda) | `ACTUATOR`, `EVENT`, `CMD_RESULT` | `bag_ret_<mesaj>` |
| V4 | Alanlar arası tutarlılık (aşağıda; Tablo 4.6a, 4.6b) | `ACTUATOR`, `EVENT` | `alan_ret_<mesaj>` |
| V5 | `out_seq` akışta kesin artan (A1-K105) | `ACTUATOR` | `yanit_ret_eski_uretim` |

| Mesaj | V2 | V3 | V4 |
|---|---|---|---|
| `IDA_ACTUATOR` | `act_flags`'te 0x03FF dışı bit yok; `fs_level` ≤ 5; `core_rc` ∈ {0, 3, 4, 8}; `n_events` ≤ 4; `THR_VALID = 1` ise `valid_for_ns` A5'teki değere eşit ve her `pwm_us` A5'in ESC alt–üst aralığında; `THR_VALID = 0` ise `valid_for_ns` = 0 ve bütün `pwm_us` = 0 | `ASYNC` yoksa `batch_seq` bu oturumda gönderilmiş bir TF'nin; `ASYNC` varsa `batch_seq` 0 ya da bu oturumda gönderilmiş bir TF'nin | `ASYNC` ⇒ `THR_VALID`, `IS_SAFE`, `SRC_TICK`; `THR_VALID ∧ IS_SAFE` ⇒ her `pwm_us` A5'in nötr değeri |
| `IDA_EVENT` | `kind` 1–3; `tube` 0–1, ya da `kind = SAFETY_CLOSE` iken 255; `apply_by_ns` 3.9 aralığında; `SAFETY_OPEN`'da `hold_ns` 3.9 aralığında, diğerlerinde 0; `call_kind` 1–3 | `batch_seq` bu oturumda gönderilmiş bir TF'nin ve `t_decide_ns` o TF'nin `t_sim_ns`'ine eşit; ya da `kind = SAFETY_CLOSE` iken `batch_seq` = 0 ve `t_decide_ns` = 0 (çekirdeğin "son geçerli" zaman çerçevesi yok; A4 4.14, A1-K73) | `kind` ∈ {SAFETY_OPEN, FIRE} ⇒ `safety_rev` ≠ 0 |
| `IDA_CMD_RESULT` | `kind` 1–3; `ack` 1–5; `reason` 0–15 ya da 128–131 (A4-K120; 16–127 ve 132 üstü ayrılmış, ret); `kind = PARAM_VALUE` ise `param_type` 1–3 | `cmd_id` bu oturumda gönderilmiş bir komutun | — |
| `IDA_STATUS` | `mode` ≤ 7; `fs_level` ≤ 5; `link_state` ≤ 2; `health`'te tanımsız bit yok (8 bitin hepsi tanımlı); `armed`, `param_ok`, `events_ok` ≤ 1 | — | — |
| `IDA_FC_STATUS` | `state` ≤ 5 (`IDAW_FC_STATE`; A4 §3.16); `inhibit_mask`'te bit 18–31 0; `last_event_result` 0 ya da `IDAW_EVENT_RESULT_CODE` değeri | — | — |
| `IDA_NAV` | `est_level` ≤ 4 | — | — |
| `IDA_LOG` | `code` 1–23 (A4 §3.15); `severity` ≤ 7 | — | — |
| `IDA_CALL_TRACE` | `call_kind` 1–4; `n_rx` ≤ 28; `plat_flags`'te 0x3F dışı bit yok; `core_rc` ≤ 8 | — | — |
| `IDA_KEEPALIVE`, `IDA_SESSION_CLOSE`, `IDA_BOARD_HELLO` | `state` ≤ 2; `reason` 1–7; `role` 1–2 ve `nonce` ≠ 0 (diğer alanlar A1-K14'te) | — | — |
| Diğerleri (`IDA_CORE_COUNTERS`, `IDA_LINK_STATS`, `IDA_FAULT_RECORD`, `IDA_DET_RECORD`) | Aralık denetimi yok (sayaç ve tanı); `IDA_DET_RECORD` değerlendirmeye ayrıca A8-K28'in sim satırlarıyla girer | — | — |

**Tablo 4.6a — V4: `IDA_ACTUATOR`'da dönüş kodu × çıktı bayrakları** (kaynak: A4 §4.14 düzeltme turu 2; `DUP_SEQ` ve `OLD_SEQ` A1-K31 gereği hiç gönderilmez):

| `core_rc` | `SRC_TICK` | `THR_VALID` | `IS_SAFE` | `valid_for_ns`, `pwm_us` | `ASYNC` | `n_events` |
|---|---|---|---|---|---|---|
| `OK` (0), step | 0 | 1 | 0 ya da 1 (1 ise her `pwm_us` nötr) | A5 değeri; ESC aralığı | 0 | 0–4 |
| `OK` (0), tick | 1 | 0 ya da 1; 1 ise `IS_SAFE = 1` | — | `THR_VALID = 1` ise A5 değeri ve nötr; değilse 0 | 0 ya da 1 (1 ise `THR_VALID = 1`) | 0–4, yalnız `SAFETY_CLOSE` |
| `IMU_REJECTED` (8) | 0 | 0 ya da 1; 1 ise `IS_SAFE = 1` | — | `THR_VALID = 1` ise A5 değeri ve nötr; değilse 0 | 0 | 0–4, yalnız `SAFETY_CLOSE` |
| `BAD_INPUT` (3) | 0 ya da 1 | 0 | bilgi | 0 | 0 | 0–4, yalnız `SAFETY_CLOSE` |
| `NOT_INIT` (4) | 0 ya da 1 | 0 | 1 | 0 | 0 | 0 |
| `DUP_SEQ` (1), `OLD_SEQ` (2), diğerleri | — | — | — | — | — | ret |

**Tablo 4.6b — V4: `IDA_EVENT`'te olay türü × çağrı türü** (kaynak: A4-K64): `SAFETY_CLOSE` her `call_kind` ile (1–3) geçerlidir; `SAFETY_OPEN` ve `FIRE` yalnız `call_kind = STEP` ile geçerlidir; `TICK_TF` ya da `TICK_GRID` ile gelen `SAFETY_OPEN`/`FIRE` çelişkilidir ve reddedilir. Olayın aynı `batch_seq`'li yanıtla tutarlılığı (`core_rc = OK`, `FIRE` için `ARMED = 1`, `IS_SAFE = 0`) A1-K118'dedir; tutarsız yanıtın yaptırımı A1-K109'un tek ret ve etkisizlik yoludur. Ayrıca, `core_rc` ∈ {`OK`, `IMU_REJECTED`} olan bir yanıtın `n_events`'ine sayılan (aynı `batch_seq`'li; A1-K73) olaylardan herhangi biri bu tablolara aykırıysa yanıt ve o olaylar birlikte reddedilir; hata dönüşünün olayları yanıta bağlanmaz ve tek başlarına değerlendirilir.

`event_id`'nin üst 32 bitinin oturumun `boot_count`'una eşit olmaması alan hatası değildir; A1-K82'ye göre `REJ_SESSION` sonucudur.

- **A1-K118.** Sim izin açan bir olayı (`SAFETY_OPEN`, `FIRE`; `call_kind = STEP`) ancak aynı `batch_seq`'in `ASYNC` bayraksız `IDA_ACTUATOR`'ı A1-K109'dan (Tablo 4.6b dahil) geçmiş ve olayla tutarlıysa uygular: `core_rc = OK`, `SRC_TICK = 0`; `FIRE` için ayrıca `ARMED = 1` ve `IS_SAFE = 0` (A4-K64). Yanıt hiç gelmediyse, reddedildiyse ya da tutarsızsa olay kalıcı `REJ_SAFE` alır; tutarsızlık durumunda yanıt da reddedilir ve `alan_ret_ACTUATOR`'a sayılır.
### 4.7 Tek seferlik olaylar

- **A1-K80.** Kartın olay tablosu `N_olay` = 8 yuvadır ve ilk kabul kapısı A4-K67 ile aynıdır (U12): izin açan olaylar (`SAFETY_OPEN`, `FIRE`) en çok **yedi** yuva kullanır, bir yuva `SAFETY_CLOSE`'a ayrılmıştır; izin açan bir olay, yedi yuvanın hepsi doluysa ya da A1-K120'nin izin açan olay kovası boşsa `DROPPED_NO_SLOT` alır (`DROPPED_NO_SLOT`'un yalnız bu iki nedeni vardır); `SAFETY_CLOSE` yer ya da kova yokluğundan hiçbir zaman düşürülmez: boş yuvalardan birini (gerekirse ayrılmış yuvayı) kullanır, kova boşsa A1-K120'ye göre tabloda bekler; **sekiz yuvanın hepsi doluyken** yeni bir `SAFETY_CLOSE` gelirse kart `IDA_SESSION_CLOSE(EVENT_OVERFLOW)` (= 7) göndermeyi dener ve oturumu bitirir (A1-K22): sim'de oturum sonu iki tüpün iznini kapatır (A1-K23), kartta oturum sonu çağrısı silahlandırmayı ve izinleri kaldırır (A4-K71, K73). Kart bir olayı ilk kabul kapısından geçirdiği çağrıda `IDA_EVENT` olarak (zorunlu-güvenilir sınıf; A1-K110), izin açan olayları A1-K106'ya göre `safety_rev` ile damgalayarak gönderir ve sonucu gelene ya da olayı çıkaran çağrının `now_w`'sinden `apply_by_ns` geçene kadar aynı yükle `T_olay_yin` aralıkla yineler; son geçerlilik dolunca yinelemeyi durdurur ama **yuvayı** sonuç gelene ya da `now_w + apply_by_ns + T_sonuç_pay` (95 ms; E8) dolana kadar açık tutar ve `EXPIRED_NO_ACK`'i çekirdeğe ancak bu pay sonuçsuz dolunca verir (A4-K106); sonucu beklenen izin açan olayın izni açık varsayılmaz ve çekirdek aynı tüp için yeni izin açan olay ya da `FIRE` üretmez (A4-K123).
- **A1-K81.** Sim bir çalıştırma boyunca karara bağladığı her `event_id`'nin son sonucunu ve `t_apply`'ını saklı sonuç tablosunda tutar; aynı `event_id` yeniden gelirse olayı yeniden işlemez: karar henüz verilmediyse yinelemeyi yok sayar; karar verildiyse bu bir **saklı sonuç talebidir** ve A1-K111'in (b) yoluyla yanıtlanır (`APPLIED` saklıysa `DUPLICATE` ve özgün `t_apply`, ret saklıysa aynı ret kodu).
- **A1-K82.** Sim üst 32 biti oturumun `BOARD_HELLO.boot_count`'una eşit olmayan `event_id`'yi `REJ_SESSION` ile reddeder.
- **A1-K83.** Sim A1-K109'dan geçmiş, `batch_seq = n` taşıyan ve `a_n`'deki fizik adımı hesaplanmadan alınan bir olayı `a_n`'de, yuvanın sürekli komutuyla aynı adımda uygulamaya çalışır; daha sonra alınan olayı ve `call_kind = TICK_GRID` taşıyan her olayı (Tablo 4.6b'ye göre yalnız `SAFETY_CLOSE` olabilir) alımından sonraki ilk fizik adımı sınırında uygulamaya çalışır ve `olay_gec`'i artırır; izin açan olaylar ayrıca A1-K118'e tabidir. Uygulama anı yalnız A1-K109'dan geçmiş olaylar için tanımlıdır.
- **A1-K84.** Sim A1-K109'dan geçmiş bir olayı uygulama anı `t_apply > t_decide + apply_by_ns` (sim zamanı) ise ya da `TF(batch_seq)`'in yazım başlangıcından bu yana kendi duvar saatinde `apply_by_ns + T_bekle`'den fazla geçmişse uygulamaz ve `REJ_EXPIRED` ile yanıtlar; `TF(batch_seq)` A1-K109 gereği bu oturumda gönderilmiş bir TF'dir. Tek istisna `batch_seq` = 0 ve `t_decide_ns` = 0 taşıyan `SAFETY_CLOSE`'dur (Tablo 4.6 V3): son geçerlilik denetimi yapılmaz, alımdan sonraki ilk fizik adımı sınırında A1-K87'ye göre uygulanır (yalnız kapatır; bu oturumda bundan önce izin açılmış olamaz, çünkü izin açan olay kabul edilmiş bir zaman çerçevesine bağlıdır).
- **A1-K85.** Sim aynı fizik sınırına düşen emniyet işlemlerini iskelet §3.2 "Fizik sınırındaki sıra"nın beş adımıyla, aynen şu sırayla işler (A3-K95 aynı sırayı yazar; A5 ve A6 atıf verir); sim duraklatılmışken gelen emniyet işlemleri beklemez, aynı sırayla, sim zamanı ilerlemeden işlenir: **(1)** oturum sonu ve süresi dolan izinler; **(2)** kısıtlayan komutlar ve senaryonun kısıtlayan atamaları (hepsi uygulanır); **(3)** kart olayları: önce `SAFETY_CLOSE`, sonra izin açan olaylar, sonra `FIRE`; her biri önceki adımlardan sonraki emniyet durumuna ve revizyona göre doğrulanır; **(4)** gevşetmeler: revizyon bu sınırda 1–3. adımlarda arttıysa bu sınırdaki bütün gevşetmeler reddedilir ve yeniden denenmez; artmadıysa önce senaryonun gevşeten atamaları, sonra panel istekleri (her biri taşıdığı revizyona göre) uygulanır; **(5)** sürekli çıktılar. Bir olay, aynı sınırdaki gevşetmeden yararlanmaz. A1'e özgü ayrıntılar (sırayı değiştirmez): (3)'te aynı türdeki olaylar `event_id` sırasıyla işlenir; izin açan olay `SAFETY_OPEN`'dır (A1-K86), `FIRE` A1-K88'e göre doğrulanır; (5)'te sürekli çıktılar A1-K78'e göre `out_seq` sırasıyla uygulanır; revizyon artışları A1-K106'daki işlemlerin bu sıradaki yerinde yapılır (ör. (1)'de oturum sonu ve dolan izinler, (3)'te her `SAFETY_CLOSE`).
- **A1-K86.** Sim bir `SAFETY_OPEN`'ı ancak o adımda `estop = 0`, ilgili tüpte `PLUG_IN` ve `PIN_OUT` kurulu ve olayın `safety_rev`'i sim'in güncel `safety_rev`'ine eşitse uygular (`APPLIED`; tüpün izni `t_apply + hold_ns`'e kadar açık; daha yeni bir `SAFETY_OPEN` bu süreyi kendi `t_apply + hold_ns`'ine taşır; süre dolduğunda izin A1-K85'in (1). adımında kapanır; aynı sınırda dolan izinlerin tümü tek artıştır, A1-K106 md. 5); aksi halde kalıcı `REJ_SAFE` döner (A1-K81'e göre saklanır, aynı kimlik sonradan uygulanmaz).
- **A1-K87.** Sim süresi geçmemiş ve oturumu tutan bir `SAFETY_CLOSE`'u `safety_rev`'inden bağımsız olarak her durumda uygular (`APPLIED`), `tube`'un gösterdiği tüpün (255'te iki tüpün) iznini kapatır ve `safety_rev`'i bir artırır (A1-K106 md. 3; tüp sayısından bağımsız).
- **A1-K88.** Sim bir `FIRE`'ı şu denetimlerin ilk tutmayanının koduyla reddeder, hepsi tutarsa uygular: `REJ_SESSION` (A1-K82); `REJ_EXPIRED` (A1-K84); `REJ_SAFE` (olayın `safety_rev`'i güncel `safety_rev`'e eşit değil, ya da o adımda `estop = 1`, ya da tüpte `PLUG_IN` veya `PIN_OUT` kurulu değil, ya da tüpün izni kapalı); `REJ_TUBE_EMPTY` (tüpte `LOADED` kurulu değil); ret kalıcıdır (A1-K81); uygulanan `FIRE` o adımda 04'ün fırlatma modelini tetikler ve tüpün `LOADED` bitini siler.
- **A1-K89.** Sim bir tüpün iznini, A1-K86–K88'e ek olarak, o tüpü etkileyen kısıtlayan bir emniyet girdisi uygulandığında (A1-K85 adım 2: `estop` basıldı; tüpün fişi çıkarıldı ya da pimi takıldı) ve oturum bittiğinde (adım 1) aynı sınırda kapatır. Bu kapanmalar **bağımsız bir revizyon artışı tanımlamaz**: artış yalnız A1-K106'nın kapalı listesindeki işlemden gelir ve işlem başına tam birdir (iki tüpü kapatan tek acil durdurma, durumu değiştirmeyen bir kısıtlama ya da aynı sınırdaki örtüşen kapanış nedenleri yine işlem başına bir artış üretir; U2). İzin durumu ya da `safety_rev` her değiştiğinde sim A1-K56'ya göre `IDA_SAFETY` gönderir. Bir girdi yeniden açıldığında eski yetki geri gelmez: o ana kadar damgalanmış her `SAFETY_OPEN` ve `FIRE` eski revizyonu taşır ve reddedilir; yeni bir olay gerekir.
- **A1-K103.** Sim karara bağladığı her olayın sonucunu `IDA_EVENT_RESULT` ile, `t_hazır`'ı karar adımı olan kritik üye olarak A1-K111'in sıra kuralıyla gönderir; sonuç hiçbir zaman düşmez ve birleştirilmez.
- **A1-K106.** Emniyet revizyonu iskelet §3.2'nin tanımıdır (U2): dünya tarafı (sim) tek bir emniyet revizyonu tutar; işaretsiz 32 bit (`uint32_t`); süreç başında 1; çalıştırmalar ve oturumlar arasında sıfırlanmaz; `0xFFFFFFFF`'te doyar ve doyduktan sonra süreç yeniden başlayana kadar bütün gevşetmeler reddedilir (izin açan her olay da kalıcı `REJ_SAFE` alır; H49); 0 değeri yalnız "platform henüz emniyet örneği görmedi" anlamındadır; A3'ün emniyet revizyonuyla aynı sayaçtır. Revizyonu **yalnız** şu altı işlem artırır; her işlem tam bir artırır, işlemin kaç tüpü etkilediği ya da durumu değiştirip değiştirmediği artışı değiştirmez; artış işlemin işlendiği fizik sınırında, A1-K85'teki sıradaki yerinde yapılır: **(1)** kabul edilen her kısıtlayan komut: sanal emniyet girdisini kısıtlayan yönde değiştiren panel isteği ya da A4'ün daraltıcı sınıfındaki yer istasyonu komutu (komut başına); **(2)** bir fizik sınırında uygulanan, senaryo kaynaklı kısıtlayan atamaların tümü (tek işlem); **(3)** uygulanan her `SAFETY_CLOSE` kart olayı; **(4)** her bağlantı oturumunun sonu (nedeni ne olursa olsun); **(5)** bir fizik sınırında süresi dolan izinlerin tümü (tek işlem); **(6)** uygulanan her gevşetme: panelin gevşeten sanal girdi isteği (istek başına) ya da bir fizik sınırında uygulanan senaryo kaynaklı gevşeten atamaların tümü (tek işlem). Artırmayanlar: uygulanan `SAFETY_OPEN` (izin açma ve yenileme), `FIRE`, reddedilen her girdi, sürekli çıktılar. Gevşeten (A4'ün genişletici sınıfındaki) yer istasyonu komutu kabul edildiği andaki revizyona bağlanır; bağlantı emülatöründen çıkıp A1'e verileceği anda revizyon değişmişse atılır (sahibi A3; U4a). Değer her `IDA_SAFETY`'de taşınır. Kartın platformu izin açan her olayı (`SAFETY_OPEN`, `FIRE`) olayı üreten çekirdek çağrısına bu oturumda verilmiş en son `IDA_SAFETY` örneğinin revizyonuyla damgalar ve oturum her kurulduğunda (`SESSION_START`) bu değeri 0'a çeker (A4 arayüzü değişmez; değer platformda tutulur). Sim izin açan bir olayı yalnız damgası sıfırdan farklı ve A1-K85'in (3). adımındaki güncel revizyona eşitse uygular; değilse kalıcı `REJ_SAFE` döner ve sonucu A1-K81'e göre saklar.
- **A1-K111.** Sim olay sonuçlarını iki ayrı yoldan gönderir. (a) **Yeni sonuçlar** (bir `event_id`'nin ilk kararı) `Q_sonuç` = 8 girişlik FIFO'ya girer ve hiçbir zaman atılmaz ya da birleştirilmez; sim bir partiye en çok 4 sonuç koyar, ilk dördü karar anında içinde bulunulan pencereye hemen yazar, kalanlar sırasıyla sonraki pencerelere kalır; kuyruk oturum sonunda silinir. (b) **Saklı sonuç talepleri** (A1-K81) kuyruğa girmez: sim bunları saklı sonuç tablosundan, bir partiye en çok 1 tane ve yalnız (a) yolundan sonra partide yer kaldıysa yanıtlar; sığmayan talebi atar ve `sonuc_talebi_atildi`'yi artırır (kart `T_olay_yin` sonra yeniden sorar; bu yol bilerek kayıplıdır). (a) kuyruğu E16'ya göre taşamaz; yine de taşarsa sim oturumu protokol hatasıyla bitirir (A1-K23).

- **A1-K120.** Kartın platformu yeni olayların ilk gönderimini kendi duvar saatinde iki kovayla sınırlar: izin açan olaylar (`SAFETY_OPEN`, `FIRE`) için patlama 4, hız 5 ms'de 4; `SAFETY_CLOSE` için patlama 3, hız 5 ms'de 3. Kapı A4-K67 ile aynıdır: kova boşken gelen izin açan olay ilk kabul kapısında `DROPPED_NO_SLOT` alır (A1-K80); kova boşken `SAFETY_CLOSE` atılmaz, tabloda (A1-K80'in yuvalarından birinde) bekler ve kova izin verince gönderilir (bu durumda o çağrının `n_events`'ine sayılmaz ve sim onu A1-K83'e göre geç olay olarak uygular).
### 4.8 Telemetri, sonuçlar ve izler (kart → sim)

- **A1-K90.** Kart bir çağrının çıktısını şu sırayla gönderir: yeni `IDA_EVENT`'ler, `IDA_ACTUATOR`; ardından zorunlu-güvenilir sınıfın kalanı (`IDA_CMD_RESULT`, olay yinelemeleri, oturum çerçeveleri) ve kaybedilebilir sınıf bu öncelikle: `IDA_STATUS`, `IDA_FC_STATUS`, `IDA_CALL_TRACE`, `IDA_NAV`, `IDA_LOG`, `IDA_CORE_COUNTERS`, `IDA_LINK_STATS`, `IDA_FAULT_RECORD`, `IDA_DET_RECORD`; sınıfların kuyruk kuralları A1-K110'dadır.
- **A1-K91.** Kart `IDA_NAV`'ı `IDA_RC_OK` dönen ve `batch_seq` 4'ün katı olan step çıktılarından, `IDA_STATUS` ve `IDA_FC_STATUS`'u kendi duvar saatiyle 100 ms'de bir son çıktıdan, `IDA_CORE_COUNTERS` ve `IDA_LINK_STATS`'ı 1 s'de bir gönderir.
- **A1-K92.** Kart her çağrının `out.results[]` öğelerinin her biri için bir `IDA_CMD_RESULT` gönderir (A1-K67).
- **A1-K93.** Kart her çağrının `out.logs[]` öğelerinin her biri için bir `IDA_LOG` gönderir.
- **A1-K94.** Kart her çağrının `out.det_tx[]` öğelerinin her biri için bir `IDA_DET_RECORD` gönderir (kart içi sonar algılamasının değerlendirme kaydı; parti başlığı gönderilmez).
- **A1-K95.** Kart her çekirdek çağrısı (step, zaman çerçeveli tick, ızgara tick'i, oturum sonu çağrısı; `call_kind` 1–4, oturum sonu çağrısı A4-K122'ye göre ayrı türdür) için tam bir `IDA_CALL_TRACE`'i kaybedilebilir sınıfta gönderir; sim bunu A6'ya yazar; gönderilemeyen iz (kartta `tx_dropped`, sim'de `CALL_TRACE`'in `batch_seq` ya da çağrı boşluğu) işlevsel hata değildir, ilgili periyot "zamanlaması doğrulanmamış" sayılır ve o çalıştırmanın girdi tekrarı "tam değil" diye işaretlenir. Oturum sonu çağrısının izi en iyi çabayla, `IDLE`'a geçmeden önce gönderilir (A1-K22); oturum bittiği için sim onu oturum dışında alır ve kaydeder, kaybı `cagri_izi_eksik`'e sayılmaz.
- **A1-K96.** Kart oturum kurulduğunda önceki açılıştan bir hata kaydı varsa tam bir `IDA_FAULT_RECORD` gönderir.
- **A1-K110.** Kart gönderim kuyruğunu üç sınıfla yönetir: (i) kritik sürekli çıktı (`IDA_ACTUATOR`); çağrının yeni olayları o çağrının `IDA_ACTUATOR`'ıyla birlikte bu öncelikte ve ondan önce gider (A1-K73); (ii) zorunlu-güvenilir (`IDA_EVENT`'in ilk gönderimi ve yinelemeleri, `IDA_CMD_RESULT`, `IDA_KEEPALIVE`, `IDA_SESSION_CLOSE`, `IDA_BOARD_HELLO`): bu sınıfa her periyotta en az `S_ii` = 1 000 B hizmet ayrılmıştır ((iii) bu payı kullanamaz), `Q_ii` = 64 çerçevelik yeri vardır ve bu sınıftan hiçbir çerçeve atılmaz; yine de taşarsa (E17'nin geliş varsayımı bozulmuştur) kart bunu bağlantı hatası sayar, `IDA_SESSION_CLOSE(TX_OVERFLOW)` göndermeyi dener ve oturumu bitirir (A1-K22; daraltma); (iii) kaybedilebilir (diğer bütün çerçeveler): periyot başına en çok `R_iii` bayt gönderilir, kuyruğu dolunca en eskisi atılır ve `tx_dropped` sayılır. Kart taşıma sürücüsüne aynı anda en çok bir çerçeve verir (önceki çerçevenin son baytı sürücüden çıkmadan yenisini vermez) ve her çerçeve sınırında (i) > (ii) > (iii) önceliğiyle seçer; böylece kritik çıktının önünde en çok bir kritik olmayan çerçeve (≤ 156 B) bulunur.

### 4.9 SIL taşıması

- **A1-K97.** SIL'de A1 yalnız 127.0.0.1 üzerindeki bir TCP bağlantısıdır: sim bir bağlantı noktasında dinler, SIL koşucusunu `--a1-port <ondalık>` komut satırı argümanıyla başlatır, koşucu bağlanır ve iki taraf `TCP_NODELAY` kullanır.
- **A1-K98.** SIL'deki bayt akışı kartlı akışla aynıdır: aynı çerçeveler, aynı mesajlar, aynı `sysid`/`compid`, aynı oturum el sıkışması; koşucu `role = IDAW_ROLE_SIL` bildirir.
- **A1-K99.** SIL'de her yeni TCP bağlantısı yeni bir akıştır (tampon boşaltma gerekmez) ve bağlantının kapanması iki tarafta oturumun bitişidir.
- **A1-K100.** SIL'de kartın duvar saati zaman çerçevesinden türetilir (A4-K11), ızgara tick'i yoktur (dolayısıyla `ASYNC` çıktı da yoktur), A1-K22'nin `T_bağ` koşulu uygulanmaz ve bariyer süre sınırı yerine A1-K76 geçerlidir.
- **A1-K101.** `protocol/ida_link` ayrıştırma ve kodlama durumunu yalnız örneğin kendi yapısında tutar; MAVLink kitaplığının kanal dizilerini (kitaplık içi durum) kullanan işlevleri kullanmaz; böylece bir süreçte birden çok SIL örneği koşabilir. Hangi kitaplık işlevlerinin kullanılacağı uygulamanındır (W2'deki uygulama üretilmiş kitaplıktan yalnız tipleri, sabitleri ve CRC'yi alır; kitaplığın ayrıştırıcısı başlıkta reddettiği adayda hemen boşa döndüğü için A1-K07'nin devam kuralını sağlamaz).
- **A1-K102.** (U9; iskelet §4 "Canlı koşular arası eşitlik".) Güvence yalnız **kamera kapalıyken** verilir: kamera açık koşularda kamera karesinin üretimi ve algılama sonucunun dönüşü bilgisayarın zamanlamasına bağlıdır, sim SIL'de bunu beklemez (A8) ve iki canlı koşu farklı algılama başlıkları (ör. `OK` ya da `LATE`) taşıyabilir; kamera açık koşularda güvence yalnız SIL port kaydının, kayıtlı çekirdek çağrı girdilerinin tekrarıdır (A6-K34, K56). Kamera kapalıyken, A1-K121'in kilitli adım koşulları altında, aynı senaryo, aynı çalıştırma tohumu, aynı kalıcı başlangıç durumu (A1-K114), aynı `N_bg` ve aynı ikililerle iki SIL koşusu, TCP bağlantısının açılışından kapanışına kadar **her iki yöndeki bütün çerçeve dizisini** (başlık `seq` ve sağlamalar dahil) bayt bayt aynı üretir; tek istisna sim'in duvar saati gözetimiyle (`T_SIL`) biten koşudur, o koşu karşılaştırmaya girmez. Bu, A6'nın "tekrar güvencesi yalnız SIL'de, A4 port sınırında" sınırıyla aynıdır: A1 baytlarının eşitliği, A4 port girdilerinin eşitliğini de gerektirir. HIL'de bayt eşitliği iddia edilmez (nonce'lar gerçek rastgelelikten gelir; çağrı takvimi duvar saatine bağlıdır).
- **A1-K114.** SIL'de sim, `IDA_SIM_HELLO.nonce`'u ve `sess`'i çalıştırma tohumundan, SIL koşucusu `IDA_BOARD_HELLO.nonce`'u sim'in `--a1-seed <16 onaltılık hane>` argümanıyla verdiği tohumdan, A5'in anahtar türetme işleviyle ve A1'e özgü etiketlerle (`ida/a1/nonce_sim/<deneme>`, `ida/a1/nonce_board/<boot_count>/<deneme>`, `ida/a1/sess/<oturum_sırası>`) belirlenimci olarak türetir; `--a1-seed` değeri A5'in `a1/board_tohum` alt akışından, SIL'deki `run_id` `a1/run_id` alt akışından (A1-K126) gelir. Etiketler A5 Ek D.7'nin alt akış kataloğunda (A5-K84) bu adlarla yer alır; değer türetimi, sıfır ya da yasak sonuçta (ör. `sess` için A1-K15'in yasak listesi) yeniden çekiliş ve sayaç tahsisi A5-K118 ve Ek D.7'dedir; A1 yinelemez, yalnız atıf verir (U17); SIL koşucusunun kalıcı başlangıç durumu (önceki `boot_count`, `prev_sess`, `sessions_since_boot`, `last_cmd_id`, parametre bloğu) sınama girdisinin parçasıdır ve verilmezse sıfırdır (önceki `boot_count` = 0, blok yok); HIL'de nonce'lar ve `sess` gerçek rastgelelikten gelir. **Etiket parametreleri (W1 doğrulama incelemesi parça 3 D8):**

| Parametre | Tutan | İlk değer | Artırılma olayı | Artırmayanlar | Ömür |
|---|---|---|---|---|---|
| `nonce_sim/<deneme>` | Sim | 0 | Sim'in yeni bir el sıkışma için yeni `nonce` seçmesi (A1-K12: port açılışı, oturum sonundan sonraki yeniden el sıkışma, `REBOOT`'tan sonraki el sıkışma); her seçimden sonra bir artar | Aynı `nonce`'lu `SIM_HELLO`'nun `T_hello` ile yinelenmesi; A5-K118'in sıfır sonucunda yeniden çekilişi (aynı etiketin sonraki `n`'i) | Çalıştırma (her çalıştırmanın başında 0) |
| `nonce_board/<boot_count>/<deneme>` | Kart (SIL koşucusu) | 0 | Koşucunun yeni `nonce` çekmesi: açılıştan sonraki ilk `IDA_BOARD_HELLO`'da (duyuru ya da yanıt) ve A1-K13'e göre farklı `nonce`'lu her yeni `SIM_HELLO`'ya ilk yanıtta; her çekilişten sonra bir artar | Aynı `BOARD_HELLO`'nun yeniden gönderimi (A1-K13); sonraki duyurular (son çekilen `nonce`'u taşır); sıfır sonucunda A5-K118'in yeniden çekilişi | Kart açılışı (`boot_count` etiketin içindedir; `REBOOT` ve yeni açılışta 0) |
| `sess/<oturum_sırası>` | Sim | 0 | `OK` sonuçlu her `IDA_SESSION_ACCEPT` için `sess` seçimi (A1-K15); her seçimden sonra bir artar | `REBOOT` ve `REJECT_*` sonuçları; A1-K15'in yasak listesi (0, `prev_sess`, sim sürecinin son 16 oturumu) nedeniyle A5-K118'in sonraki `n`'e geçmesi | Çalıştırma (her çalıştırmanın başında 0) |

Bu tanımla her parametre, kamera kapalı kilitli adımlı SIL'de (A1-K121) yalnız senaryonun, tohumun ve kalıcı başlangıç durumunun belirlediği olay dizisinden (el sıkışma, oturum ve açılış sırası) türer; hiçbiri duvar saatine, yeniden gönderim sayısına ya da TCP zamanlamasına bağlı değildir (yinelemeler sayacı artırmaz). Sim'in A1-K15 yasak listesindeki son 16 oturum kimliği sim sürecinin durumudur ve bayt eşitliğinin (A1-K102) "aynı kalıcı başlangıç durumu" koşuluna girer: eşitlik, ikisi de yeni bir sim sürecinde ya da aynı yasak liste geçmişiyle başlayan iki koşu içindir. A5 Ek D.7 vektörleri bu tanımla tutarlıdır: `ida/a1/nonce_sim/0` çalıştırmanın ilk el sıkışması, `ida/a1/nonce_board/1/0` `boot_count` = 1 açılışının ilk çekilişi, `ida/a1/sess/0` çalıştırmanın ilk oturumudur.


- **A1-K121.** SIL kilitli adımla çalışır: (i) A1'in bütün zamanlayıcıları (`T_hello`, `T_canlı`, `T_hazır`, `T_reboot`, `T_bağ`, `T_olay_yin`, `T_sonuç_pay`) iki tarafta da sim zamanıyla sayılır (koşucunun duvar saati zaten zaman çerçevesinden türer, A4-K11); yalnız `T_SIL` gerçek duvar saatiyle asılmayı gözler; (ii) sıra kesindir: sim parti `n`'i (TF dahil) gönderir, koşucu o partinin çekirdek çağrısını tamamlar ve bütün çıktısını gönderir, sim `IDA_ACTUATOR(n)`'i ve o çağrının çıktılarını aldıktan sonra fiziği ilerletir ve parti `n + 1`'i gönderir; böylece SIL'de parti birleştirme ve çağrı çakışması oluşmaz; (iii) koşucu her step ve tick çağrısından önce tam `N_bg` arka plan çağrısı yapar, oturum sonu çağrısından (A1-K107) önce arka plan çağrısı yapmaz (`N_bg` çalıştırma parametresidir ve A6 üst verisinde `sil_n_bg` olarak saklanır; `bg_slices = N_bg`; A4-K95 buna eşittir; U10); (iii-a) iki canlı koşunun bayt eşitliği yalnız kamera kapalıyken güvencelidir (A1-K102; U9); (iv) kanal kayıpsız olduğundan el sıkışma tek `IDA_SIM_HELLO` ile tamamlanır (sim zamanı el sıkışmada ilerlemediği için `T_hello` yinelemesi doğmaz; koşucu tek `IDA_BOARD_HELLO` gönderir).
- **A1-K126.** (Çalıştırma kimliği; U17, P4-20.) A1'in `run_id`'si (`IDA_SIM_HELLO.run_id`, `IDA_SESSION_ACCEPT.run_id`; `uint32_t`, ≠ 0) A3/A6'nın metin çalıştırma kimliğinden (`YYYYMMDD-hhmmss-xxxxxx` biçiminde) ayrı bir alandır ve ondan şöyle türetilir: kartlı kiplerde (HIL 1x, bariyerli) metin kimliğinin UTF-8 baytlarının SHA-256 özetinin ilk dört baytı, küçük uçlu `uint32` olarak; sonuç 0 ise 1. SIL'de A5'in türetimiyle `a1/run_id` etiketinden üretilir (A5-K118, Ek D.7; 0 sonucunda sonraki sayaç); böylece aynı tohumla iki SIL koşusunda aynıdır (A1-K102). Bir çalıştırmanın bütün oturumları aynı `run_id`'yi taşır; kart değeri yalnız bilgi olarak saklar ve hiçbir davranışı ona bağlı değildir. A6 üst verisi iki kimliği de saklar (`run_id` metin, `a1_run_id` sayı); 32 bitlik değerde çakışma olası olduğundan çalıştırmanın kimliği metin kimliğidir, `a1_run_id` yalnız A1 trafiğini kayıtla eşlemek için kullanılır.
- **A1-K125.** (Panel komut kataloğu; U16, P4-13, P4-14.) `protocol/komutlar.json` panelin yer istasyonu komutlarının tek makinece okunur listesidir; A1'in kapalı komut listesinden (Tablo 3.6, 3.6a) türetilir ve bu belgenin parçasıdır (ikisi çelişemez). Biçim: `{"surum": "1.0", "komutlar": [...]}`; her öğe `ad` (küçük harf, `^[a-z0-9_]{1,64}$`), `a1` (`mesaj`: A1 ileti adı; `tur`: komutu ileti içinde belirleyen tel numaralandırma girdisi — `IDA_CMD_FC`'de `IDAW_FC_OP_*`, diğerlerinde `IDAW_CMD_TYPE_*`), `sinif` (`daraltici` | `genisletici` | `notr`; Tablo 3.6), `onay` (genişleticilerde `true`: A3'ün iki adımlı onay akışı; diğerlerinde `false`), `parametreler` (panelin vereceği değerlerin JSON Schema 2020-12 tanımı; alan adları A1 ileti alanlarının adlarıdır, numaralandırmalar girdi adının öneksiz biçimidir), `sonuc` (nihai yanıtın `veri` nesnesinin JSON Schema'sı; komutun `IDA_CMD_RESULT`'ının alanları) ve `bilesik` taşır. Koşullu sınıflı `SET_MODE` hedefine göre iki öğeye ayrılır (`set_mode_daraltici`: 2–5; `set_mode_genisletici`: 6–7). **Bileşik komutlar** (`bilesik: true`) birden çok ileti gerektiren işlemlerdir; `a1.mesaj` ve `a1.tur` o zaman gönderim sırasıyla dizilerdir; dizilerin hangi öğesinin yinelendiği katalogda işaretli değildir ve aşağıdaki tablodadır (dizi dizini 0'dan):

| Komut | `a1.tur` dizisi | Yinelenen dizin | Yineleme sayısı | Gönderim sırası |
|---|---|---|---|---|
| `mission_upload` | `MISSION_BEGIN`, `MISSION_ITEM`, `MISSION_END` | 1 | `items`'ın öğe sayısı `count` (1–128) | `MISSION_BEGIN(count)`, `count` adet `MISSION_ITEM` (`index` 0'dan), `MISSION_END` |
| `param_apply` | `PARAM_SET`, `PARAM_COMMIT` | 0 | `params`'ın öğe sayısı `n` (1–997) | `n` adet `PARAM_SET`, `PARAM_COMMIT` |
| `param_save` | `PARAM_SET`, `PARAM_COMMIT`, `PARAM_SAVE` | 0 | `params`'ın öğe sayısı `n` (0–997) | `n` adet `PARAM_SET`, `PARAM_COMMIT`, `PARAM_SAVE` |

Yinelenmeyen öğeler tam bir kez gönderilir; `adim` gönderim sırasındaki iletinin sırasıdır (0'dan). Sim'in ağ geçidi bileşik komutun iletilerini sırayla, her birinin `ACCEPTED` sonucunu aldıktan sonra sıradakini göndererek yürütür; ilk `ACCEPTED` olmayan sonuçta durur ve nihai yanıtın `veri`'sinde o sonucu ve başarısız adımın sırasını (`adim`, 0'dan) bildirir (görev yüklemesinin atomikliğini çekirdek sağlar, A4-K51). `HEARTBEAT` katalogda yoktur (ağ geçidi onu A1-K70'e göre kendisi üretir). `FC_FIRE` katalogda genişletici ve onaylı bir komuttur (`fc_fire`; parametresi yalnız `tube` 0–1, argümanlar 0; Tablo 3.6); sim fırlatıcısını kartı atlayarak tetikleyen öğe yoktur ve hiçbir öğe hedef, menzil, yön ya da atış ölçütü taşıyan parametre içermez. Katalogdaki sayısal sınırlar A4'ün alan aralıklarıyla ve Tablo 3.6a ile aynıdır (`guided_sp`: `depth` ve `altitude` ≥ 0, `yaw` (−π, π], öteki alanlar sonlu (A4 3.4, A4-K50); `manual.axes` [−1, 1] (A4-K49); görev öğeleri Tablo 3.6a; `MISSION_BEGIN` 1–128 (A4-K51)); A4 "sonlu" dediği alanlarda katalog f32 aralığını kullanır. Katalogdaki her `parametreler` ve `sonuc` şeması A3-K121'in sınırlarına uyar: her tamsayı ve ondalık sayı `minimum`/`maximum` taşır (ondalıklar f32 aralığında, tamsayılar ±(2⁵³ − 1) içinde), her dizi `maxItems` taşır (parametre listesi en çok (8 000 − 24) / 8 = 997 öğe, A4 parametre bloğundan; görev en çok 128 öğe), serbest dize ve ek özellikli nesne yoktur; bileşik sonuçların `adim` alanı 0 ile son adımın sırası arasındadır (`mission_upload` 0–129, `param_apply` 0–997, `param_save` 0–998). Katalogda olmayan hiçbir komut panelden gönderilemez (A3-K100). Kataloğu değiştiren her değişiklik bu sözleşmenin revizyonudur.
- **A1-K127.** (Kartlı kayıttan yeniden kurma girişi; U18, P4-18, iskelet §3 A6 satırı.) `protocol/ida_link` yalnız PC derlemesinde (kart yazılımında bulunmaz) bir **yeniden kurma girişi** sunar: girdisi A6'daki kartlı kaydın iki yöndeki A1 çerçeveleridir; giriş el sıkışma yapmaz, oturum değerlerini (`sess`, `boot_count`, `run_id`, `dialect_hash`, `port_*`, `param_*` ve `IDA_BOARD_HELLO`'nun öteki açılış alanları) kayıttaki `IDA_SIM_HELLO`, `IDA_BOARD_HELLO` ve `IDA_SESSION_ACCEPT` çerçevelerinden alır; kayıttaki her `OK` sonuçlu `IDA_SESSION_ACCEPT` yeni bir oturumdur ve her oturumda PC'deki çekirdek o oturumun `IDA_BOARD_HELLO`'sundaki açılış alanlarıyla (`boot_count`, `reset_cause`, `reset_flags`, `consecutive_faults`, `param_src`) yeniden başlatılır; ardından kartın A1-K28–K58, K63, K107 ve K124 kurallarıyla çağrı girdileri kayıttaki çerçevelerden ve `IDA_CALL_TRACE`'lerden (`now_w_ns`, `bg_slices`, `rx_age_ns`) yeniden kurulur; oturum sonu çağrısı kayıttaki oturum sonundan (`IDA_SESSION_CLOSE`, `T_bağ` aşımı ya da yeni `nonce`) kurulur, oturum sonu çağrısının izine dayanmaz. Bu yeniden kurma bit düzeyinde eşitlik güvencesi taşımaz (iskelet §3, A6 satırı; O11).

---

## 5. Hata durumları

| # | Durum | Algılama | Sim'in davranışı | Kartın davranışı |
|---|---|---|---|---|
| H1 | Bozuk çerçeve (CRC) | Sağlama | Atar, sayar, yeniden eşler (A1-K07) | Aynı |
| H2 | Yanlış başlık ya da yanlış yönlü/kimlikli çerçeve: `incompat_flags ≠ 0`, MAVLink 1 baytı, bilinmeyen kimlik, geçersiz `len` (başlıkta; aday `0xFD`'den sonra yeniden taranır); sağlaması tutan ama yabancı `sysid`/`compid`'li ya da yönü yanlış çerçeve (sağlamadan sonra) | Başlık alanları; A1-K07'nin denetim sırası | Atar, sayar (A1-K02, K04–K07); sağlaması bozuk her dialect çerçevesi, `sysid`'i ne olursa olsun `rx_crc_err`'dir | Aynı |
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
| H17 | Eski bir `SAFETY_OPEN` ya da `FIRE`, araya giren daraltıcı emniyet değişiminden (girdinin kapanması, `SAFETY_CLOSE`, izin süresinin dolması, oturum sonu) sonra geldi | Olayın `safety_rev` damgası güncel değil | Kalıcı `REJ_SAFE`; girdi yeniden açılsa da eski yetki geri gelmez (A1-K106, K89) | Yeni olay gerekir |
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
| H37 | Yukarı yönde kaybedilebilir talep `R_iii`'yi aşıyor | Gönderim kuyruğu | `cagri_izi_eksik` (iz düştüyse) | Önce `DET_RECORD`, sonra `LOG` seyreltilir; `tx_dropped` (A1-K90, K110) |
| H39 | Kartın zorunlu-güvenilir kuyruğu taştı | `Q_ii` (E17'nin varsayımı bozuldu: taşıma boşalmıyor) | Oturum sonu (A1-K23) | `SESSION_CLOSE(TX_OVERFLOW)` dener, oturumu bitirir; daraltma (A1-K22, K110) |
| H40 | CRC'si doğru, alanı geçersiz kart çıktısı | Tablo 4.6 | Bütünüyle reddeder, hiçbir etki yok, sayar; geçersiz yanıt partiyi yanıtsız bırakır (A1-K109) | — |
| H41 | Sürekli çıktılar üretim sırasına aykırı | `out_seq` (V5) | Reddeder (A1-K78, K105) | — |
| H42 | Oturum sonu ile yeni el sıkışma çakıştı (hızlı yeniden bağlantı) | Oturum sonu tick'i sürerken yeni `SIM_HELLO` | `SIM_HELLO`'yu yinelemeyi sürdürür | Önce oturum sonu tick'i, sonra yanıt; `SESSION_START` sonraki çağrıda (A1-K107) |
| H43 | Olay sonucu kuyruğu taştı | `Q_sonuç` (E16'nın varsayımı bozuldu) | Protokol hatası, oturum sonu (A1-K111) | — |
| H44 | Yapılandırma izin verilen aralıklar dışında | 3.9 | `REJECT_CONFIG`; çalıştırma başlamaz; A3'e eşitsizlik numarası (A1-K104) | `IDLE` |
| H45 | Bir partinin kritik baytı `B_krit`'i aştı | A1-K113 | Sensörleri öncelik sırasıyla kaydırır, sayar | — |
| H46 | Planlanan son tarih kaçırıldı ya da sim gecikti | A1-K108 | `son_tarih_kacirildi` / `sim_gecikmesi`; çalıştırma geçersiz | — |
| H47 | TF geldiğinde başka bir çekirdek çağrısı sürüyor | Kart (A1-K117) | `cagri_cakismasi`; çalıştırma geçersiz | `CALL_CONFLICT` bayrağı |
| H48 | Görev öğesi kapalı listenin dışında ya da parametresi geçersiz | Kart (A1-K122) | Sonucu A3'e iletir | `LINK_BAD_FIELD` |
| H49 | Emniyet revizyonu doydu | Sim (A1-K106) | Her gevşetme ve izin açan olay reddedilir; A3'e bildirilir; sim yeniden başlatılmalı | — |
| H50 | İzin açan olayın yanıtı gelmedi, reddedildi ya da tutarsız | Sim (A1-K118) | Olay kalıcı `REJ_SAFE` | — |
| H51 | Olay tablosunun sekiz yuvası doluyken yeni `SAFETY_CLOSE` | Kart (A1-K80) | Oturum sonu (A1-K23): iticiler nötr, iki tüpün izni kapalı, revizyon artar | `IDA_SESSION_CLOSE(EVENT_OVERFLOW)`, oturumu bitirir; olay atılmaz |
| H52 | Bir çağrıya düşen emniyet örnekleri örnek kapasitesini aşıyor (E20'nin varsayımı bozuldu) | Kart (A1-K124) | Oturum sonu (A1-K23) | Hiçbir emniyet örneğini atmaz; `IDA_SESSION_CLOSE(PROTOCOL)`, oturumu bitirir |
| H53 | Daraltıcı komut mandala alınırken ertelenmiş genişletici komutlar var | Kart (A1-K63) | Sonuçları A3'e iletir | Genişleticileri atar, her birine `REJECTED` / `NARROWED` |
| H54 | Eski ya da yinelenen partide geçerli `IDA_SAFETY` üyesi | Kart (A1-K31, K124) | — | Üyeyi atmaz, sıradaki çağrıya verir (çekirdek reddeder, kısıtlayıcı içeriği uygular) |
| H55 | Sim zamanı süreksiz (yeni çalıştırma, sıfırlama, atlama) | Sim (A1-K128) | `IDA_SESSION_CLOSE(RUN_END)`, yeni el sıkışma | Oturum sonu çağrısı (A1-K107) akış durumlarını sıfırlar |
| H38 | Sim gerçek zamanın gerisinde | TF yazım gecikmesi | `tf_gec_yazim_ns`; geride kalınca A1-K39 ile yetişir; yanıt kaçırılırsa geçersiz | Sıkışık varışı birleştirir (A1-K40) |

### 5.1 Hata enjeksiyonu (sim)

Sim'in hata enjektörü A1 çerçeveleri üzerinde, her yönde ayrı ve mesaj adına (`protocol/ida.xml`'deki `IDA_*` adları) göre seçilerek çalışır; işlemler: çerçeve düşürme, çoğaltma, komşu çerçevelerin yer değiştirmesi, bayt bozma, kesme, gecikme sıçraması, çöp bayt ekleme (06 §2.12). Her işlem tohumludur ve A6'ya yazılır. Beklenen birincil belirtiler: bozma ve kesmede `rx_crc_err`; düşürmede `batch_incomplete` (üye) ya da `batch_skipped` ve `parti_kacirildi` (TF), `olay_eksik` (olay); çoğaltmada `batch_dup` (TF), çekirdeğin `dup_seq`'i değil `sample_rejected`'i (üye), `yanit_ret_yinelenen` (yanıt), `DUPLICATE` (komut ve olay); çöp baytta `rx_resync_bytes`. Bir enjeksiyon birden çok belirti üretebilir. Fiziksel bağlantıda enjeksiyon yapılmaz; enjektör taşıma ile ayrıştırıcı arasında, sim'dedir.

---

## 6. Kalite kademelerinin etkisi

- A1'in mesaj kümesi, tel biçimi, oturum ve zamanlama kuralları her kademede aynıdır (iskelet §6).
- Sonar kademesi Ping360 satırının içeriğini ve `n_samples`'ı değiştirebilir; iki kademede de üst sınır 1 200 örnektir ve bant bütçesi bu sınırla hesaplanmıştır (3.10). Kademe karta bildirilmez.
- Kamera kapalı kademede algılama partisi gelmez; sim A8-K64'ün `NO_FRAME` başlığını 1 Hz'te gönderir (A1-K58).
- Kayıt kademesi `olaylar` iken A1 trafiği tam kaydedilmez, TF yazım zamanları kayda girmez (A1-K43) ve girdi tekrarı ile zamanlamanın yeniden kurulması güvencesi yoktur (iskelet §3, A6 satırı); sim'in zamanlama geçerliliği kararı kademeden bağımsızdır.
- Emniyet zinciriyle ilgili bütün kurallar (A1-K22, K23, K31, K56, K61, K63, K77, K78, K80–K89, K103, K105–K107, K109–K111, K118, K120, K124, K128, K129) her kademede aynıdır ve aynı sınamalardan geçer.

---

## 7. Uygunluk

Test kodu bu işte yazılmaz. Tür kısaltmaları: **D** derleme, şema ya da statik denetim; **B** birim (`ida_link` ya da sim'in HIL köprüsü tek başına); **S** sözleşme testi (bir taraf gerçek, karşı taraf baytlarla konuşan sahte uç); **SIL** sim ve SIL koşucusu; **K** kartlı (HIL).

### 7.1 Kural başına sınama

| Kurallar | Tür | Sınama yöntemi ve gözlenecek büyüklük |
|---|---|---|
| K01 | K | Kartlı oturumda iki yönde 600 s akış (MP3 yönteminin ürün çerçeveleriyle yinelenmesi): kayıp ve `rx_crc_err` 0 |
| K02, K04–K07 | B, S | Her başlık hatası türüyle üretilmiş çerçeve dizileri; atılan çerçeve sayısı ve ilgili sayaç; ardından gelen geçerli çerçevenin kabulü |
| K03 | D | Üretilmiş başlık ile `port.h` aynı çeviri biriminde (C17 ve C++20) ad çakışmasız derlenir ve her `IDAW_` ayna değeri karşılık `IDA_` sabitine `_Static_assert` ile eşittir; denetim listesi `protocol/ayna.json`'dan üretilir (136 eşitlik; 7.3) |
| K05 | B, S | Ek B dizilerinin kırpılmış ve kırpılmamış biçimleri; `len` = 0 ve `len` > azami: ret |
| K07 | B | Rastgele bayt akışına gömülü geçerli çerçeveler: bozulan adayın kapladığı bölgeden sonraki ilk geçerli çerçevenin yakalanması |
| K08 | B | `seq` boşluklu dizi: `rx_seq_gap` artışı, davranış değişmez |
| K09 | K | Kart alım işini yapay olarak durdurup taşma üretme: `rx_overflow`, yeniden eşleme, eksik parti |
| K10 | B, S | Gönderim yolunda kısmi yazım: bayt akışında iç içe çerçeve yokluğu |
| K11–K17, K107 | S, SIL, K | El sıkışma senaryoları: başarılı; her `REJECT_*`; aynı `nonce`'un yinelenmesi (oturum bitmez); yeni `nonce` (oturum biter, `SESSION_END` çağrısı); `SESSION_ACCEPT` kaybı (sim `T_hazır` sonra yeniden dener); `REBOOT` (kartta `reset_cause = SOFTWARE`, `boot_count` artar); **hızlı yeniden bağlantı**: bir step sürerken `SESSION_CLOSE` ve hemen ardından yeni `SIM_HELLO`: oturum sonu tick'inin (`SESSION_END`) step dönüşünden sonra ve `BOARD_HELLO` yanıtından önce olması, `SESSION_START`'ın ayrı ve sonraki bir çağrıda olması, yeni oturumun hiçbir girdisinin oturum sonu tick'inden önce çekirdeğe ulaşmaması (sahte çekirdekle çağrı sırası kaydı) |
| K18, K19 | SIL, K | Bloğu farklı kart: hazırlık oturumu, `PARAM_SET` × N, `COMMIT`, `SAVE`, `REBOOT`; sonunda `param_blob_crc` beklenene eşit |
| K20, K21 | S | Trafiksiz bekleyen iki uçta 100 ms'de bir `KEEPALIVE`; `SESSION_CLOSE`'un iki yönde hemen oturum sonu üretmesi |
| K22, K23 | S, SIL, K | Silahlı ve izinli durumda: kablo çekme (kartlı, elle), sim sürecini öldürme, `T_bağ` aşımı: kartta aynı çağrıda `armed = 0` ve `SAFETY_CLOSE` (A4-K73), sim'de iticiler nötr ve izinler kapalı; yeni oturumda eski komutların ve olayların etkisizliği |
| K25–K27 | S | Eski `sess`'li TF, komut ve olay sonucu: atılma ve sayaç; `RUNNING` dışı üyeler atılır |
| K28–K31, K123 | B, S, SIL | Parti dizileri: olağan, üye kaybı, TF kaybı, `DUP`, `OLD`, büyük `GAP`; fark 2³¹ − 1 (boşluk), tam 2³¹ ve üstü (`OLD`); kapanış, sayaçlar, çekirdek çağrısı sayısı, yanıt sayısı; `DUP`/`OLD` penceresindeki `IDA_SAFETY`'nin sıradaki çağrıda bulunması |
| K32 | S | Sahte çekirdekle: IMU'lu parti → step, IMU'suz → tick; `tf` alanları |
| K33–K38 | B (sim) | Pencere planlayıcısına sentetik `t_hazır` ve boy dizileri: atama tablosunun aynı girdiyle iki koşuda bit düzeyinde aynılığı; sınırların hiçbirinin aşılmaması; kritik üyelerin ertelenmemesi; Ping360 seyreltmesi; kamera partisinin `P_k`'da ve bölünmeden |
| K39, K40 | K | Kartta TF varış aralıklarının dağılımı ve `n_merged` sayısı (W4); sim tarafında ardışık TF yazımları arası en küçük aralık ≥ 2,5 ms |
| K41 | SIL, K | Duraklatma ve sürdürme: `batch_seq` ve `t_sim_ns` sürekliliği; HIL'de geçersizlik bayrağı |
| K42, K43 | D, inceleme | Mesaj alanlarında karşı tarafın duvar saatini taşıyan ya da ona çeviren alan olmadığının incelemesi |
| K44, K45, K78, K105 | S, SIL, K | IMU kesilmesi: ızgara tick'inin `ASYNC` çıktısı (en çok 100 Hz); sim'de bir sonraki fizik adımında uygulanması; `out_seq`'i küçük, uygulanmamış bütün olağan yuvaların iptali; `ASYNC`'ten sonra üretilen olağan yanıtın kendi yuvasında uygulanması; aynı fizik sınırına düşen iki çıktıda `out_seq` sırası (sonuncusu kalır); artmayan `out_seq`'in reddi |
| K46–K48 | S, K | Her sensör mesajının A4 alanlarına eşlemesi (Tablo 3.4b) için alan alan karşılaştırma; çevrilemeyen değerde `VALID = 0`; kartta damgaların akış sırasında azalmaması ve `t_rx ≤ now_w` (A4-K14) |
| K49–K53 | B, S | `n_samples` ∈ {200, 239, 240, 241, 600, 1199, 1200}; sonu sıfır olan son parça; eksik, yinelenmiş, çelişkili parça; HEAD'siz parça; iki yuva doluyken üçüncü satır; birden çok pencereye yayılan satır; teslim partisinin HEAD'in partisi olması; kutu zamanı formülünün sayısal referansla karşılaştırılması |
| K54–K58 | S | Ping1D, DVL, emniyet ve algılama mesajlarının alan alan eşlemesi; aynı pencerede kapanma ve açılmanın iki partiye bölünmesi; algılama alanlarının bit düzeyinde aynı taşınması (A8 altın kayıtlarıyla) |
| K59–K64 | S, SIL | Her komut türü ve her daraltıcı için mesaj → A4 eşlemesi; dönüşüm hataları (`SET_MODE` 0, 1, 8; `FC_DISARM`/`FC_ABORT`'ta `tube` ≠ 255, diğer FC işlemlerinde `tube` = 255); daraltıcı yağmurunda mandal bitleri (A4-K46 testiyle birlikte); ertelenmiş genişleticiler bekliyorken gelen daraltıcıda her genişleticiye `NARROWED`, nötrlerin korunması |
| K65–K67 | S, SIL | `cmd_id` tekliği; oturum değişiminde `last_cmd_id` üstünden devam; her komut çerçevesine en az bir sonuç; çoğaltılmış komutta ikinci sonucun `DUPLICATE` |
| K68, K69 | SIL, K | `PARAM_SAVE` sonucunun `save_crc`'si ile kartın sonraki `param_blob_crc`'sinin eşitliği; çok partiye yayılan görev yüklemesinin atomikliği |
| K71–K77 | B (sim), SIL | Bariyer ve kabul kuralları: gecikme dağılımı taramasında kaçırmanın yalnız ve tam olarak `a_n`'de yuva boşken sayılması; uygulama anlarının her koşulda `a_n`; `THR_VALID = 0` yanıtında komutun değişmemesi; kullanım süresi dolunca nötr; geç ve yinelenen yanıtın reddi |
| K79 | SIL, K | Her geçersizlik nedeninin tek başına üretilmesi ve bayrağın kalkması; nedensiz koşuda bayrağın kalkmaması |
| K80–K89, K103, K106, K111 | B (sim), S, SIL | Olay kaybı, sonuç kaybı, yineleme, süre aşımı, eski açılış; her `REJ_*` kodunun tek başına üretilmesi; aynı fizik sınırına düşen oturum sonu, dolan izin, kısıtlayan girdi, `SAFETY_CLOSE`, `SAFETY_OPEN`, `FIRE`, gevşetme ve sürekli çıktının A1-K85 sırası (gevşetmenin olaydan sonra ve revizyon arttıysa reddi); A1-K106'nın altı işleminin her birinin tam bir artış, artırmayanların sıfır artış üretmesi (iki tüpü kapatan tek girdide bir artış); sekiz yuva doluyken `SAFETY_CLOSE`'ta `EVENT_OVERFLOW`; sonuç payı dolana kadar yuvanın açık kalması; `DUPLICATE`'te özgün `t_apply`; `safety_rev`: girdi kapanıp yeniden açıldıktan sonra eski damgalı `SAFETY_OPEN`/`FIRE`'ın kalıcı reddi ve aynı kimliğin sonra da uygulanmaması; yeni olayın kabulü; partide 5. ve sonraki sonuçların sonraki partiye kalması, hiçbir sonucun düşmemesi, yeni sonucun yinelemeden önce gitmesi, kuyruktaki kimliğe yinelemenin yeni giriş açmaması |
| K90–K96 | S, K | Yukarı yön önceliği: kritik çerçevenin önünde yazımı süren en çok bir kritik olmayan çerçeve; hızların ölçümü; her çağrı için tam bir `IDA_CALL_TRACE` |
| K97–K102 | SIL | Kamera kapalı aynı senaryonun iki SIL koşusunda iki yönün bayt dizilerinin (A1-K102'nin istisnaları dışında) eşitliği; aynı süreçte iki SIL örneği |
| K116 | B (sim), K | Ön denetimin olağan profili ve bağlayıcı üst sınırı ayrı hesaplaması; kartlı 600 s koşuda iki yönün ölçülen ortalaması ve en kötü periyodu |
| K117 | B (kart), K | TF akarken ızgara tick'lerinin yalnız beklenen TF `E + 1 ms` geçince başlaması; `[E − 1 ms, E + 1 ms)` içinde hiç tick başlamaması; yapay olarak geciktirilen TF'de `CALL_CONFLICT` ve sim'de `cagri_cakismasi` |
| K118 | B (sim), S | `SAFETY_OPEN`/`FIRE` ile yanıtın her tutarsız birleşimi (yanıt yok, `core_rc` ≠ `OK`, `SRC_TICK = 1`, `FIRE`'da `ARMED = 0` ya da `IS_SAFE = 1`): kalıcı `REJ_SAFE` |
| K119, K120 | B, S | Sim'in komut kovası ve platformun olay kovaları: patlama ve hız sınırlarının hiçbir aralıkta aşılmaması; kova boşken izin açan olayın `DROPPED_NO_SLOT`, `SAFETY_CLOSE`'un bekletilmesi |
| K121 | SIL | Kilitli adımda kamera kapalı iki koşunun bütün çerçeve dizilerinin eşitliği (K102); el sıkışmada tek `SIM_HELLO` ve tek `BOARD_HELLO`; `bg_slices = N_bg`; oturum sonu çağrısından önce arka plan çağrısı olmaması |
| K124, E20 | B (kart), S | Sentetik kapasite yükünde emniyet örneklerinin `in.samples[]`'in başında olması ve hiç düşmemesi; birleştirilmiş 17 emniyet taşıyan pencerede oturumun `PROTOCOL` ile bitmesi |
| K125 | D | `protocol/komutlar.json`'un geçerli JSON olması, her öğenin Tablo 3.6 ile ve `ida.xml` adlarıyla tutarlılığı, `FC_FIRE`'ın genişletici ve onaylı olması, `HEARTBEAT`'in yokluğu; parametre şemalarının JSON Schema 2020-12 üst şemasına göre doğrulanması (bu turda yapılamadı; 7.3) |
| K126 | B | Bilinen metin kimliği için `run_id` değeri; 0 çıkan özette 1; SIL'de A5 Ek D.7 vektörü |
| K127 | B (PC) | Çok oturumlu kartlı kayıttan yeniden kurmada her oturumda çekirdeğin kayıttaki açılış alanlarıyla yeniden başlatılması |
| K128 | SIL, S | Sıfırlama ve atlamada `SESSION_CLOSE(RUN_END)`, yeni oturum; bir oturum içinde TF zamanının geriye gitmemesi ve sıçramaması |
| K129 | B (sim), SIL | Kuyrukta bekleyen `ARM` ve `SET_MODE(GUIDED)` varken kabul edilen `DISARM`: genişleticilerin tele yazılmaması, A3'e `bayat_yetki`, `komut_bayat_yetki` sayacı; revizyon değişmemişse gönderimin sürmesi |
| K73 (hata yolu) | S, SIL | `BAD_INPUT` dönen çağrıda `SAFETY_CLOSE`: yuvanın olay beklemeden tamamlanması, olayın kendi `batch_seq`'iyle uygulanması; `batch_seq` = 0 ve `t_decide_ns` = 0 taşıyan `SAFETY_CLOSE`'un V3'ten geçip uygulanması (A1-K84 istisnası) |
| K02, K06–K08 | B | Başlıkta reddedilen adayın arkasındaki geçerli çerçevenin yakalanması; `0xFE` başına bir `rx_mav1`; bilinmeyen kimliğin yalnız `rx_unknown_msgid`'e sayılması; sağlaması bozuk yabancı `sysid`'li çerçevenin `rx_crc_err`'e sayılması; `seq` tekrarında ve atlamasında birer `rx_seq_gap` |
| K122 | B (kart), S | Tablo 3.6a'nın her türü ve her koşulu için sınır değerleri: `item_type` = 8 (eski `TARGET_ALIGN`), sıfır olmayan tanımsız `p[i]`, yanlış `frame`, aralık dışı `depth`; hepsinde `LINK_BAD_FIELD` |
| 3.9 (E1–E19), K104 | D, S | Yapılandırma doğrulayıcısı: aralığın her köşesinde eşitsizliklerin sağlanması; aralık dışı her parametrede `REJECT_CONFIG` ve doğru eşitsizlik numarası |
| K108 | SIL (yapay gecikme), K | Gecikme enjeksiyonuyla bariyerde kurtarılan bir yuvanın `son_tarih_kacirildi`'ye sayılması; sim'in fizik adımını yapay olarak geciktirince `sim_gecikmesi`; ikisinde de geçersizlik bayrağı |
| K109 | B (sim), S | Tablo 4.6'nın her satırı için tek alanı bozulmuş ama CRC'si doğru çerçeve: ret sayacı, hiçbir etki (yuva boş kalır, olay sonucu dönmez); gönderilmemiş partiye bağlı olay ve `t_decide` uyuşmazlığı |
| K110 | B (kart), K | Yapay olarak durdurulmuş alıcıda (ii) sınıfının hiç atılmaması ve taşmada `SESSION_CLOSE(TX_OVERFLOW)`; (iii)'te en eskiden atma; sürücüde aynı anda bir çerçeve; kritik çıktının önündeki en uzun engellemenin ölçümü (≤ 156 B) |
| K112, K113, K115 | B (sim) | Sentetik yükte sonarın her pencerede en az bir tam çerçeve göndermesi (komut yağmurunda da); 1 200 örnekli satırın ≤ 6 pencerede teslimi; kritik baytın hiçbir yapılandırmada 520 B'ı aşmaması; TF, IMU, emniyet girdisi, daraltıcı ve olay sonucunun kaymaması; çok örnekli sensör yapılandırmasında kayma sırası |
| K114, K102 | SIL | Kamera kapalı, aynı tohum ve aynı kalıcı başlangıç durumuyla iki koşuda nonce, `sess`, `run_id`, `event_id` ve bütün baytların eşitliği; A5 Ek D.7 vektörleriyle karşılaştırma; farklı tohumda nonce'ların farklı olması |
| 3.10 | B (sim), K | Ön denetimin bant hesabı; kartlı 600 s koşuda iki yönün ölçülen ortalama ve en kötü pencere baytı |

### 7.2 Uçtan uca zamanlama ölçümü

Bütçenin (3.9) kalemleri kartlı koşuda ayrı ayrı ölçülür: sim'de `W(t_n)`'den TF yazım başlangıcına (`tf_gec_yazim_ns`), TF yazım başlangıcından `IDA_ACTUATOR`'ın alımına (sim duvarı; gidiş-dönüş), kartta `rx_to_tx_us`; gidiş-dönüşten `rx_to_tx_us` çıkarılınca taşıma payı kalır (iki saat karşılaştırılmaz, A1-K42). Ölçütler: kaçırılan parti sayısı ve en büyük gidiş-dönüş, 30 dakikalık bariyerli koşuda (W4).

### 7.3 Bu işte yapılan doğrulama (makinece okunur tanım)

- **Şema:** `protocol/ida.xml`, pymavlink 2.4.50'nin `mavgen` aracıyla `lxml` yüklüyken üretildi; araç dosyayı kendi `mavschema.xsd` şemasına göre doğruladı ("Validating … ida.xml") ve 37 mesaj buldu.
- **C başlıkları:** `--lang=C --wire-protocol=2.0` ile üretilen başlıklar `cc -std=c17 -pedantic-errors -Wall -Wextra -fsyntax-only` (Apple clang 17.0.0, macOS) ile uyarısız derlendi; `c++ -std=c++20 -Wall -Wextra -fsyntax-only` ile de uyarısız. Üretilmiş başlıklar depoya girmez. **Çapraz uyum turu (U15):** enum adları `IDAW_` önekine geçtikten sonra üretilmiş `ida/mavlink.h` ile A4'ün güncel `core/include/ida/port.h`'i (A4 yazarının çapraz uyum turu çıktısı; A8'in `algilama_kaydi.h`'i ile) aynı çeviri biriminde C17 (`-pedantic-errors`) ve C++20'de uyarısız derlendi; aynı dosyada 21 ayna numaralandırmasının 136 girdisi için `_Static_assert(IDAW_… == IDA_…)` denetimi geçti (A1-K03; karşılığı `port.h`'te bulunmayan ad yok). **Eşleme `protocol/ayna.json`'dadır; ad kuralı mekanik değildir.** Dosya, bu turda kullanılan numaralandırma → `port.h` öneki tablosuyla betikle üretildi ve şunlar denetlendi: (i) K03'te sayılan 21 numaralandırmanın XML'deki her girdisi (`IDAW_REASON`'da 0–15) dosyada tam bir kez; (ii) her `ida` adı `port.h`'te tanımlı (A4 yazarının çapraz uyum sonrası başlığı); (iii) toplam 136; (iv) `_Static_assert` listesi dosyadan üretilip üretilmiş başlık ve `port.h` ile C17 (`-pedantic-errors`) ve C++20'de uyarısız derlendi. Dosya `dialect_hash`'e girmez. Bu denetim W2'de derleme betiğine girer; bu turda yalnız elle koşturuldu.
- **`common.xml` denemesi:** dialect `common.xml`'i içerdiğinde üretilen `common/common.h` aynı bayraklarla dört hata verdi (ISO C, `int` aralığını aşan numaralandırıcıları kısıtlar: `MAV_SYS_STATUS_EXTENSION_USED`, `MAV_SYS_STATUS_SENSOR_ENUM_END`, `HIL_SENSOR_UPDATED_RESET`, `HIL_SENSOR_UPDATED_FLAGS_ENUM_END`); bu yüzden dialect standart dosyaları içermez (3.2).
- **Altın diziler (Ek B):** 37 mesajın her biri için ve ayrıca 1 baytlık tel yükü taşıyan sıfır Ping360 parçası (A1-K51) için, toplam 38 çerçeve, üç bağımsız yolla: (1) bu işte yazılan, yalnız MAVLink serileştirme belgesindeki kurallara (alan sırası, `CRC_EXTRA`, CRC-16/MCRF4XX, kırpma) dayanan standart kitaplık kodlayıcısı; (2) pymavlink'in ürettiği Python dialect'i; ikisi 38 çerçevenin 38'inde bayt bayt aynı. (3) Üretilmiş C başlıklarının `*_encode_status` işlevleriyle 46010, 46030 (kırpılmış son Ping360 parçası), 46030 ek vakası (sıfır parça) ve 46060 kodlandı; dördü de aynı. Düzeltme turu 1'den sonra hepsi yeniden üretildi. Çapraz uyum turunda 38 çerçeve yeniden üretildi ve iki yol yine 38'de 38 aynı; (3)'teki dört C kodlaması da Ek B ile aynı. Enum adlarının değişmesi hiçbir baytı değiştirmez (`CRC_EXTRA` yalnız ileti adına, alan tiplerine ve adlarına bağlıdır); değişen yalnız iki örneğin alan değerleridir: 46045 artık geçerli bir `FC_DISARM` (`tube` = 255, `arg0 = arg1 = 0`), 46004 `RUN_END`. CRC-16/MCRF4XX denetim değeri ("123456789" → 0x6F91) üretilmiş C işleviyle elde edildi ve RevEng katalogundaki değerle aynı.
- **Kitaplığın örnek başına durum işlevleri:** üretilmiş `mavlink_helpers.h`'te `mavlink_frame_char_buffer(rxmsg, status, c, r_message, r_status)` ve `mavlink_finalize_message_buffer(msg, sysid, compid, status, min_len, len, crc_extra)` işlevlerinin durumu çağırandan aldığı kaynaktan okundu; ayrıştırıcının bozuk CRC'de aday çerçeveyi bütünüyle tükettiği, `incompat_flags`'te bilinmeyen bit görünce hemen boşta durumuna döndüğü ve `MAVLINK_CHECK_MESSAGE_LENGTH` tanımlıyken kırpılmış (kısa) yükleri de reddettiği görüldü (bu yüzden A1-K05'in uzunluk denetimi `ida_link`'te yapılır, bu makro tanımlanmaz). W2-protokol uygulaması bu işlevleri kullanmıyor (A1-K101); birden çok örneğin aynı süreçte çalışmasını sınayan iki örnek testi orada yazıldı (bu sözleşme işinde koşturulmadı).
- **`protocol/komutlar.json` (A1-K125):** standart kitaplığın JSON ayrıştırıcısıyla (ve `python3 -m json.tool` ile) geçerli JSON; 19 komut; bir denetim betiği her öğenin anahtarlarını, adın `^[a-z0-9_]{1,64}$` biçimini, adların tekliğini, `onay` = (`sinif` = `genisletici`) eşitliğini, `a1.mesaj` ve `a1.tur` adlarının `ida.xml`'de bulunmasını, bileşik dizilerin eşit boyunu, `FC_FIRE`'ın genişletici ve onaylı olup parametresinin yalnız `tube` 0–1 olmasını ve `HEARTBEAT`'in yokluğunu denetledi; ikinci bir betik bütün `parametreler` ve `sonuc` şemalarını A3-K121'in sınırlarına göre taradı (sınırsız tamsayı, sayı, dizi, dize ya da ek özellikli nesne yok; ondalık sınırlar f32, tamsayı sınırlar ±(2⁵³ − 1) içinde); hata yok. Parametre ve sonuç şemalarının JSON Schema 2020-12 üst şemasına göre doğrulanması **yapılamadı** (makinede `jsonschema` yok; kurulum yapılmadı).
- **`dialect_hash`:** 3.2'deki tanımla standart kitaplıkla, çapraz uyum turunun en son adımı olarak hesaplandı; betik M/F/E satır sayılarını XML'deki mesaj, alan ve numaralandırma girdisi sayılarıyla karşılaştırır.
- **Doğrulanmadı:** `arm-none-eabi-gcc` ile derleme (makinede yok); kartta kodlama ve ayrıştırma maliyeti (MP5); MAVLink belgesindeki bir mesajın en çok 64 alan sınırı (MAVLink XML tanımı) bu dialect'te sağlanıyor (en çok 28 alan, `IDA_BOARD_HELLO`) ama araçla ayrıca sınanmadı.

---

## 8. Açık kalanlar

### 8.1 Ölçüm ya da karar bekleyenler

| # | Konu | Etiket | Kapanacağı yer |
|---|---|---|---|
| O1 | Uçtan uca bütçe (3.9): sim yazım gecikmesi, `C_sim`, USB/ST-LINK ek gecikmesi, kartta dağıtım ve kodlama | Tahmin | W4 HIL köprüsü ve kartlı 30 dakikalık bariyerli koşu (7.2); W3 sonunda gerçek çekirdeğin adım süresi (M5) |
| O2 | Kartta MAVLink ayrıştırma ve kodlama maliyeti ve 1 200 B'lık pencerenin kart alım yoluna yükü | Ölçülecek | MP5 ya da protokol kütüphanesinin ilk sürümü (W2); sonuç adım bütçesini aşarsa yedek biçim (iskelet §8, Ö2) ayrı karardır |
| O3 | MP3'teki 10–14,5 ms'lik seyrek gecikmelerin nedeni ve temiz koşulda sıklığı | Ölçülecek | MP3 temiz koşul yinelemesi |
| O4 | `T_bekle`, `T_kullanım`, `T_bağ`, `T_canlı`, `T_olay_yin`, `T_sonuç_pay`, `apply_by_ns`, `hold_ns` varsayılanları ve izin verilen aralıkları; `B_krit`, `R_s`, `R_c`, `R_iii`, `N_olay` | Tahmin | SIL arıza senaryoları ve kartlı koşu (W4); E1–E19 korunarak |
| O5 | `B_pencere` (%80) ve `B_p360` | Tahmin | Kartlı 600 s koşu (7.1, 3.10 satırı); MP3'te yalnız sabit 256 B çerçeveli %80 yük sınandı |
| O6 | SIL TCP geri döngüsünün gidiş-dönüşü ve koşu hızına etkisi (AB-215) | Ölçülecek | W4 SIL koşucusu |
| O7 | İşletim sisteminin uyku ve USB güç yönetiminin gidiş-dönüşe etkisi; ST-LINK köprüsünün kendi gecikmesi (AB-215) | Ölçülecek | W4 kartlı koşu; MP8 Windows |
| O8 | Kartta TF varışlarının sıkışma sıklığı ve birleştirme oranı (A1-K40) | Ölçülecek | W4 |
| O9 | Ping360 satır hızının gerçekçi üst sınırı (motor ve işlem süreleri 05'te parametre) | A5 / sensör modeli | Sonar modeli uygulaması (W4) ve A5 |
| O10 | DTR/RTS hatlarının ST-LINK V3 sanal seri portunda kartı etkileyip etkilemediği | Doğrulanmadı | Kart yazılımının ilk sürümü (W3); A1-K01 davranış bağlamaz |
| O11 | `IDA_CALL_TRACE`'in girdi tekrarı için yeterliliği (birleştirme ve kapasite düşmelerinde hangi üyenin hangi çağrıya girdiği izde yoktur; sim bunu partinin içeriği ve `n_merged` ile yeniden kurar) | Tahmin | A6 tekrar oynatıcısının ilk sürümü (W5) |
| O12 | Kritik sensör üyelerinin `B_krit` yüzünden en uzun kayma süresi | Güvence yok | Ön denetim (`A_sensör` ≤ 150 B) kuyruğun kararlılığını sağlar; en kötü kayma süresi kanıtlanmadı, `kritik_erteleme_en_buyuk` ile ölçülür (W4) |
| O13 | Bant ölçütü | Kural (A1-K116; mimar kararı 2026-11-20, iskelet §3) | Olağan profilde %19,2 / %23,6; bağlayıcı sınırla %60,0 (tepe %80) / %85,5; kartlı koşuda ölçülür (W4) |

### 8.2 Başka sözleşmelerden beklenenler

Çapraz uyum turunda (W1-uyum) bu liste güncellendi. Karşılandığı bu tur içinde görülen maddeler "karşılandı" diye işaretlidir; atıflar karşı yazarların bu turdaki metnine göredir (A4: A4 yazarının çapraz uyum çıktısı; A5: A5 yazarının çapraz uyum çıktısı); bütünleştirmeden sonra doğrulama incelemesi denetler.

**A4'ten beklenen**

1. Neden aralıkları: 0–15 çekirdeğin, 128–191 A1 bağlantı katmanının (port sonucu olarak üretilmez), 16–127 ve 192 üstü ayrılmış (karşılandı: A4-K120).
2. A4-K66'nın "süresi geçmiş olay uygulanmaz" kuralının `SAFETY_CLOSE` için de geçerli olduğunun bilinçli bir seçim olarak kaydı (A1-K87 bunu izler; izin `hold_ns` ile zaten düştüğü için güvenli yöndedir); değiştirilmek istenirse A1-K84 ve K87 birlikte değişir.
3. Süre aralıkları `apply_by` 16–1 000 ms, `hold` 60–450 ms, `valid_for` 60–450 ms; `T_imu_stale` 5–30 ms (E10) (karşılandı: A4 Bölüm 10 md. 5; U7).
4. Kapalı listeler (U1): `IDA_FC_SELECT`, `IDA_MI_TARGET_ALIGN`, `fc_ref`, `ext_len`, `ext[]`'in kaldırılması; FC `arg0`/`arg1` = 0 (A4-K119: değilse `OUT_OF_RANGE`; A1-K64'ün `LINK_BAD_FIELD` reddi ondan önce gelir); görev öğelerinin A1 Tablo 3.6a'ya atfı; günlük kodlarının (A4 §3.15), `fc.state`'in ve `inhibit_mask`'in (A4 §3.16) kapalı tabloları (karşılandı; A1'in `IDAW_FC_STATE` değerleri §3.16 ile aynıdır, 7.3).
5. SIL'de oturum sonu çağrısı ızgara tick'inden ayrı bir çağrı türüdür ve `now_w`'si son kabul edilen zaman çerçevesinin sim zamanıdır (karşılandı: A4-K122; A1 telde `IDAW_CALL_KIND_SESSION_END` = 4 ile ayırır).
6. Olay tablosu ve kapısı (U12): `N_olay` = 8, izin açan olaylar en çok yedi yuva, `DROPPED_NO_SLOT`'un iki nedeni, `SAFETY_CLOSE`'un hiç düşmemesi ve sekiz yuva doluyken `EVENT_OVERFLOW`; yuvanın `T_sonuç_pay` dolana kadar açık kalması; sonucu beklenen olay varken aynı tüp için yeni izin açan olay ya da `FIRE` üretilmemesi (karşılandı: A4-K67, K106, K123; A1-K80, K120 aynı kapıyı yazar).
7. Izgara tick'i takvimi A1-K117'dir; çağrılar arası en uzun süre TF akarken 10 ms, akmazken 11 ms (karşılandı: A4-K18; A1-K117 ile tutarlı: önceki çağrı en erken son TF'nin alımında başlar, sonraki en geç `E + T_tf_gec` + `IDA_PORT_START_MAX_NS` = son TF + 5 + 1 + 4 ms'de başlar).
8. SIL'de arka plan çağrıları parti başına sabit `N_bg`; oturum sonu çağrısından önce yok (A4-K95; U10).
9. `safety_rev` damgası A4 arayüzünü değiştirmez: platform son verdiği `IDA_SAFETY` örneğinin revizyonunu kendisi tutar ve olay çıkışında A1 mesajına yazar (A1-K106); A4'te değişiklik beklenmez.
10. Emniyet örneklerinin korunması (U5b): A4-K102'nin kapasite düşürmesinin emniyet örneklerine uygulanmaması ve `DUP`/`OLD` pencerelerinden gelen emniyet örneklerinin bir sonraki çağrının girdisinde bulunabilmesi (A1-K31, K124); A4-K116 bunların kısıtlayıcı içeriğini uygular (karşılandı: A4-K102 emniyet örneklerini başa koyar ve düşürmez; A4-K130 taşınan örneği `VALID` = 0 ile işler).
11. Alım damgası: A4-K14, K39 ve K118'in süreleri A1-K48 damgasından ve `Δ_rx` = 1 ms sapma sınırından sayılır (3.1).
12. Sıra numaraları: iskelet §3.1 / A1-K123 kuralı (U8).
13. Alıcıda art arda kapanan partilerin üst sınırı: E21 (A4-K19 ve K102'nin varsayımı).

**A8'den beklenen**

1. Kart → sim yönünde kart içi sonar kayıtlarının parti başlığı olmadan, yalnız kayıtlarla (`IDA_DET_RECORD`) taşınmasının kabulü (A4 `out.det_tx[]` başlık taşımaz; A1-K94).
2. Çalıştırma neslinin her değişimi A1'de oturum kapanışı ve yeni oturumdur (A1-K128; mimar kararı): karta ayrı bir nesil bildirimi gitmez; kartta akış durumunun sıfırlanması oturum sonu çağrısının sonucudur (A4-K122).

**A3'ten beklenen**

0. **Emniyet revizyonu:** A3'ün `emniyet_rev`'i A1'in `safety_rev`'idir ve iskelet §3.2 / A1-K106 tanımını taşır (`u32`, JSON'da 0…4294967295; başlangıç 1; sıfırlanmaz; doyar; artıran altı işlem); A3-K117 aynı listeyi aynen yazar, A3-K95 A1-K85'in beş adımlı sırasını aynen yazar (U2, U3). Gevşeten yer istasyonu komutunun bağlantı emülatöründen çıkarken revizyon denetimiyle atılması A3'ündür (U4a).
1. Tablo 3.11a sayaçlarının, Tablo 3.11b'nin 36 bağlantı sayacının, `kritik_erteleme`, `cagri_izi_eksik`, `sonuc_talebi_atildi` sayaçlarının ve yapılandırma reddinin parametre ve eşitsizlik numarasının adlandırılmış taşıyıcıları (U18); zamanlama geçerliliği nedenlerinin A1-K79'un nedenleriyle bire bir olması.
1a. Sim'in A1 gönderim kuyruğunda bayatlayan genişleticinin A3'e A3-K101/K102'nin `bayat_yetki` sonucuyla bildirilmesi (A1-K129; emülatör çıkışındakiyle aynı sonuç) ve `komut_bayat_yetki` sayacının taşıyıcısı.
2. Panel komutlarının yalnız `protocol/komutlar.json`'dan (A1-K125) okunması, `GET /api/v1/yer_istasyonu/komutlar`'ın bu dosyayı değiştirmeden döndürmesi, nihai yanıtın `veri` nesnesinin `sonuc` şemasına uyması (U16); `komutlar.json`'daki bileşik dizi biçiminin (`a1.mesaj`/`a1.tur` dizi) A3 şemasınca kabulü.
3. Sanal emniyet girdilerinin (acil durdurma, tüp başına fiş ve pim) denetimi; bunların A1 mesajı olmadığı, etkilerinin `IDA_SAFETY` ile karta gittiği (U6).
4. A1'in sayısal `run_id`'sinin metin kimliğinden A1-K126'ya göre türetilmesi.

**A5'ten beklenen**

1. Otopilot parametresi `valid_for_ns` = 100 ms; atış kontrol parametreleri `apply_by_ns` (varsayılan 20 ms) ve `hold_ns` (varsayılan 200 ms); sim parametreleri `T_bekle`, `T_bağ`; hepsi 3.9'daki izin verilen aralıklarla (A1-K104).
1a. Rastgelelik kataloğu: A1-K114'ün etiketleri, `a1/run_id`, `a1/board_tohum` ve değer türetimi A5 Ek D.7 ve A5-K118'dedir (karşılandı; etiket adları A1 ile harfi harfine aynı).
1b. ESC eşlemesinin nötr değerinin ve alt–üst aralığının sim'in doğrulamasında kullanılabilmesi (Tablo 4.6, `IDA_ACTUATOR` V2/V4).
1c. Sim komut kovası (A1-K119) ve bağlantı emülatörü profillerinin birlikte sınanması: emülatörün bıraktığı komutlar kovada bekleyebilir.
1d. Görev öğelerinin A1 Tablo 3.6a alanlarını bire bir taşıması (U1; karşılandı). Tablo 3.6a bu turda değişmedi.
1e. Senaryonun emniyet atamalarının A1-K85'in 2. (kısıtlayan) ve 4. (gevşeten) adımlarına bağlanması (U3; A5-K62/K63).
2. İtici modelinin nötr durumunun "sıfır itki isteği" olarak tanımlanması; ESC "sinyal kaybı" seçeneğinin (son değeri tutma) A1 nötrü için kullanılmaması (A1-K77; AB-001).
3. Sensör hızları ve örnek sayıları, Ping360 tarama yapılandırması (`n_samples` 200–1 200, `sample_period` 80–40 000), bağlantı emülatörü profilleri; ön denetimin 3.10 hesabını bu değerlerle yapabilmesi.
4. Otopilot parametreleri açık (kimlik, değer) çiftleri olarak; SIL koşucusu A4 3.10 bloğunu bunlardan üretir (A1-K18, K19).

**A6'dan beklenen**

1. İki yöndeki bütün A1 çerçevelerinin sim duvar saatiyle alım ve gönderim damgaları, ilgili sim zamanı ve parti atamasıyla kaydı; her bariyer kararının (zamanında, bekleme, kaçırıldı, iptal) ve uygulanan fizik adımının kaydı; Ping360 seyreltme, komut erteleme ve pencere planı kararlarının kaydı; her zaman çerçevesinin gerçek yazım başlangıcı ve bitişi, yalnız `telemetri` ve `tam` kademelerinde (`hil/tf_yazim`; A1-K43; A6-K28).
2. `IDA_CALL_TRACE`'lerin kaydı ve girdi tekrarında kullanılması (`bg_slices` yalnız tutarlılık denetimi; `sil_n_bg` üst verisi; U10).
3. Üst veride iki çalıştırma kimliği (`run_id` metin, `a1_run_id` sayı; A1-K126); şema özeti alanı A1'in tek `dialect_hash`'i, açılış kaydı `IDA_BOARD_HELLO` (U18).
4. Kartlı kayıttan yeniden kurmanın A1-K127'nin girişini kullanması.

**Uygulama işlerinden beklenen**

1. Kart yazılımı (W3): A1-K09 ve K48'in alım yolu ve damga halkası; kesme gecikmesinin ≤ 96 µs olması (`Δ_rx`; ölçülecek); A1-K90'ın gönderim önceliği; A1-K17'nin planlı yeniden başlatması; `last_cmd_id`'nin tutulması; A1-K63 ve K124'ün platform kuralları.
2. HIL köprüsü (W4): pencere planlayıcısı (A1-K33–K39), bariyer (A1-K71–K79), olay uygulayıcısı (A1-K80–K89), sayaçlar (3.11).
3. Fizik çekirdeği (04): A1-K85'teki sıranın fizik adımı sınırına bağlanması; A1-K88'deki fırlatma tetiğinin 04'ün darbe modeline bağlanması.
4. Derleme (W2): üretilmiş başlık ile `port.h`'in aynı çeviri biriminde derlenmesi ve A1-K03 eşitlik denetiminin `protocol/ayna.json`'dan üretilmesi (U15; bu turda betikle koşturuldu, 7.3).

### 8.3 İskelete öneriler

Çapraz uyum notu: 1–4, 7 ve 9 iskelet 0.9 §4'e, 5 ve 8 iskelet 0.10 §3.2'ye ("Emniyet revizyonu", "Fizik sınırındaki sıra") işlendi; 5'in aşağıdaki ilk metni iskeletin beş adımlı sırasıyla değişti ve geçerli metin A1-K85'tir. Kalan öneri yalnız 6'dır.

İskeletin tanımsız bıraktığı ya da yalnız sürekli komut için yazdığı durumlar; bu sözleşme en yalın tutarlı kuralı yazdı, iskelete işlenmesi önerilir:

1. **Kullanım süresinin başlangıcı** (iskelet §4 "Komutun ömrü"): süre, yanıtın sabit uygulama anından (`a_n`) sayılır (A1-K74); geç yanıt yeni ömür kazanmaz çünkü reddedilir.
2. **`THR_VALID = 0` yanıtı:** yuvayı doldurur (parti kaçırılmış sayılmaz) ama yürürlükteki komutu değiştirmez (A1-K74).
3. **Eşzamansız güvenli çıktı:** iskelet §4'ün sabit uygulama anı kuralının tek istisnasıdır; alımdan sonraki ilk fizik adımında uygulanır ve önceki olağan yuvaları iptal eder (A1-K78).
4. **Tek seferlik olayların zamanlaması:** iskelet §4 yalnız sürekli komutun geç kalmasını tanımlar; olaylar için: zamanında gelen olay yuvasıyla aynı adımda, geç gelen olay son geçerliliği içinde alımdan sonraki ilk adımda uygulanır ve çalıştırmayı geçersiz kılar (A1-K83, K79).
5. **Sim'de aynı fizik adımındaki sıra:** emniyet girdisi değişimleri ve izin kapatmaları sürekli komuttan ve izin açan ve atış olaylarından önce (A1-K85); iskelet §3.2 yalnız çekirdek içindeki sırayı yazar.
6. **Parti üyeliği:** üyelik akıştaki konumla belirlenir, üyeler parti numarası taşımaz (A1-K28); iskelet §4 "partinin bittiğini işaretler" ifadesiyle uyumludur.
7. **Sürekli çıktıların üretim sırası:** sim yalnız üretim sayacı büyüyen sürekli çıktıyı uygular; aynı fizik sınırında üretim sırası (A1-K78, K105); iskelet §4'ün yuva kuralına bu tek sıralama ilkesinin eklenmesi önerilir.
8. **Emniyet revizyonu:** izin açan bir olay, karar anındaki emniyet revizyonu hâlâ güncelse uygulanır; araya giren her daraltıcı emniyet değişimi bekleyen yetkileri kalıcı olarak düşürür (A1-K106); iskelet §3.2'nin "izin veren durumlar süreli" ilkesine eklenmesi önerilir.
9. **Oturum sonunun önceliği:** oturum sonunun daraltması, yeni oturumun başlangıcından ve girdilerinden önce ayrı bir çekirdek çağrısında işlenir (A1-K107).

---

## 9. Açık bulgu kapanış tablosu

Kaynak: `docs/mimari/acik-bulgular.md` "A1 — Sim sunucusu ↔ kart" başlığı (49 bulgu: AB-001…AB-047, AB-234, AB-235) ve iskelet §10'un son paragrafında anılan AB-003, AB-211, AB-215, AB-224. Bulgu metinleri değiştirilmedi.

| Bulgu | Önem | Durum | Sözleşmede nerede | Gerekçe |
|---|---|---|---|---|
| AB-001 | kritik | kapandı | A1-K77, K23; 8.2 A5 md. 2 | Nötr durum "sıfır itki isteği" olarak tanımlandı ve ESC sinyal kaybı modeliyle temsil edilmesi yasaklandı; kart sıfırlanması ve oturum kaybı sim'de hemen nötr üretir. |
| AB-002 | kritik | kapandı | A1-K10, K12, K13, K22, K25–K28, K107 | Oturum sonunda kart başlamamış gönderimleri siler, başlamışı tamamlar ya da keser; dönem bilgisi el sıkışma nonce'ları, `sess` ve FIFO akıştaki konumla sağlanır; oturum sonu daraltması, yeni el sıkışmanın yanıtlanmasından ve yeni oturumun girdilerinden önce ayrı bir çağrıda işlenir; sim port açılışında tamponları boşaltır. |
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
| AB-014 | kritik | kapandı | A1-K106, K118, K85–K89, K22, K82 | Tek emniyet sayacı (`uint32`, süreç başında 1, tekdüze, doyan) her kısıtlama, gevşetme, süre dolumu, `SAFETY_CLOSE`, oturum sonu, kart sıfırlanması ve yeni çalıştırmada artar; bekleyen izin açan her olay karar anındaki revizyonla damgalıdır, damga güncel değilse ya da 0 ise kalıcı `REJ_SAFE` alır; olay ayrıca yanıtıyla tutarlı olmalıdır. |
| AB-015 | kritik | kapandı | A1-K85, K78, K86, K88, K106 | Aynı adımda sıra tek anlamlı: emniyet girdisi değişimi, süre dolumu ve `SAFETY_CLOSE` önce (revizyonu artırır), sürekli çıktılar `out_seq` sırasıyla, sonra izin açan olaylar güncel revizyonla. |
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
| AB-027 | kritik | kapandı | A1-K73, K109, K118; Tablo 4.6, 4.6a, 4.6b | Yanıt, çağrının yeni olay sayısını taşır ve olaylar yanıttan önce gelir; geçersiz alanlar, dönüş kodu × bayrak ve olay türü × çağrı türü çelişkileri tek ret ve etkisizlik yolundan reddedilir. |
| AB-028 | kritik | kapandı | A1-K81 | Sim reddedilenler dahil her `event_id`'nin son sonucunu saklar ve yinelemeye aynı sonucu döner. |
| AB-029 | kritik | kapandı | A1-K106, K85–K87, K89 | İzin olaylarının nesli tek emniyet sayacıdır; eski revizyonla damgalı açma daha yeni bir kapatmadan ya da gevşetmeden sonra uygulanamaz; oturum yenilenmesinde platformun revizyonu 0'a çekilir; sayaç doyduğunda bütün izin açan olaylar reddedilir. |
| AB-030 | kritik | kapandı | A1-K49–K51, K112; E12, E14 | Satırın teslim partisi HEAD'in partisidir ve HEAD satırın son çerçevesidir; parçalar bölünmez tam çerçevelerdir, sonarın komutlarca çiğnenmeyen ayrılmış payı her pencerede en az bir çerçeve gönderir; yetim süresi bu sınırdan türetildi; parça uzunluğu sıfırla tamamlamadan sonra denetlenir. |
| AB-031 | önemli | kapandı | A1-K28, K30 | Üye indisi ve bit eşlemi kaldırıldı; üyelik akıştaki konumla, eksiklik `n_members` ile, yineleme çekirdeğin `seq` denetimiyle ele alınır. |
| AB-032 | önemli | kapandı | A1-K31, K99 | Büyük boşluklar `GAP` olarak kabul edilir (üst sınır yok); SIL'de TCP TF kaybetmez. |
| AB-033 | önemli | kapandı | A1-K72, K74, K78 | Ömür sabit uygulama anından sayılır, geç yanıt reddedilir; `THR_VALID = 0`'ın anlamı yazıldı; sürekli çıktılar tek üretim sayacıyla sıralanır. |
| AB-034 | önemli | kapandı | 3.9 (bütçe ilk kalemi), A1-K108, 7.2 | Bütçe `W(t_n)`'den başlar; sim yazım gecikmesi ve sim'in fizik adımı gecikmesi ayrı kalem ve ayrı sayaçtır; planlanan son tarih yuva sonucundan ayrı izlenir. |
| AB-035 | önemli | kapandı | 3.9 (bütçe), A1-K113, K117, O1 | Kritik bayt `B_krit` = 520 B; çakışmasız partilerde tahmini en kötü 9,733 ms (pay 0,267 ms); çakışma tick takvimiyle önlenir, olursa sayılır ve geçersiz kılar (çakışmalı en kötü 10,233 / 13,233 ms). |
| AB-036 | önemli | kapandı | A1-K73, K44, K56, K110, K113; 3.9 | Yukarı yön kritik içeriği (en çok dört yeni olay + yanıt) ve önündeki en uzun engelleme (bir çerçeve) sınırlandı; aşağı yön kritik bayt sınırlı; emniyet geri okuması partide birdir. |
| AB-037 | önemli | kapandı | A1-K33–K39, K110, K112, K115, K119, K120; E17 | Pencere planı bölünmez tam çerçevelerle; taşımaya iki yönde tek çerçeve; zorunlu sınıfın kuyruğu (σ, ρ) geliş ve hizmet eğrileriyle kanıtlandı (≤ 55 çerçeve, `Q_ii` = 64). |
| AB-038 | önemli | kapandı | A1-K11, K13, K27 | Aynı nonce'un yinelenmesi oturumu bitirmez; ilk kart çerçevesi gelmezse sim yeniden dener; kart sıfırlandıktan sonra duyuru gönderir. |
| AB-039 | önemli | kapandı | A1-K12, K15, K25, K27, K28 | Oturum kimliği son oturumlardan ve kartın son oturumundan farklı seçilir; veri mesajlarının oturuma bağlılığı nonce'lu el sıkışma ve FIFO akıştaki konumla sağlanır; karar taşıyan mesajlar ayrıca `sess` taşır. |
| AB-040 | kritik | kapandı | 3.4 (`IDA_SAFETY`), 3.7 (`IDA_EVENT_RESULT`); A1-K56 | Tüp başına izin durumu (`PERMIT_OPEN`) eklendi; bakım girdisi yoktur (iskelet §10 md. 8); olay sonucu `in.evres[]` ile çekirdeğe döner. |
| AB-041 | önemli | kapandı | 3.5; A1-K54 | Algılama mesajları A8'in bütün alanlarını (`t_sigma_ns`, `calib_ver` dahil) taşır; Ping1D yansıma anı. |
| AB-042 | önemli | kapandı | A1-K42; 3.7 | Mutlak kart saati damgası ve saat eşlemesi yoktur; tanı alanları süre olarak taşınır. |
| AB-043 | önemli | kapandı | 3.2 (`dialect_hash`), A1-K14 | Uzantı alanı yoktur; sürüm ve özet tam eşitlik ister. |
| AB-044 | kritik | kapandı | A1-K65–K67 | `cmd_id` sim'de komut başına tektir, yinelemelerde aynıdır, oturumlar arasında kartın son kimliğinden sürer. |
| AB-045 | önemli | kapandı | 3.7 (`IDA_FC_STATUS`, `IDA_CORE_COUNTERS`, `IDA_LINK_STATS`), 3.6 (`IDA_CMD_FC`) | Bütün alanlar ve tipleri tanımlı; atış kontrol komutları kapalı listedir ve argümansızdır, durum emniyet zinciriyle sınırlıdır, anlamı tanımsız genel alan yoktur; `BAD_INPUT` `core_rc` ile taşınır. |
| AB-046 | belirtilmemiş | kapandı | 3.1, 3.2, A1-K101; 7.3 | Kimlikler, aralık, kanonik metin (satır sayısı üç kalemin toplamı olarak yazılı) ve altı eksenli elle kumanda tanımlı; örnek başına durum işlevleri kaynaktan doğrulandı, çok örnekli koşu sınanmadı. |
| AB-047 | belirtilmemiş | kapandı | 3.5, 3.10 | Partide en çok 4 kayıt (A8); bütçe 399 B/kare ile hesaplandı. |
| AB-234 | önemli | kapandı | A1-K11, K12; H31 | Port açılışında tampon boşaltma, nonce eşleşmesine kadar atma, oturumsuz kartın yalnız duyuru göndermesi. |
| AB-235 | kritik | uygulamaya devredildi | 3.9 (bütçe), O1 | A1 200 Hz'e ve A4'ün 3,5 ms'lik adım bütçesine göre yazıldı; gerçek çekirdeğin ölçümü W3 sonunun işidir (M5). |
| AB-211 | önemli | kapandı | 3.9 | USB kısmı K17 ile düştü; sanal seri port için tam kritik yük dahil bütçe `L_act` = 10 ms ile yeniden kuruldu (tahmin; O1). |
| AB-215 | belirtilmemiş | açık | O6, O7 | 400 Hz ve USB önerileri K17 ve iskelet 0.8 ile düştü; SIL geri döngü, işletim sistemi güç yönetimi ve ST-LINK köprüsü gecikmesi ölçümleri bekliyor. |
| AB-224 | belirtilmemiş | uygulamaya devredildi | 7.1 (K22, K23 satırı) | Kablo çekme sınaması kartlı testte elle yapılır; yazılımla kesilebilir USB çoklayıcı seçimi W4 HIL test işinindir. |

Özet (49 + 3 ek bulgu; düzeltme turu 1'den sonra değişmedi, gerekçeler güncellendi): 52 bulgudan 46'sı kapandı, 5'i uygulamaya devredildi (AB-019, AB-023, AB-025, AB-224, AB-235), 1'i açık (AB-215; belirtilmemiş önem). Kapsam dışı yok. Açık kalan kritik bulgu yoktur; AB-235 (kritik) uygulamaya (W3 ölçümü) devredildi.

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

### 9.1 W1 çapraz uyum turu maddeleri (`docs/gorevler/W1-uyum.md`, A1 başlığı)

| Madde | Bulgu | Durum | Sözleşmede nerede |
|---|---|---|---|
| U2 | P1-4, P1-5, P2-İ1, P4-4 | kapandı | A1-K106 (iskelet §3.2'nin altı işlemi aynen; doyma); A1-K89 bağımsız artış tanımlamaz; K86, K87 |
| U3 | P1-6, P1-7, P4-5 | kapandı | A1-K85 (iskelet §3.2'nin beş adımı aynen) |
| U4b | P1-3 | kapandı | A1-K63 (mandala alınan daraltıcı ertelenmiş genişleticileri `NARROWED` ile düşürür); H53 |
| U5b | P1-9 | kısmen | A1-K31 (`DUP`/`OLD` penceresinin `IDA_SAFETY`'si atılmaz), K124 (emniyet örnekleri önce, kapasiteden düşmez), K34 (emniyet örneği 16'lık kapasitenin içinde); E20 sınırı koşulludur (çağrı süresi ve köprü sıkışması sınırlanmadığından zamanlamadan bağımsız kanıt verilemez); aşılırsa örnek atılmaz, oturum kapanır |
| U8 | P1-12 | kapandı | A1-K123; K31, K66 ("üst sınır yoktur" kalktı) |
| U9 | P2-13, P3-6, P4-3 | kapandı | A1-K102, K121 (eşitlik güvencesi kamera kapalıyken) |
| U12 | P2-8, P2-9 | kapandı | A1-K80, K120 (A4-K67 ile aynı kapı); `IDAW_CLOSE_REASON_EVENT_OVERFLOW` = 7; H51 |
| U15 | P2-18 | kapandı | 3.2 (adlandırma); `protocol/ida.xml`; A1-K03; 7.3 (birlikte derleme ve 136 eşitlik denetimi) |
| U16 | P4-13, P4-14 | kapandı | A1-K125; `protocol/komutlar.json`, 19 komut, `fc_fire` onaylı genişletici olarak dahil (mimar kararı; Tablo 3.6 altındaki not); şema üst-doğrulaması yapılamadı, 7.3 |
| U17 | P3-15, P4-20 | kapandı | A1-K126 (`run_id` türetimi), K114 (A5 Ek D.7'ye atıf) |
| + P2-2 | `SET_MODE.arg` | kapandı | Tablo 3.6; A1-K64 (0, 1 ve > 7 `LINK_BAD_FIELD`) |
| + P2-3 | Daraltıcı FC tüp kapsamı, `source` | kapandı | Tablo 3.6 ve "Daraltıcıların kapsamı"; A1-K64 |
| + P2-4 | `ACTUATOR.batch_seq` | kapandı | Tablo 3.7; A1-K44, K45; `IDA_CALL_TRACE.batch_seq` |
| + P2-11 | E10 | kapandı | E10 (`T_imu_stale` 30 ms ile 44,5 < 60) |
| + P2-20 | Damga sapması | kapandı | A1-K48; 3.1 `Δ_rx` (kesme gecikmesi sınırı tahmin, W3'te ölçülür) |
| + P4-18 | PC yeniden kurma girişi | kapandı | A1-K127 |
| + U1 atıfları | `IDA_LOG`, `reason`, `inhibit_mask` | kapandı | 3.7 (A4 §3.15, §3.16, A4-K120); `IDAW_FC_STATE` (A4 §3.16 ile aynı değerler) |
| + §8.2 | A4'ten beklenenler | kapandı | 8.2 |
| Ek karar | Çalıştırma nesli değişimi = yeni oturum | kapandı | A1-K128 (`RUN_END`; yeni kod yok); 8.2 A8 md. 2 |
| Ek karar (A4) | Oturum sonu çağrısının çağrı izinde ayrılması | kapandı | `IDAW_CALL_KIND_SESSION_END` = 4; A1-K95 |

### 9.2 W1 doğrulama incelemesi bulguları (sol, parça 1–2; A1'e düşenler)

| Bulgu | Durum | Sözleşmede nerede |
|---|---|---|
| P1-1 Sim kuyruğunda bekleyen genişletici | kapandı | A1-K129 (gönderimden hemen önce revizyon denetimi; üç bekleme noktası); 3.11a `komut_bayat_yetki` |
| P1-2 K66 uzlaştırması | kapandı | A1-K66 (seçim doğrudan `last_cmd_id` penceresiyle; ilk komutta da 0 atlanır) |
| P1-3 = P2-Y2 E20 en küçük pencere | kapandı | E20 (en küçük tel boyu kuralı; 29 + 25 = 54 B; 2,70 ms); K124/H52 aynı |
| P1-4 Taşınan emniyet örneği | kapandı | A1-K31, K124 (A4-K130: `VALID` = 0 ile teslim; gevşetme yalnız kabul edilen partiden; düşen gevşetme `T_emniyet` ile yeniden gelir) |
| P1-6 = P2-Y7 Kaldırılmış alanlar, eski atıflar | kapandı | Küçük turda (`99f5713`): Ek A satırları ve §3.7 cümleleri; bu turda K122 ve 7.1'deki `TARGET_ALIGN` anlatımı ve §8.2 A4 md. 10 |
| P2-Y1 Hata yanıtı ↔ olay bağı | kapandı | A1-K73, K84, Tablo 4.6 V3 ve 4.6b notu |
| P2-Y5 Eski aralıklar | kapandı | Tablo 3.7 `valid_for_ns`, 3.9 `apply_by_ns` dayanağı (tek kaynak A1-K104) |
| P2-Y6 Oturum sonu izinin gönderimi | kapandı | A1-K11, K22, K95, K127 |
| P4-Y9 Komut kataloğu | kapandı | A1-K125 (yinelenen öğe tablosu; A4 aralıklarıyla eşitlik), `komutlar.json` `guided_sp.depth`/`altitude` ≥ 0 |
| P3-D8 Rastgelelik etiketlerinin parametreleri | kapandı | A1-K114 (tutan, ilk değer, artırılma olayı, ömür; A5 Ek D.7 vektörleriyle tutarlılık; K102 koşulu) |
| P4-Y8 TF yazım zamanlarının kademesi | kapandı | A1-K43, §6, §8.2 A6 md. 1 |
| P2-M1 (A1 payı) E9 | kapandı | E9, 3.9 `hold_ns` satırı (yeterli koşul; sonuç gecikirse izin güvenli biçimde düşer) |

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
| `ida_gnss_t` (`lat_e7`, `lon_e7`, `alt_mm`, `vel_ned`, `eph`, `epv`, `fix_type`, `n_sv`) | `IDA_GNSS` (aynı adlar; `fix_type` `IDAW_GNSS_FIX`) |
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
| `ida_event_t` (`event_id`, `kind`, `tube`, `seq`, `t_decide`, `apply_by_ns`, `hold_ns`) | `IDA_EVENT` (`seq` → `batch_seq`, `t_decide` → `t_decide_ns`); platform ayrıca `call_kind` ve `safety_rev` yazar (A1-K106) |
| `ida_cmd_result_t` (`cmd_id`, `kind`, `ack`, `reason`, `value`, `save.gen`, `save.len`) | `IDA_CMD_RESULT`; `save.blob` telde yoktur (`save_crc` ile özetlenir; A1-K68) |
| `ida_nav_out_t` (14 alan) | `IDA_NAV` |
| `ida_status_t` (`mode`, `armed`, `fs_level`, `fs_reasons`, `regime`, `phase`, `mission_state`, `mission_item`, `param_ok`, `events_ok`, `boot_count`) | `IDA_STATUS` |
| `ida_status_t.cnt.*` (22 alan) | `IDA_CORE_COUNTERS`'ın aynı adlı 22 alanı (3.7) |
| `ida_health_t` (`core_ok`, `data_ok`, `imu_rejected`, `imu_lag`, `input_dropped`, `narrowed`, `sample_conflict`, `bg_bad_input`) | `IDA_STATUS.health` bit 0–7 (3.7); ayrıca `IDA_ACTUATOR` `INPUT_DROPPED`, `NARROWED` |
| `ida_log_t` (`t_w`, `code`, `severity`, `arg0`, `arg1`) | `IDA_LOG` |
| `ida_fc_status_t` (`state`, `inhibit_mask`, `last_event_id`, `last_event_result`) | `IDA_FC_STATUS` |
| `ida_mission_item_t.p[4]` | Tablo 3.6a'daki tür başına anlam, birim ve aralık |
| `ida_fc_cmd_t.arg0`, `.arg1` | `IDA_CMD_FC.arg0/arg1`; ayrılmış, 0 (A1-K64) |
| `ida_init_in_t` (`boot_count`, `boot_count_ok`, `reset_cause`, `reset_flags`, `consecutive_faults`, `param_src`) | `IDA_BOARD_HELLO` |
| `ida_core_info_t` (`port_major`, `port_minor`, `param_schema_hash`, `core_version`) | `IDA_BOARD_HELLO` |
| Dönüş kodları (`OK`, `IMU_REJECTED`, `DUP_SEQ`, `OLD_SEQ`, `BAD_INPUT`, `NOT_INIT`) | `IDA_ACTUATOR.core_rc`, `IDA_CALL_TRACE.core_rc` (`DUP_SEQ`/`OLD_SEQ` olağanda görülmez; A1-K31) |
| `ida_core_step`, `ida_core_tick` | A1-K32, K44 |
| `ida_core_background` | Çağrı sayısı `bg_slices` ile izlenir; `IDA_BG_BAD_INPUT` dönen çağrı sayılmaz (A4-K117) |
| `ida_core_param_count`, `_desc`, `_find` | A1'de karşılığı yoktur: parametre tablosu A1'de taşınmaz; sim aynı kaynaktan derlenen tabloyu kullanır ve `param_schema_hash`'le doğrular (A1-K14) |
| `ida_core_param_get` | A1'de doğrudan karşılığı yoktur; değer `IDA_CMD_SIMPLE(PARAM_GET)` komutunun sonucuyla gelir (A4-K43) |

## Ek B. Altın bayt dizileri

Her özel mesaj için bir çerçeve ve sonda A1-K51'in ek vakası (1 baytlık tel yükü taşıyan sıfır Ping360 parçası); toplam 38 çerçeve. Alan değerleri belirlenimci bir desenle seçilmiştir (birkaç mesajda anlamlı değerler); sim'den çıkan çerçevelerde `sysid = 250`, `compid = 25`, karttan çıkanlarda `1`, `1`. Bütün diziler 7.3'teki bağımsız kodlayıcı ve pymavlink üreteciyle bayt bayt aynıdır; 46010, 46030, 46030 ek vakası ve 46060 ayrıca üretilmiş C başlıklarıyla doğrulandı. Diziler `protocol/ida.xml`'in bu adayına aittir; XML değişirse yeniden üretilir.

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

- Alanlar: sess = 6699; reason = 5

```
fd 03 00 00 04 fa 19 b4 b3 00 2b 1a 05 85 f0
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

- Başlık: seq = 14, sysid = 250, compid = 25; CRC_EXTRA = 93; yük azami 22 B, çerçeve azami 34 B; bu dizide yük 22 B, çerçeve 34 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_meas_ns = 1000000000; seq = 100011; flags = 1006; inst = 16; estop = 21; tube = [26, 27]; safety_rev = 100066

```
fd 16 00 00 0e fa 19 cc b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 e2 86 01 00 ee 03 10 15 1a 1b
50 f6
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

- Alanlar: cmd_id = 1002; arg0 = 0; arg1 = 0; sess = 6699; op = 2; tube = 255; source = 255

```
fd 11 00 00 18 fa 19 dd b3 00 ea 03 00 00 00 00 00 00 00 00 00 00 2b 1a 02 ff ff 98 e4
```

### 46046 IDA_EVENT_RESULT

- Başlık: seq = 25, sysid = 250, compid = 25; CRC_EXTRA = 144; yük azami 19 B, çerçeve azami 31 B; bu dizide yük 19 B, çerçeve 31 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: event_id = 30064771072; t_apply_ns = 1005000000; sess = 1006; result = 16

```
fd 13 00 00 19 fa 19 de b3 00 00 00 00 00 07 00 00 00 40 15 e7 3b 00 00 00 00 ee 03 10 de a1
```

### 46060 IDA_ACTUATOR

- Başlık: seq = 26, sysid = 1, compid = 1; CRC_EXTRA = 145; yük azami 38 B, çerçeve azami 50 B; bu dizide yük 34 B, çerçeve 46 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: batch_seq = 2469; valid_for_ns = 100000000; pwm_us = [1500, 1520, 1480, 1500, 1550, 1450, 1500, 1500]; sess = 6699; act_flags = 5; rx_to_tx_us = 3210; fs_level = 0; core_rc = 0; n_events = 0; n_merged = 0; out_seq = 1234

```
fd 22 00 00 1a 01 01 ec b3 00 a5 09 00 00 00 e1 f5 05 d2 04 00 00 dc 05 f0 05 c8 05 dc 05 0e 06
aa 05 dc 05 dc 05 2b 1a 05 00 8a 0c f0 16
```

### 46061 IDA_EVENT

- Başlık: seq = 27, sysid = 1, compid = 1; CRC_EXTRA = 124; yük azami 37 B, çerçeve azami 49 B; bu dizide yük 37 B, çerçeve 49 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: event_id = 21474836483; t_decide_ns = 12345000000; batch_seq = 2469; apply_by_ns = 20000000; hold_ns = 200000000; safety_rev = 7; sess = 6699; kind = 1; tube = 0; call_kind = 46

```
fd 25 00 00 1b 01 01 ed b3 00 03 00 00 00 05 00 00 00 40 c0 d1 df 02 00 00 00 a5 09 00 00 00 2d
31 01 00 c2 eb 0b 07 00 00 00 2b 1a 01 00 2e 76 1d
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

- Başlık: seq = 31, sysid = 1, compid = 1; CRC_EXTRA = 46; yük azami 14 B, çerçeve azami 26 B; bu dizide yük 14 B, çerçeve 26 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: last_event_id = 30064771072; inhibit_mask = 100011; state = 11; last_event_result = 16

```
fd 0e 00 00 1f 01 01 f1 b3 00 00 00 00 00 07 00 00 00 ab 86 01 00 0b 10 15 c7
```

### 46066 IDA_LOG

- Başlık: seq = 32, sysid = 1, compid = 1; CRC_EXTRA = 56; yük azami 19 B, çerçeve azami 31 B; bu dizide yük 19 B, çerçeve 31 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: t_w_ns = 1000000000; arg0 = -1007; arg1 = -1014; code = 1009; severity = 21

```
fd 13 00 00 20 01 01 f2 b3 00 00 ca 9a 3b 00 00 00 00 11 fc ff ff 0a fc ff ff f1 03 15 9d 6b
```

### 46067 IDA_CORE_COUNTERS

- Başlık: seq = 33, sysid = 1, compid = 1; CRC_EXTRA = 23; yük azami 88 B, çerçeve azami 100 B; bu dizide yük 87 B, çerçeve 99 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: sample_rejected = 100000; sample_future = 100011; sample_conflict = 100022; sample_dup_dropped = 100033; narrow_from_rejected = 100044; bg_bad_input = 100055; imu_rejected = 100066; imu_gap = 100077; bad_input = 100088; dup_seq = 100099; old_seq = 100110; wall_backstep = 100121; cmd_rejected = 100132; widen_blocked = 100143; evres_ignored = 100154; event_unknown = 100165; det_rejected = 100176; ping360_rejected = 100187; bg_dropped = 100198; bg_slice_mismatch = 100209; log_dropped = 100220; step_miss = 100231

```
fd 57 00 00 21 01 01 f3 b3 00 a0 86 01 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 cc 86 01 00 d7 86
01 00 e2 86 01 00 ed 86 01 00 f8 86 01 00 03 87 01 00 0e 87 01 00 19 87 01 00 24 87 01 00 2f 87
01 00 3a 87 01 00 45 87 01 00 50 87 01 00 5b 87 01 00 66 87 01 00 71 87 01 00 7c 87 01 00 87 87
01 5d de
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

- Başlık: seq = 35, sysid = 1, compid = 1; CRC_EXTRA = 182; yük azami 137 B, çerçeve azami 149 B; bu dizide yük 137 B, çerçeve 149 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: now_w_ns = 1000000000; batch_seq = 100011; bg_slices = 100022; prev_call_ns = 100033; rx_age_ns = [100044, 100045, 100046, 100047, 100048, 100049, … (28 öğe)]; n_rx = 26; call_kind = 31; core_rc = 36; plat_flags = 41; n_merged = 46

```
fd 89 00 00 23 01 01 f5 b3 00 00 ca 9a 3b 00 00 00 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 cc 86
01 00 cd 86 01 00 ce 86 01 00 cf 86 01 00 d0 86 01 00 d1 86 01 00 d2 86 01 00 d3 86 01 00 d4 86
01 00 d5 86 01 00 d6 86 01 00 d7 86 01 00 d8 86 01 00 d9 86 01 00 da 86 01 00 db 86 01 00 dc 86
01 00 dd 86 01 00 de 86 01 00 df 86 01 00 e0 86 01 00 e1 86 01 00 e2 86 01 00 e3 86 01 00 e4 86
01 00 e5 86 01 00 e6 86 01 00 e7 86 01 00 1a 1f 24 29 2e 05 e6
```

### 46070 IDA_FAULT_RECORD

- Başlık: seq = 36, sysid = 1, compid = 1; CRC_EXTRA = 89; yük azami 35 B, çerçeve azami 47 B; bu dizide yük 35 B, çerçeve 47 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: cfsr = 100000; hfsr = 100011; mmfar = 100022; bfar = 100033; pc = 100044; lr = 100055; xpsr = 100066; step_counter = 100077; fault_seq = 1024; stacking_valid = 46

```
fd 23 00 00 24 01 01 f6 b3 00 a0 86 01 00 ab 86 01 00 b6 86 01 00 c1 86 01 00 cc 86 01 00 d7 86
01 00 e2 86 01 00 ed 86 01 00 00 04 2e 5d 4c
```

### 46030 IDA_PING360_DATA — Ek vaka (A1-K51): sıfır profil parçası; tel yükü 1 bayta kırpılır ve geçerlidir

- Başlık: seq = 37, sysid = 250, compid = 25; CRC_EXTRA = 174; yük azami 246 B, çerçeve azami 258 B; bu dizide yük 1 B, çerçeve 13 B. Doğrulama: bağımsız kodlayıcı ve pymavlink üreteci bayt bayt aynı.

- Alanlar: seq = 1; inst = 0; frag_idx = 0; data = [0, 0, 0, 0, 0, 0, … (240 öğe)]

```
fd 01 00 00 25 fa 19 ce b3 00 01 dc a6
```

## Ek C. Başka sözleşmelerin "A1'den beklenen" listelerinin karşılanması

**A4 "A1'den beklenen"** (A4 yazarının çapraz uyum çıktısındaki Bölüm 10 numaralarıyla)

| # | Beklenen | Karşılığı |
|---|---|---|
| 1 | Ek A'daki alanların eşlenmesi; TF'nin `t_rx`'i | Ek A; A1-K48 |
| 2 | Tüp başına `PERMIT_OPEN`, `PLUG_IN`, `PIN_OUT`, `LOADED`, `estop` | `IDA_SAFETY`; A1-K56, K89 |
| 3 | Olay alanları ve alıcı kodları (`fc_ref` yok) | `IDA_EVENT`, `IDA_EVENT_RESULT`; A1-K81–K88 |
| 4 | Alıcıda iznin sim zamanında `hold_ns` | A1-K86, K89 |
| 5 | Süre aralıkları | 3.9; A1-K104 |
| 6 | Ping360 kutu zamanı | A1-K52 |
| 7 | Hangi dönüş kodunda hangi yanıt | A1-K31, K44, K45 (`OLD`/`DUP`'ta çağrı ve yanıt yok; hata dönüşleri `THR_VALID = 0` ile ve `batch_seq` = çağrının `tf.seq`'i ile gider; `IMU_REJECTED` olağan yanıt; ızgara tick'i yalnız `thr_valid = 1` iken) |
| 8 | Alıcıda art arda kapanan partilere üst sınır (sayıyla) | E21; A1-K40 (gönderen aralığı korunursa `τ` ms'de ≤ 1 + ⌊τ/2,5⌋, 5 ms'de 3; koşulsuz hat hızı sınırı ≤ 1 + ⌊τ·300/29⌋, 5 ms'de 52); gönderen tarafı A1-K39, E11 |
| 9 | Birleştirilen partilerin yanıtı | A1-K40, K75 (`n_merged`; `parti_kacirildi_birlesik`) |
| 10 | Komut çevirisi, `cmd_id`, birleştirilen daraltıcıların yanıtı, parametre protokolü, GNSS fix | 3.6; A1-K63–K68, K123; `COALESCED` sonucu; fix türü A4 değerleriyle |
| 11 | `plat` sayaçlarının kaynağı | Tablo 3.11b; A1-K30, K31 |
| 12 | Olay yuvasının ömrü, `N_olay` = 8, yedi + bir yuva, `EVENT_OVERFLOW` = 7 | A1-K80, K120; 3.9 (`T_olay_yin`, `T_sonuç_pay`; E7, E8); `IDAW_CLOSE_REASON_EVENT_OVERFLOW` |
| 13 | Yolda bekleyen genişletici (U4b) | A1-K63 (ertelenmiş genişleticiler atılır, her birine `REJECTED` / `NARROWED`) |
| 14 | Alım damgasının en büyük sapması (sayıyla) | A1-K48; 3.1 `Δ_rx`: en çok 0,86 ms erken, 0,1 ms geç; `|sapma|` ≤ 1 ms |
| 15 | Oturum sonu çağrısının çağrı izinde ayrılması | `IDAW_CALL_KIND_SESSION_END` = 4 (3.7 `IDA_CALL_TRACE`; A1-K95) |
| 16 | Kısıtlayıcı emniyet örneği (U5b) | A1-K31, K124; E20 |
| 17 | Kapalı tablolara atıf; FC `arg0`/`arg1` = 0; `tube` = 255 yalnız `FC_DISARM`/`FC_ABORT`'ta | 3.7 (`IDA_LOG` → A4 §3.15; `IDA_FC_STATUS` → A4 §3.16; `IDA_CMD_RESULT.reason` → A4-K120); Tablo 3.6; A1-K64 |

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
| Makinece okunur komut listesi ve sınıfları | `protocol/komutlar.json` (A1-K125; U16); sınıflar Tablo 3.6 ve `protocol/ida.xml` `[sinif: …]` (A1-K60) |
| Bariyer kararlarının ve sayaçların tanımı | A1-K72–K79; Tablo 3.11a |
| Olay kimliğinin bit düzeni | 3.7 (`IDA_EVENT`) |
| Bekleme sınırı ve komut kullanım süresi | 3.9 (`T_bekle` varsayılan 30 ms, 10–35 ms; `T_kullanım` = `valid_for` varsayılan 100 ms, 60–450 ms) |
| `bg_slices`'ın taşınması | `IDA_CALL_TRACE` (A1-K95) |

**A5'in beklediği (görev mesajındaki liste)**

| Beklenen | Karşılığı |
|---|---|
| Sürekli komutun kullanım süresi | 100 ms (3.9) |
| Kartlı kipte otopilot parametre bloğunun yüklenmesi ya da karşılaştırılması | A1-K18, K19 (CRC karşılaştırması; hazırlık oturumunda eşitleme) |
| Hata enjeksiyonu ve yer istasyonu komutlarında kullanılacak mesaj adları | 5.1 (`IDA_*` mesaj adları); Tablo 3.6 |
| `--a1-seed`'in `a1/board_tohum`'dan, SIL `run_id`'sinin `a1/run_id`'den türetildiğinin yazılması (A5 Ek C md. 8) | A1-K114, K126 |
| Görev öğesi alanları ve koşulları | Tablo 3.6a, A1-K122 (bu turda değişmedi) |
