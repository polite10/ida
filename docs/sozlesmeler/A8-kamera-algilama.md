# A8 — Algılama kaydı ve sim sunucusu ↔ kamera algılama bileşeni sözleşmesi

| | |
|---|---|
| Sürüm | 1.0 (donduruldu, 2026-11-21) |
| Durum | **Donduruldu (2026-11-21; kullanıcı kararı K20).** Fark incelemesi turunun (D-17…D-27) düzeltmeleri sol'a yeniden incelettirilmedi; kalan hatalar uygulama sırasında bulunur ve küçük sürüm artışıyla (1.0.x) düzeltilir. |
| Görev | `docs/gorevler/W1-A8.md`; ortak kurallar `docs/gorevler/W1-ortak.md` |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.10 (§3 A8 satırı, "Algılama kaydının çerçevesi", "Kamera ile hedef algılama"; §3.1; §4; §5 "Bilinen sınır"; §6; §8; §10 md. 6, 21, 25) |
| Makinece okunur tanım | `core/include/ida/algilama_kaydi.h` (ISO C17; C++20 ile de derlenir) |
| Yazan | `ida-uygulayici` (Opus); inceleyen sol (`docs/sozlesmeler/inceleme/A8-sol.md`; yanıt `A8-yanit.md`) |

Bu belge kural yazar. Mimari bölümlerdeki (03, 05, 07) bir şey buraya girmediyse A8 için bağlayıcı değildir. Çelişkide öncelik: kullanıcı kararları > iskelet 0.10 > ölçüm raporları ve görev belgeleri > bölümler 01–09.

Sayıların niteliği: her sayının yanında dayanağı yazar. **Hesap** aritmetiktir, ölçüm değildir. **Mimari seçimi (tahmin)** bu sözleşmenin seçtiği başlangıç değeridir; ölçülmemiştir ve gösterilen işle yeniden değerlendirilir.

---

## 1. Kapsam ve taraflar

Sözleşme iki şeyi sabitler:

1. **Algılama kaydı:** bir sensörün "şu anda, sensör çerçevesinde şu yönde ve varsa şu mesafede, şu belirsizlikle bir nesne gördüm" bilgisini taşıyan ortak veri yapısı (`ida_det_record_t`) ve kayıtları bir girdi birimine (kamera karesi ya da sonar tarama satırı) bağlayan parti başlığı (`ida_det_batch_t`). Kart içinde sonar verisinden üretilen kayıt ile PC'deki kamera algılama bileşeninin ürettiği kayıt aynı biçimdedir (iskelet §3).
2. **A8 bağlantısı:** sim sunucusu ile PC'de ayrı süreçte çalışan kamera algılama bileşeni arasındaki bağlantı. Kareler paylaşımlı bellekten (A2'nin kare halkası) okunur, kayıtlar yerel soketten sim'e döner; sim kayıtları A1 üzerinden karta iletir.

**Taraflar**

| Taraf | Rolü |
|---|---|
| Sim sunucusu (C++20) | A8'in sunucusu; kare halkasının sahibi (A2); bileşene kare sunar, sonucu doğrular, karta A1 üzerinden iletir; durumları ve retleri sayar (A3) ve kaydeder (A6) |
| Kamera algılama bileşeni (C++20, ayrı süreç; bundan sonra "bileşen") | A8'in istemcisi; kareyi salt okunur okur, algılama kaydı üretir |
| Kart yazılımı / SIL koşucusu (çekirdek, C17) | Kaydın tüketicisi (A1 → A4 üzerinden); kart içi sonar algılamasında kaydın üreticisi |
| Görselleştirici (Godot) | Kareyi üretir ve halkaya yazar (A2); A8'in tarafı değildir |

**Kapsam dışı:** algılama algoritmasının içi, hedef sınıflandırma başarımı, takip, atış ölçütü ve atış kararı; kamera karesinin biçimi, halkanın düzeni ve kamera kademe değerleri (A2); kaydın A1 üzerindeki mesaj biçimi ve parti ataması (A1); kaydın çekirdeğe teslim biçimi ve alım zamanı (A4); senaryo parametrelerinin dosya biçimi (A5); kayıt dosyası (A6). Bunlara atıf verilir; beklenenler 8.2'de listelidir.

**Bu sürümde tek kamera vardır** (iskelet §10 md. 25): A8 oturumu tam bir kamera taşır (K35).

## 2. Terimler ve kabuller

Birimler, çerçeveler, kuaterniyon ve iki zaman kabulü iskelet §3.1'dir; burada yinelenmez. Bu sözleşmeye özgü olanlar:

