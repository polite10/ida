# A8 — Algılama kaydı ve sim sunucusu ↔ kamera algılama bileşeni sözleşmesi

| | |
|---|---|
| Sürüm | 1.0-aday, düzeltme turu 1 (2026-11-19) |
| Görev | `docs/gorevler/W1-A8.md`; ortak kurallar `docs/gorevler/W1-ortak.md` |
| Dayanak | `docs/mimari/00-iskelet.md` sürüm 0.8 (§3 A8 satırı, "Algılama kaydının çerçevesi", "Kamera ile hedef algılama"; §3.1; §4; §5 "Bilinen sınır"; §6; §8; §10 md. 6, 21, 25) |
| Makinece okunur tanım | `core/include/ida/algilama_kaydi.h` (ISO C17; C++20 ile de derlenir) |
| Yazan | `ida-uygulayici` (Opus); inceleyen sol (`docs/sozlesmeler/inceleme/A8-sol.md`; yanıt `A8-yanit.md`) |

Bu belge kural yazar. Mimari bölümlerdeki (03, 05, 07) bir şey buraya girmediyse A8 için bağlayıcı değildir. Çelişkide öncelik: kullanıcı kararları > iskelet 0.8 > ölçüm raporları ve görev belgeleri > bölümler 01–09.

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
| 1 | `t_meas_ns` | i64 | ns (sim zamanı) | K12, K13 | Ölçüm zamanı (iskelet §3.1; K10) | `TIME` |
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
| 18 | `class_id` | u8 | — | her değer | Hedef sınıfı; 0 = bilinmiyor (K23) | — (bilinmeyen değer 0 sayılır) |
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
| 1 | `t_meas_ns` | i64 | ns | ≥ 0; K17 | Girdi biriminin ölçüm zamanı (K11); `NO_FRAME` başlığında K64 | `TIME` |
| 2 | `batch_seq` | u32 | — | akışta öncekinden büyük; `NO_FRAME`'de 0 | Kamerada `kare_seq` (A2); sonarda üreticinin satır sayacı (K16) | `SEQ`; `NO_FRAME`'deki 0 koşulu `STRUCT` |
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
| 1 | `SKIPPED_BUSY` | sim ya da bileşen | Kare bileşene sunulmadı ya da bileşen işlemeden atladı |
| 2 | `LATE` | yalnız sim | Sonuç `M_k`'ya yetişmedi |
| 3 | `COMPONENT_DOWN` | yalnız sim | Bağlı, el sıkışmış bileşen yok |
| 4 | `FRAME_DROPPED` | yalnız sim | Kare üretilmedi, düştü ya da `M_k`'ya kadar hazır olmadı (A2) |
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

Bütün çok baytlı alanlar little-endian'dır. `f32` IEEE 754 binary32; `f64` taşınmaz. Metin alanları ASCII'dir, NUL ile doldurulur; ilk NUL'dan sonrası yok sayılır.

**Çerçeve başlığı (16 bayt), her mesajın önünde:**

| Konum | Alan | Tip | Değer / anlam |
|---|---|---|---|
| 0 | `magic` | u32 | `0x38414449` (baytlar `'I' 'D' 'A' '8'`) |
| 4 | `type` | u16 | Mesaj türü (aşağıda) |
| 6 | `flags` | u16 | 0 |
| 8 | `length` | u32 | Yük uzunluğu, bayt; en çok 4096 |
| 12 | `msg_seq` | u32 | Gönderenin bağlantı başına sayacı; ilk mesajda 1, her mesajda +1 |

**Mesaj türleri:**

| `type` | Ad | Yön | Yük (bayt) |
|---|---|---|---|
| `0x0001` | `HELLO` | bileşen → sim | 56 |
| `0x0002` | `WELCOME` | sim → bileşen | 92 |
| `0x0003` | `REJECT` | sim → bileşen | 64 |
| `0x0004` | `CAMERA_CONFIG` | sim → bileşen | 84 |
| `0x0005` | `READY` | bileşen → sim | 8 |
| `0x0010` | `FRAME_AVAILABLE` | sim → bileşen | 32 |
| `0x0011` | `DETECTION_BATCH` | bileşen → sim | 40 + 80 × `count` |
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
| 20 | `pixel_format_mask` | u32 | Desteklenen piksel biçimleri; bit `n` = A2'nin `n` numaralı biçim kodu |
| 24 | `name` | char[32] | Bileşenin adı (yalnız A3/A6) |

**`WELCOME` (92 bayt):**

| Konum | Alan | Tip | Anlamı |
|---|---|---|---|
| 0 | `a8_major` | u16 | Sim'in sürümü |
| 2 | `a8_minor` | u16 | Ortak minor (K39) |
| 4 | `det_schema_major` | u16 | Sim'in şema sürümü |
| 6 | `det_schema_minor` | u16 | Ortak minor |
| 8 | `session_id` | u64 | Bu A8 oturumunun kimliği |
| 16 | `ring_generation` | u32 | A2 halkasının nesli |
| 20 | `det_seq_start` | u32 | Bileşenin ilk kaydına vereceği `det_seq` (K15) |
| 24 | `camera_count` | u8 | Ardından gelecek `CAMERA_CONFIG` sayısı; bu sürümde tam 1 (K35) |
| 25 | `reserved` | u8[3] | 0 |
| 28 | `ring_name` | char[64] | A2 halkasının işletim sistemi adı |

**`REJECT` (64 bayt):** `reason` u16 (konum 0; kodlar aşağıda), `reserved` u16 (2), `text` char[60] (4; yalnız tanı).

**`CAMERA_CONFIG` (84 bayt; geçerlilik koşulları 4.7 K36 tablosunda):**