| Terim | Tanım |
|---|---|
| Sensör çerçevesi (S) | Kaydı üreten sensöre sabit, sağ elli çerçeve: x ekseni sensörün bakış ekseni (kamerada optik eksen, ileri), y sağa, z aşağı. Gövdeye göre konumu ve yönelimi montajdır (`l_b`, `q_bs`; iskelet §3.1 gövde referans noktasına göre). Kamerada optik çerçeveden (z ileri, x sağ, y aşağı) S'ye çeviri bileşenin işidir |
| Yön açısı `az` | `az = atan2(y, x)`; aralık [−π, π]; +y yönüne (sağa) pozitif |
| Yükseliş `el` | `el = atan2(−z, √(x² + y²))`; aralık [−π/2, π/2]; bakış düzleminin üstüne (−z yönüne) pozitif |
| Mesafe `r` | S'nin orijininden nesneye eğik uzaklık, m; `r > 0` |
| Birim yön | `u = (cos el · cos az, cos el · sin az, −sin el)` (S'de) |
| Akış | Aynı (`source`, `inst`) çiftinden gelen kayıtlar ve partiler |
| Girdi birimi | Bir partinin işlediği veri: kamerada tek kare; sonarda tek tarama satırı (bir açıdaki menzil profili) |
| Parti | Bir girdi biriminin sonucu: bir parti başlığı ve 0–4 kayıt |
| `t_valid` | Kaydın tek geçerlilik anı: `t_valid = t_meas_ns − (window_ns / 2)` (işaretsiz tamsayı bölmesi, aşağı yuvarlar). Bölüm 03'teki `t_capture` budur |
| `t_teslim_k` | Kare `k`'nın modellenmiş teslim anı (sim zamanı; A2 isteğinde taşınır; bölüm 05 §2.5) |
| `L_alg` | Algılama sonucunun karta ulaşmasının modellenmiş ek gecikmesi (sim zamanı; A5 parametresi; 6.1) |
| `t_alg_k` | `t_teslim_k + L_alg` |
| `P_k` | Kapanış anı `t_alg_k`'ya eşit ya da ondan sonra olan ilk A1 partisi (iskelet §4 "adım partisi") |
| `M_k` | Sim'in `P_k`'yı oluşturmaya başladığı an (sim'in duvar saatinde) |
| Duvar saati | Her sürecin kendi işletim sistemi monoton saati. Süreçler arası saat karşılaştırması bu sözleşmede yapılmaz; her taraf süreleri yalnız kendi saatiyle ölçer |
| Parti reddi | Partinin başlığının ve bütün kayıtlarının kullanılmaması ve reddin tek bir neden koduyla sayılması (K27) |

## 3. Veri tanımları

### 3.1 Genel

- Tamsayı ve kayan noktalı alanların anlamı aşağıdaki tablolardadır. `f32` IEEE 754 binary32'dir. Başlık dosyasındaki yapı düzeni bir taşıma biçimi değildir; A8 soketindeki bayt düzeni 3.5'te, A1'deki düzen A1 sözleşmesindedir.
- Bir alanın geçersizliğinin sonucu her zaman **bütün partinin reddidir**; hangi denetimin hangi neden koduyla hangi sırada yapıldığı 4.6'daki tek tablodadır.

### 3.2 Algılama kaydı: `ida_det_record_t` (80 bayt; kodlanmış en çok 78 bayt)

| # | Alan | Tip | Birim | Geçerli aralık | Anlamı | Ret nedeni (4.6) |
|---|---|---|---|---|---|---|
| 1 | `t_meas_ns` | i64 | ns (sim zamanı) | 0 … 2⁶² (K81); K12, K13 | Ölçüm zamanı (iskelet §3.1; K10) | `TIME` |
| 2 | `det_seq` | u32 | — | akışta öncekinden büyük | Kayıt kimliği (K15) | `SEQ` |
| 3 | `batch_seq` | u32 | — | başlığın `batch_seq`'i | Ait olduğu parti (K19) | `LINK` |
| 4 | `window_ns` | u32 | ns | kamerada başlığınki; sonarda 0 | Ölçüm aralığının süresi; anlık ölçümde 0 (K10) | `TIME` |
| 5 | `t_sigma_ns` | u32 | ns | 0 … 1 000 000 000 | `t_valid`'in belirsizliği, 1σ | `VALUE` |
| 6 | `z[0]` = `r` | f32 | m | `kind` RB2/RB3: (0, 1000]; B2: tam 0 | Mesafe | `VALUE` |
| 7 | `z[1]` = `az` | f32 | rad | [−π₃₂, π₃₂] | Yön açısı | `VALUE` |
| 8 | `z[2]` = `el` | f32 | rad | `kind` B2/RB3: [−π₃₂/2, π₃₂/2]; RB2: tam 0 | Yükseliş | `VALUE` |
| 9 | `chol[6]` | f32 | satır `r`: m; satır `az`, `el`: rad | K07 | `(r, az, el)` ölçüm kovaryansının alt üçgen Cholesky çarpanı `L`, satır sırasıyla `[L_rr, L_ar, L_aa, L_er, L_ea, L_ee]`; kovaryans `L·Lᵀ` (K09) | `UNCERT` |
| 10 | `confidence` | f32 | — | [0, 1] | Üreticinin sıralayıcı güven puanı (K08) | `VALUE` |
| 11 | `calib_ver` | u16 | — | başlığınki | Üreticinin kullandığı iç kalibrasyonun ve montajın sürümü (K20) | `CALIB` |
| 12 | `producer_ver` | u16 | — | başlığınki | Üretici algoritmanın sürümü | `LINK` |
| 13 | `flags` | u16 | bit | K21, K22 | 3.4'teki bayraklar | `FLAGS` |
| 14 | `source` | u8 | — | başlığınki | Kaynak sensör (algılayıcının yeri değil) | `LINK` |
| 15 | `inst` | u8 | — | başlığınki | Sensör örneği; kamerada `camera_id` | `LINK` |
| 16 | `kind` | u8 | — | K05 | Hangi `z` bileşenlerinin ölçüldüğü (K03–K05) | `KIND` |
| 17 | `frame` | u8 | — | 1 (SENSOR) | Ölçüm çerçevesi (K01) | `KIND` |
| 18 | `class_id` | u8 | — | 0 … 3 (A5'in kapalı tablosu; K23) | Algılanan nesnenin kaba türü; 0 = bilinmiyor | `VALUE` |
| 19 | `producer_id` | u8 | — | başlığınki; ≥ 1 | Üretici algoritmanın kimliği (K24) | `LINK` |
| 20 | `batch_index` | u8 | — | kaydın partideki sırası | Partideki sıra, 0'dan (K18) | `LINK` |
| 21 | `batch_count` | u8 | — | başlığın `count`'u | Partideki kayıt sayısı (K18) | `LINK` |
| 22 | `reserved[2]` | u8 | — | 0 | Taşınmaz | `STRUCT` (yalnız A8'de görünür) |

`π₃₂`, π'ye en yakın binary32 değeridir (3,14159274…; başlıkta `IDA_DET_PI_F32`). Açı denetimi kapalı aralıkla yapılır; `az = −π₃₂` ile `az = π₃₂` aynı yönü gösterir ve ikisi de geçerlidir.

Mesafe üst sınırı 1000 m ve süre üst sınırı 1 s **mimari seçimidir (tahmin)**; yalnız bozuk değeri ayıklamak içindir, sensör menzili değildir.

**`chol` alanının geçerliliği** (K07; sınırlar mimari seçimi, tahmin):

| Eleman | Birim | Ölçülen bileşenin köşegeni | Ölçülmeyen bileşenin satırı ve sütunu | Bütün elemanlar |
|---|---|---|---|---|
| Satır `r`: `L_rr` | m | ≥ 1,0·10⁻³ | tam 0 | sonlu; mutlak değer ≤ 1000 |
| Satır `az`: `L_ar`, `L_aa` | rad | `L_aa` ≥ 1,0·10⁻⁶ | tam 0 | sonlu; mutlak değer ≤ 10 |
| Satır `el`: `L_er`, `L_ea`, `L_ee` | rad | `L_ee` ≥ 1,0·10⁻⁶ | tam 0 | sonlu; mutlak değer ≤ 10 |

Ölçülmeyen bileşen `kind`'dan çıkar: RB2'de `el` (dolayısıyla `L_er`, `L_ea`, `L_ee` = 0), B2'de `r` (dolayısıyla `L_rr`, `L_ar`, `L_er` = 0). Ölçülen bileşenlerin köşegen dışı elemanlarının işareti serbesttir. Denetim yalnız karşılaştırmadır; hiçbir aritmetik işlem içermez.

### 3.3 Parti başlığı: `ida_det_batch_t` (32 bayt; kodlanmış en çok 27 bayt)

| # | Alan | Tip | Birim | Geçerli aralık | Anlamı | Ret nedeni |
|---|---|---|---|---|---|---|
| 1 | `t_meas_ns` | i64 | ns | 0 … 2⁶² (K81); K17 | Girdi biriminin ölçüm zamanı (K11); `NO_FRAME` başlığında K64 | `TIME` |
| 2 | `batch_seq` | u32 | — | akışta öncekinden büyük; `NO_FRAME`'de 0 | Kamerada `kare_sirasi` (A2); sonarda üreticinin satır sayacı (K16) | `SEQ`; `NO_FRAME`'deki 0 koşulu `STRUCT` |
| 3 | `window_ns` | u32 | ns | kamerada 0 … 1 000 000 000; sonarda 0 | Girdi biriminin aralık süresi (kamerada pozlama süresi) | `TIME` |
| 4 | `calib_ver` | u16 | — | akışın kalibrasyon sürümü | K20 | `CALIB` |
| 5 | `producer_ver` | u16 | — | her değer | Bileşen yoksa 0 | — |
| 6 | `flags` | u16 | bit | ayrılmış bitler 0; `NO_FRAME` K64 | 3.4 | `STRUCT` |
| 7 | `source` | u8 | — | 1, 2 | Kaynak sensör | `STRUCT` |
| 8 | `inst` | u8 | — | 0 … 254 | Sensör örneği | `STRUCT` |
| 9 | `status` | u8 | — | 0 … 8 (3.4) | Partinin durumu (K25) | `STRUCT` |
| 10 | `count` | u8 | — | `status` OK: 0 … 4; diğerleri: 0 | Ardından gelen kayıt sayısı | `STRUCT` |
| 11 | `producer_id` | u8 | — | her değer | Bileşen yoksa 0 | — |
| 12 | `reserved[5]` | u8 | — | 0 | Taşınmaz | `STRUCT` (yalnız A8'de görünür) |

Partide ret sayacı yoktur; ret sayaçları sim'de ve kartta tutulur (K30).

### 3.4 Sabitler ve sayılandırmalar

**`source`:** 0 geçersiz; 1 `PING360`; 2 `CAM_FWD`.

**`kind`:** 0 geçersiz; 1 `RB2` (`r`, `az`); 2 `B2` (`az`, `el`); 3 `RB3` (`r`, `az`, `el`). Poz (6 serbestlik dereceli) türü yoktur.

**`frame`:** 0 geçersiz; 1 `SENSOR`.

**Kayıt bayrakları (`flags`):**

| Bit | Ad | Anlamı |
|---|---|---|
| 0 | `VALID` | Her kayıtta 1'dir (K21); sıfırlanmış belleğin kayıt sanılmasını önler |
| 1 | `NEAR_BLIND_EDGE` | Kör bölge sınırında |
| 2 | `SATURATED` | Doymuş veri içeriyor |
| 3 | `IMAGE_EDGE` | Nesne kare kenarında kesik (kamera) |
| 4 | `RANGE_FROM_PRIOR` | `r` ölçülmedi; üreticinin önsel bilgisinden çıkarıldı (K05) |
| 5 | `SYNTHETIC` | Kayıt yer gerçeğinden üretildi; bu sürümde 0 (K22) |
| 6–15 | ayrılmış | 0 |

**Parti bayrakları:** bit 0 `NO_FRAME` (bir kareye bağlı olmayan durum başlığı; K64); bit 1–15 ayrılmış, 0.

**Parti durumu (`status`):**

| Değer | Ad | Üreten | Anlamı |
|---|---|---|---|
| 0 | `OK` | bileşen, sim doğrular | Kare işlendi; `count` = 0 ise "algılama yok" |
| 1 | `SKIPPED_BUSY` | sim ya da bileşen | Kare teslim anında bileşene sunulamadı (bileşen meşgul ya da karenin halkasıyla henüz el sıkışmamış; K46) ya da bileşen işlemeden atladı |
| 2 | `LATE` | yalnız sim | Sonuç `M_k`'ya yetişmedi |
| 3 | `COMPONENT_DOWN` | yalnız sim | Bağlı, el sıkışmış bileşen yok ya da bileşenin kamera yolu kapalı (K77) |
| 4 | `FRAME_DROPPED` | yalnız sim | A2 kareyi teslim etmedi; A2'nin düşme nedenlerinin eşlemesi K76'da |
| 5 | `FRAME_INVALID` | bileşen | Kare biçimi ya da bütünlüğü geçersiz (K48) |
| 6 | `CALIB_MISMATCH` | bileşen ya da sim | Kalibrasyon sürümü uyuşmuyor; sim'de tek kaynağı 4.6'daki `CALIB` satırıdır |
| 7 | `CAMERA_OFF` | yalnız sim | Kamera kapalı kademe (K64) |
| 8 | `COMPONENT_ERROR` | bileşen ya da sim | Bileşen iç hata bildirdi ya da partisi 4.6'da `CALIB` dışındaki bir nedenle reddedildi |

**Ret neden kodları:** 4.6'daki tablonun sıra numaralarıdır (başlıkta `IDA_DET_RJ_*`; 0 = ret yok).

**Sınırlar:**

| Sabit | Değer | Dayanak |
|---|---|---|
| `IDA_DET_MAX_PER_BATCH` | 4 | Mimari seçimi (tahmin); gerekçe 3.6 |
| Kamera sayısı | 1 | İskelet §10 md. 25 |
| Kamera kare ve parti hızı üst sınırı | 30 Hz | İskelet §10 md. 25 |
| Kamera çözünürlüğü üst sınırı | 640 × 480 | İskelet §10 md. 25 |
| Sonar kaydının parti zamanına göre aralığı | −30 s … +1 s | Mimari seçimi (tahmin); +1 s, 1000 m / 1400 m/s ≈ 0,71 s'yi kapsar (hesap) |
| Bileşenin aynı anda elinde tutabildiği kare | 1 | Mimari seçimi (tahmin) |

### 3.5 A8 soket mesajları (bayt düzeyi)

Bütün çok baytlı alanlar little-endian'dır. `f32` IEEE 754 binary32, `f64` IEEE 754 binary64. Metin alanları ASCII'dir, NUL ile doldurulur; ilk NUL'dan sonrası yok sayılır. A2 kaynaklı alanlar (halka, kare, nesil kimlikleri) A2'deki adlarıyla eşlenir; anlamları A2'dedir (A2 §3.5, §3.7, §3.8).

**Çerçeve başlığı (16 bayt), her mesajın önünde:**

| Konum | Alan | Tip | Değer / anlam |
|---|---|---|---|
| 0 | `magic` | u32 | `0x38414449` (baytlar `'I' 'D' 'A' '8'`) |
| 4 | `type` | u16 | Mesaj türü (aşağıda) |
| 6 | `flags` | u16 | 0 |
| 8 | `length` | u32 | Yük uzunluğu, bayt; en çok 4096; türün boyut aralığı K42'de |
| 12 | `msg_seq` | u32 | Gönderenin bağlantı başına sayacı; ilk mesajda 1, her mesajda +1 |

**Mesaj türleri** (kodların ve adların sahibi A8'dir; W1-uyum U14):

| `type` | Ad | Yön | Yük (bayt) |
|---|---|---|---|
| `0x0001` | `HELLO` | bileşen → sim | 56 |
| `0x0002` | `WELCOME` | sim → bileşen | 32 |
| `0x0003` | `REJECT` | sim → bileşen | 64 |
| `0x0004` | `CAMERA_CONFIG` | sim → bileşen | 152 |
| `0x0005` | `READY` | bileşen → sim | 8 |
| `0x0006` | `RING_OFFER` | sim → bileşen | 112 |
| `0x0007` | `RING_ACK` | bileşen → sim | 16 |
| `0x0008` | `A2_SESSION_END` | sim → bileşen | 8 |
| `0x0009` | `RUN_EPOCH` | sim → bileşen | 8 |
| `0x0010` | `FRAME_AVAILABLE` | sim → bileşen | 56 |
| `0x0011` | `DETECTION_BATCH` | bileşen → sim | 40 + 80 × `count` |
| `0x0012` | `FRAME_RELEASE` | bileşen → sim | 16 |
| `0x0020` | `HEARTBEAT` | iki yön | 0 |
| `0x0021` | `BYE` | iki yön | 4 |

**`HELLO` (56 bayt):**

| Konum | Alan | Tip | Anlamı |
|---|---|---|---|
| 0 | `a8_major` | u16 | 1 |
| 2 | `a8_minor` | u16 | 0 |
| 4 | `det_schema_major` | u16 | `IDA_DET_SCHEMA_MAJOR` (1) |
| 6 | `det_schema_minor` | u16 | `IDA_DET_SCHEMA_MINOR` (0) |
| 8 | `session_token` | u64 | Başlatırken verilen jeton (K31) |
| 16 | `producer_ver` | u16 | Algoritma sürümü |
| 18 | `producer_id` | u8 | ≥ 1 |
| 19 | `capabilities` | u8 | bit 0 `RANGE_FROM_PRIOR` (K05); diğer bitler 0 |
| 20 | `pixel_format_mask` | u32 | Desteklenen piksel biçimleri; bit `n` = A2'nin `n` numaralı `piksel_bicimi` kodu |
| 24 | `name` | char[32] | Bileşenin adı (yalnız A3/A6) |

**`WELCOME` (32 bayt):**

| Konum | Alan | Tip | Anlamı |
|---|---|---|---|
| 0 | `a8_major` | u16 | Sim'in sürümü |
| 2 | `a8_minor` | u16 | Ortak minor (K39) |
| 4 | `det_schema_major` | u16 | Sim'in şema sürümü |
| 6 | `det_schema_minor` | u16 | Ortak minor |
| 8 | `session_id` | u64 | Bu A8 oturumunun kimliği |
| 16 | `det_seq_start` | u32 | Bileşenin ilk kaydına vereceği `det_seq` (K15) |
| 20 | `run_gen` | u32 | O anki çalıştırma nesli (A2 `calisma_nesli`, A2-K80; K78) |
| 24 | `camera_count` | u8 | Ardından gelecek `CAMERA_CONFIG` sayısı; bu sürümde tam 1 (K35) |
| 25 | `reserved` | u8[7] | 0 |

Halka bilgisi `WELCOME`'da değil, `RING_OFFER`'da gelir (K70): A8 oturumu, görselleştirici bağlı değilken ve halka yokken de kurulabilir.

**`REJECT` (64 bayt):** `reason` u16 (konum 0; kodlar aşağıda), `reserved` u16 (2), `text` char[60] (4; yalnız tanı).

**`CAMERA_CONFIG` (152 bayt; geçerlilik K36):** bit eşitliği her bağlantının kendi A5 kaynağına bağlıdır. Tamsayı alanlar A2'nin `KameraYapilandirmasi`'ndaki (A2 §3.6) karşılıklarının, A2'nin geçerli aralığını kayıpsız taşıyan genişlikte kopyasıdır. İç parametre ve bozulma alanları (`fx`, `fy`, `cx`, `cy`, `dist`) A5'teki değerlerin binary64 kayıpsız kopyasıdır ve A2'deki karşılıklarıyla aynı bitlerdedir; hiçbir çevirme ya da yuvarlama yapılmaz ve sınır denetimi (K36) bu değerler üzerinde, A2 §3.6'daki karşılaştırmalarla yapılır. Montaj alanları (`mount_l_b`, `mount_q_bs`) A2'den kopyalanmaz: A5'in **nominal** montajıdır (S çerçevesinde; K53) ve kendi A5 kaynağıyla bit eşittir; A8 fizik gerçeği montajını almaz.

| Konum | Alan | Tip | Birim | Anlamı (A2 §3.6 karşılığı) |
|---|---|---|---|---|
| 0 | `config_ver` | u32 | — | `yapilandirma_surumu`; oturum boyunca sabit (K41) |
| 4 | `camera_id` | u8 | — | `kamera_kimligi`; kayıtta `inst` |
| 5 | `source` | u8 | — | 2 (`CAM_FWD`) |
| 6 | `tier` | u8 | — | A2'nin kamera kademesi kodu (A2 §3.8 yuva başlığı `kademe`: 1 sade, 2 tam) |
| 7 | `distortion_model` | u8 | — | `bozulma_modeli` |
| 8 | `width`, `height` | u16 × 2 | piksel | `genislik`, `yukseklik` |
| 12 | `rate_mhz` | u32 | mHz | `1000 · hiz_pay / hiz_payda`; A2 §3.6 bu değerin tamsayı olmasını garanti eder, dolayısıyla kayıpsızdır |
| 16 | `calib_ver` | u16 | — | `kalibrasyon_surumu`; kayıtlara ve başlığa yazılan sürüm (K20) |
| 18 | `pixel_format` | u16 | — | `piksel_bicimi`; bu oturumda gelecek bütün kareler bu biçimdedir |
| 20 | `reserved` | u32 | — | 0 |
| 24 | `fx`, `fy`, `cx`, `cy` | f64 × 4 | piksel | iğne deliği iç parametreleri; piksel kabulü A2 §3.6 |
| 56 | `dist[5]` | f64 × 5 | — | `bozulma` |
| 96 | `mount_l_b[3]` | f64 × 3 | m | **Nominal** montaj konumu, gövde referans noktasına göre (A5; K53) |
| 120 | `mount_q_bs[4]` | f64 × 4 | — | **Nominal** montaj yönelimi, birim kuaterniyon (w, x, y, z); S'den (x ileri) gövdeye. A2'nin optik çerçeveli (z ileri) fizik gerçeği montajıyla aynı alan değildir (U13) |

**`READY` (8 bayt):** `config_ver_ack` u32 (konum 0; alınan `CAMERA_CONFIG`'in `config_ver`'i), `reserved` u32 (4; 0). Geçerlilik K37.

**`RING_OFFER` (112 bayt; sim → bileşen; K70):** A2 `HalkaBilgisi` ve halka başlığı denetimi (A2-K74) için gereken alanlar.

| Konum | Alan | Tip | Anlamı (A2 karşılığı) |
|---|---|---|---|
| 0 | `a2_session` | u64 | `oturum_kimligi` |
| 8 | `ring_gen` | u32 | `halka_nesli` |
| 12 | `slot_count` | u32 | `yuva_sayisi` |
| 16 | `slot_size` | u64 | `yuva_boyutu` |
| 24 | `total_size` | u64 | `toplam_boyut` |
| 32 | `width`, `height` | u16 × 2 | `genislik`, `yukseklik` |
| 36 | `row_stride` | u32 | `satir_adimi` |
| 40 | `camera_id` | u8 | `kamera_kimligi` |
| 41 | `pixel_format` | u8 | `piksel_bicimi` |
| 42 | `reserved` | u8[6] | 0 |
| 48 | `ring_name` | char[64] | `ad` (A2-K70–K72) |

**`RING_ACK` (16 bayt; bileşen → sim; K71):** `a2_session` u64 (0), `ring_gen` u32 (8), `status` u8 (12; 0 eşlendi ve doğrulandı, 1 eşlenemedi, 2 halka düzeni uyuşmuyor), `reserved` u8[3] (13; 0).

**`A2_SESSION_END` (8 bayt; sim → bileşen; K75):** `a2_session` u64 (0).

**`RUN_EPOCH` (8 bayt; sim → bileşen; K78):** `run_gen` u32 (0; yeni çalıştırma nesli), `reserved` u32 (4; 0).

**`FRAME_AVAILABLE` (56 bayt; K72):**

| Konum | Alan | Tip | Birim | Anlamı (A2 karşılığı) |
|---|---|---|---|---|
| 0 | `t_meas_ns` | i64 | ns | `t_olcum_ns` (pozlama sonu) |
| 8 | `request_id` | u64 | — | `istek_kimligi` |
| 16 | `a2_session` | u64 | — | `oturum_kimligi` |
| 24 | `frame_seq` | u32 | — | `kare_sirasi` (A2-K49) |
| 28 | `window_ns` | u32 | ns | `pozlama_ns` (A2 §3.6 sınırıyla u32'ye sığar) |
| 32 | `ring_gen` | u32 | — | `halka_nesli` |
| 36 | `run_gen` | u32 | — | `calisma_nesli` |
| 40 | `write_count` | u32 | — | `yazim_sayaci` (A2-K53; yalnız tanılama) |
| 44 | `budget_ns` | u32 | ns | Gönderim anında sim'in saatine göre `M_k`'ya kalan süre; yalnız bilgi (K58) |
| 48 | `config_ver` | u32 | — | `yapilandirma_surumu` |
| 52 | `slot` | u16 | — | `yuva` |
| 54 | `camera_id` | u8 | — | `kamera_kimligi` |
| 55 | `reserved` | u8 | — | 0 |

**`DETECTION_BATCH` (40 + 80 × `count` bayt):**

| Konum | Alan | Tip | Anlamı |
|---|---|---|---|
| 0 | `proc_wall_ns` | u32 | `FRAME_AVAILABLE` alımından yanıta bileşenin kendi saatiyle geçen süre (yalnız A3/A6) |
| 4 | `run_gen` | u32 | Yanıtlanan karenin `FRAME_AVAILABLE.run_gen`'i (K79) |
| 8 | parti başlığı | 32 bayt | 3.3'teki alanlar, başlık dosyasındaki konumlarla (8 + konum), `reserved` 0 |
| 40 + 80·i | kayıt `i` | 80 bayt | 3.2'deki alanlar, başlık dosyasındaki konumlarla, `reserved` 0 |

**`FRAME_RELEASE` (16 bayt; bileşen → sim; K74):** A2-K58'deki bırakma üçlüsü: `request_id` u64 (0; `istek_kimligi`), `ring_gen` u32 (8; `halka_nesli`), `slot` u16 (12; `yuva`), `reserved` u16 (14; 0).

**`BYE` (4 bayt):** `reason` u16 (0), `reserved` u16 (2).

**`REJECT` ve `BYE` neden kodları:**

| Kod | Ad | Gönderen | Kullanıldığı kural |
|---|---|---|---|
| 1 | `VERSION_MAJOR` | sim | K34 |
| 2 | `SCHEMA_MAJOR` | sim | K34 |
| 3 | `TOKEN` | sim | K34 |
| 4 | `FORMAT_UNSUPPORTED` | sim | K34 |
| 5 | `RING_MAP_FAILED` | sim | K71 |
| 6 | `PROTOCOL_ERROR` | iki taraf | K42 |
| 7 | `HEARTBEAT_TIMEOUT` | iki taraf | K40 |
| 8 | `SLOT_HOLD_TIMEOUT` | — | kaldırıldı (W1-uyum): yuva zaman aşımı bağlantıyı kapatmaz, halkayı emekliye ayırır (K50) |
| 9 | `CONFIG_CHANGED` | sim | K41 |
| 10 | `SHUTDOWN` | iki taraf | olağan kapanış |
| 11 | `ALREADY_CONNECTED` | sim | K32 |
| 12 | `CAMERA_OFF` | sim | K67 |
| 13 | `HANDSHAKE_TIMEOUT` | sim | K33, K37, K71 |
| 14 | `CAMERA_COUNT` | bileşen | K35 |
| 15 | `CONFIG_INVALID` | bileşen | K36 |
| 16 | `READY_INVALID` | sim | K37 |
| 17 | `PRODUCER_INVALID` | sim | K34 |
| 18 | `CAMERA_PATH_CLOSED` | sim | K77 |

### 3.6 Boyut ve hız bütçesi (hesap)

A1 üzerinde kodlanmış yük, ayrılmış baytlar taşınmadan kayıt için en çok 78, parti başlığı için en çok 27 bayttır. MAVLink 2 çerçevesinin imzasız ek yükü 12 bayttır (10 bayt başlık + 2 bayt sağlama; en büyük yük 255 bayt; imza isteğe bağlı 13 bayt; birincil kaynak: [MAVLink serileştirme](https://mavlink.io/en/guide/serialization.html)). A1'in kaydı tek mesajda mı, birleştirerek mi taşıyacağı A1'in kararıdır; aşağıdaki hesap her öğe ayrı mesaj varsayımıyla üst sınırdır ve tek kamera içindir.

| Büyüklük | Değer (hesap) |
|---|---|
| Bir kamera partisi, 4 kayıtla | 39 + 4 × 90 = 399 B |
| Kamera, 30 Hz, her karede 4 kayıt | 30 × 399 = 11 970 B/s; 200 Hz periyot başına ortalama 59,85 B |
| Kamera, 30 Hz, algılama yok (yalnız başlık) | 30 × 39 = 1 170 B/s; periyot başına 5,85 B |
| A1 sim → kart yön bütçesi (iskelet §3) | Periyot başına ortalama 900 B (kapasitenin %60'ı); kapasite 1 500 B |
| Kamera algılamasının payı, en kötü | Ortalamada 900 B'nin %6,65'i; tek periyotta tepe 399 B (1 500 B'nin %26,6'sı) |

Partideki kayıt sayısının 4 ile sınırlanma gerekçesi tepe yüktür: 03 ve 07'nin önerdiği 8 kayıt tek periyotta 39 + 8 × 90 = 759 B eder (kapasitenin %50,6'sı) ve aynı periyottaki zaman çerçevesi, IMU ve parçalanmış sonar profiliyle birlikte kapasiteyi zorlayabilir (hesap; A1'in gerçek periyot dolgusu A1 sözleşmesinde ve MP5'te belli olacak). 4 tahmindir (8.1).

---

## 4. Davranış kuralları

### 4.1 Kayıt: çerçeve ve içerik

- **A8-K01.** Kayıt ölçümü yalnız sensör çerçevesinde taşır; `frame` 1 (`SENSOR`) dışındaki değer partinin reddidir (`KIND`).
- **A8-K02.** Kayıt araç pozu, hızı, seyrüsefer durumu, gövde ya da dünya çerçevesinde konum, hedef kimliği ya da başka bir yer gerçeği alanı taşımaz; gövde ve dünya çerçevesine dönüşümü yalnız kart, kendi seyrüsefer çözümü ve kendi parametresindeki montajla yapar (iskelet §3).
- **A8-K03.** `z` bileşenlerinin yeri sabittir (`z[0]` = `r`, `z[1]` = `az`, `z[2]` = `el`); `kind`'ın içermediği bileşenin `z` değeri tam 0'dır (aksi `VALUE`) ve `chol`'daki satırı ve sütunu tam 0'dır (aksi `UNCERT`).
- **A8-K04.** Mesafe ölçmeyen kaynak (tek kamera) bunu `kind` = B2 ile bildirir; B2 kaydında `r` = 0'dır ve mesafe başka hiçbir alanda taşınmaz.
- **A8-K05.** `source` = CAM_FWD olan kayıt yalnız B2 ya da RB3 olur ve RB3 yalnız `RANGE_FROM_PRIOR` bayrağıyla geçerlidir; `source` = PING360 olan kayıt yalnız RB2 olur; aksi `KIND`. Ayrıca sim, `HELLO`'da `RANGE_FROM_PRIOR` yeteneğini bildirmemiş bileşenden gelen RB3 kaydını `KIND` ile reddeder; bu yetenek denetimi yalnız sim'indir (K28).
- **A8-K06.** 3.2 tablosundaki aralığın dışında kalan ya da NaN veya sonsuz değer içeren bir alan partinin reddidir; neden kodu tablodaki sütundur.
- **A8-K07.** Belirsizlik, `(r, az, el)` ölçüm kovaryansının alt üçgen Cholesky çarpanı `L` olarak taşınır. `chol` alanı ancak 3.2'deki tabloya uyarsa geçerlidir: bütün elemanlar sonlu ve mutlak değer üst sınırının altında, ölçülen her bileşenin köşegen elemanı alt sınıra eşit ya da ondan büyük, ölçülmeyen bileşenin satırı ve sütunu tam 0. Aksi `UNCERT`. Bu denetim yalnız binary32 karşılaştırmasıdır; hiçbir taraf kabul kararı için çarpanı ya da kovaryansı yeniden hesaplamaz ve pozitif tanımlılık sınaması yapmaz.
- **A8-K08.** `confidence` [0, 1] aralığında sıralayıcı bir puandır; olasılık kalibrasyonu taşımaz ve hiçbir tüketici onu belirsizliğin yerine kullanmaz.
- **A8-K09.** Kovaryans `Σ = L·Lᵀ`'dir ve yalnız ölçüm belirsizliğini taşır; seyrüsefer ve montaj belirsizliği kayda eklenmez. K07'yi geçen her `L` için `Σ`'nın ölçülen bileşenlere ait alt matrisi tanım gereği pozitif tanımlıdır; tüketici `Σ`'yı kurup kurmamakta ve sayısal yöntemini seçmekte serbesttir, ama kaydı bu hesabın sonucuna göre reddetmez.

### 4.2 Kayıt: zaman

- **A8-K10.** `t_meas_ns` iskelet §3.1'deki ölçüm zamanıdır: anlık ölçümde geçerlilik anı ve `window_ns` = 0; aralık boyunca toplanan ölçümde aralığın sonu ve `window_ns` aralığın süresi. Tek geçerlilik anı gereken tüketici `t_valid = t_meas_ns − (window_ns / 2)` kullanır.
- **A8-K11.** Parti başlığının `t_meas_ns` ve `window_ns` alanları, partinin işlediği girdi biriminin iskelet §3.1'e göre ölçüm zamanı ve aralık süresidir ve kayıtlardan türetilmez; boş partide de dolu partide de aynıdır. Kamerada bunlar karenin ölçüm zamanı (pozlama sonu) ve pozlama süresidir (A2); sonarda tarama satırının gönderim anı (`t_tx`; iskelet §3.1 menzil profili) ve 0'dır.
- **A8-K12.** Kamera kaynaklı kaydın `t_meas_ns` ve `window_ns` alanları parti başlığındakilerle, dolayısıyla A2'nin o kare için verdiği değerlerle bit düzeyinde aynıdır (aksi `TIME`); işlem bitişi, kare teslimi ya da alım anı bu alanlara yazılmaz.
- **A8-K13.** Sonar kaynaklı kayıtta `window_ns` = 0'dır, `t_meas_ns` yansıma anıdır (iskelet §3.1) ve `d = kayıt.t_meas_ns − başlık.t_meas_ns` farkı `−30 000 000 000 ≤ d ≤ 1 000 000 000` koşulunu sağlar (aksi `TIME`); fark K81'e göre taşmasız hesaplanır. Kümenin zaman yayılımı `t_sigma_ns`'te taşınır.
- **A8-K14.** Kayıt alım zamanı taşımaz; alım zamanını alıcı kendi saatiyle damgalar ve kayıttan ayrı tutar (A4).

### 4.3 Kayıt: kimlik ve parti

- **A8-K15.** `det_seq` her akışta bir sim süreci boyunca (A8 oturumları, sıfırlama ve atlama dahil) kesin artar (boşluk serbesttir): bir partideki kayıtların `det_seq`'leri `batch_index` sırasıyla kesin artar ve ilki akışta son kabul edilen `det_seq`'ten büyüktür (aksi `SEQ`). Kamera bileşeni ilk kaydına `WELCOME.det_seq_start`'ı verir ve gönderdiği her kayıtta 1 artırır; sim `det_seq_start`'ı, o akışta daha önce verdiği bütün `det_seq`'lerden büyük seçer. Sayaç sarmaz (hesap: 30 Hz × 4 kayıtla 2³² kayıt ≈ 414 gün).
- **A8-K16.** Parti başlığının `batch_seq`'i her akışta, `NO_FRAME` başlıkları dışında, akışta son kabul edilen partininkinden büyüktür (aksi `SEQ`). Kamerada `batch_seq` karenin A2 `kare_sirasi`'dır ve ömrü K80'dedir; sonarda üreticinin satır sayacıdır ve bir tarama satırı için en çok bir parti üretilir.
- **A8-K17.** Zaman ilerleme denetimi parti düzeyindedir: `NO_FRAME` dışındaki bir partinin `t_meas_ns`'i akışta son kabul edilen partininkinden büyüktür (aksi `TIME`); aynı partideki kayıtlar arasında zaman ilerlemesi aranmaz. Çalıştırma neslinin her değişimi (sıfırlama, atlama, yeni çalıştırma; A2-K80) A1'de oturum kapanışı ve yeni oturumdur; karta ayrı bir nesil iletisi gitmez. Kart tarafında akışın zaman ilerleme durumu oturum sonu çağrısında sıfırlanır (A1-K107, A1-K128, A4-K122). Sim'in kabul kapısında (K28) akışın zaman ilerleme durumu, sim çalıştırma neslini artırdığı anda (A2-K80; K78) sıfırlanır: yeni nesilde kabul edilen ilk parti, eski neslin son kabul edilen `t_meas_ns`'ine göre denetlenmez. Hem kartta hem sim'de akışın `batch_seq` ve `det_seq` kabul durumları nesil değişiminde sıfırlanmaz ve korunur (K15, K80).
- **A8-K18.** Bir parti en çok 4 kayıt içerir (aksi `STRUCT`); kayıtlar başlıktan hemen sonra gelir, `i`'nci kaydın `batch_index`'i `i`'dir ve her kaydın `batch_count`'u başlığın `count`'una eşittir (aksi `LINK`).
- **A8-K19.** Partideki her kaydın şu alanları başlıktaki karşılığına eşittir; eşit değilse parti tablodaki nedenle reddedilir:

| Kayıt alanı | Başlık alanı | Ret nedeni |
|---|---|---|
| `calib_ver` | `calib_ver` | `CALIB` |
| `source` | `source` | `LINK` |
| `inst` | `inst` | `LINK` |
| `producer_id` | `producer_id` | `LINK` |
| `producer_ver` | `producer_ver` | `LINK` |
| `batch_seq` | `batch_seq` | `LINK` |
| `batch_count` | `count` | `LINK` |
| `batch_index` | kaydın sırası (0'dan) | `LINK` |
| `t_meas_ns`, `window_ns` (kamera) | `t_meas_ns`, `window_ns` | `TIME` (K12) |
| `t_meas_ns` (sonar) | `t_meas_ns` ile K13 aralığı | `TIME` (K13) |

### 4.4 Kayıt: diğer alanlar

- **A8-K20.** `calib_ver` üreticinin kullandığı iç kalibrasyon ve montaj sürümüdür. Kamera bileşeni başlığa ve her kayda `CAMERA_CONFIG.calib_ver`'i yazar. Başlığın `calib_ver`'i başvuru değerine eşit değilse ya da bir kaydınki başlığınkine eşit değilse parti `CALIB` ile reddedilir; başvuru değeri sim için `CAMERA_CONFIG.calib_ver`, kart için kendi parametresinde o akış için tuttuğu sürümdür. Kalibrasyon uyuşmazlığının tek neden kodu `CALIB`, karta giden tek durumu `CALIB_MISMATCH`'tir.
- **A8-K21.** Kayıtta ayrılmış bayrak bitlerinden biri 1 ise ya da `VALID` biti 0 ise parti `FLAGS` ile reddedilir.
- **A8-K22.** Bu sürümde `SYNTHETIC` biti 1 olan kayıt içeren parti `FLAGS` ile reddedilir; yer gerçeğinden üretilen kayıt (K56) kapsama alınırsa bu kural ayrı bir sözleşme değişikliğiyle değişir.
- **A8-K23.** `class_id`, A5'in kapalı hedef sınıfı tablosundaki koddur: 0 `bilinmiyor`, 1 `sabit_nesne`, 2 `hareketli_nesne`, 3 `yapi`; 4–255 ayrılmıştır ve partinin reddidir (`VALUE`). Kod yalnız algılanan nesnenin kaba türünü kodlar; hedef seçimi ya da başka bir karar anlamı taşımaz ve bu sözleşmenin hiçbir kuralı ona göre dallanmaz.
- **A8-K24.** Kayıtta `producer_id` = 0 partinin reddidir (`VALUE`); `producer_id` ve `producer_ver` yalnız telemetri ve kayıt içindir, kaydın yorumu bunlara göre değişmez; algılayıcının nerede çalıştığı yalnız bu alanlarda görünür.

### 4.5 Parti durumu, hız ve boyut

- **A8-K25.** `status` = OK ve `count` = 0 "girdi birimi işlendi, algılama yok" demektir; `status` OK dışındaysa `count` = 0'dır (aksi `STRUCT`) ve parti "bu girdi birimi için algılama sonucu yok" demektir.
- **A8-K26.** Kamera akışında parti hızı kare hızını, kare hızı 30 Hz'i aşmaz; sonar akışında tarama satırı başına en çok bir parti üretilir; bir kaydın kodlanmış yükü 78 baytı, parti başlığınınki 27 baytı aşmaz.

### 4.6 Doğrulama

- **A8-K27.** Bir partinin doğrulaması aşağıdaki tablonun satırları sırasıyla yapılır; ilk başarısız satır partinin tek ret nedenidir ve sonucu **bütün partinin reddidir**: hiçbir kayıt çıkarılıp kalanı iletilmez. Bir satır başarısızsa sonraki satırlara bakılmaz.

| Sıra (`IDA_DET_RJ_*`) | Ad | Denetim | Sim | Kart | Karta giden durum (sim reddinde) |
|---|---|---|---|---|---|
| 1 | `STRUCT` | `count` ≤ 4; `status` 0 … 8; `status` ≠ OK ise `count` = 0; `source` 1 ya da 2; `inst` ≤ 254; başlık bayraklarının ayrılmış bitleri 0; `NO_FRAME` yalnız `status` = CAMERA_OFF, `batch_seq` = 0, `count` = 0 ile; sim'de ayrıca: çerçevesi K42'ye göre geçerli bir `DETECTION_BATCH`'in yük uzunluğu = 40 + 80 × `count` (K42 sınıf b) ve bütün `reserved` alanları 0 | evet | evet (yük uzunluğu ve `reserved` hariç) | `COMPONENT_ERROR` |
| 2 | `ECHO` | Başlığın `source` = CAM_FWD, `inst` = `camera_id`, `batch_seq` = bekleyen karenin `frame_seq`'i, `t_meas_ns` ve `window_ns` = o karenin değerleri; `producer_id` ve `producer_ver` = `HELLO`'daki; `status` ∈ {OK, SKIPPED_BUSY, FRAME_INVALID, CALIB_MISMATCH, COMPONENT_ERROR} | evet | hayır | `COMPONENT_ERROR` |
| 3 | `CALIB` | K20 | evet | evet | `CALIB_MISMATCH` |
| 4 | `LINK` | K18'in `batch_index`/`batch_count` kısmı; K19 tablosunun `LINK` satırları | evet | evet | `COMPONENT_ERROR` |
| 5 | `SEQ` | K15, K16 | evet | evet | `COMPONENT_ERROR` |
| 6 | `TIME` | **Başlık:** `t_meas_ns` 0 … 2⁶² (K81); `source` = CAM_FWD ve `NO_FRAME` yokken `window_ns` ≤ 1 000 000 000; `source` = PING360 ise `window_ns` = 0; `NO_FRAME` başlığında `window_ns` = 0; `NO_FRAME` dışındaki başlıkta K17. **Kayıt:** `t_meas_ns` 0 … 2⁶² (K81); kamerada K12; sonarda K13 (`window_ns` = 0 dahil; fark K81'e göre). K11'in kamera eşitliği sim'de `ECHO` satırında sınanır. K11'in sonar gönderim anı eşitliği ve K64'ün "partinin kapanış anı" eşitliği üretici kurallarıdır; alıcının elindeki alanlarla sınanamaz ve doğrulamaya girmez | evet | evet | `COMPONENT_ERROR` |
| 7 | `KIND` | K01, K05 (sim'de yetenek denetimi dahil) | evet | evet (yetenek denetimi hariç) | `COMPONENT_ERROR` |
| 8 | `FLAGS` | K21, K22 | evet | evet | `COMPONENT_ERROR` |
| 9 | `VALUE` | 3.2'deki `VALUE` sütunlu alanlar; K03'ün `z` kısmı; K23; K24 | evet | evet | `COMPONENT_ERROR` |
| 10 | `UNCERT` | K07 (K03'ün `chol` kısmı dahil) | evet | evet | `COMPONENT_ERROR` |

- **A8-K28.** **Sim'in doğruladıkları:** sim, önce `DETECTION_BATCH.run_gen`'i denetler: güncel çalıştırma neslinden farklıysa yanıt K79'a göre `eski_nesil` ile reddedilir, aşağıdaki tabloya girmez ve karta hiçbir şey gitmez. Sonra sim, A8'den aldığı her `DETECTION_BATCH`'i K27 tablosunun "Sim" sütunu "evet" olan bütün satırlarıyla, karta iletmeden önce doğrular. `HELLO` yeteneklerine ve bekleyen kareye dayanan denetimler (`ECHO`, `KIND`'ın yetenek kısmı) yalnız sim'indir. Reddedilen partinin yerine karta, başlığı K62'ye göre doldurulmuş, `count` = 0 ve durumu ilk başarısız satırın son sütunundaki değer olan bir parti gider. Bileşenin bildirdiği durum (OK dahil) karta yalnız yanıt, sim için uygulanabilir bütün satırlardan geçtiyse aktarılır; herhangi bir satır başarısızsa yalnız ilk başarısız satırın sonucu geçerlidir. Kayıt içermeyen bir partide kayıtlara ilişkin denetimler boş kümeye uygulanır ve geçer. Bu kural yalnız `M_k`'dan önce alınan yanıta uygulanır; sonra alınan yanıt doğrulanmadan `LATE`'tir (K59, K63).

  **Karar tablosu** (sim; yanıt `M_k`'dan önce alındı; bütün durum × denetim birleşimleri):

  | Bileşenin `status`'u ve `count`'u | Uygulanan satırlar | Hepsi geçerse karta | Bir satır başarısızsa karta |
  |---|---|---|---|
  | OK, `count` 0 … 4 | 1–10 | OK ve kayıtlar | İlk başarısız satırın son sütunu (`CALIB` → `CALIB_MISMATCH`, diğerleri → `COMPONENT_ERROR`) |
  | SKIPPED_BUSY, FRAME_INVALID, CALIB_MISMATCH ya da COMPONENT_ERROR; `count` = 0 | 1, 2, 3, 5, 6 başlığa uygulanır; 4 ve 7–10 yalnız kayıtlarla ilgili olduğundan boş kümeye uygulanır ve geçer | Bileşenin bildirdiği durum | İlk başarısız satırın son sütunu |
  | Bu dört durumdan biri; `count` > 0 | 1 başarısız | — | `COMPONENT_ERROR` |
  | OK; `count` > 4 | 1 başarısız | — | `COMPONENT_ERROR` |
  | LATE, COMPONENT_DOWN, FRAME_DROPPED ya da CAMERA_OFF (yalnız sim'in üretebildiği durumlar) | 1 geçerse 2 başarısız | — | `COMPONENT_ERROR` |
  | 8'den büyük değer | 1 başarısız | — | `COMPONENT_ERROR` |

  Örnek: `status` = FRAME_INVALID, `count` = 0, `calib_ver` yapılandırmanınkinden farklı, 1–2. satırları geçiyor → 3. satır başarısız → karta `CALIB_MISMATCH` gider.
- **A8-K29.** **Kartın doğruladıkları:** kart (ya da SIL koşucusunda çekirdek), aldığı her partiye, kart içi sonar partileri dahil, K27 tablosunun "Kart" sütunu "evet" olan satırlarını aynı sırayla uygular; bu satırlar yalnız karta ulaşan alanlarla ve kartın kendi parametreleriyle sınanabilir. Reddedilen partinin başlığı ve kayıtları kullanılmaz.
- **A8-K30.** Ret sayaçları partide taşınmaz ve çekirdek telemetrisinden beklenmez: çekirdekte algılama retleri için tek toplam `det_rejected` vardır (A4). Akış, ret nedeni (`IDA_DET_RJ_*`), `eski_nesil`, geç yanıt ve karta giden durum başına ayrıntılı sayaçları sim kendi kabul kapısında 64 bit tutar (bileşenin bildirdiği durumlar dahil); sayaçlar A3 üzerinden yayımlanır, her ret nedeniyle A6'ya yazılır. Bileşenin kendi iç sayaçları bu sürümde dışa açılmaz; ayrıntılı sayaçlar yalnız sim'in kabul kapısında tutulur ve A3/A6'da `algilama/sayaclar` ile yayımlanır (mimar kararı; W1-uyum görev belgesindeki "bileşende ve sim'in kabul kapısında" ifadesinden bilinçli daraltma).

### 4.7 A8 bağlantısı: oturum

- **A8-K31.** A8 taşıması TCP'dir ve yalnız 127.0.0.1 üzerindedir; sim dinler, bileşen bağlanır. Sim bileşeni başlatırken bağlantı noktasını ve 64 bitlik rastgele oturum jetonunu `--a8-port <ondalık>` ve `--a8-token <16 onaltılık hane>` komut satırı argümanlarıyla verir; bileşen bağlantı noktasını başka yoldan aramaz.
- **A8-K32.** Sim aynı anda tek bir el sıkışmış bileşen bağlantısı tutar; ikinci bağlantıya `REJECT(ALREADY_CONNECTED)` gönderip kapatır.
- **A8-K33.** Bileşen bağlandıktan sonra 5 s içinde (sim'in saatiyle; mimari seçimi) `HELLO` göndermezse sim `BYE(HANDSHAKE_TIMEOUT)` gönderip bağlantıyı kapatır.
- **A8-K34.** Sim `HELLO`'yu şu sırayla doğrular ve ilk başarısız denetimin nedeniyle `REJECT` gönderip bağlantıyı kapatır: `a8_major` ≠ 1 → `VERSION_MAJOR`; `det_schema_major` ≠ `IDA_DET_SCHEMA_MAJOR` → `SCHEMA_MAJOR`; jeton uyuşmazlığı → `TOKEN`; `producer_id` = 0 ya da `capabilities`'in ayrılmış bitleri 1 → `PRODUCER_INVALID`; `pixel_format_mask` A2'nin kullandığı biçimin bitini içermiyor → `FORMAT_UNSUPPORTED`. Kabul edilen `HELLO`'nun `producer_id`, `producer_ver` ve `capabilities` alanları oturum boyunca sim'de saklanır.
- **A8-K35.** Bu sürümde `WELCOME.camera_count` = 1'dir ve `WELCOME`'ın ardından tam bir `CAMERA_CONFIG` gelir. Bileşen `camera_count` ≠ 1 alırsa `BYE(CAMERA_COUNT)` gönderir, bağlantıyı kapatır ve yeniden bağlanmaz (K43). Senaryo birden çok kamera tanımlarsa sim bileşeni başlatmaz ve durumu A3'e hata olarak bildirir.
- **A8-K36.** `CAMERA_CONFIG` ancak aşağıdaki koşulların hepsini sağlarsa geçerlidir. A2 §3.6'nın karşılığı olan alanların (`config_ver`, `camera_id`, `width`, `height`, `rate_mhz` (A2'nin `hiz_pay`/`hiz_payda` çiftinden), `calib_ver`, `pixel_format`, `fx`, `fy`, `cx`, `cy`, `distortion_model`, `dist`) geçerli aralığı A2 §3.6'dadır ve burada yinelenmez (W1-uyum U13); her biri A2 §3.6'daki aralıkta olmak zorundadır. Sim geçersiz bir yapılandırma göndermez (değerleri bileşeni başlatmadan önce aynı koşullarla denetler; geçersizse bileşeni başlatmaz ve A3'e hata bildirir). Bileşen geçersiz bir yapılandırma alırsa `BYE(CONFIG_INVALID)` gönderir, bağlantıyı kapatır ve yeniden bağlanmaz (K43).

| Alan | A8'e özgü koşul | Dayanak |
|---|---|---|
| A2 §3.6 karşılığı olan alanlar | A2 §3.6'daki geçerli aralık | A2 §3.6 (sahibi) |
| `source` | 2 | — |
| `tier` | A2'nin kamera kademesi kodlarından biri (1 sade, 2 tam) | A2 §3.8, §6 |
| `pixel_format` | ayrıca `HELLO.pixel_format_mask`'te bildirilmiş | K34 |
| `mount_l_b[3]`, `mount_q_bs[4]` | A2 §3.6'nın `montaj_l_b` ve `montaj_q_bs` satırlarındaki koşullar, A5'in nominal montajına aynen uygulanır | A2 §3.6; değerin kaynağı A5 |

- **A8-K37.** Bileşen geçerli `CAMERA_CONFIG`'i aldıktan sonra `READY` gönderir. Sim `READY`'yi doğrular: `config_ver_ack` gönderilen `config_ver`'e eşit değil ya da `reserved` ≠ 0 → `BYE(READY_INVALID)` ve kapanış. `READY` son `CAMERA_CONFIG`'ten sonra 5 s içinde gelmezse sim `BYE(HANDSHAKE_TIMEOUT)` ile kapatır. A8 el sıkışması ancak geçerli `READY` alındığında tamamlanır. Halkayla el sıkışma bundan ayrıdır (K70, K71): A8 oturumu halka olmadan da kurulur ve sürer; sim bileşene, A8 el sıkışması ve karenin halkasıyla el sıkışma tamamlanmadan `FRAME_AVAILABLE` göndermez.
- **A8-K38.** Bileşen halkayı yalnız salt okunur eşler; halkanın hiçbir alanına (denetim alanları dahil) yazmaz.
- **A8-K39.** Major sürümler (A8 ve şema) eşit olmak zorundadır; minor sürümler farklıysa iki taraf küçük olanı kullanır. Minor sürüm yalnız mesaj yükünün sonuna alan ekleyebilir ve eklediği uzantının boyutunu tanımlar. Alıcı, tanıdığı bölümden sonra gelen fazla baytları yalnız el sıkışmada anlaşılan minor sürüm o mesaj türü için bir uzantı tanımlıyorsa ve fazlalık tam o boyuttaysa yok sayar; aksi halde K42'deki sınıflar uygulanır. Bu sürümde anlaşılan minor 0'dır ve hiçbir mesaj türü uzantı tanımlamaz; yani 1.0'da hiçbir fazla bayt yok sayılmaz.
- **A8-K40.** Her taraf, son 1 s içinde başka mesaj göndermediyse `HEARTBEAT` gönderir; karşı taraftan 3 s boyunca hiç mesaj alamayan taraf `BYE(HEARTBEAT_TIMEOUT)` gönderip bağlantıyı kapatır (süreler kendi duvar saatiyle; mimari seçimi).
- **A8-K41.** Bir oturum boyunca kamera yapılandırması (kademe, çözünürlük, hız, iç parametreler, montaj, `calib_ver`, piksel biçimi) değişmez; değişecekse sim önce `BYE(CONFIG_CHANGED)` gönderir ve bağlantıyı kapatır, bileşen yeniden bağlanır ve yeni el sıkışmayla yeni yapılandırmayı alır.
- **A8-K42.** Gelen her mesaj üç hata sınıfına göre, bu sırayla değerlendirilir:
  - **(a) Çerçeve hatası:** `magic` yanlış; `type` bilinmiyor ya da yöne uymuyor; çerçeve başlığının `flags`'i ≠ 0; `msg_seq` bir önceki mesajınkinden tam 1 büyük değil; mesaj el sıkışmanın sırasına uymuyor; `length` > 4096; ya da `length` türün anlaşılan minor sürümdeki boyut aralığının dışında. 1.0'da bu aralık sabit boyutlu türlerde (`HELLO` 56, `WELCOME` 32, `REJECT` 64, `CAMERA_CONFIG` 152, `READY` 8, `RING_OFFER` 112, `RING_ACK` 16, `A2_SESSION_END` 8, `RUN_EPOCH` 8, `FRAME_AVAILABLE` 56, `FRAME_RELEASE` 16, `HEARTBEAT` 0, `BYE` 4) tam o boyut, `DETECTION_BATCH`'te 40 ≤ `length` ≤ 360'tır (360 = 40 + 80 × 4). Alıcı `BYE(PROTOCOL_ERROR)` gönderip bağlantıyı kapatır.
  - **(b) Parti uzunluk hatası:** çerçeve (a)'ya göre geçerli bir `DETECTION_BATCH`'te `length` ≠ 40 + 80 × `count` (`count`, yükün 0'dan sayılan 33. konumundaki bayttır). Sim bütün partiyi K27'nin `STRUCT` satırıyla reddeder; bağlantı açık kalır.
  - **(c) Minor uzantısı:** tanınan bölümden sonra gelen fazla baytlar yalnız K39'un izin verdiği durumda yok sayılır; izin yoksa fazlalık, sabit boyutlu türde (a), `DETECTION_BATCH`'te (a) ya da (b) olarak, yukarıdaki sınırlarla değerlendirilir.

  `DETECTION_BATCH` örnekleri (1.0):

  | `count` | `length` | Sınıf | Sonuç |
  |---|---|---|---|
  | 0 | 40 | geçerli | Doğrulamaya geçer |
  | 0 | 41 | (b) | `STRUCT` reddi; bağlantı açık |
  | 0 | 360 | (b) | `STRUCT` reddi; bağlantı açık |
  | 3 | 360 | (b) | `STRUCT` reddi; bağlantı açık |
  | 4 | 360 | geçerli | Doğrulamaya geçer |
  | 5 | 360 | (b); ayrıca `count` > 4 | `STRUCT` reddi; bağlantı açık |
  | 5 | 440 | (a) | `BYE(PROTOCOL_ERROR)`; bağlantı kapanır |
  | herhangi | 39 | (a) | `BYE(PROTOCOL_ERROR)` |
  | herhangi | 361 | (a) | `BYE(PROTOCOL_ERROR)` |
  | herhangi | 4097 | (a) | `BYE(PROTOCOL_ERROR)` |
- **A8-K43.** Bağlantısı kapanan bileşen halka eşlemelerini K82'deki sırayla kaldırır. `REJECT` almışsa ya da kendisi `BYE(CAMERA_COUNT)` veya `BYE(CONFIG_INVALID)` göndermişse yeniden bağlanmaz ve sıfırdan farklı çıkış koduyla sonlanır; bağlantı başka bir nedenle kapandıysa en erken 1 s sonra (mimari seçimi) yeniden bağlanmayı dener ve yeni bağlantı yeni bir oturumdur.

### 4.8 Kare teslimi

- **A8-K44.** Sim bileşene bir kareyi yalnız şu koşulların hepsi sağlanıyorsa sunar: A8 el sıkışması tamamlanmış (K37); bileşen karenin halkasıyla el sıkışmış (K71) ve o halka etkin (A2 §4.10); bileşenin kamera yolu açık (K77); bileşenin yanıtlanmamış ya da bırakılmamış karesi yok. Nesli eskimiş kare (K79) yanıtlanmış sayılır. Bileşenin elinde aynı anda en çok bir kare bulunur.
- **A8-K45.** Yuva başvurusu A2-K58'e göredir: sim bir kareyi bileşene sunduğunda bileşeni o karenin bekleyen tüketici kümesine koyar; bu başvuruyu yalnız bileşenin aynı bağlantıdan gönderdiği, üçlüsü eşleşen `FRAME_RELEASE` kaldırır (K74). `DETECTION_BATCH`, bağlantının kapanması, A8 oturumunun bitmesi ya da bir zaman aşımı başvuruyu kaldırmaz ve yuvayı boşaltmaz (A2-K60). Bekleyen başvuru varken bağlantı kapanırsa ve kare etkin halkadaysa sim o halkayı A2-K59 ve A2-K86'ya göre emekliye ayırır; kare emekli halkadaysa yalnız sayar (A2-K87).
- **A8-K46.** Sim bir kareyi bileşene yalnız o karenin A2'deki teslim anında (`S_d`; A2-K46, A2-K61) sunar: o anda K44'ün koşulları sağlanıyorsa bileşen teslim edilen karenin bekleyen tüketici kümesine alınır ve `FRAME_AVAILABLE` gönderilir; sağlanmıyorsa kare bileşene hiçbir zaman sunulmaz ve sonradan aday olamaz. Böylece bir A8 oturumunda sunulan karelerin `kare_sirasi`'ı kesin artar. K46 hiçbir kareye durum atamaz; sunulmayan karenin nihai durumu `M_k` anında yalnız K63'e göre atanır.
- **A8-K47.** Bileşen ardışık `FRAME_AVAILABLE`'ların `frame_seq` değerlerinin ardışık olduğunu varsaymaz; boşluk olağan durumdur ve bileşenin davranışını değiştirmez.
- **A8-K48.** Bileşen K73'teki edinme çitinden sonra, okumaya başlamadan önce ve okumayı bitirdikten sonra yuva başlığını (A2 §3.8) denetler ve şu durumlarda kayıt üretmez. (a) `FRAME_INVALID` durumlu parti gönderir: `bayraklar` bit 7 (`tutarsiz`) = 1; `yapilandirma_surumu` ≠ `CAMERA_CONFIG.config_ver`; `istek_kimligi`, `halka_nesli`, `kare_sirasi`, `calisma_nesli` ya da `yazim_sayaci` `FRAME_AVAILABLE`'dakinden farklı (okumadan önce ya da sonra; A2-K53); `kamera_kimligi`, `piksel_bicimi`, `genislik`, `yukseklik` ya da `satir_adimi` `CAMERA_CONFIG` ve `RING_OFFER`'dakinden farklı; `t_olcum_ns` ya da `pozlama_ns` `FRAME_AVAILABLE`'daki `t_meas_ns` ve `window_ns`'ten farklı. (b) (a)'nın hiçbiri yoksa ve `kalibrasyon_surumu` ≠ `CAMERA_CONFIG.calib_ver` ise `CALIB_MISMATCH` durumlu parti gönderir. Yuva başlığı denetimi A2-K53 gereği yalnız tanılamadır; veri yarışına karşı koruma yuva protokolüdür (K44–K46, K74).
- **A8-K49.** Bileşen her `FRAME_AVAILABLE`'a, geç kalmış olsa bile, tam bir `DETECTION_BATCH` ile yanıt verir; tek istisna: yanıtı göndermeden önce karenin `run_gen`'inden daha yeni bir `RUN_EPOCH` almışsa o kare için `DETECTION_BATCH` göndermez (K79). Yanıtın `status`'u OK, `SKIPPED_BUSY`, `FRAME_INVALID`, `CALIB_MISMATCH` ya da `COMPONENT_ERROR`'dur. Bileşen bir karenin `DETECTION_BATCH`'ini o karenin `FRAME_RELEASE`'inden sonra gönderir (K74).
- **A8-K50.** Bileşen sunulan bir kareyi sunulmasından sonra 1 s (sim'in duvar saati; A2-K59) içinde bırakmazsa sim bağlantıyı kapatmaz ve yuvayı geri almaz: kare etkin halkadaysa o halkayı A2-K59 ve A2-K86'ya göre emekliye ayırır ve bileşene yeni halkayı K70 ile bildirir; kare emekli halkadaysa yalnız sayar (A2-K87).
- **A8-K51.** Sim bileşene yalnız `CAMERA_CONFIG`'teki çözünürlükte ve piksel biçiminde kareleri sunar ve kare hızını `rate_mhz`'in üstüne çıkarmaz; bileşen yalnız el sıkışmada bildirilen çözünürlük, biçim ve hızla çalışır ve bunlardan farklı bir kareyi K48'e göre `FRAME_INVALID` sayar.

### 4.9 Bileşenin aldığı ve almadığı veriler

- **A8-K52.** Sim'den bileşene giden veri yalnız şunlardır: kare pikselleri ve halkadaki kare üst verisi (A2), `FRAME_AVAILABLE` alanları (A2 kare ve halka kimlikleri, ölçüm zamanı, pozlama süresi, çalıştırma nesli, yazım sayacı, süre bilgisi), `RING_OFFER`, `A2_SESSION_END` ve `RUN_EPOCH` alanları (halka bilgisi, A2 oturum kimliği, çalıştırma nesli), `CAMERA_CONFIG` alanları (çözünürlük, hız, iç parametreler, bozulma, nominal montaj, `calib_ver`, piksel biçimi, kademe) ve oturum alanları. 3.5'te olmayan bir alan sim'den bileşene gönderilmez.
- **A8-K53.** `CAMERA_CONFIG`'teki montaj, kartın kullandığı nominal montajdır (A5'in otopilot tarafı); fizik gerçeğinin montajı ve montaj hatası bileşene verilmez. Bileşen montajı yalnız görüntü içi yorum için kullanabilir; kayıt yine sensör çerçevesindedir (K01).
- **A8-K54.** Bileşene aracın pozu, hızı ya da seyrüsefer durumu, hedeflerin konumu, kimliği ya da sınıfı, yer gerçeği maskesi, senaryonun hedef listesi, başka sensörlerin verisi ve kartın durumu (izler, kipler, emniyet durumu) verilmez; bileşenin okuyabildiği halka bölgesi de bunları içermez (A2'den beklenen 3).
- **A8-K55.** Bileşen sim'e yalnız A8 üzerinden bağlanır; A1, A2 durum kanalı ve A3 uç noktalarına bağlanmaz.
- **A8-K56.** Yer gerçeğinden kayıt üreten bir üretici A8'e bağlanamaz; böyle bir üretici kapsama alınırsa sim'in içinde, `SYNTHETIC` bayrağı ve ayrı bir `producer_id` ile çalışır. Kapsama alınıp alınmayacağı bu sözleşmenin konusu değildir (8.1).

### 4.10 Kayıtların geri teslimi ve karta iletim

- **A8-K57.** Sim her kamera karesi `k` için `t_alg_k = t_teslim_k + L_alg` anını ve onu içeren A1 partisi `P_k`'yı belirler; kare `k`'ya ait parti başlığı ve kayıtları karta yalnız `P_k` içinde gider.
- **A8-K58.** Sim, kare `k`'nın sonucunu `M_k` anında değerlendirir: `M_k`'dan önce alınmış ve K28'den geçmiş bir yanıt varsa onun partisini, yoksa K63'teki durumlu partiyi `P_k`'ya koyar. Nesli `M_k` anında güncel olan her kamera periyodu için (`kare_sirasi` istek gönderilmeyen periyotlarda da arttığından bunlar dahil; A2-K49) karta tam bir parti başlığı gider; nesli eskimiş kare için karta başlık gitmez (K63 adım 0).
- **A8-K59.** `M_k`'dan sonra alınan yanıt karta iletilmez; `LATE` sayılır ve yanıtın kendisi A6'ya yazılır.
- **A8-K60.** Sim hiçbir kipte (SIL, HIL 1x, HIL 1x + bariyer) bileşeni beklemez: `P_k`'nın oluşturulması ve sim zamanının ilerlemesi bileşenin yanıtı için ertelenmez. Bu sürümde ayrı bir "kilitli adım" kipi yoktur.
- **A8-K61.** Gerçek zamandan hızlı koşan SIL çalıştırmasında, kamera algılamasına dayanan sonuçlar için tekrar üretilebilirlik güvencesi verilmez (K60'ın sonucu; sonuç bilgisayarın hızına bağlıdır). Kamera algılamasına bağlı davranışın tekrarlanabilir sınaması, A6'ya kaydedilmiş algılama akışının yeniden verilmesiyle (A6'nın girdi tekrarı düzeyi) yapılır.
- **A8-K62.** Karta giden kamera parti başlığını sim doldurur: `source` = CAM_FWD, `inst` = `camera_id`, `batch_seq` = `kare_sirasi`, `t_meas_ns` ve `window_ns` karenin değerleri, `calib_ver` yapılandırmanınki, `producer_id` ve `producer_ver` el sıkışmış bileşeninki (yoksa 0), `flags` = 0, `reserved` = 0.
- **A8-K63.** Her kamera karesinin nihai durumu yalnız burada, `M_k` anında ve bir kez atanır; atanan durum sonradan değişmez ve hiçbir başka kural durum atamaz. Durum şu sırayla ilk uyan adımdan seçilir: (0) karenin çalıştırma nesli `M_k` anında güncel değil → karta başlık gitmez, A6'ya `eski_nesil` (ya da A2'nin düşme nedeni 4) yazılır; (1) A2 kareyi teslim etmedi (A2'nin düşme nedeniyle düştü) → K76 tablosu; (2) el sıkışmış bileşen yok ya da bileşenin kamera yolu kapalı (K77) → `COMPONENT_DOWN`; (3) kare bileşene sunulmadı (K46: bileşen meşgul ya da karenin halkasıyla henüz el sıkışmamış) → `SKIPPED_BUSY`; (4) sunuldu ama `M_k`'ya kadar yanıt alınmadı → `LATE`; (5) yanıt K28'de reddedildi → K27 tablosunun son sütunu; (6) aksi halde bileşenin bildirdiği durum.
- **A8-K64.** Kamera kapalı kademede sim, sim zamanında 1 s'de bir (mimari seçimi) `flags` = `NO_FRAME`, `status` = `CAMERA_OFF`, `batch_seq` = 0, `count` = 0, `window_ns` = 0 ve `t_meas_ns` = konduğu A1 partisinin kapanış anı olan bir başlığı karta gönderir. `NO_FRAME` başlığı K16 ve K17 denetimlerine girmez ve akışın "son kabul edilen" değerlerini değiştirmez.
- **A8-K65.** Sim her kamera karesinin sonucunu karta giden durum başına sayar ve sayaçları A3 üzerinden yayımlar; OK dışındaki her durum, her parti reddi (nedeniyle), her `eski_nesil` reddi ve her geç yanıt A6'ya yazılır. A2'nin düşürdüğü her kare için A6'ya `kare_sirasi`, A2'nin özgün düşme nedeni kodu (K76) ve `S_d` yazılır.

### 4.11 Kart tarafındaki üretici (sonar)

- **A8-K66.** Kart içindeki sonar algılayıcısı kayıtlarını aynı yapıyla (`ida_det_record_t`, `ida_det_batch_t`) ve 4.1–4.5'teki kurallara uyarak üretir: başlığın zamanı işlenen tarama satırının gönderim anıdır (K11), boş partiler dahil. Bu partilerin çekirdeğe teslimi A4'ün, sim'e değerlendirme için aktarılması (yapılırsa) A1'in konusudur.

### 4.12 Halka, A2 oturumu ve çalıştırma nesli (A2 yaşam döngüsüyle; W1-uyum U14)

Halkanın durumları (etkin, emekli, serbest), emeklilik, yuva durumları ve çalıştırma nesli A2 §4.6–§4.10'dadır; bu başlık A8 bileşeninin o yaşam döngüsündeki iletilerini ve yükümlülüklerini tanımlar. Bu başlıktaki iletilerin adları ve kodları A8'indir (3.5).

- **A8-K70.** **Halka teklifi.** Sim, A8 el sıkışması tamamlanmış (K37) ve kamera yolu açık (K77) bileşene şu anlarda `RING_OFFER` gönderir: (a) `READY`'den hemen sonra, o kameranın etkin bir halkası varsa; (b) yeni bir A2 oturumu için halka oluşturulduğunda (A2-K65); (c) bir halka emekliye ayrıldığında, yeni halka için (A2-K86 adım 8), A2-K89 bu bileşenin kamera yolunu kapatmıyorsa. `RING_OFFER` alanları A2'nin halka bilgisinin ve halka başlığının değerleridir (3.5). Bileşen halkayı salt okunur eşler (K38) ve A2-K74'e göre doğrular: halka başlığındaki `sihirli`, `duzen_ana`, `oturum_kimligi`, `halka_nesli`, `yuva_sayisi`, `yuva_boyutu`, `toplam_boyut`, `genislik`, `yukseklik`, `satir_adimi`, `piksel_bicimi` `RING_OFFER`'dakilerle, `genislik`, `yukseklik`, `piksel_bicimi` ayrıca `CAMERA_CONFIG`'tekilerle aynıdır ve eşlenen boyut `toplam_boyut`'a eşittir.
- **A8-K71.** **Yeni nesli onaylayan yanıt.** Bileşen her `RING_OFFER`'a bir `RING_ACK` ile yanıt verir; yanıtlanmamış bir teklif varken daha yeni bir teklif gelirse yalnız en yenisini yanıtlar. `RING_ACK`'in `a2_session` ve `ring_gen` alanları yanıtladığı teklifinkilerdir; yanıtlanabilecek bir teklifle eşleşmeyen `RING_ACK` protokol hatasıdır (K42). Bileşen `RING_ACK(status = 0)`'ı ancak daha eski halkalardaki bütün okumalarını bitirip (eski halkaların eşlemelerini kaldırırsa K82'ye göre), o kareler için gönderilecek `DETECTION_BATCH`'leri (nesli geçerliyse; K49, K79) ve `FRAME_RELEASE`'leri gönderdikten sonra (yani yanıtlanmamış ve bırakılmamış karesi yokken) gönderir. `RING_ACK(status = 0)`, A2-K88'deki "A8 tüketicisinin daha yeni halkayla el sıkışması"dır: sim bileşeni yalnız bu halkadan teslim edilen karelere aday sayar (K44) ve bu bileşen adına tutulan, nesli `ring_gen`'den küçük emekli halkaları A2-K88'e göre serbest bırakabilir. `status` = 1 ya da 2 ise sim bu bileşen için kamera yolunu kapatır: `BYE(RING_MAP_FAILED)` gönderip bağlantıyı kapatır ve bileşen K77'deki gibi yalnız yeni bağlantı ve tam el sıkışmayla (K43) döner. Teklif gönderildikten sonra 5 s (sim'in duvar saati; mimari seçimi) içinde `RING_ACK` gelmezse sim `BYE(HANDSHAKE_TIMEOUT)` ile kapatır.
- **A8-K72.** **Kare bildirimi.** Sim `FRAME_AVAILABLE`'ı, A2-K53'teki edinme ve serbest bırakma çitlerini koyduktan sonra, karenin `S_d` anında gönderir (K46). Bildirim A2'nin o kare için tuttuğu `halka_nesli`, `yuva`, `istek_kimligi`, `kare_sirasi`, `calisma_nesli`, `yazim_sayaci`, `t_olcum_ns`, `pozlama_ns`, `yapilandirma_surumu` ve A2 oturum kimliğini değiştirmeden taşır.
- **A8-K73.** **Edinme çiti.** Bileşen `FRAME_AVAILABLE`'ı aldıktan sonra, yuvayı okumadan önce bir edinme çiti (`std::atomic_thread_fence(std::memory_order_acquire)`) koyar (A2-K53). A2-K53'ün bağladığı süreçler arası görünürlük varsayımı A8 için de geçerlidir; **doğrulanmadı** (A2-U13 sınar).
- **A8-K74.** **Bırakma.** Bileşen sunulan her kare için, yuvayı okumayı bitirince tam bir `FRAME_RELEASE` gönderir; üçlü `FRAME_AVAILABLE`'daki `request_id`, `ring_gen`, `slot`'tur. Bırakma karenin nesli eskimiş olsa da (A2-K83), halka emekliye ayrılmış olsa da gönderilir; tek istisna K75'tir. Sim bırakmayı A2-K58'e göre uygular: tüketici kimliği A8 bağlantısıdır; yinelenen, yabancı ya da üçlüsü uyuşmayan bırakma yok sayılır ve sayılır; bırakma başka hiçbir başvuruyu kaldırmaz.
- **A8-K75.** **A2 oturum sonu.** A2 oturumu bitince (A2-K64) sim, A8 el sıkışması tamamlanmış bileşene `A2_SESSION_END(a2_session)` gönderir ve o oturumun hiçbir karesini bir daha sunmaz. Bileşen bunu alınca o oturumun halkalarındaki okumasını bitirir, o oturumun kareleri için `FRAME_RELEASE` göndermez (sim o halkaları A2-K64 ile serbest bırakmıştır; yine de gelen bırakma yok sayılır ve sayılır), o oturumun bütün eşlemelerini K82'deki sırayla kaldırır (eski halkayı bırakır) ve yanıtlanmamış karesi için K49'a göre `DETECTION_BATCH` gönderir. Yeni A2 oturumunun halkası K70 (b) ile gelir; A8 oturumu A2 oturum sonundan etkilenmez.
- **A8-K76.** **A2 düşme nedenlerinin kapalı eşlemesi.** A2'nin bir kareyi teslim etmediği her durum (A2-K49, `SimKareDusmeNedeni`) karta aşağıdaki tabloya göre bildirilir. Bu bildirim A2-K49'daki "kare yok" bilgisinin A8'deki biçimidir: K62'ye göre doldurulmuş başlık, `count` = 0 ve tablodaki durumla bir parti; özgün A2 neden kodu karta gitmez, A6'ya `kare_sirasi` ile yazılır (K65). Tablo kapalıdır: A2 bu kodlardan başka bir kod tanımlamaz; sim başka bir kod üretirse bu bir sim hatasıdır, kare `FRAME_DROPPED` ile bildirilir ve `bilinmeyen_dusme_nedeni` olarak sayılır. Bir kare için "kare yok" bildirimi en çok bir kez ve A2-K97'deki tek nedenle yapılır.

| A2 kodu | A2 adı | Karta giden |
|---|---|---|
| 1 | `halka_dolu` | `FRAME_DROPPED` |
| 2 | `kamera_gec` | `FRAME_DROPPED` |
| 3 | `gorsellestirici_yok` | `FRAME_DROPPED` |
| 4 | `calisma_nesli_degisti` | başlık gitmez (K63 adım 0: kare eski nesildir) |
| 5 | `halka_yenilendi` | `FRAME_DROPPED` |
| 6 | `halka_yenileniyor` | `FRAME_DROPPED` |
| 7 | `halka_hatasi` | `FRAME_DROPPED` |
| 8 | `halka_emekli_hazir` | `FRAME_DROPPED` (kare üretilmiş ama emeklilik nedeniyle teslim edilmemiştir; `SKIPPED_BUSY` ya da `COMPONENT_DOWN` değildir) |
| 33 | görselleştirici: `daha_yeni_istek` (32 + 1) | `FRAME_DROPPED` |
| 34 | görselleştirici: `iptal` (32 + 2) | `FRAME_DROPPED` |
| 35 | görselleştirici: `geri_okuma_hatasi` (32 + 3) | `FRAME_DROPPED` |
| 36 | görselleştirici: `sahne_hazir_degil` (32 + 4) | `FRAME_DROPPED` |
| 37 | görselleştirici: `gecersiz_istek` (32 + 5) | `FRAME_DROPPED` |
| 38 | görselleştirici: `tutarsiz_surum` (32 + 6) | `FRAME_DROPPED` |
| 39 | görselleştirici: `halka_hatasi` (32 + 7) | `FRAME_DROPPED` |

- **A8-K77.** **Kamera yolu kapatılan tüketicinin dönüşü.** A2-K89 bu bileşenin kamera yolunu kapattığında sim yeni halkayı bu bileşene bildirmez, `BYE(CAMERA_PATH_CLOSED)` gönderir ve bağlantıyı kapatır; bu kapanış hiçbir başvuruyu kaldırmaz (A2-K60) ve bileşen adına tutulan emekli halkalar A2 oturumu bitene kadar tutulur. Bileşen K43'e göre yeniden bağlanır; yeni bağlantının tam el sıkışması (`HELLO` … `READY`, ardından K70 ve K71) A2-K89'daki yeni tüketici kimliğidir. Yeni bağlantı halkayla el sıkışana kadar kareler K63 adım 2'ye göre `COMPONENT_DOWN` alır.
- **A8-K78.** **Çalıştırma nesli bildirimi.** `WELCOME.run_gen` el sıkışma anındaki çalıştırma neslidir (A2-K80). Nesil arttığında sim, A8 el sıkışması tamamlanmış bileşene, yeni nesle ait ilk `FRAME_AVAILABLE`'dan önce `RUN_EPOCH(run_gen)` gönderir. `FRAME_AVAILABLE.run_gen` karenin neslidir; bileşen bildiği en yeni nesilden eski nesilli bir `FRAME_AVAILABLE` alırsa bunu protokol hatası sayar (K42).
- **A8-K79.** **Eski nesil sonuçlarının reddi.** Bileşen `RUN_EPOCH`'u aldıktan sonra nesli bundan eski kareler için `DETECTION_BATCH` göndermez; o karelerin okumasını bitirir ve K74'e göre bırakır (A2-K83). Sim `run_gen`'i güncel nesilden farklı olan her `DETECTION_BATCH`'i doğrulamadan reddeder (K28), `eski_nesil` nedeniyle sayar ve A6'ya yazar; karta hiçbir şey iletmez ve böyle bir sonucu hiçbir zaman güncel neslin verisi olarak kullanmaz (A2-K84). Nesli eskimiş kare K44'te yanıtlanmış sayılır; karta o kare için başlık gitmez (K63 adım 0).
- **A8-K80.** **Kare sayacının ömrü.** Kamera parti başlığının `batch_seq`'i A2'nin `kare_sirasi`'dır (A2-K49): sim süreci başlarken 1'dir, her kamera periyodunda (istek gönderilsin ya da gönderilmesin) 1 artar; sıfırlama, atlama, A2 oturumu ve A8 oturumu değişiminde sıfırlanmaz. K16'nın kesin artışı bu yüzden sim süreci boyunca sağlanır; 0 değeri yalnız `NO_FRAME` başlığındadır (hesap: u32 sayaç 30 Hz'te 2³² / 30 s ≈ 4,5 yılda sarar).
- **A8-K81.** **Zaman aralığı ve taşmasız karşılaştırma.** Kayıt ve başlıktaki bütün zaman alanları 0 … 2⁶² ns aralığındadır (A4-K08; başlıkta `IDA_DET_TIME_MAX_NS`); aralık dışı değer `TIME` reddidir. Zaman koşulları toplama yerine fark ile değerlendirilir: iki değer bu aralıkta olduğundan farkları `|d| ≤ 2⁶²` olur ve i64'te taşmaz; K13'teki koşul `−30 000 000 000 ≤ kayıt − başlık ≤ 1 000 000 000`, K17'deki koşul `yeni − son > 0` olarak hesaplanır. Aralık denetimi farktan önce yapılır (K27 `TIME` satırı).
- **A8-K82.** **Eşlemenin kaldırılma sırası.** Bileşen bir halka eşlemesini hangi yoldan kaldırırsa kaldırsın (bağlantının kopması ya da kapanması, kalp atışı veya el sıkışma zaman aşımı, `BYE` ya da `REJECT`, A2 oturum sonu (K75), halka yenilemesinden sonra eski halkanın bırakılması (K71), kamera yolunun kapanması (K77), sürecin olağan kapanışı) sıra her zaman aynıdır: (1) o eşlemede yeni okuma başlatılmaz; (2) o eşlemede süren bütün okumaların, bütün iş parçacıklarında bittiği kesinleştirilir (ör. okuyan iş parçacıklarının bitişi beklenir); (3) ancak bundan sonra eşleme kaldırılır (macOS `munmap`, Windows `UnmapViewOfFile`; kaldırılan görünümün adres aralığı geçersizdir: Microsoft `UnmapViewOfFile` belgesi). Bileşenin eşlemesini kaldırması sim'deki bekleyen başvuruları kaldırmaz; sim'in başvuruları koruyan kuralları (K45, K50; A2-K59, K60, K88) aynen sürer. K43, K71, K75 ve K77 eşleme kaldırma için bu kurala bağlıdır.

---

## 5. Hata durumları

| Durum | Algılama koşulu | Sim'in davranışı | Bileşenin davranışı | Karta akan |
|---|---|---|---|---|
| Bileşen hiç çalışmıyor | Kamera açık; el sıkışmış bağlantı yok | Beklemez (K60); her kare için `COMPONENT_DOWN` (K63); A3 uyarısı | — | `COMPONENT_DOWN` partileri |
| Bileşen çöktü | TCP kapandı | Başvuruyu kaldırmaz, yuvayı geri almaz (K45; A2-K60); bekleyen başvuru etkin halkadaysa halkayı emekliye ayırır (A2-K59, K86); bekleyen kare `M_k`'da `COMPONENT_DOWN` olur (K63 adım 2) | Yeniden başlatılırsa K43 | Her kare `COMPONENT_DOWN` |
| Bileşen asılı kaldı | K50 (1 s içinde bırakma yok) ya da K40 (3 s sessizlik) | K50: bağlantıyı kapatmaz, yuvayı geri almaz, etkin halkayı emekliye ayırır ve K70 ile yeni halkayı teklif eder; K40: `BYE(HEARTBEAT_TIMEOUT)`, kapanış (başvurular kalır, A2-K60) | Yeni halkayı ancak eski okumayı bitirip bıraktıktan sonra onaylar (K71) | `LATE`; yeni halka onaylanana kadar `SKIPPED_BUSY`; kapanıştan sonra `COMPONENT_DOWN` |
| Yanıt geç | Yanıt `M_k`'dan sonra | Reddeder, sayar, A6'ya yazar (K59) | Yanıtını yine gönderir (K49) | `LATE` |
| Bileşen meşgul | Karenin `S_d` anında bileşenin yanıtlanmamış ya da bırakılmamış karesi var | Kareyi bileşene sunmaz; kare sonradan aday olamaz (K46) | — | `SKIPPED_BUSY` (K63 adım 3) |
| Geçersiz çıktı | K27 tablosu | Bütün partiyi reddeder, nedeni sayar (K28, K30) | — | `CALIB` için `CALIB_MISMATCH`, diğerleri için `COMPONENT_ERROR`; `count` = 0 |
| Sürüm, şema, jeton, üretici ya da biçim uyuşmazlığı | `HELLO` (K34) | `REJECT`, kapatır | Yeniden bağlanmaz, hata koduyla sonlanır (K43) | `COMPONENT_DOWN` |
| Kamera sayısı 1 değil | `WELCOME` (K35) | Bu sürümde göndermez; senaryoda birden çok kamera varsa bileşeni başlatmaz | `BYE(CAMERA_COUNT)`, sonlanır | `COMPONENT_DOWN` |
| Yapılandırma geçersiz | K36 | Göndermez; A5 değeri geçersizse bileşeni başlatmaz, A3 hatası | `BYE(CONFIG_INVALID)`, sonlanır | `COMPONENT_DOWN` |
| `READY` geçersiz ya da gelmedi | K37 | `BYE(READY_INVALID)` ya da `BYE(HANDSHAKE_TIMEOUT)` | K43 ile yeniden dener | `COMPONENT_DOWN` |
| Halka eşlenemedi, düzeni uyuşmuyor ya da `RING_ACK` gelmedi | K70, K71 | `BYE(RING_MAP_FAILED)` ya da `BYE(HANDSHAKE_TIMEOUT)` | K43 ile yeniden dener | `COMPONENT_DOWN` |
| Halka emekliye ayrıldı (bu bileşen ya da başka bir tüketici yüzünden) | A2-K86 | Hazır kareleri `halka_emekli_hazir` ile düşürür; yeni halkayı K70 (c) ile teklif eder | Eski okumayı bitirir, bırakır, sonra `RING_ACK` (K71) | Düşen kareler `FRAME_DROPPED` (K76); onaya kadar `SKIPPED_BUSY` |
| Kamera yolu kapandı | A2-K89 | `BYE(CAMERA_PATH_CLOSED)`, kapanış; emekli halkalar oturum sonuna kadar tutulur (K77) | K43 ile yeni tüketici olarak döner | `COMPONENT_DOWN` |
| A2 oturumu bitti | A2-K64 | `A2_SESSION_END` (K75); o oturumun karelerini sunmaz | Okumayı bitirir, bırakma göndermez, eşlemeleri kaldırır | Görselleştirici yokken her kare `FRAME_DROPPED` (`gorsellestirici_yok`) |
| Çalıştırma nesli değişti | A2-K80 | `RUN_EPOCH` (K78); eski nesil sonuçlarını reddeder (K79) | Eski nesil için sonuç göndermez, bırakır | Eski nesil kareleri için başlık yok (K63 adım 0) |
| Kare biçimi, bütünlüğü ya da sürümü bozuk; `tutarsiz` bayraklı kare | K48, K51 | Partiyi doğrular ve iletir | Kayıt üretmez; `FRAME_INVALID` gönderir | `FRAME_INVALID` |
| Kalibrasyon uyuşmazlığı | Bileşen: karede ya da yapılandırmada; sim ve kart: K27 `CALIB` | Sim: `CALIB_MISMATCH` iletir | `CALIB_MISMATCH` gönderir | `CALIB_MISMATCH`; kart `CALIB` ile reddeder |
| Kare A2'de düştü (her neden) | A2-K49 | Bileşene sunmaz; özgün nedeni A6'ya yazar (K65) | — | K76 tablosu |
| Görselleştirici çalışmıyor (kamera açık) | A2 bağlantısı yok | Her kamera periyodu A2'de `gorsellestirici_yok` ile düşer | Kare almaz | `FRAME_DROPPED` |
| Kamera kapalı | Kademe kapalı | Bileşeni başlatmaz; bağlanırsa `REJECT(CAMERA_OFF)` | Sonlanır | 1 Hz `CAMERA_OFF` (K64) |
| Yapılandırma değişti | Kademe ya da kalibrasyon değişimi | `BYE(CONFIG_CHANGED)` (K41) | Yeniden bağlanır | Arada `COMPONENT_DOWN` |
| Sim çöktü ya da kapandı | Bileşende TCP kapandı | — | Eşlemeyi bırakır, K43 ile dener | (A1 sözleşmesinin konusu) |
| İkinci bileşen | Bağlı bileşen varken yeni bağlantı | `REJECT(ALREADY_CONNECTED)` (K32) | Sonlanır | Değişmez |
| Protokol hatası | K42 | `BYE(PROTOCOL_ERROR)`, kapatır | K43 | `COMPONENT_DOWN` |
| Bir parti başlığı karta ulaşmadı | Kart: akıştaki `batch_seq` boşluğu ya da başlık gelmemesi | A1'in konusu | — | Kartın bayatlık kuralı (A4-K118; A4'ten beklenen 5) |

---

## 6. Kalite kademelerinin etkisi

### 6.1 Kurallar

- **A8-K67.** Kamera kapalı kademede sim bileşeni başlatmaz, A8 üzerinden gelen bağlantıya `REJECT(CAMERA_OFF)` gönderir ve karta yalnız K64'teki başlıkları akıtır.
- **A8-K68.** Kamera açık kademelerde çözünürlük 640 × 480'i, kare hızı 30 Hz'i aşmaz (iskelet §10 md. 25). Kademelerin sayısal değerleri (çözünürlük, hız) kareyi üreten tarafın sözleşmesinde, A2'de tanımlıdır; A8 bunları `CAMERA_CONFIG` ile bileşene iletir ve bileşen el sıkışmada bildirilen çözünürlük ve hıza uyar (K51).
- **A8-K69.** Oturum sürerken kademe değişirse K41 uygulanır.

`L_alg` için sözleşme değeri: varsayılan 50 ms, geçerli aralık 5–500 ms (ikisi de mimari seçimi, tahmin; bileşenin gerçek işlem süresi ölçülmedi). Değer A5'te durur (A5'ten beklenen).

### 6.2 Sınır: kamera algılamasının pencere görünürlüğüne bağlı kalabilmesi

İskelet §8'in seçtiği kamera yolu (ana iş parçacığından `force_draw` ile çizim, eşzamanlı geri okuma) pencere örtülü, gizli ve simge durumundayken kare üretmiştir; ancak bu yalnız Mac'te, 5 dakikalık ön turda ölçülmüştür (MP6 §6). Windows'ta ölçülmedi. Windows'ta bu yol geçmez ve iskeletin yedeğine (pencerenin görünür tutulması) düşülürse:

- Pencere çizilemez olduğunda kareler A2'de düşer ve karta `FRAME_DROPPED` partileri akar (K63).
- İskelet §5'e göre sonarın göremediği yakın bölgede yalnız kamera algılama kayıtları bulunur. Bu durumda **sonarın kör bölgesindeki kamera algılama kayıtları pencerenin görünür olmasına bağlı kalır** (iskelet §10 md. 21). A8 yalnız bu kayıtların arayüzünü tanımlar; kayıtların çekirdekte nasıl kullanıldığını tanımlamaz. Bu, modelin kabul edilen bir sınırı olarak kayda geçer; giderilmesi için torpidonun ya da sensör setinin yetenekleri artırılmaz.
- Kamera kullanan bir çalıştırmanın geçerliliği düşen kare sayaçlarıyla (K65) ayrıca değerlendirilir; bu değerlendirmenin eşiği bu sözleşmenin konusu değildir.

### 6.3 Sınır: hızlandırılmış SIL'de tekrar üretilebilirlik

K60 gereği sim bileşeni hiçbir kipte beklemez. Bu yüzden gerçek zamandan hızlı koşan SIL çalıştırmasında hangi karelerin işlenip hangilerinin `LATE` ya da `SKIPPED_BUSY` olacağı bilgisayarın hızına bağlıdır ve kamera algılamasına dayanan sonuç tekrar üretilebilir değildir (K61). Tekrarlanabilir sınama, kaydedilmiş algılama akışının A6'nın girdi tekrarı düzeyiyle yeniden verilmesiyle yapılır (A6'dan beklenen 3). Bu sürümde ayrı bir "kilitli adım" kipi yoktur.

Emniyet kabulü kademeden bağımsızdır (iskelet §6): kamera kapalıyken ya da bileşen yokken karta yalnız kayıtsız, "sonuç yok" bildiren partiler akar; kamera algılamasının yokluğu karta her kademede aynı biçimde (K25, K63, K64) bildirilir.

---

## 7. Uygunluk

Test kodu bu işte yazılmaz. Test türleri: **birim** (tek tarafın kodu, sahte karşı tarafla), **sözleşme** (iki tarafın gerçek kodu ya da kayıtlı bayt akışı üzerinden, A8 soketi ve halka ile), **SIL**, **kartlı** (HIL).

| Kurallar | Tür | Sınama yöntemi | Gözlenecek büyüklük |
|---|---|---|---|
| K01, K03–K08, K15–K25, K27, K29 | Birim (kart tarafı doğrulayıcı) | K27 tablosunun her satırı için yalnız o satırı bozan bir parti; iki satırı birden bozan partilerle öncelik; sınır değerleri (`az` = ±π₃₂, `el` = ±π₃₂/2, `r` = 1000, `r` = 0, köşegen = alt sınır ve bir binary32 adım altı, eleman = üst sınır, ayrılmış bit, NaN, sonsuz) | Partinin kabul/ret kararı ve tek neden kodu; reddedilen partinin hiçbir kaydının kullanılmadığı |
| K07, K09 | Birim, iki platformda (Mac arm64 ve Cortex-M33 ya da Windows x64), FMA açık ve kapalı derlemelerle | Ortak kabul/ret örnek kümesi (aşağıda) iki platformda koşulur | İki platformda bit düzeyinde aynı karar kümesi |
| K02 | Birim (derleme) ve gözden geçirme | Başlıktaki alan listesi 3.2–3.3 ile bire bir; poz, hız, dünya konumu alanı yok | Alan listesi eşitliği |
| K10–K13, K17 | Birim | Bilinen `t_meas_ns`, `window_ns` ile `t_valid`; boş ve dolu kamera ve sonar partilerinde başlık zamanı; sonar kaydı K13 aralığının iki sınırında; geri giden parti zamanı | `t_valid`; kabul/ret |
| K12 | Sözleşme | Sahte görselleştirici bilinen `t_meas_ns`/`window_ns` ile kare yazar; bileşen ve sim'den geçen kaydın alanları karşılaştırılır | Bit düzeyinde eşitlik |
| K14 | Birim (A4 tarafı) | Kayıt alanlarında alım zamanı yok; alım zamanı zarfta | Alan listesi |
| K28, K30 | Birim (sim doğrulayıcı) | K27'nin her satırı ve `ECHO`, yetenek denetimi; K28 karar tablosunun her satırı (bileşen durumu × başarısız satır); aynı parti kart doğrulayıcısından da geçirilir | Sim'in nedeni; karta giden durum; kartın aynı partide (ECHO ve yetenek hariç) aynı nedeni verdiği; sayaçlar |
| K25, K63, K64 | Sözleşme ve SIL | Her durum nedeni ayrı ayrı oluşturulur (bileşen yok, öldürülmüş, meşgul, geç, geçersiz çıktı, kalibrasyon farkı, kare düşmüş, kamera kapalı) | Karta akan partinin `status`'u ve `count`'u |
| K26, 3.6 | Sözleşme | 30 Hz, her karede 4 kayıt üreten sahte bileşenle 600 s; A1'e verilen algılama baytları sayılır | Saniye başına bayt (≤ 11 970 + A1 kodlama farkı); parti hızı ≤ kare hızı |
| K31–K43 | Sözleşme (sahte sim ya da sahte bileşenle) | El sıkışma adımları; yanlış jeton, yanlış major, `producer_id` = 0, desteklenmeyen biçim, `camera_count` = 0 ve 2, K36 tablosunun her satırı için geçersiz yapılandırma (A2 §3.6 sınırlarının iki yanı), yanlış `config_ver_ack`, eksik `HELLO`, gecikmiş `READY`, bozuk `magic`, atlanan `msg_seq`, K42 örnek tablosunun her satırı (sınıf a ve b), minor farkı, kalp atışı kesilmesi | Gönderilen `REJECT`/`BYE` nedeni; bileşenin yeniden bağlanıp bağlanmadığı ve çıkış kodu; kapanış süresi (5 s, 3 s sınırları ±100 ms) |
| K38 | Sözleşme | Bileşen halkayı salt okunur eşler; yazma girişimi işletim sistemi hatası verir | Eşleme koruma bayrakları |
| K44–K51 | Sözleşme (gerçek A2 halkası ya da A2 kurallarına uyan sahte sim) | Bileşen yapay olarak yavaşlatılır (işlem süresi > kare periyodu); kare kimlikleri izlenir; yuva başlığında `tutarsiz` bayrağı, farklı `yapilandirma_surumu`, `kalibrasyon_surumu`, `calisma_nesli`, `istek_kimligi` ya da yazım sayacı; yapılandırmadan farklı boyutta kare; bileşen bırakmayı 1 s'den uzun geciktirir ya da bekleyen başvuruyla bağlantıyı koparır | Yolda en çok 1 kare; sunulan `kare_sirasi`'ların kesin arttığı; sunulmayan karenin bir daha sunulmadığı; `SKIPPED_BUSY` sayısı = `S_d`'de sunulamayan kare sayısı; `FRAME_INVALID` / `CALIB_MISMATCH`; bırakma gecikmesinde bağlantının açık kaldığı, yuvanın boşalmadığı ve halkanın emekliye ayrıldığı |
| K52–K55 | Sözleşme ve gözden geçirme | Sim'den bileşene giden bayt akışı kaydedilir ve 3.5'e göre ayrıştırılır; halka okunabilir bölgesi A2 tanımıyla karşılaştırılır; bileşenin açtığı soketler işletim sistemi araçlarıyla listelenir | Tanımsız alan sayısı = 0; bileşenin tek bağlantısı A8 |
| K56, K22 | Sözleşme | A8 üzerinden `SYNTHETIC` = 1 kayıt gönderen sahte bileşen | `FLAGS` reddi; karta `COMPONENT_ERROR`, kayıt gitmez |
| K57–K60 | SIL ve kartlı (HIL 1x ve bariyer) | Bilinen `L_alg` ile her kare için `P_k`'nın sim zamanı; bileşen `M_k`'nın hemen öncesinde ve sonrasında yanıt verir; bileşen hiç yanıt vermez | Her `kare_sirasi` için karta tam bir başlık; başlığın konduğu partinin sim zamanı = `P_k`; sim zamanının bileşen yüzünden durmadığı (kaçırılan adım sayacı değişmez) |
| K61 | SIL | Aynı senaryo, kaydedilmiş algılama akışı A6 girdi tekrarıyla iki kez verilir | Karta giden partilerin iki koşuda bit düzeyinde aynı olduğu |
| K65 | Sözleşme | Her durumdan bilinen sayıda | A3 sayaçları ve A6 kayıtları sayıyla eşit |
| K66 | Birim (kart tarafı) | Boş ve dolu sonar partileri kart doğrulayıcısından geçer | Kabul/ret; başlık zamanı = satırın `t_tx`'i |
| K70, K71 | Sözleşme | Emeklilik (bileşen kaynaklı ve başka tüketici kaynaklı); art arda iki emeklilik; eşlenemeyen ya da düzeni uyuşmayan halka; bırakılmamış kare varken gelen teklif | `RING_ACK`'in eski kare bırakılıp yanıtlanmadan gelmediği; yalnız en yeni teklifin yanıtlandığı; onaydan önce yeni halkadan kare sunulmadığı; `RING_ACK(status ≠ 0)`'da kamera yolunun kapandığı |
| K72–K74 | Sözleşme | Bildirim alanları A2'nin istek ve yuva başlığıyla karşılaştırılır; yinelenen, yabancı ve geç bırakmalar | Alan eşitliği; her sunulan kare için tam bir bırakma; bırakmanın `DETECTION_BATCH`'ten önce geldiği; yabancı bırakmanın yok sayılıp sayıldığı |
| K75 | Sözleşme | Görselleştirici bağlantısı koparılır, yeniden bağlanır | `A2_SESSION_END`; o oturum için bırakma gelmediği; yeni oturumun halkasının K70 (b) ile teklif edildiği; A8 oturumunun sürdüğü |
| K76 | Birim (sim) | A2'nin 1–8 ve 33–39 kodlarının her biriyle düşen kare; tanımsız kod | Karta giden durum tablodaki gibi; 4'te başlık yok; A6'da özgün kod |
| K77 | Sözleşme | Tüketici başına emekli halka sınırını aşan emeklilik | `BYE(CAMERA_PATH_CLOSED)`; yeni bağlantıya kadar `COMPONENT_DOWN`; yeni bağlantının yeni tüketici sayıldığı |
| K78, K79, K17 | Sözleşme ve SIL | Sıfırlama ve geri atlama; eski nesil karesi bileşendeyken nesil değişir; bileşen `RUN_EPOCH`'tan önce ve sonra eski nesil sonucu gönderir | `RUN_EPOCH`'un yeni nesil karesinden önce geldiği; eski nesil sonucunun karta gitmediği ve `eski_nesil` sayıldığı; geri atlamadan sonra (son kabul edilen zaman 10 s, yeni nesilde 1 s'lik geçerli parti) ne sim'in kabul kapısının ne kartın yeni nesil partisini zaman ilerlemesi nedeniyle reddettiği; `batch_seq` ve `det_seq` kabul durumlarının korunduğu |
| K80 | SIL | Sıfırlama, atlama ve A2/A8 yeniden bağlanmaları içeren koşu | Karta giden `batch_seq`'in kesin arttığı ve kamera periyotlarında boşluk olmadığı |
| K81 | Birim (kart ve sim doğrulayıcı) | `t_meas_ns` = 0, 2⁶², 2⁶² + 1, −1; K13 farkının sınırları 2⁶² yakınında | Kabul/ret; taşma olmadığı (işaretli taşma denetleyicisiyle derleme) |
| K82 | Sözleşme (çok iş parçacıklı bileşen) | Bir işçi piksel okurken her kapanış yolu tek tek tetiklenir (kopma, kalp atışı zaman aşımı, `BYE`, `A2_SESSION_END`, halka yenileme, kamera yolu kapanması, olağan kapanış); bellek denetleyicisiyle (ASan/sayfa koruması) koşulur | Eşleme kaldırma çağrısının her yolda süren okumaların bitişinden sonra geldiği; kaldırılmış adrese erişim olmadığı; sim'deki başvuruların değişmediği |
| K67–K69 | SIL | Kapalı, sade, tam kademelerde koşu; koşu sırasında kademe değişimi | Kapalıda 1 Hz `CAMERA_OFF`; `CAMERA_CONFIG` değerlerinin A2'ninkilerle aynı olduğu; değişimde `BYE(CONFIG_CHANGED)` ve yeni oturum |
| 6.2 sınırı | Kartlı ve Windows ölçümü (MP8) | Windows'ta pencere görünmezken kamera açık koşu | `FRAME_DROPPED` oranı; birincil yolun Windows'ta geçip geçmediği |

**K07 için ortak kabul/ret örnekleri** (`kind` = B2; `L_rr = L_ar = L_er = 0`; değerler binary32):

| # | `L_aa` | `L_ea` | `L_ee` | Karar | Neden |
|---|---|---|---|---|---|
| E1 | 1,0 | 0,10000000149011612 | 2,0243071048753336·10⁻⁵ | kabul | Hepsi sonlu, sınırlar içinde, köşegenler ≥ 1·10⁻⁶ |
| E2 | 1,0 | 0,10000000149011612 | 0 | ret (`UNCERT`) | `L_ee` < alt sınır |
| E3 | 1,0·10⁻⁶ (binary32'de 9,99999997·10⁻⁷) | 0 | 1,0 | kabul | Köşegen alt sınıra eşit (sınır sabitin binary32 değeridir) |
| E4 | 1,0 | 10,000000953674316 (binary32'de 10'dan sonraki değer) | 1,0 | ret (`UNCERT`) | Mutlak değer > 10 |
| E5 | 1,0 | NaN | 1,0 | ret (`UNCERT`) | Sonlu değil |
| E6 | 1,0 | 0 | 1,0 (ayrıca `L_ar` = 0,5) | ret (`UNCERT`) | Ölçülmeyen `r`'nin sütunu 0 değil |

E1, inceleme karşı örneğindeki kovaryansın (`c_aa` = 1, `c_ae` = 0,10000000149011612, `c_ee` = 0,010000000707805157) çarpanıdır: üretici çarpanı kendi hesabıyla bulur ve gönderir; iki alıcı yalnız gelen bitleri karşılaştırdığı için aynı kararı verir. Yeniden kurulan `c_ee` binary32'de ayrı çarpma-toplamayla da tek işlemli çarpma-toplamayla da 0,010000000707805157 çıkar (hesap); kabul kararı buna dayanmaz.

---

## 8. Açık kalanlar

### 8.1 Ölçüm ya da karar bekleyenler

| # | Nokta | Etiket | Neyle kapanır |
|---|---|---|---|
| 1 | Partide en çok 4 kayıt (3.6) | Tahmin | A1 sözleşmesinin periyot dolgusu ve MP5 (mesaj biçimi maliyeti). Pay varsa ayrı kararla 8'e çıkarılabilir |
| 2 | `L_alg` varsayılanı 50 ms ve 5–500 ms aralığı | Tahmin | Bileşenin ilk sürümünde işlem süresinin ölçümü (W5) |
| 3 | `chol` sınırları (köşegen alt sınırı 1 mm ve 1 µrad; eleman üst sınırı 1000 m ve 10 rad) | Tahmin | Kart içi sonar algılamasının ve bileşenin ilk sürümleri (W4, W5); sınırlar yalnız bozuk değeri ayıklar |
| 4 | Sonar kaydının parti zamanına göre aralığı (−30 s … +1 s) | Tahmin | Kart içi sonar algılamasının uygulaması (W4) |
| 5 | Süre sınırları: `HELLO` 5 s, `READY` 5 s, `RING_ACK` 5 s, kalp atışı 1 s / 3 s, yeniden bağlanma 1 s, `CAMERA_OFF` başlığı 1 Hz (bırakma süresi 1 s A2'nindir, A2-K59) | Tahmin | Sözleşme testleri ve ilk bütünleşik koşu (W5) |
| 6 | 6.2'deki görünürlük sınırı | Ölçülecek | MP8 (Windows'ta birincil kamera yolu) |
| 7 | Anlamsal algılama üreteci (yer gerçeğinden kayıt) kapsama alınacak mı | Karar bekliyor | Bölüm 03 §8.4 ve 05 §8; mimar. A8 yalnız bağlanamayacağını (K56) ve bu sürümde `SYNTHETIC` kaydın reddini (K22) sabitler |
| 8 | Kart tarafında akışın tazelik süresi (kamera akışından başlık gelmezse "algılama yok" sayılması) | Kapandı: A4-K118 (`T_det_stale`) | A4'ten beklenen 5 |
| 9 | Kamera kademe değerleri, bozulma modelinin kodları ve katsayılarının anlamı | A2 | A2'den beklenen 6 |
| 10 | `f32` alanların IEEE 754 binary32 olduğu kabulü | Doğrulanmadı (derleyici ve hedef için) | W2 depo iskeletinde derleme denetimi |

SIL'de bekleme konusu kapandı: mimar kararıyla bu sürümde ayrı bir "kilitli adım" kipi yoktur; sınır 6.3'te ve K61'de.

### 8.2 Başka sözleşmelerden beklenenler

**A1'den beklenen**

1. Sim → kart yönünde `ida_det_batch_t` ve `ida_det_record_t`'nin bütün alanlarını (ayrılmış baytlar hariç) bit düzeyinde aynı taşıması; `f32` binary32, tamsayılar little-endian (MAVLink 2 zaten little-endian'dır; alan sırasını kendi kuralına göre değiştirmesi serbesttir).
2. Kare `k`'nın parti başlığını ve kayıtlarını `P_k`'da, yani kapanışı `t_alg_k`'ya eşit ya da ondan sonraki ilk partide taşıması; bir kamera partisini birden çok A1 partisine yaymaması.
3. Algılama trafiği için tek kameraya göre bütçede yer ayırması: en kötü durumda sim → kart ortalama 11 970 B/s, tek periyotta tepe 399 B (3.6; hesap). Bütçe yetmezse 8.1 md. 1 yeniden ele alınır.
4. Kart → sim yönünde sonar partilerinin değerlendirme için taşınıp taşınmayacağı ve hızı; taşınırsa aynı alanlar.
5. Kamera akışında kaybolan bir parti başlığının kartta nasıl görüneceği (boşluk denetimi).
6. Sim'in çalıştırma neslinin her değişiminin (sıfırlama, atlama, yeni çalıştırma; A2-K80) A1'de oturum kapanışı ve yeni oturum olması (A1-K128; oturum sonu çağrısı A1-K107); karta ayrı bir nesil iletisi gitmez (K17).

**A2'den beklenen**

A2'nin "A8'den beklenen" listesinin (16 madde) A8 kural eşlemesi bu sözleşmenin karşılığıdır; A2'den beklenenler:

1. Kamera isteğinde ve kare üst verisinde `t_olcum_ns`'in iskelet §3.1'e uyması (pozlama sonu) ve pozlama süresinin ayrı taşınması (A2-K41; A8-K11, K12).
2. Tek piksel biçimi ve onun 0–31 aralığında bir kodla bildirilmesi (A2 §3.8; A8-K34 `pixel_format_mask`).
3. Halkanın poz, montaj, iç parametre ya da başka bir yer gerçeği taşımaması (A2-K55; A8-K54).
4. Bekleyen tüketici kümesi, bırakma üçlüsü, kopma ve zaman aşımında emeklilik (A2-K56–K61, K86–K89); A8 bunlara K45, K50, K74 ile uyar.
5. Yuva başlığındaki `yazim_sayaci`, `istek_kimligi`, `calisma_nesli`, `halka_nesli`, `tutarsiz` bayrağı, `yapilandirma_surumu`, `kalibrasyon_surumu` (A2 §3.8; A8-K48).
6. Kamera kademe değerlerinin ve `KameraYapilandirmasi` alanlarının geçerli aralıklarının tek kaynağı olması (A2 §3.6, §6; A8-K36, K68); `1000 · hiz_pay / hiz_payda`'nın tamsayı olması (A8 `rate_mhz`).
7. `kare_sirasi`'nın sim süreci ömrü (A2-K49; A8-K80).
8. Düşen her karenin `SimKareDusmeNedeni` koduyla sim içinde bildirilmesi; kod kümesinin 1–8 ve 33–39 ile sınırlı kalması (A8-K76).
9. Halka adının en çok 63 ASCII karakter olması (`RING_OFFER.ring_name`).
10. Bu sürümde tek kamera, `kamera_kimligi` = 0 (A8-K35).
11. Emeklilikte yeni halkanın (A2-K86 adım 8), yeni A2 oturumunun halkasının (A2-K65) ve A2 oturum sonunun (A2-K64) A8 yoluna verilmesi; A8 bunları `RING_OFFER` ve `A2_SESSION_END` ile taşır (K70, K75).

**A4'ten beklenen**

1. Algılama partilerinin ve kayıtlarının çekirdeğe `ida_det_batch_t` ve `ida_det_record_t` yapılarıyla (bu başlıktan) teslimi; alım zamanının kayıttan ayrı bir zarf alanında taşınması (K14).
2. **Parti atomikliği** (A4'te; W1-uyum P2-14): kart tarafındaki doğrulayıcının K27 tablosunun "Kart" sütunundaki satırları aynı sırayla uygulaması ve bir kayıt geçersizse partiyi bütünüyle reddetmesi (K29); bütün kayıtlar doğrulanmadan hiçbirini kabul etmemesi (A4-K41).
3. Ret sayacı: çekirdekte tek toplam `det_rejected` yeterlidir (A4-K41); akış ve neden başına sayaç çekirdekten beklenmez (K30; W1-uyum P2-15).
4. `calib_ver` karşılaştırması için akış başına kalibrasyon sürümünün kalıcı parametrelerde bulunması (K20); akış başına "son kabul edilen" `batch_seq`, `det_seq` ve `t_meas_ns` durumunun tutulması ve yalnız zaman durumunun oturum sonu çağrısında sıfırlanması (K15–K17; A4-K122; nesil değişimi A1'de oturum yenilenmesidir, A1-K128).
5. **Bayatlık eşiği** (A4-K118; W1-uyum P2-16): `T_det_stale` (varsayılan 2 s, aralık 0,1–10 s) boyunca başlık gelmeyen algılama akışının bayat sayılması ve bunun A4-K118'deki durum bitiyle bildirilmesi. A8'in güvencesi: kamera açıkken her kamera periyodunda, kapalıyken 1 Hz başlık gelir (K58, K64).
6. Zaman tipinin işaretli 64 bit nanosaniye ve geçerli aralığın 0 … 2⁶² ns olması (A4-K08; A8-K81).
7. Belirsizliğin `chol` (Cholesky çarpanı) olarak teslim edilmesi; kovaryansın kurulup kurulmayacağı çekirdeğin seçimidir (K09).

**A5'ten beklenen**

1. `L_alg` parametresi (6.1 aralığıyla).
2. Kameranın nominal montajı ve `calib_ver`'i (otopilotun gördüğü değerler) ile fizik gerçeği montajının ayrı tutulması; montajın S çerçevesinde verilmesi (K53; W1-uyum U13).
3. Hedef sınıfının kapalı sayısal tablosu: 0 `bilinmiyor`, 1 `sabit_nesne`, 2 `hareketli_nesne`, 3 `yapi`; 4–255 ayrılmış (K23; W1-uyum P3-16).
4. Kamera kademesinin seçimi; en çok bir etkin kamera ve `inst` = 0 (K35).

**A3 ve A6'dan beklenen**

1. A3: K30 ve K65 sayaçlarının (ret nedeni başına, `eski_nesil`, karta giden durum başına, geç yanıt) yayımlanması; bileşenin başlatılması (K31) çalıştırma kontrolünün işidir; K35 ve K36'daki başlatma hatalarının ve K77 kamera yolu kapanmasının bildirilmesi.
2. A6: karta giden her algılama partisinin konduğu A1 partisiyle birlikte kaydı (boş partiler dahil); nihai kare durumu; A2'nin özgün düşme nedeni (K76); geç yanıtların, `eski_nesil` retlerinin ve parti retlerinin nedenleriyle kaydı; A8 sürümünün `major`/`minor` iki alan olarak kaydı (W1-uyum U18).
3. A6: girdi tekrarı düzeyinde, kaydedilmiş algılama akışının (karta giden partiler, konduğu A1 partileriyle) çekirdeğe aynı A1 partilerinde yeniden verilebilmesi; hızlandırılmış SIL'de kamera algılamasına bağlı davranışın tekrarlanabilir sınaması bununla yapılır (K61).

---

## 9. Açık bulgu kapanış tablosu

`docs/mimari/acik-bulgular.md` "A8" başlığındaki bulgular:

| Bulgu | Durum | Sözleşmede nerede | Gerekçe |
|---|---|---|---|
| AB-104 | kapandı | K15–K19; 3.3 | Kayda `batch_seq` ve parti başlığı eklendi; zaman ilerleme denetimi parti düzeyine alındığı için aynı karedeki birden çok algılama takılmaz |
| AB-105 | kapandı | K52, K53, K54, K56; A2'den beklenen 3 | Bileşen yalnız nominal montajı alır, fizik gerçeği montajı ve hiçbir yer gerçeği verilmez; yer gerçeğinden kayıt üreten üretici A8'e bağlanamaz |
| AB-106 | kapandı | Başlık, §1 | Bağlantı iskelet 0.6'daki adıyla A8 olarak sözleşmeye bağlandı; 07 §3.5'in "adsız arayüz" ifadesi geçersizdir |
| AB-236 | uygulamaya devredildi | 6.2, K63 | Kamera çizim yolu (force_draw + eşzamanlı okuma) iskelet §8'de seçildi ve Godot sahnesi işinde (W5, bölüm 07) uygulanır; A8'in payı kapandı: A8 çizim yoluna ve pencere görünürlüğüne bağlı bir varsayım içermez, düşen kare karta `FRAME_DROPPED` olarak akar ve görünürlük sınırı 6.2'de belgelendi |
| AB-237 | kapandı | K34, K36, K48, K51; A2'den beklenen 2 | Bileşen biçim varsayımı yapmaz; A2'nin seçtiği tek biçimi el sıkışmada ve her karede denetler; biçimin kendisi (RGB8 ya da RGBA8) A2'nin seçimidir |
| AB-238 | kapandı | K57–K60 | Asenkron geri okuma iskelet §8'de kullanım dışıdır; A8'in teslim kuralı kare hattının hızına dair bir varsayım içermez, sonuç yalnız `M_k`'ya göre kabul ya da reddedilir |

Özet: 6 bulgunun 5'i kapandı, 1'i uygulamaya devredildi; açık ya da kapsam dışı yok. Açık kalan kritik bulgu yok (tek kritik bulgu AB-236 devredildi).

**Başka gruplarda olup A8'e değinen bulgular (bilgi için; kapanışları kendi işlerindedir):**

| Bulgu / satır | Grup | A8'in katkısı |
|---|---|---|
| AB-047, U-09 | A1 | Partide en çok 4 kayıt ve `t_sigma_ns`, `calib_ver` alanları sabitlendi (3.2, 3.6); bayt hesabı 3.6'da |
| AB-049, AB-050, AB-051 | A2 | A8 okuyucusu halkaya yazmaz (K38); başvurusunu yalnız üçlüyle, kare başına bir kez bırakır (K74); kopma ve zaman aşımı yuvayı bırakmaz, etkin halka emekliye ayrılır (K45, K50); yeni halkayı eski okumaları bitirmeden onaylamaz (K71); düzeneğin kendisi A2'dedir |
| AB-053, U-05 | A2 | Başlık ve kayıt karenin `t_meas_ns` ve `window_ns`'ini değiştirmeden taşır, tek geçerlilik anı `t_valid` ile türetilir (K10–K12); kart alım zamanı kayıttan ayrıdır (K14) |
| AB-142 | Atış kontrol uygulaması | Belirsizlik Cholesky çarpanı olarak taşınır; ölçülmeyen bileşenin satırı ve sütunu 0'dır ve kabul kararı pozitif tanımlılık sınamasına dayanmaz (K03, K07, K09) |
| AB-155 | A5 | `class_id` A5'in kapalı sınıf tablosuna bağlandı; 4–255 reddedilir (K23) |
| U-21 | — | AB-106 ile kapandı |
| U-22 | — | AB-105 ile kapandı |
| U-29 | A8; A1 | Bölüm 03'ün `RB2`/`B2`/`RB3` ve `SENSOR` kuralları ile 07'nin kare kimliği bu sözleşmede eşlendi (3.2, K01, K03–K05, K16); A1 eşlemesi A1'den beklenen 1 |