| Konum | Alan | Tip | Birim | Anlamı |
|---|---|---|---|---|
| 0 | `config_ver` | u32 | — | Yapılandırmanın sürümü (oturum boyunca sabit; K41) |
| 4 | `camera_id` | u8 | — | Kayıtta `inst` |
| 5 | `source` | u8 | — | 2 (`CAM_FWD`) |
| 6 | `tier` | u8 | — | 1 sade, 2 tam (değerleri A2'dedir; 6. başlık) |
| 7 | `distortion_model` | u8 | — | 0 bozulma yok; diğer kodlar ve katsayıların anlamı A2'nin kamera tanımındadır |
| 8 | `width` | u16 | piksel | Karenin genişliği |
| 10 | `height` | u16 | piksel | Karenin yüksekliği |
| 12 | `rate_mhz` | u32 | mHz | Kare hızı × 1000 |
| 16 | `fx`, `fy` | f32 × 2 | piksel | Odak uzaklığı |
| 24 | `cx`, `cy` | f32 × 2 | piksel | Ana nokta; piksel (0,0) sol üst pikselin merkezi |
| 32 | `dist[5]` | f32 × 5 | — | Bozulma katsayıları; kullanılmayan 0 |
| 52 | `mount_l_b[3]` | f32 × 3 | m | **Nominal** montaj konumu, gövde referans noktasına göre (K53) |
| 64 | `mount_q_bs[4]` | f32 × 4 | — | **Nominal** montaj yönelimi, birim kuaterniyon (w, x, y, z); S'den gövdeye |
| 80 | `calib_ver` | u16 | — | Kayıtlara ve başlığa yazılacak sürüm (K20) |
| 82 | `pixel_format` | u16 | — | A2'nin biçim kodu; bu oturumda gelecek bütün kareler bu biçimdedir |

**`READY` (8 bayt):** `config_ver_ack` u32 (konum 0; alınan `CAMERA_CONFIG`'in `config_ver`'i), `ring_mapped` u8 (4; 1 = halka salt okunur eşlendi), `reserved` u8[3] (5; 0). Geçerlilik K37.

**`FRAME_AVAILABLE` (32 bayt):**

| Konum | Alan | Tip | Birim | Anlamı |
|---|---|---|---|---|
| 0 | `t_meas_ns` | i64 | ns | Karenin ölçüm zamanı (A2 isteğinden) |
| 8 | `frame_seq` | u32 | — | `kare_seq` (A2) |
| 12 | `window_ns` | u32 | ns | Pozlama süresi (A2 isteğinden) |
| 16 | `ring_generation` | u32 | — | Halkanın nesli |
| 20 | `budget_ns` | u32 | ns | Gönderim anında sim'in saatine göre `M_k`'ya kalan süre; yalnız bilgi (K58) |
| 24 | `slot` | u16 | — | Halkadaki yuva numarası |
| 26 | `camera_id` | u8 | — | Kamera |
| 27 | `reserved` | u8 | — | 0 |
| 28 | `config_ver` | u32 | — | Karenin üretildiği yapılandırma |

**`DETECTION_BATCH` (40 + 80 × `count` bayt):**

| Konum | Alan | Tip | Anlamı |
|---|---|---|---|
| 0 | `proc_wall_ns` | u32 | `FRAME_AVAILABLE` alımından yanıta bileşenin kendi saatiyle geçen süre (yalnız A3/A6) |
| 4 | `reserved` | u32 | 0 |
| 8 | parti başlığı | 32 bayt | 3.3'teki alanlar, başlık dosyasındaki konumlarla (8 + konum), `reserved` 0 |
| 40 + 80·i | kayıt `i` | 80 bayt | 3.2'deki alanlar, başlık dosyasındaki konumlarla, `reserved` 0 |

**`BYE` (4 bayt):** `reason` u16 (0), `reserved` u16 (2).

**`REJECT` ve `BYE` neden kodları:**

| Kod | Ad | Gönderen | Kullanıldığı kural |
|---|---|---|---|
| 1 | `VERSION_MAJOR` | sim | K34 |
| 2 | `SCHEMA_MAJOR` | sim | K34 |
| 3 | `TOKEN` | sim | K34 |
| 4 | `FORMAT_UNSUPPORTED` | sim | K34 |
| 5 | `RING_MAP_FAILED` | sim | K37 |
| 6 | `PROTOCOL_ERROR` | iki taraf | K42 |
| 7 | `HEARTBEAT_TIMEOUT` | iki taraf | K40 |
| 8 | `SLOT_HOLD_TIMEOUT` | sim | K50 |
| 9 | `CONFIG_CHANGED` | sim | K41 |
| 10 | `SHUTDOWN` | iki taraf | olağan kapanış |
| 11 | `ALREADY_CONNECTED` | sim | K32 |
| 12 | `CAMERA_OFF` | sim | K67 |
| 13 | `HANDSHAKE_TIMEOUT` | sim | K33, K37 |
| 14 | `CAMERA_COUNT` | bileşen | K35 |
| 15 | `CONFIG_INVALID` | bileşen | K36 |
| 16 | `READY_INVALID` | sim | K37 |
| 17 | `PRODUCER_INVALID` | sim | K34 |

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
- **A8-K13.** Sonar kaynaklı kayıtta `window_ns` = 0'dır, `t_meas_ns` yansıma anıdır (iskelet §3.1) ve `başlık.t_meas_ns − 30 s ≤ kayıt.t_meas_ns ≤ başlık.t_meas_ns + 1 s` koşulunu sağlar (aksi `TIME`); kümenin zaman yayılımı `t_sigma_ns`'te taşınır.
- **A8-K14.** Kayıt alım zamanı taşımaz; alım zamanını alıcı kendi saatiyle damgalar ve kayıttan ayrı tutar (A4).

### 4.3 Kayıt: kimlik ve parti

- **A8-K15.** `det_seq` her akışta bir çalıştırma boyunca kesin artar (boşluk serbesttir): bir partideki kayıtların `det_seq`'leri `batch_index` sırasıyla kesin artar ve ilki akışta son kabul edilen `det_seq`'ten büyüktür (aksi `SEQ`). Kamera bileşeni ilk kaydına `WELCOME.det_seq_start`'ı verir ve gönderdiği her kayıtta 1 artırır. Sayaç bir çalıştırmada sarmaz (hesap: 30 Hz × 4 kayıtla 2³² kayıt ≈ 414 gün).
- **A8-K16.** Parti başlığının `batch_seq`'i her akışta, `NO_FRAME` başlıkları dışında, akışta son kabul edilen partininkinden büyüktür (aksi `SEQ`). Kamerada `batch_seq` karenin `kare_seq`'idir; sonarda üreticinin satır sayacıdır ve bir tarama satırı için en çok bir parti üretilir.
- **A8-K17.** Zaman ilerleme denetimi parti düzeyindedir: `NO_FRAME` dışındaki bir partinin `t_meas_ns`'i akışta son kabul edilen partininkinden büyüktür (aksi `TIME`); aynı partideki kayıtlar arasında zaman ilerlemesi aranmaz.
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
- **A8-K23.** `class_id` = 0 "bilinmiyor" demektir; diğer değerlerin anlamı A5'in senaryo hedef sınıfı tablosundadır; tabloda olmayan `class_id`'yi alıcı 0 sayar ve partiyi reddetmez.
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
| 6 | `TIME` | **Başlık:** `t_meas_ns` ≥ 0; `source` = CAM_FWD ve `NO_FRAME` yokken `window_ns` ≤ 1 000 000 000; `source` = PING360 ise `window_ns` = 0; `NO_FRAME` başlığında `window_ns` = 0; `NO_FRAME` dışındaki başlıkta K17. **Kayıt:** `t_meas_ns` ≥ 0; kamerada K12; sonarda K13 (`window_ns` = 0 dahil). K11'in kamera eşitliği sim'de `ECHO` satırında sınanır. K11'in sonar gönderim anı eşitliği ve K64'ün "partinin kapanış anı" eşitliği üretici kurallarıdır; alıcının elindeki alanlarla sınanamaz ve doğrulamaya girmez | evet | evet | `COMPONENT_ERROR` |
| 7 | `KIND` | K01, K05 (sim'de yetenek denetimi dahil) | evet | evet (yetenek denetimi hariç) | `COMPONENT_ERROR` |
| 8 | `FLAGS` | K21, K22 | evet | evet | `COMPONENT_ERROR` |
| 9 | `VALUE` | 3.2'deki `VALUE` sütunlu alanlar; K03'ün `z` kısmı; K24 | evet | evet | `COMPONENT_ERROR` |
| 10 | `UNCERT` | K07 (K03'ün `chol` kısmı dahil) | evet | evet | `COMPONENT_ERROR` |

- **A8-K28.** **Sim'in doğruladıkları:** sim, A8'den aldığı her `DETECTION_BATCH`'i K27 tablosunun "Sim" sütunu "evet" olan bütün satırlarıyla, karta iletmeden önce doğrular. `HELLO` yeteneklerine ve bekleyen kareye dayanan denetimler (`ECHO`, `KIND`'ın yetenek kısmı) yalnız sim'indir. Reddedilen partinin yerine karta, başlığı K62'ye göre doldurulmuş, `count` = 0 ve durumu ilk başarısız satırın son sütunundaki değer olan bir parti gider. Bileşenin bildirdiği durum (OK dahil) karta yalnız yanıt, sim için uygulanabilir bütün satırlardan geçtiyse aktarılır; herhangi bir satır başarısızsa yalnız ilk başarısız satırın sonucu geçerlidir. Kayıt içermeyen bir partide kayıtlara ilişkin denetimler boş kümeye uygulanır ve geçer. Bu kural yalnız `M_k`'dan önce alınan yanıta uygulanır; sonra alınan yanıt doğrulanmadan `LATE`'tir (K59, K63).

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
- **A8-K30.** Ret sayaçları partide taşınmaz. Sim her akış için ret nedeni başına ve karta giden durum başına 64 bitlik sayaç tutar, bunları A3'e bildirir ve her reddi nedeniyle A6'ya yazar; kart her akış için ret nedeni başına sayaç tutar (A4 telemetrisi).

### 4.7 A8 bağlantısı: oturum

- **A8-K31.** A8 taşıması TCP'dir ve yalnız 127.0.0.1 üzerindedir; sim dinler, bileşen bağlanır. Sim bileşeni başlatırken bağlantı noktasını ve 64 bitlik rastgele oturum jetonunu `--a8-port <ondalık>` ve `--a8-token <16 onaltılık hane>` komut satırı argümanlarıyla verir; bileşen bağlantı noktasını başka yoldan aramaz.
- **A8-K32.** Sim aynı anda tek bir el sıkışmış bileşen bağlantısı tutar; ikinci bağlantıya `REJECT(ALREADY_CONNECTED)` gönderip kapatır.
- **A8-K33.** Bileşen bağlandıktan sonra 5 s içinde (sim'in saatiyle; mimari seçimi) `HELLO` göndermezse sim `BYE(HANDSHAKE_TIMEOUT)` gönderip bağlantıyı kapatır.
- **A8-K34.** Sim `HELLO`'yu şu sırayla doğrular ve ilk başarısız denetimin nedeniyle `REJECT` gönderip bağlantıyı kapatır: `a8_major` ≠ 1 → `VERSION_MAJOR`; `det_schema_major` ≠ `IDA_DET_SCHEMA_MAJOR` → `SCHEMA_MAJOR`; jeton uyuşmazlığı → `TOKEN`; `producer_id` = 0 ya da `capabilities`'in ayrılmış bitleri 1 → `PRODUCER_INVALID`; `pixel_format_mask` A2'nin kullandığı biçimin bitini içermiyor → `FORMAT_UNSUPPORTED`. Kabul edilen `HELLO`'nun `producer_id`, `producer_ver` ve `capabilities` alanları oturum boyunca sim'de saklanır.
- **A8-K35.** Bu sürümde `WELCOME.camera_count` = 1'dir ve `WELCOME`'ın ardından tam bir `CAMERA_CONFIG` gelir. Bileşen `camera_count` ≠ 1 alırsa `BYE(CAMERA_COUNT)` gönderir, bağlantıyı kapatır ve yeniden bağlanmaz (K43). Senaryo birden çok kamera tanımlarsa sim bileşeni başlatmaz ve durumu A3'e hata olarak bildirir.
- **A8-K36.** `CAMERA_CONFIG` ancak aşağıdaki koşulların hepsini sağlarsa geçerlidir. Sim geçersiz bir yapılandırma göndermez (A5'ten aldığı değerleri bileşeni başlatmadan önce aynı tabloyla denetler; geçersizse bileşeni başlatmaz ve A3'e hata bildirir). Bileşen geçersiz bir yapılandırma alırsa `BYE(CONFIG_INVALID)` gönderir, bağlantıyı kapatır ve yeniden bağlanmaz (K43).

| Alan | Koşul | Dayanak |
|---|---|---|
| `config_ver` | ≥ 1 | — |
| `camera_id` | ≤ 254 | — |
| `source` | 2 | — |
| `tier` | 1 ya da 2 | iskelet §6 |
| `width`, `height` | 1 … 640 ve 1 … 480 | iskelet §10 md. 25 |
| `rate_mhz` | 1 … 30 000 | iskelet §10 md. 25 |
| `fx`, `fy` | sonlu ve > 0 | — |
| `cx`, `cy` | sonlu; −0,5 ≤ `cx` ≤ `width` − 0,5; −0,5 ≤ `cy` ≤ `height` − 0,5 | mimari seçimi |
| `distortion_model` | A2'nin tanımladığı kodlardan biri | A2 |
| `dist[5]` | sonlu; modelin kullanmadığı katsayılar 0 | A2 |
| `mount_l_b[3]` | sonlu; her bileşenin mutlak değeri ≤ 10 m | mimari seçimi (tahmin) |
| `mount_q_bs[4]` | sonlu; `|w² + x² + y² + z² − 1| ≤ 1·10⁻⁵`; toplam binary64'te w, x, y, z sırasıyla hesaplanır (her kare binary64'te tamdır) | mimari seçimi (tahmin) |
| `calib_ver` | ≥ 1 | — |
| `pixel_format` | A2'nin kullandığı biçim ve `HELLO.pixel_format_mask`'te bildirilmiş | A2, K34 |

- **A8-K37.** Bileşen geçerli `CAMERA_CONFIG`'i aldıktan ve halkayı salt okunur eşledikten sonra `READY` gönderir. Sim `READY`'yi şu sırayla doğrular: `ring_mapped` ≠ 1 → `BYE(RING_MAP_FAILED)`; `config_ver_ack` gönderilen `config_ver`'e eşit değil ya da `reserved` ≠ 0 → `BYE(READY_INVALID)`; her iki durumda bağlantıyı kapatır. `READY` son `CAMERA_CONFIG`'ten sonra 5 s içinde gelmezse sim `BYE(HANDSHAKE_TIMEOUT)` ile kapatır. El sıkışma ancak geçerli `READY` alındığında tamamlanır; sim bundan önce `FRAME_AVAILABLE` göndermez ve bileşeni "el sıkışmış" saymaz.
- **A8-K38.** Bileşen halkayı yalnız salt okunur eşler; halkanın hiçbir alanına (denetim alanları dahil) yazmaz.
- **A8-K39.** Major sürümler (A8 ve şema) eşit olmak zorundadır; minor sürümler farklıysa iki taraf küçük olanı kullanır. Minor sürüm yalnız mesaj yükünün sonuna alan ekleyebilir ve eklediği uzantının boyutunu tanımlar. Alıcı, tanıdığı bölümden sonra gelen fazla baytları yalnız el sıkışmada anlaşılan minor sürüm o mesaj türü için bir uzantı tanımlıyorsa ve fazlalık tam o boyuttaysa yok sayar; aksi halde K42'deki sınıflar uygulanır. Bu sürümde anlaşılan minor 0'dır ve hiçbir mesaj türü uzantı tanımlamaz; yani 1.0'da hiçbir fazla bayt yok sayılmaz.
- **A8-K40.** Her taraf, son 1 s içinde başka mesaj göndermediyse `HEARTBEAT` gönderir; karşı taraftan 3 s boyunca hiç mesaj alamayan taraf `BYE(HEARTBEAT_TIMEOUT)` gönderip bağlantıyı kapatır (süreler kendi duvar saatiyle; mimari seçimi).
- **A8-K41.** Bir oturum boyunca kamera yapılandırması (kademe, çözünürlük, hız, iç parametreler, montaj, `calib_ver`, piksel biçimi) değişmez; değişecekse sim önce `BYE(CONFIG_CHANGED)` gönderir ve bağlantıyı kapatır, bileşen yeniden bağlanır ve yeni el sıkışmayla yeni yapılandırmayı alır.
- **A8-K42.** Gelen her mesaj üç hata sınıfına göre, bu sırayla değerlendirilir:
  - **(a) Çerçeve hatası:** `magic` yanlış; `type` bilinmiyor ya da yöne uymuyor; çerçeve başlığının `flags`'i ≠ 0; `msg_seq` bir önceki mesajınkinden tam 1 büyük değil; mesaj el sıkışmanın sırasına uymuyor; `length` > 4096; ya da `length` türün anlaşılan minor sürümdeki boyut aralığının dışında. 1.0'da bu aralık sabit boyutlu türlerde (`HELLO` 56, `WELCOME` 92, `REJECT` 64, `CAMERA_CONFIG` 84, `READY` 8, `FRAME_AVAILABLE` 32, `HEARTBEAT` 0, `BYE` 4) tam o boyut, `DETECTION_BATCH`'te 40 ≤ `length` ≤ 360'tır (360 = 40 + 80 × 4). Alıcı `BYE(PROTOCOL_ERROR)` gönderip bağlantıyı kapatır.
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
- **A8-K43.** Bağlantısı kapanan bileşen halka eşlemesini bırakır. `REJECT` almışsa ya da kendisi `BYE(CAMERA_COUNT)` veya `BYE(CONFIG_INVALID)` göndermişse yeniden bağlanmaz ve sıfırdan farklı çıkış koduyla sonlanır; bağlantı başka bir nedenle kapandıysa en erken 1 s sonra (mimari seçimi) yeniden bağlanmayı dener ve yeni bağlantı yeni bir oturumdur.

### 4.8 Kare teslimi

- **A8-K44.** Sim, bileşene yalnız el sıkışma tamamlandıktan sonra ve bileşenin yanıtlanmamış `FRAME_AVAILABLE`'ı yokken yeni bir `FRAME_AVAILABLE` gönderir; bileşenin elinde aynı anda en çok bir kare bulunur.
- **A8-K45.** Sim `FRAME_AVAILABLE` göndermeden önce o yuvayı A8 adına tutar (A2'nin okuyucu tutma düzeneğiyle) ve tutmayı yalnız o karenin `DETECTION_BATCH`'i alındığında, bağlantı kapandığında ya da K50'deki süre aşıldığında bırakır; tutulan yuvaya görselleştirici yazmaz (A2).
- **A8-K46.** Bir oturumda bileşene sunulan karelerin `kare_seq`'i kesin artar. Bileşen boşaldığında sim, halkada hazır olan, `M_k` anı henüz gelmemiş ve `kare_seq`'i son sunulandan büyük kareler arasından `kare_seq`'i en büyük olanı sunar; sunulan karenin `kare_seq`'inden küçük, hiç sunulmamış ve henüz **açık** (nihai durumu atanmamış, yani `M_k` anı gelmemiş) bütün kareler o anda kalıcı olarak sunulmayacak sayılır ve sonradan aday olamaz. Bu karelerin, ve `M_k` anına kadar hiç sunulmamış her karenin, nihai durumu `M_k` anında K63'teki tek öncelik sırasıyla atanır. Nihai durumu atanmış (kapanmış) kareye K46 dokunmaz.
- **A8-K47.** Bileşen ardışık `FRAME_AVAILABLE`'ların `frame_seq` değerlerinin ardışık olduğunu varsaymaz; boşluk olağan durumdur ve bileşenin davranışını değiştirmez.
- **A8-K48.** Yuvadaki kare üst verisinin (A2) kare kimliği, halka nesli, piksel biçimi, genişlik, yükseklik ya da satır adımı `FRAME_AVAILABLE` ve `CAMERA_CONFIG` ile uyuşmuyorsa, ya da işlem bittikten sonra yeniden okunan yuva yazım sayacı (A2) işlemin başındakinden farklıysa, bileşen kayıt üretmez ve `FRAME_INVALID` durumlu parti gönderir.
- **A8-K49.** Bileşen her `FRAME_AVAILABLE`'a, geç kalmış olsa bile, tam bir `DETECTION_BATCH` ile yanıt verir; yanıtın `status`'u OK, `SKIPPED_BUSY`, `FRAME_INVALID`, `CALIB_MISMATCH` ya da `COMPONENT_ERROR`'dur.
- **A8-K50.** Bir yuva `FRAME_AVAILABLE` gönderiminden sonra sim'in saatiyle 1 s (mimari seçimi) içinde bırakılmazsa sim `BYE(SLOT_HOLD_TIMEOUT)` gönderir, bağlantıyı kapatır ve yuvayı bırakır.
- **A8-K51.** Sim bileşene yalnız `CAMERA_CONFIG`'teki çözünürlükte ve piksel biçiminde kareleri sunar ve kare hızını `rate_mhz`'in üstüne çıkarmaz; bileşen yalnız el sıkışmada bildirilen çözünürlük, biçim ve hızla çalışır ve bunlardan farklı bir kareyi K48'e göre `FRAME_INVALID` sayar.

### 4.9 Bileşenin aldığı ve almadığı veriler

- **A8-K52.** Sim'den bileşene giden veri yalnız şunlardır: kare pikselleri ve halkadaki kare üst verisi (A2), `FRAME_AVAILABLE` alanları (kare kimliği, ölçüm zamanı, pozlama süresi, yuva, nesil, süre bilgisi), `CAMERA_CONFIG` alanları (çözünürlük, hız, iç parametreler, bozulma, nominal montaj, `calib_ver`, piksel biçimi, kademe) ve oturum alanları. 3.5'te olmayan bir alan sim'den bileşene gönderilmez.
- **A8-K53.** `CAMERA_CONFIG`'teki montaj, kartın kullandığı nominal montajdır (A5'in otopilot tarafı); fizik gerçeğinin montajı ve montaj hatası bileşene verilmez. Bileşen montajı yalnız görüntü içi yorum için kullanabilir; kayıt yine sensör çerçevesindedir (K01).
- **A8-K54.** Bileşene aracın pozu, hızı ya da seyrüsefer durumu, hedeflerin konumu, kimliği ya da sınıfı, yer gerçeği maskesi, senaryonun hedef listesi, başka sensörlerin verisi ve kartın durumu (izler, kipler, emniyet durumu) verilmez; bileşenin okuyabildiği halka bölgesi de bunları içermez (A2'den beklenen 3).
- **A8-K55.** Bileşen sim'e yalnız A8 üzerinden bağlanır; A1, A2 durum kanalı ve A3 uç noktalarına bağlanmaz.
- **A8-K56.** Yer gerçeğinden kayıt üreten bir üretici A8'e bağlanamaz; böyle bir üretici kapsama alınırsa sim'in içinde, `SYNTHETIC` bayrağı ve ayrı bir `producer_id` ile çalışır. Kapsama alınıp alınmayacağı bu sözleşmenin konusu değildir (8.1).

### 4.10 Kayıtların geri teslimi ve karta iletim

- **A8-K57.** Sim her kamera karesi `k` için `t_alg_k = t_teslim_k + L_alg` anını ve onu içeren A1 partisi `P_k`'yı belirler; kare `k`'ya ait parti başlığı ve kayıtları karta yalnız `P_k` içinde gider.
- **A8-K58.** Sim, kare `k`'nın sonucunu `M_k` anında değerlendirir: `M_k`'dan önce alınmış ve K28'den geçmiş bir yanıt varsa onun partisini, yoksa K63'teki durumlu partiyi `P_k`'ya koyar; istenen her `kare_seq` için karta tam bir parti başlığı gider.
- **A8-K59.** `M_k`'dan sonra alınan yanıt karta iletilmez; `LATE` sayılır ve yanıtın kendisi A6'ya yazılır.
- **A8-K60.** Sim hiçbir kipte (SIL, HIL 1x, HIL 1x + bariyer) bileşeni beklemez: `P_k`'nın oluşturulması ve sim zamanının ilerlemesi bileşenin yanıtı için ertelenmez. Bu sürümde ayrı bir "kilitli adım" kipi yoktur.
- **A8-K61.** Gerçek zamandan hızlı koşan SIL çalıştırmasında, kamera algılamasına dayanan sonuçlar için tekrar üretilebilirlik güvencesi verilmez (K60'ın sonucu; sonuç bilgisayarın hızına bağlıdır). Kamera algılamasına bağlı davranışın tekrarlanabilir sınaması, A6'ya kaydedilmiş algılama akışının yeniden verilmesiyle (A6'nın girdi tekrarı düzeyi) yapılır.
- **A8-K62.** Karta giden kamera parti başlığını sim doldurur: `source` = CAM_FWD, `inst` = `camera_id`, `batch_seq` = `kare_seq`, `t_meas_ns` ve `window_ns` karenin değerleri, `calib_ver` yapılandırmanınki, `producer_id` ve `producer_ver` el sıkışmış bileşeninki (yoksa 0), `flags` = 0, `reserved` = 0.
- **A8-K63.** Her kamera karesinin nihai durumu yalnız burada, `M_k` anında ve bir kez atanır; atanan durum sonradan değişmez. Durum şu sırayla ilk uyan koşuldan seçilir: kare görselleştiricide düştü ya da `M_k`'ya kadar halkada hazır olmadı → `FRAME_DROPPED`; el sıkışmış bileşen yok → `COMPONENT_DOWN`; kare bileşene sunulmadı → `SKIPPED_BUSY` (K46); sunuldu ama yanıt `M_k`'ya kadar alınmadı → `LATE`; yanıt K28'de reddedildi → K27 tablosunun son sütunu; aksi halde bileşenin bildirdiği durum.
- **A8-K64.** Kamera kapalı kademede sim, sim zamanında 1 s'de bir (mimari seçimi) `flags` = `NO_FRAME`, `status` = `CAMERA_OFF`, `batch_seq` = 0, `count` = 0, `window_ns` = 0 ve `t_meas_ns` = konduğu A1 partisinin kapanış anı olan bir başlığı karta gönderir. `NO_FRAME` başlığı K16 ve K17 denetimlerine girmez ve akışın "son kabul edilen" değerlerini değiştirmez.
- **A8-K65.** Sim her kamera karesinin sonucunu karta giden durum başına sayar ve sayaçları A3 üzerinden yayımlar; OK dışındaki her durum, her parti reddi (nedeniyle) ve her geç yanıt A6'ya yazılır.

### 4.11 Kart tarafındaki üretici (sonar)

- **A8-K66.** Kart içindeki sonar algılayıcısı kayıtlarını aynı yapıyla (`ida_det_record_t`, `ida_det_batch_t`) ve 4.1–4.5'teki kurallara uyarak üretir: başlığın zamanı işlenen tarama satırının gönderim anıdır (K11), boş partiler dahil. Bu partilerin çekirdeğe teslimi A4'ün, sim'e değerlendirme için aktarılması (yapılırsa) A1'in konusudur.

---

## 5. Hata durumları

| Durum | Algılama koşulu | Sim'in davranışı | Bileşenin davranışı | Karta akan |
|---|---|---|---|---|
| Bileşen hiç çalışmıyor | Kamera açık; el sıkışmış bağlantı yok | Beklemez (K60); her kare için `COMPONENT_DOWN` (K63); A3 uyarısı | — | `COMPONENT_DOWN` partileri |
| Bileşen çöktü | TCP kapandı | Tutulan yuvayı bırakır (K45); bekleyen kare `M_k`'da `COMPONENT_DOWN` olur (K63 sırası) | Yeniden başlatılırsa K43 | Her kare `COMPONENT_DOWN` |
| Bileşen asılı kaldı | K40 (3 s sessizlik) ya da K50 (1 s tutma) | `BYE`, bağlantıyı kapatır, yuvayı bırakır | — | Kapanıştan önce `LATE`, sonra `COMPONENT_DOWN` |
| Yanıt geç | Yanıt `M_k`'dan sonra | Reddeder, sayar, A6'ya yazar (K59) | Yanıtını yine gönderir (K49) | `LATE` |
| Bileşen meşgul | Yanıtlanmamış kare varken yeni kare hazır | Yeni kareyi sunmaz; boşalınca en yenisini sunar, geride kalanları kalıcı atlar (K46) | — | Sunulmayanlar için `SKIPPED_BUSY` |
| Geçersiz çıktı | K27 tablosu | Bütün partiyi reddeder, nedeni sayar (K28, K30) | — | `CALIB` için `CALIB_MISMATCH`, diğerleri için `COMPONENT_ERROR`; `count` = 0 |
| Sürüm, şema, jeton, üretici ya da biçim uyuşmazlığı | `HELLO` (K34) | `REJECT`, kapatır | Yeniden bağlanmaz, hata koduyla sonlanır (K43) | `COMPONENT_DOWN` |
| Kamera sayısı 1 değil | `WELCOME` (K35) | Bu sürümde göndermez; senaryoda birden çok kamera varsa bileşeni başlatmaz | `BYE(CAMERA_COUNT)`, sonlanır | `COMPONENT_DOWN` |
| Yapılandırma geçersiz | K36 | Göndermez; A5 değeri geçersizse bileşeni başlatmaz, A3 hatası | `BYE(CONFIG_INVALID)`, sonlanır | `COMPONENT_DOWN` |
| `READY` geçersiz ya da gelmedi | K37 | `BYE(RING_MAP_FAILED)`, `BYE(READY_INVALID)` ya da `BYE(HANDSHAKE_TIMEOUT)` | K43 ile yeniden dener | `COMPONENT_DOWN` |
| Kare biçimi ya da bütünlüğü bozuk | K48, K51 | Partiyi doğrular ve iletir | `FRAME_INVALID` gönderir | `FRAME_INVALID` |
| Kalibrasyon uyuşmazlığı | Bileşen: karede ya da yapılandırmada; sim ve kart: K27 `CALIB` | Sim: `CALIB_MISMATCH` iletir | `CALIB_MISMATCH` gönderir | `CALIB_MISMATCH`; kart `CALIB` ile reddeder |
| Kare görselleştiricide düştü ya da gelmedi | A2 düşme bildirimi ya da `M_k`'da halkada yok | Bileşene sunmaz | — | `FRAME_DROPPED` |
| Görselleştirici çalışmıyor (kamera açık) | A2 bağlantısı yok | Kareler istenir ama gelmez (A2) | Kare almaz | `FRAME_DROPPED` |
| Kamera kapalı | Kademe kapalı | Bileşeni başlatmaz; bağlanırsa `REJECT(CAMERA_OFF)` | Sonlanır | 1 Hz `CAMERA_OFF` (K64) |
| Yapılandırma değişti | Kademe ya da kalibrasyon değişimi | `BYE(CONFIG_CHANGED)` (K41) | Yeniden bağlanır | Arada `COMPONENT_DOWN` |
| Sim çöktü ya da kapandı | Bileşende TCP kapandı | — | Eşlemeyi bırakır, K43 ile dener | (A1 sözleşmesinin konusu) |
| İkinci bileşen | Bağlı bileşen varken yeni bağlantı | `REJECT(ALREADY_CONNECTED)` (K32) | Sonlanır | Değişmez |
| Protokol hatası | K42 | `BYE(PROTOCOL_ERROR)`, kapatır | K43 | `COMPONENT_DOWN` |
| Bir parti başlığı karta ulaşmadı | Kart: akıştaki `batch_seq` boşluğu ya da başlık gelmemesi | A1'in konusu | — | Kartın tazelik kuralı (A4'ten beklenen 3) |

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
- İskelet §5'e göre hedef sonarın en kısa menzilinin içindeyken konumu kamera algılamasıyla sürdürülür. Bu durumda **sonarın kör bölgesindeki algılama pencerenin görünür olmasına bağlı kalır** (iskelet §10 md. 21). Bu, modelin kabul edilen bir sınırı olarak kayda geçer; giderilmesi için torpidonun ya da sensör setinin yetenekleri artırılmaz.
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
| K31–K43 | Sözleşme (sahte sim ya da sahte bileşenle) | El sıkışma adımları; yanlış jeton, yanlış major, `producer_id` = 0, desteklenmeyen biçim, `camera_count` = 0 ve 2, K36 tablosunun her satırı için geçersiz yapılandırma, `ring_mapped` = 0, yanlış `config_ver_ack`, eksik `HELLO`, gecikmiş `READY`, bozuk `magic`, atlanan `msg_seq`, K42 örnek tablosunun her satırı (sınıf a ve b), minor farkı, kalp atışı kesilmesi | Gönderilen `REJECT`/`BYE` nedeni; bileşenin yeniden bağlanıp bağlanmadığı ve çıkış kodu; kapanış süresi (5 s, 3 s sınırları ±100 ms) |
| K38 | Sözleşme | Bileşen halkayı salt okunur eşler; yazma girişimi işletim sistemi hatası verir | Eşleme koruma bayrakları |
| K44–K51 | Sözleşme | Bileşen yapay olarak yavaşlatılır (işlem süresi > kare periyodu); kare kimlikleri izlenir; yuva yazım sayacı işlem sırasında değiştirilir; yapılandırmadan farklı boyutta kare | Yolda en çok 1 kare; sunulan `kare_seq`'lerin kesin arttığı; atlanan karelerin bir daha sunulmadığı; `SKIPPED_BUSY` sayısı = sunulmayan kare sayısı; `FRAME_INVALID`; 1 s tutma sınırında kapanış |
| K52–K55 | Sözleşme ve gözden geçirme | Sim'den bileşene giden bayt akışı kaydedilir ve 3.5'e göre ayrıştırılır; halka okunabilir bölgesi A2 tanımıyla karşılaştırılır; bileşenin açtığı soketler işletim sistemi araçlarıyla listelenir | Tanımsız alan sayısı = 0; bileşenin tek bağlantısı A8 |
| K56, K22 | Sözleşme | A8 üzerinden `SYNTHETIC` = 1 kayıt gönderen sahte bileşen | `FLAGS` reddi; karta `COMPONENT_ERROR`, kayıt gitmez |
| K57–K60 | SIL ve kartlı (HIL 1x ve bariyer) | Bilinen `L_alg` ile her kare için `P_k`'nın sim zamanı; bileşen `M_k`'nın hemen öncesinde ve sonrasında yanıt verir; bileşen hiç yanıt vermez | Her `kare_seq` için karta tam bir başlık; başlığın konduğu partinin sim zamanı = `P_k`; sim zamanının bileşen yüzünden durmadığı (kaçırılan adım sayacı değişmez) |
| K61 | SIL | Aynı senaryo, kaydedilmiş algılama akışı A6 girdi tekrarıyla iki kez verilir | Karta giden partilerin iki koşuda bit düzeyinde aynı olduğu |
| K65 | Sözleşme | Her durumdan bilinen sayıda | A3 sayaçları ve A6 kayıtları sayıyla eşit |
| K66 | Birim (kart tarafı) | Boş ve dolu sonar partileri kart doğrulayıcısından geçer | Kabul/ret; başlık zamanı = satırın `t_tx`'i |
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
| 5 | Süre sınırları: `HELLO` 5 s, `READY` 5 s, kalp atışı 1 s / 3 s, yuva tutma 1 s, yeniden bağlanma 1 s, `CAMERA_OFF` başlığı 1 Hz; montaj konumu ≤ 10 m; kuaterniyon normu toleransı 1·10⁻⁵ | Tahmin | Sözleşme testleri ve ilk bütünleşik koşu (W5) |
| 6 | 6.2'deki görünürlük sınırı | Ölçülecek | MP8 (Windows'ta birincil kamera yolu) |
| 7 | Anlamsal algılama üreteci (yer gerçeğinden kayıt) kapsama alınacak mı | Karar bekliyor | Bölüm 03 §8.4 ve 05 §8; mimar. A8 yalnız bağlanamayacağını (K56) ve bu sürümde `SYNTHETIC` kaydın reddini (K22) sabitler |
| 8 | Kart tarafında akışın tazelik süresi (kamera akışından başlık gelmezse "algılama yok" sayılması) | A4/çekirdek | A4'ten beklenen 3 |
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

**A2'den beklenen**

1. Kamera isteğinde ve kare üst verisinde `t_meas_ns`'in iskelet §3.1'e uyması: pozlama aralığının sonu; pozlama süresinin ayrı alanda taşınması. A8 bu iki değeri başlığa ve kayda bit düzeyinde kopyalar (K11, K12); kartın tek geçerlilik anı `t_valid` = pozlama ortasıdır.
2. Tek bir piksel biçimi seçmesi ve onu 0–31 aralığında sayısal bir kodla (A8 `pixel_format_mask` bit numarası) kare üst verisinde ve yapılandırmada bildirmesi; kare üst verisinde genişlik, yükseklik ve satır adımı.
3. Halkanın, A8 bileşeninin salt okunur eşleyeceği bölgesinde poz, montajın fizik gerçeği, hedef bilgisi ya da başka bir yer gerçeği bulunmaması (K54). Kare üst verisinde montaj gerekiyorsa A8'in okuyabildiği bölgenin dışında tutulması.
4. Halka sahibinin (sim) bir yuvayı A8 adına tutabilmesi ve tutulan yuvaya görselleştiricinin yazmaması (K45); bileşenin denetim alanlarına yazmadan okuyabilmesi.
5. Her yuvada, yuvaya her yazımda değişen bir yazım sayacı ya da nesil alanı (K48).
6. Kamera kademe değerlerinin (çözünürlük, hız; üst sınır 640 × 480 ve 30 Hz) tanımı; bozulma modeli kodlarının ve katsayı anlamlarının tanımı. A8 bu değerleri kendisi tanımlamaz (K68).
7. `kare_seq`'in kamera başına bir çalıştırmada 1'den başlayıp kesin artan u32 olması (K16; 0 değeri `NO_FRAME` başlığına ayrılmıştır).
8. Görselleştiricinin bir kareyi düşürdüğünü sim'e bildirmesi (K63 `FRAME_DROPPED`).
9. Halka adının ve neslinin `WELCOME`'da iletilebilecek biçimde olması (ad en çok 63 ASCII karakter).
10. Bu sürümde tek kamera (K35).

**A4'ten beklenen**

1. Algılama partilerinin ve kayıtlarının çekirdeğe `ida_det_batch_t` ve `ida_det_record_t` yapılarıyla (bu başlıktan) teslimi; alım zamanının kayıttan ayrı bir zarf alanında taşınması (K14).
2. Kart tarafındaki doğrulayıcının K27 tablosunun "Kart" sütunundaki satırları aynı sırayla, bütün parti reddiyle uygulaması (K29); `IDA_DET_RJ_*` kodlarıyla neden başına sayaç (K30); `calib_ver` karşılaştırması için akış başına kalibrasyon sürümünün kalıcı parametrelerde bulunması (K20); akış başına "son kabul edilen" `batch_seq`, `det_seq` ve `t_meas_ns` durumunun tutulması (K15–K17).
3. Bir algılama akışının tazelik kuralı: kamera açıkken her karede, kapalıyken 1 Hz başlık geldiği güvencesiyle (K58, K64), başlık gelmeyen akışın "algılama yok" sayılacağı süre.
4. Zaman tipinin işaretli 64 bit nanosaniye olması (başlık `int64_t` kullanır).
5. Belirsizliğin `chol` (Cholesky çarpanı) olarak teslim edilmesi; kovaryansın kurulup kurulmayacağı çekirdeğin seçimidir (K09).

**A5'ten beklenen**

1. `L_alg` parametresi (6.1 aralığıyla).
2. Kameranın nominal montajı ve `calib_ver`'i (otopilotun gördüğü değerler) ile fizik gerçeği montajının ayrı tutulması (K53); bu değerlerin K36 tablosuna uyması.
3. Senaryo hedef sınıfı tablosu (`class_id` anlamı; K23).
4. Kamera kademesinin seçimi; bu sürümde en çok bir kamera (K35).

**A3 ve A6'dan beklenen**

1. A3: K30 ve K65 sayaçlarının (ret nedeni başına, karta giden durum başına, geç yanıt) yayımlanması; bileşenin başlatılması (K31) çalıştırma kontrolünün işidir; K35 ve K36'daki başlatma hatalarının bildirilmesi.
2. A6: karta giden her algılama partisinin konduğu A1 partisiyle birlikte kaydı; geç yanıtların ve parti retlerinin nedenleriyle kaydı.
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
| AB-049, AB-050, AB-051 | A2 | A8 okuyucusu halkanın denetim alanlarına yazmaz; tutmayı sim yapar ve bırakır, çöken bileşenin yuvasını sim kurtarır (K38, K45, K50); düzeneğin kendisi A2'den beklenen 4 |
| AB-053, U-05 | A2 | Başlık ve kayıt karenin `t_meas_ns` ve `window_ns`'ini değiştirmeden taşır, tek geçerlilik anı `t_valid` ile türetilir (K10–K12); kart alım zamanı kayıttan ayrıdır (K14) |
| AB-142 | Atış kontrol uygulaması | Belirsizlik Cholesky çarpanı olarak taşınır; ölçülmeyen bileşenin satırı ve sütunu 0'dır ve kabul kararı pozitif tanımlılık sınamasına dayanmaz (K03, K07, K09) |
| AB-155 | A5 | `class_id`'nin anlamı A5'e bırakıldı (K23) |
| U-21 | — | AB-106 ile kapandı |
| U-22 | — | AB-105 ile kapandı |
| U-29 | A8; A1 | Bölüm 03'ün `RB2`/`B2`/`RB3` ve `SENSOR` kuralları ile 07'nin kare kimliği bu sözleşmede eşlendi (3.2, K01, K03–K05, K16); A1 eşlemesi A1'den beklenen 1 |
